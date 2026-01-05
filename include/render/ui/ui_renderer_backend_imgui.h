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

#include "render/ui/ui_renderer_backend.h"

namespace Render::UI {

/**
 * @brief ImGui UI渲染后端
 * 
 * 使用Dear ImGui作为UI渲染后端，提供即时模式的UI渲染
 */
class ImGuiUIRendererBackend : public IUIRendererBackend {
public:
    ImGuiUIRendererBackend();
    ~ImGuiUIRendererBackend() override;

    void Initialize(Application::AppContext& ctx) override;
    void Shutdown(Application::AppContext& ctx) override;

    void PrepareFrame(const Application::FrameUpdateArgs& frame,
                     UICanvas& canvas,
                     UIWidgetTree& tree,
                     Application::AppContext& ctx) override;

    void Flush(const Application::FrameUpdateArgs& frame,
              UICanvas& canvas,
              UIWidgetTree& tree,
              Application::AppContext& ctx) override;

    void SetDebugConfig(const UIDebugConfig* config) override;
    void SetThemeManager(UIThemeManager* themeManager) override;

    /**
     * @brief 处理SDL事件（需要在事件循环中调用）
     * @param event SDL事件指针
     * @return 如果事件被ImGui处理，返回true
     */
    bool ProcessEvent(const void* event);

private:
    bool m_initialized = false;
    const UIDebugConfig* m_debugConfig = nullptr;
    UIThemeManager* m_themeManager = nullptr;
};

} // namespace Render::UI

