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


