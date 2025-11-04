#pragma once

#include "system.h"
#include "entity.h"
#include "render/renderer.h"
#include "render/renderable.h"
#include "render/async_resource_loader.h"
#include "render/camera.h"
#include <vector>

namespace Render {
namespace ECS {

// 前向声明
class CameraSystem;

// ============================================================
// Transform 更新系统（维护变换层级）
// ============================================================

/**
 * @brief Transform 更新系统
 * 
 * 更新所有 TransformComponent 的层级关系
 * 优先级：10（高优先级）
 */
class TransformSystem : public System {
public:
    void Update(float deltaTime) override;
    [[nodiscard]] int GetPriority() const override { return 10; }
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
     * @brief 设置异步加载器
     * @param asyncLoader 异步加载器指针
     */
    void SetAsyncLoader(AsyncResourceLoader* asyncLoader) { m_asyncLoader = asyncLoader; }
    
private:
    void LoadMeshResources();
    void LoadSpriteResources();
    void ProcessAsyncTasks();
    void ApplyPendingUpdates();  ///< 应用延迟更新
    
    // 资源加载完成回调（不直接修改组件，而是加入队列）
    void OnMeshLoaded(EntityID entity, const MeshLoadResult& result);
    void OnTextureLoaded(EntityID entity, const TextureLoadResult& result);
    
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
    
    size_t m_maxTasksPerFrame = 10;         ///< 每帧最大处理任务数
    AsyncResourceLoader* m_asyncLoader = nullptr;  ///< 异步加载器
    
    std::vector<PendingMeshUpdate> m_pendingMeshUpdates;       ///< 待应用的网格更新
    std::vector<PendingTextureUpdate> m_pendingTextureUpdates; ///< 待应用的纹理更新
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
    };
    
    /**
     * @brief 获取渲染统计信息
     * @return 渲染统计信息
     */
    [[nodiscard]] const RenderStats& GetStats() const { return m_stats; }
    
    void OnCreate(World* world) override;
    void OnDestroy() override;
    
private:
    void SubmitRenderables();
    bool ShouldCull(const Vector3& position, float radius);
    
    Renderer* m_renderer;                       ///< 渲染器指针
    CameraSystem* m_cameraSystem = nullptr;     ///< 缓存的相机系统（避免递归锁）
    RenderStats m_stats;                        ///< 渲染统计信息
    std::vector<MeshRenderable> m_renderables;  ///< Renderable 对象池（避免每帧创建销毁）
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
class SpriteRenderSystem : public System {
public:
    explicit SpriteRenderSystem(Renderer* renderer);
    
    void Update(float deltaTime) override;
    [[nodiscard]] int GetPriority() const override { return 200; }
    
private:
    Renderer* m_renderer;            ///< 渲染器指针
};

// ============================================================
// Camera 系统（管理相机和视锥体裁剪）
// ============================================================

/**
 * @brief Camera 系统
 * 
 * 管理相机组件，更新视图矩阵和投影矩阵
 * 优先级：5（最高优先级）
 */
class CameraSystem : public System {
public:
    CameraSystem() : m_mainCamera(EntityID::Invalid()) {}
    
    void Update(float deltaTime) override;
    [[nodiscard]] int GetPriority() const override { return 5; }
    
    /**
     * @brief 获取主相机实体
     * @return 主相机实体 ID
     */
    [[nodiscard]] EntityID GetMainCamera() const;
    
    /**
     * @brief 获取主相机对象
     * @return 主相机对象指针
     */
    [[nodiscard]] Camera* GetMainCameraObject() const;
    
private:
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
    
public:
    // 获取主光源数据的接口（供渲染系统使用）
    Vector3 GetPrimaryLightPosition() const { return m_primaryLightPosition; }
    Color GetPrimaryLightColor() const { return m_primaryLightColor; }
    float GetPrimaryLightIntensity() const { return m_primaryLightIntensity; }
};

} // namespace ECS
} // namespace Render

