#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "render/application/event_bus.h"
#include "render/application/scene_types.h"

namespace Render::Application::Events {

struct SceneTransitionEvent final : public EventBase {
    enum class Type {
        Push,
        Replace,
        Pop
    };

    std::string sceneId;
    Type type = Type::Push;
    std::optional<SceneEnterArgs> enterArgs;
    std::optional<SceneExitArgs> exitArgs;
};

struct SceneManifestEvent final : public EventBase {
    std::string sceneId;
    SceneResourceManifest manifest;
};

struct SceneLifecycleEvent final : public EventBase {
    enum class Stage {
        Attached,
        Entering,
        Entered,
        Exiting,
        Exited,
        Detached
    };

    std::string sceneId;
    Stage stage = Stage::Attached;
    SceneFlags flags = SceneFlags::None;
    std::optional<SceneEnterArgs> enterArgs;
    std::optional<SceneExitArgs> exitArgs;
    std::optional<SceneSnapshot> snapshot;
};

struct ScenePreloadProgressEvent final : public EventBase {
    std::string sceneId;
    size_t requiredLoaded = 0;
    size_t requiredTotal = 0;
    size_t optionalLoaded = 0;
    size_t optionalTotal = 0;
    bool completed = false;
    bool failed = false;
    std::vector<ResourceRequest> missingRequired;
    std::vector<ResourceRequest> missingOptional;
};

} // namespace Render::Application::Events


