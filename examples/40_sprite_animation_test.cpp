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
#include <SDL3/SDL.h>
#include <cmath>
#include <memory>
#include <vector>

#include "render/logger.h"
#include "render/renderer.h"
#include "render/resource_manager.h"
#include "render/texture_loader.h"
#include "render/sprite/sprite_atlas_importer.h"
#include "render/texture.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/ecs/systems.h"
#include "render/async_resource_loader.h"

using namespace Render;
using namespace Render::ECS;

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    Logger::GetInstance().SetLogToFile(true);
    Logger::GetInstance().Info("=== Sprite Animation Test ===");

    auto renderer = std::make_unique<Renderer>();
    if (!renderer->Initialize("Sprite Animation Test", 1280, 720)) {
        Logger::GetInstance().Error("Failed to initialize renderer");
        return -1;
    }

    auto renderState = renderer->GetRenderState();
    renderState->SetDepthTest(true);
    renderState->SetDepthWrite(true);
    renderState->SetBlendMode(BlendMode::Alpha);
    renderState->SetCullFace(CullFace::None);
    renderState->SetClearColor(Color(0.15f, 0.17f, 0.2f, 1.0f));

    AsyncResourceLoader::GetInstance().Initialize();

    auto world = std::make_shared<World>();
    world->Initialize();

    world->RegisterComponent<TransformComponent>();
    world->RegisterComponent<SpriteRenderComponent>();
    world->RegisterComponent<SpriteAnimationComponent>();
    world->RegisterComponent<CameraComponent>();
    world->RegisterComponent<NameComponent>();

    world->RegisterSystem<TransformSystem>();
    world->RegisterSystem<CameraSystem>();
    world->RegisterSystem<SpriteAnimationSystem>();
    world->RegisterSystem<SpriteRenderSystem>(renderer.get());
    world->RegisterSystem<UniformSystem>(renderer.get());

    world->PostInitialize();

    Logger::GetInstance().Info("[SpriteAnimationTest] World post-initialized");

    // Create world-space sprite with camera
    EntityDescriptor cameraDesc{};
    cameraDesc.name = "MainCamera";
    EntityID cameraEntity = world->CreateEntity(cameraDesc);

    TransformComponent cameraTransform;
    cameraTransform.SetPosition(Vector3(0.0f, 0.0f, 6.0f));
    cameraTransform.SetRotation(MathUtils::FromEulerDegrees(0.0f, 0.0f, 0.0f));
    world->AddComponent(cameraEntity, cameraTransform);

    auto camera = std::make_shared<Camera>();
    float aspect = 1.0f;
    int width = renderer->GetWidth();
    int height = renderer->GetHeight();
    if (height > 0) {
        aspect = static_cast<float>(width) / static_cast<float>(height);
    }
    camera->SetPerspective(60.0f, aspect, 0.1f, 100.0f);

    CameraComponent cameraComp;
    cameraComp.camera = camera;
    cameraComp.active = true;
    cameraComp.depth = 0;
    world->AddComponent(cameraEntity, cameraComp);

    if (auto* cameraSystem = world->GetSystem<CameraSystem>()) {
        cameraSystem->SetMainCamera(cameraEntity);
    }

    Logger::GetInstance().Info("[SpriteAnimationTest] Camera entity created and set as main camera");

    // Load sprite atlas
    const std::string atlasPath = "assets/atlases/test_sprite_atlas.json";
    std::string atlasError;
    auto atlasResultOpt = SpriteAtlasImporter::LoadFromFile(atlasPath, "demo_sprite_atlas", atlasError);
    if (!atlasResultOpt.has_value()) {
        Logger::GetInstance().Error("[SpriteAnimationTest] Failed to load sprite atlas: " + atlasError);
        return -1;
    }

    auto atlasResult = atlasResultOpt.value();
    auto spriteAtlas = atlasResult.atlas;
    auto spriteTexture = spriteAtlas->GetTexture();
    if (!spriteTexture) {
        Logger::GetInstance().Error("[SpriteAnimationTest] Atlas texture is null");
        return -1;
    }

    // Register atlas to resource manager
    auto& resourceManager = ResourceManager::GetInstance();
    if (!resourceManager.HasSpriteAtlas(spriteAtlas->GetName())) {
        resourceManager.RegisterSpriteAtlas(spriteAtlas->GetName(), spriteAtlas);
    }

    Logger::GetInstance().Info("[SpriteAnimationTest] Sprite atlas loaded successfully");

    // Create 2D UI sprite (screen space)
    EntityDescriptor uiDesc{};
    uiDesc.name = "UI_Sprite";
    EntityID uiEntity = world->CreateEntity(uiDesc);

    TransformComponent uiTransform;
    uiTransform.SetPosition(Vector3(200.0f, 200.0f, 0.0f));
    uiTransform.SetScale(Vector3(1.0f, 1.0f, 1.0f));
    world->AddComponent(uiEntity, uiTransform);

    SpriteRenderComponent uiSprite;
    uiSprite.textureName = "sprite_animation_demo";
    uiSprite.texture = spriteTexture;
    if (atlasResult.spriteSheet.HasFrame("tile_0")) {
        const auto& frame = atlasResult.spriteSheet.GetFrame("tile_0");
        uiSprite.sourceRect = frame.uv;
        uiSprite.size = frame.size;
    } else {
        uiSprite.size = Vector2(256.0f, 256.0f);
    }
    uiSprite.screenSpace = true;
    uiSprite.layerID = 800;
    uiSprite.resourcesLoaded = true;
    world->AddComponent(uiEntity, uiSprite);

    SpriteAnimationComponent uiAnim = atlasResult.animationComponent;
    uiAnim.Play("pulse");
    world->AddComponent(uiEntity, uiAnim);

    Logger::GetInstance().Info("[SpriteAnimationTest] UI sprite entity created");

    EntityDescriptor worldSpriteDesc{};
    worldSpriteDesc.name = "WorldSprite";
    EntityID worldSpriteEntity = world->CreateEntity(worldSpriteDesc);

    TransformComponent worldSpriteTransform;
    worldSpriteTransform.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
    worldSpriteTransform.SetScale(Vector3(1.0f, 1.0f, 1.0f));
    world->AddComponent(worldSpriteEntity, worldSpriteTransform);

    SpriteRenderComponent worldSprite;
    worldSprite.textureName = "sprite_animation_demo";
    worldSprite.texture = spriteTexture;
    if (atlasResult.spriteSheet.HasFrame("tile_0")) {
        const auto& frame = atlasResult.spriteSheet.GetFrame("tile_0");
        worldSprite.sourceRect = frame.uv;
    }
    worldSprite.size = Vector2(1.0f, 1.0f);
    worldSprite.screenSpace = false;
    worldSprite.layerID = 300;
    worldSprite.resourcesLoaded = true;
    world->AddComponent(worldSpriteEntity, worldSprite);

    SpriteAnimationComponent worldAnim = atlasResult.animationComponent;
    worldAnim.Play("rotate");
    world->AddComponent(worldSpriteEntity, worldAnim);

    Logger::GetInstance().Info("[SpriteAnimationTest] World-space sprite entity created");

    auto uiTransformRef = world->GetComponent<TransformComponent>(uiEntity).transform;
    auto worldTransformRef = world->GetComponent<TransformComponent>(worldSpriteEntity).transform;
    auto cameraTransformRef = world->GetComponent<TransformComponent>(cameraEntity).transform;

    bool running = true;
    const float kMaxRuntimeSeconds = 12.0f;
    float elapsedTime = 0.0f;
    float runtime = 0.0f;
    int loggedFrames = 0;
    float transformLogTimer = 0.0f;
    float worldMovePosition = -2.0f;
    constexpr float kWorldMoveMin = -2.0f;
    constexpr float kWorldMoveMax = 2.0f;
    constexpr float kWorldMoveSpeed = 2.5f;
    float worldMoveDirection = 1.0f;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
                running = false;
            }
        }

        renderer->BeginFrame();
        float deltaTime = renderer->GetDeltaTime();
        elapsedTime += deltaTime;
        runtime += deltaTime;

        if (loggedFrames < 5) {
            Logger::GetInstance().InfoFormat("[SpriteAnimationTest] Frame %d, deltaTime=%.4f", loggedFrames, deltaTime);
            ++loggedFrames;
        }

        if (uiTransformRef) {
            float pulse = 0.9f + 0.2f * std::sin(elapsedTime * 2.0f);
            uiTransformRef->SetScale(Vector3(pulse, pulse, 1.0f));
        }

        if (worldTransformRef) {
            Quaternion rotation = MathUtils::FromEulerDegrees(0.0f, elapsedTime * 45.0f, 0.0f);
            worldTransformRef->SetRotation(rotation);

            worldMovePosition += worldMoveDirection * kWorldMoveSpeed * deltaTime;
            if (worldMovePosition > kWorldMoveMax) {
                worldMovePosition = kWorldMoveMax;
                worldMoveDirection = -1.0f;
            } else if (worldMovePosition < kWorldMoveMin) {
                worldMovePosition = kWorldMoveMin;
                worldMoveDirection = 1.0f;
            }

            float verticalAmplitude = 0.5f;
            Vector3 worldPos(
                worldMovePosition,
                std::sin(elapsedTime * 2.0f) * verticalAmplitude,
                0.0f
            );
            worldTransformRef->SetPosition(worldPos);
        }

        if (cameraTransformRef) {
            cameraTransformRef->SetPosition(Vector3(0.0f, 1.5f, 6.0f));
            cameraTransformRef->LookAt(Vector3::Zero());
        }

        world->Update(deltaTime);

        transformLogTimer += deltaTime;
        if (transformLogTimer >= 0.5f) {
            transformLogTimer = 0.0f;
            if (worldTransformRef) {
                Vector3 pos = worldTransformRef->GetWorldPosition();
                Logger::GetInstance().InfoFormat(
                    "[SpriteAnimationTest] World sprite worldPos=(%.2f, %.2f, %.2f)",
                    pos.x(), pos.y(), pos.z());
            }
            if (cameraTransformRef) {
                Vector3 camPos = cameraTransformRef->GetWorldPosition();
                Logger::GetInstance().InfoFormat(
                    "[SpriteAnimationTest] Camera worldPos=(%.2f, %.2f, %.2f) looking at origin",
                    camPos.x(), camPos.y(), camPos.z());
            }
        }

        renderer->Clear(true, true, false);
        renderer->FlushRenderQueue();
        renderer->EndFrame();
        renderer->Present();

        if (runtime >= kMaxRuntimeSeconds) {
            running = false;
        }

        SDL_Delay(16);
    }

    Logger::GetInstance().Info("[SpriteAnimationTest] Main loop exited");

    Logger::GetInstance().Info("[SpriteAnimationTest] Begin cleanup: world shutdown");
    world->Shutdown();

    Logger::GetInstance().Info("[SpriteAnimationTest] Cleanup: release sprite texture");
    spriteTexture.reset();

    Logger::GetInstance().Info("[SpriteAnimationTest] Cleanup: remove texture cache entry");
    TextureLoader::GetInstance().RemoveTexture("sprite_animation_demo");

    Logger::GetInstance().Info("[SpriteAnimationTest] Cleanup: texture cleanup unused");
    TextureLoader::GetInstance().CleanupUnused();

    Logger::GetInstance().Info("[SpriteAnimationTest] Cleanup: shutdown async resource loader");
    AsyncResourceLoader::GetInstance().Shutdown();

    Logger::GetInstance().Info("[SpriteAnimationTest] Cleanup: shutdown renderer");
    renderer->Shutdown();

    Logger::GetInstance().Info("=== Sprite Animation Test Completed ===");
    return 0;
}
