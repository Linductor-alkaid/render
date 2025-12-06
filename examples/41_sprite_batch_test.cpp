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
#include "render/logger.h"
#include "render/renderer.h"
#include "render/texture_loader.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/ecs/systems.h"
#include "render/sprite/sprite_layer.h"

using namespace Render;
using namespace Render::ECS;

namespace {

void SpawnSprite(World& world,
                 const Ref<Texture>& texture,
                 const std::string& textureName,
                 const Vector3& position,
                 bool screenSpace = true,
                 const std::string& layerName = "ui.default",
                 int32_t localOrder = 0) {
    EntityDescriptor desc{};
    desc.name = screenSpace ? "UI_Sprite" : "World_Sprite";
    EntityID entity = world.CreateEntity(desc);

    TransformComponent transform;
    transform.SetPosition(position);
    transform.SetScale(Vector3(1.0f, 1.0f, 1.0f));
    world.AddComponent(entity, transform);

    SpriteRenderComponent sprite;
    sprite.texture = texture;
    sprite.textureName = textureName;
    sprite.screenSpace = screenSpace;
    sprite.resourcesLoaded = true;
    sprite.size = Vector2(128.0f, 128.0f);
    sprite.sourceRect = Rect(0, 0, static_cast<float>(texture->GetWidth()), static_cast<float>(texture->GetHeight()));

    if (!SpriteRenderLayer::ApplyLayer(layerName, sprite, localOrder)) {
        sprite.layerID = screenSpace ? 800u : 700u;
        sprite.sortOrder = localOrder;
    }
    world.AddComponent(entity, sprite);
}

} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    Logger::GetInstance().SetLogToFile(false);
    Logger::GetInstance().Info("=== Sprite Batch Test ===");

    auto renderer = std::make_unique<Renderer>();
    if (!renderer->Initialize("Sprite Batch Test", 800, 600)) {
        Logger::GetInstance().Error("[SpriteBatchTest] Renderer initialization failed");
        return -1;
    }
    renderer->SetBatchingMode(BatchingMode::GpuInstancing);

    AsyncResourceLoader::GetInstance().Initialize();

    auto world = std::make_shared<World>();
    world->Initialize();

    world->RegisterComponent<TransformComponent>();
    world->RegisterComponent<SpriteRenderComponent>();
    world->RegisterComponent<NameComponent>();

    world->RegisterSystem<TransformSystem>();
    world->RegisterSystem<SpriteRenderSystem>(renderer.get());

    world->PostInitialize();

    const std::string textureName = "sprite_batch_test_texture";
    auto texture = TextureLoader::GetInstance().LoadTexture(textureName, "textures/test.jpg", true);
    if (!texture) {
        Logger::GetInstance().Error("[SpriteBatchTest] Failed to load texture");
        return -1;
    }

    for (int i = 0; i < 12; ++i) {
        Vector3 position(50.0f + i * 60.0f, 100.0f + (i % 3) * 70.0f, 0.0f);
        SpawnSprite(*world, texture, textureName, position, true, "ui.default", i);
    }

    auto* spriteSystem = world->GetSystem<SpriteRenderSystem>();

    bool running = true;
    int frames = 0;
    while (running && frames < 5) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        renderer->BeginFrame();
        world->Update(renderer->GetDeltaTime());
        renderer->Clear(true, true, false);
        renderer->FlushRenderQueue();
        renderer->EndFrame();
        renderer->Present();

        ++frames;
    }

    size_t batchCount = spriteSystem ? spriteSystem->GetLastBatchCount() : 0;
    Logger::GetInstance().InfoFormat("[SpriteBatchTest] Detected sprite batches: %zu", batchCount);
    if (batchCount == 0) {
        Logger::GetInstance().Warning("[SpriteBatchTest] Sprite batching did not produce any batches.");
    } else {
        Logger::GetInstance().Info("[SpriteBatchTest] Sprite batching is active.");
    }

    world->Shutdown();
    AsyncResourceLoader::GetInstance().Shutdown();
    renderer->Shutdown();

    Logger::GetInstance().Info("=== Sprite Batch Test Completed ===");
    return 0;
}

