#include "render/application/scene_manager.h"

#include <algorithm>
#include <optional>
#include <unordered_set>
#include <utility>

#include "render/application/module_registry.h"
#include "render/logger.h"
#include "render/resource_manager.h"
#include "render/shader_cache.h"

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
    Logger::GetInstance().Info("[SceneManager] Update begin");
    ProcessPendingTransition();
    ProcessPreloadStates();

    if (m_sceneStack.empty()) {
        Logger::GetInstance().Info("[SceneManager] No active scene");
        return;
    }

    auto& activeEntry = m_sceneStack.back();
    if (activeEntry.scene && activeEntry.entered) {
        Logger::GetInstance().InfoFormat("[SceneManager] Updating scene '%s'",
                                          activeEntry.id.c_str());
        activeEntry.scene->OnUpdate(frameArgs);
    }
    Logger::GetInstance().Info("[SceneManager] Update end");
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
    storedEntry.manifest = storedEntry.scene->BuildManifest();
    EmitManifestEvent(storedEntry.id, storedEntry.manifest);

    BeginPreload(storedEntry, transition.args);
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
    EmitLifecycleEvent(entry.id, Events::SceneLifecycleEvent::Stage::Detached, entry.flags, nullptr, nullptr, nullptr);
}

void SceneManager::BeginPreload(SceneStackEntry& entry, SceneEnterArgs& args) {
    entry.pendingEnterArgs = std::move(args);
    entry.preload = SceneStackEntry::PreloadState{};
    entry.preload.requiredTotal = entry.manifest.required.size();
    entry.preload.optionalTotal = entry.manifest.optional.size();

    UpdatePreloadState(entry);

    if (entry.preload.completed && entry.pendingEnterArgs.has_value()) {
        EnterScene(entry, std::move(*entry.pendingEnterArgs));
        entry.pendingEnterArgs.reset();
    }
}

void SceneManager::ProcessPreloadStates() {
    if (m_sceneStack.empty()) {
        return;
    }
    Logger::GetInstance().Info("[SceneManager] ProcessPreloadStates");
    for (auto& entry : m_sceneStack) {
        if (!entry.scene || entry.entered) {
            continue;
        }

        UpdatePreloadState(entry);

        if (entry.preload.completed && entry.pendingEnterArgs.has_value()) {
            EnterScene(entry, std::move(*entry.pendingEnterArgs));
            entry.pendingEnterArgs.reset();
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
    EmitLifecycleEvent(entry.id, Events::SceneLifecycleEvent::Stage::Entering, entry.flags, &args, nullptr, nullptr);
    if (entry.scene) {
        entry.scene->OnEnter(args);
    }
    entry.entered = true;
    entry.pendingEnterArgs.reset();
    EmitLifecycleEvent(entry.id, Events::SceneLifecycleEvent::Stage::Entered, entry.flags, &args, nullptr, nullptr);
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
    if (!m_appContext || !m_appContext->globalEventBus) {
        return;
    }

    Events::SceneLifecycleEvent event;
    event.sceneId = sceneId;
    event.stage = stage;
    event.flags = flags;
    if (enterArgs) {
        event.enterArgs = *enterArgs;
    }
    if (exitArgs) {
        event.exitArgs = *exitArgs;
    }
    if (snapshot) {
        event.snapshot = *snapshot;
    }

    m_appContext->globalEventBus->Publish(event);
}

} // namespace Render::Application


