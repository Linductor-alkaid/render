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

#include <functional>
#include <memory>
#include <string_view>

#include "render/application/app_context.h"
#include "render/application/scene_types.h"

namespace Render {

namespace ECS {
class World;
}

namespace Application {

class ModuleRegistry;

class Scene {
public:
    virtual ~Scene() = default;

    virtual std::string_view Name() const = 0;

    virtual void OnAttach(AppContext& ctx, ModuleRegistry& modules) = 0;
    virtual void OnDetach(AppContext& ctx) = 0;

    virtual SceneResourceManifest BuildManifest() const = 0;

    virtual void OnEnter(const SceneEnterArgs& args) = 0;
    virtual void OnUpdate(const FrameUpdateArgs& frame) = 0;
    virtual SceneSnapshot OnExit(const SceneExitArgs& args) = 0;

    virtual bool WantsOverlay() const { return false; }
    virtual SceneFlags DefaultFlags() const { return SceneFlags::None; }
};

using ScenePtr = std::unique_ptr<Scene>;
using SceneFactory = std::function<ScenePtr()>;

} // namespace Application
} // namespace Render


