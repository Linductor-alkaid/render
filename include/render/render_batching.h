#pragma once

#include "render/render_state.h"
#include "render/renderable.h"
#include "render/resource_handle.h"
#include "render/types.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace Render {

class ResourceManager;
class Shader;
class Mesh;
class Material;

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
    uint64_t materialHandle = 0;
    uint64_t shaderHandle = 0;
    BlendMode blendMode = BlendMode::None;
    CullFace cullFace = CullFace::Back;
    bool depthTest = true;
    bool depthWrite = true;
    bool castShadows = true;
    bool receiveShadows = true;
    uint32_t layerID = 0;

    bool operator==(const RenderBatchKey& other) const noexcept {
        return renderableType == other.renderableType &&
               materialHandle == other.materialHandle &&
               shaderHandle == other.shaderHandle &&
               blendMode == other.blendMode &&
               cullFace == other.cullFace &&
               depthTest == other.depthTest &&
               depthWrite == other.depthWrite &&
               castShadows == other.castShadows &&
               receiveShadows == other.receiveShadows &&
               layerID == other.layerID;
    }
};

struct RenderBatchKeyHasher {
    size_t operator()(const RenderBatchKey& key) const noexcept {
        size_t hash = std::hash<uint64_t>{}(key.materialHandle);
        hash ^= std::hash<uint64_t>{}(key.shaderHandle) + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
        using BlendModeUnderlying = std::underlying_type_t<BlendMode>;
        using CullFaceUnderlying = std::underlying_type_t<CullFace>;
        using RenderableTypeUnderlying = std::underlying_type_t<RenderableType>;
        hash ^= static_cast<size_t>(static_cast<BlendModeUnderlying>(key.blendMode)) + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
        hash ^= static_cast<size_t>(static_cast<CullFaceUnderlying>(key.cullFace)) + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
        hash ^= static_cast<size_t>(static_cast<RenderableTypeUnderlying>(key.renderableType)) + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
        hash ^= static_cast<size_t>(key.depthTest) + (hash << 6) + (hash >> 2);
        hash ^= static_cast<size_t>(key.depthWrite) + (hash << 6) + (hash >> 2);
        hash ^= static_cast<size_t>(key.castShadows) + (hash << 6) + (hash >> 2);
        hash ^= static_cast<size_t>(key.receiveShadows) + (hash << 6) + (hash >> 2);
        hash ^= std::hash<uint32_t>{}(key.layerID) + (hash << 6) + (hash >> 2);
        return hash;
    }
};

enum class BatchItemType {
    Unsupported,
    Mesh
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

/**
 * @brief 批处理可提交条目
 */
struct BatchableItem {
    Renderable* renderable = nullptr;
    BatchItemType type = BatchItemType::Unsupported;
    RenderBatchKey key{};
    MeshBatchData meshData{};
    bool batchable = false;
    bool isTransparent = false;
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
        uint32_t fallbackDrawCalls = 0;
        uint32_t batchedTriangles = 0;
        uint32_t batchedVertices = 0;
        uint32_t fallbackBatches = 0;
    };

    BatchManager();

    void SetMode(BatchingMode mode);
    [[nodiscard]] BatchingMode GetMode() const noexcept;

    void SetResourceManager(ResourceManager* resourceManager);

    void Reset();
    void AddItem(const BatchableItem& item);
    FlushResult Flush(RenderState* renderState);

    [[nodiscard]] size_t GetPendingItemCount() const noexcept;

private:
    struct ExecutionCommand {
        enum class Type {
            Immediate,
            Batch
        } type = Type::Immediate;

        size_t batchIndex = 0;
        Renderable* renderable = nullptr;
    };

    BatchingMode m_mode;
    std::vector<RenderBatch> m_batches;
    std::unordered_map<RenderBatchKey, size_t, RenderBatchKeyHasher> m_batchLookup;
    std::vector<ExecutionCommand> m_executionOrder;
    ResourceManager* m_resourceManager;
    bool m_warnedFallback;
};

} // namespace Render


