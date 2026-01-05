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
#include <memory>

namespace Render::UI {

class UIRendererBridge;

/**
 * @brief Custom UI渲染后端
 * 
 * 包装现有的UIRendererBridge，作为默认的UI渲染后端实现
 */
class CustomUIRendererBackend : public IUIRendererBackend {
public:
    CustomUIRendererBackend();
    ~CustomUIRendererBackend() override;

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

private:
    std::unique_ptr<UIRendererBridge> m_bridge;
};

} // namespace Render::UI

