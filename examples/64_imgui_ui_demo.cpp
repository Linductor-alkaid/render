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
/**
 * @file 64_imgui_ui_demo.cpp
 * @brief ImGui UI后端演示程序（包含Docking功能）
 * 
 * 本示例展示了如何使用ImGui作为UI渲染后端：
 * - 初始化ImGui后端
 * - 使用ImGui API创建各种UI控件
 * - 显示ImGui示例窗口
 * - 演示基本的ImGui功能
 * - 演示ImGui Docking（窗口停靠）功能
 * 
 * Docking功能说明：
 * - 使用DockSpaceOverViewport()创建停靠空间
 * - 所有窗口都可以停靠到视口边缘或其他窗口
 * - 支持拖拽窗口标题栏进行停靠
 * - 支持标签页界面
 * - 按住SHIFT键可以禁用停靠
 */

#include <SDL3/SDL.h>

#include "render/application/application_host.h"
#include "render/application/modules/core_render_module.h"
#include "render/application/modules/input_module.h"
#include "render/application/modules/ui_runtime_module.h"
#include "render/application/module_registry.h"
#include "render/ui/ui_renderer_backend.h"
#include "render/ui/ui_theme.h"
#include "render/logger.h"
#include "render/renderer.h"
#include "render/resource_manager.h"
#include "render/async_resource_loader.h"

// ImGui includes
#include "imgui.h"

using namespace Render;
using namespace Render::Application;

namespace {

void ConfigureLogger() {
    auto& logger = Logger::GetInstance();
    logger.SetLogToConsole(true);
    logger.SetLogToFile(false);
    logger.SetLogLevel(LogLevel::Info);
}

Renderer* InitializeRenderer() {
    Renderer* renderer = Renderer::Create();
    if (!renderer) {
        Logger::GetInstance().Error("[ImGuiDemo] Failed to create renderer");
        return nullptr;
    }

    if (!renderer->Initialize("ImGui UI Demo", 1280, 720)) {
        Logger::GetInstance().Error("[ImGuiDemo] Failed to initialize renderer");
        Renderer::Destroy(renderer);
        return nullptr;
    }

    renderer->SetClearColor(0.15f, 0.15f, 0.15f, 1.0f);
    renderer->SetVSync(true);
    renderer->SetBatchingMode(BatchingMode::CpuMerge);
    return renderer;
}

// ImGui UI状态
struct ImGuiDemoState {
    bool showDemoWindow = true;
    bool showAnotherWindow = false;
    float f = 0.0f;
    int counter = 0;
    char textBuffer[256] = "Hello, ImGui!";
    float color[3] = {1.0f, 0.0f, 0.0f};
    bool checkbox = true;
    int radioButton = 0;
    float sliderFloat = 0.5f;
    int sliderInt = 50;
    int selectedTheme = 0;  // 当前选择的主题索引
} g_demoState;

// 主题名称列表
const char* g_themeNames[] = {
    "默认主题",
    "暗色主题",
    "亮色主题",
    "蓝色主题",
    "绿色主题"
};

// 主题ID列表（对应UIThemeManager中的主题名称）
const char* g_themeIds[] = {
    "default",
    "dark",
    "light",
    "blue",
    "green"
};

// 创建自定义主题
UI::UITheme CreateLightTheme() {
    UI::UITheme theme;
    
    // 按钮颜色（亮色主题 - 更亮的白色）
    theme.button.normal.inner = Color(0.98f, 0.98f, 0.98f, 1.0f);
    theme.button.normal.text = Color(0.1f, 0.1f, 0.1f, 1.0f);
    theme.button.hover.inner = Color(1.0f, 1.0f, 1.0f, 1.0f);
    theme.button.hover.text = Color(0.05f, 0.05f, 0.05f, 1.0f);
    theme.button.pressed.inner = Color(0.9f, 0.9f, 0.9f, 1.0f);
    theme.button.pressed.text = Color(0.15f, 0.15f, 0.15f, 1.0f);
    theme.button.disabled.inner = Color(0.85f, 0.85f, 0.85f, 1.0f);
    theme.button.disabled.text = Color(0.6f, 0.6f, 0.6f, 1.0f);
    
    // 文本输入框颜色
    theme.textField.normal.inner = Color(1.0f, 1.0f, 1.0f, 1.0f);
    theme.textField.normal.text = Color(0.1f, 0.1f, 0.1f, 1.0f);
    theme.textField.hover.inner = Color(1.0f, 1.0f, 1.0f, 1.0f);
    theme.textField.hover.text = Color(0.1f, 0.1f, 0.1f, 1.0f);
    theme.textField.active.inner = Color(1.0f, 1.0f, 1.0f, 1.0f);
    theme.textField.active.text = Color(0.05f, 0.05f, 0.05f, 1.0f);
    theme.textField.disabled.inner = Color(0.95f, 0.95f, 0.95f, 1.0f);
    theme.textField.disabled.text = Color(0.7f, 0.7f, 0.7f, 1.0f);
    
    // 面板颜色
    theme.panel.normal.inner = Color(0.98f, 0.98f, 0.98f, 1.0f);
    theme.panel.normal.text = Color(0.1f, 0.1f, 0.1f, 1.0f);
    
    // 菜单颜色
    theme.menu.normal.inner = Color(1.0f, 1.0f, 1.0f, 1.0f);
    theme.menu.normal.text = Color(0.1f, 0.1f, 0.1f, 1.0f);
    theme.menu.hover.inner = Color(0.95f, 0.95f, 1.0f, 1.0f);
    theme.menu.hover.text = Color(0.05f, 0.05f, 0.05f, 1.0f);
    
    // 背景和边框
    theme.backgroundColor = Color(1.0f, 1.0f, 1.0f, 1.0f);
    theme.borderColor = Color(0.8f, 0.8f, 0.8f, 1.0f);
    
    return theme;
}

UI::UITheme CreateBlueTheme() {
    UI::UITheme theme;
    
    // 按钮颜色（蓝色主题）
    theme.button.normal.inner = Color(0.2f, 0.4f, 0.8f, 1.0f);
    theme.button.normal.text = Color(1.0f, 1.0f, 1.0f, 1.0f);
    theme.button.hover.inner = Color(0.3f, 0.5f, 0.9f, 1.0f);
    theme.button.hover.text = Color(1.0f, 1.0f, 1.0f, 1.0f);
    theme.button.pressed.inner = Color(0.15f, 0.35f, 0.7f, 1.0f);
    theme.button.pressed.text = Color(1.0f, 1.0f, 1.0f, 1.0f);
    theme.button.disabled.inner = Color(0.3f, 0.3f, 0.3f, 1.0f);
    theme.button.disabled.text = Color(0.6f, 0.6f, 0.6f, 1.0f);
    
    // 文本输入框颜色
    theme.textField.normal.inner = Color(0.15f, 0.2f, 0.3f, 1.0f);
    theme.textField.normal.text = Color(0.9f, 0.9f, 0.9f, 1.0f);
    theme.textField.hover.inner = Color(0.2f, 0.25f, 0.35f, 1.0f);
    theme.textField.hover.text = Color(0.9f, 0.9f, 0.9f, 1.0f);
    theme.textField.active.inner = Color(0.1f, 0.3f, 0.5f, 1.0f);
    theme.textField.active.text = Color(1.0f, 1.0f, 1.0f, 1.0f);
    theme.textField.disabled.inner = Color(0.1f, 0.1f, 0.1f, 1.0f);
    theme.textField.disabled.text = Color(0.5f, 0.5f, 0.5f, 1.0f);
    
    // 面板颜色
    theme.panel.normal.inner = Color(0.1f, 0.15f, 0.25f, 1.0f);
    theme.panel.normal.text = Color(0.9f, 0.9f, 0.9f, 1.0f);
    
    // 菜单颜色
    theme.menu.normal.inner = Color(0.15f, 0.2f, 0.3f, 1.0f);
    theme.menu.normal.text = Color(0.9f, 0.9f, 0.9f, 1.0f);
    theme.menu.hover.inner = Color(0.25f, 0.4f, 0.6f, 1.0f);
    theme.menu.hover.text = Color(1.0f, 1.0f, 1.0f, 1.0f);
    
    // 背景和边框
    theme.backgroundColor = Color(0.08f, 0.12f, 0.2f, 1.0f);
    theme.borderColor = Color(0.3f, 0.5f, 0.8f, 1.0f);
    
    return theme;
}

UI::UITheme CreateGreenTheme() {
    UI::UITheme theme;
    
    // 按钮颜色（绿色主题）
    theme.button.normal.inner = Color(0.2f, 0.7f, 0.3f, 1.0f);
    theme.button.normal.text = Color(1.0f, 1.0f, 1.0f, 1.0f);
    theme.button.hover.inner = Color(0.3f, 0.8f, 0.4f, 1.0f);
    theme.button.hover.text = Color(1.0f, 1.0f, 1.0f, 1.0f);
    theme.button.pressed.inner = Color(0.15f, 0.6f, 0.25f, 1.0f);
    theme.button.pressed.text = Color(1.0f, 1.0f, 1.0f, 1.0f);
    theme.button.disabled.inner = Color(0.3f, 0.3f, 0.3f, 1.0f);
    theme.button.disabled.text = Color(0.6f, 0.6f, 0.6f, 1.0f);
    
    // 文本输入框颜色
    theme.textField.normal.inner = Color(0.15f, 0.25f, 0.2f, 1.0f);
    theme.textField.normal.text = Color(0.9f, 0.9f, 0.9f, 1.0f);
    theme.textField.hover.inner = Color(0.2f, 0.3f, 0.25f, 1.0f);
    theme.textField.hover.text = Color(0.9f, 0.9f, 0.9f, 1.0f);
    theme.textField.active.inner = Color(0.1f, 0.4f, 0.3f, 1.0f);
    theme.textField.active.text = Color(1.0f, 1.0f, 1.0f, 1.0f);
    theme.textField.disabled.inner = Color(0.1f, 0.1f, 0.1f, 1.0f);
    theme.textField.disabled.text = Color(0.5f, 0.5f, 0.5f, 1.0f);
    
    // 面板颜色
    theme.panel.normal.inner = Color(0.1f, 0.2f, 0.15f, 1.0f);
    theme.panel.normal.text = Color(0.9f, 0.9f, 0.9f, 1.0f);
    
    // 菜单颜色
    theme.menu.normal.inner = Color(0.15f, 0.25f, 0.2f, 1.0f);
    theme.menu.normal.text = Color(0.9f, 0.9f, 0.9f, 1.0f);
    theme.menu.hover.inner = Color(0.25f, 0.5f, 0.35f, 1.0f);
    theme.menu.hover.text = Color(1.0f, 1.0f, 1.0f, 1.0f);
    
    // 背景和边框
    theme.backgroundColor = Color(0.08f, 0.15f, 0.12f, 1.0f);
    theme.borderColor = Color(0.3f, 0.7f, 0.4f, 1.0f);
    
    return theme;
}

// 在ImGui后端中渲染自定义UI
void RenderImGuiUI() {
    // 0. 创建DockSpace - 必须在任何窗口之前提交
    // 这将允许所有窗口可以停靠到视口的边缘
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        // 创建覆盖整个视口的停靠空间
        // 参数：dockspace_id (0=自动生成), viewport (nullptr=主视口), flags, window_class
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
    }

    // 1. 显示ImGui示例窗口（包含所有控件演示）
    if (g_demoState.showDemoWindow) {
        ImGui::ShowDemoWindow(&g_demoState.showDemoWindow);
    }

    // 2. 创建一个简单的窗口
    {
        ImGui::Begin("Hello, ImGui!");

        ImGui::Text("这是一个使用ImGui渲染的UI示例");
        ImGui::Text("应用程序平均帧率: %.3f ms/frame (%.1f FPS)", 
                    1000.0f / ImGui::GetIO().Framerate, 
                    ImGui::GetIO().Framerate);
        ImGui::Separator();

        if (ImGui::Button("按钮")) {
            g_demoState.counter++;
        }
        ImGui::SameLine();
        ImGui::Text("按钮被点击了 %d 次", g_demoState.counter);

        ImGui::Checkbox("显示示例窗口", &g_demoState.showDemoWindow);
        ImGui::Checkbox("显示另一个窗口", &g_demoState.showAnotherWindow);
        ImGui::Checkbox("复选框", &g_demoState.checkbox);

        ImGui::InputText("文本输入", g_demoState.textBuffer, IM_ARRAYSIZE(g_demoState.textBuffer));

        ImGui::SliderFloat("浮点滑块", &g_demoState.sliderFloat, 0.0f, 1.0f);
        ImGui::SliderInt("整数滑块", &g_demoState.sliderInt, 0, 100);

        ImGui::ColorEdit3("颜色选择", g_demoState.color);

        ImGui::RadioButton("选项 1", &g_demoState.radioButton, 0);
        ImGui::SameLine();
        ImGui::RadioButton("选项 2", &g_demoState.radioButton, 1);
        ImGui::SameLine();
        ImGui::RadioButton("选项 3", &g_demoState.radioButton, 2);

        ImGui::Text("当前选择的选项: %d", g_demoState.radioButton);

        ImGui::Separator();
        ImGui::Text("主题选择:");
        
        // 主题选择单选按钮组
        for (int i = 0; i < IM_ARRAYSIZE(g_themeNames); i++) {
            bool wasSelected = (g_demoState.selectedTheme == i);
            if (ImGui::RadioButton(g_themeNames[i], &g_demoState.selectedTheme, i)) {
                // 主题切换
                if (!wasSelected) {
                    auto& themeManager = UI::UIThemeManager::GetInstance();
                    themeManager.SetCurrentTheme(g_themeIds[i]);
                    Logger::GetInstance().InfoFormat("[ImGuiDemo] Switched to theme: %s", g_themeIds[i]);
                }
            }
        }

        ImGui::Separator();
        ImGui::Text("主题序列化:");
        
        // 保存当前主题到JSON
        if (ImGui::Button("保存当前主题到JSON")) {
            auto& themeManager = UI::UIThemeManager::GetInstance();
            const UI::UITheme& currentTheme = themeManager.GetCurrentTheme();
            std::string themeName = g_themeIds[g_demoState.selectedTheme];
            std::string filePath = "themes/" + themeName + ".json";
            
            if (UI::UITheme::SaveToJSON(currentTheme, filePath)) {
                Logger::GetInstance().InfoFormat("[ImGuiDemo] Theme saved to: %s", filePath.c_str());
                ImGui::OpenPopup("保存成功");
            } else {
                Logger::GetInstance().ErrorFormat("[ImGuiDemo] Failed to save theme to: %s", filePath.c_str());
                ImGui::OpenPopup("保存失败");
            }
        }
        
        ImGui::SameLine();
        
        // 从JSON加载主题
        if (ImGui::Button("从JSON加载主题")) {
            auto& themeManager = UI::UIThemeManager::GetInstance();
            std::string themeName = g_themeIds[g_demoState.selectedTheme];
            std::string filePath = "themes/" + themeName + ".json";
            
            UI::UITheme loadedTheme;
            if (UI::UITheme::LoadFromJSON(filePath, loadedTheme)) {
                themeManager.RegisterBuiltinTheme(themeName + "_loaded", loadedTheme);
                themeManager.SetCurrentTheme(themeName + "_loaded");
                Logger::GetInstance().InfoFormat("[ImGuiDemo] Theme loaded from: %s", filePath.c_str());
                ImGui::OpenPopup("加载成功");
            } else {
                Logger::GetInstance().WarningFormat("[ImGuiDemo] Failed to load theme from: %s (file may not exist)", filePath.c_str());
                ImGui::OpenPopup("加载失败");
            }
        }
        
        // 显示序列化状态信息
        ImGui::Text("当前主题: %s", g_themeIds[g_demoState.selectedTheme]);
        ImGui::Text("主题文件路径: themes/%s.json", g_themeIds[g_demoState.selectedTheme]);
        
        // 弹出提示窗口
        if (ImGui::BeginPopupModal("保存成功", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("主题已成功保存到 themes/%s.json", g_themeIds[g_demoState.selectedTheme]);
            if (ImGui::Button("确定")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        if (ImGui::BeginPopupModal("保存失败", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("保存失败！请检查 themes/ 目录是否存在。");
            if (ImGui::Button("确定")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        if (ImGui::BeginPopupModal("加载成功", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("主题已成功从 themes/%s.json 加载", g_themeIds[g_demoState.selectedTheme]);
            if (ImGui::Button("确定")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        if (ImGui::BeginPopupModal("加载失败", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("加载失败！文件不存在或格式错误。");
            ImGui::Text("请先使用'保存当前主题到JSON'按钮创建主题文件。");
            if (ImGui::Button("确定")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::End();
    }

    // 3. 显示另一个窗口
    if (g_demoState.showAnotherWindow) {
        ImGui::Begin("另一个窗口", &g_demoState.showAnotherWindow);
        ImGui::Text("这是另一个窗口");
        if (ImGui::Button("关闭这个窗口")) {
            g_demoState.showAnotherWindow = false;
        }
        ImGui::End();
    }

    // 4. 显示关于窗口
    {
        ImGui::Begin("关于");
        ImGui::Text("ImGui UI后端演示");
        ImGui::Text("使用Dear ImGui作为UI渲染后端");
        ImGui::Separator();
        ImGui::Text("ImGui版本: %s", IMGUI_VERSION);
        ImGui::Text("ImGui版本号: %d", IMGUI_VERSION_NUM);
        ImGui::End();
    }

    // 5. Docking功能演示窗口
    {
        static bool showDockingDemo = true;
        if (showDockingDemo) {
            ImGui::Begin("Docking演示", &showDockingDemo);
            ImGui::Text("这是Docking功能演示窗口");
            ImGui::Separator();
            ImGui::TextWrapped("使用说明：");
            ImGui::BulletText("拖拽窗口标题栏可以将窗口停靠到边缘");
            ImGui::BulletText("拖拽窗口标签页可以重新排列窗口");
            ImGui::BulletText("按住SHIFT键拖拽可以禁用停靠");
            ImGui::BulletText("点击窗口左上角的菜单按钮可以取消停靠整个节点");
            ImGui::Separator();
            ImGui::Text("当前窗口可以停靠到其他窗口或视口边缘");
            ImGui::Text("尝试将多个窗口停靠在一起创建标签页界面！");
            ImGui::End();
        }
    }

    // 6. 控制面板窗口（演示多个可停靠窗口）
    {
        static bool showControlPanel = true;
        if (showControlPanel) {
            ImGui::Begin("控制面板", &showControlPanel);
            ImGui::Text("这是一个控制面板窗口");
            ImGui::Separator();
            ImGui::Text("可以停靠到左侧或右侧");
            ImGui::Separator();
            ImGui::Text("帧率信息:");
            ImGui::Text("  %.3f ms/frame", 1000.0f / ImGui::GetIO().Framerate);
            ImGui::Text("  %.1f FPS", ImGui::GetIO().Framerate);
            ImGui::Separator();
            ImGui::Text("窗口状态:");
            ImGui::Text("  Docking已启用: %s", 
                (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) ? "是" : "否");
            ImGui::End();
        }
    }

    // 7. 属性编辑器窗口（演示另一个可停靠窗口）
    {
        static bool showPropertyEditor = true;
        static float propertyValue1 = 0.5f;
        static float propertyValue2 = 1.0f;
        static int propertyInt = 42;
        static bool propertyBool = true;
        
        if (showPropertyEditor) {
            ImGui::Begin("属性编辑器", &showPropertyEditor);
            ImGui::Text("属性编辑器窗口");
            ImGui::Separator();
            ImGui::SliderFloat("属性值1", &propertyValue1, 0.0f, 1.0f);
            ImGui::SliderFloat("属性值2", &propertyValue2, 0.0f, 2.0f);
            ImGui::SliderInt("整数属性", &propertyInt, 0, 100);
            ImGui::Checkbox("布尔属性", &propertyBool);
            ImGui::Separator();
            ImGui::Text("当前值:");
            ImGui::Text("  属性值1: %.3f", propertyValue1);
            ImGui::Text("  属性值2: %.3f", propertyValue2);
            ImGui::Text("  整数属性: %d", propertyInt);
            ImGui::Text("  布尔属性: %s", propertyBool ? "真" : "假");
            ImGui::End();
        }
    }

    // 8. 日志窗口（演示另一个可停靠窗口）
    {
        static bool showLogWindow = true;
        static ImGuiTextBuffer logBuffer;
        static int logLineCount = 0;
        
        if (showLogWindow) {
            ImGui::Begin("日志窗口", &showLogWindow);
            
            // 添加一些示例日志
            if (logLineCount < 100) {
                logBuffer.appendf("[%04d] 这是一条日志消息 %d\n", logLineCount, logLineCount);
                logLineCount++;
            }
            
            ImGui::Text("日志输出:");
            ImGui::BeginChild("ScrollingRegion", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::TextUnformatted(logBuffer.begin(), logBuffer.end());
            ImGui::EndChild();
            
            if (ImGui::Button("清空日志")) {
                logBuffer.clear();
                logLineCount = 0;
            }
            ImGui::SameLine();
            if (ImGui::Button("添加日志")) {
                logBuffer.appendf("[%04d] 新日志消息: %s\n", logLineCount++, "手动添加");
            }
            
            ImGui::End();
        }
    }

    // 9. 场景视图窗口（演示另一个可停靠窗口）
    {
        static bool showSceneView = true;
        if (showSceneView) {
            ImGui::Begin("场景视图", &showSceneView);
            ImGui::Text("场景视图窗口");
            ImGui::Separator();
            ImGui::Text("这里可以显示3D场景预览");
            ImGui::Text("窗口可以停靠到中央区域");
            ImGui::Separator();
            
            // 模拟一个简单的场景视图区域
            ImVec2 canvasSize = ImGui::GetContentRegionAvail();
            ImVec2 canvasPos = ImGui::GetCursorScreenPos();
            
            // 绘制一个简单的背景
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), 
                IM_COL32(30, 30, 30, 255));
            drawList->AddRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), 
                IM_COL32(100, 100, 100, 255));
            
            // 在中心绘制一个简单的网格
            float gridSize = 20.0f;
            ImU32 gridColor = IM_COL32(60, 60, 60, 255);
            for (float x = canvasPos.x; x < canvasPos.x + canvasSize.x; x += gridSize) {
                drawList->AddLine(ImVec2(x, canvasPos.y), ImVec2(x, canvasPos.y + canvasSize.y), gridColor);
            }
            for (float y = canvasPos.y; y < canvasPos.y + canvasSize.y; y += gridSize) {
                drawList->AddLine(ImVec2(canvasPos.x, y), ImVec2(canvasPos.x + canvasSize.x, y), gridColor);
            }
            
            // 在中心绘制一个简单的形状
            ImVec2 center = ImVec2(canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f);
            float radius = std::min(canvasSize.x, canvasSize.y) * 0.2f;
            drawList->AddCircleFilled(center, radius, IM_COL32(100, 150, 200, 255), 32);
            
            ImGui::Dummy(canvasSize);
            
            ImGui::End();
        }
    }
}

} // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    ConfigureLogger();

    Renderer* renderer = InitializeRenderer();
    if (!renderer) {
        return -1;
    }

    auto& resourceManager = ResourceManager::GetInstance();
    auto& asyncLoader = AsyncResourceLoader::GetInstance();
    asyncLoader.Initialize();

    ApplicationHost host;
    ApplicationHost::Config config{};
    config.renderer = renderer;
    config.resourceManager = &resourceManager;
    config.asyncLoader = &asyncLoader;
    config.uniformManager = nullptr;

    if (!host.Initialize(config)) {
        Logger::GetInstance().Error("[ImGuiDemo] ApplicationHost initialization failed.");
        asyncLoader.Shutdown();
        Renderer::Destroy(renderer);
        return -1;
    }

    // 在注册模块之前初始化主题管理器并注册所有主题
    // 这样UIRuntimeModule在初始化时就能使用所有主题
    auto& themeManager = UI::UIThemeManager::GetInstance();
    themeManager.InitializeDefaults();
    
    // 注册自定义主题
    themeManager.RegisterBuiltinTheme("light", CreateLightTheme());
    themeManager.RegisterBuiltinTheme("blue", CreateBlueTheme());
    themeManager.RegisterBuiltinTheme("green", CreateGreenTheme());
    
    // 设置默认主题
    themeManager.SetCurrentTheme("default");
    
    Logger::GetInstance().Info("[ImGuiDemo] Registered themes: default, dark, light, blue, green");
    
    // 注册核心模块
    host.GetModuleRegistry().RegisterModule(std::make_unique<CoreRenderModule>());
    host.GetModuleRegistry().RegisterModule(std::make_unique<InputModule>());
    
    // 创建UI运行时模块并设置为使用ImGui后端
    auto uiModule = std::make_unique<UIRuntimeModule>();
    uiModule->SetBackendType(UI::UIRendererBackendType::ImGui);
    host.GetModuleRegistry().RegisterModule(std::move(uiModule));

    auto* inputModule = dynamic_cast<InputModule*>(host.GetModuleRegistry().GetModule("InputModule"));

    Logger::GetInstance().Info("[ImGuiDemo] ImGui UI Demo");
    Logger::GetInstance().Info("[ImGuiDemo] - Using ImGui as UI rendering backend");
    Logger::GetInstance().Info("[ImGuiDemo] - Press ESC or close the window to exit.");

    bool running = true;
    uint64_t frameIndex = 0;
    double absoluteTime = 0.0;

    // 获取UIRuntimeModule以便在每帧中调用ImGui API
    auto* uiRuntimeModule = dynamic_cast<UIRuntimeModule*>(
        host.GetModuleRegistry().GetModule("UIRuntimeModule"));

    while (running) {
        // 注意：事件处理在InputModule的OnPreFrame中进行
        // UIRuntimeModule的优先级(250)高于InputModule(200)，
        // 所以UIRuntimeModule的PrepareFrame会先执行，调用NewFrame
        // 但事件处理应该在NewFrame之前，所以我们需要在InputModule中先处理ImGui事件
        // 或者，我们可以不在主循环中处理事件，完全由InputModule处理
        
        // 暂时不在主循环中处理事件，完全由InputModule处理
        // 但我们需要检查退出事件
        if (inputModule && inputModule->WasQuitRequested()) {
            running = false;
        }

        // 获取帧时间
        const float deltaTime = renderer->GetDeltaTime();
        absoluteTime += static_cast<double>(deltaTime);

        // 准备帧更新参数
        FrameUpdateArgs frameArgs{};
        frameArgs.deltaTime = deltaTime;
        frameArgs.absoluteTime = absoluteTime;
        frameArgs.frameIndex = frameIndex;

        // 更新应用（这会调用UIRuntimeModule的OnPreFrame -> PrepareFrame，初始化ImGui帧）
        // PrepareFrame会调用ImGui::NewFrame()，此时可以安全使用ImGui API
        // 注意：UpdateFrame也会调用PostFrame，但我们在Clear之后会再次调用PostFrame来渲染ImGui
        host.GetModuleRegistry().InvokePhase(ModulePhase::PreFrame, frameArgs);

        // 在ImGui帧中渲染自定义UI
        // 注意：ImGui::NewFrame()已经在UIRuntimeModule::PrepareFrame中调用
        // 这里我们直接使用ImGui API创建UI
        RenderImGuiUI();

        // 渲染
        renderer->BeginFrame();
        renderer->Clear();

        // 更新世界（ECS系统）
        host.UpdateWorld(deltaTime);

        // Flush渲染队列（先渲染3D内容）
        renderer->FlushRenderQueue();

        // 现在调用PostFrame来渲染ImGui（在Clear之后，这样ImGui不会被清除）
        host.GetModuleRegistry().InvokePhase(ModulePhase::PostFrame, frameArgs);

        renderer->EndFrame();
        renderer->Present();

        frameIndex++;

        // 限制帧率（减少CPU占用）
        SDL_Delay(16);  // ~60 FPS
    }

    Logger::GetInstance().Info("[ImGuiDemo] Total frames: " + std::to_string(frameIndex));

    // 清理
    host.Shutdown();
    asyncLoader.Shutdown();
    Renderer::Destroy(renderer);

    Logger::GetInstance().Info("[ImGuiDemo] Exiting...");
    return 0;
}

