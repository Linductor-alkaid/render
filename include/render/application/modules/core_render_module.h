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

#include <string_view>
#include <vector>

#include "render/application/app_module.h"
#include "render/application/events/frame_events.h"

namespace Render {
namespace ECS {
class System;
} // namespace ECS
} // namespace Render

namespace Render::Application {

class CoreRenderModule final : public AppModule {
public:
    CoreRenderModule() = default;
    ~CoreRenderModule() override = default;

    std::string_view Name() const override { return "CoreRenderModule"; }
    ModuleDependencies Dependencies() const override { return {}; }

    int Priority(ModulePhase phase) const override;

    void OnRegister(ECS::World& world, AppContext& ctx) override;
    void OnUnregister(ECS::World& world, AppContext& ctx) override;

    void OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx) override;
    void OnPostFrame(const FrameUpdateArgs& frame, AppContext& ctx) override;

private:
    void RegisterCoreSystems(ECS::World& world, AppContext& ctx);
    void RegisterCoreComponents(ECS::World& world, AppContext& ctx);

    bool m_registered = false;
    size_t m_asyncTasksPerFrame = 32;
    bool m_loggedAsyncLoaderMissing = false;
    bool m_systemsRegistered = false;
};

} // namespace Render::Application


