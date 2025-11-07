#include "render/render_batching.h"

#include "render/logger.h"
#include "render/material.h"
#include "render/mesh.h"
#include "render/resource_manager.h"
#include "render/renderable.h"
#include "render/shader.h"
#include <cmath>
#include <sstream>

namespace Render {

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
    if (mode != BatchingMode::CpuMerge) {
        return;
    }

    if (m_items.empty()) {
        return;
    }

    m_resourceManager = resourceManager;
    m_cpuVertices.clear();
    m_cpuIndices.clear();
    m_indexCount = 0;
    m_cachedTriangleCount = 0;
    m_gpuResourcesReady = false;

    uint32_t baseVertex = 0;
    bool buildFailed = false;

    m_cpuVertices.reserve(m_items.size() * 128); // 预估容量
    m_cpuIndices.reserve(m_items.size() * 192);

    for (const auto& item : m_items) {
        if (item.type != BatchItemType::Mesh || !item.meshData.mesh || !item.meshData.material) {
            buildFailed = true;
            break;
        }

        auto mesh = item.meshData.mesh;

        if (mesh->GetIndexCount() == 0) {
            buildFailed = true;
            break;
        }

        const Matrix4& modelMatrix = item.meshData.modelMatrix;
        Matrix3 normalMatrix = modelMatrix.topLeftCorner<3, 3>();
        const float determinant = normalMatrix.determinant();
        if (std::fabs(determinant) > 1e-6f) {
            normalMatrix = normalMatrix.inverse().transpose();
        } else {
            normalMatrix = Matrix3::Identity();
        }

        size_t vertexCountBefore = m_cpuVertices.size();

        mesh->AccessVertices([&](const std::vector<Vertex>& vertices) {
            m_cpuVertices.reserve(m_cpuVertices.size() + vertices.size());
            for (const auto& vertex : vertices) {
                Vertex transformed = vertex;
                Vector4 position(vertex.position.x(), vertex.position.y(), vertex.position.z(), 1.0f);
                position = modelMatrix * position;
                transformed.position = position.head<3>();

                Vector3 transformedNormal = normalMatrix * vertex.normal;
                if (transformedNormal.norm() > 1e-6f) {
                    transformedNormal.normalize();
                } else {
                    transformedNormal = Vector3::UnitY();
                }
                transformed.normal = transformedNormal;

                m_cpuVertices.push_back(transformed);
            }
        });

        const size_t verticesAdded = m_cpuVertices.size() - vertexCountBefore;
        if (verticesAdded == 0) {
            buildFailed = true;
            break;
        }

        mesh->AccessIndices([&](const std::vector<uint32_t>& indices) {
            m_cpuIndices.reserve(m_cpuIndices.size() + indices.size());
            for (uint32_t index : indices) {
                m_cpuIndices.push_back(index + baseVertex);
            }
        });

        baseVertex += static_cast<uint32_t>(verticesAdded);
    }

    if (buildFailed || m_cpuVertices.empty() || m_cpuIndices.empty()) {
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

    auto mergedMesh = std::make_shared<Mesh>();
    mergedMesh->SetData(m_cpuVertices, m_cpuIndices);
    mergedMesh->Upload();

    if (m_resourceManager) {
        if (!m_meshHandle.IsValid()) {
            if (m_meshResourceName.empty()) {
                std::ostringstream oss;
                oss << "batch_mesh_" << std::hex << reinterpret_cast<uintptr_t>(this);
                m_meshResourceName = oss.str();
            }
            m_meshHandle = m_resourceManager->CreateMeshHandle(m_meshResourceName, mergedMesh);
        } else {
            m_resourceManager->ReloadMesh(m_meshHandle, mergedMesh);
        }
    }

    m_batchMesh = mergedMesh;
    m_gpuResourcesReady = true;
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

    if (mode != BatchingMode::CpuMerge) {
        drawFallback();
        return false;
    }

    if (!m_gpuResourcesReady || m_items.empty()) {
        drawFallback();
        return false;
    }

    const auto& firstItem = m_items.front();
    auto material = firstItem.meshData.material;
    if (!material) {
        drawFallback();
        return false;
    }

    material->Bind(renderState);

    if (auto shader = material->GetShader()) {
        if (auto* uniformMgr = shader->GetUniformManager()) {
            const Matrix4 identity = Matrix4::Identity();
            uniformMgr->SetMatrix4("uModel", identity);
            uniformMgr->SetMatrix4("model", identity);
        }
    }

    Ref<Mesh> meshToDraw = m_batchMesh;
    if (m_resourceManager && m_meshHandle.IsValid()) {
        meshToDraw = m_resourceManager->GetMeshSharedByHandle(m_meshHandle);
    }

    if (!meshToDraw) {
        drawFallback();
        return false;
    }

    meshToDraw->Draw();
    ++drawCallCounter;
    return true;
}

BatchManager::BatchManager()
    : m_mode(BatchingMode::Disabled)
    , m_resourceManager(nullptr)
    , m_warnedFallback(false) {
}

void BatchManager::SetMode(BatchingMode mode) {
    m_mode = mode;
    m_warnedFallback = false;
}

BatchingMode BatchManager::GetMode() const noexcept {
    return m_mode;
}

void BatchManager::SetResourceManager(ResourceManager* resourceManager) {
    m_resourceManager = resourceManager;
}

void BatchManager::Reset() {
    for (auto& batch : m_batches) {
        batch.Reset();
    }
    m_batches.clear();
    m_batchLookup.clear();
    m_executionOrder.clear();
    m_warnedFallback = false;
}

void BatchManager::AddItem(const BatchableItem& item) {
    if (!item.renderable) {
        return;
    }

    const bool shouldBatch = (m_mode != BatchingMode::Disabled) &&
                             item.batchable &&
                             !item.isTransparent &&
                             item.type != BatchItemType::Unsupported;

    if (!shouldBatch) {
        ExecutionCommand cmd;
        cmd.type = ExecutionCommand::Type::Immediate;
        cmd.renderable = item.renderable;
        m_executionOrder.push_back(cmd);
        return;
    }

    auto it = m_batchLookup.find(item.key);
    size_t batchIndex;
    if (it == m_batchLookup.end()) {
        batchIndex = m_batches.size();
        m_batches.emplace_back();
        m_batches.back().SetKey(item.key);
        m_batchLookup.emplace(item.key, batchIndex);

        ExecutionCommand cmd;
        cmd.type = ExecutionCommand::Type::Batch;
        cmd.batchIndex = batchIndex;
        m_executionOrder.push_back(cmd);
    } else {
        batchIndex = it->second;
    }

    m_batches[batchIndex].AddItem(item);
}

BatchManager::FlushResult BatchManager::Flush(RenderState* renderState) {
    FlushResult result{};

    if (!renderState) {
        Reset();
        return result;
    }

    for (const auto& command : m_executionOrder) {
        if (command.type == ExecutionCommand::Type::Immediate) {
            if (command.renderable && command.renderable->IsVisible()) {
                command.renderable->Render(renderState);
                ++result.drawCalls;
                ++result.fallbackDrawCalls;
            }
            ++result.fallbackBatches;
            continue;
        }

        if (command.batchIndex >= m_batches.size()) {
            continue;
        }

        auto& batch = m_batches[command.batchIndex];
        if (batch.GetItemCount() == 0) {
            continue;
        }

        switch (m_mode) {
            case BatchingMode::CpuMerge:
            case BatchingMode::GpuInstancing: {
                if (m_mode == BatchingMode::GpuInstancing && !m_warnedFallback) {
                    Logger::GetInstance().Debug(
                        "[BatchManager] GPU instancing not implemented yet, falling back to immediate rendering"
                    );
                    m_warnedFallback = true;
                }

                batch.UploadResources(m_resourceManager, m_mode);

                const uint32_t drawCallsBefore = result.drawCalls;
                const bool merged = batch.Draw(
                    renderState,
                    result.drawCalls,
                    m_mode == BatchingMode::GpuInstancing ? BatchingMode::Disabled : m_mode
                );
                const uint32_t drawCallDelta = result.drawCalls - drawCallsBefore;

                if (m_mode == BatchingMode::CpuMerge && merged) {
                    ++result.batchCount;
                    result.batchedDrawCalls += drawCallDelta;
                    result.batchedTriangles += batch.GetTriangleCount();
                    result.batchedVertices += batch.GetVertexCount();
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
                break;
            }
            default:
                break;
        }
    }

    Reset();
    return result;
}

size_t BatchManager::GetPendingItemCount() const noexcept {
    return m_executionOrder.size();
}

} // namespace Render


