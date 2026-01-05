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
#include "render/ui/ui_renderer_backend.h"

namespace Render::UI {
class UICanvas;
struct UILayoutContext;
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

    /**
     * @brief 设置UI渲染后端类型
     * @param backendType 后端类型（Custom或ImGui）
     * @note 必须在模块注册之前调用，否则使用默认的Custom后端
     */
    void SetBackendType(UI::UIRendererBackendType backendType);

    /**
     * @brief 获取当前使用的后端类型
     */
    UI::UIRendererBackendType GetBackendType() const { return m_backendType; }

    /**
     * @brief 处理SDL事件（用于ImGui后端）
     * @param event SDL事件指针
     * @return 如果事件被UI系统处理（如ImGui），返回true，否则返回false
     */
    bool ProcessEvent(const void* event);

private:
    void EnsureInitialized(AppContext& ctx);
    void EnsureSampleWidgets();
    void UpdateToggleAnimations(UI::UIWidget& widget, float deltaTime);
    void Shutdown(AppContext& ctx);

    std::unique_ptr<UI::UICanvas> m_canvas;
    std::unique_ptr<UI::UILayoutContext> m_layoutContext;
    std::unique_ptr<UI::IUIRendererBackend> m_rendererBackend;
    std::unique_ptr<UI::UIWidgetTree> m_widgetTree;
    std::unique_ptr<UI::UIInputRouter> m_inputRouter;
    UI::UIDebugConfig m_debugConfig{};
    UI::UIRendererBackendType m_backendType = UI::UIRendererBackendType::Custom;
    bool m_registered = false;
    
    // 示例控件组（用于演示）
    std::unique_ptr<UI::UIRadioButtonGroup> m_sampleRadioGroup;
};

} // namespace Render::Application


