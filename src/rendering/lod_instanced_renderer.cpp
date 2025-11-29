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

namespace Render {

// ==================== LODInstancedRenderer 实现 ====================

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
    }
    
    // 添加实例数据
    group.instances.push_back(instanceData);
    group.entities.push_back(entity);
}

void LODInstancedRenderer::RenderAll(Renderer* renderer, RenderState* renderState) {
    if (!renderer) {
        return;
    }
    
    if (m_groups.empty()) {
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
    
    // 按排序键排序
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
    
    // 渲染每个组
    for (auto* group : sortedGroups) {
        RenderGroup(group, renderer, renderState);
    }
}

void LODInstancedRenderer::Clear() {
    ClearInstanceVBOs();
    m_groups.clear();
}

LODInstancedRenderer::Stats LODInstancedRenderer::GetStats() const {
    Stats stats;
    stats.groupCount = m_groups.size();
    
    for (const auto& [key, group] : m_groups) {
        size_t instanceCount = group.GetInstanceCount();
        stats.totalInstances += instanceCount;
        stats.drawCalls++;  // 每个组一次 Draw Call
        
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
    
    // 上传实例数据到 GPU
    UploadInstanceData(group->instances, group->mesh);
    
    // 获取网格的 VAO
    uint32_t vao = group->mesh->GetVertexArrayID();
    if (vao == 0) {
        LOG_WARNING("LODInstancedRenderer: Mesh VAO is invalid");
        return;
    }
    
    // ✅ 使用 RenderState 绑定 VAO（如果提供）
    if (renderState) {
        renderState->BindVertexArray(vao);
    } else {
        glBindVertexArray(vao);
    }
    
    // 设置实例化属性（需要在 VAO 绑定的情况下设置）
    size_t instanceCount = group->instances.size();
    auto& instanceVBOs = GetOrCreateInstanceVBOs(group->mesh, instanceCount);
    SetupInstanceAttributes(vao, instanceVBOs, instanceCount, renderState);
    
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
    
    // 获取或创建实例化 VBO
    auto& instanceVBOs = GetOrCreateInstanceVBOs(mesh, matrices.size());
    
    // 准备矩阵数据（按列上传，每个矩阵 4 列）
    // GLSL的mat4(vec4, vec4, vec4, vec4)构造函数将4个vec4参数作为列向量
    // Eigen矩阵是列主序存储的，可以直接按列上传
    std::vector<float> matrixData;
    matrixData.reserve(matrices.size() * 16);  // 每个矩阵 16 个 float
    
    for (const auto& matrix : matrices) {
        // Eigen 矩阵是列主序存储的，GLSL mat4构造函数也期望列向量
        // 直接按列上传：每一列作为一个vec4
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                matrixData.push_back(matrix(row, col));
            }
        }
    }
    
    // 创建或更新 VBO
    if (instanceVBOs.matrixVBO == 0) {
        glGenBuffers(1, &instanceVBOs.matrixVBO);
    }
    
    // ✅ 注意：这里直接使用 OpenGL API，因为 RenderState 主要用于运行时状态管理
    // VBO 的创建和更新通常在准备阶段完成，不需要通过 RenderState
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBOs.matrixVBO);
    glBufferData(GL_ARRAY_BUFFER, 
                 matrixData.size() * sizeof(float),
                 matrixData.data(),
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    instanceVBOs.capacity = matrices.size();
    
    LOG_DEBUG_F("LODInstancedRenderer: UploadInstanceMatrices - %zu matrices", matrices.size());
}

void LODInstancedRenderer::UploadInstanceColors(
    const std::vector<Vector4>& colors,
    Ref<Mesh> mesh
) {
    if (colors.empty() || !mesh) {
        return;
    }
    
    GL_THREAD_CHECK();
    
    // 获取或创建实例化 VBO
    auto& instanceVBOs = GetOrCreateInstanceVBOs(mesh, colors.size());
    
    // 创建或更新 VBO
    if (instanceVBOs.colorVBO == 0) {
        glGenBuffers(1, &instanceVBOs.colorVBO);
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBOs.colorVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 colors.size() * sizeof(Vector4),
                 colors.data(),
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    LOG_DEBUG_F("LODInstancedRenderer: UploadInstanceColors - %zu colors", colors.size());
}

void LODInstancedRenderer::UploadInstanceCustomParams(
    const std::vector<Vector4>& customParams,
    Ref<Mesh> mesh
) {
    if (customParams.empty() || !mesh) {
        return;
    }
    
    GL_THREAD_CHECK();
    
    // 获取或创建实例化 VBO
    auto& instanceVBOs = GetOrCreateInstanceVBOs(mesh, customParams.size());
    
    // 创建或更新 VBO
    if (instanceVBOs.paramsVBO == 0) {
        glGenBuffers(1, &instanceVBOs.paramsVBO);
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBOs.paramsVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 customParams.size() * sizeof(Vector4),
                 customParams.data(),
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    LOG_DEBUG_F("LODInstancedRenderer: UploadInstanceCustomParams - %zu params", customParams.size());
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
        m_instanceVBOs[mesh] = vbos;
        return m_instanceVBOs[mesh];
    }
    
    // 如果容量不足，需要重新分配
    if (it->second.capacity < requiredCapacity) {
        it->second.capacity = requiredCapacity;
        // VBO 会在上传时重新创建
    }
    
    return it->second;
}

void LODInstancedRenderer::ClearInstanceVBOs() {
    GL_THREAD_CHECK();
    
    for (auto& [mesh, vbos] : m_instanceVBOs) {
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
        vbos.capacity = 0;
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

