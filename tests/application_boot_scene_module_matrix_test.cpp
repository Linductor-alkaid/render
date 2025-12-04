#include <filesystem>
#include <iostream>
#include <vector>

#include <render/application/application_host.h>
#include <render/application/modules/core_render_module.h>
#include <render/application/modules/debug_hud_module.h>
#include <render/application/modules/input_module.h>
#include <render/application/modules/ui_runtime_module.h>
#include <render/application/scenes/boot_scene.h>
#include <render/async_resource_loader.h>
#include <render/ecs/components.h>
#include <render/ecs/world.h>
#include <render/logger.h>
#include <render/renderer.h>
#include <render/resource_manager.h>
#include <render/shader_cache.h>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Render;
using namespace Render::Application;

namespace {

struct ScenarioConfig {
    const char* name;
    bool enableInput = false;
    bool enableUIRuntime = false;
    bool enableDebugHUD = false;
};

constexpr ScenarioConfig kScenarios[] = {
    {"CoreOnly", false, false, false},
    // {"CorePlusInput", true, false, false},
    // {"CorePlusInputUI", true, true, false},
    // {"CorePlusAll", true, true, true},
};

constexpr const char* kCubeMeshName = "boot.demo.mesh";
constexpr const char* kCubeMaterialName = "boot.demo.material";
constexpr const char* kCubeShaderName = "boot.demo.shader";
constexpr const char* kCubeEntityName = "BootScene.Cube";

bool ValidateCubeEntity(ECS::World& world) {
    auto& entityManager = world.GetEntityManager();
    for (const auto& entity : entityManager.GetAllEntities()) {
        if (entityManager.GetName(entity) == kCubeEntityName) {
            if (!world.HasComponent<ECS::MeshRenderComponent>(entity)) {
                std::cerr << "[application_boot_scene_module_matrix_test] entity missing MeshRenderComponent\n";
                return false;
            }
            const auto& meshComponent = world.GetComponent<ECS::MeshRenderComponent>(entity);
            if (!meshComponent.resourcesLoaded || !meshComponent.mesh || !meshComponent.material) {
                std::cerr << "[application_boot_scene_module_matrix_test] mesh/material not ready\n";
                return false;
            }
            if (!meshComponent.mesh->IsUploaded()) {
                std::cerr << "[application_boot_scene_module_matrix_test] mesh not uploaded\n";
                return false;
            }
            return true;
        }
    }
    std::cerr << "[application_boot_scene_module_matrix_test] BootScene.Cube entity not found\n";
    return false;
}

bool RunScenario(const ScenarioConfig& scenario) {
    std::cout << "[application_boot_scene_module_matrix_test] Scenario: " << scenario.name << std::endl;

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    try {
        std::filesystem::current_path(PROJECT_SOURCE_DIR);
    } catch (const std::exception& ex) {
        std::cerr << "[application_boot_scene_module_matrix_test] Failed to set working directory: " << ex.what()
                  << std::endl;
        return false;
    }

    auto& logger = Logger::GetInstance();
    logger.SetLogToConsole(false);
    logger.SetLogToFile(false);

    Renderer renderer;
    if (!renderer.Initialize("BootScene Module Matrix Test", 320, 240)) {
        std::cerr << "[application_boot_scene_module_matrix_test] Renderer initialization failed.\n";
        return false;
    }

    auto& resourceManager = ResourceManager::GetInstance();
    auto& shaderCache = ShaderCache::GetInstance();
    auto& asyncLoader = AsyncResourceLoader::GetInstance();
    if (asyncLoader.IsInitialized()) {
        asyncLoader.Shutdown();
    }
    asyncLoader.Initialize();

    ApplicationHost host;
    ApplicationHost::Config config;
    config.renderer = &renderer;
    config.resourceManager = &resourceManager;
    config.asyncLoader = &asyncLoader;
    config.uniformManager = nullptr;
    config.createWorldIfMissing = true;

    if (!host.Initialize(config)) {
        std::cerr << "[application_boot_scene_module_matrix_test] ApplicationHost initialization failed.\n";
        asyncLoader.Shutdown();
        renderer.Shutdown();
        return false;
    }

    auto& modules = host.GetModuleRegistry();
    if (!modules.RegisterModule(std::make_unique<CoreRenderModule>())) {
        std::cerr << "[application_boot_scene_module_matrix_test] Failed to register CoreRenderModule.\n";
        host.Shutdown();
        asyncLoader.Shutdown();
        renderer.Shutdown();
        return false;
    }
    if (scenario.enableInput) {
        modules.RegisterModule(std::make_unique<InputModule>());
    }
    if (scenario.enableUIRuntime) {
        modules.RegisterModule(std::make_unique<UIRuntimeModule>());
    }
    if (scenario.enableDebugHUD) {
        modules.RegisterModule(std::make_unique<DebugHUDModule>(), false);
    }

    host.RegisterSceneFactory("BootScene", []() { return std::make_unique<BootScene>(); });
    if (!host.PushScene("BootScene")) {
        std::cerr << "[application_boot_scene_module_matrix_test] Failed to push BootScene.\n";
        host.Shutdown();
        asyncLoader.Shutdown();
        renderer.Shutdown();
        return false;
    }

    const float deltaTime = 0.016f;
    double absoluteTime = 0.0;
    const uint64_t maxFrames = 60;
    bool success = true;

    for (uint64_t frameIndex = 0; frameIndex < maxFrames && success; ++frameIndex) {
        FrameUpdateArgs frame{};
        frame.deltaTime = deltaTime;
        frame.absoluteTime = absoluteTime;
        frame.frameIndex = frameIndex;

        renderer.BeginFrame();
        renderer.Clear();

        try {
            std::cout << "[application_boot_scene_module_matrix_test] (" << scenario.name << ") frame "
                      << frameIndex << " PreFrame" << std::endl;
            modules.InvokePhase(ModulePhase::PreFrame, frame);
            std::cout << "[application_boot_scene_module_matrix_test] (" << scenario.name << ") frame "
                      << frameIndex << " PreFrame done" << std::endl;
        } catch (const std::exception& ex) {
            std::cerr << "[application_boot_scene_module_matrix_test] PreFrame exception: "
                      << ex.what() << std::endl;
            success = false;
        } catch (...) {
            std::cerr << "[application_boot_scene_module_matrix_test] PreFrame threw unknown exception\n";
            success = false;
        }

        if (success) {
            try {
                std::cout << "[application_boot_scene_module_matrix_test] (" << scenario.name << ") frame "
                          << frameIndex << " SceneManager.Update" << std::endl;
                host.GetSceneManager().Update(frame);
                std::cout << "[application_boot_scene_module_matrix_test] (" << scenario.name << ") frame "
                          << frameIndex << " SceneManager.Update done" << std::endl;
            } catch (const std::exception& ex) {
                std::cerr << "[application_boot_scene_module_matrix_test] SceneManager.Update exception: "
                          << ex.what() << std::endl;
                success = false;
            } catch (...) {
                std::cerr << "[application_boot_scene_module_matrix_test] SceneManager.Update threw unknown exception\n";
                success = false;
            }
        }

        if (success) {
            try {
                std::cout << "[application_boot_scene_module_matrix_test] (" << scenario.name << ") frame "
                          << frameIndex << " PostFrame" << std::endl;
                modules.InvokePhase(ModulePhase::PostFrame, frame);
                std::cout << "[application_boot_scene_module_matrix_test] (" << scenario.name << ") frame "
                          << frameIndex << " PostFrame done" << std::endl;
            } catch (const std::exception& ex) {
                std::cerr << "[application_boot_scene_module_matrix_test] PostFrame exception: "
                          << ex.what() << std::endl;
                success = false;
            } catch (...) {
                std::cerr << "[application_boot_scene_module_matrix_test] PostFrame threw unknown exception\n";
                success = false;
            }
        }

        host.GetContext().lastFrame = frame;

        if (success) {
            try {
                std::cout << "[application_boot_scene_module_matrix_test] (" << scenario.name << ") frame "
                          << frameIndex << " UpdateWorld" << std::endl;
                host.UpdateWorld(deltaTime);
                std::cout << "[application_boot_scene_module_matrix_test] (" << scenario.name << ") frame "
                          << frameIndex << " UpdateWorld done" << std::endl;
            } catch (const std::exception& ex) {
                std::cerr << "[application_boot_scene_module_matrix_test] host.UpdateWorld exception: "
                          << ex.what() << std::endl;
                success = false;
            } catch (...) {
                std::cerr << "[application_boot_scene_module_matrix_test] host.UpdateWorld threw unknown exception\n";
                success = false;
            }
        }

        renderer.FlushRenderQueue();
        renderer.EndFrame();
        renderer.Present();

        absoluteTime += deltaTime;
    }

    if (!resourceManager.HasMesh(kCubeMeshName) ||
        !resourceManager.HasMaterial(kCubeMaterialName)) {
        std::cerr << "[application_boot_scene_module_matrix_test] ResourceManager missing required mesh/material.\n";
        success = false;
    }
    if (!shaderCache.HasShader(kCubeShaderName)) {
        std::cerr << "[application_boot_scene_module_matrix_test] ShaderCache missing required shader.\n";
        success = false;
    }

    auto& world = host.GetWorld();
    if (!ValidateCubeEntity(world)) {
        success = false;
    }

    host.Shutdown();
    asyncLoader.Shutdown();
    renderer.Shutdown();

    resourceManager.Clear();
    shaderCache.Clear();

    return success;
}

} // namespace

int main() {
    for (const auto& scenario : kScenarios) {
        if (!RunScenario(scenario)) {
            std::cerr << "[application_boot_scene_module_matrix_test] Scenario failed: " << scenario.name << std::endl;
            return 1;
        }
    }

    std::cout << "[application_boot_scene_module_matrix_test] All scenarios passed." << std::endl;
    return 0;
}


