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
 * @file 61_ui_menu_example.cpp
 * @brief UI 菜单系统使用示例
 * 
 * 演示如何使用 UIMenu、UIMenuItem 和 UIPullDownMenu
 * 参考 Blender UI 菜单系统设计
 * 
 * 注意：本示例目前仅展示菜单API的使用方式
 * 完整的UI渲染集成需要等待UI Runtime模块完善
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
#include "render/ui/widgets/ui_menu.h"
#include "render/ui/widgets/ui_menu_item.h"
#include "render/ui/widgets/ui_pulldown_menu.h"

using namespace Render;
using namespace Render::UI;
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
        Logger::GetInstance().Error("[MenuExample] Failed to create renderer");
        return nullptr;
    }

    if (!renderer->Initialize("UI Menu System Example", 1280, 720)) {
        Logger::GetInstance().Error("[MenuExample] Failed to initialize renderer");
        Renderer::Destroy(renderer);
        return nullptr;
    }

    renderer->SetClearColor(0.08f, 0.09f, 0.12f, 1.0f);
    renderer->SetVSync(true);
    return renderer;
}

void DemonstrateMenuAPI() {
    Logger::GetInstance().Info("[MenuExample] Demonstrating Menu API...");
    
    // 创建文件菜单
    auto fileMenu = std::make_shared<UIMenu>("file_menu");
    fileMenu->SetMinWidth(200.0f);
    
    // 添加普通菜单项
    auto newItem = fileMenu->AddMenuItem("file_new", "New");
    newItem->SetShortcut("Ctrl+N");
    newItem->SetOnClicked([](UIMenuItem&) {
        Logger::GetInstance().Info("[MenuExample] Menu Action: New");
    });
    
    auto openItem = fileMenu->AddMenuItem("file_open", "Open...");
    openItem->SetShortcut("Ctrl+O");
    openItem->SetOnClicked([](UIMenuItem&) {
        Logger::GetInstance().Info("[MenuExample] Menu Action: Open");
    });
    
    // 添加最近文件子菜单
    auto recentMenu = std::make_shared<UIMenu>("file_recent_menu");
    recentMenu->AddMenuItem("recent_1", "project1.blend");
    recentMenu->AddMenuItem("recent_2", "project2.blend");
    recentMenu->AddMenuItem("recent_3", "project3.blend");
    fileMenu->AddSubMenuItem("file_recent", "Recent Files", recentMenu);
    
    // 添加分隔符
    fileMenu->AddSeparator();
    
    // 添加保存菜单项
    auto saveItem = fileMenu->AddMenuItem("file_save", "Save");
    saveItem->SetShortcut("Ctrl+S");
    saveItem->SetOnClicked([](UIMenuItem&) {
        Logger::GetInstance().Info("[MenuExample] Menu Action: Save");
    });
    
    auto exitItem = fileMenu->AddMenuItem("file_exit", "Exit");
    exitItem->SetShortcut("Alt+F4");
    exitItem->SetOnClicked([](UIMenuItem&) {
        Logger::GetInstance().Info("[MenuExample] Menu Action: Exit");
    });
    
    // 创建编辑菜单
    auto editMenu = std::make_shared<UIMenu>("edit_menu");
    editMenu->SetMinWidth(200.0f);
    
    auto undoItem = editMenu->AddMenuItem("edit_undo", "Undo");
    undoItem->SetShortcut("Ctrl+Z");
    undoItem->SetOnClicked([](UIMenuItem&) {
        Logger::GetInstance().Info("[MenuExample] Menu Action: Undo");
    });
    
    auto redoItem = editMenu->AddMenuItem("edit_redo", "Redo");
    redoItem->SetShortcut("Ctrl+Y");
    redoItem->SetOnClicked([](UIMenuItem&) {
        Logger::GetInstance().Info("[MenuExample] Menu Action: Redo");
    });
    
    editMenu->AddSeparator();
    
    auto copyItem = editMenu->AddMenuItem("edit_copy", "Copy");
    copyItem->SetShortcut("Ctrl+C");
    
    auto pasteItem = editMenu->AddMenuItem("edit_paste", "Paste");
    pasteItem->SetShortcut("Ctrl+V");
    
    // 创建视图菜单（带可选中项）
    auto viewMenu = std::make_shared<UIMenu>("view_menu");
    viewMenu->SetMinWidth(200.0f);
    
    auto gridItem = viewMenu->AddCheckableItem("view_grid", "Show Grid", true);
    gridItem->SetOnCheckChanged([](UIMenuItem&, bool checked) {
        Logger::GetInstance().Info(checked ? "[MenuExample] View: Grid On" : "[MenuExample] View: Grid Off");
    });
    
    auto axisItem = viewMenu->AddCheckableItem("view_axis", "Show Axis", true);
    axisItem->SetOnCheckChanged([](UIMenuItem&, bool checked) {
        Logger::GetInstance().Info(checked ? "[MenuExample] View: Axis On" : "[MenuExample] View: Axis Off");
    });
    
    auto wireframeItem = viewMenu->AddCheckableItem("view_wireframe", "Wireframe Mode", false);
    wireframeItem->SetOnCheckChanged([](UIMenuItem&, bool checked) {
        Logger::GetInstance().Info(checked ? "[MenuExample] View: Wireframe On" : "[MenuExample] View: Wireframe Off");
    });
    
    viewMenu->AddSeparator();
    
    // 添加摄像机子菜单
    auto cameraMenu = std::make_shared<UIMenu>("view_camera_menu");
    cameraMenu->AddMenuItem("camera_perspective", "Perspective");
    cameraMenu->AddMenuItem("camera_orthographic", "Orthographic");
    cameraMenu->AddMenuItem("camera_front", "Front View");
    cameraMenu->AddMenuItem("camera_side", "Side View");
    cameraMenu->AddMenuItem("camera_top", "Top View");
    viewMenu->AddSubMenuItem("view_camera", "Camera", cameraMenu);
    
    // 创建下拉菜单（未来可集成到UI树）
    auto filePulldown = std::make_unique<UIPullDownMenu>("file_pulldown");
    filePulldown->SetLabel("File");
    filePulldown->SetPlacement(UIMenuPlacement::Below);
    filePulldown->SetMenu(fileMenu);
    
    auto editPulldown = std::make_unique<UIPullDownMenu>("edit_pulldown");
    editPulldown->SetLabel("Edit");
    editPulldown->SetPlacement(UIMenuPlacement::Below);
    editPulldown->SetMenu(editMenu);
    
    auto viewPulldown = std::make_unique<UIPullDownMenu>("view_pulldown");
    viewPulldown->SetLabel("View");
    viewPulldown->SetPlacement(UIMenuPlacement::Below);
    viewPulldown->SetMenu(viewMenu);
    
    Logger::GetInstance().Info("[MenuExample] Menu API demonstration complete");
    Logger::GetInstance().Info("[MenuExample] - Created 3 menus with multiple items");
    Logger::GetInstance().Info("[MenuExample] - Features: Normal items, checkable items, separators, sub-menus");
    Logger::GetInstance().Info("[MenuExample] - Shortcuts: Keyboard shortcuts registered for common actions");
}

} // namespace

int main(int /*argc*/, char* /*argv*/[]) {
    ConfigureLogger();
    Logger::GetInstance().Info("[MenuExample] Starting UI Menu System Example...");

    // 初始化渲染器（Renderer::Initialize 内部会初始化 SDL）
    Renderer* renderer = InitializeRenderer();
    if (!renderer) {
        return 1;
    }

    auto& resourceManager = ResourceManager::GetInstance();
    auto& asyncLoader = AsyncResourceLoader::GetInstance();
    asyncLoader.Initialize();

    // 创建应用主机
    ApplicationHost host;
    ApplicationHost::Config config{};
    config.renderer = renderer;
    config.resourceManager = &resourceManager;
    config.asyncLoader = &asyncLoader;
    config.uniformManager = nullptr;

    if (!host.Initialize(config)) {
        Logger::GetInstance().Error("[MenuExample] ApplicationHost initialization failed");
        asyncLoader.Shutdown();
        Renderer::Destroy(renderer);
        return 1;
    }

    // 注册模块
    host.GetModuleRegistry().RegisterModule(std::make_unique<CoreRenderModule>());
    host.GetModuleRegistry().RegisterModule(std::make_unique<InputModule>());
    host.GetModuleRegistry().RegisterModule(std::make_unique<UIRuntimeModule>());

    auto* inputModule = dynamic_cast<InputModule*>(host.GetModuleRegistry().GetModule("InputModule"));

    // 演示菜单API
    DemonstrateMenuAPI();

    Logger::GetInstance().Info("[MenuExample] Entering main loop...");
    Logger::GetInstance().Info("[MenuExample] Press ESC or close the window to exit");

    bool running = true;
    uint64_t frameIndex = 0;
    double absoluteTime = 0.0;

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

    // 清理
    Logger::GetInstance().Info("[MenuExample] Shutting down...");
    host.Shutdown();
    asyncLoader.Shutdown();
    Renderer::Destroy(renderer);

    Logger::GetInstance().Info("[MenuExample] Shutdown complete");
    return 0;
}
