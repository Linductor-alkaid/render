#pragma once

#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "render/application/app_context.h"
#include "render/application/scene.h"
#include "render/application/scene_types.h"
#include "render/application/events/scene_events.h"

namespace Render {

namespace ECS {
class World;
}

namespace Application {

class ModuleRegistry;

class SceneManager {
public:
    SceneManager();
    ~SceneManager();

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    void Initialize(AppContext* appContext, ModuleRegistry* modules);
    void Shutdown();

    void RegisterSceneFactory(std::string sceneId, SceneFactory factory);
    bool HasSceneFactory(std::string_view sceneId) const;

    bool PushScene(std::string_view sceneId, SceneEnterArgs args = {});
    bool ReplaceScene(std::string_view sceneId, SceneEnterArgs args = {});
    std::optional<SceneSnapshot> PopScene(SceneExitArgs args = {});

    void Update(const FrameUpdateArgs& frameArgs);

    Scene* GetActiveScene() noexcept;
    const Scene* GetActiveScene() const noexcept;

    [[nodiscard]] bool IsTransitionInProgress() const noexcept;
    [[nodiscard]] size_t SceneCount() const noexcept;

private:
    struct SceneStackEntry {
        std::string id;
        ScenePtr scene;
        SceneSnapshot lastSnapshot;
        SceneFlags flags = SceneFlags::None;
        bool attached = false;
        bool entered = false;
        SceneResourceManifest manifest;
        std::optional<SceneEnterArgs> pendingEnterArgs;

        struct PreloadState {
            size_t requiredReady = 0;
            size_t optionalReady = 0;
            size_t requiredTotal = 0;
            size_t optionalTotal = 0;
            bool completed = false;
            bool failed = false;
            size_t lastReportedRequiredReady = std::numeric_limits<size_t>::max();
            size_t lastReportedOptionalReady = std::numeric_limits<size_t>::max();
            bool lastReportedCompleted = false;
            bool lastReportedFailed = false;
            size_t lastReportedMissingRequired = std::numeric_limits<size_t>::max();
            size_t lastReportedMissingOptional = std::numeric_limits<size_t>::max();
            std::vector<ResourceRequest> missingRequired;
            std::vector<ResourceRequest> missingOptional;
            // 跟踪已提交的加载任务，避免重复提交
            std::unordered_set<std::string> pendingLoadTasks;  // key: "type:identifier"
        } preload;
    };

    struct PendingTransition {
        std::string targetId;
        SceneEnterArgs args;
        enum class Type { Push, Replace } type = Type::Push;
    };

    ScenePtr CreateSceneInstance(const std::string& sceneId);
    void ProcessPendingTransition();
    void AttachScene(SceneStackEntry& entry);
    void DetachScene(SceneStackEntry& entry);
    void BeginPreload(SceneStackEntry& entry, SceneEnterArgs& args);
    void ProcessPreloadStates();
    void UpdatePreloadState(SceneStackEntry& entry);
    void BeginAsyncLoad(SceneStackEntry& entry, const ResourceRequest& request);
    void EnterScene(SceneStackEntry& entry, SceneEnterArgs&& args);
    void ReleaseSceneResources(SceneStackEntry& entry);

    enum class ResourceAvailability {
        Available,
        Missing,
        Unsupported
    };

    ResourceAvailability CheckResourceAvailability(const ResourceRequest& request) const;
    void EmitTransitionEvent(const std::string& sceneId, Events::SceneTransitionEvent::Type type,
                             const SceneEnterArgs* enterArgs = nullptr, const SceneExitArgs* exitArgs = nullptr);
    void EmitManifestEvent(const std::string& sceneId, const SceneResourceManifest& manifest);
    void EmitLifecycleEvent(const std::string& sceneId, Events::SceneLifecycleEvent::Stage stage, SceneFlags flags,
                            const SceneEnterArgs* enterArgs = nullptr, const SceneExitArgs* exitArgs = nullptr,
                            const SceneSnapshot* snapshot = nullptr);
    void EmitPreloadProgressEvent(const std::string& sceneId, const SceneStackEntry::PreloadState& state);

    AppContext* m_appContext = nullptr;
    ModuleRegistry* m_modules = nullptr;

    std::vector<SceneStackEntry> m_sceneStack;
    std::optional<PendingTransition> m_pendingTransition;
    std::unordered_map<std::string, SceneFactory> m_factories;
};

} // namespace Application
} // namespace Render


