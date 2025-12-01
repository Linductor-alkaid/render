#pragma once

#include "system.h"
#include "entity.h"
#include "render/renderer.h"
#include "render/renderable.h"
#include "render/sprite/sprite_batcher.h"
#include "render/async_resource_loader.h"
#include "render/model_loader.h"
#include "render/camera.h"
#include "render/types.h"
#include "render/lod_instanced_renderer.h"  // LOD 实例化渲染器
#include <vector>
#include <memory>
#include <unordered_map>

namespace Render {

// 前向声明
class Mesh;

namespace ECS {

// 前向声明
class CameraSystem;
struct GeometryComponent;

// ============================================================
// Transform 更新系统（维护变换层级）
// ============================================================

/**
 * @brief Transform 更新系统
 * 
 * **功能**：
 * - 同步父子关系（实体ID -> Transform指针）
 * - 批量更新 Transform 世界变换
 * - 验证父实体有效性
 * 
 * **优化**：
 * - 按层级深度排序，确保父对象先更新
 * - 只更新标记为dirty的Transform
 * - 批量处理减少单个调用开销
 * 
 * 优先级：10（高优先级，在其他系统之前运行）
 */
class TransformSystem : public System {
public:
    void Update(float deltaTime) override;
    [[nodiscard]] int GetPriority() const override { return 10; }
    
    /**
     * @brief 同步所有实体的父子关系
     * 
     * @note 将 TransformComponent 的 parentEntity 同步到 Transform 的父指针
     * @note 验证父实体有效性，自动清除无效的父子关系
     */
    void SyncParentChildRelations();
    
    /**
     * @brief 批量更新所有 dirty Transform 的世界变换
     * 
     * @note 按层级深度排序，确保父对象先更新
     * @note 只更新标记为 dirty 的 Transform
     */
    void BatchUpdateTransforms();
    
    /**
     * @brief 验证所有 Transform 状态（调试用）
     * @return 无效 Transform 数量
     */
    size_t ValidateAll();
    
    /**
     * @brief 启用/禁用批量更新优化
     * @param enable 是否启用
     */
    void SetBatchUpdateEnabled(bool enable) { m_batchUpdateEnabled = enable; }
    
    /**
     * @brief 获取上次更新的统计信息
     */
    struct UpdateStats {
        size_t totalEntities = 0;      ///< 总实体数
        size_t dirtyTransforms = 0;    ///< 需要更新的 Transform 数
        size_t syncedParents = 0;      ///< 同步的父子关系数
        size_t clearedParents = 0;     ///< 清除的无效父子关系数
        size_t batchGroups = 0;         ///< 批量更新组数（阶段2.1优化）
    };
    
    [[nodiscard]] const UpdateStats& GetStats() const { return m_stats; }
    
private:
    bool m_batchUpdateEnabled = true;  ///< 是否启用批量更新
    UpdateStats m_stats;               ///< 更新统计信息
};

// ============================================================
// 资源加载系统（处理异步资源加载）
// ============================================================

/**
 * @brief 资源加载系统
 * 
 * 处理 MeshRenderComponent 和 SpriteRenderComponent 的异步资源加载
 * 优先级：20（次高优先级）
 */
class ResourceLoadingSystem : public System {
public:
    ResourceLoadingSystem();
    explicit ResourceLoadingSystem(AsyncResourceLoader* asyncLoader);
    
    void OnCreate(World* world) override;
    void OnDestroy() override;
    void Update(float deltaTime) override;
    [[nodiscard]] int GetPriority() const override { return 20; }
    
    /**
     * @brief 设置每帧最大处理任务数
     * @param maxTasks 最大任务数
     */
    void SetMaxTasksPerFrame(size_t maxTasks) { m_maxTasksPerFrame = maxTasks; }
    
    /**
     * @brief 获取每帧最大处理任务数
     * @return 每帧最大处理任务数
     */
    [[nodiscard]] size_t GetMaxTasksPerFrame() const { return m_maxTasksPerFrame; }
    
    /**
     * @brief 设置异步加载器
     * @param asyncLoader 异步加载器指针
     */
    void SetAsyncLoader(AsyncResourceLoader* asyncLoader) { m_asyncLoader = asyncLoader; }
    
private:
    void LoadMeshResources();
    void LoadModelResources();
    void LoadSpriteResources();
    void LoadTextureOverrides();  ///< 加载纹理覆盖
    void ProcessAsyncTasks();
    void ApplyPendingUpdates();  ///< 应用延迟更新
    
    // 资源加载完成回调（不直接修改组件，而是加入队列）
    void OnMeshLoaded(EntityID entity, const MeshLoadResult& result);
    void OnTextureLoaded(EntityID entity, const TextureLoadResult& result);
    void OnModelLoaded(EntityID entity, const ModelLoadResult& result);
    
    // 延迟更新的数据结构
    struct PendingMeshUpdate {
        EntityID entity;
        std::shared_ptr<Mesh> mesh;
        bool success;
        std::string errorMessage;
    };
    
    struct PendingTextureUpdate {
        EntityID entity;
        std::shared_ptr<Texture> texture;
        bool success;
        std::string errorMessage;
    };
    
    // ✅ 新增：纹理覆盖更新结构
    struct PendingTextureOverrideUpdate {
        EntityID entity;
        std::string textureName;        ///< 纹理在材质中的名称（如"diffuse"）
        std::shared_ptr<Texture> texture;
        bool success;
        std::string errorMessage;
    };

    struct PendingModelUpdate {
        EntityID entity;
        ModelPtr model;
        std::vector<std::string> meshResourceNames;
        std::vector<std::string> materialResourceNames;
        bool success;
        std::string errorMessage;
    };
    
    size_t m_maxTasksPerFrame = 10;         ///< 每帧最大处理任务数
    AsyncResourceLoader* m_asyncLoader = nullptr;  ///< 异步加载器
    
    std::vector<PendingMeshUpdate> m_pendingMeshUpdates;       ///< 待应用的网格更新
    std::vector<PendingTextureUpdate> m_pendingTextureUpdates; ///< 待应用的纹理更新
    std::vector<PendingTextureOverrideUpdate> m_pendingTextureOverrideUpdates; ///< 待应用的纹理覆盖更新
    std::vector<PendingModelUpdate> m_pendingModelUpdates;     ///< 待应用的模型更新
    std::mutex m_pendingMutex;  ///< 保护待更新队列的互斥锁
    
    std::atomic<bool> m_shuttingDown{false};  ///< 是否正在关闭
};

// ============================================================
// Mesh 渲染系统
// ============================================================

/**
 * @brief Mesh 渲染系统
 * 
 * 遍历所有 MeshRenderComponent，创建 MeshRenderable 并提交渲染
 * 优先级：100（标准优先级）
 */
class MeshRenderSystem : public System {
public:
    explicit MeshRenderSystem(Renderer* renderer);
    
    void Update(float deltaTime) override;
    [[nodiscard]] int GetPriority() const override { return 100; }
    
    /**
     * @brief 渲染统计信息
     */
    struct RenderStats {
        size_t visibleMeshes = 0;    ///< 可见网格数量
        size_t culledMeshes = 0;     ///< 被裁剪的网格数量
        size_t drawCalls = 0;        ///< 绘制调用数量
        
        // LOD 统计信息
        size_t lodEnabledEntities = 0;      ///< 启用 LOD 的实体数量
        size_t lod0Count = 0;               ///< 使用 LOD0 的实体数量
        size_t lod1Count = 0;               ///< 使用 LOD1 的实体数量
        size_t lod2Count = 0;               ///< 使用 LOD2 的实体数量
        size_t lod3Count = 0;               ///< 使用 LOD3 的实体数量
        size_t lodCulledCount = 0;          ///< 被 LOD 剔除的实体数量
    };
    
    /**
     * @brief 获取渲染统计信息
     * @return 渲染统计信息
     */
    [[nodiscard]] const RenderStats& GetStats() const { return m_stats; }
    
    /**
     * @brief 设置是否启用 LOD 实例化渲染
     * 
     * @deprecated 阶段2.3：请使用 Renderer::SetLODInstancingEnabled() 代替
     * 此方法保留用于向后兼容，但会同步到 Renderer 的设置
     * 
     * @param enabled 是否启用
     * 
     * @note 启用后，相同网格、相同材质、相同 LOD 级别的实例会批量渲染
     * @note 可以显著减少 Draw Call，提升性能
     */
    void SetLODInstancingEnabled(bool enabled);
    
    /**
     * @brief 获取是否启用 LOD 实例化渲染
     * 
     * 阶段2.3：从 Renderer 获取设置，而不是使用本地设置
     * 
     * @return 如果启用返回 true
     */
    [[nodiscard]] bool IsLODInstancingEnabled() const;
    
    /**
     * @brief 设置是否启用 LOD 视锥体裁剪优化（阶段3.3）
     * 
     * 启用后，使用 LODFrustumCullingSystem 进行批量视锥体裁剪和 LOD 选择
     * 可以进一步提升性能，特别是在大量实体的场景中
     * 
     * @param enabled 是否启用
     */
    void SetLODFrustumCullingEnabled(bool enabled) { m_lodFrustumCullingEnabled = enabled; }
    
    /**
     * @brief 获取是否启用 LOD 视锥体裁剪优化（阶段3.3）
     * @return 如果启用返回 true
     */
    [[nodiscard]] bool IsLODFrustumCullingEnabled() const { return m_lodFrustumCullingEnabled; }
    
    /**
     * @brief 设置LOD实例化渲染的分批处理参数
     * 
     * 控制每帧处理的实例数量，避免一次性处理大量实例导致卡死。
     * 建议值：简单场景100，中等50-100，复杂20-50，超复杂10-20
     * 
     * @param maxInstancesPerFrame 每帧最大处理实例数（默认100）
     */
    void SetLODInstancingBatchSize(size_t maxInstancesPerFrame) {
        m_lodRenderer.SetMaxInstancesPerFrame(maxInstancesPerFrame);
    }
    
    /**
     * @brief 获取当前分批处理大小
     * @return 每帧最大处理实例数
     */
    [[nodiscard]] size_t GetLODInstancingBatchSize() const {
        return m_lodRenderer.GetMaxInstancesPerFrame();
    }
    
    /**
     * @brief 获取当前待处理的实例数量
     */
    [[nodiscard]] size_t GetPendingInstanceCount() const {
        return m_lodRenderer.GetPendingInstanceCount();
    }
    
    /**
     * @brief 启用多线程数据准备（阶段3.2）
     * @param numThreads 工作线程数量（0=禁用，-1=自动检测）
     * 
     * 注意：多线程优化需要非常小心的同步设计，建议先实现前面的优化，
     * 确认性能瓶颈后再考虑。多线程主要用于处理大量实例的场景。
     */
    void EnableLODMultithreading(int numThreads = -1) {
        m_lodRenderer.EnableMultithreading(numThreads);
    }
    
    /**
     * @brief 禁用多线程数据准备（阶段3.2）
     */
    void DisableLODMultithreading() {
        m_lodRenderer.DisableMultithreading();
    }
    
    void OnCreate(World* world) override;
    void OnDestroy() override;
    
private:
    void SubmitRenderables();
    bool ShouldCull(const Vector3& position, float radius) const;
    
    /**
     * @brief 批量计算 LOD 级别（阶段2.2）
     * @param entities 实体列表
     * @param cameraPosition 相机位置
     * @param frameId 当前帧 ID
     */
    void BatchCalculateLOD(const std::vector<EntityID>& entities, 
                           const Vector3& cameraPosition, 
                           uint64_t frameId);
    
    /**
     * @brief 获取主相机位置
     * @return 相机位置，如果无法获取返回零向量
     */
    Vector3 GetMainCameraPosition() const;
    
    /**
     * @brief 获取当前帧 ID
     * @return 帧 ID
     */
    uint64_t GetCurrentFrameId() const;
    
    Renderer* m_renderer;                       ///< 渲染器指针
    CameraSystem* m_cameraSystem = nullptr;     ///< 缓存的相机系统（避免递归锁）
    RenderStats m_stats;                        ///< 渲染统计信息
    std::vector<MeshRenderable> m_renderables;  ///< Renderable 对象池（避免每帧创建销毁）
    
    // LOD 实例化渲染（阶段2.2 + 阶段2.3）
    LODInstancedRenderer m_lodRenderer;         ///< LOD 实例化渲染器
    // 注意：阶段2.3后，m_lodInstancingEnabled 不再使用
    // 改为从 Renderer 获取设置（通过 IsLODInstancingEnabled()）
    uint64_t m_frameId = 0;                     ///< 当前帧 ID（用于 LOD 计算）
    
    // 阶段3.3：LOD 视锥体裁剪优化
    bool m_lodFrustumCullingEnabled = false;    ///< 是否启用 LOD 视锥体裁剪优化
};

// ============================================================
// Model 渲染系统
// ============================================================

class ModelRenderSystem : public System {
public:
    explicit ModelRenderSystem(Renderer* renderer);

    void Update(float deltaTime) override;
    [[nodiscard]] int GetPriority() const override { return 105; }

    struct RenderStats {
        size_t visibleModels = 0;
        size_t culledModels = 0;
        size_t submittedParts = 0;
        size_t submittedRenderables = 0;
        
        // LOD 统计信息
        size_t lodEnabledEntities = 0;      ///< 启用 LOD 的实体数量
        size_t lod0Count = 0;               ///< 使用 LOD0 的实体数量
        size_t lod1Count = 0;               ///< 使用 LOD1 的实体数量
        size_t lod2Count = 0;               ///< 使用 LOD2 的实体数量
        size_t lod3Count = 0;               ///< 使用 LOD3 的实体数量
        size_t lodCulledCount = 0;          ///< 被 LOD 剔除的实体数量
    };

    [[nodiscard]] const RenderStats& GetStats() const { return m_stats; }

    /**
     * @brief 设置LOD实例化渲染的分批处理参数
     * 
     * 控制每帧处理的实例数量，避免一次性处理大量实例导致卡死。
     * 建议值：简单场景100，中等50-100，复杂20-50，超复杂10-20
     * 
     * @param maxInstancesPerFrame 每帧最大处理实例数（默认100）
     */
    void SetLODInstancingBatchSize(size_t maxInstancesPerFrame) {
        m_lodRenderer.SetMaxInstancesPerFrame(maxInstancesPerFrame);
    }
    
    /**
     * @brief 获取当前分批处理大小
     * @return 每帧最大处理实例数
     */
    [[nodiscard]] size_t GetLODInstancingBatchSize() const {
        return m_lodRenderer.GetMaxInstancesPerFrame();
    }
    
    /**
     * @brief 获取当前待处理的实例数量
     */
    [[nodiscard]] size_t GetPendingInstanceCount() const {
        return m_lodRenderer.GetPendingInstanceCount();
    }

    void OnCreate(World* world) override;
    void OnDestroy() override;

private:
    void SubmitRenderables();
    bool ShouldCull(const Vector3& position, float radius) const;
    
    /**
     * @brief 检查是否启用 LOD 实例化渲染（阶段2.3）
     * @return 如果启用返回 true
     */
    [[nodiscard]] bool IsLODInstancingEnabled() const;

    Renderer* m_renderer = nullptr;
    CameraSystem* m_cameraSystem = nullptr;
    RenderStats m_stats;
    std::vector<ModelRenderable> m_renderables;
    
    // LOD 实例化渲染（阶段2.2 + 阶段2.3）
    LODInstancedRenderer m_lodRenderer;         ///< LOD 实例化渲染器
    uint64_t m_frameId = 0;                     ///< 当前帧 ID（用于 LOD 计算）
    
    // 阶段3.3：LOD 视锥体裁剪优化（可选）
    bool m_lodFrustumCullingEnabled = false;    ///< 是否启用 LOD 视锥体裁剪优化
};

// ============================================================
// Sprite 渲染系统（2D）
// ============================================================

/**
 * @brief Sprite 渲染系统
 * 
 * 遍历所有 SpriteRenderComponent，创建 SpriteRenderable 并提交渲染
 * 优先级：200（较低优先级，在 3D 之后渲染）
 */
class SpriteAnimationSystem : public System {
public:
    SpriteAnimationSystem() = default;

    void Update(float deltaTime) override;
    [[nodiscard]] int GetPriority() const override { return 180; }
};

class SpriteRenderSystem : public System {
public:
    explicit SpriteRenderSystem(Renderer* renderer);
    
    void Update(float deltaTime) override;
    [[nodiscard]] int GetPriority() const override { return 200; }
    void OnCreate(World* world) override;
    void OnDestroy() override;
    [[nodiscard]] size_t GetLastBatchCount() const { return m_lastBatchCount; }
    [[nodiscard]] size_t GetLastSubmittedSpriteCount() const { return m_lastSubmittedSpriteCount; }

private:
    Renderer* m_renderer;            ///< 渲染器指针
    CameraSystem* m_cameraSystem = nullptr; ///< 相机系统引用（用于世界空间渲染）
    SpriteBatcher m_batcher;         ///< 精灵批处理器
    std::vector<std::unique_ptr<SpriteBatchRenderable>> m_batchRenderables; ///< 批次渲染代理
    size_t m_lastBatchCount = 0;
    size_t m_lastSubmittedSpriteCount = 0;
};

// ============================================================
// Camera 系统（管理相机和视锥体裁剪）
// ============================================================

/**
 * @brief Camera 系统
 * 
 * 管理相机组件，更新视图矩阵和投影矩阵
 * 优先级：5（最高优先级）
 * 
 * **主相机管理策略**：
 * - 自动验证主相机有效性，如果无效会自动选择新的主相机
 * - 按照depth值选择主相机（depth越小优先级越高）
 * - 支持手动设置和清除主相机
 * 
 * **线程安全性**：
 * - 此系统不是线程安全的，应在主线程的Update中调用
 * - Camera对象本身是线程安全的
 */
class CameraSystem : public System {
public:
    CameraSystem() : m_mainCamera(EntityID::Invalid()) {}
    
    void Update(float deltaTime) override;
    [[nodiscard]] int GetPriority() const override { return 5; }
    
    // ==================== 主相机查询 ====================
    
    /**
     * @brief 获取主相机实体
     * @return 主相机实体 ID（可能无效）
     */
    [[nodiscard]] EntityID GetMainCamera() const;
    
    /**
     * @brief 获取主相机对象（裸指针）
     * @return 主相机对象指针，失败返回nullptr
     * 
     * @warning 返回的指针可能失效，建议使用GetMainCameraSharedPtr()
     * @deprecated 推荐使用GetMainCameraSharedPtr()替代
     */
    [[nodiscard]] Camera* GetMainCameraObject() const;
    
    /**
     * @brief 获取主相机对象（shared_ptr）
     * @return 主相机对象智能指针，失败返回nullptr
     * 
     * @note 推荐使用此方法，更加安全
     */
    [[nodiscard]] Ref<Camera> GetMainCameraSharedPtr() const;
    
    // ==================== 主相机管理 ====================
    
    /**
     * @brief 手动设置主相机
     * @param entity 要设置为主相机的实体
     * @return 成功返回true，失败返回false
     * 
     * @note 会验证实体是否有效、是否有CameraComponent等
     */
    bool SetMainCamera(EntityID entity);
    
    /**
     * @brief 清除主相机（下次Update时会自动选择）
     */
    void ClearMainCamera();
    
    /**
     * @brief 按深度选择主相机
     * @return 返回选中的相机实体ID（可能无效）
     * 
     * @note 会选择depth最小的激活相机
     */
    EntityID SelectMainCameraByDepth();
    
private:
    /**
     * @brief 验证主相机是否仍然有效
     * @return 如果有效返回true
     */
    bool ValidateMainCamera() const;
    
    EntityID m_mainCamera;   ///< 主相机实体 ID
};

// ============================================================
// Light 系统（光照管理）
// ============================================================

/**
 * @brief Light 系统
 * 
 * 管理光源组件，收集光源数据并上传到着色器
 * 优先级：50（中等优先级）
 */
class LightSystem : public System {
public:
    explicit LightSystem(Renderer* renderer);
    
    void Update(float deltaTime) override;
    [[nodiscard]] int GetPriority() const override { return 50; }
    
    /**
     * @brief 获取可见光源
     * @return 可见光源实体列表
     */
    [[nodiscard]] std::vector<EntityID> GetVisibleLights() const;
    
    /**
     * @brief 获取光源数量
     * @return 光源数量
     */
    [[nodiscard]] size_t GetLightCount() const;
    
private:
    void UpdateLightUniforms();
    
    Renderer* m_renderer;            ///< 渲染器指针
    
    // 缓存的主光源数据
    Vector3 m_primaryLightPosition;
    Color m_primaryLightColor;
    float m_primaryLightIntensity = 1.0f;

    std::unordered_map<EntityID, Lighting::LightHandle, EntityID::Hash> m_lightHandles;
    
public:
    // 获取主光源数据的接口（供渲染系统使用）
    Vector3 GetPrimaryLightPosition() const { return m_primaryLightPosition; }
    Color GetPrimaryLightColor() const { return m_primaryLightColor; }
    float GetPrimaryLightIntensity() const { return m_primaryLightIntensity; }
};

// ============================================================
// Uniform 系统（全局 uniform 管理）
// ============================================================

/**
 * @brief Uniform 系统
 * 
 * 自动管理全局 uniform，包括相机矩阵、光源数据等
 * 遍历所有材质，设置通用的 uniform 参数
 * 优先级：90（在渲染系统之前，在光照系统之后）
 */
class UniformSystem : public System {
public:
    explicit UniformSystem(Renderer* renderer);
    
    void Update(float deltaTime) override;
    [[nodiscard]] int GetPriority() const override { return 90; }
    
    void OnCreate(World* world) override;
    void OnDestroy() override;
    
    /**
     * @brief 设置是否启用自动 uniform 管理
     * @param enable 是否启用
     */
    void SetEnabled(bool enable) { m_enabled = enable; }
    
    /**
     * @brief 获取是否启用
     */
    [[nodiscard]] bool IsEnabled() const { return m_enabled; }
    
private:
    void SetCameraUniforms();
    void SetLightUniforms();
    void SetTimeUniforms();
    
    Renderer* m_renderer;                    ///< 渲染器指针
    CameraSystem* m_cameraSystem = nullptr;  ///< 缓存的相机系统
    LightSystem* m_lightSystem = nullptr;    ///< 缓存的光源系统
    
    bool m_enabled = true;                   ///< 是否启用自动管理
    float m_time = 0.0f;                     ///< 累计时间
    Vector3 m_lastCameraPosition = Vector3::Zero(); ///< 最近一次相机位置缓存
};

// ============================================================
// Window 系统（窗口管理）
// ============================================================

/**
 * @brief Window 系统
 * 
 * 监控窗口大小变化，自动更新相机宽高比和视口
 * 优先级：3（在相机系统之前）
 * 
 * @note 前置条件：
 * 1. Renderer 必须已经初始化（调用 Renderer::Initialize()）
 * 2. OpenGLContext 必须已经初始化
 * 3. 必须在主线程（OpenGL线程）中注册和更新
 * 
 * @note 实现机制：
 * - 使用 OpenGLContext 的窗口大小变化回调机制（事件驱动）
 * - 不再使用轮询检测窗口大小变化
 * - 自动更新相机宽高比和渲染视口
 */
class WindowSystem : public System {
public:
    explicit WindowSystem(Renderer* renderer);
    
    void Update(float deltaTime) override;
    [[nodiscard]] int GetPriority() const override { return 3; }
    
    void OnCreate(World* world) override;
    void OnDestroy() override;
    
private:
    /// 窗口大小变化回调处理
    void OnWindowResized(int width, int height);
    
    Renderer* m_renderer;                    ///< 渲染器指针
    CameraSystem* m_cameraSystem = nullptr;  ///< 缓存的相机系统
};

// ============================================================
// Geometry 系统（几何形状生成）
// ============================================================

/**
 * @brief Geometry 系统
 * 
 * 自动生成基本几何形状的网格
 * 优先级：15（在资源加载之前）
 */
class GeometrySystem : public System {
public:
    GeometrySystem() = default;
    
    void Update(float deltaTime) override;
    [[nodiscard]] int GetPriority() const override { return 15; }
    
private:
    void GenerateGeometry();
    std::shared_ptr<Mesh> CreateGeometryMesh(const GeometryComponent& geom);
};

// ============================================================
// ResourceCleanup 系统（资源清理）
// ============================================================

/**
 * @brief 资源清理系统
 * 
 * 定期清理未使用的资源，防止内存泄漏
 * 优先级：1000（最后执行，低优先级）
 */
class ResourceCleanupSystem : public System {
public:
    /**
     * @brief 构造函数
     * @param cleanupIntervalSeconds 清理间隔（秒）
     * @param unusedFrameThreshold 资源未使用多少帧后清理（默认 60 帧 = 约 1 秒）
     */
    explicit ResourceCleanupSystem(float cleanupIntervalSeconds = 60.0f, 
                                  uint32_t unusedFrameThreshold = 60);
    
    void Update(float deltaTime) override;
    [[nodiscard]] int GetPriority() const override { return 1000; }
    
    /**
     * @brief 设置清理间隔
     * @param seconds 间隔秒数
     */
    void SetCleanupInterval(float seconds) { m_cleanupInterval = seconds; }
    
    /**
     * @brief 获取清理间隔
     */
    [[nodiscard]] float GetCleanupInterval() const { return m_cleanupInterval; }
    
    /**
     * @brief 手动触发清理
     */
    void ForceCleanup();
    
    /**
     * @brief 启用/禁用自动清理
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    
    /**
     * @brief 是否启用
     */
    [[nodiscard]] bool IsEnabled() const { return m_enabled; }
    
    /**
     * @brief 获取上次清理的统计信息
     */
    struct CleanupStats {
        size_t meshCleaned = 0;
        size_t textureCleaned = 0;
        size_t materialCleaned = 0;
        size_t shaderCleaned = 0;
        size_t totalCleaned = 0;
    };
    
    [[nodiscard]] const CleanupStats& GetLastCleanupStats() const { return m_lastStats; }
    
private:
    float m_timer = 0.0f;                    ///< 计时器
    float m_cleanupInterval;                 ///< 清理间隔（秒）
    uint32_t m_unusedFrameThreshold;         ///< 未使用帧数阈值
    bool m_enabled = true;                   ///< 是否启用
    CleanupStats m_lastStats;                ///< 上次清理统计
};

} // namespace ECS
} // namespace Render

