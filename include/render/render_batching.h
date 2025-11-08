#pragma once

#include "render/render_state.h"
#include "render/material_sort_key.h"
#include "render/renderable.h"
#include "render/resource_handle.h"
#include "render/types.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <thread>
#include <condition_variable>
#include <deque>
#include <atomic>

namespace Render {

class ResourceManager;
class Shader;
class Mesh;
class Material;
class Texture;
class SpriteBatcher;

/**
 * @brief 批处理模式
 */
enum class BatchingMode {
    Disabled,       ///< 禁用批处理，逐对象渲染
    CpuMerge,       ///< CPU 侧合批（合并网格数据后一次性 Draw）
    GpuInstancing   ///< GPU 实例化渲染
};

/**
 * @brief 批次键，用于区分不同的渲染状态组合
 *
 * Phase 0：结构占位，具体字段将在后续阶段补全
 */
struct RenderBatchKey {
    RenderableType renderableType = RenderableType::Mesh;
    MaterialSortKey materialKey{};
    uint64_t materialHandle = 0;
    uint64_t shaderHandle = 0;
    uint64_t meshHandle = 0;
    BlendMode blendMode = BlendMode::None;
    CullFace cullFace = CullFace::Back;
    bool depthTest = true;
    bool depthWrite = true;
    bool castShadows = true;
    bool receiveShadows = true;
    uint32_t layerID = 0;
    uint64_t textureHandle = 0;
    uint32_t viewHash = 0;
    uint32_t projectionHash = 0;
    bool screenSpace = true;

    bool operator==(const RenderBatchKey& other) const noexcept {
        return materialKey == other.materialKey &&
               renderableType == other.renderableType &&
               materialHandle == other.materialHandle &&
               shaderHandle == other.shaderHandle &&
               blendMode == other.blendMode &&
               cullFace == other.cullFace &&
               depthTest == other.depthTest &&
               depthWrite == other.depthWrite &&
               castShadows == other.castShadows &&
               receiveShadows == other.receiveShadows &&
               layerID == other.layerID &&
               meshHandle == other.meshHandle &&
               textureHandle == other.textureHandle &&
               viewHash == other.viewHash &&
               projectionHash == other.projectionHash &&
               screenSpace == other.screenSpace;
    }
};

struct RenderBatchKeyHasher {
    size_t operator()(const RenderBatchKey& key) const noexcept {
        auto combine = [](size_t& seed, size_t value) {
            constexpr size_t kMul = 0x9e3779b97f4a7c15ull;
            seed ^= value + kMul + (seed << 6) + (seed >> 2);
        };

        size_t hash = MaterialSortKeyHasher{}(key.materialKey);
        combine(hash, std::hash<uint64_t>{}(key.materialHandle));
        combine(hash, std::hash<uint64_t>{}(key.shaderHandle));
        using BlendModeUnderlying = std::underlying_type_t<BlendMode>;
        using CullFaceUnderlying = std::underlying_type_t<CullFace>;
        using RenderableTypeUnderlying = std::underlying_type_t<RenderableType>;
        combine(hash, static_cast<size_t>(static_cast<BlendModeUnderlying>(key.blendMode)));
        combine(hash, static_cast<size_t>(static_cast<CullFaceUnderlying>(key.cullFace)));
        combine(hash, static_cast<size_t>(static_cast<RenderableTypeUnderlying>(key.renderableType)));
        combine(hash, static_cast<size_t>(key.depthTest));
        combine(hash, static_cast<size_t>(key.depthWrite));
        combine(hash, static_cast<size_t>(key.castShadows));
        combine(hash, static_cast<size_t>(key.receiveShadows));
        combine(hash, std::hash<uint32_t>{}(key.layerID));
        combine(hash, std::hash<uint64_t>{}(key.meshHandle));
        combine(hash, std::hash<uint64_t>{}(key.textureHandle));
        combine(hash, std::hash<uint32_t>{}(key.viewHash));
        combine(hash, std::hash<uint32_t>{}(key.projectionHash));
        combine(hash, std::hash<uint8_t>{}(static_cast<uint8_t>(key.screenSpace)));
        return hash;
    }
};

enum class BatchItemType {
    Unsupported,
    Mesh,
    Sprite,
    Text
};

struct MeshBatchData {
    Ref<Mesh> mesh;
    Ref<Material> material;
    MaterialOverride materialOverride;
    Matrix4 modelMatrix = Matrix4::Identity();
    bool castShadows = true;
    bool receiveShadows = true;
    bool hasMaterialOverride = false;
};

struct InstancePayload {
    float matrix[16] = {0.0f};
};

struct SpriteBatchData {
    SpriteBatcher* batcher = nullptr;
    size_t batchIndex = 0;
    uint32_t instanceCount = 0;
    BlendMode blendMode = BlendMode::Alpha;
    bool screenSpace = true;
    Ref<Texture> texture;
};

/**
 * @brief 批处理可提交条目
 */
struct BatchableItem {
    Renderable* renderable = nullptr;
    BatchItemType type = BatchItemType::Unsupported;
    RenderBatchKey key{};
    MeshBatchData meshData{};
    SpriteBatchData spriteData{};
    TextRenderBatchData textData{};
    bool batchable = false;
    bool isTransparent = false;
    bool instanceEligible = false;
};

struct BatchCommand {
    enum class Type {
        Immediate,
        Batch
    } type = Type::Immediate;

    size_t batchIndex = 0;
    Renderable* renderable = nullptr;
};

class BatchCommandBuffer {
public:
    void Clear();
    void AddImmediate(Renderable* renderable);
    void AddBatch(size_t batchIndex);
    void Swap(BatchCommandBuffer& other);
    [[nodiscard]] const std::vector<BatchCommand>& GetCommands() const noexcept { return m_commands; }
    [[nodiscard]] size_t GetCommandCount() const noexcept { return m_commands.size(); }

private:
    std::vector<BatchCommand> m_commands;
    mutable std::mutex m_mutex;
};

/**
 * @brief 渲染批次占位实现
 *
 * Phase 0：仅保留接口，具体的合批逻辑将在后续阶段补充
 */
class RenderBatch {
public:
    RenderBatch() = default;

    void SetKey(const RenderBatchKey& key);
    [[nodiscard]] const RenderBatchKey& GetKey() const noexcept;

    void Reset();
    void AddItem(const BatchableItem& item);
    void UploadResources(ResourceManager* resourceManager, BatchingMode mode);
    bool Draw(RenderState* renderState, uint32_t& drawCallCounter, BatchingMode mode);
    [[nodiscard]] size_t GetItemCount() const noexcept { return m_items.size(); }
    [[nodiscard]] uint32_t GetTriangleCount() const noexcept { return m_cachedTriangleCount; }
    [[nodiscard]] uint32_t GetVertexCount() const noexcept { return static_cast<uint32_t>(m_cpuVertices.size()); }
    [[nodiscard]] uint32_t GetInstanceCount() const noexcept { return m_instanceCount; }

private:
    RenderBatchKey m_key{};
    bool m_keyInitialized = false;
    std::vector<BatchableItem> m_items;
    std::vector<Vertex> m_cpuVertices;
    std::vector<uint32_t> m_cpuIndices;
    uint32_t m_indexCount = 0;
    bool m_gpuResourcesReady = false;
    uint32_t m_drawVertexCount = 0;
    uint32_t m_cachedTriangleCount = 0;
    MeshHandle m_meshHandle;
    Ref<Mesh> m_batchMesh;
    Ref<Mesh> m_sourceMesh;
    std::vector<InstancePayload> m_instancePayloads;
    uint32_t m_instanceBuffer = 0;
    uint32_t m_instanceCount = 0;
    std::string m_meshResourceName;
    uint64_t m_keyHash = 0;
    ResourceManager* m_resourceManager = nullptr;

    void ReleaseGpuResources();
};

/**
 * @brief 批处理管理器（Phase 0 骨架）
 */
class BatchManager {
public:
    struct FlushResult {
        uint32_t drawCalls = 0;
        uint32_t batchCount = 0;
        uint32_t batchedDrawCalls = 0;
        uint32_t instancedDrawCalls = 0;
        uint32_t instancedInstances = 0;
        uint32_t fallbackDrawCalls = 0;
        uint32_t batchedTriangles = 0;
        uint32_t batchedVertices = 0;
        uint32_t fallbackBatches = 0;
        uint32_t workerProcessed = 0;
        uint32_t workerMaxQueueDepth = 0;
        float workerWaitTimeMs = 0.0f;
    };

    BatchManager();
    ~BatchManager();

    void SetMode(BatchingMode mode);
    [[nodiscard]] BatchingMode GetMode() const noexcept;

    void SetResourceManager(ResourceManager* resourceManager);

    void Reset();
    void AddItem(const BatchableItem& item);
    FlushResult Flush(RenderState* renderState);

    [[nodiscard]] size_t GetPendingItemCount() const noexcept;

private:
    struct BatchStorage {
        std::vector<RenderBatch> batches;
        std::unordered_map<RenderBatchKey, size_t, RenderBatchKeyHasher> lookup;

        void Clear();
    };

    struct WorkItem {
        BatchableItem item;
        bool shouldBatch = false;
    };

    BatchingMode m_mode;
    BatchStorage m_executionStorage;
    BatchStorage m_recordingStorage;
    BatchCommandBuffer m_executionBuffer;
    BatchCommandBuffer m_recordingBuffer;
    ResourceManager* m_resourceManager;

    std::thread m_workerThread;
    mutable std::mutex m_queueMutex;
    std::condition_variable m_queueCv;
    std::condition_variable m_idleCv;
    std::deque<WorkItem> m_pendingItems;
    bool m_shutdown;
    bool m_processing;

    std::mutex m_storageMutex;
    std::atomic<uint32_t> m_workerProcessedCount;
    std::atomic<uint32_t> m_workerQueueHighWater;
    std::atomic<uint64_t> m_workerDrainWaitNs;

    void SwapBuffers();
    void DrainWorker();
    void WorkerLoop();
    void ProcessWorkItem(const WorkItem& workItem);
    void EnqueueWork(WorkItem workItem);
};

} // namespace Render


