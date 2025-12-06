/*
 * Copyright (c) 2025 Li Chaoyu
 * 
 * This file is part of Render.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * For commercial licensing, please contact: 2052046346@qq.com
 */
#include "render/lod_instanced_renderer.h"
#include "render/mesh.h"
#include "render/material.h"
#include "render/material_sort_key.h"
#include "render/material_state_cache.h"
#include "render/renderer.h"
#include "render/render_state.h"
#include "render/logger.h"
#include "render/gl_thread_checker.h"
#include "render/shader.h"
#include "render/camera.h"
#include "render/file_utils.h"
#include <glad/glad.h>
#include <algorithm>
#include <cstring>
#include <cstddef>
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
    
    // ✅ 阶段3.3：检查是否支持Compute Shader（OpenGL 4.3+）
    m_supportsComputeShader = (major > 4) || (major == 4 && minor >= 3);
    
    if (m_supportsComputeShader) {
        LOG_INFO("LODInstancedRenderer: Compute Shader supported (OpenGL " + 
                 std::to_string(major) + "." + std::to_string(minor) + 
                 "), GPU culling available");
    } else {
        LOG_INFO("LODInstancedRenderer: Compute Shader not available (OpenGL " + 
                 std::to_string(major) + "." + std::to_string(minor) + 
                 "), GPU culling disabled");
    }
}

LODInstancedRenderer::~LODInstancedRenderer() {
    // ✅ 不再需要DisableMultithreading()，TaskScheduler由全局管理
    CleanupGPUCulling();
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
    
    m_frameCounter++;
    
    // ✅ 重置统计
    m_stats.vboUploadCount = 0;
    m_stats.bytesUploaded = 0;
    m_stats.uploadTimeMs = 0.0f;
    m_stats.sortTimeMs = 0.0f;
    m_stats.renderTimeMs = 0.0f;
    
    ScopedTimer totalTimer(m_stats.renderTimeMs);
    
    // ✅ 重置当前帧处理计数
    m_currentFrameProcessed = 0;
    
    // ✅ 处理待处理队列，添加到构建缓冲区
    size_t processCount = std::min(m_maxInstancesPerFrame, m_pendingInstances.size());
    
    // ✅ 使用TaskScheduler进行并行处理
    const size_t minInstancesForParallel = 100;
    
    if (processCount >= minInstancesForParallel && TaskScheduler::GetInstance().IsInitialized()) {
        // ✅ 并行模式：使用TaskScheduler
        const size_t numThreads = TaskScheduler::GetInstance().GetWorkerCount();
        const size_t batchSize = std::max(size_t(50), processCount / numThreads);
        
        std::vector<std::shared_ptr<TaskHandle>> taskHandles;
        taskHandles.reserve((processCount + batchSize - 1) / batchSize);
        
        for (size_t i = 0; i < processCount; i += batchSize) {
            const size_t endIdx = std::min(i + batchSize, processCount);
            
            // 复制当前批次的实例
            std::vector<PendingInstance> batch;
            batch.reserve(endIdx - i);
            for (size_t j = i; j < endIdx; ++j) {
                batch.push_back(m_pendingInstances[j]);
            }
            
            // 提交并行任务
            auto handle = TaskScheduler::GetInstance().SubmitLambda(
                [this, batch = std::move(batch)]() {
                    ProcessInstanceBatch(batch, &m_groups[m_currentBuildBuffer]);
                },
                TaskPriority::High,
                "LODPrepare"
            );
            
            taskHandles.push_back(handle);
        }
        
        // ✅ 等待所有任务完成
        TaskScheduler::GetInstance().WaitForAll(taskHandles);
        
        m_currentFrameProcessed += processCount;
        
        if (processCount > 0) {
            m_pendingInstances.erase(
                m_pendingInstances.begin(),
                m_pendingInstances.begin() + processCount
            );
        }
    } else {
        // ✅ 串行模式：直接处理（小规模场景或TaskScheduler未初始化）
        for (size_t i = 0; i < processCount; ++i) {
            const auto& pending = m_pendingInstances[i];
            
            AddInstanceToGroup(
                pending.entity,
                pending.mesh,
                pending.material,
                pending.instanceData,
                pending.lodLevel
            );
            
            m_currentFrameProcessed++;
        }
        
        if (processCount > 0) {
            m_pendingInstances.erase(
                m_pendingInstances.begin(),
                m_pendingInstances.begin() + processCount
            );
        }
    }
    
    // ✅ 先交换缓冲区（让构建缓冲区变成渲染缓冲区）
    // 这样第一帧也能正确渲染（因为第一帧时构建缓冲区有数据，渲染缓冲区是空的）
    std::swap(m_currentRenderBuffer, m_currentBuildBuffer);
    
    // ✅ 从渲染缓冲区获取要渲染的组
    auto& renderGroups = m_groups[m_currentRenderBuffer];
    
    if (renderGroups.empty() && m_pendingInstances.empty()) {
        return;
    }
    
    // 排序
    std::vector<LODInstancedGroup*> sortedGroups;
    sortedGroups.reserve(renderGroups.size());
    
    for (auto& [key, group] : renderGroups) {
        if (!group.IsEmpty()) {
            sortedGroups.push_back(&group);
        }
    }
    
    if (sortedGroups.empty()) {
        return;
    }
    
    {
        ScopedTimer sortTimer(m_stats.sortTimeMs);
        
        MaterialSortKeyLess less;
        std::sort(sortedGroups.begin(), sortedGroups.end(),
            [&less](const LODInstancedGroup* a, const LODInstancedGroup* b) {
                if (less(a->sortKey, b->sortKey) || less(b->sortKey, a->sortKey)) {
                    return less(a->sortKey, b->sortKey);
                }
                return static_cast<int>(a->lodLevel) < static_cast<int>(b->lodLevel);
            });
    }
    
    // 渲染
    for (auto* group : sortedGroups) {
        RenderGroup(group, renderer, renderState);
    }
    
    // ✅ 清空构建缓冲区（为下一帧做准备）
    // 注意：此时构建缓冲区是上一帧的渲染缓冲区，已经渲染过了，可以清空
    for (auto& [key, group] : m_groups[m_currentBuildBuffer]) {
        group.Clear();
    }
}

void LODInstancedRenderer::Clear() {
    ClearInstanceVBOs();
    
    // ✅ 清空两个缓冲区
    m_groups[0].clear();
    m_groups[1].clear();
    
    m_pendingInstances.clear();
    m_currentFrameProcessed = 0;
    m_frameCounter = 0;
}

LODInstancedRenderer::Stats LODInstancedRenderer::GetStats() const {
    Stats stats = m_stats;
    
    // ✅ 从渲染缓冲区统计（因为这是正在显示的）
    const auto& renderGroups = m_groups[m_currentRenderBuffer];
    
    stats.groupCount = renderGroups.size();
    stats.pendingCount = m_pendingInstances.size();
    stats.totalInstances = 0;
    stats.drawCalls = 0;
    
    stats.lod0Instances = 0;
    stats.lod1Instances = 0;
    stats.lod2Instances = 0;
    stats.lod3Instances = 0;
    stats.culledCount = 0;
    
    for (const auto& [key, group] : renderGroups) {
        size_t instanceCount = group.GetInstanceCount();
        stats.totalInstances += instanceCount;
        stats.drawCalls++;
        
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
    
    if (stats.totalInstances > stats.peakInstanceCount) {
        stats.peakInstanceCount = stats.totalInstances;
    }
    
    // 计算两个缓冲区的内存使用
    stats.totalAllocatedMemory = 0;
    for (int i = 0; i < 2; ++i) {
        for (const auto& [key, group] : m_groups[i]) {
            stats.totalAllocatedMemory += 
                group.instances.capacity() * sizeof(InstanceData) +
                group.entities.capacity() * sizeof(ECS::EntityID);
        }
    }
    
    return stats;
}

size_t LODInstancedRenderer::GetInstanceCount(LODLevel lodLevel) const {
    size_t count = 0;
    
    // ✅ 从渲染缓冲区统计
    const auto& renderGroups = m_groups[m_currentRenderBuffer];
    
    for (const auto& [key, group] : renderGroups) {
        if (group.lodLevel == lodLevel) {
            count += group.GetInstanceCount();
        }
    }
    
    return count;
}

size_t LODInstancedRenderer::GetGroupCount() const {
    // ✅ 从渲染缓冲区统计
    return m_groups[m_currentRenderBuffer].size();
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
    
    // ✅ 添加到构建缓冲区（如果多线程启用，需要加锁）
    std::lock_guard<std::mutex> lock(m_buildBufferMutex);
    
    auto& group = m_groups[m_currentBuildBuffer][key];
    
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
    
    // 使用MaterialStateCache避免重复绑定材质
    auto& stateCache = MaterialStateCache::Get();
    if (stateCache.ShouldBind(group->material.get(), renderState)) {
        group->material->Bind(renderState);
        stateCache.OnBind(group->material.get(), renderState);
    }
    
    // 设置实例化标志
    if (auto shader = group->material->GetShader()) {
        if (auto uniformMgr = shader->GetUniformManager()) {
            uniformMgr->SetBool("uHasInstanceData", true);
            uniformMgr->SetMatrix4("uModel", Matrix4::Identity());
        }
    }
    
    // 仅在需要时上传数据
    if (group->NeedsUpload()) {
        ScopedTimer uploadTimer(m_stats.uploadTimeMs);
        UploadInstanceData(group->instances, group->mesh);
        group->MarkUploaded();
    }
    
    size_t instanceCount = group->instances.size();
    auto& instanceVBOs = GetOrCreateInstanceVBOs(group->mesh, instanceCount);
    
    // 获取或创建实例化VAO（传入material以查询着色器中的实例化属性location）
    GLuint vao = GetOrCreateInstancedVAO(group->mesh, group->material, instanceVBOs);
    
    if (vao == 0) {
        LOG_WARNING("LODInstancedRenderer: Failed to create instanced VAO");
        return;
    }
    
    // 绑定VAO
    if (renderState) {
        renderState->BindVertexArray(vao);
    } else {
        glBindVertexArray(vao);
    }
    
    // ✅ 直接使用OpenGL绘制，而不是通过Mesh::DrawInstanced
    // 因为DrawInstanced会绑定Mesh自己的VAO，覆盖我们绑定的实例化VAO
    size_t indexCount = group->mesh->GetIndexCount();
    if (indexCount > 0) {
        // 使用索引绘制实例
        glDrawElementsInstanced(GL_TRIANGLES, 
                                static_cast<GLsizei>(indexCount), 
                                GL_UNSIGNED_INT, 
                                0, 
                                static_cast<GLsizei>(instanceCount));
    } else {
        // 直接绘制顶点实例
        size_t vertexCount = group->mesh->GetVertexCount();
        glDrawArraysInstanced(GL_TRIANGLES, 
                              0, 
                              static_cast<GLsizei>(vertexCount), 
                              static_cast<GLsizei>(instanceCount));
    }
    
    // ✅ 关键修复：在解绑VAO之前，确保禁用所有实例化属性（location 6-11）
    // 这样可以确保即使后续代码错误地使用了基础VAO，也不会受到实例化属性的影响
    // 注意：虽然VAO解绑后属性状态会被保存，但为了安全起见，我们显式禁用
    // 实际上，由于我们使用的是独立的实例化VAO，这一步主要是防御性编程
    
    // 解绑VAO
    if (renderState) {
        renderState->BindVertexArray(0);
    } else {
        glBindVertexArray(0);
    }
    
    // ✅ 完全清理OpenGL状态，确保不会影响后续渲染
    // 解绑所有缓冲区
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    
    // ✅ 额外保护：确保实例化属性（location 6-11）在全局状态中被禁用
    // 虽然VAO解绑后这些属性状态会被保存到VAO中，但为了确保基础VAO不受影响，
    // 我们在解绑后显式禁用这些属性（如果当前没有绑定VAO，这些调用会被忽略，但更安全）
    // 注意：实际上，由于我们使用的是独立的实例化VAO，这一步主要是防御性编程
    // 如果当前没有绑定VAO，glDisableVertexAttribArray会被忽略，不会影响任何VAO
    for (int i = 6; i <= 11; ++i) {
        glDisableVertexAttribArray(i);
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
    Ref<Material> material,
    InstanceVBOs& instanceVBOs
) {
    GL_THREAD_CHECK();
    
    // 获取基础VAO
    GLuint baseVAO = mesh->GetVertexArrayID();
    if (baseVAO == 0) {
        LOG_WARNING("LODInstancedRenderer: Base mesh VAO is invalid");
        return 0;
    }
    
    // ✅ 从着色器查询实例化属性的location
    // 支持不同着色器的location定义（如basic.vert使用4-7，material_phong.vert使用6-9）
    GLint instanceMatrixLocations[4] = {-1, -1, -1, -1};
    GLint instanceColorLocation = -1;
    GLint instanceParamsLocation = -1;
    
    if (material && material->GetShader()) {
        auto shader = material->GetShader();
        if (shader->IsValid()) {
            GLuint programID = shader->GetProgramID();
            
            // 查询实例化矩阵的location（尝试多种可能的名称）
            const char* instanceRowNames[4] = {
                "aInstanceRow0", "aInstanceRow1", "aInstanceRow2", "aInstanceRow3"
            };
            for (int i = 0; i < 4; ++i) {
                instanceMatrixLocations[i] = glGetAttribLocation(programID, instanceRowNames[i]);
            }
            
            // 查询实例化颜色和参数的location
            instanceColorLocation = glGetAttribLocation(programID, "aInstanceColor");
            instanceParamsLocation = glGetAttribLocation(programID, "aInstanceParams");
        }
    }
    
    // ✅ 如果查询失败，回退到硬编码的location（向后兼容）
    // 默认使用location 6-9作为实例化矩阵（与material_phong.vert匹配）
    bool useFallback = false;
    for (int i = 0; i < 4; ++i) {
        if (instanceMatrixLocations[i] == -1) {
            useFallback = true;
            break;
        }
    }
    
    if (useFallback) {
        // 回退到硬编码的location 6-9
        for (int i = 0; i < 4; ++i) {
            instanceMatrixLocations[i] = 6 + i;
        }
        if (instanceColorLocation == -1) {
            instanceColorLocation = 10;
        }
        if (instanceParamsLocation == -1) {
            instanceParamsLocation = 11;
        }
    }
    
    // ✅ 如果instancedVAO为0，创建新的独立VAO
    // 注意：我们需要为每个不同的location组合创建不同的VAO
    // 这里简化处理，假设同一mesh+material组合的location是固定的
    if (instanceVBOs.instancedVAO == 0) {
        glGenVertexArrays(1, &instanceVBOs.instancedVAO);
        instanceVBOs.attributesSetup = false;
    }
    
    // 如果属性已经设置，直接返回
    if (instanceVBOs.attributesSetup) {
        return instanceVBOs.instancedVAO;
    }
    
    // ✅ 保存当前OpenGL状态（VAO、VBO、EBO）
    GLint currentVAO = 0;
    GLint currentArrayBuffer = 0;
    GLint currentElementArrayBuffer = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &currentVAO);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &currentArrayBuffer);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &currentElementArrayBuffer);
    
    // ✅ 步骤1：查询基础VAO的配置（只读操作，不修改状态）
    glBindVertexArray(baseVAO);
    
    // 查询EBO（必须在绑定VAO后查询）
    GLint meshEBO = 0;
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &meshEBO);
    
    // 查询并保存所有启用的顶点属性配置（location 0-5）
    struct VertexAttribConfig {
        GLint enabled;
        GLint size;
        GLint type;
        GLint normalized;
        GLint stride;
        GLvoid* pointer;
        GLint bufferBinding;
        GLint divisor;  // ✅ 新增：保存divisor状态
    };
    
    VertexAttribConfig attribConfigs[6] = {};
    
    for (int i = 0; i < 6; ++i) {
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &attribConfigs[i].enabled);
        
        if (attribConfigs[i].enabled) {
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_SIZE, &attribConfigs[i].size);
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_TYPE, &attribConfigs[i].type);
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &attribConfigs[i].normalized);
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &attribConfigs[i].stride);
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &attribConfigs[i].bufferBinding);
            glGetVertexAttribPointerv(i, GL_VERTEX_ATTRIB_ARRAY_POINTER, &attribConfigs[i].pointer);
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &attribConfigs[i].divisor);  // ✅ 保存divisor
        }
    }
    
    // ✅ 关键修复：立即恢复基础VAO绑定，确保不会影响基础VAO状态
    // 在查询完成后立即恢复，避免任何潜在的状态污染
    if (currentVAO != 0) {
        glBindVertexArray(static_cast<GLuint>(currentVAO));
    } else {
        glBindVertexArray(0);
    }
    
    // ✅ 步骤2：绑定新的实例化VAO并应用配置
    glBindVertexArray(instanceVBOs.instancedVAO);
    
    // 复制基础顶点属性配置（location 0-5）
    for (int i = 0; i < 6; ++i) {
        if (attribConfigs[i].enabled) {
            // 绑定相应的VBO
            glBindBuffer(GL_ARRAY_BUFFER, attribConfigs[i].bufferBinding);
            
            // 设置属性指针
            glEnableVertexAttribArray(i);
            glVertexAttribPointer(
                i,
                attribConfigs[i].size,
                attribConfigs[i].type,
                attribConfigs[i].normalized,
                attribConfigs[i].stride,
                attribConfigs[i].pointer
            );
            
            // ✅ 确保基础属性的divisor为0（非实例化）
            // 即使基础VAO的divisor可能不是0，我们也强制设置为0，确保实例化VAO的独立性
            glVertexAttribDivisor(i, 0);
        } else {
            // ✅ 确保未启用的属性在实例化VAO中也被禁用
            glDisableVertexAttribArray(i);
        }
    }
    
    // 绑定索引缓冲区
    if (meshEBO != 0) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshEBO);
    }
    
    // ✅ 步骤3：设置实例化属性（使用从着色器查询到的location）
    // 注意：确保实例化属性在基础VAO中是禁用的
    
    // 实例化矩阵（使用查询到的location）
    if (instanceVBOs.matrixVBO != 0) {
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBOs.matrixVBO);
        for (int i = 0; i < 4; ++i) {
            GLuint location = static_cast<GLuint>(instanceMatrixLocations[i]);
            if (instanceMatrixLocations[i] != -1) {
                glEnableVertexAttribArray(location);
                glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE,
                                      sizeof(float) * 16,
                                      (void*)(sizeof(float) * 4 * i));
                glVertexAttribDivisor(location, 1);  // ✅ 关键：设置为实例化属性
            }
        }
    } else {
        // ✅ 如果矩阵VBO不存在，确保这些属性被禁用
        for (int i = 0; i < 4; ++i) {
            GLuint location = static_cast<GLuint>(instanceMatrixLocations[i]);
            if (instanceMatrixLocations[i] != -1) {
                glDisableVertexAttribArray(location);
            }
        }
    }
    
    // 实例化颜色（使用查询到的location）
    if (instanceVBOs.colorVBO != 0 && instanceColorLocation != -1) {
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBOs.colorVBO);
        glEnableVertexAttribArray(static_cast<GLuint>(instanceColorLocation));
        glVertexAttribPointer(static_cast<GLuint>(instanceColorLocation), 4, GL_FLOAT, GL_FALSE, sizeof(Vector4), 0);
        glVertexAttribDivisor(static_cast<GLuint>(instanceColorLocation), 1);
    } else if (instanceColorLocation != -1) {
        glDisableVertexAttribArray(static_cast<GLuint>(instanceColorLocation));
    }
    
    // 自定义参数（使用查询到的location）
    if (instanceVBOs.paramsVBO != 0 && instanceParamsLocation != -1) {
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBOs.paramsVBO);
        glEnableVertexAttribArray(static_cast<GLuint>(instanceParamsLocation));
        glVertexAttribPointer(static_cast<GLuint>(instanceParamsLocation), 4, GL_FLOAT, GL_FALSE, sizeof(Vector4), 0);
        glVertexAttribDivisor(static_cast<GLuint>(instanceParamsLocation), 1);
    } else if (instanceParamsLocation != -1) {
        glDisableVertexAttribArray(static_cast<GLuint>(instanceParamsLocation));
    }
    
    // ✅ 步骤4：完全恢复之前的OpenGL状态
    // 恢复VAO绑定
    if (currentVAO != 0) {
        glBindVertexArray(static_cast<GLuint>(currentVAO));
    } else {
        glBindVertexArray(0);
    }
    
    // 恢复VBO绑定
    if (currentArrayBuffer != 0) {
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(currentArrayBuffer));
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    
    // 恢复EBO绑定（注意：EBO绑定是VAO的一部分，但为了安全起见，我们也恢复它）
    if (currentElementArrayBuffer != 0) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(currentElementArrayBuffer));
    } else {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
    
    instanceVBOs.attributesSetup = true;
    
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
        // 根据是否使用持久映射选择清理方式
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
        
        // ✅ 删除独立创建的实例化VAO
        if (vbos.instancedVAO != 0) {
            glDeleteVertexArrays(1, &vbos.instancedVAO);
            vbos.instancedVAO = 0;
        }
        
        vbos.capacity = 0;
        vbos.colorCapacity = 0;
        vbos.paramsCapacity = 0;
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

// ==================== 多线程数据准备 ====================

// ✅ EnableMultithreading, DisableMultithreading, WorkerThreadFunction 已移除
// 所有多线程功能现在由TaskScheduler统一管理

void LODInstancedRenderer::ProcessInstanceBatch(
    const std::vector<PendingInstance>& instances,
    std::map<GroupKey, LODInstancedGroup>* targetGroups)
{
    // 在工作线程中准备数据
    // 注意：需要加锁保护构建缓冲区的访问
    std::lock_guard<std::mutex> lock(m_buildBufferMutex);
    
    for (const auto& pending : instances) {
        MaterialSortKey sortKey = GenerateSortKey(pending.material, pending.mesh);
        
        GroupKey key;
        key.mesh = pending.mesh;
        key.material = pending.material;
        key.lodLevel = pending.lodLevel;
        key.sortKey = sortKey;
        
        // 线程安全的访问构建缓冲区
        auto& group = (*targetGroups)[key];
        
        if (group.instances.empty()) {
            group.mesh = pending.mesh;
            group.material = pending.material;
            group.lodLevel = pending.lodLevel;
            group.sortKey = sortKey;
            
            // ✅ 根据预估数量预分配内存
            size_t estimatedInstancesPerGroup = m_estimatedInstanceCount / 
                std::max(m_estimatedGroupCount, size_t(1));
            estimatedInstancesPerGroup = std::max(estimatedInstancesPerGroup, size_t(16));
            
            group.instances.reserve(estimatedInstancesPerGroup);
            group.entities.reserve(estimatedInstancesPerGroup);
        }
        
        group.instances.push_back(pending.instanceData);
        group.entities.push_back(pending.entity);
        group.MarkDirty();
    }
}

// ==================== 阶段3.3：GPU剔除实现 ====================

void LODInstancedRenderer::EnableGPUCulling(bool enable) {
    if (enable == m_gpuCullingEnabled) {
        return;
    }
    
    if (enable) {
        if (!m_supportsComputeShader) {
            LOG_WARNING("LODInstancedRenderer: GPU culling requested but Compute Shader not supported");
            return;
        }
        
        InitGPUCulling();
        m_gpuCullingEnabled = true;
        LOG_INFO("LODInstancedRenderer: GPU culling enabled");
    } else {
        CleanupGPUCulling();
        m_gpuCullingEnabled = false;
        LOG_INFO("LODInstancedRenderer: GPU culling disabled");
    }
}

bool LODInstancedRenderer::IsGPUCullingAvailable() const {
    return m_supportsComputeShader;
}

void LODInstancedRenderer::InitGPUCulling() {
    if (!m_supportsComputeShader) {
        LOG_WARNING("LODInstancedRenderer: Cannot initialize GPU culling - Compute Shader not supported");
        return;
    }
    
    GL_THREAD_CHECK();
    
    // 加载Compute Shader
    m_cullingComputeShader = CreateRef<Shader>();
    if (!m_cullingComputeShader->LoadComputeShaderFromFile("shaders/instance_culling.comp")) {
        LOG_ERROR("LODInstancedRenderer: Failed to load GPU culling compute shader");
        m_cullingComputeShader.reset();
        return;
    }
    
    // 创建SSBO
    glGenBuffers(1, &m_allInstancesSSBO);
    glGenBuffers(1, &m_instanceRadiiSSBO);
    glGenBuffers(1, &m_visibleIndicesSSBO);
    glGenBuffers(1, &m_counterSSBO);
    glGenBuffers(1, &m_lodCountersSSBO);
    
    // 初始容量：10000个实例
    m_gpuCullingMaxInstances = 10000;
    
    // 初始化计数器缓冲区
    struct CounterData {
        uint32_t visibleCount = 0;
        uint32_t lod0Offset = 0;
        uint32_t lod1Offset = 0;
        uint32_t lod2Offset = 0;
        uint32_t lod3Offset = 0;
    } counterInit;
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_counterSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(CounterData), &counterInit, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    // 初始化LOD计数器缓冲区
    struct LODCounterData {
        uint32_t lod0Count = 0;
        uint32_t lod1Count = 0;
        uint32_t lod2Count = 0;
        uint32_t lod3Count = 0;
    } lodCounterInit;
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_lodCountersSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(LODCounterData), &lodCounterInit, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    LOG_INFO("LODInstancedRenderer: GPU culling initialized");
}

void LODInstancedRenderer::CleanupGPUCulling() {
    GL_THREAD_CHECK();
    
    m_cullingComputeShader.reset();
    
    if (m_allInstancesSSBO != 0) {
        glDeleteBuffers(1, &m_allInstancesSSBO);
        m_allInstancesSSBO = 0;
    }
    if (m_instanceRadiiSSBO != 0) {
        glDeleteBuffers(1, &m_instanceRadiiSSBO);
        m_instanceRadiiSSBO = 0;
    }
    if (m_visibleIndicesSSBO != 0) {
        glDeleteBuffers(1, &m_visibleIndicesSSBO);
        m_visibleIndicesSSBO = 0;
    }
    if (m_counterSSBO != 0) {
        glDeleteBuffers(1, &m_counterSSBO);
        m_counterSSBO = 0;
    }
    if (m_lodCountersSSBO != 0) {
        glDeleteBuffers(1, &m_lodCountersSSBO);
        m_lodCountersSSBO = 0;
    }
    
    m_gpuCullingMaxInstances = 0;
}

LODInstancedRenderer::GPUCullingResult LODInstancedRenderer::PerformGPUCulling(
    const Camera* camera,
    const std::vector<Matrix4>& allInstances,
    const std::vector<float>& instanceRadii,
    const std::vector<float>& lodDistances
) {
    GPUCullingResult result;
    
    if (!m_gpuCullingEnabled || !m_cullingComputeShader || !camera || allInstances.empty()) {
        return result;
    }
    
    GL_THREAD_CHECK();
    
    size_t instanceCount = allInstances.size();
    
    // 检查是否需要扩容
    if (instanceCount > m_gpuCullingMaxInstances) {
        // 扩容到实例数的1.5倍，避免频繁扩容
        m_gpuCullingMaxInstances = static_cast<size_t>(instanceCount * 1.5f);
        
        // 重新创建SSBO
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_allInstancesSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, 
                     m_gpuCullingMaxInstances * sizeof(Matrix4), 
                     nullptr, GL_DYNAMIC_DRAW);
        
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_instanceRadiiSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, 
                     m_gpuCullingMaxInstances * sizeof(float), 
                     nullptr, GL_DYNAMIC_DRAW);
        
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_visibleIndicesSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, 
                     m_gpuCullingMaxInstances * sizeof(uint32_t), 
                     nullptr, GL_DYNAMIC_DRAW);
        
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
    
    // 1. 上传所有实例矩阵到SSBO
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_allInstancesSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 
                    instanceCount * sizeof(Matrix4), 
                    allInstances.data());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_allInstancesSSBO);
    
    // 2. 上传实例包围球半径（如果没有提供，使用默认值）
    std::vector<float> radii(instanceCount, 1.0f);
    if (!instanceRadii.empty() && instanceRadii.size() == instanceCount) {
        radii = instanceRadii;
    }
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_instanceRadiiSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 
                    instanceCount * sizeof(float), 
                    radii.data());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_instanceRadiiSSBO);
    
    // 3. 重置计数器并设置LOD偏移
    // 注意：LOD偏移需要预先计算，但这里我们先设为0，然后在读取结果时再计算
    struct CounterData {
        uint32_t visibleCount = 0;
        uint32_t lod0Offset = 0;  // 将在读取结果后计算
        uint32_t lod1Offset = 0;
        uint32_t lod2Offset = 0;
        uint32_t lod3Offset = 0;
    } counterInit;
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_counterSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(CounterData), &counterInit);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_counterSSBO);
    
    struct LODCounterData {
        uint32_t lod0Count = 0;
        uint32_t lod1Count = 0;
        uint32_t lod2Count = 0;
        uint32_t lod3Count = 0;
    } lodCounterInit;
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_lodCountersSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(LODCounterData), &lodCounterInit);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_lodCountersSSBO);
    
    // 4. 绑定可见实例索引SSBO
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_visibleIndicesSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_visibleIndicesSSBO);
    
    // 5. 设置Compute Shader uniforms
    m_cullingComputeShader->Use();
    
    if (auto uniformMgr = m_cullingComputeShader->GetUniformManager()) {
        Matrix4 viewProj = camera->GetViewProjectionMatrix();
        uniformMgr->SetMatrix4("uViewProj", viewProj);
        
        Vector3 cameraPos = camera->GetTransform().GetPosition();
        uniformMgr->SetVector3("uCameraPos", cameraPos);
        
        // 获取视锥体平面
        const Frustum& frustum = camera->GetFrustum();
        Vector4 frustumPlanes[6];
        for (int i = 0; i < 6; ++i) {
            // 平面方程：normal.x * x + normal.y * y + normal.z * z - distance = 0
            // 转换为：normal.x * x + normal.y * y + normal.z * z + (-distance) = 0
            frustumPlanes[i] = Vector4(
                frustum.planes[i].normal.x(),
                frustum.planes[i].normal.y(),
                frustum.planes[i].normal.z(),
                -frustum.planes[i].distance
            );
        }
        uniformMgr->SetVector4Array("uFrustumPlanes", frustumPlanes, 6);
        
        // LOD距离阈值
        if (lodDistances.size() >= 4) {
            uniformMgr->SetFloatArray("uLODDistances", lodDistances.data(), 4);
        } else {
            // 使用默认值
            float defaultDistances[4] = {50.0f, 150.0f, 500.0f, 1000.0f};
            uniformMgr->SetFloatArray("uLODDistances", defaultDistances, 4);
        }
        
        uniformMgr->SetFloat("uDefaultRadius", 1.0f);
    }
    
    // 6. 执行Compute Shader
    uint32_t numGroups = static_cast<uint32_t>((instanceCount + 255) / 256);
    glDispatchCompute(numGroups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    // 7. 读取结果
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_counterSSBO);
    CounterData* counterData = static_cast<CounterData*>(
        glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY)
    );
    
    if (counterData) {
        result.visibleCount = counterData->visibleCount;
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_lodCountersSSBO);
    LODCounterData* lodCounterData = static_cast<LODCounterData*>(
        glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY)
    );
    
    if (lodCounterData) {
        result.lod0Count = lodCounterData->lod0Count;
        result.lod1Count = lodCounterData->lod1Count;
        result.lod2Count = lodCounterData->lod2Count;
        result.lod3Count = lodCounterData->lod3Count;
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        
        // 计算LOD偏移（按顺序排列：LOD0, LOD1, LOD2, LOD3）
        result.lod0Offset = 0;
        result.lod1Offset = result.lod0Count;
        result.lod2Offset = result.lod1Offset + result.lod1Count;
        result.lod3Offset = result.lod2Offset + result.lod2Count;
    }
    
    // 读取可见实例索引
    if (result.visibleCount > 0) {
        result.visibleIndices.resize(result.visibleCount);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_visibleIndicesSSBO);
        uint32_t* indices = static_cast<uint32_t*>(
            glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY)
        );
        
        if (indices) {
            std::memcpy(result.visibleIndices.data(), indices, 
                       result.visibleCount * sizeof(uint32_t));
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }
    }
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    m_cullingComputeShader->Unuse();
    
    return result;
}

} // namespace Render

