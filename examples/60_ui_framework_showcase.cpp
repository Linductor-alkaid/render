/**
 * @file 60_ui_framework_showcase.cpp
 * @brief UI 框架基础设施演示：初始化 ApplicationHost，注册 CoreRenderModule 与 UIRuntimeModule，
 *        加载占位 UI 图集并驱动帧循环。
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
    asyncLoader.Initialize(1);

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


