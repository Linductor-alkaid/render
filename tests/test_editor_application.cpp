/*
 * Copyright (c) 2026 Li Chaoyu
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
#include <SDL3/SDL.h>

#include "render/application/modules/core_render_module.h"
#include "render/async_resource_loader.h"
#include "render/editor/editor_application.h"
#include "render/editor/editor_context.h"
#include "render/editor/editor_state.h"
#include "render/logger.h"
#include "render/renderer.h"
#include "render/resource_manager.h"

using namespace Render;
using namespace Render::Editor;

namespace {

void ConfigureLogger() {
    auto& logger = Logger::GetInstance();
    logger.SetLogToConsole(true);
    logger.SetLogToFile(false);
    logger.SetLogLevel(LogLevel::Info);
}

bool InitializeRenderer(Renderer*& renderer) {
    renderer = Renderer::Create();
    if (!renderer) {
        Logger::GetInstance().Error("[TestEditorApplication] Failed to create renderer");
        return false;
    }

    if (!renderer->Initialize("Editor Application Test", 1280, 720)) {
        Logger::GetInstance().Error("[TestEditorApplication] Failed to initialize renderer");
        Renderer::Destroy(renderer);
        renderer = nullptr;
        return false;
    }

    renderer->SetClearColor(0.1f, 0.12f, 0.16f, 1.0f);
    renderer->SetVSync(true);
    return true;
}

void TestEditorState(EditorState& state) {
    Logger::GetInstance().Info("[TestEditorApplication] Testing EditorState...");

    // 测试初始状态
    if (state.GetCurrentMode() != EditorMode::SceneEdit) {
        Logger::GetInstance().Error("[TestEditorApplication] Initial mode should be SceneEdit");
        return;
    }
    Logger::GetInstance().Info("[TestEditorApplication] Initial mode is correct: SceneEdit");

    // 测试模式切换
    if (!state.SetMode(EditorMode::AnimationEdit)) {
        Logger::GetInstance().Error("[TestEditorApplication] Failed to switch to AnimationEdit mode");
        return;
    }
    if (state.GetCurrentMode() != EditorMode::AnimationEdit) {
        Logger::GetInstance().Error("[TestEditorApplication] Mode should be AnimationEdit");
        return;
    }
    Logger::GetInstance().Info("[TestEditorApplication] Successfully switched to AnimationEdit mode");

    // 测试状态保存和恢复
    state.SaveState(EditorMode::SceneEdit);
    state.SetMode(EditorMode::PhysicsEdit);
    state.RestoreState(EditorMode::SceneEdit);
    Logger::GetInstance().Info("[TestEditorApplication] State save/restore test passed");

    // 测试切换保护
    state.SetCanSwitch(false);
    if (state.SetMode(EditorMode::URDFEdit)) {
        Logger::GetInstance().Error("[TestEditorApplication] Should not be able to switch when CanSwitch is false");
        return;
    }
    Logger::GetInstance().Info("[TestEditorApplication] Switch protection test passed");

    state.SetCanSwitch(true);
    state.SetMode(EditorMode::SceneEdit);
    Logger::GetInstance().Info("[TestEditorApplication] EditorState tests passed");
}

void TestEditorContext(EditorContext& context) {
    Logger::GetInstance().Info("[TestEditorApplication] Testing EditorContext...");

    // 测试引擎服务访问
    try {
        auto& world = context.GetWorld();
        (void)world; // 避免未使用警告
        Logger::GetInstance().Info("[TestEditorApplication] GetWorld() works");

        auto& resourceManager = context.GetResourceManager();
        (void)resourceManager; // 避免未使用警告
        Logger::GetInstance().Info("[TestEditorApplication] GetResourceManager() works");

        auto& sceneManager = context.GetSceneManager();
        (void)sceneManager; // 避免未使用警告
        Logger::GetInstance().Info("[TestEditorApplication] GetSceneManager() works");

        auto& moduleRegistry = context.GetModuleRegistry();
        (void)moduleRegistry; // 避免未使用警告
        Logger::GetInstance().Info("[TestEditorApplication] GetModuleRegistry() works");

        auto& renderer = context.GetRenderer();
        (void)renderer; // 避免未使用警告
        Logger::GetInstance().Info("[TestEditorApplication] GetRenderer() works");
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("[TestEditorApplication] Error accessing services: %s", e.what());
        return;
    }

    // 测试配置管理
    auto& config = context.GetConfig();
    config.projectPath = "test_project";
    config.darkTheme = true;
    config.autoSave = true;
    config.autoSaveInterval = 300;

    const std::string testConfigPath = "test_editor_config.json";
    if (!context.SaveConfig(testConfigPath)) {
        Logger::GetInstance().Error("[TestEditorApplication] Failed to save config");
        return;
    }
    Logger::GetInstance().Info("[TestEditorApplication] Config saved successfully");

    // 重置配置并重新加载
    config.projectPath = "";
    config.darkTheme = false;
    config.autoSave = false;

    if (!context.LoadConfig(testConfigPath)) {
        Logger::GetInstance().Error("[TestEditorApplication] Failed to load config");
        return;
    }

    if (config.projectPath != "test_project") {
        Logger::GetInstance().Error("[TestEditorApplication] Config projectPath not loaded correctly");
        return;
    }
    if (!config.darkTheme) {
        Logger::GetInstance().Error("[TestEditorApplication] Config darkTheme not loaded correctly");
        return;
    }
    Logger::GetInstance().Info("[TestEditorApplication] Config loaded and verified successfully");

    // 清理测试文件(可选)
    // std::remove(testConfigPath.c_str());

    Logger::GetInstance().Info("[TestEditorApplication] EditorContext tests passed");
}

} // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    ConfigureLogger();

    try {
        Renderer* renderer = nullptr;
        if (!InitializeRenderer(renderer)) {
            return -1;
        }

        auto& resourceManager = ResourceManager::GetInstance();
        auto& asyncLoader = AsyncResourceLoader::GetInstance();
        asyncLoader.Initialize();

        // 创建编辑器应用
        EditorApplication editor;
        EditorApplication::Config editorConfig{};
        editorConfig.appConfig.renderer = renderer;
        editorConfig.appConfig.resourceManager = &resourceManager;
        editorConfig.appConfig.asyncLoader = &asyncLoader;
        editorConfig.configPath = "editor_test_config.json";

        if (!editor.Initialize(editorConfig)) {
            Logger::GetInstance().Error("[TestEditorApplication] Failed to initialize EditorApplication");
            asyncLoader.Shutdown();
            Renderer::Destroy(renderer);
            return -1;
        }

        Logger::GetInstance().Info("[TestEditorApplication] EditorApplication initialized successfully");

        // 注册核心模块
        auto& moduleRegistry = editor.GetApplicationHost().GetModuleRegistry();
        moduleRegistry.RegisterModule(std::make_unique<Render::Application::CoreRenderModule>());
        Logger::GetInstance().Info("[TestEditorApplication] Registered CoreRenderModule");

        // 测试EditorState
        TestEditorState(editor.GetState());

        // 测试EditorContext
        TestEditorContext(editor.GetContext());

        // 运行几帧以验证更新循环
        Logger::GetInstance().Info("[TestEditorApplication] Running update loop for 3 frames...");
        uint64_t frameIndex = 0;
        double absoluteTime = 0.0;

        for (int i = 0; i < 3; ++i) {
            renderer->BeginFrame();
            renderer->Clear();

            const float deltaTime = renderer->GetDeltaTime();
            absoluteTime += static_cast<double>(deltaTime);

            Application::FrameUpdateArgs frameArgs{};
            frameArgs.deltaTime = deltaTime;
            frameArgs.absoluteTime = absoluteTime;
            frameArgs.frameIndex = frameIndex++;

            editor.Update(frameArgs);

            renderer->EndFrame();
        }

        Logger::GetInstance().Info("[TestEditorApplication] Update loop test passed");

        // 关闭编辑器
        editor.Shutdown();
        Logger::GetInstance().Info("[TestEditorApplication] EditorApplication shutdown successfully");

        asyncLoader.Shutdown();
        Renderer::Destroy(renderer);

        Logger::GetInstance().Info("[TestEditorApplication] All tests passed!");
        return 0;

    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("[TestEditorApplication] Exception: %s", e.what());
        return -1;
    }
}
