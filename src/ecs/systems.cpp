#include "render/ecs/systems.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/renderable.h"
#include "render/logger.h"
#include "render/resource_manager.h"
#include "render/shader_cache.h"
// ✅ 修复：移除 TextureLoader 头文件，统一使用 ResourceManager [[memory:7392268]]
#include "render/error.h"
#include "render/mesh_loader.h"

namespace Render {
namespace ECS {

// ============================================================
// TransformSystem 实现（方案B - 批量更新和实体ID管理）
// ============================================================

void TransformSystem::Update(float deltaTime) {
    (void)deltaTime;  // 未使用
    
    if (!m_world) return;
    
    // 重置统计信息
    m_stats = UpdateStats{};
    
    // 1. 同步父子关系（实体ID -> Transform指针）
    SyncParentChildRelations();
    
    // 2. 批量更新 Transform（如果启用）
    if (m_batchUpdateEnabled) {
        BatchUpdateTransforms();
    }
    
    // 3. 定期验证（调试模式）
    #ifdef DEBUG
    static int frameCount = 0;
    if (++frameCount % 300 == 0) {  // 每5秒验证一次（假设60FPS）
        size_t invalidCount = ValidateAll();
        if (invalidCount > 0) {
            Logger::GetInstance().WarningFormat(
                "[TransformSystem] Found %zu invalid Transform(s)", invalidCount
            );
        }
    }
    #endif
}

void TransformSystem::SyncParentChildRelations() {
    if (!m_world) return;
    
    auto entities = m_world->Query<TransformComponent>();
    m_stats.totalEntities = entities.size();
    
    for (const auto& entity : entities) {
        auto& comp = m_world->GetComponent<TransformComponent>(entity);
        
        if (!comp.parentEntity.IsValid()) {
            // 无父实体，确保 Transform 父指针也为空
            if (comp.transform && comp.transform->GetParent() != nullptr) {
                comp.transform->SetParent(nullptr);
            }
            continue;
        }
        
        // 有父实体，验证并同步
        if (!comp.ValidateParentEntity(m_world)) {
            // 验证失败，可能父实体已销毁或不一致
            if (!comp.parentEntity.IsValid()) {
                // 已被清除
                m_stats.clearedParents++;
                continue;
            }
            
            // 需要重新同步
            if (m_world->IsValidEntity(comp.parentEntity) && 
                m_world->HasComponent<TransformComponent>(comp.parentEntity)) {
                
                auto& parentComp = m_world->GetComponent<TransformComponent>(comp.parentEntity);
                if (comp.transform && parentComp.transform) {
                    // 重新设置父指针
                    bool success = comp.transform->SetParent(parentComp.transform.get());
                    if (success) {
                        m_stats.syncedParents++;
                    } else {
                        // 设置失败（循环引用等），清除父实体
                        Logger::GetInstance().WarningFormat(
                            "[TransformSystem] Failed to sync parent for entity %u, clearing", 
                            entity.index
                        );
                        comp.parentEntity = EntityID::Invalid();
                        comp.transform->SetParent(nullptr);
                        m_stats.clearedParents++;
                    }
                }
            }
        }
    }
    
    // 输出统计信息（可选）
    static int logCounter = 0;
    if (logCounter++ % 600 == 0 && (m_stats.syncedParents > 0 || m_stats.clearedParents > 0)) {
        Logger::GetInstance().DebugFormat(
            "[TransformSystem] Synced %zu parents, cleared %zu invalid parents", 
            m_stats.syncedParents, m_stats.clearedParents
        );
    }
}

void TransformSystem::BatchUpdateTransforms() {
    if (!m_world) return;
    
    auto entities = m_world->Query<TransformComponent>();
    if (entities.empty()) return;
    
    // 收集需要更新的 Transform
    struct TransformInfo {
        Transform* transform;
        int depth;
    };
    std::vector<TransformInfo> dirtyTransforms;
    dirtyTransforms.reserve(entities.size() / 4);  // 预估25%的Transform是dirty的
    
    for (const auto& entity : entities) {
        auto& comp = m_world->GetComponent<TransformComponent>(entity);
        if (comp.transform && comp.transform->IsDirty()) {
            dirtyTransforms.push_back({
                comp.transform.get(),
                comp.transform->GetHierarchyDepth()
            });
        }
    }
    
    m_stats.dirtyTransforms = dirtyTransforms.size();
    
    if (dirtyTransforms.empty()) {
        return;  // 没有需要更新的
    }
    
    // 按层级深度排序（父对象先更新）
    std::sort(dirtyTransforms.begin(), dirtyTransforms.end(),
        [](const TransformInfo& a, const TransformInfo& b) {
            return a.depth < b.depth;
        });
    
    // 批量更新
    for (const auto& info : dirtyTransforms) {
        info.transform->ForceUpdateWorldTransform();
    }
    
    // 输出统计信息（可选）
    static int logCounter = 0;
    if (logCounter++ % 60 == 0 && m_stats.dirtyTransforms > 0) {
        Logger::GetInstance().DebugFormat(
            "[TransformSystem] Batch updated %zu Transform(s)", m_stats.dirtyTransforms
        );
    }
}

size_t TransformSystem::ValidateAll() {
    if (!m_world) return 0;
    
    auto entities = m_world->Query<TransformComponent>();
    size_t invalidCount = 0;
    
    for (const auto& entity : entities) {
        const auto& comp = m_world->GetComponent<TransformComponent>(entity);
        if (!comp.Validate()) {
            Logger::GetInstance().WarningFormat(
                "[TransformSystem] Entity %u has invalid Transform: %s", 
                entity.index, comp.DebugString().c_str()
            );
            invalidCount++;
        }
        
        // 验证父实体一致性
        if (comp.parentEntity.IsValid()) {
            if (!m_world->IsValidEntity(comp.parentEntity)) {
                Logger::GetInstance().WarningFormat(
                    "[TransformSystem] Entity %u has invalid parent entity %u", 
                    entity.index, comp.parentEntity.index
                );
                invalidCount++;
            }
        }
    }
    
    return invalidCount;
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
    
    // ✅ 检查AsyncResourceLoader初始化状态
    if (m_asyncLoader && !m_asyncLoader->IsInitialized()) {
        Logger::GetInstance().WarningFormat(
            "[ResourceLoadingSystem] AsyncResourceLoader is not initialized. "
            "Please call AsyncResourceLoader::GetInstance().Initialize() before creating this system. "
            "Async resource loading will be disabled.");
        m_asyncLoader = nullptr;  // 禁用异步加载
    }
    
    Logger::GetInstance().InfoFormat("[ResourceLoadingSystem] ResourceLoadingSystem created");
}

void ResourceLoadingSystem::OnDestroy() {
    // 标记正在关闭，防止回调继续执行
    m_shuttingDown = true;
    
    // ✅ 清理AsyncResourceLoader中的所有待处理任务
    if (m_asyncLoader && m_asyncLoader->IsInitialized()) {
        size_t pendingCount = m_asyncLoader->GetPendingTaskCount();
        size_t loadingCount = m_asyncLoader->GetLoadingTaskCount();
        size_t uploadCount = m_asyncLoader->GetWaitingUploadCount();
        
        if (pendingCount + loadingCount + uploadCount > 0) {
            Logger::GetInstance().InfoFormat(
                "[ResourceLoadingSystem] Clearing async tasks (pending: %zu, loading: %zu, waiting upload: %zu)",
                pendingCount, loadingCount, uploadCount
            );
            
            // 注意：ClearAllPendingTasks() 只清理未开始的任务，已在处理的任务会完成
            // 但由于我们设置了 m_shuttingDown 和 weak_ptr 保护，回调会被安全忽略
            m_asyncLoader->ClearAllPendingTasks();
        }
    }
    
    // 清空待处理的更新队列
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        m_pendingMeshUpdates.clear();
        m_pendingTextureUpdates.clear();
        m_pendingTextureOverrideUpdates.clear();
    }
    
    Logger::GetInstance().InfoFormat("[ResourceLoadingSystem] ResourceLoadingSystem destroyed");
    System::OnDestroy();
}

void ResourceLoadingSystem::Update(float deltaTime) {
    (void)deltaTime;  // 未使用
    
    // ✅ 更严格的检查：确保AsyncResourceLoader已初始化
    if (!m_asyncLoader || !m_asyncLoader->IsInitialized()) {
        return;
    }
    
    // 1. 首先应用上一帧收集的待更新数据（此时没有持有World的锁）
    ApplyPendingUpdates();
    
    // 2. 加载 Mesh 资源
    LoadMeshResources();
    
    // 3. 加载 Sprite 资源
    LoadSpriteResources();
    
    // 4. 加载纹理覆盖（多纹理支持）
    LoadTextureOverrides();
    
    // 5. 处理异步任务完成回调（回调会将更新加入队列，不直接修改组件）
    ProcessAsyncTasks();
}

void ResourceLoadingSystem::LoadMeshResources() {
    // 获取所有 MeshRenderComponent
    auto entities = m_world->Query<MeshRenderComponent>();
    
    auto& resMgr = ResourceManager::GetInstance();
    auto& shaderCache = ShaderCache::GetInstance();
    
    // ✅ 检查ShaderCache状态（一次性警告）
    static bool shaderCacheChecked = false;
    if (!shaderCacheChecked) {
        if (shaderCache.GetShaderCount() == 0) {
            Logger::GetInstance().WarningFormat(
                "[ResourceLoadingSystem] ShaderCache is empty. "
                "Please preload shaders during initialization or provide shader paths in MeshRenderComponent.");
        } else {
            Logger::GetInstance().InfoFormat(
                "[ResourceLoadingSystem] ShaderCache has %zu shader(s) preloaded", 
                shaderCache.GetShaderCount());
        }
        shaderCacheChecked = true;
    }
    
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
            
            // ==================== 网格加载（通过 ResourceManager 统一管理）====================
            if (!meshComp.meshName.empty() && !meshComp.mesh) {
                // 先检查 ResourceManager 缓存
                if (resMgr.HasMesh(meshComp.meshName)) {
                    meshComp.mesh = resMgr.GetMesh(meshComp.meshName);
                    Logger::GetInstance().DebugFormat("[ResourceLoadingSystem] Mesh loaded from ResourceManager cache: %s", 
                                 meshComp.meshName.c_str());
                } else {
                    // 缓存中没有，异步加载
                    Logger::GetInstance().DebugFormat("[ResourceLoadingSystem] Starting async load for mesh: %s", 
                                 meshComp.meshName.c_str());
                    
                    // ✅ 使用weak_ptr捕获World的生命周期
                    EntityID entityCopy = entity;  // 捕获entity
                    std::string meshNameCopy = meshComp.meshName;  // 捕获名称
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
                            [this, entityCopy, meshNameCopy](const MeshLoadResult& result) {
                                if (!m_shuttingDown.load()) {
                                    // ✅ 修复：注册到 ResourceManager（带竞态检查）
                                    if (result.IsSuccess() && result.resource) {
                                        auto& resMgr = ResourceManager::GetInstance();
                                        
                                        if (!resMgr.HasMesh(meshNameCopy)) {
                                            if (resMgr.RegisterMesh(meshNameCopy, result.resource)) {
                                                Logger::GetInstance().DebugFormat(
                                                    "[ResourceLoadingSystem] Mesh registered (legacy): %s", 
                                                    meshNameCopy.c_str());
                                            } else {
                                                Logger::GetInstance().DebugFormat(
                                                    "[ResourceLoadingSystem] Mesh already registered (legacy): %s", 
                                                    meshNameCopy.c_str());
                                            }
                                        }
                                    }
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
                        [this, entityCopy, meshNameCopy, worldWeak](const MeshLoadResult& result) {
                            // ✅ 尝试锁定weak_ptr，检查World是否还存活
                            if (auto worldShared = worldWeak.lock()) {
                                // World仍然存活，可以安全访问
                                if (!m_shuttingDown.load()) {
                                    // ✅ 注册到 ResourceManager 供其他实体复用
                                    if (result.IsSuccess() && result.resource) {
                                        auto& resMgr = ResourceManager::GetInstance();
                                        
                                        // ✅ 修复：先检查是否已注册，避免重复注册和竞态条件
                                        if (!resMgr.HasMesh(meshNameCopy)) {
                                            if (resMgr.RegisterMesh(meshNameCopy, result.resource)) {
                                                Logger::GetInstance().DebugFormat(
                                                    "[ResourceLoadingSystem] Mesh registered to ResourceManager: %s", 
                                                    meshNameCopy.c_str());
                                            } else {
                                                // 注册失败（可能已被其他线程注册），这是正常情况
                                                Logger::GetInstance().DebugFormat(
                                                    "[ResourceLoadingSystem] Mesh already registered by another thread: %s", 
                                                    meshNameCopy.c_str());
                                            }
                                        } else {
                                            Logger::GetInstance().DebugFormat(
                                                "[ResourceLoadingSystem] Mesh already exists in ResourceManager: %s", 
                                                meshNameCopy.c_str());
                                        }
                                    }
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
            }
            
            // ==================== 材质加载（通过 ResourceManager）====================
            if (!meshComp.materialName.empty() && !meshComp.material) {
                meshComp.material = resMgr.GetMaterial(meshComp.materialName);
                
                if (!meshComp.material) {
                    Logger::GetInstance().WarningFormat("[ResourceLoadingSystem] Material not found in ResourceManager: %s", 
                                 meshComp.materialName.c_str());
                } else {
                    // ✅ 验证加载的材质是否有效
                    if (!meshComp.material->IsValid()) {
                        Logger::GetInstance().WarningFormat("[ResourceLoadingSystem] Loaded material '%s' is invalid (missing shader or shader invalid)", 
                                     meshComp.materialName.c_str());
                        meshComp.material.reset();  // 清除无效材质
                    } else {
                        Logger::GetInstance().DebugFormat("[ResourceLoadingSystem] Material loaded from ResourceManager: %s", 
                                     meshComp.materialName.c_str());
                    }
                }
            }
            
            // ==================== 着色器加载（通过 ShaderCache）====================
            if (!meshComp.shaderName.empty() && meshComp.material) {
                std::shared_ptr<Shader> shader;
                
                // 如果提供了着色器路径，尝试动态加载到ShaderCache
                if (!meshComp.shaderVertPath.empty() && !meshComp.shaderFragPath.empty()) {
                    shader = shaderCache.LoadShader(
                        meshComp.shaderName,
                        meshComp.shaderVertPath,
                        meshComp.shaderFragPath,
                        meshComp.shaderGeomPath
                    );
                    if (shader) {
                        Logger::GetInstance().DebugFormat("[ResourceLoadingSystem] Shader loaded dynamically: %s", 
                                     meshComp.shaderName.c_str());
                    } else {
                        Logger::GetInstance().WarningFormat("[ResourceLoadingSystem] Failed to load shader: %s (vert: %s, frag: %s)", 
                                     meshComp.shaderName.c_str(),
                                     meshComp.shaderVertPath.c_str(),
                                     meshComp.shaderFragPath.c_str());
                    }
                } else {
                    // 仅从缓存获取（要求着色器已预加载）
                    shader = shaderCache.GetShader(meshComp.shaderName);
                    if (shader) {
                        Logger::GetInstance().DebugFormat("[ResourceLoadingSystem] Shader found in ShaderCache: %s", 
                                     meshComp.shaderName.c_str());
                    } else {
                        Logger::GetInstance().WarningFormat("[ResourceLoadingSystem] Shader not found in ShaderCache: %s (Hint: preload shaders or provide shader paths)", 
                                     meshComp.shaderName.c_str());
                    }
                }
                
                // ✅ 应用着色器到材质（并验证）
                if (shader && shader->IsValid()) {
                    meshComp.material->SetShader(shader);
                    
                    // ✅ 验证材质在设置着色器后是否有效
                    if (!meshComp.material->IsValid()) {
                        Logger::GetInstance().WarningFormat("[ResourceLoadingSystem] Material became invalid after setting shader '%s'", 
                                     meshComp.shaderName.c_str());
                    }
                } else if (shader) {
                    Logger::GetInstance().WarningFormat("[ResourceLoadingSystem] Shader '%s' is invalid, not applying to material", 
                                 meshComp.shaderName.c_str());
                }
            }
        }
    }
}

void ResourceLoadingSystem::LoadTextureOverrides() {
    // 加载 MeshRenderComponent 中的纹理覆盖
    auto entities = m_world->Query<MeshRenderComponent>();
    auto& resMgr = ResourceManager::GetInstance();
    // ✅ 修复：统一使用 ResourceManager，不再使用 TextureLoader [[memory:7392268]]
    
    for (const auto& entity : entities) {
        auto& meshComp = m_world->GetComponent<MeshRenderComponent>(entity);
        
        // ✅ 检查材质是否有效（不仅检查指针，还要检查材质状态）
        if (!meshComp.material) {
            continue;  // 没有材质，跳过
        }
        
        if (!meshComp.material->IsValid()) {
            Logger::GetInstance().WarningFormat("[ResourceLoadingSystem] Entity %u has invalid material, cannot apply texture overrides", 
                                               entity.index);
            continue;
        }
        
        if (meshComp.textureOverrides.empty()) {
            continue;  // 没有纹理覆盖，跳过
        }
        
        // 遍历所有纹理覆盖
        for (const auto& [texName, texPath] : meshComp.textureOverrides) {
            // 检查材质是否已有该纹理
            if (meshComp.material->HasTexture(texName)) {
                continue;  // 已存在，跳过
            }
            
            // ✅ 统一纹理加载流程：只通过 ResourceManager
            auto texture = resMgr.GetTexture(texPath);
            if (texture) {
                // 从 ResourceManager 缓存获取成功
                meshComp.material->SetTexture(texName, texture);
                Logger::GetInstance().DebugFormat("[ResourceLoadingSystem] Texture '%s' loaded from ResourceManager cache for entity %u", 
                                                 texName.c_str(), entity.index);
                
                // 应用纹理设置
                if (meshComp.textureSettings.count(texName)) {
                    auto& settings = meshComp.textureSettings[texName];
                    if (settings.generateMipmaps && texture) {
                        texture->GenerateMipmap();
                    }
                }
                continue;
            }
            
            // 缓存中没有，异步加载纹理
            Logger::GetInstance().DebugFormat("[ResourceLoadingSystem] Starting async load for texture '%s': %s", 
                                             texName.c_str(), texPath.c_str());
            
            EntityID entityCopy = entity;
            std::string texNameCopy = texName;
            std::string texPathCopy = texPath;
            
            // 检查是否需要生成 mipmaps
            bool generateMipmaps = true;
            if (meshComp.textureSettings.count(texName)) {
                generateMipmaps = meshComp.textureSettings[texName].generateMipmaps;
            }
            
            // ✅ 添加 World 生命周期保护
            std::weak_ptr<World> worldWeak;
            try {
                worldWeak = m_world->weak_from_this();
            } catch (const std::bad_weak_ptr&) {
                // World 不是通过 shared_ptr 管理的，使用传统方式（但有风险）
                Logger::GetInstance().WarningFormat("[ResourceLoadingSystem] World not managed by shared_ptr for texture override, using legacy callback");
                
                m_asyncLoader->LoadTextureAsync(
                    texPath, texPath, generateMipmaps,
                    [this, entityCopy, texNameCopy, texPathCopy](const TextureLoadResult& result) {
                        if (!m_shuttingDown.load() && result.IsSuccess()) {
                            // ✅ 统一注册到 ResourceManager
                            ResourceManager::GetInstance().RegisterTexture(texPathCopy, result.resource);
                            
                            // ⚠️ 直接修改（不安全，但是 fallback）
                            if (m_world && m_world->IsValidEntity(entityCopy) && 
                                m_world->HasComponent<MeshRenderComponent>(entityCopy)) {
                                auto& meshComp = m_world->GetComponent<MeshRenderComponent>(entityCopy);
                                if (meshComp.material) {
                                    meshComp.material->SetTexture(texNameCopy, result.resource);
                                    Logger::GetInstance().InfoFormat("[ResourceLoadingSystem] ✅ Texture '%s' applied to entity %u (legacy)", 
                                                                    texNameCopy.c_str(), entityCopy.index);
                                }
                            }
                        }
                    }
                );
                continue;
            }
            
            // ✅ 使用 weak_ptr 保护的异步加载
            m_asyncLoader->LoadTextureAsync(
                texPath, texPath, generateMipmaps,
                [this, entityCopy, texNameCopy, texPathCopy, worldWeak](const TextureLoadResult& result) {
                    // ✅ 检查 World 是否还存活
                    if (auto worldShared = worldWeak.lock()) {
                        // World 仍然存活，可以安全访问
                        if (!m_shuttingDown.load() && result.IsSuccess()) {
                            // ✅ 统一注册到 ResourceManager（不再使用TextureLoader）
                            ResourceManager::GetInstance().RegisterTexture(texPathCopy, result.resource);
                            
                            // ✅ 使用队列机制，避免直接修改组件
                            PendingTextureOverrideUpdate update;
                            update.entity = entityCopy;
                            update.textureName = texNameCopy;
                            update.texture = result.resource;
                            update.success = true;
                            update.errorMessage = "";
                            
                            std::lock_guard<std::mutex> lock(m_pendingMutex);
                            m_pendingTextureOverrideUpdates.push_back(std::move(update));
                            
                            Logger::GetInstance().DebugFormat("[ResourceLoadingSystem] Texture override '%s' queued for entity %u", 
                                                             texNameCopy.c_str(), entityCopy.index);
                        } else if (!result.IsSuccess()) {
                            // 记录失败
                            PendingTextureOverrideUpdate update;
                            update.entity = entityCopy;
                            update.textureName = texNameCopy;
                            update.texture = nullptr;
                            update.success = false;
                            update.errorMessage = result.errorMessage;
                            
                            std::lock_guard<std::mutex> lock(m_pendingMutex);
                            m_pendingTextureOverrideUpdates.push_back(std::move(update));
                        }
                    } else {
                        // World 已被销毁，忽略回调
                        Logger::GetInstance().InfoFormat("[ResourceLoadingSystem] World destroyed, skip texture override callback for '%s'", 
                                                        texNameCopy.c_str());
                    }
                }
            );
        }
    }
}

void ResourceLoadingSystem::LoadSpriteResources() {
    // 获取所有 SpriteRenderComponent
    auto entities = m_world->Query<SpriteRenderComponent>();
    
    // ✅ 获取ResourceManager引用，用于缓存检查
    auto& resMgr = ResourceManager::GetInstance();
    
    for (const auto& entity : entities) {
        auto& spriteComp = m_world->GetComponent<SpriteRenderComponent>(entity);
        
        // 检查是否需要加载资源
        if (!spriteComp.resourcesLoaded && !spriteComp.asyncLoading) {
            // 标记正在加载
            spriteComp.asyncLoading = true;
            
            // 异步加载纹理
            if (!spriteComp.textureName.empty() && !spriteComp.texture) {
                // ✅ 先检查ResourceManager缓存
                if (resMgr.HasTexture(spriteComp.textureName)) {
                    spriteComp.texture = resMgr.GetTexture(spriteComp.textureName);
                    spriteComp.resourcesLoaded = true;
                    spriteComp.asyncLoading = false;
                    Logger::GetInstance().DebugFormat(
                        "[ResourceLoadingSystem] Texture loaded from ResourceManager cache: %s", 
                        spriteComp.textureName.c_str());
                    continue;  // 跳过异步加载
                }
                
                // 缓存中没有，异步加载
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
    
    // ✅ 使用可配置的值处理完成的异步任务
    // 这会在主线程中执行GPU上传
    m_asyncLoader->ProcessCompletedTasks(m_maxTasksPerFrame);
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
        m_pendingTextureOverrideUpdates.clear();  // ✅ 清空纹理覆盖更新
        return;
    }
    
    // 获取待更新的队列（交换到本地变量以减少锁持有时间）
    std::vector<PendingMeshUpdate> meshUpdates;
    std::vector<PendingTextureUpdate> textureUpdates;
    std::vector<PendingTextureOverrideUpdate> textureOverrideUpdates;  // ✅ 新增
    
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        meshUpdates.swap(m_pendingMeshUpdates);
        textureUpdates.swap(m_pendingTextureUpdates);
        textureOverrideUpdates.swap(m_pendingTextureOverrideUpdates);  // ✅ 交换纹理覆盖更新
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
    
    // ✅ 应用纹理覆盖更新
    for (const auto& update : textureOverrideUpdates) {
        // ✅ 再次检查是否正在关闭
        if (m_shuttingDown.load()) {
            Logger::GetInstance().InfoFormat("[ResourceLoadingSystem] System shutting down, abort applying texture override updates");
            break;
        }
        
        // ✅ 多重安全检查：实体是否有效
        if (!m_world->IsValidEntity(update.entity)) {
            Logger::GetInstance().WarningFormat("[ResourceLoadingSystem] Entity %u is no longer valid (texture override)", 
                                               update.entity.index);
            continue;
        }
        
        // ✅ 检查组件是否存在
        if (!m_world->HasComponent<MeshRenderComponent>(update.entity)) {
            Logger::GetInstance().WarningFormat("[ResourceLoadingSystem] Entity %u missing MeshRenderComponent (texture override)", 
                                               update.entity.index);
            continue;
        }
        
        // ✅ 安全获取组件引用
        try {
            auto& meshComp = m_world->GetComponent<MeshRenderComponent>(update.entity);
            
            // 检查材质是否存在
            if (!meshComp.material) {
                Logger::GetInstance().WarningFormat("[ResourceLoadingSystem] Entity %u has no material for texture override '%s'", 
                                                   update.entity.index, update.textureName.c_str());
                continue;
            }
            
            if (update.success && update.texture) {
                // 应用纹理覆盖到材质
                meshComp.material->SetTexture(update.textureName, update.texture);
                
                Logger::GetInstance().InfoFormat("[ResourceLoadingSystem] ✅ Texture override '%s' applied successfully to entity %u", 
                                                 update.textureName.c_str(), update.entity.index);
            } else {
                // 记录加载失败
                Logger::GetInstance().ErrorFormat("[ResourceLoadingSystem] Texture override '%s' loading failed for entity %u: %s", 
                                                  update.textureName.c_str(), update.entity.index, update.errorMessage.c_str());
            }
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat("[ResourceLoadingSystem] Exception applying texture override update: %s", e.what());
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
    
    // 延迟获取 CameraSystem（避免在 OnCreate 时获取导致死锁）
    if (!m_cameraSystem && m_world) {
        m_cameraSystem = m_world->GetSystemNoLock<CameraSystem>();
        if (m_cameraSystem) {
            Logger::GetInstance().InfoFormat("[MeshRenderSystem] CameraSystem acquired, frustum culling enabled");
        }
    }
    
    // 重置统计信息
    m_stats = RenderStats{};
    
    // 提交渲染对象
    SubmitRenderables();
}

void MeshRenderSystem::SubmitRenderables() {
    // ✅ 使用错误处理保护渲染流程
    RENDER_TRY {
        if (!m_world || !m_renderer) {
            throw RENDER_WARNING(ErrorCode::NullPointer, "MeshRenderSystem: World or Renderer is null");
        }
        
        // ✅ 检查Renderer是否已初始化
        if (!m_renderer->IsInitialized()) {
            throw RENDER_WARNING(ErrorCode::NotInitialized, "MeshRenderSystem: Renderer is not initialized");
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
        
        // ✅ 检查RenderState是否有效
        auto renderState = m_renderer->GetRenderState();
        if (!renderState) {
            throw RENDER_WARNING(ErrorCode::NullPointer, "MeshRenderSystem: RenderState is null");
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
            
            // ==================== 材质属性覆盖处理 ====================
            // ✅ 注意：MaterialOverride 不应该修改共享的 Material 对象
            // 而是在绑定材质后，通过 UniformManager 临时覆盖 uniform 值
            // 这样不会影响其他使用同一 Material 的实体
            
            // ✅ 检查材质是否有效（不仅检查指针，还要检查材质状态）
            if (meshComp.material && !meshComp.material->IsValid()) {
                Logger::GetInstance().WarningFormat("[MeshRenderSystem] Entity %u has invalid material, skipping", 
                                                   entity.index);
                continue;
            }
            
            // ==================== 实例化渲染支持 ====================
            // 注意：完整的实例化渲染需要：
            // 1. 上传实例变换矩阵到 GPU（VBO 或 UBO）
            // 2. 着色器支持实例化（layout(location=3) in mat4 instanceMatrix）
            // 3. 调用 DrawInstanced 而不是 Draw
            
            // 当前实现：检测实例化标志并记录警告
            if (meshComp.useInstancing && meshComp.instanceCount > 1) {
                static bool warnedOnce = false;
                if (!warnedOnce) {
                    Logger::GetInstance().WarningFormat("[MeshRenderSystem] Instanced rendering detected but not fully implemented. "
                                                       "Will render single instance. See docs for full implementation.");
                    warnedOnce = true;
                }
                
                // TODO: 完整实现需要：
                // 1. 创建实例变换矩阵 VBO
                // 2. 绑定到 VAO 的实例化属性
                // 3. 调用 mesh->DrawInstanced(meshComp.instanceCount)
                
                // 临时：只渲染第一个实例
                // 未来可以扩展 MeshRenderable 支持实例化
            }
            
            // 创建 MeshRenderable 并添加到对象池
            // ✅ 添加断言检查
            RENDER_ASSERT(meshComp.mesh != nullptr, "Mesh is null");
            RENDER_ASSERT(meshComp.material != nullptr, "Material is null");
            RENDER_ASSERT(transform.transform != nullptr, "Transform is null");
            
            MeshRenderable renderable;
            renderable.SetMesh(meshComp.mesh);
            renderable.SetMaterial(meshComp.material);
            renderable.SetTransform(transform.transform);
            renderable.SetVisible(meshComp.visible);
            renderable.SetLayerID(meshComp.layerID);
            renderable.SetRenderPriority(meshComp.renderPriority);
            renderable.SetCastShadows(meshComp.castShadows);
            renderable.SetReceiveShadows(meshComp.receiveShadows);
            
            // ==================== ✅ MaterialOverride 处理 ====================
            // 将ECS组件的MaterialOverride传递给MeshRenderable
            // MeshRenderable在渲染时会应用这些覆盖，而不修改共享的Material对象
            
            // 构造Renderable的MaterialOverride（需要类型匹配）
            Render::MaterialOverride renderableOverride;
            auto& ecsOverride = meshComp.materialOverride;
            
            if (ecsOverride.diffuseColor.has_value()) {
                renderableOverride.diffuseColor = ecsOverride.diffuseColor.value();
            }
            if (ecsOverride.specularColor.has_value()) {
                renderableOverride.specularColor = ecsOverride.specularColor.value();
            }
            if (ecsOverride.emissiveColor.has_value()) {
                renderableOverride.emissiveColor = ecsOverride.emissiveColor.value();
            }
            if (ecsOverride.shininess.has_value()) {
                renderableOverride.shininess = ecsOverride.shininess.value();
            }
            if (ecsOverride.metallic.has_value()) {
                renderableOverride.metallic = ecsOverride.metallic.value();
            }
            if (ecsOverride.roughness.has_value()) {
                renderableOverride.roughness = ecsOverride.roughness.value();
            }
            if (ecsOverride.opacity.has_value()) {
                renderableOverride.opacity = ecsOverride.opacity.value();
            }
            
            // 如果有任何覆盖，设置到Renderable
            if (renderableOverride.HasAnyOverride()) {
                renderable.SetMaterialOverride(renderableOverride);
            }
            
            m_renderables.push_back(std::move(renderable));
            
            m_stats.visibleMeshes++;
        }
        
        // ==================== 透明物体排序 ====================
        // 分离不透明和透明物体
        std::vector<size_t> opaqueIndices;
        std::vector<size_t> transparentIndices;
        
        // ✅ 重用上面查询的entities变量以便检查MaterialOverride
        // entities 已在函数开始处定义（第803行）
        
        for (size_t i = 0; i < m_renderables.size(); i++) {
            auto& renderable = m_renderables[i];
            auto material = renderable.GetMaterial();
            
            // ✅ 判断是否透明（需要考虑Material的BlendMode和MaterialOverride的opacity）
            bool isTransparent = false;
            if (material && material->IsValid()) {
                // 检查材质的混合模式
                auto blendMode = material->GetBlendMode();
                isTransparent = (blendMode == BlendMode::Alpha || blendMode == BlendMode::Additive);
                
                // ✅ 检查对应实体的MaterialOverride中的opacity
                // 注意：这里假设renderables和entities的顺序一致（在同一次Query中生成的）
                if (i < entities.size()) {
                    const auto& entity = entities[i];
                    const auto& meshComp = m_world->GetComponent<MeshRenderComponent>(entity);
                    
                    // 如果MaterialOverride设置了opacity < 1.0，也视为透明物体
                    if (meshComp.materialOverride.opacity.has_value() && 
                        meshComp.materialOverride.opacity.value() < 1.0f) {
                        isTransparent = true;
                    }
                }
            }
            
            if (isTransparent) {
                transparentIndices.push_back(i);
            } else {
                opaqueIndices.push_back(i);
            }
        }
        
        // 提交不透明物体（顺序无关）
        for (size_t idx : opaqueIndices) {
            m_renderer->SubmitRenderable(&m_renderables[idx]);
            m_stats.drawCalls++;
        }
        
        // 对透明物体按距离排序（从远到近）
        if (!transparentIndices.empty() && m_cameraSystem) {
            Camera* camera = m_cameraSystem->GetMainCameraObject();
            if (camera) {
                Vector3 cameraPos = camera->GetPosition();
                
                std::sort(transparentIndices.begin(), transparentIndices.end(),
                    [&](size_t a, size_t b) {
                        auto& renderableA = m_renderables[a];
                        auto& renderableB = m_renderables[b];
                        
                        // 获取物体世界位置（从变换矩阵）
                        auto transformA = renderableA.GetTransform();
                        auto transformB = renderableB.GetTransform();
                        
                        Vector3 posA = transformA ? transformA->GetPosition() : Vector3::Zero();
                        Vector3 posB = transformB ? transformB->GetPosition() : Vector3::Zero();
                        
                        // 计算到相机的距离
                        float distA = (posA - cameraPos).squaredNorm();
                        float distB = (posB - cameraPos).squaredNorm();
                        
                        // 从远到近排序
                        return distA > distB;
                    });
                
                Logger::GetInstance().DebugFormat("[MeshRenderSystem] Sorted %zu transparent objects", 
                                                 transparentIndices.size());
            }
        }
        
        // 提交透明物体（从远到近）
        for (size_t idx : transparentIndices) {
            m_renderer->SubmitRenderable(&m_renderables[idx]);
            m_stats.drawCalls++;
        }
    
        // ✅ 调试信息：显示提交的数量
        static int logCounter = 0;
        if (logCounter++ < 10 || logCounter % 60 == 0) {
            Logger::GetInstance().InfoFormat("[MeshRenderSystem] Submitted %zu renderables (total entities: %zu, culled: %zu)", 
                                             m_stats.visibleMeshes, entities.size(), m_stats.culledMeshes);
        }
    }
    RENDER_CATCH {
        // 错误已被 ErrorHandler 处理和记录
    }
}

bool MeshRenderSystem::ShouldCull(const Vector3& position, float radius) {
    // ✅ 视锥体剔除优化（带近距离保护）
    if (!m_cameraSystem) {
        return false;
    }
    
    Camera* mainCamera = m_cameraSystem->GetMainCameraObject();
    if (!mainCamera) {
        return false;
    }
    
    // ==================== 近距离保护：相机附近的物体永不剔除 ====================
    Vector3 cameraPos = mainCamera->GetPosition();
    float distanceToCamera = (position - cameraPos).norm();
    
    // 定义不剔除球体半径（相机周围这个范围内的物体永远可见）
    const float noCullRadius = 5.0f;  // 5米内的物体不剔除
    
    if (distanceToCamera < noCullRadius + radius) {
        // 物体在相机附近，不剔除
        return false;
    }
    
    // ==================== 视锥体剔除 ====================
    // 获取视锥体（这会自动触发更新）
    const Frustum& frustum = mainCamera->GetFrustum();
    
    // 扩大包围球半径以避免过度剔除（考虑Miku模型的特殊性）
    float expandedRadius = radius * 1.5f;  // 增加50%的安全边距
    
    bool culled = !frustum.IntersectsSphere(position, expandedRadius);
    
    // 调试：前10次剔除时输出信息
    static int cullDebugCount = 0;
    if (culled && cullDebugCount < 10) {
        Logger::GetInstance().DebugFormat(
            "[MeshRenderSystem] Culled object at (%.1f, %.1f, %.1f) with radius %.2f, distance %.1f", 
            position.x(), position.y(), position.z(), expandedRadius, distanceToCamera);
        cullDebugCount++;
    }
    
    return culled;
}

// ============================================================
// SpriteRenderSystem 实现
// ============================================================

SpriteRenderSystem::SpriteRenderSystem(Renderer* renderer)
    : m_renderer(renderer) {
    // ✅ 空指针检查
    if (!m_renderer) {
        Logger::GetInstance().ErrorFormat("[SpriteRenderSystem] Renderer is null");
    }
    // 注意：当前实现暂未使用Renderer，保留用于未来2D渲染扩展
}

void SpriteRenderSystem::Update(float deltaTime) {
    (void)deltaTime;  // 未使用
    
    // ✅ 安全检查
    if (!m_world) {
        return;
    }
    
    // ✅ 检查Renderer是否有效（当前未使用，但为未来实现保留检查）
    if (!m_renderer || !m_renderer->IsInitialized()) {
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
        // 未来需要使用 m_renderer->SubmitRenderable() 提交SpriteRenderable
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
    
    // ==================== 验证并重置主相机（如果当前主相机无效）====================
    bool needsNewMainCamera = false;
    
    if (m_mainCamera.IsValid()) {
        // 验证主相机是否仍然有效
        if (!ValidateMainCamera()) {
            Logger::GetInstance().DebugFormat(
                "[CameraSystem] Main camera (entity %u) is invalid, will select new one", 
                m_mainCamera.index
            );
            m_mainCamera = EntityID::Invalid();
            needsNewMainCamera = true;
        }
    } else {
        needsNewMainCamera = true;
    }
    
    // ==================== 更新所有相机 ====================
    EntityID bestCameraCandidate = EntityID::Invalid();
    int32_t bestCameraDepth = std::numeric_limits<int32_t>::max();
    
    for (const auto& entity : entities) {
        auto& transform = m_world->GetComponent<TransformComponent>(entity);
        auto& cameraComp = m_world->GetComponent<CameraComponent>(entity);
        
        // 跳过无效的相机
        if (!cameraComp.IsValid()) {
            continue;
        }
        
        // 同步Transform到Camera
        Vector3 pos = transform.GetPosition();
        Quaternion rot = transform.GetRotation();
        
        cameraComp.camera->SetPosition(pos);
        cameraComp.camera->SetRotation(rot);
        
        // 如果需要新的主相机，选择depth最小的
        if (needsNewMainCamera) {
            if (!bestCameraCandidate.IsValid() || cameraComp.depth < bestCameraDepth) {
                bestCameraCandidate = entity;
                bestCameraDepth = cameraComp.depth;
            }
        }
    }
    
    // ==================== 设置新的主相机 ====================
    if (needsNewMainCamera && bestCameraCandidate.IsValid()) {
        m_mainCamera = bestCameraCandidate;
        Logger::GetInstance().DebugFormat(
            "[CameraSystem] Selected new main camera: entity %u (depth: %d)", 
            m_mainCamera.index, bestCameraDepth
        );
    }
}

bool CameraSystem::ValidateMainCamera() const {
    if (!m_world || !m_mainCamera.IsValid()) {
        return false;
    }
    
    // 检查实体是否仍然存在
    if (!m_world->IsValidEntity(m_mainCamera)) {
        return false;
    }
    
    // 检查是否有CameraComponent
    if (!m_world->HasComponent<CameraComponent>(m_mainCamera)) {
        return false;
    }
    
    // 检查CameraComponent是否有效
    const auto& cameraComp = m_world->GetComponent<CameraComponent>(m_mainCamera);
    if (!cameraComp.IsValid()) {
        return false;
    }
    
    return true;
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

Ref<Camera> CameraSystem::GetMainCameraSharedPtr() const {
    if (!m_world || !m_mainCamera.IsValid()) {
        return nullptr;
    }
    
    if (!m_world->HasComponent<CameraComponent>(m_mainCamera)) {
        return nullptr;
    }
    
    const auto& cameraComp = m_world->GetComponent<CameraComponent>(m_mainCamera);
    return cameraComp.camera;
}

bool CameraSystem::SetMainCamera(EntityID entity) {
    if (!m_world) {
        Logger::GetInstance().Warning("[CameraSystem] SetMainCamera: World is null");
        return false;
    }
    
    if (!entity.IsValid()) {
        Logger::GetInstance().Warning("[CameraSystem] SetMainCamera: Invalid entity ID");
        return false;
    }
    
    // 验证实体存在
    if (!m_world->IsValidEntity(entity)) {
        Logger::GetInstance().WarningFormat(
            "[CameraSystem] SetMainCamera: Entity %u does not exist", 
            entity.index
        );
        return false;
    }
    
    // 验证有CameraComponent
    if (!m_world->HasComponent<CameraComponent>(entity)) {
        Logger::GetInstance().WarningFormat(
            "[CameraSystem] SetMainCamera: Entity %u has no CameraComponent", 
            entity.index
        );
        return false;
    }
    
    // 验证CameraComponent有效
    const auto& cameraComp = m_world->GetComponent<CameraComponent>(entity);
    if (!cameraComp.IsValid()) {
        Logger::GetInstance().WarningFormat(
            "[CameraSystem] SetMainCamera: Entity %u has invalid CameraComponent", 
            entity.index
        );
        return false;
    }
    
    // 设置主相机
    m_mainCamera = entity;
    Logger::GetInstance().InfoFormat(
        "[CameraSystem] Main camera set to entity %u", entity.index
    );
    return true;
}

void CameraSystem::ClearMainCamera() {
    m_mainCamera = EntityID::Invalid();
    Logger::GetInstance().Debug("[CameraSystem] Main camera cleared");
}

EntityID CameraSystem::SelectMainCameraByDepth() {
    if (!m_world) {
        return EntityID::Invalid();
    }
    
    auto entities = m_world->Query<CameraComponent>();
    
    EntityID bestCamera = EntityID::Invalid();
    int32_t bestDepth = std::numeric_limits<int32_t>::max();
    
    for (const auto& entity : entities) {
        const auto& cameraComp = m_world->GetComponent<CameraComponent>(entity);
        
        if (!cameraComp.IsValid()) {
            continue;
        }
        
        if (!bestCamera.IsValid() || cameraComp.depth < bestDepth) {
            bestCamera = entity;
            bestDepth = cameraComp.depth;
        }
    }
    
    if (bestCamera.IsValid()) {
        m_mainCamera = bestCamera;
        Logger::GetInstance().DebugFormat(
            "[CameraSystem] Selected main camera: entity %u (depth: %d)", 
            bestCamera.index, bestDepth
        );
    }
    
    return bestCamera;
}

// ============================================================
// LightSystem 实现
// ============================================================

LightSystem::LightSystem(Renderer* renderer)
    : m_renderer(renderer) {
    // ✅ 空指针检查
    if (!m_renderer) {
        Logger::GetInstance().ErrorFormat("[LightSystem] Renderer is null");
    }
    // 注意：当前实现中LightSystem只收集光源数据，不直接使用Renderer
    // 光源数据通过UniformSystem设置到着色器
    // 保留Renderer参数用于未来可能的阴影渲染等高级功能
}

void LightSystem::Update(float deltaTime) {
    (void)deltaTime;  // 未使用
    
    // ✅ 安全检查（虽然当前不直接使用Renderer，但为未来扩展保留）
    if (!m_renderer || !m_renderer->IsInitialized()) {
        return;
    }
    
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
    
    // ✅ 这些光源数据会由 UniformSystem 自动设置到所有着色器的 uniform
    // UniformSystem::SetLightUniforms() 会读取这些缓存的数据并设置到 shader uniform
}

// ============================================================
// UniformSystem 实现
// ============================================================

UniformSystem::UniformSystem(Renderer* renderer)
    : m_renderer(renderer) {
    // ✅ 空指针检查
    if (!m_renderer) {
        Logger::GetInstance().ErrorFormat("[UniformSystem] Renderer is null");
    }
    // 注意：Renderer主要用于安全检查和访问RenderState
    // UniformSystem通过材质的Shader设置uniform，不直接使用Renderer渲染
}

void UniformSystem::OnCreate(World* world) {
    System::OnCreate(world);
    
    // 不在这里获取其他系统，避免在 World::RegisterSystem 期间的锁问题
    // 将在 Update 中按需获取（World::PostInitialize 后）
    Logger::GetInstance().InfoFormat("[UniformSystem] UniformSystem created");
}

void UniformSystem::OnDestroy() {
    m_cameraSystem = nullptr;
    m_lightSystem = nullptr;
    Logger::GetInstance().InfoFormat("[UniformSystem] UniformSystem destroyed");
    System::OnDestroy();
}

void UniformSystem::Update(float deltaTime) {
    // ✅ 安全检查：系统是否启用、World和Renderer是否有效
    if (!m_enabled || !m_world || !m_renderer) {
        return;
    }
    
    // ✅ 检查Renderer是否已初始化
    if (!m_renderer->IsInitialized()) {
        return;
    }
    
    // 累计时间
    m_time += deltaTime;
    
    // 延迟获取系统（避免在 OnCreate 时获取）
    if (!m_cameraSystem) {
        m_cameraSystem = m_world->GetSystemNoLock<CameraSystem>();
    }
    
    if (!m_lightSystem) {
        m_lightSystem = m_world->GetSystemNoLock<LightSystem>();
    }
    
    // 设置相机 uniform
    SetCameraUniforms();
    
    // 设置光源 uniform
    SetLightUniforms();
    
    // 设置时间 uniform
    SetTimeUniforms();
}

void UniformSystem::SetCameraUniforms() {
    if (!m_cameraSystem) {
        return;
    }
    
    Camera* camera = m_cameraSystem->GetMainCameraObject();
    if (!camera) {
        return;
    }
    
    // 获取相机矩阵
    Matrix4 viewMatrix = camera->GetViewMatrix();
    Matrix4 projectionMatrix = camera->GetProjectionMatrix();
    Vector3 cameraPos = camera->GetPosition();
    
    // 遍历所有 MeshRenderComponent，为每个材质的着色器设置 uniform
    auto entities = m_world->Query<MeshRenderComponent>();
    std::unordered_set<Shader*> processedShaders;  // 避免重复设置同一着色器
    
    for (const auto& entity : entities) {
        const auto& meshComp = m_world->GetComponent<MeshRenderComponent>(entity);
        
        // ✅ 检查材质是否有效（完整检查）
        if (!meshComp.material) {
            continue;
        }
        
        if (!meshComp.material->IsValid()) {
            continue;  // 无效材质，跳过
        }
        
        auto shader = meshComp.material->GetShader();
        if (!shader) {
            continue;  // 没有着色器，跳过
        }
        
        Shader* shaderPtr = shader.get();
        
        // 如果已经处理过这个着色器，跳过
        if (processedShaders.count(shaderPtr)) {
            continue;
        }
        
        // ✅ 检查着色器是否有效
        if (!shader->IsValid()) {
            Logger::GetInstance().WarningFormat("[UniformSystem] Shader is invalid, skipping camera uniforms");
            continue;
        }
        
        try {
            shader->Use();
            auto uniformMgr = shader->GetUniformManager();
            
            // ✅ 检查 uniformMgr 是否有效
            if (!uniformMgr) {
                Logger::GetInstance().WarningFormat("[UniformSystem] Shader '%s' has null UniformManager", 
                                                   shader->GetName().c_str());
                continue;
            }
            
            // 设置相机矩阵（统一使用u前缀）
            if (uniformMgr->HasUniform("uView")) {
                uniformMgr->SetMatrix4("uView", viewMatrix);
            }
            
            if (uniformMgr->HasUniform("uProjection")) {
                uniformMgr->SetMatrix4("uProjection", projectionMatrix);
            }
            
            if (uniformMgr->HasUniform("uViewPos")) {
                uniformMgr->SetVector3("uViewPos", cameraPos);
            }
            
            // 标记已处理
            processedShaders.insert(shaderPtr);
            
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat("[UniformSystem] Exception setting camera uniforms: %s", e.what());
        }
    }
    
    static bool firstLog = true;
    if (firstLog && !processedShaders.empty()) {
        Logger::GetInstance().InfoFormat("[UniformSystem] Set camera uniforms for %zu shaders", 
                                        processedShaders.size());
        firstLog = false;
    }
}

void UniformSystem::SetLightUniforms() {
    if (!m_lightSystem) {
        return;
    }
    
    // 获取主光源数据
    Vector3 lightPos = m_lightSystem->GetPrimaryLightPosition();
    Color lightColor = m_lightSystem->GetPrimaryLightColor();
    float lightIntensity = m_lightSystem->GetPrimaryLightIntensity();
    
    // 遍历所有材质，设置光源 uniform
    auto entities = m_world->Query<MeshRenderComponent>();
    std::unordered_set<Shader*> processedShaders;
    
    for (const auto& entity : entities) {
        const auto& meshComp = m_world->GetComponent<MeshRenderComponent>(entity);
        
        // ✅ 检查材质是否有效（完整检查）
        if (!meshComp.material) {
            continue;
        }
        
        if (!meshComp.material->IsValid()) {
            continue;  // 无效材质，跳过
        }
        
        auto shader = meshComp.material->GetShader();
        if (!shader) {
            continue;  // 没有着色器，跳过
        }
        
        Shader* shaderPtr = shader.get();
        
        if (processedShaders.count(shaderPtr)) {
            continue;
        }
        
        // ✅ 检查着色器是否有效
        if (!shader->IsValid()) {
            Logger::GetInstance().WarningFormat("[UniformSystem] Shader is invalid, skipping light uniforms");
            continue;
        }
        
        try {
            shader->Use();
            auto uniformMgr = shader->GetUniformManager();
            
            // ✅ 检查 uniformMgr 是否有效
            if (!uniformMgr) {
                Logger::GetInstance().WarningFormat("[UniformSystem] Shader '%s' has null UniformManager", 
                                                   shader->GetName().c_str());
                continue;
            }
            
            // 设置光源数据
            if (uniformMgr->HasUniform("uLightPos")) {
                uniformMgr->SetVector3("uLightPos", lightPos);
            }
            
            if (uniformMgr->HasUniform("uLightColor")) {
                uniformMgr->SetColor("uLightColor", lightColor);
            }
            
            if (uniformMgr->HasUniform("uLightIntensity")) {
                uniformMgr->SetFloat("uLightIntensity", lightIntensity);
            }
            
            // 环境光颜色（通常是光源颜色的较暗版本）
            if (uniformMgr->HasUniform("uAmbientColor")) {
                Color ambientColor = lightColor;
                ambientColor.r *= 0.2f;
                ambientColor.g *= 0.2f;
                ambientColor.b *= 0.2f;
                uniformMgr->SetColor("uAmbientColor", ambientColor);
            }
            
            processedShaders.insert(shaderPtr);
            
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat("[UniformSystem] Exception setting light uniforms: %s", e.what());
        }
    }
}

void UniformSystem::SetTimeUniforms() {
    // 为支持时间动画的着色器设置时间 uniform
    auto entities = m_world->Query<MeshRenderComponent>();
    std::unordered_set<Shader*> processedShaders;
    
    for (const auto& entity : entities) {
        const auto& meshComp = m_world->GetComponent<MeshRenderComponent>(entity);
        
        // ✅ 检查材质是否有效（完整检查）
        if (!meshComp.material) {
            continue;
        }
        
        if (!meshComp.material->IsValid()) {
            continue;  // 无效材质，跳过
        }
        
        auto shader = meshComp.material->GetShader();
        if (!shader) {
            continue;  // 没有着色器，跳过
        }
        
        Shader* shaderPtr = shader.get();
        
        if (processedShaders.count(shaderPtr)) {
            continue;
        }
        
        // ✅ 检查着色器是否有效
        if (!shader->IsValid()) {
            Logger::GetInstance().WarningFormat("[UniformSystem] Shader is invalid, skipping time uniforms");
            continue;
        }
        
        try {
            shader->Use();
            auto uniformMgr = shader->GetUniformManager();
            
            // ✅ 检查 uniformMgr 是否有效
            if (!uniformMgr) {
                Logger::GetInstance().WarningFormat("[UniformSystem] Shader '%s' has null UniformManager", 
                                                   shader->GetName().c_str());
                continue;
            }
            
            // 设置时间（如果着色器需要）
            if (uniformMgr->HasUniform("uTime")) {
                uniformMgr->SetFloat("uTime", m_time);
            }
            
            processedShaders.insert(shaderPtr);
            
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat("[UniformSystem] Exception setting time uniforms: %s", e.what());
        }
    }
}

// ============================================================
// WindowSystem 实现
// ============================================================

WindowSystem::WindowSystem(Renderer* renderer)
    : m_renderer(renderer) {
    if (!m_renderer) {
        Logger::GetInstance().ErrorFormat("[WindowSystem] Renderer is null");
    }
}

void WindowSystem::OnCreate(World* world) {
    System::OnCreate(world);
    
    // ✅ 检查前置条件
    if (!m_renderer) {
        Logger::GetInstance().ErrorFormat("[WindowSystem] Renderer is null. "
                                         "Make sure to initialize Renderer before creating WindowSystem.");
        return;
    }
    
    if (!m_renderer->IsInitialized()) {
        Logger::GetInstance().ErrorFormat("[WindowSystem] Renderer is not initialized. "
                                         "Make sure to call Renderer::Initialize() before registering WindowSystem.");
        return;
    }
    
    auto context = m_renderer->GetContext();
    if (!context) {
        Logger::GetInstance().ErrorFormat("[WindowSystem] OpenGLContext is null.");
        return;
    }
    
    if (!context->IsInitialized()) {
        Logger::GetInstance().ErrorFormat("[WindowSystem] OpenGLContext is not initialized.");
        return;
    }
    
    // ✅ 使用OpenGLContext的窗口大小变化回调机制（事件驱动）
    context->AddResizeCallback([this](int width, int height) {
        this->OnWindowResized(width, height);
    });
    
    int width = context->GetWidth();
    int height = context->GetHeight();
    Logger::GetInstance().InfoFormat("[WindowSystem] WindowSystem created (initial size: %dx%d, using resize callbacks)", 
                                    width, height);
}

void WindowSystem::OnDestroy() {
    m_cameraSystem = nullptr;
    
    // 注意：OpenGLContext的回调会随着Context的销毁自动清理
    // 如果需要更精确的控制，可以在这里调用 context->ClearResizeCallbacks()
    // 但这会清除所有回调，包括其他系统注册的回调
    // 更好的方式是在OpenGLContext中实现回调ID和单独移除机制
    
    Logger::GetInstance().InfoFormat("[WindowSystem] WindowSystem destroyed");
    System::OnDestroy();
}

void WindowSystem::Update(float deltaTime) {
    (void)deltaTime;  // 未使用
    
    // ✅ 删除轮询检测代码
    // 窗口大小变化由回调机制处理，Update不再需要检测
    
    // 延迟获取 CameraSystem（仅在首次）
    if (!m_cameraSystem && m_world) {
        m_cameraSystem = m_world->GetSystemNoLock<CameraSystem>();
    }
}

// ✅ 新增：窗口大小变化回调处理
void WindowSystem::OnWindowResized(int width, int height) {
    // GL_THREAD_CHECK();  // ✅ 确保在OpenGL线程（需要在编译选项中启用）
    
    if (!m_world || !m_renderer) {
        return;
    }
    
    Logger::GetInstance().InfoFormat("[WindowSystem] Window resized to %dx%d", width, height);
    
    // ==================== 更新相机宽高比 ====================
    if (!m_cameraSystem) {
        m_cameraSystem = m_world->GetSystemNoLock<CameraSystem>();
    }
    
    if (m_cameraSystem) {
        if (height == 0) {
            return;  // 避免除零
        }
        
        float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
        
        // 遍历所有相机组件，更新宽高比
        auto cameras = m_world->Query<CameraComponent>();
        
        for (const auto& entity : cameras) {
            auto& cameraComp = m_world->GetComponent<CameraComponent>(entity);
            
            if (!cameraComp.camera) {
                continue;
            }
            
            // 更新宽高比
            cameraComp.camera->SetAspectRatio(aspectRatio);
        }
        
        if (!cameras.empty()) {
            Logger::GetInstance().DebugFormat("[WindowSystem] Updated %zu camera(s) aspect ratio to %.3f", 
                                             cameras.size(), aspectRatio);
        }
    }
    
    // ==================== 更新视口 ====================
    if (!m_renderer->IsInitialized()) {
        return;
    }
    
    auto renderState = m_renderer->GetRenderState();
    if (renderState) {
        renderState->SetViewport(0, 0, width, height);
        Logger::GetInstance().DebugFormat("[WindowSystem] Viewport updated to %dx%d", width, height);
    } else {
        Logger::GetInstance().WarningFormat("[WindowSystem] RenderState is null, cannot update viewport");
    }
}

// ============================================================
// GeometrySystem 实现
// ============================================================

void GeometrySystem::Update(float deltaTime) {
    (void)deltaTime;  // 未使用
    
    if (!m_world) {
        return;
    }
    
    // 生成几何形状
    GenerateGeometry();
}

void GeometrySystem::GenerateGeometry() {
    // 查询所有具有 GeometryComponent 和 MeshRenderComponent 的实体
    auto entities = m_world->Query<GeometryComponent, MeshRenderComponent>();
    
    for (const auto& entity : entities) {
        auto& geomComp = m_world->GetComponent<GeometryComponent>(entity);
        auto& meshComp = m_world->GetComponent<MeshRenderComponent>(entity);
        
        // 如果已经生成过，跳过
        if (geomComp.generated) {
            continue;
        }
        
        // 生成网格
        try {
            std::shared_ptr<Mesh> mesh = CreateGeometryMesh(geomComp);
            
            if (mesh) {
                meshComp.mesh = mesh;
                meshComp.resourcesLoaded = true;
                geomComp.generated = true;
                
                Logger::GetInstance().InfoFormat("[GeometrySystem] Generated %s mesh for entity %u", 
                                                [&]() {
                                                    switch (geomComp.type) {
                                                        case GeometryType::Cube: return "Cube";
                                                        case GeometryType::Sphere: return "Sphere";
                                                        case GeometryType::Cylinder: return "Cylinder";
                                                        case GeometryType::Cone: return "Cone";
                                                        case GeometryType::Plane: return "Plane";
                                                        case GeometryType::Quad: return "Quad";
                                                        case GeometryType::Torus: return "Torus";
                                                        case GeometryType::Capsule: return "Capsule";
                                                        case GeometryType::Triangle: return "Triangle";
                                                        case GeometryType::Circle: return "Circle";
                                                        default: return "Unknown";
                                                    }
                                                }(), 
                                                entity.index);
            }
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat("[GeometrySystem] Failed to generate mesh for entity %u: %s", 
                                             entity.index, e.what());
        }
    }
}

std::shared_ptr<Mesh> GeometrySystem::CreateGeometryMesh(const GeometryComponent& geom) {
    // 使用 MeshLoader 创建几何形状
    switch (geom.type) {
        case GeometryType::Cube:
            return MeshLoader::CreateCube(geom.size, geom.size, geom.size);
            
        case GeometryType::Sphere:
            return MeshLoader::CreateSphere(geom.size, static_cast<uint32_t>(geom.segments), static_cast<uint32_t>(geom.rings));
            
        case GeometryType::Cylinder:
            // CreateCylinder(radiusTop, radiusBottom, height, segments)
            return MeshLoader::CreateCylinder(geom.size, geom.size, geom.height, static_cast<uint32_t>(geom.segments));
            
        case GeometryType::Cone:
            return MeshLoader::CreateCone(geom.size, geom.height, static_cast<uint32_t>(geom.segments));
            
        case GeometryType::Plane:
            return MeshLoader::CreatePlane(geom.size, geom.size, static_cast<uint32_t>(geom.segments), static_cast<uint32_t>(geom.segments));
            
        case GeometryType::Quad:
            return MeshLoader::CreateQuad(geom.size, geom.size);
            
        case GeometryType::Torus:
            return MeshLoader::CreateTorus(geom.outerRadius, geom.innerRadius, static_cast<uint32_t>(geom.segments), static_cast<uint32_t>(geom.rings));
            
        case GeometryType::Capsule:
            return MeshLoader::CreateCapsule(geom.radius, geom.cylinderHeight, static_cast<uint32_t>(geom.segments), static_cast<uint32_t>(geom.rings));
            
        case GeometryType::Triangle:
            return MeshLoader::CreateTriangle(geom.size);
            
        case GeometryType::Circle:
            return MeshLoader::CreateCircle(geom.size, static_cast<uint32_t>(geom.segments));
            
        default:
            Logger::GetInstance().WarningFormat("[GeometrySystem] Unknown geometry type");
            return nullptr;
    }
}

// ============================================================
// ResourceCleanupSystem 实现
// ============================================================

ResourceCleanupSystem::ResourceCleanupSystem(float cleanupIntervalSeconds, uint32_t unusedFrameThreshold)
    : m_cleanupInterval(cleanupIntervalSeconds)
    , m_unusedFrameThreshold(unusedFrameThreshold) {
    Logger::GetInstance().InfoFormat("[ResourceCleanupSystem] Created with interval=%.1fs, threshold=%u frames", 
                                    cleanupIntervalSeconds, unusedFrameThreshold);
}

void ResourceCleanupSystem::Update(float deltaTime) {
    if (!m_enabled) {
        return;
    }
    
    // 累计时间
    m_timer += deltaTime;
    
    // 检查是否到达清理间隔
    if (m_timer >= m_cleanupInterval) {
        ForceCleanup();
        m_timer = 0.0f;
    }
}

void ResourceCleanupSystem::ForceCleanup() {
    auto& resMgr = ResourceManager::GetInstance();
    
    // 记录清理前的资源数量（通过 GetStats）
    auto statsBefore = resMgr.GetStats();
    size_t totalBefore = statsBefore.totalCount;
    
    Logger::GetInstance().InfoFormat("[ResourceCleanupSystem] Starting cleanup (current: %zu mesh, %zu texture, %zu material, %zu shader)",
                                    statsBefore.meshCount, statsBefore.textureCount, 
                                    statsBefore.materialCount, statsBefore.shaderCount);
    
    // 清理未使用的资源
    size_t cleaned = resMgr.CleanupUnused(m_unusedFrameThreshold);
    
    // 记录清理后的资源数量
    auto statsAfter = resMgr.GetStats();
    
    // 更新统计信息
    m_lastStats.meshCleaned = statsBefore.meshCount - statsAfter.meshCount;
    m_lastStats.textureCleaned = statsBefore.textureCount - statsAfter.textureCount;
    m_lastStats.materialCleaned = statsBefore.materialCount - statsAfter.materialCount;
    m_lastStats.shaderCleaned = statsBefore.shaderCount - statsAfter.shaderCount;
    m_lastStats.totalCleaned = cleaned;
    
    if (cleaned > 0) {
        Logger::GetInstance().InfoFormat("[ResourceCleanupSystem] ✅ Cleaned %zu unused resources (Mesh: -%zu, Texture: -%zu, Material: -%zu, Shader: -%zu)",
                                        cleaned,
                                        m_lastStats.meshCleaned,
                                        m_lastStats.textureCleaned,
                                        m_lastStats.materialCleaned,
                                        m_lastStats.shaderCleaned);
        
        // 打印详细统计
        resMgr.PrintStatistics();
    } else {
        Logger::GetInstance().DebugFormat("[ResourceCleanupSystem] No unused resources to clean");
    }
}

} // namespace ECS
} // namespace Render


