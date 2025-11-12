#include <render/application/application_host.h>
#include <render/application/modules/core_render_module.h>
#include <render/application/modules/debug_hud_module.h>
#include <render/application/scenes/boot_scene.h>
#include <render/async_resource_loader.h>
#include <render/logger.h>
#include <render/renderer.h>
#include <render/resource_manager.h>

#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Render;
using namespace Render::Application;

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    Logger::GetInstance().SetLogToConsole(false);
    Logger::GetInstance().SetLogToFile(false);

    Renderer renderer;
    if (!renderer.Initialize("ApplicationBootSceneTest", 320, 240)) {
        std::cerr << "[application_boot_scene_test] Renderer initialization failed." << std::endl;
        return 1;
    }

    auto& resourceManager = ResourceManager::GetInstance();
    auto& asyncLoader = AsyncResourceLoader::GetInstance();
    asyncLoader.Initialize(1);

    ApplicationHost host;
    ApplicationHost::Config config;
    config.renderer = &renderer;
    config.resourceManager = &resourceManager;
    config.asyncLoader = &asyncLoader;
    config.uniformManager = nullptr;
    config.createWorldIfMissing = true;

    if (!host.Initialize(config)) {
        std::cerr << "[application_boot_scene_test] ApplicationHost initialization failed." << std::endl;
        asyncLoader.Shutdown();
        renderer.Shutdown();
        return 1;
    }

    auto& modules = host.GetModuleRegistry();
    if (!modules.RegisterModule(std::make_unique<CoreRenderModule>())) {
        std::cerr << "[application_boot_scene_test] Failed to register CoreRenderModule." << std::endl;
        host.Shutdown();
        asyncLoader.Shutdown();
        renderer.Shutdown();
        return 1;
    }
    modules.RegisterModule(std::make_unique<DebugHUDModule>(), true);

    host.RegisterSceneFactory("BootScene", []() {
        return std::make_unique<BootScene>();
    });

    if (!host.PushScene("BootScene")) {
        std::cerr << "[application_boot_scene_test] Failed to push BootScene." << std::endl;
        host.Shutdown();
        asyncLoader.Shutdown();
        renderer.Shutdown();
        return 1;
    }

    double absoluteTime = 0.0;
    const float deltaTime = 0.016f;

    for (uint64_t frameIndex = 0; frameIndex < 5; ++frameIndex) {
        FrameUpdateArgs frame{};
        frame.deltaTime = deltaTime;
        frame.absoluteTime = absoluteTime;
        frame.frameIndex = frameIndex;

        renderer.BeginFrame();
        renderer.Clear();

        host.UpdateFrame(frame);
        host.UpdateWorld(deltaTime);

        renderer.FlushRenderQueue();
        renderer.EndFrame();
        renderer.Present();

        absoluteTime += deltaTime;
    }

    host.Shutdown();
    asyncLoader.Shutdown();
    renderer.Shutdown();

    std::cout << "[application_boot_scene_test] BootScene executed successfully." << std::endl;
    return 0;
}


