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

#include "render/application/event_bus.h"
#include "render/application/scene.h"
#include "render/application/scene_graph.h"

namespace Render {

namespace Application {

class BootScene final : public Scene {
public:
    BootScene() = default;
    ~BootScene() override = default;

    std::string_view Name() const override { return "BootScene"; }

    void OnAttach(AppContext& ctx, ModuleRegistry& modules) override;
    void OnDetach(AppContext& ctx) override;

    SceneResourceManifest BuildManifest() const override;

    void OnEnter(const SceneEnterArgs& args) override;
    void OnUpdate(const FrameUpdateArgs& frame) override;
    SceneSnapshot OnExit(const SceneExitArgs& args) override;

private:
    AppContext* m_context = nullptr;
    EventBus::ListenerId m_frameListener = 0;
    SceneGraph m_sceneGraph;

    void SubscribeFrameEvents();
    void UnsubscribeFrameEvents();
};

} // namespace Application
} // namespace Render


