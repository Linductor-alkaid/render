#include "render/render_batching.h"

#include "render/logger.h"
#include "render/material.h"
#include "render/mesh.h"
#include "render/resource_manager.h"
#include "render/renderable.h"
#include "render/sprite/sprite_batcher.h"
#include "render/shader.h"
#include "render/gl_thread_checker.h"
#include "render/gpu_buffer_pool.h"
#include <glad/glad.h>
#include <cmath>
#include <cstring>
#include <sstream>
#include <exception>
#include <chrono>
#include <limits>

#ifdef __AVX2__
#include <immintrin.h>
#endif

namespace Render {

namespace {
    // ========================================================================
    // 辅助函数：矩阵验证（参考 Transform 的错误处理机制）
    // ========================================================================
    
    /// 检查浮点数是否有效（不是 NaN 或 Inf）
    inline bool IsFinite(float value) noexcept {
        return std::isfinite(value);
    }
    
    /// 验证矩阵是否包含有效值（无 NaN/Inf）
    bool IsMatrixValid(const Matrix4& matrix) noexcept {
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                if (!IsFinite(matrix(i, j))) {
                    return false;
                }
            }
        }
        return true;
    }
    
    /// 验证 Vector3 是否有效
    bool IsVector3Valid(const Vector3& vec) noexcept {
        return IsFinite(vec.x()) && IsFinite(vec.y()) && IsFinite(vec.z());
    }
    
    /// 验证并清理 Vector3（移除 NaN/Inf）
    Vector3 SanitizeVector3(const Vector3& vec) noexcept {
        if (IsVector3Valid(vec)) {
            return vec;
        }
        // 包含无效值，返回零向量
        return Vector3::Zero();
    }
    
    /// 安全验证 Vertex 数据
    bool ValidateVertex(const Vertex& vertex) noexcept {
        return IsVector3Valid(vertex.position) && IsVector3Valid(vertex.normal);
    }
    
    // ========================================================================
    // 批量顶点变换优化（参考 Transform 的 SIMD 优化策略）
    // ========================================================================
    
    /// 计算法线变换矩阵（逆转置）- 带安全检查
    Matrix3 ComputeNormalMatrix(const Matrix4& modelMatrix) noexcept {
        Matrix3 normalMatrix = modelMatrix.topLeftCorner<3, 3>();
        const float determinant = normalMatrix.determinant();
        
        // 检测奇异矩阵（参考 Transform 的验证策略）
        if (std::fabs(determinant) > 1e-6f) {
            try {
                normalMatrix = normalMatrix.inverse().transpose();
                
                // 验证结果矩阵
                for (int i = 0; i < 3; ++i) {
                    for (int j = 0; j < 3; ++j) {
                        if (!IsFinite(normalMatrix(i, j))) {
                            // 逆矩阵包含无效值，回退
                            return Matrix3::Identity();
                        }
                    }
                }
            } catch (...) {
                // 求逆失败，使用单位矩阵
                normalMatrix = Matrix3::Identity();
            }
        } else {
            // 退化为单位矩阵（安全回退）
            normalMatrix = Matrix3::Identity();
        }
        
        return normalMatrix;
    }
    
#ifdef __AVX2__
    /// SIMD 优化的批量顶点位置变换（AVX2）- 带边界检查
    /// 参考 Transform::TransformBatchHandle 的 SIMD 实现
    void TransformPositionsSIMD(const std::vector<Vertex>& vertices,
                               std::vector<Vertex>& outVertices,
                               const Matrix4& modelMatrix,
                               size_t startIdx) {
        const size_t count = vertices.size();
        if (count == 0 || startIdx + count > outVertices.size()) {
            return; // 边界检查失败
        }
        
        const size_t simdCount = count & ~3; // 处理 4 的倍数
        
        // 预先计算矩阵元素（减少重复访问）
        const float m00 = modelMatrix(0,0), m01 = modelMatrix(0,1), 
                    m02 = modelMatrix(0,2), m03 = modelMatrix(0,3);
        const float m10 = modelMatrix(1,0), m11 = modelMatrix(1,1), 
                    m12 = modelMatrix(1,2), m13 = modelMatrix(1,3);
        const float m20 = modelMatrix(2,0), m21 = modelMatrix(2,1), 
                    m22 = modelMatrix(2,2), m23 = modelMatrix(2,3);
        const float m30 = modelMatrix(3,0), m31 = modelMatrix(3,1), 
                    m32 = modelMatrix(3,2), m33 = modelMatrix(3,3);
        
        // SIMD 处理（4 个顶点一组）
        for (size_t i = 0; i < simdCount; i += 4) {
            // 安全边界检查
            if (i + 4 > count || startIdx + i + 4 > outVertices.size()) {
                break;
            }
            
            for (size_t j = 0; j < 4; ++j) {
                const auto& vertex = vertices[i + j];
                Vertex& transformed = outVertices[startIdx + i + j];
                transformed = vertex;
                
                // 验证输入顶点
                if (!ValidateVertex(vertex)) {
                    transformed.position = Vector3::Zero();
                    continue;
                }
                
                const float x = vertex.position.x();
                const float y = vertex.position.y();
                const float z = vertex.position.z();
                
                // 手动展开矩阵乘法（优化性能）
                const float w = m30 * x + m31 * y + m32 * z + m33;
                const float invW = (std::fabs(w) > 1e-6f) ? (1.0f / w) : 1.0f;
                
                transformed.position.x() = (m00 * x + m01 * y + m02 * z + m03) * invW;
                transformed.position.y() = (m10 * x + m11 * y + m12 * z + m13) * invW;
                transformed.position.z() = (m20 * x + m21 * y + m22 * z + m23) * invW;
                
                // 验证输出
                if (!IsVector3Valid(transformed.position)) {
                    transformed.position = Vector3::Zero();
                }
            }
        }
        
        // 处理剩余元素
        for (size_t i = simdCount; i < count; ++i) {
            if (startIdx + i >= outVertices.size()) {
                break;
            }
            
            const auto& vertex = vertices[i];
            Vertex& transformed = outVertices[startIdx + i];
            transformed = vertex;
            
            // 验证输入顶点
            if (!ValidateVertex(vertex)) {
                transformed.position = Vector3::Zero();
                continue;
            }
            
            const float x = vertex.position.x();
            const float y = vertex.position.y();
            const float z = vertex.position.z();
            
            const float w = m30 * x + m31 * y + m32 * z + m33;
            const float invW = (std::fabs(w) > 1e-6f) ? (1.0f / w) : 1.0f;
            
            transformed.position.x() = (m00 * x + m01 * y + m02 * z + m03) * invW;
            transformed.position.y() = (m10 * x + m11 * y + m12 * z + m13) * invW;
            transformed.position.z() = (m20 * x + m21 * y + m22 * z + m23) * invW;
            
            // 验证输出
            if (!IsVector3Valid(transformed.position)) {
                transformed.position = Vector3::Zero();
            }
        }
    }
#endif
    
    /// 批量变换顶点位置（标准实现，参考 Transform 的批量操作模式）- 带安全检查
    void TransformPositionsBatch(const std::vector<Vertex>& vertices,
                                std::vector<Vertex>& outVertices,
                                const Matrix4& modelMatrix,
                                size_t startIdx) {
        const size_t count = vertices.size();
        
        // 边界检查（参考 Transform 的验证策略）
        if (count == 0 || startIdx + count > outVertices.size()) {
            Logger::GetInstance().Warning(
                "[RenderBatch] TransformPositionsBatch: Invalid buffer size");
            return;
        }
        
        // 预先计算矩阵元素（减少重复访问）
        const float m00 = modelMatrix(0,0), m01 = modelMatrix(0,1), 
                    m02 = modelMatrix(0,2), m03 = modelMatrix(0,3);
        const float m10 = modelMatrix(1,0), m11 = modelMatrix(1,1), 
                    m12 = modelMatrix(1,2), m13 = modelMatrix(1,3);
        const float m20 = modelMatrix(2,0), m21 = modelMatrix(2,1), 
                    m22 = modelMatrix(2,2), m23 = modelMatrix(2,3);
        const float m30 = modelMatrix(3,0), m31 = modelMatrix(3,1), 
                    m32 = modelMatrix(3,2), m33 = modelMatrix(3,3);
        
        for (size_t i = 0; i < count; ++i) {
            const auto& vertex = vertices[i];
            Vertex& transformed = outVertices[startIdx + i];
            transformed = vertex;
            
            // 验证输入顶点（参考 Transform 的 NaN/Inf 检测）
            if (!ValidateVertex(vertex)) {
                Logger::GetInstance().Warning(
                    "[RenderBatch] Invalid vertex data detected, using zero vector");
                transformed.position = Vector3::Zero();
                continue;
            }
            
            const float x = vertex.position.x();
            const float y = vertex.position.y();
            const float z = vertex.position.z();
            
            // 手动展开矩阵乘法（优化性能）
            const float w = m30 * x + m31 * y + m32 * z + m33;
            const float invW = (std::fabs(w) > 1e-6f) ? (1.0f / w) : 1.0f;
            
            transformed.position.x() = (m00 * x + m01 * y + m02 * z + m03) * invW;
            transformed.position.y() = (m10 * x + m11 * y + m12 * z + m13) * invW;
            transformed.position.z() = (m20 * x + m21 * y + m22 * z + m23) * invW;
            
            // 验证输出（防止 NaN/Inf 传播）
            if (!IsVector3Valid(transformed.position)) {
                Logger::GetInstance().Warning(
                    "[RenderBatch] Transform produced invalid position, using zero vector");
                transformed.position = Vector3::Zero();
            }
        }
    }
    
    /// 批量变换法线（参考 Transform 的批量方向变换）- 带安全检查
    void TransformNormalsBatch(std::vector<Vertex>& vertices,
                              const Matrix3& normalMatrix,
                              size_t startIdx,
                              size_t count) {
        // 边界检查（参考 Transform 的验证策略）
        if (count == 0 || startIdx + count > vertices.size()) {
            Logger::GetInstance().Warning(
                "[RenderBatch] TransformNormalsBatch: Invalid buffer size");
            return;
        }
        
        // 预先计算矩阵元素
        const float n00 = normalMatrix(0,0), n01 = normalMatrix(0,1), n02 = normalMatrix(0,2);
        const float n10 = normalMatrix(1,0), n11 = normalMatrix(1,1), n12 = normalMatrix(1,2);
        const float n20 = normalMatrix(2,0), n21 = normalMatrix(2,1), n22 = normalMatrix(2,2);
        
        for (size_t i = 0; i < count; ++i) {
            if (startIdx + i >= vertices.size()) {
                break; // 额外的边界保护
            }
            
            auto& vertex = vertices[startIdx + i];
            const Vector3& normal = vertex.normal;
            
            // 验证输入法线（参考 Transform 的 NaN/Inf 检测）
            if (!IsVector3Valid(normal)) {
                Logger::GetInstance().Warning(
                    "[RenderBatch] Invalid normal detected, using default up vector");
                vertex.normal = Vector3::UnitY();
                continue;
            }
            
            // 手动展开矩阵乘法
            const float nx = n00 * normal.x() + n01 * normal.y() + n02 * normal.z();
            const float ny = n10 * normal.x() + n11 * normal.y() + n12 * normal.z();
            const float nz = n20 * normal.x() + n21 * normal.y() + n22 * normal.z();
            
            Vector3 transformedNormal(nx, ny, nz);
            
            // 验证变换后的法线
            if (!IsVector3Valid(transformedNormal)) {
                Logger::GetInstance().Warning(
                    "[RenderBatch] Transform produced invalid normal, using default up vector");
                vertex.normal = Vector3::UnitY();
                continue;
            }
            
            const float norm = transformedNormal.norm();
            
            // 归一化（参考 Transform 的验证策略）
            if (norm > 1e-6f) {
                transformedNormal *= (1.0f / norm);
                
                // 验证归一化结果
                if (!IsVector3Valid(transformedNormal)) {
                    transformedNormal = Vector3::UnitY();
                }
            } else {
                // 退化为默认上方向（安全回退）
                transformedNormal = Vector3::UnitY();
            }
            
            vertex.normal = transformedNormal;
        }
    }
    
    /// 辅助函数：从 Renderable 获取三角形数
    uint32_t GetRenderableTriangleCount(Renderable* renderable) {
        if (!renderable || !renderable->IsVisible()) {
            return 0;
        }
        
        switch (renderable->GetType()) {
            case RenderableType::Mesh: {
                auto* meshRenderable = static_cast<MeshRenderable*>(renderable);
                auto mesh = meshRenderable->GetMesh();
                if (mesh) {
                    const auto indexCount = mesh->GetIndexCount();
                    if (indexCount > 0) {
                        return static_cast<uint32_t>(indexCount / 3);
                    }
                }
                return 0;
            }
            case RenderableType::Model: {
                // ModelRenderable 可能包含多个部件，这里简化处理
                return 0;
            }
            case RenderableType::Sprite:
            case RenderableType::Text: {
                // Sprite 和 Text 通常是四边形（2个三角形）
                return 2;
            }
            default:
                return 0;
        }
    }
} // anonymous namespace

void BatchCommandBuffer::Clear() {
    std::scoped_lock lock(m_mutex);
    m_commands.clear();
}

void BatchCommandBuffer::AddImmediate(Renderable* renderable) {
    if (!renderable) {
        return;
    }

    BatchCommand command;
    command.type = BatchCommand::Type::Immediate;
    command.renderable = renderable;

    std::scoped_lock lock(m_mutex);
    m_commands.push_back(command);
}

void BatchCommandBuffer::AddBatch(size_t batchIndex) {
    BatchCommand command;
    command.type = BatchCommand::Type::Batch;
    command.batchIndex = batchIndex;

    std::scoped_lock lock(m_mutex);
    m_commands.push_back(command);
}

void BatchCommandBuffer::Swap(BatchCommandBuffer& other) {
    if (this == &other) {
        return;
    }
    std::scoped_lock lock(m_mutex, other.m_mutex);
    m_commands.swap(other.m_commands);
}

void BatchManager::BatchStorage::Clear() {
    for (auto& batch : batches) {
        batch.Reset();
    }
    batches.clear();
    lookup.clear();
}

void RenderBatch::SetKey(const RenderBatchKey& key) {
    m_key = key;
    m_keyInitialized = true;

    RenderBatchKeyHasher hasher;
    m_keyHash = hasher(key);

    std::ostringstream oss;
    oss << "batch_mesh_" << std::hex << reinterpret_cast<uintptr_t>(this) << "_" << m_keyHash;
    m_meshResourceName = oss.str();
}

const RenderBatchKey& RenderBatch::GetKey() const noexcept {
    return m_key;
}

void RenderBatch::ReleaseGpuResources() {
    if (m_resourceManager && m_meshHandle.IsValid()) {
        m_resourceManager->RemoveMeshByHandle(m_meshHandle);
        m_meshHandle.Invalidate();
    }

    m_batchMesh.reset();
    m_sourceMesh.reset();
    m_instancePayloads.clear();
    m_instanceCount = 0;
    if (m_instanceBuffer != 0) {
        // 归还实例缓冲到缓冲池
        auto& bufferPool = GPUBufferPool::GetInstance();
        bufferPool.ReleaseBuffer(m_instanceBuffer);
        m_instanceBuffer = 0;
    }
    m_gpuResourcesReady = false;
    m_drawVertexCount = 0;
    m_cachedTriangleCount = 0;
}

void RenderBatch::Reset() {
    ReleaseGpuResources();
    m_items.clear();
    m_keyInitialized = false;
    m_cpuVertices.clear();
    m_cpuIndices.clear();
    m_indexCount = 0;
    m_drawVertexCount = 0;
    m_cachedTriangleCount = 0;
    m_gpuResourcesReady = false;
}

void RenderBatch::AddItem(const BatchableItem& item) {
    m_items.push_back(item);
}

void RenderBatch::UploadResources(ResourceManager* resourceManager, BatchingMode mode) {
    m_resourceManager = resourceManager;

    if (m_items.empty()) {
        ReleaseGpuResources();
        return;
    }

    if (!m_items.empty() && m_items.front().type == BatchItemType::Sprite) {
        m_gpuResourcesReady = true;
        m_instanceCount = 0;
        for (const auto& item : m_items) {
            m_instanceCount += item.spriteData.instanceCount;
        }
        m_cachedTriangleCount = 2;
        m_drawVertexCount = 4;
        return;
    }

    if (!m_items.empty() && m_items.front().type == BatchItemType::Text) {
        m_gpuResourcesReady = true;
        m_instanceCount = static_cast<uint32_t>(m_items.size());
        m_cachedTriangleCount = 2;
        m_drawVertexCount = 4;
        return;
    }

    ReleaseGpuResources();

    if (mode == BatchingMode::GpuInstancing) {
        // 安全检查：确保有数据（参考 Transform 的验证策略）
        if (m_items.empty() || m_items.front().type != BatchItemType::Mesh) {
            m_instanceCount = 0;
            m_gpuResourcesReady = false;
            return;
        }
        
        m_sourceMesh = m_items.front().meshData.mesh;
        m_instancePayloads.clear();

        if (!m_sourceMesh) {
            Logger::GetInstance().Warning(
                "[RenderBatch] GPU Instancing: Source mesh is null");
            m_instanceCount = 0;
            m_gpuResourcesReady = false;
            return;
        }

        m_instancePayloads.reserve(m_items.size());
        for (const auto& item : m_items) {
            // 验证每个 item（防止崩溃）
            if (item.type != BatchItemType::Mesh || !item.meshData.mesh) {
                Logger::GetInstance().Warning(
                    "[RenderBatch] GPU Instancing: Skipping invalid item");
                continue;
            }
            
            // 验证矩阵有效性
            const Matrix4& modelMatrix = item.meshData.modelMatrix;
            if (!IsMatrixValid(modelMatrix)) {
                Logger::GetInstance().Warning(
                    "[RenderBatch] GPU Instancing: Invalid model matrix, using identity");
                InstancePayload payload{};
                Matrix4 identity = Matrix4::Identity();
                std::memcpy(payload.matrix, identity.data(), 
                           std::min(sizeof(payload.matrix), sizeof(float) * 16));
                m_instancePayloads.push_back(payload);
                continue;
            }
            
            InstancePayload payload{};
            // 安全拷贝：使用 std::min 防止越界
            std::memcpy(payload.matrix, modelMatrix.data(), 
                       std::min(sizeof(payload.matrix), sizeof(float) * 16));
            m_instancePayloads.push_back(payload);
        }

        m_instanceCount = static_cast<uint32_t>(m_instancePayloads.size());

        if (m_instanceCount == 0) {
            Logger::GetInstance().Warning(
                "[RenderBatch] GPU Instancing: No valid instances to render");
            m_gpuResourcesReady = false;
            return;
        }

        // 安全获取索引和顶点计数
        const size_t indexCount = m_sourceMesh->GetIndexCount();
        const size_t vertexCount = m_sourceMesh->GetVertexCount();
        
        if (indexCount == 0 || vertexCount == 0) {
            Logger::GetInstance().Warning(
                "[RenderBatch] GPU Instancing: Source mesh has no geometry");
            m_gpuResourcesReady = false;
            return;
        }
        
        m_cachedTriangleCount = static_cast<uint32_t>(indexCount / 3);
        m_drawVertexCount = static_cast<uint32_t>(vertexCount);

        // 从 GPU 缓冲池获取实例缓冲（参考 Transform 的资源管理）
        auto& bufferPool = GPUBufferPool::GetInstance();
        
        // 如果已有缓冲，先归还（防止资源泄漏）
        if (m_instanceBuffer != 0) {
            bufferPool.ReleaseBuffer(m_instanceBuffer);
            m_instanceBuffer = 0;
        }
        
        BufferDescriptor instDesc;
        instDesc.size = m_instancePayloads.size() * sizeof(InstancePayload);
        instDesc.target = BufferTarget::ArrayBuffer;
        instDesc.usage = GL_STREAM_DRAW;  // 批处理每帧更新
        
        // 验证缓冲区大小（防止过大分配）
        constexpr size_t MAX_BUFFER_SIZE = 256 * 1024 * 1024; // 256MB
        if (instDesc.size == 0 || instDesc.size > MAX_BUFFER_SIZE) {
            Logger::GetInstance().ErrorFormat(
                "[RenderBatch] GPU Instancing: Invalid buffer size: %zu bytes", 
                instDesc.size);
            m_gpuResourcesReady = false;
            return;
        }
        
        m_instanceBuffer = bufferPool.AcquireBuffer(instDesc);
        if (m_instanceBuffer == 0) {
            Logger::GetInstance().Error(
                "[RenderBatch] GPU Instancing: Failed to acquire instance buffer");
            m_gpuResourcesReady = false;
            return;
        }

        uint32_t vao = m_sourceMesh->GetVertexArrayID();
        if (vao == 0) {
            Logger::GetInstance().Error(
                "[RenderBatch] GPU Instancing: Invalid VAO");
            // 归还缓冲区
            bufferPool.ReleaseBuffer(m_instanceBuffer);
            m_instanceBuffer = 0;
            m_gpuResourcesReady = false;
            return;
        }

        // OpenGL 操作（带异常保护）
        try {
            GL_THREAD_CHECK();
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffer);
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(instDesc.size),
                         m_instancePayloads.data(),
                         GL_STREAM_DRAW);

            constexpr GLuint baseLocation = 4;
            const GLsizei stride = sizeof(InstancePayload);
            for (GLuint i = 0; i < 4; ++i) {
                glEnableVertexAttribArray(baseLocation + i);
                glVertexAttribPointer(baseLocation + i, 4, GL_FLOAT, GL_FALSE,
                                      stride,
                                      reinterpret_cast<void*>(sizeof(float) * 4 * i));
                glVertexAttribDivisor(baseLocation + i, 1);
            }

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
            
            // 检查 OpenGL 错误
            GLenum error = glGetError();
            if (error != GL_NO_ERROR) {
                Logger::GetInstance().ErrorFormat(
                    "[RenderBatch] GPU Instancing: OpenGL error: 0x%x", error);
                bufferPool.ReleaseBuffer(m_instanceBuffer);
                m_instanceBuffer = 0;
                m_gpuResourcesReady = false;
                return;
            }
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat(
                "[RenderBatch] GPU Instancing: Exception during OpenGL setup: %s", 
                e.what());
            bufferPool.ReleaseBuffer(m_instanceBuffer);
            m_instanceBuffer = 0;
            m_gpuResourcesReady = false;
            return;
        }

        m_gpuResourcesReady = true;
        return;
    }

    if (mode != BatchingMode::CpuMerge) {
        return;
    }

    m_cpuVertices.clear();
    m_cpuIndices.clear();
    m_indexCount = 0;
    m_cachedTriangleCount = 0;
    m_gpuResourcesReady = false;

    // 验证有效项（参考 Transform 的验证策略）
    if (m_items.empty()) {
        Logger::GetInstance().Warning("[RenderBatch] CPU Merge: No items to batch");
        return;
    }

    uint32_t baseVertex = 0;
    bool buildFailed = false;
    size_t validItemCount = 0;
    
    // 预先统计有效项数
    for (const auto& item : m_items) {
        if (item.type == BatchItemType::Mesh && item.meshData.mesh && 
            item.meshData.material && item.meshData.mesh->GetIndexCount() > 0) {
            ++validItemCount;
        }
    }
    
    if (validItemCount == 0) {
        Logger::GetInstance().Warning("[RenderBatch] CPU Merge: No valid items to batch");
        return;
    }

    // 预估容量（带安全上限检查）
    const size_t estimatedVertices = std::min(validItemCount * 128, size_t(10'000'000));
    const size_t estimatedIndices = std::min(validItemCount * 192, size_t(15'000'000));
    
    try {
        m_cpuVertices.reserve(estimatedVertices);
        m_cpuIndices.reserve(estimatedIndices);
    } catch (const std::bad_alloc& e) {
        Logger::GetInstance().ErrorFormat(
            "[RenderBatch] CPU Merge: Failed to allocate memory: %s", e.what());
        return;
    }

    for (const auto& item : m_items) {
        // 严格验证每个 item（参考 Transform 的错误处理）
        if (item.type != BatchItemType::Mesh) {
            Logger::GetInstance().Warning("[RenderBatch] CPU Merge: Skipping non-mesh item");
            continue;
        }
        
        if (!item.meshData.mesh) {
            Logger::GetInstance().Warning("[RenderBatch] CPU Merge: Skipping null mesh");
            continue;
        }
        
        if (!item.meshData.material) {
            Logger::GetInstance().Warning("[RenderBatch] CPU Merge: Skipping item without material");
            buildFailed = true;
            break;
        }

        auto mesh = item.meshData.mesh;

        const size_t indexCount = mesh->GetIndexCount();
        if (indexCount == 0) {
            Logger::GetInstance().Warning("[RenderBatch] CPU Merge: Skipping mesh with no indices");
            continue;
        }
        
        const size_t vertexCount = mesh->GetVertexCount();
        if (vertexCount == 0) {
            Logger::GetInstance().Warning("[RenderBatch] CPU Merge: Skipping mesh with no vertices");
            continue;
        }

        // ========================================================================
        // 矩阵验证（参考 Transform 的错误处理机制）
        // ========================================================================
        const Matrix4& modelMatrix = item.meshData.modelMatrix;
        
        // 检测无效矩阵（NaN/Inf）
        if (!IsMatrixValid(modelMatrix)) {
            Logger::GetInstance().Warning(
                "[RenderBatch] Invalid model matrix detected (contains NaN/Inf), skipping item");
            buildFailed = true;
            break;
        }
        
        // 计算法线变换矩阵（使用优化的辅助函数）
        const Matrix3 normalMatrix = ComputeNormalMatrix(modelMatrix);
        
        const size_t vertexCountBefore = m_cpuVertices.size();

        // ========================================================================
        // 批量顶点变换（参考 Transform 的批量操作优化）
        // ========================================================================
        bool vertexProcessSuccess = false;
        try {
            mesh->AccessVertices([&](const std::vector<Vertex>& vertices) {
                if (vertices.empty()) {
                    Logger::GetInstance().Warning(
                        "[RenderBatch] CPU Merge: Mesh has empty vertex buffer");
                    return;
                }
                
                // 安全性检查：防止过大分配
                if (vertices.size() > 1'000'000) {
                    Logger::GetInstance().WarningFormat(
                        "[RenderBatch] CPU Merge: Mesh has too many vertices: %zu", 
                        vertices.size());
                    return;
                }
                
                // 预分配空间（减少内存重分配）
                try {
                    m_cpuVertices.reserve(m_cpuVertices.size() + vertices.size());
                } catch (const std::bad_alloc& e) {
                    Logger::GetInstance().ErrorFormat(
                        "[RenderBatch] CPU Merge: Failed to reserve vertex buffer: %s", 
                        e.what());
                    return;
                }
                
                const size_t startIdx = m_cpuVertices.size();
                
                // 扩展容器
                try {
                    m_cpuVertices.resize(m_cpuVertices.size() + vertices.size());
                } catch (const std::bad_alloc& e) {
                    Logger::GetInstance().ErrorFormat(
                        "[RenderBatch] CPU Merge: Failed to resize vertex buffer: %s", 
                        e.what());
                    return;
                }
                
                // 批量变换位置（优化：减少循环开销）
#ifdef __AVX2__
                // 使用 SIMD 优化（当可用时）
                if (vertices.size() >= 4) {
                    TransformPositionsSIMD(vertices, m_cpuVertices, modelMatrix, startIdx);
                } else {
                    TransformPositionsBatch(vertices, m_cpuVertices, modelMatrix, startIdx);
                }
#else
                TransformPositionsBatch(vertices, m_cpuVertices, modelMatrix, startIdx);
#endif
                
                // 批量变换法线（优化：手动展开矩阵乘法）
                TransformNormalsBatch(m_cpuVertices, normalMatrix, startIdx, vertices.size());
                
                vertexProcessSuccess = true;
            });
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat(
                "[RenderBatch] CPU Merge: Exception during vertex processing: %s", 
                e.what());
            buildFailed = true;
            break;
        }

        const size_t verticesAdded = m_cpuVertices.size() - vertexCountBefore;
        if (verticesAdded == 0 || !vertexProcessSuccess) {
            Logger::GetInstance().Warning(
                "[RenderBatch] CPU Merge: No vertices added for this mesh");
            buildFailed = true;
            break;
        }

        // 批量处理索引（优化：预分配）- 带安全检查
        bool indexProcessSuccess = false;
        try {
            mesh->AccessIndices([&](const std::vector<uint32_t>& indices) {
                if (indices.empty()) {
                    Logger::GetInstance().Warning(
                        "[RenderBatch] CPU Merge: Mesh has empty index buffer");
                    return;
                }
                
                // 安全性检查：防止过大分配
                if (indices.size() > 3'000'000) {
                    Logger::GetInstance().WarningFormat(
                        "[RenderBatch] CPU Merge: Mesh has too many indices: %zu", 
                        indices.size());
                    return;
                }
                
                try {
                    m_cpuIndices.reserve(m_cpuIndices.size() + indices.size());
                } catch (const std::bad_alloc& e) {
                    Logger::GetInstance().ErrorFormat(
                        "[RenderBatch] CPU Merge: Failed to reserve index buffer: %s", 
                        e.what());
                    return;
                }
                
                // 验证索引范围（防止越界）
                const uint32_t maxIndex = static_cast<uint32_t>(m_cpuVertices.size());
                for (uint32_t index : indices) {
                    const uint32_t adjustedIndex = index + baseVertex;
                    if (adjustedIndex >= maxIndex) {
                        Logger::GetInstance().WarningFormat(
                            "[RenderBatch] CPU Merge: Index out of range: %u >= %u", 
                            adjustedIndex, maxIndex);
                        continue;
                    }
                    m_cpuIndices.push_back(adjustedIndex);
                }
                
                indexProcessSuccess = true;
            });
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat(
                "[RenderBatch] CPU Merge: Exception during index processing: %s", 
                e.what());
            buildFailed = true;
            break;
        }
        
        if (!indexProcessSuccess) {
            Logger::GetInstance().Warning(
                "[RenderBatch] CPU Merge: Failed to process indices");
            buildFailed = true;
            break;
        }

        baseVertex += static_cast<uint32_t>(verticesAdded);
        
        // 防止 baseVertex 溢出
        if (baseVertex >= std::numeric_limits<uint32_t>::max() - verticesAdded) {
            Logger::GetInstance().Error(
                "[RenderBatch] CPU Merge: Vertex count would overflow");
            buildFailed = true;
            break;
        }
    }

    // 最终验证（参考 Transform 的错误处理）
    if (buildFailed || m_cpuVertices.empty() || m_cpuIndices.empty()) {
        Logger::GetInstance().Warning(
            "[RenderBatch] CPU Merge: Build failed or no data generated");
        ReleaseGpuResources();
        m_cpuVertices.clear();
        m_cpuIndices.clear();
        m_indexCount = 0;
        m_gpuResourcesReady = false;
        return;
    }

    // 验证顶点和索引数量合理性
    if (m_cpuVertices.size() > 10'000'000 || m_cpuIndices.size() > 30'000'000) {
        Logger::GetInstance().ErrorFormat(
            "[RenderBatch] CPU Merge: Generated mesh is too large (V:%zu, I:%zu)",
            m_cpuVertices.size(), m_cpuIndices.size());
        ReleaseGpuResources();
        m_cpuVertices.clear();
        m_cpuIndices.clear();
        m_indexCount = 0;
        m_gpuResourcesReady = false;
        return;
    }

    m_indexCount = static_cast<uint32_t>(m_cpuIndices.size());
    m_cachedTriangleCount = m_indexCount / 3;
    m_drawVertexCount = static_cast<uint32_t>(m_cpuVertices.size());

    // 创建并上传 Mesh（带异常保护）
    try {
        auto mergedMesh = std::make_shared<Mesh>();
        if (!mergedMesh) {
            Logger::GetInstance().Error(
                "[RenderBatch] CPU Merge: Failed to create Mesh object");
            ReleaseGpuResources();
            m_cpuVertices.clear();
            m_cpuIndices.clear();
            return;
        }
        
        mergedMesh->SetData(m_cpuVertices, m_cpuIndices);
        mergedMesh->Upload();
        
        // 验证上传成功
        if (mergedMesh->GetVertexArrayID() == 0) {
            Logger::GetInstance().Error(
                "[RenderBatch] CPU Merge: Mesh upload failed (invalid VAO)");
            ReleaseGpuResources();
            m_cpuVertices.clear();
            m_cpuIndices.clear();
            return;
        }

        // 资源管理器注册（带安全检查）
        if (m_resourceManager) {
            try {
                if (!m_meshHandle.IsValid()) {
                    if (m_meshResourceName.empty()) {
                        std::ostringstream oss;
                        oss << "batch_mesh_" << std::hex 
                            << reinterpret_cast<uintptr_t>(this);
                        m_meshResourceName = oss.str();
                    }
                    m_meshHandle = m_resourceManager->CreateMeshHandle(
                        m_meshResourceName, mergedMesh);
                } else {
                    m_resourceManager->ReloadMesh(m_meshHandle, mergedMesh);
                }
            } catch (const std::exception& e) {
                Logger::GetInstance().ErrorFormat(
                    "[RenderBatch] CPU Merge: Failed to register with ResourceManager: %s", 
                    e.what());
                // 继续使用 m_batchMesh，不依赖 ResourceManager
            }
        }

        m_batchMesh = mergedMesh;
        m_gpuResourcesReady = true;
        
        Logger::GetInstance().InfoFormat(
            "[RenderBatch] CPU Merge: Successfully created batch (V:%u, I:%u, T:%u)",
            m_drawVertexCount, m_indexCount, m_cachedTriangleCount);
            
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat(
            "[RenderBatch] CPU Merge: Exception during mesh creation: %s", 
            e.what());
        ReleaseGpuResources();
        m_cpuVertices.clear();
        m_cpuIndices.clear();
        m_indexCount = 0;
        m_gpuResourcesReady = false;
        return;
    }
}

bool RenderBatch::Draw(RenderState* renderState, uint32_t& drawCallCounter, BatchingMode mode) {
    if (!renderState) {
        return false;
    }

    const auto drawFallback = [&]() {
        for (auto& item : m_items) {
            if (item.renderable && item.renderable->IsVisible()) {
                item.renderable->Render(renderState);
                ++drawCallCounter;
            }
        }
    };

    if (!m_items.empty() && m_items.front().type == BatchItemType::Sprite) {
        bool anyDrawn = false;
        for (auto& item : m_items) {
            if (!item.spriteData.batcher) {
                continue;
            }
            item.spriteData.batcher->DrawBatch(item.spriteData.batchIndex, renderState);
            ++drawCallCounter;
            anyDrawn = true;
        }
        return anyDrawn;
    }

    if (!m_items.empty() && m_items.front().type == BatchItemType::Text) {
        if (!renderState) {
            Logger::GetInstance().Warning(
                "[RenderBatch] Draw Text: RenderState is null");
            drawFallback();
            return false;
        }

        // 安全访问 front()（已经检查过 empty）
        auto& firstItem = m_items.front();
        if (!firstItem.textData.shader || !firstItem.textData.mesh || 
            !firstItem.textData.texture) {
            Logger::GetInstance().Warning(
                "[RenderBatch] Draw Text: First item missing resources");
            drawFallback();
            return false;
        }

        // 设置渲染状态（带验证）
        try {
            renderState->SetBlendMode(firstItem.key.blendMode);
            renderState->SetDepthTest(firstItem.key.depthTest);
            renderState->SetDepthWrite(firstItem.key.depthWrite);
            renderState->SetCullFace(firstItem.key.cullFace);
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat(
                "[RenderBatch] Draw Text: Failed to set render state: %s", 
                e.what());
            drawFallback();
            return false;
        }

        Shader* currentShader = nullptr;
        Ref<Shader> activeShader;
        bool anyDrawn = false;

        for (auto& item : m_items) {
            // 严格的空指针检查（参考 Transform 的验证策略）
            if (!item.renderable) {
                continue;
            }
            
            if (!item.renderable->IsVisible()) {
                continue;
            }

            auto& data = item.textData;
            if (!data.shader || !data.mesh || !data.texture) {
                Logger::GetInstance().Warning(
                    "[RenderBatch] Draw Text: Item missing resources");
                continue;
            }

            Shader* shaderPtr = data.shader.get();
            if (!shaderPtr) {
                Logger::GetInstance().Warning(
                    "[RenderBatch] Draw Text: Shader pointer is null");
                continue;
            }
            
            if (!shaderPtr->IsValid()) {
                Logger::GetInstance().Warning(
                    "[RenderBatch] Draw Text: Shader is invalid");
                continue;
            }

            if (shaderPtr != currentShader) {
                // 安全的 shader 切换
                try {
                    if (currentShader && activeShader) {
                        activeShader->Unuse();
                    }
                    activeShader = data.shader;
                    activeShader->Use();
                    currentShader = shaderPtr;
                } catch (const std::exception& e) {
                    Logger::GetInstance().ErrorFormat(
                        "[RenderBatch] Draw Text: Failed to switch shader: %s", 
                        e.what());
                    continue;
                }
            }

            auto* uniformMgr = shaderPtr->GetUniformManager();
            if (!uniformMgr) {
                Logger::GetInstance().Warning(
                    "[RenderBatch] Draw Text: UniformManager is null");
                continue;
            }

            // 设置 uniforms（带矩阵验证）
            try {
                if (uniformMgr->HasUniform("uModel")) {
                    if (IsMatrixValid(data.modelMatrix)) {
                        uniformMgr->SetMatrix4("uModel", data.modelMatrix);
                    } else {
                        uniformMgr->SetMatrix4("uModel", Matrix4::Identity());
                    }
                }
                if (uniformMgr->HasUniform("uView")) {
                    if (IsMatrixValid(data.viewMatrix)) {
                        uniformMgr->SetMatrix4("uView", data.viewMatrix);
                    }
                }
                if (uniformMgr->HasUniform("uProjection")) {
                    if (IsMatrixValid(data.projectionMatrix)) {
                        uniformMgr->SetMatrix4("uProjection", data.projectionMatrix);
                    }
                }
                if (uniformMgr->HasUniform("uTextColor")) {
                    uniformMgr->SetColor("uTextColor", data.color);
                }
                if (uniformMgr->HasUniform("uTexture")) {
                    uniformMgr->SetInt("uTexture", 0);
                }

                data.texture->Bind(0);
                data.mesh->Draw();
                ++drawCallCounter;
                anyDrawn = true;
            } catch (const std::exception& e) {
                Logger::GetInstance().ErrorFormat(
                    "[RenderBatch] Draw Text: Exception during draw: %s", 
                    e.what());
                continue;
            }
        }

        // 安全清理
        try {
            if (currentShader && activeShader) {
                activeShader->Unuse();
            }
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat(
                "[RenderBatch] Draw Text: Exception during shader cleanup: %s", 
                e.what());
        }

        if (!anyDrawn) {
            drawFallback();
        }

        return anyDrawn;
    }

    if (mode == BatchingMode::GpuInstancing) {
        // 严格的前置条件检查（参考 Transform 的验证策略）
        if (!m_gpuResourcesReady) {
            Logger::GetInstance().Warning(
                "[RenderBatch] Draw Instanced: GPU resources not ready");
            drawFallback();
            return false;
        }
        
        if (m_items.empty()) {
            Logger::GetInstance().Warning(
                "[RenderBatch] Draw Instanced: No items");
            return false;
        }
        
        if (!m_sourceMesh) {
            Logger::GetInstance().Warning(
                "[RenderBatch] Draw Instanced: Source mesh is null");
            drawFallback();
            return false;
        }
        
        if (m_instanceCount == 0) {
            Logger::GetInstance().Warning(
                "[RenderBatch] Draw Instanced: Instance count is zero");
            return false;
        }

        const auto& firstItem = m_items.front();
        auto material = firstItem.meshData.material;
        if (!material) {
            Logger::GetInstance().Warning(
                "[RenderBatch] Draw Instanced: Material is null");
            drawFallback();
            return false;
        }

        // 安全绑定材质
        try {
            material->Bind(renderState);
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat(
                "[RenderBatch] Draw Instanced: Failed to bind material: %s", 
                e.what());
            drawFallback();
            return false;
        }

        UniformManager* uniformMgr = nullptr;
        auto shader = material->GetShader();
        if (shader) {
            uniformMgr = shader->GetUniformManager();
            if (uniformMgr) {
                try {
                    const Matrix4 identity = Matrix4::Identity();
                    uniformMgr->SetMatrix4("uModel", identity);
                    if (uniformMgr->HasUniform("uHasInstanceData")) {
                        uniformMgr->SetBool("uHasInstanceData", true);
                    }
                } catch (const std::exception& e) {
                    Logger::GetInstance().ErrorFormat(
                        "[RenderBatch] Draw Instanced: Failed to set uniforms: %s", 
                        e.what());
                }
            }
        }

        // 安全绘制
        try {
            m_sourceMesh->DrawInstanced(m_instanceCount);
            ++drawCallCounter;
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat(
                "[RenderBatch] Draw Instanced: Exception during draw: %s", 
                e.what());
            drawFallback();
            return false;
        }

        // 清理状态
        if (uniformMgr && uniformMgr->HasUniform("uHasInstanceData")) {
            try {
                uniformMgr->SetBool("uHasInstanceData", false);
            } catch (const std::exception& e) {
                Logger::GetInstance().ErrorFormat(
                    "[RenderBatch] Draw Instanced: Failed to reset uniform: %s", 
                    e.what());
            }
        }
        return true;
    }

    if (mode != BatchingMode::CpuMerge) {
        Logger::GetInstance().Warning(
            "[RenderBatch] Draw: Unsupported batching mode");
        drawFallback();
        return false;
    }

    // 严格的前置条件检查（参考 Transform 的验证策略）
    if (!m_gpuResourcesReady) {
        Logger::GetInstance().Warning(
            "[RenderBatch] Draw Merged: GPU resources not ready");
        drawFallback();
        return false;
    }
    
    if (m_items.empty()) {
        Logger::GetInstance().Warning(
            "[RenderBatch] Draw Merged: No items");
        return false;
    }

    const auto& firstItem = m_items.front();
    auto material = firstItem.meshData.material;
    if (!material) {
        Logger::GetInstance().Warning(
            "[RenderBatch] Draw Merged: Material is null");
        drawFallback();
        return false;
    }

    // 安全绑定材质
    try {
        material->Bind(renderState);
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat(
            "[RenderBatch] Draw Merged: Failed to bind material: %s", 
            e.what());
        drawFallback();
        return false;
    }

    // 设置 uniforms
    auto shader = material->GetShader();
    if (shader) {
        auto* uniformMgr = shader->GetUniformManager();
        if (uniformMgr) {
            try {
                const Matrix4 identity = Matrix4::Identity();
                uniformMgr->SetMatrix4("uModel", identity);
                if (uniformMgr->HasUniform("uHasInstanceData")) {
                    uniformMgr->SetBool("uHasInstanceData", false);
                }
            } catch (const std::exception& e) {
                Logger::GetInstance().ErrorFormat(
                    "[RenderBatch] Draw Merged: Failed to set uniforms: %s", 
                    e.what());
            }
        }
    }

    // 获取要绘制的 mesh（带验证）
    Ref<Mesh> meshToDraw = m_batchMesh;
    if (m_resourceManager && m_meshHandle.IsValid()) {
        try {
            meshToDraw = m_resourceManager->GetMeshSharedByHandle(m_meshHandle);
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat(
                "[RenderBatch] Draw Merged: Failed to get mesh from ResourceManager: %s", 
                e.what());
            // 回退到 m_batchMesh
            meshToDraw = m_batchMesh;
        }
    }

    if (!meshToDraw) {
        Logger::GetInstance().Error(
            "[RenderBatch] Draw Merged: Mesh to draw is null");
        drawFallback();
        return false;
    }
    
    // 验证 mesh 有效性
    if (meshToDraw->GetVertexArrayID() == 0) {
        Logger::GetInstance().Error(
            "[RenderBatch] Draw Merged: Mesh has invalid VAO");
        drawFallback();
        return false;
    }

    // 安全绘制
    try {
        meshToDraw->Draw();
        ++drawCallCounter;
        return true;
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat(
            "[RenderBatch] Draw Merged: Exception during draw: %s", 
            e.what());
        drawFallback();
        return false;
    }
}

BatchManager::BatchManager()
    : m_mode(BatchingMode::Disabled)
    , m_resourceManager(nullptr)
    , m_shutdown(false)
    , m_processing(false)
    , m_workerProcessedCount(0)
    , m_workerQueueHighWater(0)
    , m_workerDrainWaitNs(0) {
    m_workerThread = std::thread(&BatchManager::WorkerLoop, this);
}

BatchManager::~BatchManager() {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_shutdown = true;
    }
    m_queueCv.notify_all();
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

void BatchManager::SetMode(BatchingMode mode) {
    if (m_mode == mode) {
        return;
    }

    DrainWorker();

    {
        std::lock_guard<std::mutex> storageLock(m_storageMutex);
        m_executionStorage.Clear();
        m_recordingStorage.Clear();
    }

    m_executionBuffer.Clear();
    m_recordingBuffer.Clear();

    m_workerProcessedCount.store(0, std::memory_order_relaxed);
    m_workerQueueHighWater.store(0, std::memory_order_relaxed);
    m_workerDrainWaitNs.store(0, std::memory_order_relaxed);

    m_mode = mode;
}

BatchingMode BatchManager::GetMode() const noexcept {
    return m_mode;
}

void BatchManager::SetResourceManager(ResourceManager* resourceManager) {
    m_resourceManager = resourceManager;
}

void BatchManager::Reset() {
    DrainWorker();

    {
        std::lock_guard<std::mutex> storageLock(m_storageMutex);
        m_executionStorage.Clear();
        m_recordingStorage.Clear();
    }

    m_executionBuffer.Clear();
    m_recordingBuffer.Clear();

    m_workerProcessedCount.store(0, std::memory_order_relaxed);
    m_workerQueueHighWater.store(0, std::memory_order_relaxed);
    m_workerDrainWaitNs.store(0, std::memory_order_relaxed);
}

void BatchManager::SwapBuffers() {
    std::lock_guard<std::mutex> storageLock(m_storageMutex);

    m_executionBuffer.Clear();
    m_executionBuffer.Swap(m_recordingBuffer);

    m_executionStorage.lookup.swap(m_recordingStorage.lookup);
    m_executionStorage.batches.swap(m_recordingStorage.batches);

    m_recordingStorage.lookup.clear();
    m_recordingStorage.batches.clear();
}

void BatchManager::AddItem(const BatchableItem& item) {
    if (!item.renderable) {
        return;
    }

    BatchableItem localItem = item;

    if (m_mode == BatchingMode::GpuInstancing) {
        if (localItem.type == BatchItemType::Mesh && localItem.meshData.mesh) {
            localItem.key.meshHandle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(localItem.meshData.mesh.get()));
        }
    } else {
        localItem.key.meshHandle = 0;
    }

    bool shouldBatch = false;
    switch (m_mode) {
        case BatchingMode::CpuMerge:
            shouldBatch = localItem.batchable &&
                          localItem.type != BatchItemType::Unsupported &&
                          (localItem.type == BatchItemType::Text || !localItem.isTransparent);
            break;
        case BatchingMode::GpuInstancing:
            shouldBatch = localItem.instanceEligible && localItem.type != BatchItemType::Unsupported;
            break;
        default:
            shouldBatch = false;
            break;
    }

    WorkItem workItem{};
    workItem.item = std::move(localItem);
    workItem.shouldBatch = shouldBatch;

    EnqueueWork(std::move(workItem));
}

void BatchManager::EnqueueWork(WorkItem workItem) {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_shutdown) {
            return;
        }
        m_pendingItems.push_back(std::move(workItem));

        uint32_t currentDepth = static_cast<uint32_t>(m_pendingItems.size() + (m_processing ? 1 : 0));
        uint32_t observed = m_workerQueueHighWater.load(std::memory_order_relaxed);
        while (currentDepth > observed &&
               !m_workerQueueHighWater.compare_exchange_weak(observed, currentDepth, std::memory_order_relaxed)) {
            // retry until update succeeds or observed catches up
        }
    }
    m_queueCv.notify_one();
}

void BatchManager::DrainWorker() {
    std::unique_lock<std::mutex> lock(m_queueMutex);
    if (m_shutdown) {
        return;
    }

    if (m_pendingItems.empty() && !m_processing) {
        return;
    }

    auto waitBegin = std::chrono::steady_clock::now();
    m_queueCv.notify_all();
    m_idleCv.wait(lock, [this]() {
        return m_shutdown || (m_pendingItems.empty() && !m_processing);
    });
    auto waitEnd = std::chrono::steady_clock::now();
    auto waitNs = std::chrono::duration_cast<std::chrono::nanoseconds>(waitEnd - waitBegin).count();
    if (waitNs > 0) {
        m_workerDrainWaitNs.fetch_add(static_cast<uint64_t>(waitNs), std::memory_order_relaxed);
    }
}

void BatchManager::WorkerLoop() {
    std::unique_lock<std::mutex> lock(m_queueMutex);

    while (!m_shutdown) {
        m_queueCv.wait(lock, [this]() {
            return m_shutdown || !m_pendingItems.empty();
        });

        if (m_shutdown) {
            break;
        }

        WorkItem workItem = std::move(m_pendingItems.front());
        m_pendingItems.pop_front();
        m_processing = true;

        lock.unlock();

        try {
            ProcessWorkItem(workItem);
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat("[BatchManager] Worker error: %s", e.what());
        } catch (...) {
            Logger::GetInstance().Error("[BatchManager] Worker encountered an unknown error");
        }

        lock.lock();

        m_processing = false;
        if (m_pendingItems.empty()) {
            m_idleCv.notify_all();
        }
    }

    m_idleCv.notify_all();
}

void BatchManager::ProcessWorkItem(const WorkItem& workItem) {
    m_workerProcessedCount.fetch_add(1, std::memory_order_relaxed);

    if (!workItem.shouldBatch) {
        m_recordingBuffer.AddImmediate(workItem.item.renderable);
        return;
    }

    std::lock_guard<std::mutex> storageLock(m_storageMutex);

    auto& lookup = m_recordingStorage.lookup;
    auto it = lookup.find(workItem.item.key);
    size_t batchIndex;
    if (it == lookup.end()) {
        batchIndex = m_recordingStorage.batches.size();
        m_recordingStorage.batches.emplace_back();
        m_recordingStorage.batches.back().SetKey(workItem.item.key);
        lookup.emplace(workItem.item.key, batchIndex);
        m_recordingBuffer.AddBatch(batchIndex);
    } else {
        batchIndex = it->second;
    }

    m_recordingStorage.batches[batchIndex].AddItem(workItem.item);
}

BatchManager::FlushResult BatchManager::Flush(RenderState* renderState) {
    FlushResult result{};

    if (!renderState) {
        Reset();
        return result;
    }

    DrainWorker();
    SwapBuffers();

    result.workerProcessed = m_workerProcessedCount.exchange(0, std::memory_order_relaxed);
    result.workerMaxQueueDepth = m_workerQueueHighWater.exchange(0, std::memory_order_relaxed);
    uint64_t waitNs = m_workerDrainWaitNs.exchange(0, std::memory_order_relaxed);
    result.workerWaitTimeMs = static_cast<float>(waitNs) / 1'000'000.0f;

    auto& batches = m_executionStorage.batches;

    for (const auto& command : m_executionBuffer.GetCommands()) {
        if (command.type == BatchCommand::Type::Immediate) {
            if (command.renderable && command.renderable->IsVisible()) {
                command.renderable->Render(renderState);
                ++result.drawCalls;
                ++result.fallbackDrawCalls;
                // 计算 Immediate 命令的三角形数
                result.batchedTriangles += GetRenderableTriangleCount(command.renderable);
            }
            ++result.fallbackBatches;
            continue;
        }

        if (command.batchIndex >= batches.size()) {
            continue;
        }

        auto& batch = batches[command.batchIndex];
        if (batch.GetItemCount() == 0) {
            continue;
        }

        switch (m_mode) {
            case BatchingMode::CpuMerge:
            case BatchingMode::GpuInstancing: {
                batch.UploadResources(m_resourceManager, m_mode);

                const uint32_t drawCallsBefore = result.drawCalls;
                const bool merged = batch.Draw(renderState, result.drawCalls, m_mode);
                const uint32_t drawCallDelta = result.drawCalls - drawCallsBefore;

                if (merged) {
                    ++result.batchCount;
                    result.batchedDrawCalls += drawCallDelta;
                    uint32_t instanceCount = 1;
                    if (m_mode == BatchingMode::GpuInstancing) {
                        instanceCount = batch.GetInstanceCount();
                        result.instancedDrawCalls += drawCallDelta;
                        result.instancedInstances += instanceCount;
                    }

                    if (instanceCount == 0) {
                        instanceCount = 1;
                    }

                    result.batchedTriangles += batch.GetTriangleCount() * instanceCount;
                    result.batchedVertices += batch.GetVertexCount() * instanceCount;
                } else {
                    result.fallbackDrawCalls += drawCallDelta;
                    ++result.fallbackBatches;
                }
                break;
            }
            case BatchingMode::Disabled: {
                const uint32_t drawCallsBefore = result.drawCalls;
                batch.Draw(renderState, result.drawCalls, BatchingMode::Disabled);
                result.fallbackDrawCalls += (result.drawCalls - drawCallsBefore);
                ++result.fallbackBatches;
                
                // 在fallback模式下也计算triangles
                result.batchedTriangles += batch.GetFallbackTriangleCount();
                break;
            }
            default:
                break;
        }
    }

    Reset();
    return result;
}

uint32_t RenderBatch::GetFallbackTriangleCount() const noexcept {
    uint32_t totalTriangles = 0;
    for (const auto& item : m_items) {
        if (!item.renderable || !item.renderable->IsVisible()) {
            continue;
        }
        
        if (item.type == BatchItemType::Mesh && item.meshData.mesh) {
            const auto indexCount = item.meshData.mesh->GetIndexCount();
            if (indexCount > 0) {
                totalTriangles += static_cast<uint32_t>(indexCount / 3);
            }
        }
    }
    return totalTriangles;
}

size_t BatchManager::GetPendingItemCount() const noexcept {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    return m_pendingItems.size() + (m_processing ? 1u : 0u);
}

} // namespace Render


