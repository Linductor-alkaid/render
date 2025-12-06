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
#include <render/renderer.h>
#include <render/logger.h>
#include <render/material.h>
#include <render/shader_cache.h>
#include <render/model_loader.h>
#include <render/camera.h>
#include <render/math_utils.h>
#include <render/ecs/world.h>
#include <render/ecs/components.h>
#include <render/ecs/systems.h>
#include <render/error.h>

#include <SDL3/SDL.h>

#include <algorithm>
#include <memory>
#include <exception>

using namespace Render;
using namespace Render::ECS;

int main() {
    Logger::GetInstance().Info("[ModelRenderTest] === Model Render System Smoke Test ===");

    Renderer* renderer = Renderer::Create();
    if (!renderer->Initialize("Model Render Test", 1280, 720)) {
        Logger::GetInstance().Error("[ModelRenderTest] Failed to initialize renderer");
        return -1;
    }
    Logger::GetInstance().Info("[ModelRenderTest] Renderer initialized");

    auto& shaderCache = ShaderCache::GetInstance();
    auto basicShader = shaderCache.LoadShader("basic_model_test", "shaders/basic.vert", "shaders/basic.frag");
    if (!basicShader || !basicShader->IsValid()) {
        Logger::GetInstance().Error("[ModelRenderTest] Failed to load basic shader");
        Renderer::Destroy(renderer);
        return -1;
    }
    Logger::GetInstance().Info("[ModelRenderTest] Shader loaded");

    ModelLoadOptions loadOptions;
    loadOptions.autoUpload = true;
    loadOptions.registerModel = true;
    loadOptions.registerMeshes = true;
    loadOptions.registerMaterials = true;
    loadOptions.resourcePrefix = "demo48";
    loadOptions.shaderOverride = basicShader;

    Logger::GetInstance().Info("[ModelRenderTest] Loading model from file...");
    Logger::GetInstance().Flush();

    auto loadResult = ModelLoader::LoadFromFile("models/cube.obj", "demo_cube", loadOptions);
    if (!loadResult.model) {
        Logger::GetInstance().Error("[ModelRenderTest] Failed to load model: models/cube.obj");
        Renderer::Destroy(renderer);
        return -1;
    }

    auto model = loadResult.model;
    Logger::GetInstance().InfoFormat(
        "[ModelRenderTest] Model loaded. name=%s, parts=%zu",
        loadResult.modelName.c_str(),
        model->GetPartCount());
    Logger::GetInstance().InfoFormat(
        "[ModelRenderTest] Registered meshes=%zu, materials=%zu",
        loadResult.meshResourceNames.size(),
        loadResult.materialResourceNames.size());
    Logger::GetInstance().Flush();

    Logger::GetInstance().Info("[ModelRenderTest] Creating world...");
    Logger::GetInstance().Flush();

    std::shared_ptr<World> world;
    try {
        world = std::make_shared<World>();
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("[ModelRenderTest] Exception during World construction: %s", e.what());
        Renderer::Destroy(renderer);
        return -1;
    } catch (...) {
        Logger::GetInstance().Error("[ModelRenderTest] Unknown exception during World construction");
        Renderer::Destroy(renderer);
        return -1;
    }

    Logger::GetInstance().Info("[ModelRenderTest] World instance created");
    Logger::GetInstance().Flush();

    world->Initialize();
    Logger::GetInstance().Info("[ModelRenderTest] World initialized");
    Logger::GetInstance().Flush();

    Logger::GetInstance().Info("[ModelRenderTest] Registering components...");
    world->RegisterComponent<TransformComponent>();
    world->RegisterComponent<ModelComponent>();
    world->RegisterComponent<CameraComponent>();
    world->RegisterComponent<NameComponent>();
    world->RegisterComponent<ActiveComponent>();
    Logger::GetInstance().Info("[ModelRenderTest] Components registered");

    Logger::GetInstance().Info("[ModelRenderTest] Registering systems...");
    world->RegisterSystem<CameraSystem>();
    world->RegisterSystem<TransformSystem>();
    world->RegisterSystem<UniformSystem>(renderer);
    world->RegisterSystem<ModelRenderSystem>(renderer);
    Logger::GetInstance().Info("[ModelRenderTest] Systems registered");

    world->PostInitialize();
    Logger::GetInstance().Info("[ModelRenderTest] World PostInitialize complete");

    Logger::GetInstance().Info("[ModelRenderTest] Creating camera entity...");
    EntityID cameraEntity = world->CreateEntity({ .name = "MainCamera", .active = true });
    TransformComponent cameraTransform;
    cameraTransform.SetPosition(Vector3(0.0f, 2.5f, 6.0f));
    cameraTransform.LookAt(Vector3(0.0f, 0.0f, 0.0f));
    world->AddComponent(cameraEntity, cameraTransform);
    Logger::GetInstance().Info("[ModelRenderTest] Camera entity created");

    CameraComponent cameraComp;
    cameraComp.camera = std::make_shared<Camera>();
    cameraComp.camera->SetPerspective(60.0f, 1280.0f / 720.0f, 0.1f, 100.0f);
    cameraComp.active = true;
    world->AddComponent(cameraEntity, cameraComp);
    Logger::GetInstance().Info("[ModelRenderTest] Camera component configured");

    Logger::GetInstance().Info("[ModelRenderTest] Creating model entity...");
    EntityID modelEntity = world->CreateEntity({ .name = "TestModelEntity", .active = true });
    TransformComponent modelTransform;
    modelTransform.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
    world->AddComponent(modelEntity, modelTransform);
    Logger::GetInstance().Info("[ModelRenderTest] Model entity created");

    ModelComponent modelComp;
    modelComp.modelName = loadResult.modelName;
    modelComp.loadOptions = loadOptions;
    modelComp.SetModel(model);
    modelComp.visible = true;
    modelComp.castShadows = true;
    modelComp.receiveShadows = true;
    modelComp.registeredMeshNames = loadResult.meshResourceNames;
    modelComp.registeredMaterialNames = loadResult.materialResourceNames;
    world->AddComponent(modelEntity, modelComp);
    Logger::GetInstance().InfoFormat(
        "[ModelRenderTest] Model component attached (resourcesLoaded=%s)",
        modelComp.resourcesLoaded ? "true" : "false");

    renderer->SetClearColor(Color(0.1f, 0.12f, 0.16f, 1.0f));
    Logger::GetInstance().Info("[ModelRenderTest] Clear color configured");

    bool running = true;
    float angle = 0.0f;
    Uint64 prevTicks = SDL_GetTicks();

    Logger::GetInstance().Info("[ModelRenderTest] Controls: ESC to exit");
    Logger::GetInstance().Info("[ModelRenderTest] Entering main loop");

    try {
        Logger::GetInstance().Info("[ModelRenderTest] Main loop start");
        while (running) {
            SDL_Event evt;
            while (SDL_PollEvent(&evt)) {
                if (evt.type == SDL_EVENT_QUIT) {
                    running = false;
                }
                if (evt.type == SDL_EVENT_KEY_DOWN && evt.key.key == SDLK_ESCAPE) {
                    running = false;
                }
            }

            Uint64 currentTicks = SDL_GetTicks();
            float deltaTime = static_cast<float>(currentTicks - prevTicks) / 1000.0f;
            prevTicks = currentTicks;
            deltaTime = std::min(deltaTime, 0.033f);

            angle += 45.0f * deltaTime;
            if (angle > 360.0f) angle -= 360.0f;

            auto& transform = world->GetComponent<TransformComponent>(modelEntity);
            Quaternion rotation = MathUtils::FromEulerDegrees(0.0f, angle, 0.0f);
            transform.SetRotation(rotation);

            renderer->BeginFrame();
            renderer->Clear();

            world->Update(deltaTime);
            renderer->FlushRenderQueue();

            renderer->EndFrame();
            renderer->Present();

            Logger::GetInstance().Info("[ModelRenderTest] Frame rendered");

            SDL_Delay(16);
        }
        Logger::GetInstance().Info("[ModelRenderTest] Main loop exited");
    } catch (const RenderError& e) {
        Logger::GetInstance().ErrorFormat("[ModelRenderTest] Caught RenderError: %s", e.GetMessage().c_str());
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("[ModelRenderTest] Caught std::exception: %s", e.what());
    } catch (...) {
        Logger::GetInstance().Error("[ModelRenderTest] Caught unknown exception");
    }

    Renderer::Destroy(renderer);
    Logger::GetInstance().Info("[ModelRenderTest] Shutdown complete");
    return 0;
}

