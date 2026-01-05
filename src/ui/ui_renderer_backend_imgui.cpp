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
#include "render/ui/ui_theme.h"
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
    // 如果已经有主题管理器，会在后面同步主题
    // 否则使用默认的暗色主题
    if (!m_themeManager) {
        ImGui::StyleColorsDark();
    }

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
    
    // 如果已经有主题管理器，立即同步主题
    if (m_themeManager) {
        SyncThemeToImGui();
    }
    
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

    // 检测主题变化并同步（在NewFrame之前）
    if (m_themeManager) {
        const UITheme& currentTheme = m_themeManager->GetCurrentTheme();
        Color currentBgColor = currentTheme.backgroundColor;
        
        // 计算当前主题的特征值
        float currentThemeHash = currentBgColor.r + currentBgColor.g * 0.1f + 
                                currentTheme.button.normal.inner.r * 0.01f + 
                                currentTheme.button.normal.text.r * 0.001f;
        std::string currentThemeHashStr = std::to_string(currentThemeHash);
        
        // 如果主题特征值变化，重新同步
        if (m_lastSyncedThemeName != currentThemeHashStr) {
            SyncThemeToImGui();
        }
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
    
    // 如果已经初始化，立即同步主题
    if (m_initialized && m_themeManager) {
        SyncThemeToImGui();
    }
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

ImVec4 ImGuiUIRendererBackend::ColorToImVec4(const Color& color) {
    return ImVec4(color.r, color.g, color.b, color.a);
}

void ImGuiUIRendererBackend::SyncThemeToImGui() {
    if (!m_initialized || !m_themeManager) {
        return;
    }

    ImGuiStyle& style = ImGui::GetStyle();
    const UITheme& theme = m_themeManager->GetCurrentTheme();

    // 文本颜色
    style.Colors[ImGuiCol_Text] = ColorToImVec4(theme.button.normal.text);
    style.Colors[ImGuiCol_TextDisabled] = ColorToImVec4(theme.button.disabled.text);

    // 按钮颜色
    style.Colors[ImGuiCol_Button] = ColorToImVec4(theme.button.normal.inner);
    style.Colors[ImGuiCol_ButtonHovered] = ColorToImVec4(theme.button.hover.inner);
    style.Colors[ImGuiCol_ButtonActive] = ColorToImVec4(theme.button.pressed.inner);

    // 输入框/框架颜色
    style.Colors[ImGuiCol_FrameBg] = ColorToImVec4(theme.textField.normal.inner);
    style.Colors[ImGuiCol_FrameBgHovered] = ColorToImVec4(theme.textField.hover.inner);
    style.Colors[ImGuiCol_FrameBgActive] = ColorToImVec4(theme.textField.active.inner);

    // 窗口和面板颜色
    style.Colors[ImGuiCol_WindowBg] = ColorToImVec4(theme.backgroundColor);
    style.Colors[ImGuiCol_ChildBg] = ColorToImVec4(theme.panel.normal.inner);
    style.Colors[ImGuiCol_Border] = ColorToImVec4(theme.borderColor);
    style.Colors[ImGuiCol_BorderShadow] = ColorToImVec4(theme.button.normal.outline);

    // 菜单和弹出窗口颜色
    style.Colors[ImGuiCol_PopupBg] = ColorToImVec4(theme.menu.normal.inner);
    style.Colors[ImGuiCol_Header] = ColorToImVec4(theme.menu.normal.inner);
    style.Colors[ImGuiCol_HeaderHovered] = ColorToImVec4(theme.menu.hover.inner);
    style.Colors[ImGuiCol_HeaderActive] = ColorToImVec4(theme.menu.pressed.inner);
    style.Colors[ImGuiCol_Separator] = ColorToImVec4(theme.menu.normal.outline);
    style.Colors[ImGuiCol_SeparatorHovered] = ColorToImVec4(theme.menu.hover.outline);
    style.Colors[ImGuiCol_SeparatorActive] = ColorToImVec4(theme.menu.pressed.outline);

    // 标题栏颜色（使用面板颜色）
    style.Colors[ImGuiCol_TitleBg] = ColorToImVec4(theme.panel.normal.inner);
    style.Colors[ImGuiCol_TitleBgActive] = ColorToImVec4(theme.panel.normal.innerSelected);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ColorToImVec4(theme.panel.normal.inner);

    // 菜单栏颜色
    style.Colors[ImGuiCol_MenuBarBg] = ColorToImVec4(theme.panel.normal.inner);

    // 滚动条颜色（使用按钮颜色）
    style.Colors[ImGuiCol_ScrollbarBg] = ColorToImVec4(theme.panel.normal.inner);
    style.Colors[ImGuiCol_ScrollbarGrab] = ColorToImVec4(theme.button.normal.inner);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ColorToImVec4(theme.button.hover.inner);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ColorToImVec4(theme.button.pressed.inner);

    // 复选框和单选按钮标记颜色
    style.Colors[ImGuiCol_CheckMark] = ColorToImVec4(theme.button.normal.text);

    // 滑块颜色
    style.Colors[ImGuiCol_SliderGrab] = ColorToImVec4(theme.button.normal.inner);
    style.Colors[ImGuiCol_SliderGrabActive] = ColorToImVec4(theme.button.pressed.inner);

    // 调整大小手柄颜色
    style.Colors[ImGuiCol_ResizeGrip] = ColorToImVec4(theme.button.normal.outline);
    style.Colors[ImGuiCol_ResizeGripHovered] = ColorToImVec4(theme.button.hover.outline);
    style.Colors[ImGuiCol_ResizeGripActive] = ColorToImVec4(theme.button.pressed.outline);

    // 输入文本光标颜色
    style.Colors[ImGuiCol_InputTextCursor] = ColorToImVec4(theme.textField.active.text);

    // 选中文本背景
    style.Colors[ImGuiCol_TextSelectedBg] = ColorToImVec4(theme.textField.active.inner);

    // 标签页颜色（使用按钮颜色）
    style.Colors[ImGuiCol_Tab] = ColorToImVec4(theme.button.normal.inner);
    style.Colors[ImGuiCol_TabHovered] = ColorToImVec4(theme.button.hover.inner);
    style.Colors[ImGuiCol_TabActive] = ColorToImVec4(theme.button.active.inner);
    style.Colors[ImGuiCol_TabSelected] = ColorToImVec4(theme.button.active.inner);
    style.Colors[ImGuiCol_TabUnfocused] = ColorToImVec4(theme.button.normal.inner);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ColorToImVec4(theme.button.hover.inner);

    // 表格颜色（使用面板颜色）
    style.Colors[ImGuiCol_TableHeaderBg] = ColorToImVec4(theme.panel.normal.inner);
    style.Colors[ImGuiCol_TableBorderStrong] = ColorToImVec4(theme.borderColor);
    style.Colors[ImGuiCol_TableBorderLight] = ColorToImVec4(theme.panel.normal.outline);
    style.Colors[ImGuiCol_TableRowBg] = ColorToImVec4(theme.backgroundColor);
    style.Colors[ImGuiCol_TableRowBgAlt] = ColorToImVec4(theme.panel.normal.inner);

    // 绘图线条颜色（使用文本颜色）
    style.Colors[ImGuiCol_PlotLines] = ColorToImVec4(theme.button.normal.text);
    style.Colors[ImGuiCol_PlotLinesHovered] = ColorToImVec4(theme.button.hover.text);
    style.Colors[ImGuiCol_PlotHistogram] = ColorToImVec4(theme.button.normal.text);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ColorToImVec4(theme.button.hover.text);

    // 拖放目标颜色
    style.Colors[ImGuiCol_DragDropTarget] = ColorToImVec4(theme.button.active.outline);
    style.Colors[ImGuiCol_DragDropTargetBg] = ColorToImVec4(theme.button.active.inner);

    // 导航光标颜色
    style.Colors[ImGuiCol_NavCursor] = ColorToImVec4(theme.button.active.outline);
    style.Colors[ImGuiCol_NavWindowingHighlight] = ColorToImVec4(theme.button.hover.outline);
    style.Colors[ImGuiCol_NavWindowingDimBg] = ColorToImVec4(Color(0.0f, 0.0f, 0.0f, 0.5f));
    style.Colors[ImGuiCol_ModalWindowDimBg] = ColorToImVec4(Color(0.0f, 0.0f, 0.0f, 0.5f));

    // 尺寸设置
    style.FramePadding = ImVec2(theme.sizes.padding, theme.sizes.padding);
    style.WindowPadding = ImVec2(theme.sizes.panelSpace, theme.sizes.panelSpace);
    style.ItemSpacing = ImVec2(theme.sizes.spacing, theme.sizes.spacing);
    style.ItemInnerSpacing = ImVec2(theme.sizes.spacing * 0.5f, theme.sizes.spacing * 0.5f);
    
    // 根据按钮高度计算FramePadding
    float framePaddingY = (theme.sizes.buttonHeight - theme.widget.size) * 0.5f;
    if (framePaddingY > 0.0f) {
        style.FramePadding.y = framePaddingY;
    }

    // 圆角设置
    style.FrameRounding = 4.0f;
    style.WindowRounding = 4.0f;
    style.ChildRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;

    // 边框大小
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 1.0f;

    // 记录当前主题的特征值（用于检测主题变化）
    // 使用背景颜色的哈希值作为主题标识
    Color currentBgColor = theme.backgroundColor;
    float themeHash = currentBgColor.r + currentBgColor.g * 0.1f + 
                     theme.button.normal.inner.r * 0.01f + 
                     theme.button.normal.text.r * 0.001f;
    m_lastSyncedThemeName = std::to_string(themeHash);
    
    Logger::GetInstance().Info("[ImGuiUIRendererBackend] Theme synchronized to ImGui");
}

} // namespace Render::UI

