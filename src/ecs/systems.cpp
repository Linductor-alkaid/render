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
    m_shuttingDown = false;
    Logger::GetInstance().InfoFormat("[ResourceLoadingSystem] ResourceLoadingSystem created");
}

void ResourceLoadingSystem::OnDestroy() {
    // 标记正在关闭，防止回调继续执行
    m_shuttingDown = true;
    
    // 清空待处理的更新队列
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        m_pendingMeshUpdates.clear();
        m_pendingTextureUpdates.clear();
    }
    
    Logger::GetInstance().InfoFormat("[ResourceLoadingSystem] ResourceLoadingSystem destroyed");
    System::OnDestroy();
}

void ResourceLoadingSystem::Update(float deltaTime) {
    (void)deltaTime;  // 未使用
    
    if (!m_asyncLoader) {
        return;
    }
    
    // 1. 首先应用上一帧收集的待更新数据（此时没有持有World的锁）
    ApplyPendingUpdates();
    
    // 2. 加载 Mesh 资源
    LoadMeshResources();
    
    // 3. 加载 Sprite 资源
    LoadSpriteResources();
    
    // 4. 处理异步任务完成回调（回调会将更新加入队列，不直接修改组件）
    ProcessAsyncTasks();
}

void ResourceLoadingSystem::LoadMeshResources() {
    // 获取所有 MeshRenderComponent
    auto entities = m_world->Query<MeshRenderComponent>();
    
    for (const auto& entity : entities) {
        auto& meshComp = m_world->GetComponent<MeshRenderComponent>(entity);
        
        // ✅ 如果资源已经加载完成，跳过
        if (meshComp.resourcesLoaded) {
            continue;
        }
        
        // ✅ 如果mesh和material都已存在，且没有设置meshName/materialName，
        //    说明是直接设置的资源，标记为已加载
        if (meshComp.mesh && meshComp.meshName.empty()) {
            // 检查material
            bool materialReady = (meshComp.material != nullptr) || meshComp.materialName.empty();
            if (materialReady) {
                meshComp.resourcesLoaded = true;
                meshComp.asyncLoading = false;
                Logger::GetInstance().DebugFormat("[ResourceLoadingSystem] Entity %u has pre-loaded resources, marked as loaded", 
                                                  entity.index);
                continue;
            }
        }
        
        // 检查是否正在加载
        if (meshComp.asyncLoading) {
            continue;  // 已经在加载中，跳过
        }
        
        // 检查是否需要加载资源
        if (!meshComp.resourcesLoaded && !meshComp.asyncLoading) {
            // 如果没有指定meshName，也没有mesh，则无法加载
            if (meshComp.meshName.empty() && !meshComp.mesh) {
                Logger::GetInstance().WarningFormat("[ResourceLoadingSystem] Entity %u: no mesh and no meshName specified", 
                                                    entity.index);
                meshComp.resourcesLoaded = true;  // 标记为已加载（避免重复警告）
                continue;
            }
            
            // 标记正在加载
            meshComp.asyncLoading = true;
            
            // 异步加载网格
            if (!meshComp.meshName.empty() && !meshComp.mesh) {
                Logger::GetInstance().DebugFormat("[ResourceLoadingSystem] Starting async load for mesh: %s", 
                             meshComp.meshName.c_str());
                
                // ✅ 使用weak_ptr捕获World的生命周期
                EntityID entityCopy = entity;  // 捕获entity
                std::weak_ptr<World> worldWeak;
                
                try {
                    // 尝试获取World的shared_ptr
                    worldWeak = m_world->weak_from_this();
                } catch (const std::bad_weak_ptr&) {
                    // World不是通过shared_ptr管理的，使用原有方式
                    Logger::GetInstance().WarningFormat("[ResourceLoadingSystem] World not managed by shared_ptr, using legacy callback");
                    
                    m_asyncLoader->LoadMeshAsync(
                        meshComp.meshName,
                        meshComp.meshName,
                        [this, entityCopy](const MeshLoadResult& result) {
                            if (!m_shuttingDown.load()) {
                                this->OnMeshLoaded(entityCopy, result);
                            }
                        }
                    );
                    continue;
                }
                
                // 使用safe callback（捕获weak_ptr）
                m_asyncLoader->LoadMeshAsync(
                    meshComp.meshName,
                    meshComp.meshName,
                    [this, entityCopy, worldWeak](const MeshLoadResult& result) {
                        // ✅ 尝试锁定weak_ptr，检查World是否还存活
                        if (auto worldShared = worldWeak.lock()) {
                            // World仍然存活，可以安全访问
                            if (!m_shuttingDown.load()) {
                                this->OnMeshLoaded(entityCopy, result);
                            } else {
                                Logger::GetInstance().DebugFormat("[ResourceLoadingSystem] System shutting down, skip mesh callback");
                            }
                        } else {
                            // World已被销毁，忽略回调
                            Logger::GetInstance().InfoFormat("[ResourceLoadingSystem] World destroyed, skip mesh callback");
                        }
                    }
                );
            }
            
            // 材质通常通过ResourceManager同步获取
            // 因为材质文件较小，不需要异步加载
            if (!meshComp.materialName.empty() && !meshComp.material) {
                auto& resMgr = ResourceManager::GetInstance();
                meshComp.material = resMgr.GetMaterial(meshComp.materialName);
                
                if (!meshComp.material) {
                    Logger::GetInstance().WarningFormat("[ResourceLoadingSystem] Material not found: %s", 
                                 meshComp.materialName.c_str());
                }
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
                Logger::GetInstance().DebugFormat("[ResourceLoadingSystem] Starting async load for texture: %s", 
                             spriteComp.textureName.c_str());
                
                // ✅ 使用weak_ptr捕获World的生命周期
                EntityID entityCopy = entity;  // 捕获entity
                std::weak_ptr<World> worldWeak;
                
                try {
                    // 尝试获取World的shared_ptr
                    worldWeak = m_world->weak_from_this();
                } catch (const std::bad_weak_ptr&) {
                    // World不是通过shared_ptr管理的，使用原有方式
                    Logger::GetInstance().WarningFormat("[ResourceLoadingSystem] World not managed by shared_ptr, using legacy callback");
                    
                    m_asyncLoader->LoadTextureAsync(
                        spriteComp.textureName,
                        spriteComp.textureName,
                        true,
                        [this, entityCopy](const TextureLoadResult& result) {
                            if (!m_shuttingDown.load()) {
                                this->OnTextureLoaded(entityCopy, result);
                            }
                        }
                    );
                    continue;
                }
                
                // 使用safe callback（捕获weak_ptr）
                m_asyncLoader->LoadTextureAsync(
                    spriteComp.textureName,
                    spriteComp.textureName,
                    true,  // 生成mipmap
                    [this, entityCopy, worldWeak](const TextureLoadResult& result) {
                        // ✅ 尝试锁定weak_ptr，检查World是否还存活
                        if (auto worldShared = worldWeak.lock()) {
                            // World仍然存活，可以安全访问
                            if (!m_shuttingDown.load()) {
                                this->OnTextureLoaded(entityCopy, result);
                            } else {
                                Logger::GetInstance().DebugFormat("[ResourceLoadingSystem] System shutting down, skip texture callback");
                            }
                        } else {
                            // World已被销毁，忽略回调
                            Logger::GetInstance().InfoFormat("[ResourceLoadingSystem] World destroyed, skip texture callback");
                        }
                    }
                );
            }
        }
    }
}

void ResourceLoadingSystem::ProcessAsyncTasks() {
    if (!m_asyncLoader) {
        return;
    }
    
    // 每帧处理最多10个完成的异步任务
    // 这会在主线程中执行GPU上传
    m_asyncLoader->ProcessCompletedTasks(10);
}

void ResourceLoadingSystem::OnMeshLoaded(EntityID entity, const MeshLoadResult& result) {
    // 注意：此回调在主线程（ProcessCompletedTasks调用时）执行
    // 但此时可能持有World::Update的锁，所以不能直接修改组件
    // 而是将更新加入延迟队列
    
    // ✅ 安全检查：如果System正在关闭，忽略回调
    if (m_shuttingDown.load()) {
        Logger::GetInstance().InfoFormat("[ResourceLoadingSystem] System shutting down, ignoring mesh load callback");
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    
    PendingMeshUpdate update;
    update.entity = entity;
    update.mesh = result.resource;
    update.success = result.IsSuccess();
    update.errorMessage = result.errorMessage;
    
    m_pendingMeshUpdates.push_back(std::move(update));
    
    if (result.IsSuccess()) {
        Logger::GetInstance().DebugFormat("[ResourceLoadingSystem] Mesh loaded, queued for update: %s", 
                     result.name.c_str());
    } else {
        Logger::GetInstance().ErrorFormat("[ResourceLoadingSystem] Failed to load mesh: %s - %s", 
                     result.name.c_str(), result.errorMessage.c_str());
    }
}

void ResourceLoadingSystem::OnTextureLoaded(EntityID entity, const TextureLoadResult& result) {
    // 注意：此回调在主线程（ProcessCompletedTasks调用时）执行
    // 但此时可能持有World::Update的锁，所以不能直接修改组件
    // 而是将更新加入延迟队列
    
    // ✅ 安全检查：如果System正在关闭，忽略回调
    if (m_shuttingDown.load()) {
        Logger::GetInstance().InfoFormat("[ResourceLoadingSystem] System shutting down, ignoring texture load callback");
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    
    PendingTextureUpdate update;
    update.entity = entity;
    update.texture = result.resource;
    update.success = result.IsSuccess();
    update.errorMessage = result.errorMessage;
    
    m_pendingTextureUpdates.push_back(std::move(update));
    
    if (result.IsSuccess()) {
        Logger::GetInstance().DebugFormat("[ResourceLoadingSystem] Texture loaded, queued for update: %s", 
                     result.name.c_str());
    } else {
        Logger::GetInstance().ErrorFormat("[ResourceLoadingSystem] Failed to load texture: %s - %s", 
                     result.name.c_str(), result.errorMessage.c_str());
    }
}

void ResourceLoadingSystem::ApplyPendingUpdates() {
    if (!m_world) {
        return;
    }
    
    // ✅ 如果正在关闭，清空队列并返回
    if (m_shuttingDown.load()) {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        m_pendingMeshUpdates.clear();
        m_pendingTextureUpdates.clear();
        return;
    }
    
    // 获取待更新的队列（交换到本地变量以减少锁持有时间）
    std::vector<PendingMeshUpdate> meshUpdates;
    std::vector<PendingTextureUpdate> textureUpdates;
    
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        meshUpdates.swap(m_pendingMeshUpdates);
        textureUpdates.swap(m_pendingTextureUpdates);
    }
    
    // 应用网格更新
    for (const auto& update : meshUpdates) {
        // ✅ 再次检查是否正在关闭
        if (m_shuttingDown.load()) {
            Logger::GetInstance().InfoFormat("[ResourceLoadingSystem] System shutting down, abort applying updates");
            break;
        }
        
        // ✅ 多重安全检查：实体是否有效
        if (!m_world->IsValidEntity(update.entity)) {
            Logger::GetInstance().WarningFormat("[ResourceLoadingSystem] Entity %u is no longer valid", 
                                               update.entity.index);
            continue;
        }
        
        // ✅ 检查组件是否存在
        if (!m_world->HasComponent<MeshRenderComponent>(update.entity)) {
            Logger::GetInstance().WarningFormat("[ResourceLoadingSystem] Entity %u missing MeshRenderComponent", 
                                               update.entity.index);
            continue;
        }
        
        // ✅ 安全获取组件引用
        try {
            auto& meshComp = m_world->GetComponent<MeshRenderComponent>(update.entity);
            
            if (update.success && update.mesh) {
                meshComp.mesh = update.mesh;
                meshComp.resourcesLoaded = true;
                meshComp.asyncLoading = false;
                
                Logger::GetInstance().InfoFormat("[ResourceLoadingSystem] ✅ Mesh applied successfully to entity %u (vertices: %zu)", 
                                                 update.entity.index, 
                                                 update.mesh->GetVertexCount());
            } else {
                // ✅ 加载失败：标记为已完成，避免重复尝试
                meshComp.asyncLoading = false;
                meshComp.resourcesLoaded = true;  // 标记为已加载（失败也算完成）
                Logger::GetInstance().ErrorFormat("[ResourceLoadingSystem] Mesh loading failed for entity %u: %s (marked as loaded to prevent retry)", 
                                                  update.entity.index, update.errorMessage.c_str());
            }
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat("[ResourceLoadingSystem] Exception applying mesh update: %s", e.what());
        }
    }
    
    // 应用纹理更新
    for (const auto& update : textureUpdates) {
        // ✅ 再次检查是否正在关闭
        if (m_shuttingDown.load()) {
            Logger::GetInstance().InfoFormat("[ResourceLoadingSystem] System shutting down, abort applying updates");
            break;
        }
        
        // ✅ 多重安全检查：实体是否有效
        if (!m_world->IsValidEntity(update.entity)) {
            Logger::GetInstance().WarningFormat("[ResourceLoadingSystem] Entity %u is no longer valid", 
                                               update.entity.index);
            continue;
        }
        
        // ✅ 检查组件是否存在
        if (!m_world->HasComponent<SpriteRenderComponent>(update.entity)) {
            Logger::GetInstance().WarningFormat("[ResourceLoadingSystem] Entity %u missing SpriteRenderComponent", 
                                               update.entity.index);
            continue;
        }
        
        // ✅ 安全获取组件引用
        try {
            auto& spriteComp = m_world->GetComponent<SpriteRenderComponent>(update.entity);
            
            if (update.success) {
                spriteComp.texture = update.texture;
                spriteComp.resourcesLoaded = true;
                spriteComp.asyncLoading = false;
                
                Logger::GetInstance().InfoFormat("[ResourceLoadingSystem] Texture applied successfully to entity %u", 
                                                 update.entity.index);
            } else {
                spriteComp.asyncLoading = false;
                Logger::GetInstance().ErrorFormat("[ResourceLoadingSystem] Texture loading failed for entity %u: %s", 
                                                  update.entity.index, update.errorMessage.c_str());
            }
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat("[ResourceLoadingSystem] Exception applying texture update: %s", e.what());
        }
    }
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

void MeshRenderSystem::OnCreate(World* world) {
    System::OnCreate(world);
    
    // 注意：不在这里获取CameraSystem，因为此时World::RegisterSystem持有unique_lock
    // 需要在World::PostInitialize后手动设置，或在ShouldCull中按需获取
}

void MeshRenderSystem::OnDestroy() {
    m_cameraSystem = nullptr;
    System::OnDestroy();
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
        Logger::GetInstance().WarningFormat("[MeshRenderSystem] World or Renderer is null");
        return;
    }
    
    // 清空上一帧的 Renderable 对象
    m_renderables.clear();
    
    // 查询所有具有 TransformComponent 和 MeshRenderComponent 的实体
    auto entities = m_world->Query<TransformComponent, MeshRenderComponent>();
    
    static bool firstFrame = true;
    if (firstFrame) {
        Logger::GetInstance().InfoFormat("[MeshRenderSystem] Found %zu entities with Transform+MeshRender", entities.size());
        firstFrame = false;
    }
    
    for (const auto& entity : entities) {
        auto& transform = m_world->GetComponent<TransformComponent>(entity);
        auto& meshComp = m_world->GetComponent<MeshRenderComponent>(entity);
        
        // 检查可见性
        if (!meshComp.visible) {
            Logger::GetInstance().DebugFormat("[MeshRenderSystem] Entity %u not visible, skip", entity.index);
            continue;
        }
        
        // 检查资源是否已加载
        if (!meshComp.resourcesLoaded || !meshComp.mesh || !meshComp.material) {
            Logger::GetInstance().DebugFormat("[MeshRenderSystem] Entity %u not ready: resourcesLoaded=%d, hasMesh=%d, hasMaterial=%d", 
                                             entity.index, meshComp.resourcesLoaded, 
                                             (meshComp.mesh != nullptr), (meshComp.material != nullptr));
            continue;
        }
        
        // 视锥体裁剪优化
        Vector3 position = transform.GetPosition();
        
        // 从网格包围盒计算半径
        float radius = 1.0f;  // 默认半径
        if (meshComp.mesh) {
            AABB bounds = meshComp.mesh->CalculateBounds();
            Vector3 size = bounds.max - bounds.min;
            radius = size.norm() * 0.5f;  // 包围盒对角线的一半作为半径
            
            // 考虑Transform的缩放
            Vector3 scale = transform.GetScale();
            float maxScale = std::max(std::max(scale.x(), scale.y()), scale.z());
            radius *= maxScale;
        }
        
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
    
    // ✅ 调试信息：显示提交的数量
    static int logCounter = 0;
    if (logCounter++ < 10 || logCounter % 60 == 0) {
        Logger::GetInstance().InfoFormat("[MeshRenderSystem] Submitted %zu renderables (total entities: %zu, culled: %zu)", 
                                         m_stats.visibleMeshes, entities.size(), m_stats.culledMeshes);
    }
}

bool MeshRenderSystem::ShouldCull(const Vector3& position, float radius) {
    // 注意：不能在这里调用GetSystem，因为此时World::Update持有shared_lock
    // 会导致尝试获取nested shared_lock而死锁
    // 暂时禁用视锥体裁剪，直到实现proper的PostInitialize机制
    
    (void)position;
    (void)radius;
    
    return false;  // 禁用裁剪
    
    /*
    // 原有的裁剪逻辑（需要在World::PostInitialize中设置m_cameraSystem后才能使用）
    if (!m_cameraSystem) {
        return false;
    }
    
    Camera* mainCamera = m_cameraSystem->GetMainCameraObject();
    if (!mainCamera) {
        return false;
    }
    
    const Frustum& frustum = mainCamera->GetFrustum();
    return !frustum.IntersectsSphere(position, radius);
    */
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
        
        // 同步Transform到Camera
        Vector3 pos = transform.GetPosition();
        Quaternion rot = transform.GetRotation();
        
        cameraComp.camera->SetPosition(pos);
        cameraComp.camera->SetRotation(rot);
        
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
    // 收集所有可见光源的数据
    // 注意：实际的uniform设置应该在渲染时进行，这里只是准备数据
    
    if (!m_world) {
        return;
    }
    
    auto visibleLights = GetVisibleLights();
    
    if (visibleLights.empty()) {
        return;
    }
    
    // 对于简单的Phong模型，我们只需要第一个光源
    // 更复杂的系统可以支持多光源
    EntityID firstLight = visibleLights[0];
    
    if (!m_world->HasComponent<TransformComponent>(firstLight)) {
        return;
    }
    
    const auto& transform = m_world->GetComponent<TransformComponent>(firstLight);
    const auto& lightComp = m_world->GetComponent<LightComponent>(firstLight);
    
    // 缓存光源数据供渲染使用
    m_primaryLightPosition = transform.GetPosition();
    m_primaryLightColor = lightComp.color;
    m_primaryLightIntensity = lightComp.intensity;
    
    // 在实际应用中，这些数据会在MeshRenderSystem渲染时
    // 通过UniformManager设置到着色器
    // 示例：shader->GetUniformManager()->SetVector3("uLightPos", m_primaryLightPosition);
}

} // namespace ECS
} // namespace Render

