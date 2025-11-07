#include "render/render_batching.h"

#include "render/logger.h"
#include "render/material.h"
#include "render/mesh.h"
#include "render/resource_manager.h"
#include "render/renderable.h"
#include "render/shader.h"
#include "render/gl_thread_checker.h"
#include <glad/glad.h>
#include <cmath>
#include <cstring>
#include <sstream>
#include <exception>
#include <chrono>

namespace Render {

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
        GL_THREAD_CHECK();
        glDeleteBuffers(1, &m_instanceBuffer);
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

    ReleaseGpuResources();

    if (mode == BatchingMode::GpuInstancing) {
        m_sourceMesh = m_items.front().meshData.mesh;
        m_instancePayloads.clear();

        if (!m_sourceMesh) {
            m_instanceCount = 0;
            m_gpuResourcesReady = false;
            return;
        }

        m_instancePayloads.reserve(m_items.size());
        for (const auto& item : m_items) {
            InstancePayload payload{};
            std::memcpy(payload.matrix, item.meshData.modelMatrix.data(), sizeof(payload.matrix));
            m_instancePayloads.push_back(payload);
        }

        m_instanceCount = static_cast<uint32_t>(m_instancePayloads.size());

        if (m_instanceCount == 0) {
            m_gpuResourcesReady = false;
            return;
        }

        m_cachedTriangleCount = static_cast<uint32_t>(m_sourceMesh->GetIndexCount() / 3);
        m_drawVertexCount = static_cast<uint32_t>(m_sourceMesh->GetVertexCount());

        if (m_instanceBuffer == 0) {
            GL_THREAD_CHECK();
            glGenBuffers(1, &m_instanceBuffer);
        }

        uint32_t vao = m_sourceMesh->GetVertexArrayID();
        if (vao == 0) {
            m_gpuResourcesReady = false;
            return;
        }

        GL_THREAD_CHECK();
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffer);
        glBufferData(GL_ARRAY_BUFFER,
                     m_instancePayloads.size() * sizeof(InstancePayload),
                     m_instancePayloads.data(),
                     GL_DYNAMIC_DRAW);

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

    if (mode == BatchingMode::GpuInstancing) {
        if (!m_gpuResourcesReady || m_items.empty() || !m_sourceMesh || m_instanceCount == 0) {
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

        UniformManager* uniformMgr = nullptr;
        if (auto shader = material->GetShader()) {
            uniformMgr = shader->GetUniformManager();
            if (uniformMgr) {
                const Matrix4 identity = Matrix4::Identity();
                uniformMgr->SetMatrix4("uModel", identity);
                if (uniformMgr->HasUniform("uHasInstanceData")) {
                    uniformMgr->SetBool("uHasInstanceData", true);
                }
            }
        }

        m_sourceMesh->DrawInstanced(m_instanceCount);
        ++drawCallCounter;

        if (uniformMgr && uniformMgr->HasUniform("uHasInstanceData")) {
            uniformMgr->SetBool("uHasInstanceData", false);
        }
        return true;
    }

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
            if (uniformMgr->HasUniform("uHasInstanceData")) {
                uniformMgr->SetBool("uHasInstanceData", false);
            }
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
        if (localItem.meshData.mesh) {
            localItem.key.meshHandle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(localItem.meshData.mesh.get()));
        }
    } else {
        localItem.key.meshHandle = 0;
    }

    bool shouldBatch = false;
    switch (m_mode) {
        case BatchingMode::CpuMerge:
            shouldBatch = localItem.batchable && !localItem.isTransparent &&
                          localItem.type != BatchItemType::Unsupported;
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
    std::lock_guard<std::mutex> lock(m_queueMutex);
    return m_pendingItems.size() + (m_processing ? 1u : 0u);
}

} // namespace Render


