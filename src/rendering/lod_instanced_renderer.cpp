#include "render/lod_instanced_renderer.h"
#include "render/mesh.h"
#include "render/material.h"
#include "render/material_sort_key.h"
#include "render/material_state_cache.h"
#include "render/renderer.h"
#include "render/render_state.h"
#include "render/logger.h"
#include "render/gl_thread_checker.h"
#include <glad/glad.h>
#include <algorithm>
#include <cstring>
#include <chrono>

namespace Render {

// ==================== 性能计时辅助类 ====================
class ScopedTimer {
public:
    ScopedTimer(float& outTime) : m_outTime(outTime) {
        m_start = std::chrono::high_resolution_clock::now();
    }
    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        m_outTime += std::chrono::duration<float, std::milli>(end - m_start).count();
    }
private:
    float& m_outTime;
    std::chrono::high_resolution_clock::time_point m_start;
};

// ==================== LODInstancedRenderer 实现 ====================

LODInstancedRenderer::LODInstancedRenderer() {
    // ✅ 检查是否支持持久映射（OpenGL 4.4+）
    GL_THREAD_CHECK();
    
    int major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    
    // 检查OpenGL版本是否 >= 4.4
    m_supportsPersistentMapping = (major > 4) || (major == 4 && minor >= 4);
    
    if (m_supportsPersistentMapping) {
        LOG_INFO("LODInstancedRenderer: Persistent buffer mapping enabled (OpenGL " + 
                 std::to_string(major) + "." + std::to_string(minor) + ")");
    } else {
        LOG_INFO("LODInstancedRenderer: Persistent buffer mapping not available (OpenGL " + 
                 std::to_string(major) + "." + std::to_string(minor) + 
                 "), using traditional approach");
    }
}

LODInstancedRenderer::~LODInstancedRenderer() {
    ClearInstanceVBOs();
}

void LODInstancedRenderer::AddInstance(
    ECS::EntityID entity,
    Ref<Mesh> mesh,
    Ref<Material> material,
    const Matrix4& worldMatrix,
    LODLevel lodLevel
) {
    InstanceData instanceData(worldMatrix, entity.index);
    AddInstance(entity, mesh, material, instanceData, lodLevel);
}

void LODInstancedRenderer::AddInstance(
    ECS::EntityID entity,
    Ref<Mesh> mesh,
    Ref<Material> material,
    const InstanceData& instanceData,
    LODLevel lodLevel
) {
    if (!mesh || !material) {
        return;
    }
    
    // ✅ 分批处理：将实例添加到待处理队列，而不是直接添加到组
    // 这样可以避免一次性处理大量实例导致的卡死问题
    PendingInstance pending;
    pending.entity = entity;
    pending.mesh = mesh;
    pending.material = material;
    pending.instanceData = instanceData;
    pending.lodLevel = lodLevel;
    
    m_pendingInstances.push_back(pending);
}

void LODInstancedRenderer::RenderAll(Renderer* renderer, RenderState* renderState) {
    if (!renderer) {
        return;
    }
    
    // ✅ 重置每帧统计
    m_stats.vboUploadCount = 0;
    m_stats.bytesUploaded = 0;
    m_stats.uploadTimeMs = 0.0f;
    m_stats.sortTimeMs = 0.0f;
    m_stats.renderTimeMs = 0.0f;
    
    ScopedTimer totalTimer(m_stats.renderTimeMs);
    
    // ✅ 分批处理机制：每帧只处理一定数量的实例
    // 重置当前帧处理计数
    m_currentFrameProcessed = 0;
    
    // ✅ 清空上一帧的组（因为已经渲染过了）
    // 注意：这里只清空组的内容，不删除组本身，因为组是按key索引的
    // 如果下一帧有相同key的实例，会复用同一个组
    for (auto& [key, group] : m_groups) {
        group.Clear();
    }
    
    // ✅ deque的size()是O(1)
    size_t processCount = std::min(m_maxInstancesPerFrame, m_pendingInstances.size());
    
    // 将待处理的实例添加到实际渲染组
    for (size_t i = 0; i < processCount; ++i) {
        const auto& pending = m_pendingInstances[i];  // deque支持随机访问
        
        // 添加到实际渲染组
        AddInstanceToGroup(
            pending.entity,
            pending.mesh,
            pending.material,
            pending.instanceData,
            pending.lodLevel
        );
        
        m_currentFrameProcessed++;
    }
    
    // ✅ deque的erase from begin效率更高
    if (processCount > 0) {
        m_pendingInstances.erase(
            m_pendingInstances.begin(),
            m_pendingInstances.begin() + processCount
        );
    }
    
    // 如果当前帧没有处理任何实例，且没有待处理的实例，直接返回
    if (m_groups.empty() && m_pendingInstances.empty()) {
        return;
    }
    
    // 按材质排序键排序
    std::vector<LODInstancedGroup*> sortedGroups;
    sortedGroups.reserve(m_groups.size());
    
    for (auto& [key, group] : m_groups) {
        if (!group.IsEmpty()) {
            sortedGroups.push_back(&group);
        }
    }
    
    // 如果没有可渲染的组，直接返回
    if (sortedGroups.empty()) {
        return;
    }
    
    // 排序
    {
        ScopedTimer sortTimer(m_stats.sortTimeMs);
        
        MaterialSortKeyLess less;
        std::sort(sortedGroups.begin(), sortedGroups.end(),
            [&less](const LODInstancedGroup* a, const LODInstancedGroup* b) {
                // 先按排序键排序
                if (less(a->sortKey, b->sortKey) || less(b->sortKey, a->sortKey)) {
                    return less(a->sortKey, b->sortKey);
                }
                // 再按 LOD 级别排序
                return static_cast<int>(a->lodLevel) < static_cast<int>(b->lodLevel);
            });
    }
    
    // 渲染每个组
    for (auto* group : sortedGroups) {
        RenderGroup(group, renderer, renderState);
    }
}

void LODInstancedRenderer::Clear() {
    ClearInstanceVBOs();
    m_groups.clear();
    m_pendingInstances.clear();  // ✅ 清空待处理队列
    m_currentFrameProcessed = 0;
}

LODInstancedRenderer::Stats LODInstancedRenderer::GetStats() const {
    Stats stats = m_stats;  // 复制持久数据
    
    stats.groupCount = m_groups.size();
    stats.pendingCount = m_pendingInstances.size();
    stats.totalInstances = 0;
    stats.drawCalls = 0;
    
    stats.lod0Instances = 0;
    stats.lod1Instances = 0;
    stats.lod2Instances = 0;
    stats.lod3Instances = 0;
    stats.culledCount = 0;
    
    for (const auto& [key, group] : m_groups) {
        size_t instanceCount = group.GetInstanceCount();
        stats.totalInstances += instanceCount;
        stats.drawCalls++;
        
        // 按 LOD 级别统计
        switch (group.lodLevel) {
            case LODLevel::LOD0:
                stats.lod0Instances += instanceCount;
                break;
            case LODLevel::LOD1:
                stats.lod1Instances += instanceCount;
                break;
            case LODLevel::LOD2:
                stats.lod2Instances += instanceCount;
                break;
            case LODLevel::LOD3:
                stats.lod3Instances += instanceCount;
                break;
            case LODLevel::Culled:
                stats.culledCount += instanceCount;
                break;
        }
    }
    
    // ✅ 更新峰值
    if (stats.totalInstances > stats.peakInstanceCount) {
        stats.peakInstanceCount = stats.totalInstances;
    }
    
    // ✅ 计算内存使用
    stats.totalAllocatedMemory = 0;
    for (const auto& [key, group] : m_groups) {
        stats.totalAllocatedMemory += 
            group.instances.capacity() * sizeof(InstanceData) +
            group.entities.capacity() * sizeof(ECS::EntityID);
    }
    
    return stats;
}

size_t LODInstancedRenderer::GetInstanceCount(LODLevel lodLevel) const {
    size_t count = 0;
    
    for (const auto& [key, group] : m_groups) {
        if (group.lodLevel == lodLevel) {
            count += group.GetInstanceCount();
        }
    }
    
    return count;
}

// ==================== 私有方法实现 ====================

void LODInstancedRenderer::AddInstanceToGroup(
    ECS::EntityID entity,
    Ref<Mesh> mesh,
    Ref<Material> material,
    const InstanceData& instanceData,
    LODLevel lodLevel
) {
    if (!mesh || !material) {
        return;
    }
    
    // 生成材质排序键
    MaterialSortKey sortKey = GenerateSortKey(material, mesh);
    
    // 创建分组键
    GroupKey key;
    key.mesh = mesh;
    key.material = material;
    key.lodLevel = lodLevel;
    key.sortKey = sortKey;
    
    // 查找或创建组
    auto& group = m_groups[key];
    
    if (group.instances.empty()) {
        // 初始化组
        group.mesh = mesh;
        group.material = material;
        group.lodLevel = lodLevel;
        group.sortKey = sortKey;
        
        // ✅ 根据预估数量预分配内存
        size_t estimatedInstancesPerGroup = m_estimatedInstanceCount / 
            std::max(m_estimatedGroupCount, size_t(1));
        
        // 至少预留16个，避免太小的预分配
        estimatedInstancesPerGroup = std::max(estimatedInstancesPerGroup, size_t(16));
        
        group.instances.reserve(estimatedInstancesPerGroup);
        group.entities.reserve(estimatedInstancesPerGroup);
    }
    
    // 添加实例数据
    group.instances.push_back(instanceData);
    group.entities.push_back(entity);
    
    // ✅ 标记组为脏
    group.MarkDirty();
}

MaterialSortKey LODInstancedRenderer::GenerateSortKey(Ref<Material> material, Ref<Mesh> mesh) const {
    // 使用 MaterialSortKey 系统构建排序键
    // 设置实例化管线标志
    uint32_t pipelineFlags = MaterialPipelineFlags_Instanced;
    
    // 如果网格有效，可以添加其他标志（例如：是否需要阴影等）
    // 这里可以根据需要扩展
    
    // 构建材质排序键
    // overrideHash 可以用于区分不同的 LOD 级别或其他覆盖参数
    uint32_t overrideHash = 0;
    
    // 调用 MaterialSortKey 构建函数
    MaterialSortKey sortKey = BuildMaterialSortKey(
        material.get(),
        overrideHash,
        pipelineFlags
    );
    
    return sortKey;
}

void LODInstancedRenderer::RenderGroup(
    LODInstancedGroup* group,
    Renderer* renderer,
    RenderState* renderState
) {
    if (!group || !group->mesh || !group->material || group->instances.empty()) {
        return;
    }
    
    if (!renderer) {
        return;
    }
    
    GL_THREAD_CHECK();
    
    // ✅ 使用 MaterialStateCache 避免重复绑定材质
    auto& stateCache = MaterialStateCache::Get();
    if (stateCache.ShouldBind(group->material.get(), renderState)) {
        group->material->Bind(renderState);
        stateCache.OnBind(group->material.get(), renderState);
    }
    
    // ✅ 设置实例化标志（告诉着色器使用实例数据）
    if (auto shader = group->material->GetShader()) {
        if (auto uniformMgr = shader->GetUniformManager()) {
            uniformMgr->SetBool("uHasInstanceData", true);
            // ✅ 设置 uModel 为单位矩阵，因为实例矩阵已经是完整的世界变换矩阵
            uniformMgr->SetMatrix4("uModel", Matrix4::Identity());
        }
    }
    
    // ✅ 应用材质相关的渲染状态（通过 RenderState）
    if (renderState) {
        // Material::Bind 已经通过 RenderState 应用了混合模式、深度测试等
        // 这里可以添加额外的状态设置（如果需要）
    }
    
    // ✅ 仅在需要时上传数据
    if (group->NeedsUpload()) {
        ScopedTimer uploadTimer(m_stats.uploadTimeMs);
        
        UploadInstanceData(group->instances, group->mesh);
        
        // ✅ 标记为已上传
        group->MarkUploaded();
    }
    
    size_t instanceCount = group->instances.size();
    auto& instanceVBOs = GetOrCreateInstanceVBOs(group->mesh, instanceCount);
    
    // ✅ 获取或创建实例化VAO
    GLuint vao = GetOrCreateInstancedVAO(group->mesh, instanceVBOs);
    
    if (vao == 0) {
        LOG_WARNING("LODInstancedRenderer: Failed to create instanced VAO");
        return;
    }
    
    // ✅ 使用 RenderState 绑定 VAO（如果提供）
    if (renderState) {
        renderState->BindVertexArray(vao);
    } else {
        glBindVertexArray(vao);
    }
    
    // ✅ 不再需要每次设置属性（已缓存在VAO中）
    
    // 实例化绘制
    group->mesh->DrawInstanced(static_cast<uint32_t>(instanceCount));
    
    // ✅ 使用 RenderState 解绑 VAO（如果提供）
    if (renderState) {
        renderState->BindVertexArray(0);
    } else {
        glBindVertexArray(0);
    }
}

void LODInstancedRenderer::UploadInstanceData(
    const std::vector<InstanceData>& instances,
    Ref<Mesh> mesh
) {
    if (instances.empty() || !mesh) {
        return;
    }
    
    // 提取矩阵数据
    std::vector<Matrix4> matrices;
    matrices.reserve(instances.size());
    for (const auto& instance : instances) {
        matrices.push_back(instance.worldMatrix);
    }
    
    // 提取颜色数据
    std::vector<Vector4> colors;
    colors.reserve(instances.size());
    for (const auto& instance : instances) {
        colors.push_back(Vector4(
            instance.instanceColor.r,
            instance.instanceColor.g,
            instance.instanceColor.b,
            instance.instanceColor.a
        ));
    }
    
    // 提取自定义参数
    std::vector<Vector4> customParams;
    customParams.reserve(instances.size());
    for (const auto& instance : instances) {
        customParams.push_back(instance.customParams);
    }
    
    // 上传到 GPU
    UploadInstanceMatrices(matrices, mesh);
    UploadInstanceColors(colors, mesh);
    UploadInstanceCustomParams(customParams, mesh);
}

void LODInstancedRenderer::UploadInstanceMatrices(
    const std::vector<Matrix4>& matrices,
    Ref<Mesh> mesh
) {
    if (matrices.empty() || !mesh) {
        return;
    }
    
    GL_THREAD_CHECK();
    
    // ✅ 编译时断言：确保Matrix4内存布局符合预期
    static_assert(sizeof(Matrix4) == 16 * sizeof(float), 
                  "Matrix4 must be 16 floats (64 bytes)");
    // 注意：Eigen矩阵的对齐要求取决于SIMD指令集（SSE=16字节，AVX=32字节，AVX512=64字节）
    // 但OpenGL缓冲区上传不需要严格对齐，只要数据连续即可
    
    auto& instanceVBOs = GetOrCreateInstanceVBOs(mesh, matrices.size());
    
    size_t requiredSize = matrices.size() * sizeof(Matrix4);
    
    // ✅ 使用持久映射（如果支持且已启用）
    if (m_supportsPersistentMapping && instanceVBOs.usePersistentMapping) {
        if (instanceVBOs.matrixMappedPtr != nullptr) {
            // 直接写入映射内存（零拷贝，最快）
            std::memcpy(instanceVBOs.matrixMappedPtr, matrices.data(), requiredSize);
            
            // 刷新映射范围（GL_MAP_COHERENT_BIT通常不需要，但保险起见）
            glBindBuffer(GL_ARRAY_BUFFER, instanceVBOs.matrixVBO);
            glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, requiredSize);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            
            m_stats.bytesUploaded += requiredSize;
            return;
        }
    }
    
    // ✅ 降级到传统方式（孤儿化）
    if (instanceVBOs.matrixVBO == 0) {
        glGenBuffers(1, &instanceVBOs.matrixVBO);
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBOs.matrixVBO);
    
    // ✅ 策略1：如果大小不变，使用孤儿化 + glBufferSubData
    if (instanceVBOs.capacity == matrices.size() && instanceVBOs.matrixVBO != 0) {
        // 孤儿化：传入nullptr让驱动分配新缓冲区
        glBufferData(GL_ARRAY_BUFFER, requiredSize, nullptr, GL_STREAM_DRAW);
        
        // 立即填充数据（使用新缓冲区，无同步）
        // ✅ 零复制：直接上传矩阵数据
        // Eigen默认是列主序(ColMajor)，与GLSL mat4一致
        glBufferSubData(GL_ARRAY_BUFFER, 0, requiredSize, matrices.data());
    }
    // ✅ 策略2：如果大小变化，直接重新分配
    else {
        // ✅ 零复制：直接上传矩阵数据
        glBufferData(GL_ARRAY_BUFFER, requiredSize, matrices.data(), GL_STREAM_DRAW);
        instanceVBOs.capacity = matrices.size();
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    // ✅ 统计上传字节数
    m_stats.bytesUploaded += requiredSize;
    m_stats.vboUploadCount++;
}

void LODInstancedRenderer::UploadInstanceColors(
    const std::vector<Vector4>& colors,
    Ref<Mesh> mesh
) {
    if (colors.empty() || !mesh) {
        return;
    }
    
    GL_THREAD_CHECK();
    
    auto& instanceVBOs = GetOrCreateInstanceVBOs(mesh, colors.size());
    
    size_t requiredSize = colors.size() * sizeof(Vector4);
    
    // ✅ 使用持久映射（如果支持且已启用）
    if (m_supportsPersistentMapping && instanceVBOs.usePersistentMapping) {
        if (instanceVBOs.colorMappedPtr != nullptr) {
            // 直接写入映射内存
            std::memcpy(instanceVBOs.colorMappedPtr, colors.data(), requiredSize);
            
            glBindBuffer(GL_ARRAY_BUFFER, instanceVBOs.colorVBO);
            glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, requiredSize);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            
            m_stats.bytesUploaded += requiredSize;
            return;
        }
    }
    
    // ✅ 降级到传统方式
    if (instanceVBOs.colorVBO == 0) {
        glGenBuffers(1, &instanceVBOs.colorVBO);
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBOs.colorVBO);
    
    // ✅ 孤儿化优化
    if (instanceVBOs.colorCapacity == colors.size() && instanceVBOs.colorVBO != 0) {
        glBufferData(GL_ARRAY_BUFFER, requiredSize, nullptr, GL_STREAM_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, requiredSize, colors.data());
    } else {
        glBufferData(GL_ARRAY_BUFFER, requiredSize, colors.data(), GL_STREAM_DRAW);
        instanceVBOs.colorCapacity = colors.size();
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    m_stats.bytesUploaded += requiredSize;
    m_stats.vboUploadCount++;
}

void LODInstancedRenderer::UploadInstanceCustomParams(
    const std::vector<Vector4>& customParams,
    Ref<Mesh> mesh
) {
    if (customParams.empty() || !mesh) {
        return;
    }
    
    GL_THREAD_CHECK();
    
    auto& instanceVBOs = GetOrCreateInstanceVBOs(mesh, customParams.size());
    
    size_t requiredSize = customParams.size() * sizeof(Vector4);
    
    // ✅ 使用持久映射（如果支持且已启用）
    if (m_supportsPersistentMapping && instanceVBOs.usePersistentMapping) {
        if (instanceVBOs.paramsMappedPtr != nullptr) {
            // 直接写入映射内存
            std::memcpy(instanceVBOs.paramsMappedPtr, customParams.data(), requiredSize);
            
            glBindBuffer(GL_ARRAY_BUFFER, instanceVBOs.paramsVBO);
            glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, requiredSize);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            
            m_stats.bytesUploaded += requiredSize;
            return;
        }
    }
    
    // ✅ 降级到传统方式
    if (instanceVBOs.paramsVBO == 0) {
        glGenBuffers(1, &instanceVBOs.paramsVBO);
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBOs.paramsVBO);
    
    // ✅ 孤儿化优化
    if (instanceVBOs.paramsCapacity == customParams.size() && instanceVBOs.paramsVBO != 0) {
        glBufferData(GL_ARRAY_BUFFER, requiredSize, nullptr, GL_STREAM_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, requiredSize, customParams.data());
    } else {
        glBufferData(GL_ARRAY_BUFFER, requiredSize, customParams.data(), GL_STREAM_DRAW);
        instanceVBOs.paramsCapacity = customParams.size();
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    m_stats.bytesUploaded += requiredSize;
    m_stats.vboUploadCount++;
}

LODInstancedRenderer::InstanceVBOs& LODInstancedRenderer::GetOrCreateInstanceVBOs(
    Ref<Mesh> mesh,
    size_t requiredCapacity
) {
    auto it = m_instanceVBOs.find(mesh);
    if (it == m_instanceVBOs.end()) {
        // 创建新的 VBO 结构
        InstanceVBOs vbos;
        vbos.capacity = requiredCapacity;
        vbos.usePersistentMapping = m_supportsPersistentMapping;
        
        // ✅ 如果支持持久映射，创建持久映射缓冲区
        if (m_supportsPersistentMapping) {
            CreatePersistentMappedVBOs(vbos, requiredCapacity);
        }
        
        m_instanceVBOs[mesh] = vbos;
        return m_instanceVBOs[mesh];
    }
    
    // 如果容量不足，需要重新分配
    if (it->second.capacity < requiredCapacity) {
        if (it->second.usePersistentMapping) {
            // ✅ 需要重新创建持久映射缓冲区
            DestroyPersistentMappedVBOs(it->second);
            CreatePersistentMappedVBOs(it->second, requiredCapacity);
        } else {
            // 传统方式：只更新容量，VBO会在上传时重新创建
            it->second.capacity = requiredCapacity;
            it->second.colorCapacity = requiredCapacity;
            it->second.paramsCapacity = requiredCapacity;
        }
    }
    
    return it->second;
}

GLuint LODInstancedRenderer::GetOrCreateInstancedVAO(
    Ref<Mesh> mesh,
    InstanceVBOs& instanceVBOs
) {
    GL_THREAD_CHECK();
    
    // 获取基础VAO
    GLuint baseVAO = mesh->GetVertexArrayID();
    if (baseVAO == 0) {
        LOG_WARNING("LODInstancedRenderer: Base mesh VAO is invalid");
        return 0;
    }
    
    // ✅ 检查基础VAO是否改变（如果Mesh重新上传了）
    bool baseVAOChanged = (instanceVBOs.instancedVAO != 0 && instanceVBOs.instancedVAO != baseVAO);
    
    // 如果已经设置且基础VAO未改变，直接返回
    if (!baseVAOChanged && instanceVBOs.instancedVAO != 0 && instanceVBOs.attributesSetup) {
        return instanceVBOs.instancedVAO;
    }
    
    // ✅ 方案：直接使用基础VAO，在其上设置实例化属性
    // 注意：实例化属性（location 6-11）与基础属性（location 0-5）不冲突
    // 实例化属性设置保存在基础VAO中，但这不会影响Mesh的正常使用
    
    // 更新基础VAO引用
    instanceVBOs.instancedVAO = baseVAO;
    
    // 如果基础VAO改变，需要重新设置属性
    if (baseVAOChanged) {
        instanceVBOs.attributesSetup = false;
    }
    
    // 如果属性尚未设置，现在设置
    if (!instanceVBOs.attributesSetup) {
        glBindVertexArray(baseVAO);
        
        // 设置实例矩阵属性（location 6-9）
        if (instanceVBOs.matrixVBO != 0) {
            glBindBuffer(GL_ARRAY_BUFFER, instanceVBOs.matrixVBO);
            for (int i = 0; i < 4; ++i) {
                GLuint location = 6 + i;
                glEnableVertexAttribArray(location);
                glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE,
                                      sizeof(float) * 16,
                                      (void*)(sizeof(float) * 4 * i));
                glVertexAttribDivisor(location, 1);
            }
        }
        
        // 设置实例颜色属性（location 10）
        if (instanceVBOs.colorVBO != 0) {
            glBindBuffer(GL_ARRAY_BUFFER, instanceVBOs.colorVBO);
            glEnableVertexAttribArray(10);
            glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, sizeof(Vector4), 0);
            glVertexAttribDivisor(10, 1);
        }
        
        // 设置自定义参数属性（location 11）
        if (instanceVBOs.paramsVBO != 0) {
            glBindBuffer(GL_ARRAY_BUFFER, instanceVBOs.paramsVBO);
            glEnableVertexAttribArray(11);
            glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE, sizeof(Vector4), 0);
            glVertexAttribDivisor(11, 1);
        }
        
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        instanceVBOs.attributesSetup = true;
    }
    
    return instanceVBOs.instancedVAO;
}

void LODInstancedRenderer::CreatePersistentMappedVBOs(
    InstanceVBOs& vbos,
    size_t capacity
) {
    GL_THREAD_CHECK();
    
    // ✅ 使用持久映射标志
    GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    GLbitfield storageFlags = flags | GL_DYNAMIC_STORAGE_BIT;
    
    // 创建矩阵VBO
    if (vbos.matrixVBO == 0) {
        glGenBuffers(1, &vbos.matrixVBO);
    }
    glBindBuffer(GL_ARRAY_BUFFER, vbos.matrixVBO);
    glBufferStorage(GL_ARRAY_BUFFER, capacity * sizeof(Matrix4), nullptr, storageFlags);
    vbos.matrixMappedPtr = glMapBufferRange(GL_ARRAY_BUFFER, 0, 
                                             capacity * sizeof(Matrix4), flags);
    
    // 创建颜色VBO
    if (vbos.colorVBO == 0) {
        glGenBuffers(1, &vbos.colorVBO);
    }
    glBindBuffer(GL_ARRAY_BUFFER, vbos.colorVBO);
    glBufferStorage(GL_ARRAY_BUFFER, capacity * sizeof(Vector4), nullptr, storageFlags);
    vbos.colorMappedPtr = glMapBufferRange(GL_ARRAY_BUFFER, 0, 
                                            capacity * sizeof(Vector4), flags);
    
    // 创建参数VBO
    if (vbos.paramsVBO == 0) {
        glGenBuffers(1, &vbos.paramsVBO);
    }
    glBindBuffer(GL_ARRAY_BUFFER, vbos.paramsVBO);
    glBufferStorage(GL_ARRAY_BUFFER, capacity * sizeof(Vector4), nullptr, storageFlags);
    vbos.paramsMappedPtr = glMapBufferRange(GL_ARRAY_BUFFER, 0, 
                                             capacity * sizeof(Vector4), flags);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    vbos.capacity = capacity;
    vbos.colorCapacity = capacity;
    vbos.paramsCapacity = capacity;
    vbos.usePersistentMapping = true;
    
    // LOG_DEBUG_F("Created persistent mapped VBOs for %zu instances", capacity);
}

void LODInstancedRenderer::DestroyPersistentMappedVBOs(InstanceVBOs& vbos) {
    GL_THREAD_CHECK();
    
    if (vbos.matrixVBO != 0) {
        if (vbos.matrixMappedPtr != nullptr) {
            glBindBuffer(GL_ARRAY_BUFFER, vbos.matrixVBO);
            glUnmapBuffer(GL_ARRAY_BUFFER);
            vbos.matrixMappedPtr = nullptr;
        }
        glDeleteBuffers(1, &vbos.matrixVBO);
        vbos.matrixVBO = 0;
    }
    
    if (vbos.colorVBO != 0) {
        if (vbos.colorMappedPtr != nullptr) {
            glBindBuffer(GL_ARRAY_BUFFER, vbos.colorVBO);
            glUnmapBuffer(GL_ARRAY_BUFFER);
            vbos.colorMappedPtr = nullptr;
        }
        glDeleteBuffers(1, &vbos.colorVBO);
        vbos.colorVBO = 0;
    }
    
    if (vbos.paramsVBO != 0) {
        if (vbos.paramsMappedPtr != nullptr) {
            glBindBuffer(GL_ARRAY_BUFFER, vbos.paramsVBO);
            glUnmapBuffer(GL_ARRAY_BUFFER);
            vbos.paramsMappedPtr = nullptr;
        }
        glDeleteBuffers(1, &vbos.paramsVBO);
        vbos.paramsVBO = 0;
    }
    
    vbos.usePersistentMapping = false;
}

void LODInstancedRenderer::ClearInstanceVBOs() {
    GL_THREAD_CHECK();
    
    for (auto& [mesh, vbos] : m_instanceVBOs) {
        // ✅ 根据是否使用持久映射选择清理方式
        if (vbos.usePersistentMapping) {
            DestroyPersistentMappedVBOs(vbos);
        } else {
            // 传统清理方式
            if (vbos.matrixVBO != 0) {
                glDeleteBuffers(1, &vbos.matrixVBO);
                vbos.matrixVBO = 0;
            }
            if (vbos.colorVBO != 0) {
                glDeleteBuffers(1, &vbos.colorVBO);
                vbos.colorVBO = 0;
            }
            if (vbos.paramsVBO != 0) {
                glDeleteBuffers(1, &vbos.paramsVBO);
                vbos.paramsVBO = 0;
            }
        }
        
        // ✅ 注意：不删除instancedVAO，因为它就是基础VAO（属于Mesh）
        vbos.capacity = 0;
        vbos.colorCapacity = 0;
        vbos.paramsCapacity = 0;
        vbos.instancedVAO = 0;
        vbos.attributesSetup = false;
    }
    
    m_instanceVBOs.clear();
}

void LODInstancedRenderer::SetupInstanceAttributes(
    uint32_t vao,
    const InstanceVBOs& instanceVBOs,
    size_t instanceCount,
    RenderState* renderState
) {
    GL_THREAD_CHECK();
    
    // ✅ 注意：VAO 必须已经绑定（由调用者 RenderGroup 负责）
    // 这里只需要设置实例化属性指针
    
    // 设置实例矩阵属性（location 6-9）
    // 每个矩阵占用 4 个 vec4（location 6, 7, 8, 9）
    // Eigen 矩阵是列主序存储的，所以每一列作为一个 vec4 属性
    if (instanceVBOs.matrixVBO != 0) {
        // ✅ 绑定实例化矩阵 VBO
        // 注意：VBO 绑定不需要通过 RenderState，因为这是数据上传阶段
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBOs.matrixVBO);
        
        // 矩阵的每一列作为一个 vec4 属性（列主序，GLSL mat4构造函数期望的格式）
        // 数据按列存储：列0在0-3，列1在4-7，列2在8-11，列3在12-15
        for (int i = 0; i < 4; ++i) {
            GLuint location = 6 + i;  // location 6, 7, 8, 9
            glEnableVertexAttribArray(location);
            glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, 
                                  sizeof(float) * 16,  // 每个矩阵 16 个 float
                                  (void*)(sizeof(float) * 4 * i));  // 每列偏移（列主序）
            glVertexAttribDivisor(location, 1);  // 每个实例更新一次
        }
        
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    
    // 设置实例颜色属性（location 10）
    if (instanceVBOs.colorVBO != 0) {
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBOs.colorVBO);
        
        GLuint location = 10;
        glEnableVertexAttribArray(location);
        glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, 
                              sizeof(Vector4), 0);
        glVertexAttribDivisor(location, 1);  // 每个实例更新一次
        
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    
    // 设置自定义参数属性（location 11）
    if (instanceVBOs.paramsVBO != 0) {
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBOs.paramsVBO);
        
        GLuint location = 11;
        glEnableVertexAttribArray(location);
        glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, 
                              sizeof(Vector4), 0);
        glVertexAttribDivisor(location, 1);  // 每个实例更新一次
        
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    
    // ✅ 注意：不解绑 VAO，因为调用者（RenderGroup）已经绑定了 VAO
    // 这里只是设置属性指针，VAO 的绑定由调用者管理
}

} // namespace Render

