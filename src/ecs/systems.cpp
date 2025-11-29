#include "render/ecs/systems.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/renderable.h"
#include "render/logger.h"
#include "render/resource_manager.h"
#include "render/shader_cache.h"
#include "render/material_sort_key.h"
#include "render/lighting/light_manager.h"
#include "render/render_layer.h"
// ✅ 修复：移除 TextureLoader 头文件，统一使用 ResourceManager [[memory:7392268]]
#include "render/error.h"
#include "render/mesh_loader.h"
#include "render/math_utils.h"
#include "render/sprite/sprite_layer.h"
#include "render/sprite/sprite_nineslice.h"
#include "render/ecs/sprite_animation_script_registry.h"
#include "render/debug/sprite_animation_debugger.h"
#include "render/lod_system.h"  // LOD 系统支持
#include "render/lod_instanced_renderer.h"  // LOD 实例化渲染器（阶段2.2）

#include <utility>
#include <algorithm>
#include <cmath>
#include <limits>
#include <functional>
#include <unordered_set>

namespace Render {
namespace ECS {

namespace {

uint32_t HashCombine(uint32_t seed, uint32_t value) {
    seed ^= value + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    return seed;
}

uint32_t HashFloat(float value) {
    return static_cast<uint32_t>(std::hash<float>{}(value));
}

uint32_t HashColor(const Color& color) {
    uint32_t seed = 0;
    seed = HashCombine(seed, HashFloat(color.r));
    seed = HashCombine(seed, HashFloat(color.g));
    seed = HashCombine(seed, HashFloat(color.b));
    seed = HashCombine(seed, HashFloat(color.a));
    return seed;
}

uint32_t ComputeMaterialOverrideHash(const Render::MaterialOverride& overrideData) {
    if (!overrideData.HasAnyOverride()) {
        return 0;
    }

    uint32_t seed = 0;
    if (overrideData.diffuseColor) {
        seed = HashCombine(seed, HashColor(*overrideData.diffuseColor));
    }
    if (overrideData.specularColor) {
        seed = HashCombine(seed, HashColor(*overrideData.specularColor));
    }
    if (overrideData.emissiveColor.has_value()) {
        seed = HashCombine(seed, HashColor(*overrideData.emissiveColor));
    }
    if (overrideData.shininess.has_value()) {
        seed = HashCombine(seed, HashFloat(*overrideData.shininess));
    }
    if (overrideData.metallic.has_value()) {
        seed = HashCombine(seed, HashFloat(*overrideData.metallic));
    }
    if (overrideData.roughness.has_value()) {
        seed = HashCombine(seed, HashFloat(*overrideData.roughness));
    }
    if (overrideData.opacity.has_value()) {
        seed = HashCombine(seed, HashFloat(*overrideData.opacity));
    }

    return seed == 0 ? 1u : seed;
}

const SpriteAnimationState* FindAnimationState(const SpriteAnimationComponent& component, const std::string& stateName) {
    auto it = component.states.find(stateName);
    if (it == component.states.end()) {
        return nullptr;
    }
    return &it->second;
}

bool InvokeAnimationScripts(const std::vector<std::string>& scripts,
                            EntityID entity,
                            const SpriteAnimationEvent& eventData,
                            SpriteAnimationComponent& component) {
    bool anyInvoked = false;
    for (const auto& scriptName : scripts) {
        if (scriptName.empty()) {
            continue;
        }
        if (SpriteAnimationScriptRegistry::Invoke(scriptName, entity, eventData, component)) {
            anyInvoked = true;
        }
    }
    return anyInvoked;
}

bool ApplyAnimationState(EntityID entity,
                         SpriteAnimationComponent& animComp,
                         SpriteRenderComponent& spriteComp,
                         const std::string& stateName) {
    const SpriteAnimationState* nextState = FindAnimationState(animComp, stateName);
    if (!nextState) {
        Logger::GetInstance().WarningFormat("[SpriteAnimationSystem] State '%s' not found", stateName.c_str());
        return false;
    }

    if (!animComp.HasClip(nextState->clip)) {
        Logger::GetInstance().WarningFormat("[SpriteAnimationSystem] Clip '%s' not found for state '%s'",
                                            nextState->clip.c_str(), stateName.c_str());
        return false;
    }

    std::string previousClip = animComp.currentClip;
    int previousFrame = animComp.currentFrame;
    const SpriteAnimationState* prevState = FindAnimationState(animComp, animComp.currentState);

    if (prevState && !prevState->onExitScripts.empty()) {
        SpriteAnimationEvent exitEvent{
            SpriteAnimationEvent::Type::ClipCompleted,
            previousClip,
            previousFrame
        };
        InvokeAnimationScripts(prevState->onExitScripts, entity, exitEvent, animComp);
    }

    animComp.currentState = nextState->name;
    animComp.stateTime = 0.0f;
    animComp.playbackSpeed = nextState->playbackSpeed;

    auto& clip = animComp.clips[nextState->clip];
    if (nextState->playbackMode.has_value()) {
        clip.playbackMode = *nextState->playbackMode;
        clip.loop = (clip.playbackMode == SpritePlaybackMode::Loop);
    }

    animComp.Play(nextState->clip, nextState->resetOnEnter);
    animComp.SetPlaybackSpeed(nextState->playbackSpeed);

    if (!clip.frames.empty()) {
        spriteComp.sourceRect = clip.frames[animComp.currentFrame];
    }

    if (!nextState->onEnterScripts.empty()) {
        SpriteAnimationEvent enterEvent{
            SpriteAnimationEvent::Type::ClipStarted,
            nextState->clip,
            animComp.currentFrame
        };
        InvokeAnimationScripts(nextState->onEnterScripts, entity, enterEvent, animComp);
    }

    return true;
}

bool MatchesEventCondition(const SpriteAnimationTransitionCondition& condition,
                           const std::vector<SpriteAnimationEvent>& events) {
    for (const auto& evt : events) {
        if (evt.type != condition.eventType) {
            continue;
        }
        if (!condition.eventClip.empty() && evt.clip != condition.eventClip) {
            continue;
        }
        if (condition.eventFrame >= 0 && evt.frameIndex != condition.eventFrame) {
            continue;
        }
        return true;
    }
    return false;
}

bool EvaluateTransitionCondition(const SpriteAnimationTransitionCondition& condition,
                                 const SpriteAnimationComponent& animComp,
                                 const std::vector<SpriteAnimationEvent>& events) {
    switch (condition.type) {
    case SpriteAnimationTransitionCondition::Type::Always:
        return true;
    case SpriteAnimationTransitionCondition::Type::StateTimeGreater:
        return animComp.stateTime >= condition.threshold;
    case SpriteAnimationTransitionCondition::Type::Trigger:
        return animComp.triggers.find(condition.parameter) != animComp.triggers.end();
    case SpriteAnimationTransitionCondition::Type::BoolEquals:
        return animComp.GetBoolParameter(condition.parameter, false) == condition.boolValue;
    case SpriteAnimationTransitionCondition::Type::FloatGreater:
        return animComp.GetFloatParameter(condition.parameter, 0.0f) > condition.threshold;
    case SpriteAnimationTransitionCondition::Type::FloatLess:
        return animComp.GetFloatParameter(condition.parameter, 0.0f) < condition.threshold;
    case SpriteAnimationTransitionCondition::Type::OnEvent:
        return MatchesEventCondition(condition, events);
    default:
        return false;
    }
}

bool ProcessAnimationTransitions(EntityID entity,
                                 SpriteAnimationComponent& animComp,
                                 SpriteRenderComponent& spriteComp,
                                 const std::vector<SpriteAnimationEvent>& events) {
    if (animComp.transitions.empty()) {
        return false;
    }

    for (auto& transition : animComp.transitions) {
        if (transition.once && transition.consumed) {
            continue;
        }
        if (!transition.fromState.empty() && transition.fromState != animComp.currentState) {
            continue;
        }
        if (transition.toState.empty() || transition.toState == animComp.currentState) {
            continue;
        }

        bool satisfied = true;
        std::vector<std::string> triggersToConsume;
        for (const auto& condition : transition.conditions) {
            if (!EvaluateTransitionCondition(condition, animComp, events)) {
                satisfied = false;
                break;
            }
            if (condition.type == SpriteAnimationTransitionCondition::Type::Trigger) {
                triggersToConsume.push_back(condition.parameter);
            }
        }

        if (!satisfied) {
            continue;
        }

        if (ApplyAnimationState(entity, animComp, spriteComp, transition.toState)) {
            for (const auto& trig : triggersToConsume) {
                animComp.ResetTrigger(trig);
            }
            if (transition.once) {
                transition.consumed = true;
            }
            return true;
        }
    }
    return false;
}

} // namespace

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

    // 阶段2.1优化：按层级批量更新，减少重复计算
    struct TransformInfo {
        Transform* transform;
        int depth;
    };
    std::vector<TransformInfo> dirtyTransforms;
    dirtyTransforms.reserve(entities.size() / 4);  // 预估25%的Transform是dirty的

    // 第一遍：收集所有dirty transforms并计算深度
    std::unordered_map<Transform*, int> transformDepths;
    for (const auto& entity : entities) {
        auto& comp = m_world->GetComponent<TransformComponent>(entity);
        if (comp.transform && comp.transform->IsDirty()) {
            int depth = comp.transform->GetHierarchyDepth();
            dirtyTransforms.push_back({
                comp.transform.get(),
                depth
            });
            transformDepths[comp.transform.get()] = depth;
        }
    }

    m_stats.dirtyTransforms = dirtyTransforms.size();

    if (dirtyTransforms.empty()) {
        return;  // 没有需要更新的
    }

    // 按深度排序（父对象先更新）
    std::sort(dirtyTransforms.begin(), dirtyTransforms.end(),
        [](const TransformInfo& a, const TransformInfo& b) {
            return a.depth < b.depth;
        });

    // 阶段2.1优化：按层级分组，同一层级的可以并行更新
    struct TransformGroup {
        std::vector<Transform*> transforms;
        int minDepth;
        int maxDepth;
    };

    std::vector<TransformGroup> groups;
    int currentDepth = -1;
    TransformGroup currentGroup;

    for (const auto& [transform, depth] : dirtyTransforms) {
        if (depth != currentDepth) {
            // 新层级，保存当前组并开始新组
            if (!currentGroup.transforms.empty()) {
                groups.push_back(currentGroup);
            }
            currentGroup = TransformGroup();
            currentGroup.minDepth = depth;
            currentGroup.maxDepth = depth;
            currentDepth = depth;
        }

        currentGroup.transforms.push_back(transform);
        currentGroup.maxDepth = depth;
    }

    if (!currentGroup.transforms.empty()) {
        groups.push_back(currentGroup);
    }

    // 按层级顺序批量更新（同一层级可以并行更新）
    for (const auto& group : groups) {
        // 同一层级的可以并行更新（如果支持多线程）
        // 未来可以考虑使用OpenMP并行化同一层级的更新
        for (Transform* transform : group.transforms) {
            transform->ForceUpdateWorldTransform();
        }
    }

    m_stats.batchGroups = groups.size();

    // 输出统计信息（可选）
    static int logCounter = 0;
    if (logCounter++ % 60 == 0 && m_stats.dirtyTransforms > 0) {
        Logger::GetInstance().DebugFormat(
            "[TransformSystem] Batch updated %zu Transform(s) in %zu group(s)",
            m_stats.dirtyTransforms, m_stats.batchGroups
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
    
    // 3. 加载 Model 资源
    LoadModelResources();
    
    // 4. 加载 Sprite 资源
    LoadSpriteResources();
    
    // 5. 加载纹理覆盖（多纹理支持）
    LoadTextureOverrides();
    
    // 6. 处理异步任务完成回调（回调会将更新加入队列，不直接修改组件）
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

void ResourceLoadingSystem::LoadModelResources() {
    auto entities = m_world->Query<ModelComponent>();
    if (entities.empty()) {
        return;
    }

    auto& resMgr = ResourceManager::GetInstance();

    for (const auto& entity : entities) {
        auto& modelComp = m_world->GetComponent<ModelComponent>(entity);

        if (modelComp.resourcesLoaded) {
            continue;
        }

        if (modelComp.model && modelComp.model->GetPartCount() > 0) {
            modelComp.resourcesLoaded = true;
            modelComp.asyncLoading = false;
            Logger::GetInstance().DebugFormat(
                "[ResourceLoadingSystem] Entity %u has pre-loaded model instance, marked as loaded",
                entity.index);
            continue;
        }

        if (modelComp.modelName.empty() && !modelComp.model) {
            Logger::GetInstance().WarningFormat(
                "[ResourceLoadingSystem] Entity %u: no model and no modelName specified",
                entity.index);
            modelComp.resourcesLoaded = true;
            modelComp.asyncLoading = false;
            continue;
        }

        if (modelComp.asyncLoading) {
            continue;
        }

        if (!modelComp.model && !modelComp.modelName.empty()) {
            if (resMgr.HasModel(modelComp.modelName)) {
                modelComp.model = resMgr.GetModel(modelComp.modelName);
                modelComp.asyncLoading = false;

                if (modelComp.model) {
                    modelComp.resourcesLoaded = true;
                    Logger::GetInstance().DebugFormat(
                        "[ResourceLoadingSystem] Model loaded from ResourceManager cache: %s",
                        modelComp.modelName.c_str());
                } else {
                    Logger::GetInstance().WarningFormat(
                        "[ResourceLoadingSystem] Model '%s' exists but returned null",
                        modelComp.modelName.c_str());
                    modelComp.resourcesLoaded = true;
                }
                continue;
            }
        }

        if (!m_asyncLoader || !m_asyncLoader->IsInitialized()) {
            Logger::GetInstance().WarningFormat(
                "[ResourceLoadingSystem] Async loader unavailable, cannot load model '%s'",
                modelComp.modelName.c_str());
            modelComp.resourcesLoaded = true;
            modelComp.asyncLoading = false;
            continue;
        }

        modelComp.asyncLoading = true;

        EntityID entityCopy = entity;
        std::string modelNameCopy = modelComp.modelName;
        ModelLoadOptions options = modelComp.loadOptions;
        if (options.resourcePrefix.empty()) {
            options.resourcePrefix = modelNameCopy;
        }
        if (options.registerModel && modelNameCopy.empty()) {
            options.registerModel = false;
        }

        std::weak_ptr<World> worldWeak;
        bool legacyCallback = false;
        try {
            worldWeak = m_world->weak_from_this();
        } catch (const std::bad_weak_ptr&) {
            legacyCallback = true;
            Logger::GetInstance().WarningFormat(
                "[ResourceLoadingSystem] World not managed by shared_ptr, using legacy model callback");
        }

        modelComp.registeredMeshNames.clear();
        modelComp.registeredMaterialNames.clear();

        Logger::GetInstance().DebugFormat(
            "[ResourceLoadingSystem] Starting async load for model: %s",
            modelNameCopy.c_str());

        auto invokeAsyncLoad = [&](auto&& callback) {
            auto task = m_asyncLoader->LoadModelAsync(
                modelNameCopy,
                modelNameCopy,
                options,
                std::forward<decltype(callback)>(callback));

            if (!task) {
                Logger::GetInstance().ErrorFormat(
                    "[ResourceLoadingSystem] Failed to enqueue async model load task: %s",
                    modelNameCopy.c_str());
                modelComp.asyncLoading = false;
                modelComp.resourcesLoaded = true;
            }
        };

        if (legacyCallback) {
            invokeAsyncLoad([this, entityCopy](const ModelLoadResult& result) {
                if (!m_shuttingDown.load()) {
                    this->OnModelLoaded(entityCopy, result);
                }
            });
        } else {
            invokeAsyncLoad([this, entityCopy, worldWeak](const ModelLoadResult& result) {
                if (auto worldShared = worldWeak.lock()) {
                    if (!m_shuttingDown.load()) {
                        this->OnModelLoaded(entityCopy, result);
                    } else {
                        Logger::GetInstance().DebugFormat(
                            "[ResourceLoadingSystem] System shutting down, skip model callback");
                    }
                } else {
                    Logger::GetInstance().InfoFormat(
                        "[ResourceLoadingSystem] World destroyed, skip model callback");
                }
            });
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
    
    if (result.IsSuccess() && result.resource) {
        auto& resMgr = ResourceManager::GetInstance();
        if (resMgr.HasTexture(result.name)) {
            Logger::GetInstance().DebugFormat(
                "[ResourceLoadingSystem] Texture '%s' already registered in ResourceManager",
                result.name.c_str());
        } else {
            if (!resMgr.RegisterTexture(result.name, result.resource)) {
                Logger::GetInstance().WarningFormat(
                    "[ResourceLoadingSystem] Failed to register texture '%s' to ResourceManager (possibly duplicate)",
                    result.name.c_str());
            } else {
                Logger::GetInstance().InfoFormat(
                    "[ResourceLoadingSystem] Registered texture '%s' to ResourceManager (async)",
                    result.name.c_str());
            }
        }
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

void ResourceLoadingSystem::OnModelLoaded(EntityID entity, const ModelLoadResult& result) {
    if (m_shuttingDown.load()) {
        Logger::GetInstance().InfoFormat("[ResourceLoadingSystem] System shutting down, ignoring model load callback");
        return;
    }

    std::lock_guard<std::mutex> lock(m_pendingMutex);

    PendingModelUpdate update;
    update.entity = entity;
    update.model = result.resource;
    update.meshResourceNames = result.meshResourceNames;
    update.materialResourceNames = result.materialResourceNames;
    update.success = result.IsSuccess();
    update.errorMessage = result.errorMessage;

    m_pendingModelUpdates.push_back(std::move(update));

    if (result.IsSuccess()) {
        Logger::GetInstance().DebugFormat(
            "[ResourceLoadingSystem] Model loaded, queued for update: %s",
            result.name.c_str());
    } else {
        Logger::GetInstance().ErrorFormat(
            "[ResourceLoadingSystem] Failed to load model: %s - %s",
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
    std::vector<PendingModelUpdate> modelUpdates;
    std::vector<PendingTextureUpdate> textureUpdates;
    std::vector<PendingTextureOverrideUpdate> textureOverrideUpdates;  // ✅ 新增
    
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        meshUpdates.swap(m_pendingMeshUpdates);
        modelUpdates.swap(m_pendingModelUpdates);
        textureUpdates.swap(m_pendingTextureUpdates);
        textureOverrideUpdates.swap(m_pendingTextureOverrideUpdates);  // ✅ 交换纹理覆盖更新
    }

    // 应用模型更新
    for (const auto& update : modelUpdates) {
        if (m_shuttingDown.load()) {
            Logger::GetInstance().InfoFormat("[ResourceLoadingSystem] System shutting down, abort applying model updates");
            break;
        }

        if (!m_world->IsValidEntity(update.entity)) {
            Logger::GetInstance().WarningFormat("[ResourceLoadingSystem] Entity %u is no longer valid (model)",
                                               update.entity.index);
            continue;
        }

        if (!m_world->HasComponent<ModelComponent>(update.entity)) {
            Logger::GetInstance().WarningFormat("[ResourceLoadingSystem] Entity %u missing ModelComponent",
                                               update.entity.index);
            continue;
        }

        try {
            auto& modelComp = m_world->GetComponent<ModelComponent>(update.entity);
            modelComp.asyncLoading = false;
            modelComp.model = update.model;
            modelComp.resourcesLoaded = update.success && (update.model != nullptr);
            modelComp.registeredMeshNames = update.meshResourceNames;
            modelComp.registeredMaterialNames = update.materialResourceNames;

            if (modelComp.resourcesLoaded) {
                Logger::GetInstance().InfoFormat(
                    "[ResourceLoadingSystem] ✅ Model applied successfully to entity %u (parts: %zu)",
                    update.entity.index,
                    update.model ? update.model->GetPartCount() : 0);
            } else {
                Logger::GetInstance().ErrorFormat(
                    "[ResourceLoadingSystem] Model loading failed for entity %u: %s",
                    update.entity.index,
                    update.errorMessage.c_str());
                modelComp.resourcesLoaded = true;
            }
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat("[ResourceLoadingSystem] Exception applying model update: %s", e.what());
        }
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

void MeshRenderSystem::SetLODInstancingEnabled(bool enabled) {
    // 阶段2.3：同步到 Renderer 的设置
    if (m_renderer) {
        m_renderer->SetLODInstancingEnabled(enabled);
    }
}

bool MeshRenderSystem::IsLODInstancingEnabled() const {
    // 阶段2.3：从 Renderer 获取设置，而不是使用本地设置
    if (m_renderer) {
        return m_renderer->IsLODInstancingEnabled();
    }
    return false;  // 如果没有 Renderer，返回 false（禁用）
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
    
    // 递增帧 ID
    m_frameId++;
    
    // 重置统计信息
    m_stats = RenderStats{};
    
    // ✅ 批量计算 LOD（无论是否启用实例化渲染）
    // 自动LOD计算应该在所有情况下都执行，以确保LOD级别根据相机距离自动更新
    if (m_world) {
        // 获取主相机位置
        Vector3 cameraPosition = GetMainCameraPosition();
        
        // 获取当前帧 ID
        uint64_t frameId = GetCurrentFrameId();
        
        // 查询所有有 LODComponent 的实体
        auto entities = m_world->Query<TransformComponent, MeshRenderComponent>();
        std::vector<EntityID> lodEntities;
        lodEntities.reserve(entities.size());
        
        for (EntityID entity : entities) {
            if (m_world->HasComponent<LODComponent>(entity)) {
                lodEntities.push_back(entity);
            }
        }
        
        // 批量计算 LOD（自动根据相机距离更新LOD级别）
        if (!lodEntities.empty()) {
            BatchCalculateLOD(lodEntities, cameraPosition, frameId);
        }
    }
    
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
        
        // ==================== 阶段2.2 + 阶段2.3: LOD 实例化渲染 ====================
        // 阶段2.3：从 Renderer 获取 LOD 实例化设置，而不是使用本地设置
        // 如果启用 LOD 实例化渲染，使用 LODInstancedRenderer 进行批量渲染
        // 优先级：LOD 实例化渲染 > 批处理系统
        bool lodInstancingEnabled = IsLODInstancingEnabled();
        bool lodInstancingAvailable = m_renderer && m_renderer->IsLODInstancingAvailable();
        
        if (lodInstancingEnabled && lodInstancingAvailable) {
            // 清空上一帧的实例化渲染器
            m_lodRenderer.Clear();
            
            // ==================== 阶段3.3：LOD 视锥体裁剪优化 ====================
            // 如果启用阶段3.3优化，使用 LODFrustumCullingSystem 进行批量视锥体裁剪和 LOD 选择
            std::vector<EntityID> entitiesToProcess;
            size_t totalEntities = entities.size();
            
            if (m_lodFrustumCullingEnabled && m_cameraSystem) {
                Camera* mainCamera = m_cameraSystem->GetMainCameraObject();
                if (mainCamera) {
                    // 使用 LODFrustumCullingSystem 进行批量视锥体裁剪和 LOD 选择
                    std::vector<EntityID> entityList(entities.begin(), entities.end());
                    auto visibleEntitiesByLOD = LODFrustumCullingSystem::BatchCullAndSelectLOD(
                        entityList,
                        m_world,
                        mainCamera,
                        m_frameId
                    );
                    
                    // 收集所有可见实体（按 LOD 级别分组）
                    size_t visibleCount = 0;
                    for (const auto& [lodLevel, visibleEntities] : visibleEntitiesByLOD) {
                        visibleCount += visibleEntities.size();
                        entitiesToProcess.insert(entitiesToProcess.end(), 
                                                 visibleEntities.begin(), 
                                                 visibleEntities.end());
                    }
                    
                    // 更新剔除统计（阶段3.3优化已经处理了视锥体裁剪）
                    size_t culledCount = totalEntities - visibleCount;
                    m_stats.culledMeshes += culledCount;
                } else {
                    // 如果无法获取相机，回退到原始逻辑
                    entitiesToProcess.assign(entities.begin(), entities.end());
                }
            } else {
                // 未启用阶段3.3优化，使用原始逻辑
                entitiesToProcess.assign(entities.begin(), entities.end());
            }
            
            // 收集实例数据（只处理可见实体）
            for (const auto& entity : entitiesToProcess) {
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
                
                // ==================== LOD 支持 ====================
                Ref<Mesh> renderMesh = meshComp.mesh;
                Ref<Material> renderMaterial = meshComp.material;
                LODLevel lodLevel = LODLevel::LOD0;
                bool usingLOD = false;
                
                if (m_world->HasComponent<LODComponent>(entity)) {
                    auto& lodComp = m_world->GetComponent<LODComponent>(entity);
                    lodLevel = lodComp.currentLOD;
                    usingLOD = lodComp.config.enabled;
                    
                    // 统计 LOD 使用情况
                    if (usingLOD) {
                        m_stats.lodEnabledEntities++;
                        
                        // 如果 LOD 级别是 Culled，跳过渲染
                        if (lodLevel == LODLevel::Culled) {
                            m_stats.culledMeshes++;
                            m_stats.lodCulledCount++;
                            continue;
                        }
                        
                        // 统计各 LOD 级别的使用数量
                        switch (lodLevel) {
                            case LODLevel::LOD0: m_stats.lod0Count++; break;
                            case LODLevel::LOD1: m_stats.lod1Count++; break;
                            case LODLevel::LOD2: m_stats.lod2Count++; break;
                            case LODLevel::LOD3: m_stats.lod3Count++; break;
                            default: break;
                        }
                    }
                    
                    // 使用 LOD 网格（如果配置了）
                    renderMesh = lodComp.config.GetLODMesh(lodLevel, meshComp.mesh);
                    
                    // 使用 LOD 材质（如果配置了）
                    renderMaterial = lodComp.config.GetLODMaterial(lodLevel, meshComp.material);
                    
                    // 应用 LOD 纹理（如果配置了）
                    if (lodComp.config.textureStrategy == TextureLODStrategy::UseLODTextures) {
                        lodComp.config.ApplyLODTextures(lodLevel, renderMaterial);
                    }
                }
                
                // 确保 LOD 网格和材质有效
                if (!renderMesh || !renderMaterial) {
                    continue;
                }
                
                // ==================== 视锥体裁剪优化 ====================
                // 阶段3.3：如果启用了LOD视锥体裁剪优化，LODFrustumCullingSystem已经处理了视锥体裁剪
                // 这里只需要对未启用阶段3.3优化的情况进行视锥体裁剪
                if (!m_lodFrustumCullingEnabled) {
                    Vector3 position = transform.GetPosition();
                    
                    // 从网格包围盒计算半径（使用 LOD 网格）
                    float radius = 1.0f;  // 默认半径
                    if (renderMesh) {
                        AABB bounds = renderMesh->CalculateBounds();
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
                } else {
                    // 阶段3.3：LODFrustumCullingSystem已经处理了视锥体裁剪和LOD选择
                    // 只需要更新剔除统计（LODFrustumCullingSystem已经剔除了不可见实体）
                    // 注意：这里的实体都是可见的，所以不需要再次剔除
                }
                
                // ✅ 检查材质是否有效
                if (renderMaterial && !renderMaterial->IsValid()) {
                    continue;
                }
                
                // 获取世界变换矩阵
                Matrix4 worldMatrix = transform.GetWorldMatrix();
                
                // 构建实例数据
                InstanceData instanceData;
                instanceData.worldMatrix = worldMatrix;
                instanceData.worldPosition = transform.GetPosition();
                instanceData.instanceID = entity.index;
                
                // 从组件获取实例颜色（如果支持）
                // 注意：当前 MeshRenderComponent 可能没有 instanceColor 字段
                // 这里使用默认白色，未来可以扩展
                instanceData.instanceColor = Color::White();
                instanceData.customParams = Vector4::Zero();
                
                // 添加到实例化渲染器
                m_lodRenderer.AddInstance(
                    entity,
                    renderMesh,
                    renderMaterial,
                    instanceData,
                    lodLevel
                );
                
                m_stats.visibleMeshes++;
            }
            
            // 渲染所有实例组
            m_lodRenderer.RenderAll(m_renderer, renderState.get());
            
            // 更新统计信息（从 LODInstancedRenderer 获取）
            auto lodStats = m_lodRenderer.GetStats();
            m_stats.drawCalls = lodStats.drawCalls;
            
            // ✅ 阶段2.3：更新 Renderer 的 LOD 实例化统计信息
            if (m_renderer) {
                Renderer::LODInstancingStats rendererStats;
                rendererStats.lodGroupCount = lodStats.groupCount;
                rendererStats.totalInstances = lodStats.totalInstances;
                rendererStats.drawCalls = lodStats.drawCalls;
                rendererStats.lod0Instances = lodStats.lod0Instances;
                rendererStats.lod1Instances = lodStats.lod1Instances;
                rendererStats.lod2Instances = lodStats.lod2Instances;
                rendererStats.lod3Instances = lodStats.lod3Instances;
                rendererStats.culledCount = lodStats.culledCount;
                m_renderer->UpdateLODInstancingStats(rendererStats);
            }
            
            // ✅ 调试信息：显示提交的数量和 LOD 统计
            static int logCounter = 0;
            if (logCounter++ < 10 || logCounter % 60 == 0) {
                if (m_stats.lodEnabledEntities > 0) {
                    Logger::GetInstance().InfoFormat(
                        "[MeshRenderSystem] LOD Instanced: %zu groups, %zu instances, %zu draw calls | "
                        "LOD: enabled=%zu, LOD0=%zu, LOD1=%zu, LOD2=%zu, LOD3=%zu, culled=%zu",
                        lodStats.groupCount, lodStats.totalInstances, lodStats.drawCalls,
                        m_stats.lodEnabledEntities, m_stats.lod0Count, m_stats.lod1Count,
                        m_stats.lod2Count, m_stats.lod3Count, m_stats.lodCulledCount
                    );
                } else {
                    Logger::GetInstance().InfoFormat(
                        "[MeshRenderSystem] LOD Instanced: %zu groups, %zu instances, %zu draw calls",
                        lodStats.groupCount, lodStats.totalInstances, lodStats.drawCalls
                    );
                }
            }
            
            // 提前返回，不使用传统渲染方式
            return;
        } else if (lodInstancingEnabled && !lodInstancingAvailable) {
            // ✅ 阶段2.3：回退机制 - LOD 实例化已启用但不可用，回退到批处理
            Logger::GetInstance().WarningFormat(
                "[MeshRenderSystem] LOD Instancing enabled but not available, falling back to batching mode"
            );
        }
        
        // ==================== 传统渲染方式（向后兼容） ====================
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
            
            // ==================== LOD 支持 ====================
            // 检查实体是否有 LODComponent，如果有则使用 LOD 网格和材质
            Ref<Mesh> renderMesh = meshComp.mesh;
            Ref<Material> renderMaterial = meshComp.material;
            LODLevel lodLevel = LODLevel::LOD0;
            bool usingLOD = false;
            
            if (m_world->HasComponent<LODComponent>(entity)) {
                auto& lodComp = m_world->GetComponent<LODComponent>(entity);
                lodLevel = lodComp.currentLOD;
                usingLOD = lodComp.config.enabled;
                
                // 统计 LOD 使用情况
                if (usingLOD) {
                    m_stats.lodEnabledEntities++;
                    
                    // 如果 LOD 级别是 Culled，跳过渲染
                    if (lodLevel == LODLevel::Culled) {
                        m_stats.culledMeshes++;
                        m_stats.lodCulledCount++;
                        continue;
                    }
                    
                    // 统计各 LOD 级别的使用数量
                    switch (lodLevel) {
                        case LODLevel::LOD0: m_stats.lod0Count++; break;
                        case LODLevel::LOD1: m_stats.lod1Count++; break;
                        case LODLevel::LOD2: m_stats.lod2Count++; break;
                        case LODLevel::LOD3: m_stats.lod3Count++; break;
                        default: break;
                    }
                }
                
                // 使用 LOD 网格（如果配置了）
                renderMesh = lodComp.config.GetLODMesh(lodLevel, meshComp.mesh);
                
                // 使用 LOD 材质（如果配置了）
                renderMaterial = lodComp.config.GetLODMaterial(lodLevel, meshComp.material);
                
                // 应用 LOD 纹理（如果配置了）
                if (lodComp.config.textureStrategy == TextureLODStrategy::UseLODTextures) {
                    lodComp.config.ApplyLODTextures(lodLevel, renderMaterial);
                }
            }
            
            // 确保 LOD 网格和材质有效
            if (!renderMesh || !renderMaterial) {
                Logger::GetInstance().DebugFormat("[MeshRenderSystem] Entity %u LOD mesh or material is null, skipping", entity.index);
                continue;
            }
            
            // 视锥体裁剪优化
            Vector3 position = transform.GetPosition();
            
            // 从网格包围盒计算半径（使用 LOD 网格）
            float radius = 1.0f;  // 默认半径
            if (renderMesh) {
                AABB bounds = renderMesh->CalculateBounds();
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
            if (renderMaterial && !renderMaterial->IsValid()) {
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
            RENDER_ASSERT(renderMesh != nullptr, "Mesh is null");
            RENDER_ASSERT(renderMaterial != nullptr, "Material is null");
            RENDER_ASSERT(transform.transform != nullptr, "Transform is null");
            
            MeshRenderable renderable;
            renderable.ClearDepthHint();
            renderable.SetMesh(renderMesh);  // 使用 LOD 网格
            renderable.SetMaterial(renderMaterial);  // 使用 LOD 材质
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

            const uint32_t overrideHash = ComputeMaterialOverrideHash(renderableOverride);
            uint32_t pipelineFlags = MaterialPipelineFlags_None;
            if (meshComp.castShadows) {
                pipelineFlags |= MaterialPipelineFlags_CastShadow;
            }
            if (meshComp.receiveShadows) {
                pipelineFlags |= MaterialPipelineFlags_ReceiveShadow;
            }
            if (meshComp.useInstancing && meshComp.instanceCount > 1) {
                pipelineFlags |= MaterialPipelineFlags_Instanced;
            }

            renderable.SetMaterialSortKey(
                BuildMaterialSortKey(renderMaterial.get(), overrideHash, pipelineFlags)  // 使用 LOD 材质
            );

            bool transparentHint = false;
            if (renderMaterial && renderMaterial->IsValid()) {  // 使用 LOD 材质
                const auto blendMode = renderMaterial->GetBlendMode();
                transparentHint = (blendMode == BlendMode::Alpha || blendMode == BlendMode::Additive);
            }
            if (!transparentHint && meshComp.materialOverride.opacity.has_value()) {
                transparentHint = meshComp.materialOverride.opacity.value() < 1.0f;
            }
            renderable.SetTransparentHint(transparentHint);
            
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
            bool isTransparent = renderable.GetTransparentHint();

            if (!isTransparent) {
                auto material = renderable.GetMaterial();
                
                if (material && material->IsValid()) {
                    auto blendMode = material->GetBlendMode();
                    isTransparent = (blendMode == BlendMode::Alpha || blendMode == BlendMode::Additive);
                    
                    if (!isTransparent && i < entities.size()) {
                        const auto& entity = entities[i];
                        const auto& meshComp = m_world->GetComponent<MeshRenderComponent>(entity);
                        
                        if (meshComp.materialOverride.opacity.has_value() && 
                            meshComp.materialOverride.opacity.value() < 1.0f) {
                            isTransparent = true;
                        }
                    }
                }
                renderable.SetTransparentHint(isTransparent);
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

                for (size_t idx : transparentIndices) {
                    auto& renderable = m_renderables[idx];
                    auto transformPtr = renderable.GetTransform();
                    Vector3 pos = transformPtr ? transformPtr->GetPosition() : Vector3::Zero();
                    float dist = (pos - cameraPos).squaredNorm();
                    renderable.SetDepthHint(dist);
                }
                
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
                        float distA = renderableA.HasDepthHint() ? renderableA.GetDepthHint()
                                                                 : (posA - cameraPos).squaredNorm();
                        float distB = renderableB.HasDepthHint() ? renderableB.GetDepthHint()
                                                                 : (posB - cameraPos).squaredNorm();
                        
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
    
        // ✅ 调试信息：显示提交的数量和 LOD 统计
        static int logCounter = 0;
        if (logCounter++ < 10 || logCounter % 60 == 0) {
            if (m_stats.lodEnabledEntities > 0) {
                Logger::GetInstance().InfoFormat(
                    "[MeshRenderSystem] Submitted %zu renderables (total: %zu, culled: %zu) | "
                    "LOD: enabled=%zu, LOD0=%zu, LOD1=%zu, LOD2=%zu, LOD3=%zu, culled=%zu",
                    m_stats.visibleMeshes, entities.size(), m_stats.culledMeshes,
                    m_stats.lodEnabledEntities, m_stats.lod0Count, m_stats.lod1Count,
                    m_stats.lod2Count, m_stats.lod3Count, m_stats.lodCulledCount
                );
            } else {
                Logger::GetInstance().InfoFormat("[MeshRenderSystem] Submitted %zu renderables (total entities: %zu, culled: %zu)", 
                                                 m_stats.visibleMeshes, entities.size(), m_stats.culledMeshes);
            }
        }
    }
    RENDER_CATCH {
        // 错误已被 ErrorHandler 处理和记录
    }
}

bool MeshRenderSystem::ShouldCull(const Vector3& position, float radius) const {
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

void MeshRenderSystem::BatchCalculateLOD(const std::vector<EntityID>& entities, 
                                          const Vector3& cameraPosition, 
                                          uint64_t frameId) {
    if (!m_world || entities.empty()) {
        return;
    }
    
    // ✅ 使用 LODSelector 批量计算 LOD
    LODSelector::BatchCalculateLOD(entities, m_world, cameraPosition, frameId);
    
    // ✅ 调试：记录LOD更新统计和距离信息（每100帧记录一次）
    static uint64_t debugCounter = 0;
    if (++debugCounter % 100 == 0 && !entities.empty()) {
        size_t lod0Count = 0, lod1Count = 0, lod2Count = 0, lod3Count = 0, culledCount = 0;
        float minDistance = std::numeric_limits<float>::max();
        float maxDistance = 0.0f;
        float avgDistance = 0.0f;
        
        for (EntityID entity : entities) {
            if (m_world->HasComponent<LODComponent>(entity) && 
                m_world->HasComponent<TransformComponent>(entity)) {
                auto& lodComp = m_world->GetComponent<LODComponent>(entity);
                auto& transformComp = m_world->GetComponent<TransformComponent>(entity);
                
                Vector3 entityPos = transformComp.GetPosition();
                float distance = (entityPos - cameraPosition).norm();
                minDistance = std::min(minDistance, distance);
                maxDistance = std::max(maxDistance, distance);
                avgDistance += distance;
                
                switch (lodComp.currentLOD) {
                    case LODLevel::LOD0: lod0Count++; break;
                    case LODLevel::LOD1: lod1Count++; break;
                    case LODLevel::LOD2: lod2Count++; break;
                    case LODLevel::LOD3: lod3Count++; break;
                    case LODLevel::Culled: culledCount++; break;
                    default: break;
                }
            }
        }
        
        if (lod0Count + lod1Count + lod2Count + lod3Count + culledCount > 0) {
            avgDistance /= static_cast<float>(lod0Count + lod1Count + lod2Count + lod3Count + culledCount);
        }
        
        Logger::GetInstance().InfoFormat(
            "[MeshRenderSystem] LOD Stats (frame %llu, camera pos: %.1f,%.1f,%.1f): "
            "LOD0=%zu, LOD1=%zu, LOD2=%zu, LOD3=%zu, Culled=%zu | "
            "Distance: min=%.1f, max=%.1f, avg=%.1f",
            frameId, cameraPosition.x(), cameraPosition.y(), cameraPosition.z(),
            lod0Count, lod1Count, lod2Count, lod3Count, culledCount,
            minDistance, maxDistance, avgDistance
        );
    }
}

Vector3 MeshRenderSystem::GetMainCameraPosition() const {
    if (!m_cameraSystem) {
        return Vector3::Zero();
    }
    
    Camera* camera = m_cameraSystem->GetMainCameraObject();
    if (!camera) {
        return Vector3::Zero();
    }
    
    return camera->GetPosition();
}

uint64_t MeshRenderSystem::GetCurrentFrameId() const {
    // 返回当前帧 ID（递增）
    return m_frameId;
}

// ============================================================
// ModelRenderSystem 实现
// ============================================================

ModelRenderSystem::ModelRenderSystem(Renderer* renderer)
    : m_renderer(renderer) {
    if (!m_renderer) {
        Logger::GetInstance().ErrorFormat("[ModelRenderSystem] Renderer is null");
    }
}

void ModelRenderSystem::OnCreate(World* world) {
    System::OnCreate(world);
}

void ModelRenderSystem::OnDestroy() {
    m_cameraSystem = nullptr;
    System::OnDestroy();
}

void ModelRenderSystem::Update(float deltaTime) {
    (void)deltaTime;

    if (!m_cameraSystem && m_world) {
        m_cameraSystem = m_world->GetSystemNoLock<CameraSystem>();
        if (m_cameraSystem) {
            Logger::GetInstance().InfoFormat("[ModelRenderSystem] CameraSystem acquired");
        }
    }

    m_stats = RenderStats{};
    SubmitRenderables();
}

void ModelRenderSystem::SubmitRenderables() {
    RENDER_TRY {
        if (!m_world || !m_renderer) {
            throw RENDER_WARNING(ErrorCode::NullPointer, "ModelRenderSystem: World or Renderer is null");
        }

        if (!m_renderer->IsInitialized()) {
            throw RENDER_WARNING(ErrorCode::NotInitialized, "ModelRenderSystem: Renderer is not initialized");
        }

        m_renderables.clear();

        auto entities = m_world->Query<TransformComponent, ModelComponent>();
        if (entities.empty()) {
            return;
        }

        Camera* camera = m_cameraSystem ? m_cameraSystem->GetMainCameraObject() : nullptr;
        Vector3 cameraPos = camera ? camera->GetPosition() : Vector3::Zero();

        std::vector<size_t> opaqueIndices;
        std::vector<size_t> transparentIndices;
        opaqueIndices.reserve(entities.size());
        transparentIndices.reserve(entities.size());

        for (const auto& entity : entities) {
            auto& transformComp = m_world->GetComponent<TransformComponent>(entity);
            auto& modelComp = m_world->GetComponent<ModelComponent>(entity);

            if (!modelComp.visible) {
                continue;
            }

            if (!modelComp.resourcesLoaded || !modelComp.model) {
                continue;
            }
            
            // ==================== LOD 支持 ====================
            // 检查实体是否有 LODComponent，如果有则使用 LOD 模型
            Ref<Model> renderModel = modelComp.model;
            LODLevel lodLevel = LODLevel::LOD0;
            bool usingLOD = false;
            
            if (m_world->HasComponent<LODComponent>(entity)) {
                auto& lodComp = m_world->GetComponent<LODComponent>(entity);
                lodLevel = lodComp.currentLOD;
                usingLOD = lodComp.config.enabled;
                
                // 统计 LOD 使用情况
                if (usingLOD) {
                    m_stats.lodEnabledEntities++;
                    
                    // 如果 LOD 级别是 Culled，跳过渲染
                    if (lodLevel == LODLevel::Culled) {
                        m_stats.culledModels++;
                        m_stats.lodCulledCount++;
                        continue;
                    }
                    
                    // 统计各 LOD 级别的使用数量
                    switch (lodLevel) {
                        case LODLevel::LOD0: m_stats.lod0Count++; break;
                        case LODLevel::LOD1: m_stats.lod1Count++; break;
                        case LODLevel::LOD2: m_stats.lod2Count++; break;
                        case LODLevel::LOD3: m_stats.lod3Count++; break;
                        default: break;
                    }
                }
                
                // 使用 LOD 模型（如果配置了）
                renderModel = lodComp.config.GetLODModel(lodLevel, modelComp.model);
            }
            
            // 确保 LOD 模型有效
            if (!renderModel) {
                Logger::GetInstance().DebugFormat("[ModelRenderSystem] Entity %u LOD model is null, skipping", entity.index);
                continue;
            }

            Matrix4 worldMatrix = transformComp.GetWorldMatrix();
            AABB localBounds = renderModel->GetBounds();  // 使用 LOD 模型的包围盒
            Vector3 localCenter = localBounds.GetCenter();
            Vector3 localSize = localBounds.GetSize();
            float radius = localSize.norm() * 0.5f;
            if (radius <= 0.0f) {
                radius = 0.5f;
            }

            Vector3 worldCenter = (worldMatrix * Vector4(localCenter.x(), localCenter.y(), localCenter.z(), 1.0f)).head<3>();

            Vector3 scale = transformComp.GetScale();
            float maxScale = std::max({std::fabs(scale.x()), std::fabs(scale.y()), std::fabs(scale.z())});
            if (maxScale > 0.0f) {
                radius *= maxScale;
            }

            if (ShouldCull(worldCenter, radius)) {
                m_stats.culledModels++;
                continue;
            }

            ModelRenderable renderable;
            renderable.SetTransform(transformComp.transform);
            renderable.SetModel(renderModel);  // 使用 LOD 模型
            renderable.SetVisible(modelComp.visible);
            renderable.SetLayerID(modelComp.layerID);
            renderable.SetRenderPriority(modelComp.renderPriority);
            renderable.SetCastShadows(modelComp.castShadows);
            renderable.SetReceiveShadows(modelComp.receiveShadows);
            renderable.ClearDepthHint();
            renderable.MarkMaterialSortKeyDirty();

            if (camera) {
                float depth = (worldCenter - cameraPos).squaredNorm();
                renderable.SetDepthHint(depth);
            }

            size_t index = m_renderables.size();
            m_renderables.push_back(std::move(renderable));

            auto& stored = m_renderables.back();
            if (stored.GetTransparentHint()) {
                transparentIndices.push_back(index);
            } else {
                opaqueIndices.push_back(index);
            }

            m_stats.visibleModels++;
            m_stats.submittedParts += modelComp.model->GetPartCount();
        }

        for (size_t idx : opaqueIndices) {
            m_renderer->SubmitRenderable(&m_renderables[idx]);
            m_stats.submittedRenderables++;
        }

        if (!transparentIndices.empty()) {
            if (camera) {
                std::sort(transparentIndices.begin(), transparentIndices.end(),
                    [&](size_t a, size_t b) {
                        const auto& renderableA = m_renderables[a];
                        const auto& renderableB = m_renderables[b];

                        float distA = renderableA.HasDepthHint()
                            ? renderableA.GetDepthHint()
                            : (renderableA.GetTransform() ? (renderableA.GetTransform()->GetPosition() - cameraPos).squaredNorm() : 0.0f);

                        float distB = renderableB.HasDepthHint()
                            ? renderableB.GetDepthHint()
                            : (renderableB.GetTransform() ? (renderableB.GetTransform()->GetPosition() - cameraPos).squaredNorm() : 0.0f);

                        return distA > distB;
                    });
            }

            for (size_t idx : transparentIndices) {
                m_renderer->SubmitRenderable(&m_renderables[idx]);
                m_stats.submittedRenderables++;
            }
        }

        static int logCounter = 0;
        if (logCounter++ < 10 || logCounter % 120 == 0) {
            if (m_stats.lodEnabledEntities > 0) {
                Logger::GetInstance().InfoFormat(
                    "[ModelRenderSystem] Submitted %zu models (culled: %zu, parts: %zu) | "
                    "LOD: enabled=%zu, LOD0=%zu, LOD1=%zu, LOD2=%zu, LOD3=%zu, culled=%zu",
                    m_stats.visibleModels, m_stats.culledModels, m_stats.submittedParts,
                    m_stats.lodEnabledEntities, m_stats.lod0Count, m_stats.lod1Count,
                    m_stats.lod2Count, m_stats.lod3Count, m_stats.lodCulledCount
                );
            } else {
                Logger::GetInstance().InfoFormat(
                    "[ModelRenderSystem] Submitted %zu models (culled: %zu, parts: %zu)",
                    m_stats.visibleModels,
                    m_stats.culledModels,
                    m_stats.submittedParts);
            }
        }
    }
    RENDER_CATCH {
        // 错误已由 ErrorHandler 记录
    }
}

bool ModelRenderSystem::ShouldCull(const Vector3& position, float radius) const {
    if (!m_cameraSystem) {
        return false;
    }

    Camera* mainCamera = m_cameraSystem->GetMainCameraObject();
    if (!mainCamera) {
        return false;
    }

    Vector3 cameraPos = mainCamera->GetPosition();
    float distanceToCamera = (position - cameraPos).norm();
    const float noCullRadius = 5.0f;
    if (distanceToCamera < noCullRadius + radius) {
        return false;
    }

    const Frustum& frustum = mainCamera->GetFrustum();
    float expandedRadius = radius * 1.5f;
    return !frustum.IntersectsSphere(position, expandedRadius);
}

// ============================================================
// SpriteAnimationSystem 实现
// ============================================================

void SpriteAnimationSystem::Update(float deltaTime) {
    if (!m_world) {
        return;
    }

    auto entities = m_world->Query<SpriteRenderComponent, SpriteAnimationComponent>();
    if (entities.empty()) {
        return;
    }

    for (const auto& entity : entities) {
        auto& animComp = m_world->GetComponent<SpriteAnimationComponent>(entity);
        auto& spriteComp = m_world->GetComponent<SpriteRenderComponent>(entity);

#if defined(DEBUG) || defined(_DEBUG)
        auto& debugger = SpriteAnimationDebugger::GetInstance();
        debugger.ApplyPendingCommands(entity, animComp);
#endif

        // 为常见默认值自动应用命名层，便于 UI 与世界层级管理
        if (spriteComp.screenSpace) {
            const bool needsUiDefault =
                spriteComp.sortOrder == 0 &&
                (spriteComp.layerID == Layers::UI::Default.value || spriteComp.layerID == 0u);
            if (needsUiDefault) {
                SpriteRenderLayer::ApplyLayer("ui.default", spriteComp, 0);
            }
        } else {
            const bool needsWorldDefault =
                spriteComp.sortOrder == 0 &&
                (spriteComp.layerID == Layers::World::Midground.value ||
                 spriteComp.layerID == Layers::UI::Default.value ||
                 spriteComp.layerID == 0u);
            if (needsWorldDefault) {
                SpriteRenderLayer::ApplyLayer("world.midground", spriteComp, 0);
            }
        }

        bool stateJustInitialized = false;
        if (!animComp.states.empty()) {
            if (animComp.currentState.empty()) {
                std::string startState = animComp.defaultState;
                if (startState.empty() || !animComp.HasState(startState)) {
                    startState = animComp.states.begin()->first;
                }
                if (!startState.empty()) {
                    stateJustInitialized = ApplyAnimationState(entity, animComp, spriteComp, startState);
                }
            }
            if (!stateJustInitialized) {
                animComp.stateTime += deltaTime;
            }
        }

        animComp.ClearEvents();
        animComp.FlushDebugEvents(animComp.events);
        bool needsFrameUpdate = animComp.dirty;

        if (animComp.currentClip.empty()) {
            animComp.dirty = false;
            continue;
        }

        auto clipIt = animComp.clips.find(animComp.currentClip);
        if (clipIt == animComp.clips.end()) {
            continue;
        }

        auto& clip = clipIt->second;
        if (clip.frames.empty()) {
            continue;
        }

        SpritePlaybackMode playbackMode = clip.playbackMode;
        if (playbackMode == SpritePlaybackMode::Loop && !clip.loop) {
            playbackMode = SpritePlaybackMode::Once;
        }

        const int frameCount = static_cast<int>(clip.frames.size());
        if (frameCount == 0) {
            continue;
        }

        auto pushEvent = [&](SpriteAnimationEvent::Type type) {
            animComp.events.push_back(SpriteAnimationEvent{
                type,
                animComp.currentClip,
                std::clamp(animComp.currentFrame, 0, frameCount - 1)
            });
        };

        if (animComp.currentFrame < 0 || animComp.currentFrame >= frameCount) {
            animComp.currentFrame = std::clamp(animComp.currentFrame, 0, frameCount - 1);
            animComp.timeInFrame = 0.0f;
            needsFrameUpdate = true;
        }

        if (animComp.clipJustChanged) {
            animComp.playbackDirection = animComp.playbackSpeed < 0.0f ? -1 : 1;
            animComp.currentFrame = animComp.playbackDirection < 0
                                        ? frameCount - 1
                                        : std::clamp(animComp.currentFrame, 0, frameCount - 1);
            animComp.timeInFrame = 0.0f;
            pushEvent(SpriteAnimationEvent::Type::ClipStarted);
            animComp.clipJustChanged = false;
            needsFrameUpdate = true;
        }

        if (animComp.playing && deltaTime > 0.0f) {
            float frameDuration = clip.frameDuration > 0.0f ? clip.frameDuration : 0.016f;
            float speedAbs = std::abs(animComp.playbackSpeed);

            if (speedAbs > std::numeric_limits<float>::epsilon()) {
                animComp.timeInFrame += deltaTime * speedAbs;
                int direction = (animComp.playbackSpeed < 0.0f)
                                    ? -1
                                    : (animComp.playbackDirection == 0 ? 1 : animComp.playbackDirection);

                while (animComp.timeInFrame >= frameDuration && animComp.playing) {
                    animComp.timeInFrame -= frameDuration;
                    int previousFrame = animComp.currentFrame;
                    animComp.currentFrame += direction;

                    switch (playbackMode) {
                    case SpritePlaybackMode::Loop:
                        if (direction > 0 && animComp.currentFrame >= frameCount) {
                            animComp.currentFrame = 0;
                        } else if (direction < 0 && animComp.currentFrame < 0) {
                            animComp.currentFrame = frameCount - 1;
                        }
                        break;
                    case SpritePlaybackMode::Once:
                        if (direction > 0 && animComp.currentFrame >= frameCount) {
                            animComp.currentFrame = frameCount - 1;
                            animComp.playing = false;
                            animComp.timeInFrame = 0.0f;
                            pushEvent(SpriteAnimationEvent::Type::ClipCompleted);
                        } else if (direction < 0 && animComp.currentFrame < 0) {
                            animComp.currentFrame = 0;
                            animComp.playing = false;
                            animComp.timeInFrame = 0.0f;
                            pushEvent(SpriteAnimationEvent::Type::ClipCompleted);
                        }
                        break;
                    case SpritePlaybackMode::PingPong:
                        if (frameCount > 1) {
                            if (direction > 0 && animComp.currentFrame >= frameCount) {
                                animComp.currentFrame = frameCount - 2;
                                direction = -1;
                            } else if (direction < 0 && animComp.currentFrame < 0) {
                                animComp.currentFrame = frameCount > 1 ? 1 : 0;
                                direction = 1;
                            }
                        } else {
                            animComp.currentFrame = 0;
                            direction = 0;
                        }
                        break;
                    }

                    animComp.currentFrame = std::clamp(animComp.currentFrame, 0, frameCount - 1);

                    if (animComp.currentFrame != previousFrame) {
                        pushEvent(SpriteAnimationEvent::Type::FrameChanged);
                        needsFrameUpdate = true;
                    }

                    if (!animComp.playing || direction == 0) {
                        break;
                    }

                    animComp.playbackDirection = direction;
                }

                if (direction == 0) {
                    animComp.playbackDirection = animComp.playbackDirection == 0 ? 1 : animComp.playbackDirection;
                } else {
                    animComp.playbackDirection = direction;
                }
            }
        }

        if (needsFrameUpdate) {
            animComp.currentFrame = std::clamp(animComp.currentFrame, 0, frameCount - 1);
            spriteComp.sourceRect = clip.frames[animComp.currentFrame];
            animComp.dirty = false;

            Logger::GetInstance().DebugFormat(
                "[SpriteAnimationSystem] entity %u clip='%s' frame=%d rect=(%.3f, %.3f, %.3f, %.3f)",
                entity.index,
                animComp.currentClip.c_str(),
                animComp.currentFrame,
                spriteComp.sourceRect.x,
                spriteComp.sourceRect.y,
                spriteComp.sourceRect.width,
                spriteComp.sourceRect.height);

            bool hasFrameEvent = std::any_of(animComp.events.begin(), animComp.events.end(),
                [&](const SpriteAnimationEvent& evt) {
                    return evt.type == SpriteAnimationEvent::Type::FrameChanged &&
                           evt.frameIndex == animComp.currentFrame &&
                           evt.clip == animComp.currentClip;
                });

            if (!hasFrameEvent) {
                pushEvent(SpriteAnimationEvent::Type::FrameChanged);
            }
        }

        bool stateChangedThisFrame = false;
        if (!animComp.states.empty() && !animComp.currentState.empty()) {
            stateChangedThisFrame = ProcessAnimationTransitions(entity, animComp, spriteComp, animComp.events);
        }

        if (!animComp.eventListeners.empty() && !animComp.events.empty()) {
            for (const auto& evt : animComp.events) {
                for (const auto& listener : animComp.eventListeners) {
                    if (listener) {
                        listener(entity, evt);
                    }
                }
            }
        }

        if (!animComp.scriptBindings.empty() && !animComp.events.empty()) {
            for (const auto& evt : animComp.events) {
                for (const auto& binding : animComp.scriptBindings) {
                    if (binding.scriptName.empty()) {
                        continue;
                    }
                    if (binding.eventType != evt.type) {
                        continue;
                    }
                    if (!binding.clip.empty() && binding.clip != evt.clip) {
                        continue;
                    }
                    if (binding.frameIndex >= 0 && binding.frameIndex != evt.frameIndex) {
                        continue;
                    }
                    if (SpriteAnimationScriptRegistry::Invoke(binding.scriptName, entity, evt, animComp)) {
#if defined(DEBUG) || defined(_DEBUG)
                        debugger.RecordScriptInvocation(entity, binding.scriptName, evt);
#endif
                    }
                }
            }
        }

        if (stateChangedThisFrame) {
            // 状态切换已在 ApplyAnimationState 中完成，无需额外处理
        }

#if defined(DEBUG) || defined(_DEBUG)
        debugger.CaptureSnapshot(entity, animComp, spriteComp);
        debugger.AppendEvents(entity, animComp.events);
#endif
    }
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

void SpriteRenderSystem::OnCreate(World* world) {
    System::OnCreate(world);
    // ⚠️ 注意：不要在此处调用 world->GetSystem()，因为 RegisterSystem 内部已经持有写锁，
    // 再次请求读锁会导致死锁。改为在 Update 中按需获取（使用 GetSystemNoLock）。
    m_cameraSystem = nullptr;
}

void SpriteRenderSystem::OnDestroy() {
    m_cameraSystem = nullptr;
    System::OnDestroy();
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
    
    // 延迟获取 CameraSystem，避免在 OnCreate 中读取导致死锁
    if (!m_cameraSystem) {
        m_cameraSystem = m_world->GetSystemNoLock<CameraSystem>();
        if (m_cameraSystem) {
            Logger::GetInstance().Info("[SpriteRenderSystem] CameraSystem acquired");
        }
    }
    
    auto entities = m_world->Query<TransformComponent, SpriteRenderComponent>();
    if (entities.empty()) {
        return;
    }

    m_batcher.Clear();

    float width = static_cast<float>(m_renderer->GetWidth());
    float height = static_cast<float>(m_renderer->GetHeight());
    if (width <= 0.0f) {
        width = 1.0f;
    }
    if (height <= 0.0f) {
        height = 1.0f;
    }

    Matrix4 screenView = Matrix4::Identity();
    Matrix4 screenProjection = MathUtils::Orthographic(0.0f, width, height, 0.0f, -1.0f, 1.0f);
    SpriteRenderable::SetViewProjection(screenView, screenProjection);

    Matrix4 worldView = Matrix4::Identity();
    Matrix4 worldProjection = Matrix4::Identity();
    bool worldMatricesReady = false;

    if (m_cameraSystem) {
        if (auto camera = m_cameraSystem->GetMainCameraSharedPtr()) {
            worldView = camera->GetViewMatrix();
            worldProjection = camera->GetProjectionMatrix();
            worldMatricesReady = true;
        }
    }

    static bool loggedCameraWarning = false;

    size_t submittedSprites = 0;

    auto& layerRegistry = m_renderer->GetLayerRegistry();
    const uint32_t uiDefaultLayer = Layers::UI::Default.value;
    const uint32_t worldMidLayer = Layers::World::Midground.value;

    for (const auto& entity : entities) {
        auto& transform = m_world->GetComponent<TransformComponent>(entity);
        auto& spriteComp = m_world->GetComponent<SpriteRenderComponent>(entity);

        if (!spriteComp.visible) {
            continue;
        }

        const RenderLayerId currentLayerId(spriteComp.layerID);
        const bool layerRegistered = currentLayerId.IsValid() && layerRegistry.HasLayer(currentLayerId);

        // 若保持默认层配置，则自动映射到命名层，保证排序一致性
        if (spriteComp.screenSpace) {
            const bool needsUiDefault =
                spriteComp.sortOrder == 0 &&
                (!layerRegistered || spriteComp.layerID == uiDefaultLayer || spriteComp.layerID == 0u);
            if (needsUiDefault) {
                SpriteRenderLayer::ApplyLayer("ui.default", spriteComp, 0);
            }
        } else {
            const bool needsWorldDefault =
                spriteComp.sortOrder == 0 &&
                (!layerRegistered ||
                 spriteComp.layerID == worldMidLayer ||
                 spriteComp.layerID == uiDefaultLayer ||
                 spriteComp.layerID == 0u);
            if (needsWorldDefault) {
                SpriteRenderLayer::ApplyLayer("world.midground", spriteComp, 0);
            }
        }

        static int s_debugged = 0;
        if (s_debugged < 4) {
            Logger::GetInstance().InfoFormat(
                "[SpriteRenderSystem] entity %u sourceRect=(%.3f, %.3f, %.3f, %.3f)",
                entity.index,
                spriteComp.sourceRect.x,
                spriteComp.sourceRect.y,
                spriteComp.sourceRect.width,
                spriteComp.sourceRect.height);
            ++s_debugged;
        }

        if (!spriteComp.resourcesLoaded || !spriteComp.texture) {
            Logger::GetInstance().DebugFormat("[SpriteRenderSystem] Entity %u sprite texture not ready", entity.index);
            continue;
        }

        if (!transform.transform) {
            Logger::GetInstance().WarningFormat("[SpriteRenderSystem] Entity %u has null transform", entity.index);
            continue;
        }

        Vector2 effectiveSize = spriteComp.size;
        float texWidth = static_cast<float>(spriteComp.texture->GetWidth());
        float texHeight = static_cast<float>(spriteComp.texture->GetHeight());
        if (effectiveSize.x() <= 0.0f) {
            effectiveSize.x() = texWidth > 0.0f ? texWidth : 1.0f;
        }
        if (effectiveSize.y() <= 0.0f) {
            effectiveSize.y() = texHeight > 0.0f ? texHeight : 1.0f;
        }
        Matrix4 baseMatrix = transform.transform ? transform.transform->GetWorldMatrix() : Matrix4::Identity();

        if (spriteComp.screenSpace && transform.transform) {
            const float offsetX = spriteComp.subPixelOffset.x();
            const float offsetY = spriteComp.subPixelOffset.y();
            if (spriteComp.snapToPixel) {
                Vector3 worldPos = transform.transform->GetWorldPosition();
                float snappedX = std::round(worldPos.x()) + offsetX;
                float snappedY = std::round(worldPos.y()) + offsetY;
                Vector3 delta(snappedX - worldPos.x(), snappedY - worldPos.y(), 0.0f);
                if (delta.squaredNorm() > MathUtils::EPSILON) {
                    baseMatrix = MathUtils::Translate(delta) * baseMatrix;
                }
            } else if (std::fabs(offsetX) > MathUtils::EPSILON || std::fabs(offsetY) > MathUtils::EPSILON) {
                baseMatrix = MathUtils::Translate(Vector3(offsetX, offsetY, 0.0f)) * baseMatrix;
            }
        }

        const bool flipX = Render::SpriteUI::HasFlag(spriteComp.flipFlags, Render::SpriteUI::SpriteFlipFlags::FlipX);
        const bool flipY = Render::SpriteUI::HasFlag(spriteComp.flipFlags, Render::SpriteUI::SpriteFlipFlags::FlipY);
        if (flipX || flipY) {
            Vector3 flipOffset(
                flipX ? effectiveSize.x() : 0.0f,
                flipY ? effectiveSize.y() : 0.0f,
                0.0f);
            Vector3 flipScale(flipX ? -1.0f : 1.0f,
                              flipY ? -1.0f : 1.0f,
                              1.0f);
            baseMatrix *= MathUtils::Translate(flipOffset);
            baseMatrix *= MathUtils::Scale(flipScale);
        }

        Matrix4 viewForSprite = screenView;
        Matrix4 projForSprite = screenProjection;
        if (spriteComp.screenSpace) {
            viewForSprite = screenView;
            projForSprite = screenProjection;
        } else {
            if (worldMatricesReady) {
                viewForSprite = worldView;
                projForSprite = worldProjection;
            } else {
                if (!loggedCameraWarning) {
                    Logger::GetInstance().Warning("[SpriteRenderSystem] No active camera found for world-space sprites, using screen-space projection as fallback");
                    loggedCameraWarning = true;
                }
                viewForSprite = screenView;
                projForSprite = screenProjection;
            }
        }

        auto submitSprite = [&](const Rect& srcRect,
                                const Vector2& sliceSize,
                                const Matrix4& sliceMatrix) {
            m_batcher.AddSprite(spriteComp.texture,
                                srcRect,
                                sliceSize,
                                spriteComp.tintColor,
                                sliceMatrix,
                                viewForSprite,
                                projForSprite,
                                spriteComp.screenSpace,
                                spriteComp.layerID,
                                spriteComp.sortOrder);
            submittedSprites++;
        };

        if (spriteComp.nineSlice.IsEnabled()) {
            Rect srcRectPixels = spriteComp.sourceRect;
            const bool normalizedRect = std::fabs(srcRectPixels.width) <= 1.0f || std::fabs(srcRectPixels.height) <= 1.0f;
            if (normalizedRect) {
                srcRectPixels.x *= texWidth;
                srcRectPixels.y *= texHeight;
                srcRectPixels.width *= texWidth;
                srcRectPixels.height *= texHeight;
            }

            const float widthPx = std::fabs(srcRectPixels.width);
            const float heightPx = std::fabs(srcRectPixels.height);
            const float srcXStart = srcRectPixels.width >= 0.0f ? srcRectPixels.x : srcRectPixels.x + srcRectPixels.width;
            const float srcYStart = srcRectPixels.height >= 0.0f ? srcRectPixels.y : srcRectPixels.y + srcRectPixels.height;

            if (widthPx < MathUtils::EPSILON || heightPx < MathUtils::EPSILON ||
                effectiveSize.x() < MathUtils::EPSILON || effectiveSize.y() < MathUtils::EPSILON) {
                Matrix4 modelMatrix = baseMatrix;
                modelMatrix *= MathUtils::Scale(Vector3(effectiveSize.x(), effectiveSize.y(), 1.0f));
                submitSprite(spriteComp.sourceRect, effectiveSize, modelMatrix);
                continue;
            }

            const Vector4 borders = spriteComp.nineSlice.borderPixels;
            const float leftPx = std::clamp(borders.x(), 0.0f, widthPx);
            const float rightPx = std::clamp(borders.y(), 0.0f, widthPx - leftPx);
            const float topPx = std::clamp(borders.z(), 0.0f, heightPx);
            const float bottomPx = std::clamp(borders.w(), 0.0f, heightPx - topPx);

            const float centerWidthPx = std::max(widthPx - leftPx - rightPx, 0.0f);
            const float centerHeightPx = std::max(heightPx - topPx - bottomPx, 0.0f);

            const float destLeft = widthPx > MathUtils::EPSILON ? effectiveSize.x() * (leftPx / widthPx) : 0.0f;
            const float destRight = widthPx > MathUtils::EPSILON ? effectiveSize.x() * (rightPx / widthPx) : 0.0f;
            const float destTop = heightPx > MathUtils::EPSILON ? effectiveSize.y() * (topPx / heightPx) : 0.0f;
            const float destBottom = heightPx > MathUtils::EPSILON ? effectiveSize.y() * (bottomPx / heightPx) : 0.0f;

            const float destCenterWidth = std::max(effectiveSize.x() - destLeft - destRight, 0.0f);
            const float destCenterHeight = std::max(effectiveSize.y() - destTop - destBottom, 0.0f);

            const float destWidths[3] = {destLeft, destCenterWidth, destRight};
            const float destHeights[3] = {destBottom, destCenterHeight, destTop};
            const float srcWidths[3] = {leftPx, centerWidthPx, rightPx};
            const float srcHeights[3] = {bottomPx, centerHeightPx, topPx};

            float srcXOffsets[3] = {
                srcXStart,
                srcXStart + leftPx,
                srcXStart + leftPx + centerWidthPx
            };

            float srcYOffsets[3] = {
                srcYStart,
                srcYStart + bottomPx,
                srcYStart + bottomPx + centerHeightPx
            };

            float destXOffsets[3] = {
                0.0f,
                destLeft,
                destLeft + destCenterWidth
            };

            float destYOffsets[3] = {
                0.0f,
                destBottom,
                destBottom + destCenterHeight
            };

            auto mapIndex = [&](int idx, bool flip) -> int {
                if (!flip) {
                    return idx;
                }
                if (idx == 0) return 2;
                if (idx == 2) return 0;
                return 1;
            };

            for (int row = 0; row < 3; ++row) {
                const int mappedRow = mapIndex(row, flipY);
                if (destHeights[mappedRow] <= MathUtils::EPSILON || srcHeights[row] <= MathUtils::EPSILON) {
                    continue;
                }
                for (int col = 0; col < 3; ++col) {
                    const int mappedCol = mapIndex(col, flipX);
                    if (destWidths[mappedCol] <= MathUtils::EPSILON || srcWidths[col] <= MathUtils::EPSILON) {
                        continue;
                    }

                    Rect sliceRect;
                    sliceRect.x = srcXOffsets[col];
                    sliceRect.y = srcYOffsets[row];
                    sliceRect.width = srcWidths[col];
                    sliceRect.height = srcHeights[row];

                    Matrix4 sliceMatrix = baseMatrix;
                    sliceMatrix *= MathUtils::Translate(Vector3(destXOffsets[mappedCol], destYOffsets[mappedRow], 0.0f));
                    sliceMatrix *= MathUtils::Scale(Vector3(destWidths[mappedCol], destHeights[mappedRow], 1.0f));

                    submitSprite(sliceRect, Vector2(destWidths[mappedCol], destHeights[mappedRow]), sliceMatrix);
                }
            }
            continue;
        }

        Matrix4 modelMatrix = baseMatrix;
        modelMatrix *= MathUtils::Scale(Vector3(effectiveSize.x(), effectiveSize.y(), 1.0f));

        submitSprite(spriteComp.sourceRect, effectiveSize, modelMatrix);
    }

    m_lastSubmittedSpriteCount = submittedSprites;

    static bool loggedOnce = false;
    if (submittedSprites > 0 && !loggedOnce) {
        Logger::GetInstance().InfoFormat("[SpriteRenderSystem] Collected %zu sprites for batching", submittedSprites);
        loggedOnce = true;
    }

    m_batcher.BuildBatches();
    size_t batchCount = m_batcher.GetBatchCount();
    if (batchCount > m_batchRenderables.size()) {
        size_t oldSize = m_batchRenderables.size();
        m_batchRenderables.resize(batchCount);
        for (size_t i = oldSize; i < batchCount; ++i) {
            m_batchRenderables[i] = std::make_unique<SpriteBatchRenderable>();
        }
    }

    for (size_t i = 0; i < batchCount; ++i) {
        auto& renderable = m_batchRenderables[i];
        if (!renderable) {
            renderable = std::make_unique<SpriteBatchRenderable>();
        }
        renderable->ClearDepthHint();
        renderable->SetBatch(&m_batcher, i);
        renderable->SetLayerID(m_batcher.GetBatchLayer(i));
        renderable->SetRenderPriority(m_batcher.GetBatchSortOrder(i));
        SpriteBatcher::SpriteBatchInfo batchInfo{};
        if (m_batcher.GetBatchInfo(i, batchInfo)) {
            uint32_t overrideHash = 0;
            overrideHash = HashCombine(overrideHash, batchInfo.viewHash);
            overrideHash = HashCombine(overrideHash, batchInfo.projectionHash);
            if (batchInfo.texture) {
                const uintptr_t ptrValue = reinterpret_cast<uintptr_t>(batchInfo.texture.get());
                overrideHash = HashCombine(overrideHash, static_cast<uint32_t>(ptrValue & 0xFFFFFFFFu));
                overrideHash = HashCombine(overrideHash, static_cast<uint32_t>((ptrValue >> 32) & 0xFFFFFFFFu));
            }

            Logger::GetInstance().DebugFormat(
                "[LayerMaskDebug][SpriteBatch] index=%zu layer=%u screenSpace=%s instances=%u sortOrder=%d blend=%d textureValid=%s",
                i,
                batchInfo.layer,
                batchInfo.screenSpace ? "true" : "false",
                batchInfo.instanceCount,
                batchInfo.sortOrder,
                static_cast<int>(batchInfo.blendMode),
                (batchInfo.texture && batchInfo.texture->IsValid()) ? "true" : "false");

            uint32_t pipelineFlags = MaterialPipelineFlags_None;
            if (batchInfo.screenSpace) {
                pipelineFlags |= MaterialPipelineFlags_ScreenSpace;
            }
            if (batchInfo.instanceCount > 1) {
                pipelineFlags |= MaterialPipelineFlags_Instanced;
            }

            MaterialSortKey sortKey = BuildMaterialSortKey(nullptr, overrideHash, pipelineFlags);
            sortKey.blendMode = batchInfo.blendMode;
            sortKey.depthWrite = false;
            sortKey.depthTest = !batchInfo.screenSpace;

            renderable->SetMaterialSortKey(sortKey);

            const bool transparentHint =
                (batchInfo.blendMode == BlendMode::Alpha ||
                 batchInfo.blendMode == BlendMode::Additive ||
                 batchInfo.blendMode == BlendMode::Multiply);
            renderable->SetTransparentHint(transparentHint);
            const float depthHint = static_cast<float>((batchInfo.screenSpace ? (std::numeric_limits<int32_t>::max() - m_batcher.GetBatchSortOrder(i))
                                                                             : static_cast<int32_t>(batchCount - i)));
            renderable->SetDepthHint(depthHint);
        } else {
            renderable->MarkMaterialSortKeyDirty();
        }
        renderable->SetVisible(true);
        renderable->SubmitToRenderer(m_renderer);
    }
    m_lastBatchCount = batchCount;
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
    if (!m_world || !m_renderer) {
        return;
    }

    auto& lightManager = m_renderer->GetLightManager();
    auto visibleLights = GetVisibleLights();

    if (visibleLights.empty()) {
        for (auto& entry : m_lightHandles) {
            if (entry.second != Lighting::InvalidLightHandle) {
                lightManager.RemoveLight(entry.second);
            }
        }
        m_lightHandles.clear();
        m_primaryLightPosition = Vector3::Zero();
        m_primaryLightColor = Color::Black();
        m_primaryLightIntensity = 0.0f;
        return;
    }

    std::unordered_set<EntityID, EntityID::Hash> activeLights;
    activeLights.reserve(visibleLights.size());

    auto convertType = [](LightType type) {
        switch (type) {
            case LightType::Directional:
                return Lighting::LightType::Directional;
            case LightType::Point:
                return Lighting::LightType::Point;
            case LightType::Spot:
                return Lighting::LightType::Spot;
            default:
                return Lighting::LightType::Unknown;
        }
    };

    bool primaryAssigned = false;

    for (const auto& entity : visibleLights) {
        activeLights.insert(entity);

        if (!m_world->HasComponent<TransformComponent>(entity)) {
            continue;
        }

        const auto& transformComp = m_world->GetComponent<TransformComponent>(entity);
        const auto& lightComp = m_world->GetComponent<LightComponent>(entity);

        Lighting::LightParameters params{};
        params.type = convertType(lightComp.type);
        if (params.type == Lighting::LightType::Unknown) {
            continue;
        }

        params.common.color = lightComp.color;
        params.common.intensity = lightComp.intensity;
        params.common.castsShadows = lightComp.castShadows;
        params.common.shadowBias = lightComp.shadowBias;
        params.common.enabled = lightComp.enabled;
        params.common.layerID = Layers::World::Midground.value;

        const Vector3 position = transformComp.GetPosition();
        const float attenuationBase = std::max(0.0001f, lightComp.attenuation);
        Vector3 attenuationVector(1.0f, attenuationBase, attenuationBase * attenuationBase);

        switch (params.type) {
            case Lighting::LightType::Directional: {
                Vector3 direction = transformComp.transform ? transformComp.transform->GetForward() : Vector3(0.0f, -1.0f, 0.0f);
                if (direction.squaredNorm() <= 1e-6f) {
                    direction = Vector3(0.0f, -1.0f, 0.0f);
                }
                direction.normalize();
                params.directional.direction = direction;
                break;
            }
            case Lighting::LightType::Point: {
                params.point.position = position;
                params.point.range = lightComp.range;
                params.point.attenuation = attenuationVector;
                break;
            }
            case Lighting::LightType::Spot: {
                Vector3 direction = transformComp.transform ? transformComp.transform->GetForward() : Vector3(0.0f, -1.0f, 0.0f);
                if (direction.squaredNorm() <= 1e-6f) {
                    direction = Vector3(0.0f, -1.0f, 0.0f);
                }
                direction.normalize();
                params.spot.position = position;
                params.spot.direction = direction;
                params.spot.range = lightComp.range;
                params.spot.attenuation = attenuationVector;
                params.spot.innerCutoff = lightComp.innerConeAngle;
                params.spot.outerCutoff = lightComp.outerConeAngle;
                break;
            }
            default:
                break;
        }

        auto handleIt = m_lightHandles.find(entity);
        Lighting::LightHandle handle = Lighting::InvalidLightHandle;

        if (handleIt == m_lightHandles.end()) {
            handle = lightManager.RegisterLight(params);
            if (handle != Lighting::InvalidLightHandle) {
                m_lightHandles.emplace(entity, handle);
            }
        } else {
            handle = handleIt->second;
            if (handle != Lighting::InvalidLightHandle) {
                if (!params.common.enabled) {
                    lightManager.SetLightEnabled(handle, false);
                } else {
                    lightManager.UpdateLight(handle, params);
                    lightManager.SetLightEnabled(handle, true);
                }
            }
        }

        if (!primaryAssigned && params.common.enabled) {
            m_primaryLightPosition = position;
            m_primaryLightColor = lightComp.color;
            m_primaryLightIntensity = lightComp.intensity;
            primaryAssigned = true;
        }
    }

    for (auto it = m_lightHandles.begin(); it != m_lightHandles.end(); ) {
        if (activeLights.find(it->first) == activeLights.end()) {
            if (it->second != Lighting::InvalidLightHandle) {
                lightManager.RemoveLight(it->second);
            }
            it = m_lightHandles.erase(it);
        } else {
            ++it;
        }
    }

    if (!primaryAssigned) {
        m_primaryLightPosition = Vector3::Zero();
        m_primaryLightColor = Color::Black();
        m_primaryLightIntensity = 0.0f;
    }
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
    
    uint32_t layerMask = 0xFFFFFFFFu;
    if (m_world) {
        EntityID mainCameraEntity = m_cameraSystem->GetMainCamera();
        if (mainCameraEntity.IsValid() && m_world->HasComponent<CameraComponent>(mainCameraEntity)) {
            const auto& cameraComp = m_world->GetComponent<CameraComponent>(mainCameraEntity);
            layerMask = cameraComp.layerMask;
        }
    }
    if (m_renderer) {
        m_renderer->SetActiveLayerMask(layerMask);
    }
    
    Camera* camera = m_cameraSystem->GetMainCameraObject();
    if (!camera) {
        return;
    }
    
    // 获取相机矩阵
    Matrix4 viewMatrix = camera->GetViewMatrix();
    Matrix4 projectionMatrix = camera->GetProjectionMatrix();
    Vector3 cameraPos = camera->GetPosition();
    m_lastCameraPosition = cameraPos;
    
    // ✅ 将相机矩阵存储到 RenderState，供 MeshRenderable::Render() 使用
    if (m_renderer) {
        auto renderState = m_renderer->GetRenderState();
        if (renderState) {
            renderState->SetViewMatrix(viewMatrix);
            renderState->SetProjectionMatrix(projectionMatrix);
        }
    }
    
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

    // 处理模型材质的相机 uniform
    auto modelEntities = m_world->Query<ModelComponent>();
    for (const auto& entity : modelEntities) {
        const auto& modelComp = m_world->GetComponent<ModelComponent>(entity);
        if (!modelComp.resourcesLoaded || !modelComp.model) {
            continue;
        }

        modelComp.model->AccessParts([&](const std::vector<ModelPart>& parts) {
            for (const auto& part : parts) {
                if (!part.material || !part.material->IsValid()) {
                    continue;
                }

                auto shader = part.material->GetShader();
                if (!shader) {
                    continue;
                }

                Shader* shaderPtr = shader.get();
                if (processedShaders.count(shaderPtr)) {
                    continue;
                }

                if (!shader->IsValid()) {
                    Logger::GetInstance().WarningFormat(
                        "[UniformSystem] Shader is invalid (model), skipping camera uniforms");
                    continue;
                }

                try {
                    shader->Use();
                    if (auto uniformMgr = shader->GetUniformManager()) {
                        if (uniformMgr->HasUniform("uView")) {
                            uniformMgr->SetMatrix4("uView", viewMatrix);
                        }
                        if (uniformMgr->HasUniform("uProjection")) {
                            uniformMgr->SetMatrix4("uProjection", projectionMatrix);
                        }
                        if (uniformMgr->HasUniform("uViewPos")) {
                            uniformMgr->SetVector3("uViewPos", cameraPos);
                        }
                    }
                    processedShaders.insert(shaderPtr);
                } catch (const std::exception& e) {
                    Logger::GetInstance().ErrorFormat(
                        "[UniformSystem] Exception setting model camera uniforms: %s", e.what());
                }
            }
        });
    }
    
    static bool firstLog = true;
    if (firstLog && !processedShaders.empty()) {
        Logger::GetInstance().InfoFormat("[UniformSystem] Set camera uniforms for %zu shaders", 
                                        processedShaders.size());
        firstLog = false;
    }
}

void UniformSystem::SetLightUniforms() {
    if (!m_renderer) {
        return;
    }

    Vector3 cameraPos = m_lastCameraPosition;
    if (m_cameraSystem) {
        if (Camera* camera = m_cameraSystem->GetMainCameraObject()) {
            cameraPos = camera->GetPosition();
            m_lastCameraPosition = cameraPos;
        }
    }

    auto& lightManager = m_renderer->GetLightManager();
    Lighting::LightingFrameSnapshot snapshot = lightManager.BuildFrameSnapshot(cameraPos);

    const uint32_t directionalCount = static_cast<uint32_t>(snapshot.directionalLights.size());
    const uint32_t pointCount = static_cast<uint32_t>(snapshot.pointLights.size());
    const uint32_t spotCount = static_cast<uint32_t>(snapshot.spotLights.size());
    const uint32_t ambientCount = static_cast<uint32_t>(snapshot.ambientLights.size());

    const bool hasAdvancedLights = (directionalCount + pointCount + spotCount + ambientCount) > 0;

    std::vector<Vector4> directionalDirections;
    std::vector<Vector4> directionalColors;
    directionalDirections.reserve(directionalCount);
    directionalColors.reserve(directionalCount);
    for (const auto& light : snapshot.directionalLights) {
        Vector3 direction = light.directional.direction;
        if (direction.squaredNorm() <= 1e-6f) {
            direction = Vector3(0.0f, -1.0f, 0.0f);
        }
        direction.normalize();
        directionalDirections.emplace_back(direction.x(), direction.y(), direction.z(), 0.0f);
        directionalColors.emplace_back(light.common.color.r, light.common.color.g, light.common.color.b, light.common.intensity);
    }

    std::vector<Vector4> pointPositions;
    std::vector<Vector4> pointColors;
    std::vector<Vector3> pointAttenuations;
    pointPositions.reserve(pointCount);
    pointColors.reserve(pointCount);
    pointAttenuations.reserve(pointCount);
    for (const auto& light : snapshot.pointLights) {
        pointPositions.emplace_back(light.point.position.x(), light.point.position.y(), light.point.position.z(), light.point.range);
        pointColors.emplace_back(light.common.color.r, light.common.color.g, light.common.color.b, light.common.intensity);
        pointAttenuations.push_back(light.point.attenuation);
    }

    std::vector<Vector4> spotPositions;
    std::vector<Vector4> spotColors;
    std::vector<Vector4> spotDirections;
    std::vector<Vector3> spotAttenuations;
    std::vector<float> spotInnerCos;
    spotPositions.reserve(spotCount);
    spotColors.reserve(spotCount);
    spotDirections.reserve(spotCount);
    spotAttenuations.reserve(spotCount);
    spotInnerCos.reserve(spotCount);
    for (const auto& light : snapshot.spotLights) {
        spotPositions.emplace_back(light.spot.position.x(), light.spot.position.y(), light.spot.position.z(), light.spot.range);
        spotColors.emplace_back(light.common.color.r, light.common.color.g, light.common.color.b, light.common.intensity);
        Vector3 direction = light.spot.direction;
        if (direction.squaredNorm() <= 1e-6f) {
            direction = Vector3(0.0f, -1.0f, 0.0f);
        }
        direction.normalize();
        spotDirections.emplace_back(direction.x(), direction.y(), direction.z(), light.spot.outerCutoff);
        spotAttenuations.push_back(light.spot.attenuation);
        spotInnerCos.push_back(light.spot.innerCutoff);
    }

    std::vector<Vector4> ambientColors;
    ambientColors.reserve(ambientCount);
    for (const auto& light : snapshot.ambientLights) {
        ambientColors.emplace_back(light.common.color.r, light.common.color.g, light.common.color.b, light.common.intensity * light.ambient.ambience);
    }

    Vector3 legacyLightPos = Vector3::Zero();
    Color legacyLightColor = Color::Black();
    float legacyLightIntensity = 0.0f;
    if (m_lightSystem) {
        legacyLightPos = m_lightSystem->GetPrimaryLightPosition();
        legacyLightColor = m_lightSystem->GetPrimaryLightColor();
        legacyLightIntensity = m_lightSystem->GetPrimaryLightIntensity();
    }

    auto entities = m_world->Query<MeshRenderComponent>();
    std::unordered_set<Shader*> processedShaders;

    for (const auto& entity : entities) {
        const auto& meshComp = m_world->GetComponent<MeshRenderComponent>(entity);
        if (!meshComp.material) {
            continue;
        }
        if (!meshComp.material->IsValid()) {
            continue;
        }

        auto shader = meshComp.material->GetShader();
        if (!shader) {
            continue;
        }

        Shader* shaderPtr = shader.get();
        if (processedShaders.count(shaderPtr)) {
            continue;
        }

        if (!shader->IsValid()) {
            Logger::GetInstance().WarningFormat("[UniformSystem] Shader is invalid, skipping light uniforms");
            continue;
        }

        try {
            shader->Use();
            auto uniformMgr = shader->GetUniformManager();
            if (!uniformMgr) {
                Logger::GetInstance().WarningFormat("[UniformSystem] Shader '%s' has null UniformManager", shader->GetName().c_str());
                continue;
            }

            if (uniformMgr->HasUniform("uLighting.cameraPosition")) {
                uniformMgr->SetVector3("uLighting.cameraPosition", cameraPos);
            }

            if (uniformMgr->HasUniform("uLighting.directionalCount")) {
                uniformMgr->SetInt("uLighting.directionalCount", static_cast<int>(directionalCount));
            }
            if (uniformMgr->HasUniform("uLighting.pointCount")) {
                uniformMgr->SetInt("uLighting.pointCount", static_cast<int>(pointCount));
            }
            if (uniformMgr->HasUniform("uLighting.spotCount")) {
                uniformMgr->SetInt("uLighting.spotCount", static_cast<int>(spotCount));
            }
            if (uniformMgr->HasUniform("uLighting.ambientCount")) {
                uniformMgr->SetInt("uLighting.ambientCount", static_cast<int>(ambientCount));
            }
            if (uniformMgr->HasUniform("uLighting.hasLights")) {
                uniformMgr->SetBool("uLighting.hasLights", hasAdvancedLights);
            }

            if (directionalCount > 0) {
                if (uniformMgr->HasUniform("uLighting.directionalDirections")) {
                    uniformMgr->SetVector4Array("uLighting.directionalDirections", directionalDirections.data(), directionalCount);
                }
                if (uniformMgr->HasUniform("uLighting.directionalColors")) {
                    uniformMgr->SetVector4Array("uLighting.directionalColors", directionalColors.data(), directionalCount);
                }
            }

            if (pointCount > 0) {
                if (uniformMgr->HasUniform("uLighting.pointPositions")) {
                    uniformMgr->SetVector4Array("uLighting.pointPositions", pointPositions.data(), pointCount);
                }
                if (uniformMgr->HasUniform("uLighting.pointColors")) {
                    uniformMgr->SetVector4Array("uLighting.pointColors", pointColors.data(), pointCount);
                }
                if (uniformMgr->HasUniform("uLighting.pointAttenuation")) {
                    uniformMgr->SetVector3Array("uLighting.pointAttenuation", pointAttenuations.data(), pointCount);
                }
            }

            if (spotCount > 0) {
                if (uniformMgr->HasUniform("uLighting.spotPositions")) {
                    uniformMgr->SetVector4Array("uLighting.spotPositions", spotPositions.data(), spotCount);
                }
                if (uniformMgr->HasUniform("uLighting.spotColors")) {
                    uniformMgr->SetVector4Array("uLighting.spotColors", spotColors.data(), spotCount);
                }
                if (uniformMgr->HasUniform("uLighting.spotDirections")) {
                    uniformMgr->SetVector4Array("uLighting.spotDirections", spotDirections.data(), spotCount);
                }
                if (uniformMgr->HasUniform("uLighting.spotAttenuation")) {
                    uniformMgr->SetVector3Array("uLighting.spotAttenuation", spotAttenuations.data(), spotCount);
                }
                if (!spotInnerCos.empty() && uniformMgr->HasUniform("uLighting.spotInnerCos")) {
                    uniformMgr->SetFloatArray("uLighting.spotInnerCos", spotInnerCos.data(), static_cast<uint32_t>(spotInnerCos.size()));
                }
            }

            if (ambientCount > 0 && uniformMgr->HasUniform("uLighting.ambientColors")) {
                uniformMgr->SetVector4Array("uLighting.ambientColors", ambientColors.data(), ambientCount);
            }

            if (uniformMgr->HasUniform("uLighting.culledDirectional")) {
                uniformMgr->SetInt("uLighting.culledDirectional", static_cast<int>(snapshot.culledDirectional));
            }
            if (uniformMgr->HasUniform("uLighting.culledPoint")) {
                uniformMgr->SetInt("uLighting.culledPoint", static_cast<int>(snapshot.culledPoint));
            }
            if (uniformMgr->HasUniform("uLighting.culledSpot")) {
                uniformMgr->SetInt("uLighting.culledSpot", static_cast<int>(snapshot.culledSpot));
            }
            if (uniformMgr->HasUniform("uLighting.culledAmbient")) {
                uniformMgr->SetInt("uLighting.culledAmbient", static_cast<int>(snapshot.culledAmbient));
            }

            // Legacy single-light uniforms for backward compatibility
            if (uniformMgr->HasUniform("uLightPos")) {
                uniformMgr->SetVector3("uLightPos", legacyLightPos);
            }
            if (uniformMgr->HasUniform("uLightColor")) {
                uniformMgr->SetColor("uLightColor", legacyLightColor);
            }
            if (uniformMgr->HasUniform("uLightIntensity")) {
                uniformMgr->SetFloat("uLightIntensity", legacyLightIntensity);
            }
            if (uniformMgr->HasUniform("uAmbientColor") && !hasAdvancedLights) {
                Color ambientColor = legacyLightColor;
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


