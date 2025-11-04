#include "render/ecs/systems.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/renderable.h"
#include "render/logger.h"
#include "render/resource_manager.h"

namespace Render {
namespace ECS {

// ============================================================
// TransformSystem 实现
// ============================================================

void TransformSystem::Update(float deltaTime) {
    (void)deltaTime;  // 未使用
    
    // Transform 的层级更新由 Transform 类自动处理（通过缓存机制）
    // 这里可以添加额外的变换更新逻辑（如果需要）
}

// ============================================================
// ResourceLoadingSystem 实现
// ============================================================

ResourceLoadingSystem::ResourceLoadingSystem()
    : m_asyncLoader(nullptr) {
}

ResourceLoadingSystem::ResourceLoadingSystem(AsyncResourceLoader* asyncLoader)
    : m_asyncLoader(asyncLoader) {
}

void ResourceLoadingSystem::OnCreate(World* world) {
    System::OnCreate(world);
    Logger::GetInstance().InfoFormat("[ResourceLoadingSystem] ResourceLoadingSystem created");
}

void ResourceLoadingSystem::Update(float deltaTime) {
    (void)deltaTime;  // 未使用
    
    if (!m_asyncLoader) {
        return;
    }
    
    // 加载 Mesh 资源
    LoadMeshResources();
    
    // 加载 Sprite 资源
    LoadSpriteResources();
    
    // 处理异步任务完成回调
    ProcessAsyncTasks();
}

void ResourceLoadingSystem::LoadMeshResources() {
    // 获取所有 MeshRenderComponent
    auto entities = m_world->Query<MeshRenderComponent>();
    
    for (const auto& entity : entities) {
        auto& meshComp = m_world->GetComponent<MeshRenderComponent>(entity);
        
        // 检查是否需要加载资源
        if (!meshComp.resourcesLoaded && !meshComp.asyncLoading) {
            // 标记正在加载
            meshComp.asyncLoading = true;
            
            // 异步加载网格
            if (!meshComp.meshName.empty() && !meshComp.mesh) {
                // TODO: 实际的异步加载逻辑
                // m_asyncLoader->LoadMeshAsync(meshComp.meshName, callback);
                Logger::GetInstance().DebugFormat("[ResourceLoadingSystem] Loading mesh: %s for entity", 
                             meshComp.meshName.c_str());
            }
            
            // 异步加载材质
            if (!meshComp.materialName.empty() && !meshComp.material) {
                // TODO: 实际的异步加载逻辑
                Logger::GetInstance().DebugFormat("[ResourceLoadingSystem] Loading material: %s for entity", 
                             meshComp.materialName.c_str());
            }
        }
    }
}

void ResourceLoadingSystem::LoadSpriteResources() {
    // 获取所有 SpriteRenderComponent
    auto entities = m_world->Query<SpriteRenderComponent>();
    
    for (const auto& entity : entities) {
        auto& spriteComp = m_world->GetComponent<SpriteRenderComponent>(entity);
        
        // 检查是否需要加载资源
        if (!spriteComp.resourcesLoaded && !spriteComp.asyncLoading) {
            // 标记正在加载
            spriteComp.asyncLoading = true;
            
            // 异步加载纹理
            if (!spriteComp.textureName.empty() && !spriteComp.texture) {
                // TODO: 实际的异步加载逻辑
                Logger::GetInstance().DebugFormat("[ResourceLoadingSystem] Loading texture: %s for entity", 
                             spriteComp.textureName.c_str());
            }
        }
    }
}

void ResourceLoadingSystem::ProcessAsyncTasks() {
    // TODO: 处理异步任务完成回调
    // 这将在后续与 AsyncResourceLoader 深度集成时实现
}

// ============================================================
// MeshRenderSystem 实现
// ============================================================

MeshRenderSystem::MeshRenderSystem(Renderer* renderer)
    : m_renderer(renderer) {
    if (!m_renderer) {
        Logger::GetInstance().ErrorFormat("[MeshRenderSystem] Renderer is null");
    }
}

void MeshRenderSystem::Update(float deltaTime) {
    (void)deltaTime;  // 未使用
    
    // 重置统计信息
    m_stats = RenderStats{};
    
    // 提交渲染对象
    SubmitRenderables();
}

void MeshRenderSystem::SubmitRenderables() {
    if (!m_world || !m_renderer) {
        return;
    }
    
    // 清空上一帧的 Renderable 对象
    m_renderables.clear();
    
    // 查询所有具有 TransformComponent 和 MeshRenderComponent 的实体
    auto entities = m_world->Query<TransformComponent, MeshRenderComponent>();
    
    for (const auto& entity : entities) {
        auto& transform = m_world->GetComponent<TransformComponent>(entity);
        auto& meshComp = m_world->GetComponent<MeshRenderComponent>(entity);
        
        // 检查可见性
        if (!meshComp.visible) {
            continue;
        }
        
        // 检查资源是否已加载
        if (!meshComp.resourcesLoaded || !meshComp.mesh || !meshComp.material) {
            continue;
        }
        
        // 简单的视锥体裁剪（基于距离）
        Vector3 position = transform.GetPosition();
        float radius = 10.0f;  // TODO: 从网格包围盒计算
        
        if (ShouldCull(position, radius)) {
            m_stats.culledMeshes++;
            continue;
        }
        
        // 创建 MeshRenderable 并添加到对象池
        MeshRenderable renderable;
        renderable.SetMesh(meshComp.mesh);
        renderable.SetMaterial(meshComp.material);
        renderable.SetTransform(transform.transform);
        renderable.SetVisible(meshComp.visible);
        renderable.SetLayerID(meshComp.layerID);
        renderable.SetRenderPriority(meshComp.renderPriority);
        renderable.SetCastShadows(meshComp.castShadows);
        renderable.SetReceiveShadows(meshComp.receiveShadows);
        
        m_renderables.push_back(std::move(renderable));
        
        m_stats.visibleMeshes++;
    }
    
    // 提交所有 Renderable 到渲染器
    for (auto& renderable : m_renderables) {
        m_renderer->SubmitRenderable(&renderable);
        m_stats.drawCalls++;
    }
}

bool MeshRenderSystem::ShouldCull(const Vector3& position, float radius) {
    // TODO: 实现真正的视锥体裁剪
    // 这里暂时返回 false（不裁剪）
    (void)position;
    (void)radius;
    return false;
}

// ============================================================
// SpriteRenderSystem 实现
// ============================================================

SpriteRenderSystem::SpriteRenderSystem(Renderer* renderer)
    : m_renderer(renderer) {
    if (!m_renderer) {
        Logger::GetInstance().ErrorFormat("[SpriteRenderSystem] Renderer is null");
    }
}

void SpriteRenderSystem::Update(float deltaTime) {
    (void)deltaTime;  // 未使用
    
    if (!m_world) {
        return;
    }
    
    // 查询所有具有 TransformComponent 和 SpriteRenderComponent 的实体
    auto entities = m_world->Query<TransformComponent, SpriteRenderComponent>();
    
    for (const auto& entity : entities) {
        auto& transform = m_world->GetComponent<TransformComponent>(entity);
        auto& spriteComp = m_world->GetComponent<SpriteRenderComponent>(entity);
        
        // 检查可见性
        if (!spriteComp.visible) {
            continue;
        }
        
        // 检查资源是否已加载
        if (!spriteComp.resourcesLoaded || !spriteComp.texture) {
            continue;
        }
        
        // TODO: 实现 2D 精灵渲染
        // 这将在后续阶段实现
        (void)transform;
    }
}

// ============================================================
// CameraSystem 实现
// ============================================================

void CameraSystem::Update(float deltaTime) {
    (void)deltaTime;  // 未使用
    
    if (!m_world) {
        return;
    }
    
    // 查询所有具有 TransformComponent 和 CameraComponent 的实体
    auto entities = m_world->Query<TransformComponent, CameraComponent>();
    
    // 更新所有相机
    for (const auto& entity : entities) {
        auto& transform = m_world->GetComponent<TransformComponent>(entity);
        auto& cameraComp = m_world->GetComponent<CameraComponent>(entity);
        
        if (!cameraComp.active || !cameraComp.camera) {
            continue;
        }
        
        // TODO: 更新相机位置和朝向
        // Camera 类可能需要专门的接口来设置位置和旋转
        (void)transform;
        
        // 设置主相机（第一个激活的相机）
        if (!m_mainCamera.IsValid()) {
            m_mainCamera = entity;
        }
    }
}

EntityID CameraSystem::GetMainCamera() const {
    return m_mainCamera;
}

Camera* CameraSystem::GetMainCameraObject() const {
    if (!m_world || !m_mainCamera.IsValid()) {
        return nullptr;
    }
    
    if (!m_world->HasComponent<CameraComponent>(m_mainCamera)) {
        return nullptr;
    }
    
    const auto& cameraComp = m_world->GetComponent<CameraComponent>(m_mainCamera);
    return cameraComp.camera.get();
}

// ============================================================
// LightSystem 实现
// ============================================================

LightSystem::LightSystem(Renderer* renderer)
    : m_renderer(renderer) {
    if (!m_renderer) {
        Logger::GetInstance().ErrorFormat("[LightSystem] Renderer is null");
    }
}

void LightSystem::Update(float deltaTime) {
    (void)deltaTime;  // 未使用
    
    // 更新光源 uniform
    UpdateLightUniforms();
}

std::vector<EntityID> LightSystem::GetVisibleLights() const {
    if (!m_world) {
        return {};
    }
    
    // 查询所有具有 LightComponent 的实体
    auto entities = m_world->Query<LightComponent>();
    
    // 过滤启用的光源
    std::vector<EntityID> visibleLights;
    for (const auto& entity : entities) {
        const auto& lightComp = m_world->GetComponent<LightComponent>(entity);
        if (lightComp.enabled) {
            visibleLights.push_back(entity);
        }
    }
    
    return visibleLights;
}

size_t LightSystem::GetLightCount() const {
    return GetVisibleLights().size();
}

void LightSystem::UpdateLightUniforms() {
    // TODO: 收集光源数据并上传到着色器
    // 这将在后续阶段实现
}

} // namespace ECS
} // namespace Render

