/*
 * Copyright (c) 2025 Li Chaoyu
 * 
 * This file is part of Render.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * For commercial licensing, please contact: 2052046346@qq.com
 */
#include "render/application/scene_manager.h"

#include <algorithm>
#include <optional>
#include <unordered_set>
#include <utility>
#include <sstream>

#include "render/application/module_registry.h"
#include "render/application/scene_serializer.h"
#include "render/logger.h"
#include "render/resource_manager.h"
#include "render/shader_cache.h"
#include "render/async_resource_loader.h"
#include "render/model_loader.h"

namespace Render::Application {

SceneManager::SceneManager() = default;

SceneManager::~SceneManager() {
    Shutdown();
}

void SceneManager::Initialize(AppContext* appContext, ModuleRegistry* modules) {
    m_appContext = appContext;
    m_modules = modules;
}

void SceneManager::Shutdown() {
    while (!m_sceneStack.empty()) {
        SceneExitArgs args;
        PopScene(args);
    }
    m_factories.clear();
    m_pendingTransition.reset();
    m_appContext = nullptr;
    m_modules = nullptr;
}

void SceneManager::RegisterSceneFactory(std::string sceneId, SceneFactory factory) {
    m_factories[std::move(sceneId)] = std::move(factory);
}

bool SceneManager::HasSceneFactory(std::string_view sceneId) const {
    return m_factories.find(std::string(sceneId)) != m_factories.end();
}

bool SceneManager::PushScene(std::string_view sceneId, SceneEnterArgs args) {
    if (!HasSceneFactory(sceneId)) {
        Logger::GetInstance().Error("SceneManager::PushScene - unknown scene: " + std::string(sceneId));
        return false;
    }

    std::string id(sceneId);
    EmitTransitionEvent(id, Events::SceneTransitionEvent::Type::Push, &args, nullptr);
    m_pendingTransition = PendingTransition{std::move(id), std::move(args), PendingTransition::Type::Push};
    return true;
}

bool SceneManager::ReplaceScene(std::string_view sceneId, SceneEnterArgs args) {
    if (!HasSceneFactory(sceneId)) {
        Logger::GetInstance().Error("SceneManager::ReplaceScene - unknown scene: " + std::string(sceneId));
        return false;
    }

    std::string id(sceneId);
    EmitTransitionEvent(id, Events::SceneTransitionEvent::Type::Replace, &args, nullptr);
    m_pendingTransition = PendingTransition{std::move(id), std::move(args), PendingTransition::Type::Replace};
    return true;
}

std::optional<SceneSnapshot> SceneManager::PopScene(SceneExitArgs args) {
    if (m_sceneStack.empty()) {
        return std::nullopt;
    }

    auto entry = std::move(m_sceneStack.back());
    m_sceneStack.pop_back();

    EmitTransitionEvent(entry.id, Events::SceneTransitionEvent::Type::Pop, nullptr, &args);

    std::optional<SceneSnapshot> snapshot;
    if (entry.scene) {
        if (entry.entered) {
            EmitLifecycleEvent(entry.id, Events::SceneLifecycleEvent::Stage::Exiting, entry.flags, nullptr, &args,
                               nullptr);
            snapshot = entry.scene->OnExit(args);
            if (snapshot.has_value()) {
                entry.lastSnapshot = snapshot.value();
                EmitLifecycleEvent(entry.id, Events::SceneLifecycleEvent::Stage::Exited, entry.flags, nullptr, &args,
                                   &snapshot.value());
            } else {
                EmitLifecycleEvent(entry.id, Events::SceneLifecycleEvent::Stage::Exited, entry.flags, nullptr, &args,
                                   nullptr);
            }
        }
        DetachScene(entry);
    }

    return snapshot;
}

void SceneManager::Update(const FrameUpdateArgs& frameArgs) {
    ProcessPendingTransition();
    ProcessPreloadStates();

    if (m_sceneStack.empty()) {
        return;
    }

    auto& activeEntry = m_sceneStack.back();
    if (activeEntry.scene && activeEntry.entered) {
        activeEntry.scene->OnUpdate(frameArgs);
    }
}

Scene* SceneManager::GetActiveScene() noexcept {
    if (m_sceneStack.empty()) {
        return nullptr;
    }
    return m_sceneStack.back().scene.get();
}

const Scene* SceneManager::GetActiveScene() const noexcept {
    if (m_sceneStack.empty()) {
        return nullptr;
    }
    return m_sceneStack.back().scene.get();
}

bool SceneManager::IsTransitionInProgress() const noexcept {
    return m_pendingTransition.has_value();
}

size_t SceneManager::SceneCount() const noexcept {
    return m_sceneStack.size();
}

ScenePtr SceneManager::CreateSceneInstance(const std::string& sceneId) {
    auto it = m_factories.find(sceneId);
    if (it == m_factories.end()) {
        return nullptr;
    }
    return it->second();
}

void SceneManager::ProcessPendingTransition() {
    if (!m_pendingTransition.has_value()) {
        return;
    }

    Logger::GetInstance().InfoFormat("[SceneManager] Processing pending transition to '%s'",
                                      m_pendingTransition->targetId.c_str());

    auto transition = std::move(*m_pendingTransition);
    m_pendingTransition.reset();

    ScenePtr scene = CreateSceneInstance(transition.targetId);
    if (!scene) {
        Logger::GetInstance().Error("SceneManager::ProcessPendingTransition - failed to create scene: " + transition.targetId);
        return;
    }
    Logger::GetInstance().InfoFormat("[SceneManager] Created scene instance '%s'", transition.targetId.c_str());

    if (transition.type == PendingTransition::Type::Replace) {
        SceneExitArgs exitArgs;
        auto snapshot = PopScene(exitArgs);
        if (snapshot.has_value()) {
            transition.args.previousSnapshot = std::move(snapshot);
        }
    }

    SceneStackEntry entry;
    entry.id = transition.targetId;
    entry.scene = std::move(scene);
    entry.flags = entry.scene->DefaultFlags();

    m_sceneStack.push_back(std::move(entry));
    auto& storedEntry = m_sceneStack.back();

    AttachScene(storedEntry);
    
    // 只有在场景成功attach后才构建manifest和开始预加载
    if (storedEntry.attached && storedEntry.scene) {
        storedEntry.manifest = storedEntry.scene->BuildManifest();
        EmitManifestEvent(storedEntry.id, storedEntry.manifest);
        BeginPreload(storedEntry, transition.args);
    } else {
        Logger::GetInstance().ErrorFormat(
            "[SceneManager] Failed to attach scene '%s', skipping manifest and preload",
            storedEntry.id.c_str());
    }
}

void SceneManager::AttachScene(SceneStackEntry& entry) {
    if (!m_appContext || !m_modules || entry.attached || !entry.scene) {
        return;
    }

    entry.scene->OnAttach(*m_appContext, *m_modules);
    entry.attached = true;
    EmitLifecycleEvent(entry.id, Events::SceneLifecycleEvent::Stage::Attached, entry.flags, nullptr, nullptr, nullptr);
}

void SceneManager::DetachScene(SceneStackEntry& entry) {
    if (!entry.attached || !entry.scene || !m_appContext) {
        return;
    }

    entry.scene->OnDetach(*m_appContext);
    entry.attached = false;
    
    // 释放场景资源（根据ResourceScope区分）
    ReleaseSceneResources(entry);
    
    EmitLifecycleEvent(entry.id, Events::SceneLifecycleEvent::Stage::Detached, entry.flags, nullptr, nullptr, nullptr);
}

void SceneManager::BeginPreload(SceneStackEntry& entry, SceneEnterArgs& args) {
    entry.pendingEnterArgs = std::move(args);
    entry.preload = SceneStackEntry::PreloadState{};
    entry.preload.requiredTotal = entry.manifest.required.size();
    entry.preload.optionalTotal = entry.manifest.optional.size();

    UpdatePreloadState(entry);

    if (entry.preload.completed && entry.pendingEnterArgs.has_value() && !entry.entered) {
        try {
            EnterScene(entry, std::move(*entry.pendingEnterArgs));
            // EnterScene内部会重置pendingEnterArgs，这里不需要再次重置
        } catch (const std::exception& ex) {
            Logger::GetInstance().ErrorFormat(
                "[SceneManager] Failed to enter scene during BeginPreload: %s",
                ex.what());
            // 保留pendingEnterArgs，以便稍后重试
        }
    }
}

void SceneManager::ProcessPreloadStates() {
    if (m_sceneStack.empty()) {
        return;
    }
    for (auto& entry : m_sceneStack) {
        if (!entry.scene || !entry.attached || entry.entered) {
            continue;
        }

        UpdatePreloadState(entry);

        if (entry.preload.completed && entry.pendingEnterArgs.has_value() && !entry.entered) {
            try {
                EnterScene(entry, std::move(*entry.pendingEnterArgs));
                // EnterScene内部会重置pendingEnterArgs，这里不需要再次重置
            } catch (const std::exception& ex) {
                Logger::GetInstance().ErrorFormat(
                    "[SceneManager] Failed to enter scene during ProcessPreloadStates: %s",
                    ex.what());
                // 保留pendingEnterArgs，以便稍后重试
            }
        }
    }
}

void SceneManager::UpdatePreloadState(SceneStackEntry& entry) {
    auto& state = entry.preload;
    state.requiredTotal = entry.manifest.required.size();
    state.optionalTotal = entry.manifest.optional.size();

    state.requiredReady = 0;
    state.optionalReady = 0;
    state.missingRequired.clear();
    state.missingOptional.clear();
    state.failed = false;

    for (const auto& request : entry.manifest.required) {
        switch (CheckResourceAvailability(request)) {
        case ResourceAvailability::Available:
            state.requiredReady++;
            break;
        case ResourceAvailability::Missing:
            state.missingRequired.push_back(request);
            // 主动触发异步加载
            BeginAsyncLoad(entry, request);
            break;
        case ResourceAvailability::Unsupported:
            state.requiredReady++;
            break;
        }
    }

    for (const auto& request : entry.manifest.optional) {
        switch (CheckResourceAvailability(request)) {
        case ResourceAvailability::Available:
            state.optionalReady++;
            break;
        case ResourceAvailability::Missing:
            state.missingOptional.push_back(request);
            // 可选资源也触发异步加载（低优先级）
            BeginAsyncLoad(entry, request);
            break;
        case ResourceAvailability::Unsupported:
            state.optionalReady++;
            break;
        }
    }

    state.completed = (state.requiredReady >= state.requiredTotal);

    bool shouldEmit = false;
    if (state.lastReportedRequiredReady == std::numeric_limits<size_t>::max() ||
        state.requiredReady != state.lastReportedRequiredReady ||
        state.optionalReady != state.lastReportedOptionalReady ||
        state.completed != state.lastReportedCompleted ||
        state.failed != state.lastReportedFailed ||
        state.missingRequired.size() != state.lastReportedMissingRequired ||
        state.missingOptional.size() != state.lastReportedMissingOptional) {
        shouldEmit = true;
    }

    if (shouldEmit) {
        EmitPreloadProgressEvent(entry.id, state);
        state.lastReportedRequiredReady = state.requiredReady;
        state.lastReportedOptionalReady = state.optionalReady;
        state.lastReportedCompleted = state.completed;
        state.lastReportedFailed = state.failed;
        state.lastReportedMissingRequired = state.missingRequired.size();
        state.lastReportedMissingOptional = state.missingOptional.size();
    }
}

void SceneManager::EnterScene(SceneStackEntry& entry, SceneEnterArgs&& args) {
    // 检查场景是否已经进入，避免重复进入
    if (entry.entered) {
        Logger::GetInstance().WarningFormat(
            "[SceneManager] Scene '%s' already entered, skipping EnterScene",
            entry.id.c_str());
        return;
    }
    
    // 检查场景和上下文是否有效
    if (!entry.scene) {
        Logger::GetInstance().ErrorFormat(
            "[SceneManager] Cannot enter scene '%s': scene is null",
            entry.id.c_str());
        return;
    }
    
    if (!entry.attached) {
        Logger::GetInstance().ErrorFormat(
            "[SceneManager] Cannot enter scene '%s': scene is not attached",
            entry.id.c_str());
        return;
    }
    
    Logger::GetInstance().InfoFormat("[SceneManager] Entering scene '%s'", entry.id.c_str());
    
    // 在OnEnter调用前保存args的副本，因为OnEnter可能会修改args
    SceneEnterArgs argsCopy = args;
    
    EmitLifecycleEvent(entry.id, Events::SceneLifecycleEvent::Stage::Entering, entry.flags, &argsCopy, nullptr, nullptr);
    
    try {
        entry.scene->OnEnter(args);
        entry.entered = true;
        entry.pendingEnterArgs.reset();
        Logger::GetInstance().InfoFormat("[SceneManager] Scene '%s' entered successfully", entry.id.c_str());
        
        try {
            // 使用之前保存的argsCopy，而不是可能被修改的args
            EmitLifecycleEvent(entry.id, Events::SceneLifecycleEvent::Stage::Entered, entry.flags, &argsCopy, nullptr, nullptr);
        } catch (const std::exception& emitEx) {
            Logger::GetInstance().ErrorFormat(
                "[SceneManager] Exception while emitting Entered event for scene '%s': %s",
                entry.id.c_str(), emitEx.what());
            throw; // 重新抛出异常
        } catch (...) {
            Logger::GetInstance().ErrorFormat(
                "[SceneManager] Unknown exception while emitting Entered event for scene '%s'",
                entry.id.c_str());
            throw; // 重新抛出异常
        }
    } catch (const std::exception& ex) {
        Logger::GetInstance().ErrorFormat(
            "[SceneManager] Exception while entering scene '%s': %s",
            entry.id.c_str(), ex.what());
        // 不设置 entry.entered = true，让场景保持未进入状态
        throw; // 重新抛出异常，让调用者处理
    } catch (...) {
        Logger::GetInstance().ErrorFormat(
            "[SceneManager] Unknown exception while entering scene '%s'",
            entry.id.c_str());
        // 不设置 entry.entered = true，让场景保持未进入状态
        throw; // 重新抛出异常，让调用者处理
    }
}

SceneManager::ResourceAvailability SceneManager::CheckResourceAvailability(const ResourceRequest& request) const {
    static std::unordered_set<std::string> s_loggedUnsupportedTypes;
    static bool s_warnedMissingResourceManager = false;

    if (request.type == "shader") {
        auto& shaderCache = ShaderCache::GetInstance();
        return shaderCache.HasShader(request.identifier) ? ResourceAvailability::Available
                                                         : ResourceAvailability::Missing;
    }

    if (!m_appContext || !m_appContext->resourceManager) {
        if (!s_warnedMissingResourceManager) {
            Logger::GetInstance().Warning(
                "SceneManager::CheckResourceAvailability - ResourceManager not available in AppContext");
            s_warnedMissingResourceManager = true;
        }
        return ResourceAvailability::Missing;
    }

    auto& resMgr = *m_appContext->resourceManager;

    if (request.type == "mesh") {
        return resMgr.HasMesh(request.identifier) ? ResourceAvailability::Available : ResourceAvailability::Missing;
    }
    if (request.type == "material") {
        return resMgr.HasMaterial(request.identifier) ? ResourceAvailability::Available : ResourceAvailability::Missing;
    }
    if (request.type == "texture") {
        return resMgr.HasTexture(request.identifier) ? ResourceAvailability::Available : ResourceAvailability::Missing;
    }
    if (request.type == "model") {
        return resMgr.HasModel(request.identifier) ? ResourceAvailability::Available : ResourceAvailability::Missing;
    }
    if (request.type == "sprite_atlas") {
        return resMgr.HasSpriteAtlas(request.identifier) ? ResourceAvailability::Available
                                                         : ResourceAvailability::Missing;
    }
    if (request.type == "font") {
        return resMgr.HasFont(request.identifier) ? ResourceAvailability::Available : ResourceAvailability::Missing;
    }

    if (s_loggedUnsupportedTypes.insert(request.type).second) {
        Logger::GetInstance().WarningFormat(
            "SceneManager::CheckResourceAvailability - unsupported resource type '%s', treating as available",
            request.type.c_str());
    }

    return ResourceAvailability::Unsupported;
}

void SceneManager::EmitPreloadProgressEvent(const std::string& sceneId,
                                            const SceneStackEntry::PreloadState& state) {
    if (!m_appContext || !m_appContext->globalEventBus) {
        return;
    }

    Events::ScenePreloadProgressEvent event;
    event.sceneId = sceneId;
    event.requiredLoaded = state.requiredReady;
    event.requiredTotal = state.requiredTotal;
    event.optionalLoaded = state.optionalReady;
    event.optionalTotal = state.optionalTotal;
    event.completed = state.completed;
    event.failed = state.failed;
    event.missingRequired = state.missingRequired;
    event.missingOptional = state.missingOptional;

    m_appContext->globalEventBus->Publish(event);
}

void SceneManager::EmitTransitionEvent(const std::string& sceneId, Events::SceneTransitionEvent::Type type,
                                       const SceneEnterArgs* enterArgs, const SceneExitArgs* exitArgs) {
    if (!m_appContext || !m_appContext->globalEventBus) {
        return;
    }

    Events::SceneTransitionEvent event;
    event.sceneId = sceneId;
    event.type = type;
    if (enterArgs) {
        event.enterArgs = *enterArgs;
    }
    if (exitArgs) {
        event.exitArgs = *exitArgs;
    }

    m_appContext->globalEventBus->Publish(event);
}

void SceneManager::EmitManifestEvent(const std::string& sceneId, const SceneResourceManifest& manifest) {
    if (!m_appContext || !m_appContext->globalEventBus) {
        return;
    }

    Events::SceneManifestEvent event;
    event.sceneId = sceneId;
    event.manifest = manifest;
    m_appContext->globalEventBus->Publish(event);
}

void SceneManager::EmitLifecycleEvent(const std::string& sceneId, Events::SceneLifecycleEvent::Stage stage,
                                      SceneFlags flags, const SceneEnterArgs* enterArgs, const SceneExitArgs* exitArgs,
                                      const SceneSnapshot* snapshot) {
    if (!m_appContext) {
        Logger::GetInstance().Warning("[SceneManager] Cannot emit lifecycle event: m_appContext is null");
        return;
    }
    
    if (!m_appContext->globalEventBus) {
        Logger::GetInstance().Warning("[SceneManager] Cannot emit lifecycle event: globalEventBus is null");
        return;
    }

    try {
        Events::SceneLifecycleEvent event;
        event.sceneId = sceneId;
        event.stage = stage;
        event.flags = flags;
        
        if (enterArgs) {
            try {
                // 先创建一个临时副本，避免直接赋值可能的问题
                SceneEnterArgs tempArgs = *enterArgs;
                event.enterArgs = std::move(tempArgs);
            } catch (const std::exception& ex) {
                Logger::GetInstance().ErrorFormat("[SceneManager] Exception while copying enterArgs: %s", ex.what());
                throw;
            } catch (...) {
                Logger::GetInstance().ErrorFormat("[SceneManager] Unknown exception while copying enterArgs");
                throw;
            }
        }
        if (exitArgs) {
            event.exitArgs = *exitArgs;
        }
        if (snapshot) {
            event.snapshot = *snapshot;
        }

        m_appContext->globalEventBus->Publish(event);
    } catch (const std::exception& ex) {
        Logger::GetInstance().ErrorFormat(
            "[SceneManager] Exception while emitting lifecycle event for scene '%s' stage %d: %s",
            sceneId.c_str(), static_cast<int>(stage), ex.what());
        // 不重新抛出异常，避免中断场景流程
    } catch (...) {
        Logger::GetInstance().ErrorFormat(
            "[SceneManager] Unknown exception while emitting lifecycle event for scene '%s' stage %d",
            sceneId.c_str(), static_cast<int>(stage));
        // 不重新抛出异常，避免中断场景流程
    }
}

void SceneManager::BeginAsyncLoad(SceneStackEntry& entry, const ResourceRequest& request) {
    // 检查AsyncResourceLoader是否可用
    if (!m_appContext || !m_appContext->asyncLoader) {
        Logger::GetInstance().WarningFormat(
            "[SceneManager] Cannot begin async load for resource '%s': AsyncResourceLoader not available",
            request.identifier.c_str());
        return;
    }

    // 生成任务键，避免重复提交
    std::string taskKey = request.type + ":" + request.identifier;
    if (entry.preload.pendingLoadTasks.find(taskKey) != entry.preload.pendingLoadTasks.end()) {
        // 任务已提交，跳过
        return;
    }

    auto& loader = *m_appContext->asyncLoader;
    
    // 根据资源类型调用相应的加载方法
    // 优先级：必需资源 > 可选资源
    float priority = request.optional ? 1.0f : 10.0f;
    
    // 使用identifier作为文件路径（假设identifier就是文件路径）
    std::string filepath = request.identifier;
    std::string name = request.identifier;  // 使用identifier作为资源名称

    // 使用entry.id而不是引用，避免lambda执行时entry已被销毁
    std::string sceneId = entry.id;
    
    if (request.type == "mesh") {
        auto task = loader.LoadMeshAsync(
            filepath,
            name,
            [this, sceneId, taskKey](const MeshLoadResult& result) {
                // 加载完成回调：注册到ResourceManager
                if (result.IsSuccess() && m_appContext && m_appContext->resourceManager) {
                    auto& resMgr = *m_appContext->resourceManager;
                    resMgr.RegisterMesh(result.name, result.resource);
                    Logger::GetInstance().InfoFormat(
                        "[SceneManager] Mesh loaded and registered: %s (scene: %s)",
                        result.name.c_str(),
                        sceneId.c_str());
                    
                    // 从pendingLoadTasks中移除（需要找到对应的entry）
                    for (auto& stackEntry : m_sceneStack) {
                        if (stackEntry.id == sceneId) {
                            stackEntry.preload.pendingLoadTasks.erase(taskKey);
                            break;
                        }
                    }
                } else {
                    Logger::GetInstance().ErrorFormat(
                        "[SceneManager] Mesh load failed: %s - %s (scene: %s)",
                        result.name.c_str(),
                        result.errorMessage.c_str(),
                        sceneId.c_str());
                    // 即使失败也要从pendingLoadTasks中移除
                    for (auto& stackEntry : m_sceneStack) {
                        if (stackEntry.id == sceneId) {
                            stackEntry.preload.pendingLoadTasks.erase(taskKey);
                            break;
                        }
                    }
                }
            },
            priority
        );
        entry.preload.pendingLoadTasks.insert(taskKey);
        Logger::GetInstance().InfoFormat(
            "[SceneManager] Submitted async mesh load: %s (priority: %.1f)",
            filepath.c_str(), priority);
    }
    else if (request.type == "texture") {
        auto task = loader.LoadTextureAsync(
            filepath,
            name,
            true,  // generateMipmap
            [this, sceneId, taskKey](const TextureLoadResult& result) {
                if (result.IsSuccess() && m_appContext && m_appContext->resourceManager) {
                    auto& resMgr = *m_appContext->resourceManager;
                    resMgr.RegisterTexture(result.name, result.resource);
                    Logger::GetInstance().InfoFormat(
                        "[SceneManager] Texture loaded and registered: %s (scene: %s)",
                        result.name.c_str(),
                        sceneId.c_str());
                    
                    for (auto& stackEntry : m_sceneStack) {
                        if (stackEntry.id == sceneId) {
                            stackEntry.preload.pendingLoadTasks.erase(taskKey);
                            break;
                        }
                    }
                } else {
                    Logger::GetInstance().ErrorFormat(
                        "[SceneManager] Texture load failed: %s - %s (scene: %s)",
                        result.name.c_str(),
                        result.errorMessage.c_str(),
                        sceneId.c_str());
                    for (auto& stackEntry : m_sceneStack) {
                        if (stackEntry.id == sceneId) {
                            stackEntry.preload.pendingLoadTasks.erase(taskKey);
                            break;
                        }
                    }
                }
            },
            priority
        );
        entry.preload.pendingLoadTasks.insert(taskKey);
        Logger::GetInstance().InfoFormat(
            "[SceneManager] Submitted async texture load: %s (priority: %.1f)",
            filepath.c_str(), priority);
    }
    else if (request.type == "model") {
        ModelLoadOptions options;
        options.autoUpload = true;
        auto task = loader.LoadModelAsync(
            filepath,
            name,
            options,
            [this, sceneId, taskKey](const ModelLoadResult& result) {
                if (result.IsSuccess() && m_appContext && m_appContext->resourceManager) {
                    Logger::GetInstance().InfoFormat(
                        "[SceneManager] Model loaded: %s (%zu meshes, %zu materials) (scene: %s)",
                        result.name.c_str(),
                        result.meshResourceNames.size(),
                        result.materialResourceNames.size(),
                        sceneId.c_str());
                } else {
                    Logger::GetInstance().ErrorFormat(
                        "[SceneManager] Model load failed: %s - %s (scene: %s)",
                        result.name.c_str(),
                        result.errorMessage.c_str(),
                        sceneId.c_str());
                }
                for (auto& stackEntry : m_sceneStack) {
                    if (stackEntry.id == sceneId) {
                        stackEntry.preload.pendingLoadTasks.erase(taskKey);
                        break;
                    }
                }
            },
            priority
        );
        entry.preload.pendingLoadTasks.insert(taskKey);
        Logger::GetInstance().InfoFormat(
            "[SceneManager] Submitted async model load: %s (priority: %.1f)",
            filepath.c_str(), priority);
    }
    else {
        Logger::GetInstance().WarningFormat(
            "[SceneManager] Unsupported resource type for async loading: %s (identifier: %s)",
            request.type.c_str(),
            request.identifier.c_str());
    }
}

void SceneManager::ReleaseSceneResources(SceneStackEntry& entry) {
    if (!m_appContext || !m_appContext->resourceManager) {
        return;
    }

    auto& resMgr = *m_appContext->resourceManager;
    size_t releasedCount = 0;

    // 遍历manifest中的所有资源，根据ResourceScope决定是否释放
    auto releaseResource = [&](const ResourceRequest& request) {
        if (request.scope == ResourceScope::Scene) {
            // Scene资源：场景退出时释放
            bool released = false;
            if (request.type == "mesh") {
                released = resMgr.RemoveMesh(request.identifier);
            } else if (request.type == "texture") {
                released = resMgr.RemoveTexture(request.identifier);
            } else if (request.type == "material") {
                released = resMgr.RemoveMaterial(request.identifier);
            } else if (request.type == "model") {
                // Model可能包含多个mesh和material，需要特殊处理
                // 这里简化处理，只记录日志
                Logger::GetInstance().InfoFormat(
                    "[SceneManager] Model resource '%s' marked for cleanup (may contain multiple sub-resources)",
                    request.identifier.c_str());
                released = true;  // 假设已处理
            } else if (request.type == "sprite_atlas") {
                released = resMgr.RemoveSpriteAtlas(request.identifier);
            } else if (request.type == "font") {
                released = resMgr.RemoveFont(request.identifier);
            } else if (request.type == "shader") {
                // Shader由ShaderCache管理，这里不直接释放
                Logger::GetInstance().InfoFormat(
                    "[SceneManager] Shader resource '%s' is managed by ShaderCache, skipping release",
                    request.identifier.c_str());
                released = true;  // 假设已处理
            }

            if (released) {
                releasedCount++;
                Logger::GetInstance().DebugFormat(
                    "[SceneManager] Released scene resource: %s (%s)",
                    request.identifier.c_str(),
                    request.type.c_str());
            }
        } else if (request.scope == ResourceScope::Shared) {
            // Shared资源：保留在ResourceManager中，不释放
            Logger::GetInstance().DebugFormat(
                "[SceneManager] Keeping shared resource: %s (%s)",
                request.identifier.c_str(),
                request.type.c_str());
        }
    };

    // 处理必需资源
    for (const auto& request : entry.manifest.required) {
        releaseResource(request);
    }

    // 处理可选资源
    for (const auto& request : entry.manifest.optional) {
        releaseResource(request);
    }

    if (releasedCount > 0) {
        Logger::GetInstance().InfoFormat(
            "[SceneManager] Released %zu scene-specific resources for scene '%s'",
            releasedCount,
            entry.id.c_str());
    }
}

bool SceneManager::SaveActiveScene(const std::string& filePath) {
    if (!m_appContext || !m_appContext->world) {
        Logger::GetInstance().Error("[SceneManager] Cannot save scene: AppContext or World is null");
        return false;
    }

    Scene* activeScene = GetActiveScene();
    if (!activeScene) {
        Logger::GetInstance().Error("[SceneManager] Cannot save scene: no active scene");
        return false;
    }

    SceneSerializer serializer;
    std::string sceneName = std::string(activeScene->Name());
    
    bool success = serializer.SaveScene(*m_appContext->world, sceneName, filePath);
    
    if (success) {
        Logger::GetInstance().InfoFormat(
            "[SceneManager] Successfully saved active scene '%s' to '%s'",
            sceneName.c_str(), filePath.c_str());
    } else {
        Logger::GetInstance().ErrorFormat(
            "[SceneManager] Failed to save active scene '%s' to '%s'",
            sceneName.c_str(), filePath.c_str());
    }

    return success;
}

bool SceneManager::LoadSceneFromFile(const std::string& filePath, SceneEnterArgs args) {
    if (!m_appContext || !m_appContext->world) {
        Logger::GetInstance().Error("[SceneManager] Cannot load scene: AppContext or World is null");
        return false;
    }

    SceneSerializer serializer;
    auto sceneName = serializer.LoadScene(*m_appContext->world, filePath, *m_appContext);
    
    if (!sceneName.has_value()) {
        Logger::GetInstance().ErrorFormat(
            "[SceneManager] Failed to load scene from '%s'",
            filePath.c_str());
        return false;
    }

    Logger::GetInstance().InfoFormat(
        "[SceneManager] Successfully loaded scene '%s' from '%s'",
        sceneName.value().c_str(), filePath.c_str());

    // 注意：从文件加载的场景不会自动推入场景栈
    // 如果需要，可以创建一个临时场景来包装加载的实体
    // 这里暂时只加载实体到World中，不创建Scene对象

    return true;
}

} // namespace Render::Application


