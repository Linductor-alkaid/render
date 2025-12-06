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
#include <render/application/application_host.h>
#include <render/application/modules/core_render_module.h>
#include <render/application/scenes/boot_scene.h>
#include <render/async_resource_loader.h>
#include <render/ecs/components.h>
#include <render/ecs/world.h>
#include <render/renderer.h>
#include <render/resource_manager.h>
#include <render/shader_cache.h>
#include <render/logger.h>

#include <filesystem>
#include <iostream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Render;
using namespace Render::Application;

namespace {

constexpr const char* kCubeMeshName = "boot.demo.mesh";
constexpr const char* kCubeMaterialName = "boot.demo.material";
constexpr const char* kCubeShaderName = "boot.demo.shader";
constexpr const char* kCubeEntityName = "BootScene.Cube";

bool ValidateCubeEntity(ECS::World& world) {
    auto& entityManager = world.GetEntityManager();
    auto entities = entityManager.GetAllEntities();
    for (const auto& entity : entities) {
        if (entityManager.GetName(entity) == kCubeEntityName) {
            if (!world.HasComponent<ECS::MeshRenderComponent>(entity)) {
                std::cerr << "[application_boot_scene_sync_test] Cube entity missing MeshRenderComponent\n";
                return false;
            }
            const auto& meshComponent = world.GetComponent<ECS::MeshRenderComponent>(entity);
            if (!meshComponent.resourcesLoaded) {
                std::cerr << "[application_boot_scene_sync_test] MeshRenderComponent.resourcesLoaded is false\n";
                return false;
            }
            if (!meshComponent.mesh || !meshComponent.material) {
                std::cerr << "[application_boot_scene_sync_test] Mesh/Material handles are null\n";
                return false;
            }
            if (!meshComponent.mesh->IsUploaded()) {
                std::cerr << "[application_boot_scene_sync_test] Mesh has not been uploaded to GPU\n";
                return false;
            }
            return true;
        }
    }
    std::cerr << "[application_boot_scene_sync_test] BootScene.Cube entity not found\n";
    return false;
}

} // namespace

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    try {
        std::filesystem::current_path(PROJECT_SOURCE_DIR);
    } catch (const std::exception& ex) {
        std::cerr << "[application_boot_scene_sync_test] Failed to set working directory: "
                  << ex.what() << "\n";
        return 1;
    }

    auto& logger = Logger::GetInstance();
    logger.SetLogToConsole(false);
    logger.SetLogToFile(false);

    Renderer renderer;
    if (!renderer.Initialize("BootScene Sync Test", 320, 240)) {
        std::cerr << "[application_boot_scene_sync_test] Renderer initialization failed.\n";
        return 1;
    }

    auto& resourceManager = ResourceManager::GetInstance();
    auto& asyncLoader = AsyncResourceLoader::GetInstance();
    asyncLoader.Initialize();

    ApplicationHost host;
    ApplicationHost::Config config;
    config.renderer = &renderer;
    config.resourceManager = &resourceManager;
    config.asyncLoader = &asyncLoader;
    config.uniformManager = nullptr;
    config.createWorldIfMissing = true;

    if (!host.Initialize(config)) {
        std::cerr << "[application_boot_scene_sync_test] ApplicationHost initialization failed.\n";
        asyncLoader.Shutdown();
        renderer.Shutdown();
        return 1;
    }

    auto& modules = host.GetModuleRegistry();
    if (!modules.RegisterModule(std::make_unique<CoreRenderModule>())) {
        std::cerr << "[application_boot_scene_sync_test] Failed to register CoreRenderModule.\n";
        host.Shutdown();
        asyncLoader.Shutdown();
        renderer.Shutdown();
        return 1;
    }

    host.RegisterSceneFactory("BootScene", []() {
        return std::make_unique<BootScene>();
    });

    if (!host.PushScene("BootScene")) {
        std::cerr << "[application_boot_scene_sync_test] Failed to push BootScene.\n";
        host.Shutdown();
        asyncLoader.Shutdown();
        renderer.Shutdown();
        return 1;
    }

    double absoluteTime = 0.0;
    constexpr float deltaTime = 0.016f;

    for (uint64_t frameIndex = 0; frameIndex < 3; ++frameIndex) {
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

    bool success = true;

    if (!resourceManager.HasMesh(kCubeMeshName) ||
        !resourceManager.HasMaterial(kCubeMaterialName)) {
        std::cerr << "[application_boot_scene_sync_test] ResourceManager missing required mesh/material.\n";
        success = false;
    }
    if (!ShaderCache::GetInstance().HasShader(kCubeShaderName)) {
        std::cerr << "[application_boot_scene_sync_test] ShaderCache missing required shader.\n";
        success = false;
    }

    auto& world = host.GetWorld();
    if (!ValidateCubeEntity(world)) {
        success = false;
    }

    host.Shutdown();
    asyncLoader.Shutdown();
    renderer.Shutdown();

    if (!success) {
        return 1;
    }

    std::cout << "[application_boot_scene_sync_test] BootScene synchronous resource loading verified.\n";
    return 0;
}


