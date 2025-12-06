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

#include <memory>
#include <string_view>

#include "render/application/app_module.h"
#include "render/ui/ui_debug_config.h"

namespace Render::UI {
class UICanvas;
struct UILayoutContext;
class UIRendererBridge;
class UIWidgetTree;
class UIInputRouter;
class UIWidget;
class UIRadioButtonGroup;
} // namespace Render::UI

namespace Render::Application {

class UIRuntimeModule final : public AppModule {
public:
    UIRuntimeModule();
    ~UIRuntimeModule() override;

    std::string_view Name() const override;
    ModuleDependencies Dependencies() const override;
    int Priority(ModulePhase phase) const override;

    void OnRegister(ECS::World& world, AppContext& ctx) override;
    void OnUnregister(ECS::World& world, AppContext& ctx) override;

    void OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx) override;
    void OnPostFrame(const FrameUpdateArgs& frame, AppContext& ctx) override;

    void SetDebugOptions(const UI::UIDebugConfig& config);

private:
    void EnsureInitialized(AppContext& ctx);
    void EnsureSampleWidgets();
    void UpdateToggleAnimations(UI::UIWidget& widget, float deltaTime);
    void Shutdown(AppContext& ctx);

    std::unique_ptr<UI::UICanvas> m_canvas;
    std::unique_ptr<UI::UILayoutContext> m_layoutContext;
    std::unique_ptr<UI::UIRendererBridge> m_rendererBridge;
    std::unique_ptr<UI::UIWidgetTree> m_widgetTree;
    std::unique_ptr<UI::UIInputRouter> m_inputRouter;
    UI::UIDebugConfig m_debugConfig{};
    bool m_registered = false;
    
    // 示例控件组（用于演示）
    std::unique_ptr<UI::UIRadioButtonGroup> m_sampleRadioGroup;
};

} // namespace Render::Application


