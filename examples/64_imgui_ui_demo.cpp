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
 * @brief ImGui UI后端演示程序
 * 
 * 本示例展示了如何使用ImGui作为UI渲染后端：
 * - 初始化ImGui后端
 * - 使用ImGui API创建各种UI控件
 * - 显示ImGui示例窗口
 * - 演示基本的ImGui功能
 */

#include <SDL3/SDL.h>

#include "render/application/application_host.h"
#include "render/application/modules/core_render_module.h"
#include "render/application/modules/input_module.h"
#include "render/application/modules/ui_runtime_module.h"
#include "render/application/module_registry.h"
#include "render/ui/ui_renderer_backend.h"
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
} g_demoState;

// 在ImGui后端中渲染自定义UI
void RenderImGuiUI() {
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

