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

#include <string>
#include <string_view>
#include <vector>

#include "render/application/app_context.h"

namespace Render {

namespace ECS {
class World;
}

namespace Application {

enum class ModulePhase {
    Register,
    PreFrame,
    PostFrame,
};

using ModuleDependencies = std::vector<std::string>;

class AppModule {
public:
    virtual ~AppModule() = default;

    virtual std::string_view Name() const = 0;
    virtual ModuleDependencies Dependencies() const { return {}; }

    virtual int Priority(ModulePhase phase) const;

    virtual void OnRegister(ECS::World& world, AppContext& ctx) = 0;
    virtual void OnUnregister(ECS::World& world, AppContext& ctx);

    virtual void OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx);
    virtual void OnPostFrame(const FrameUpdateArgs& frame, AppContext& ctx);
};

} // namespace Application
} // namespace Render


