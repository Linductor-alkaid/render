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
 * @file 60_ui_framework_showcase.cpp
 * @brief UI 框架基础设施演示：初始化 ApplicationHost，注册 CoreRenderModule 与 UIRuntimeModule，
 *        加载占位 UI 图集并驱动帧循环。
 * 
 * 本示例展示了以下 UI 布局系统功能：
 * - Flex 布局：垂直和水平方向的 Flex 布局，支持 justifyContent、alignItems、flexGrow 等属性
 * - Grid 布局：3x2 网格布局，展示 Grid 布局的基本功能
 * - 嵌套布局：Flex 布局中嵌套 Grid 布局
 * - 控件交互：按钮点击、文本输入等基本交互功能
 * 
 * 本示例还展示了新增的高级控件：
 * - UICheckBox：复选框控件（支持选中/未选中/不确定状态）
 * - UIToggle：开关控件（支持动画过渡）
 * - UISlider：滑块控件（支持水平/垂直，支持拖拽交互）
 * - UIRadioButton：单选按钮控件（支持单选组）
 * - UIColorPicker：颜色选择器控件（支持RGB颜色预览）
 * 
 * 布局系统采用两阶段设计（参考 Blender 实现）：
 * - 测量阶段：计算所有节点的理想尺寸
 * - 排列阶段：根据可用空间和布局属性计算最终位置和尺寸
 */

#include <SDL3/SDL.h>

#include "render/application/application_host.h"
#include "render/application/modules/core_render_module.h"
#include "render/application/modules/input_module.h"
#include "render/application/modules/ui_runtime_module.h"
#include "render/logger.h"
#include "render/renderer.h"
#include "render/resource_manager.h"
#include "render/async_resource_loader.h"
#include "render/sprite/sprite_atlas_importer.h"

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
        Logger::GetInstance().Error("[UIShowcase] Failed to create renderer");
        return nullptr;
    }

    if (!renderer->Initialize("UI Framework Showcase", 1280, 720)) {
        Logger::GetInstance().Error("[UIShowcase] Failed to initialize renderer");
        Renderer::Destroy(renderer);
        return nullptr;
    }

    renderer->SetClearColor(0.08f, 0.09f, 0.12f, 1.0f);
    renderer->SetVSync(true);
    renderer->SetBatchingMode(BatchingMode::CpuMerge);
    return renderer;
}

void LoadUiAtlas() {
    std::string error;
    if (!SpriteAtlasImporter::LoadAndRegister("assets/atlases/ui_core.atlas.json", "ui_core", error)) {
        Logger::GetInstance().Warning("[UIShowcase] UI atlas registration skipped: " + error);
    } else {
        Logger::GetInstance().Info("[UIShowcase] ui_core atlas ready.");
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
    config.uniformManager = nullptr; // UniformSystem 会在需要时注册全局 uniform

    if (!host.Initialize(config)) {
        Logger::GetInstance().Error("[UIShowcase] ApplicationHost initialization failed.");
        asyncLoader.Shutdown();
        Renderer::Destroy(renderer);
        return -1;
    }

    host.GetModuleRegistry().RegisterModule(std::make_unique<CoreRenderModule>());
    host.GetModuleRegistry().RegisterModule(std::make_unique<InputModule>());
    host.GetModuleRegistry().RegisterModule(std::make_unique<UIRuntimeModule>());

    auto* inputModule = dynamic_cast<InputModule*>(host.GetModuleRegistry().GetModule("InputModule"));

    LoadUiAtlas();

    bool running = true;
    uint64_t frameIndex = 0;
    double absoluteTime = 0.0;

    Logger::GetInstance().Info("[UIShowcase] UI Framework Showcase");
    Logger::GetInstance().Info("[UIShowcase] - Demonstrates Flex and Grid layout systems");
    Logger::GetInstance().Info("[UIShowcase] - Two-phase layout algorithm (Measure + Arrange)");
    Logger::GetInstance().Info("[UIShowcase] - Nested layout support");
    Logger::GetInstance().Info("[UIShowcase] - Advanced UI Controls:");
    Logger::GetInstance().Info("[UIShowcase]   * UICheckBox (Checkbox with 3-state support)");
    Logger::GetInstance().Info("[UIShowcase]   * UIToggle (Toggle switch with animation)");
    Logger::GetInstance().Info("[UIShowcase]   * UISlider (Horizontal/Vertical slider with drag)");
    Logger::GetInstance().Info("[UIShowcase]   * UIRadioButton (Radio button with group)");
    Logger::GetInstance().Info("[UIShowcase]   * UIColorPicker (Color picker with RGB preview)");
    Logger::GetInstance().Info("[UIShowcase] Press ESC or close the window to exit.");

    while (running) {
        renderer->BeginFrame();
        renderer->Clear();

        const float deltaTime = renderer->GetDeltaTime();
        absoluteTime += static_cast<double>(deltaTime);

        FrameUpdateArgs frameArgs{};
        frameArgs.deltaTime = deltaTime;
        frameArgs.absoluteTime = absoluteTime;
        frameArgs.frameIndex = frameIndex++;

        host.UpdateFrame(frameArgs);
        host.UpdateWorld(deltaTime);

        renderer->FlushRenderQueue();
        renderer->EndFrame();
        renderer->Present();

        asyncLoader.ProcessCompletedTasks(2);

        if (inputModule) {
            if (inputModule->WasQuitRequested() || inputModule->WasKeyPressed(SDL_SCANCODE_ESCAPE)) {
                running = false;
            }
        }
    }

    host.Shutdown();
    asyncLoader.Shutdown();
    Renderer::Destroy(renderer);

    Logger::GetInstance().Info("[UIShowcase] Shutdown complete.");
    return 0;
}


