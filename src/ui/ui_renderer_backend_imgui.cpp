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
#include "render/ui/ui_renderer_backend_imgui.h"

#include "render/application/app_context.h"
#include "render/logger.h"
#include "render/renderer.h"
#include "render/opengl_context.h"
#include "render/ui/uicanvas.h"
#include "render/ui/ui_widget_tree.h"
#include <SDL3/SDL.h>

// ImGui includes
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"

namespace Render::UI {

ImGuiUIRendererBackend::ImGuiUIRendererBackend() = default;

ImGuiUIRendererBackend::~ImGuiUIRendererBackend() {
    // Shutdown should be called explicitly, but we ensure cleanup here
    if (m_initialized) {
        Logger::GetInstance().Warning("[ImGuiUIRendererBackend] Destructor called but not properly shut down");
    }
}

void ImGuiUIRendererBackend::Initialize(Application::AppContext& ctx) {
    if (m_initialized) {
        Logger::GetInstance().Warning("[ImGuiUIRendererBackend] Already initialized");
        return;
    }

    if (!ctx.IsValid()) {
        Logger::GetInstance().Error("[ImGuiUIRendererBackend] AppContext is invalid");
        return;
    }

    if (!ctx.renderer) {
        Logger::GetInstance().Error("[ImGuiUIRendererBackend] Renderer is null");
        return;
    }

    auto context = ctx.renderer->GetContext();
    if (!context || !context->IsInitialized()) {
        Logger::GetInstance().Error("[ImGuiUIRendererBackend] OpenGLContext is not initialized");
        return;
    }

    SDL_Window* window = context->GetWindow();
    SDL_GLContext glContext = context->GetGLContext();

    if (!window) {
        Logger::GetInstance().Error("[ImGuiUIRendererBackend] SDL_Window is null");
        return;
    }

    if (!glContext) {
        Logger::GetInstance().Error("[ImGuiUIRendererBackend] SDL_GLContext is null");
        return;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;       // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    const char* glsl_version = "#version 330";
    if (!ImGui_ImplSDL3_InitForOpenGL(window, glContext)) {
        Logger::GetInstance().Error("[ImGuiUIRendererBackend] Failed to initialize SDL3 backend");
        ImGui::DestroyContext();
        return;
    }

    if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
        Logger::GetInstance().Error("[ImGuiUIRendererBackend] Failed to initialize OpenGL3 backend");
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return;
    }

    // 加载中文字体支持
    // 尝试加载中文字体文件（按优先级顺序）
    const std::vector<std::string> fontPaths = {
        "assets/fonts/NotoSansSC-Regular.ttf",
        "assets/fonts/NotoSansCJK-Regular.ttf",
        "assets/fonts/SimHei.ttf",
        "assets/fonts/msyh.ttf",  // 微软雅黑
        "fonts/NotoSansSC-Regular.ttf",
        "fonts/NotoSansCJK-Regular.ttf"
    };
    
    bool fontLoaded = false;
    for (const auto& fontPath : fontPaths) {
        // 尝试加载字体文件
        ImFont* chineseFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull());
        if (chineseFont) {
            Logger::GetInstance().InfoFormat("[ImGuiUIRendererBackend] Loaded Chinese font from: %s", fontPath.c_str());
            fontLoaded = true;
            break;
        }
    }
    
    if (!fontLoaded) {
        // 如果找不到中文字体文件，使用默认字体并合并中文字符范围
        // 这会使用系统默认字体，可能不支持中文，但至少不会崩溃
        Logger::GetInstance().Warning("[ImGuiUIRendererBackend] Failed to load Chinese font, using default font");
        io.Fonts->GetGlyphRangesDefault();
    }

    m_initialized = true;
    Logger::GetInstance().Info("[ImGuiUIRendererBackend] Initialized successfully");
}

void ImGuiUIRendererBackend::Shutdown(Application::AppContext& ctx) {
    if (!m_initialized) {
        return;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    m_initialized = false;
    Logger::GetInstance().Info("[ImGuiUIRendererBackend] Shutdown complete");
}

void ImGuiUIRendererBackend::PrepareFrame(const Application::FrameUpdateArgs& frame,
                                         UICanvas& canvas,
                                         UIWidgetTree& tree,
                                         Application::AppContext& ctx) {
    if (!m_initialized) {
        return;
    }

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // Update ImGui IO with canvas state
    ImGuiIO& io = ImGui::GetIO();
    const auto& canvasState = canvas.GetState();
    
    io.DisplaySize = ImVec2(static_cast<float>(canvasState.windowWidth),
                           static_cast<float>(canvasState.windowHeight));
    io.DeltaTime = frame.deltaTime;
    io.DisplayFramebufferScale = ImVec2(canvasState.dpiScale, canvasState.dpiScale);

    // Note: Widget tree conversion to ImGui calls can be added here
    // For now, we provide a basic example that can be extended
    // Users can directly use ImGui API in their code
}

void ImGuiUIRendererBackend::Flush(const Application::FrameUpdateArgs& frame,
                                   UICanvas& canvas,
                                   UIWidgetTree& tree,
                                   Application::AppContext& ctx) {
    if (!m_initialized) {
        return;
    }

    // Rendering
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    ImGui_ImplOpenGL3_RenderDrawData(draw_data);
}

void ImGuiUIRendererBackend::SetDebugConfig(const UIDebugConfig* config) {
    m_debugConfig = config;
    // ImGui debug configuration can be applied here if needed
}

void ImGuiUIRendererBackend::SetThemeManager(UIThemeManager* themeManager) {
    m_themeManager = themeManager;
    // ImGui theme can be synced with UIThemeManager here if needed
}

bool ImGuiUIRendererBackend::ProcessEvent(const void* event) {
    if (!m_initialized) {
        return false;
    }

    const SDL_Event* sdlEvent = static_cast<const SDL_Event*>(event);
    if (!sdlEvent) {
        return false;
    }

    return ImGui_ImplSDL3_ProcessEvent(sdlEvent);
}

} // namespace Render::UI

