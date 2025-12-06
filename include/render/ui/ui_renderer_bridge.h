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

#include <cstdint>

#include "render/types.h"
#include "render/ui/ui_render_commands.h"
#include "render/ui/ui_debug_config.h"
#include "render/ui/ui_theme.h"
#include "render/ui/ui_geometry_renderer.h"
#include "render/sprite/sprite_atlas.h"

namespace Render::Application {
struct AppContext;
struct FrameUpdateArgs;
} // namespace Render::Application

namespace Render {
class SpriteRenderable;
class SpriteAtlas;
class Font;
} // namespace Render

namespace Render::UI {

class UICanvas;
class UIWidget;
class UIWidgetTree;

class UIRendererBridge {
public:
    UIRendererBridge() = default;
    ~UIRendererBridge() = default;

    void Initialize(Render::Application::AppContext& ctx);
    void Shutdown(Render::Application::AppContext& ctx);

    void SetDebugConfig(const UIDebugConfig* config) { m_debugConfig = config; }
    void SetThemeManager(UIThemeManager* themeManager) { m_themeManager = themeManager; }

    void PrepareFrame(const Render::Application::FrameUpdateArgs& frame,
                      UICanvas& canvas,
                      UIWidgetTree& tree,
                      Render::Application::AppContext& ctx);

    void Flush(const Render::Application::FrameUpdateArgs& frame,
               UICanvas& canvas,
               UIWidgetTree& tree,
               Render::Application::AppContext& ctx);

private:
    void UploadPerFrameUniforms(const Render::Application::FrameUpdateArgs& frame,
                                UICanvas& canvas,
                                Render::Application::AppContext& ctx);

    void EnsureAtlas(Render::Application::AppContext& ctx);
    void EnsureTextResources(Render::Application::AppContext& ctx);
    void EnsureDebugTexture();
    void EnsureSolidTexture();
    void DrawDebugRect(const UIDebugRectCommand& cmd,
                       const Matrix4& projection,
                       Render::Application::AppContext& ctx);
    void BuildCommands(UICanvas& canvas,
                       UIWidgetTree& tree,
                       Render::Application::AppContext& ctx);

    bool m_initialized = false;
    bool m_loggedMissingAtlas = false;
    bool m_loggedMissingFont = false;
    bool m_debugTextureValid = false;
    SpriteAtlasPtr m_uiAtlas;
    Ref<Render::Font> m_defaultFont;
    bool m_loggedDebugRectShader = false;
    Ref<Render::Texture> m_debugTexture;
    Ref<Render::Texture> m_solidTexture;
    bool m_solidTextureValid = false;
    bool m_loggedSolidTexture = false;
    const UIDebugConfig* m_debugConfig = nullptr;
    UIThemeManager* m_themeManager = nullptr;

    UIRenderCommandBuffer m_commandBuffer;
    UIGeometryRenderer m_geometryRenderer;
};

} // namespace Render::UI


