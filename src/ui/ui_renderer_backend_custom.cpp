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
#include "render/ui/ui_renderer_backend_custom.h"

#include "render/ui/ui_renderer_bridge.h"
#include "render/logger.h"

namespace Render::UI {

CustomUIRendererBackend::CustomUIRendererBackend()
    : m_bridge(std::make_unique<UIRendererBridge>()) {
}

CustomUIRendererBackend::~CustomUIRendererBackend() = default;

void CustomUIRendererBackend::Initialize(Application::AppContext& ctx) {
    if (!m_bridge) {
        Logger::GetInstance().Error("[CustomUIRendererBackend] Bridge is null");
        return;
    }
    m_bridge->Initialize(ctx);
}

void CustomUIRendererBackend::Shutdown(Application::AppContext& ctx) {
    if (m_bridge) {
        m_bridge->Shutdown(ctx);
    }
}

void CustomUIRendererBackend::PrepareFrame(const Application::FrameUpdateArgs& frame,
                                          UICanvas& canvas,
                                          UIWidgetTree& tree,
                                          Application::AppContext& ctx) {
    if (!m_bridge) {
        return;
    }
    m_bridge->PrepareFrame(frame, canvas, tree, ctx);
}

void CustomUIRendererBackend::Flush(const Application::FrameUpdateArgs& frame,
                                   UICanvas& canvas,
                                   UIWidgetTree& tree,
                                   Application::AppContext& ctx) {
    if (!m_bridge) {
        return;
    }
    m_bridge->Flush(frame, canvas, tree, ctx);
}

void CustomUIRendererBackend::SetDebugConfig(const UIDebugConfig* config) {
    if (m_bridge) {
        m_bridge->SetDebugConfig(config);
    }
}

void CustomUIRendererBackend::SetThemeManager(UIThemeManager* themeManager) {
    if (m_bridge) {
        m_bridge->SetThemeManager(themeManager);
    }
}

} // namespace Render::UI

