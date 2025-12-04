#include <SDL3/SDL.h>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <limits>

#include "render/logger.h"
#include "render/renderer.h"
#include "render/async_resource_loader.h"
#include "render/texture_loader.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/ecs/systems.h"
#include "render/sprite/sprite_layer.h"

using namespace Render;
using namespace Render::ECS;

namespace {

void RunFrames(World& world, Renderer& renderer, int frameCount = 3) {
    for (int i = 0; i < frameCount; ++i) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                return;
            }
        }

        renderer.BeginFrame();
        world.Update(renderer.GetDeltaTime());
        renderer.Clear(true, true, false);
        renderer.FlushRenderQueue();
        renderer.EndFrame();
        renderer.Present();
    }
}

void SpawnSprite(World& world,
                 const Ref<Texture>& texture,
                 const std::string& textureName,
                 const Vector3& position,
                 bool screenSpace,
                 const std::string& layerName,
                 int32_t sortOffset = 0) {
    EntityDescriptor desc{};
    desc.name = screenSpace ? "Sprite.Screen" : "Sprite.World";
    EntityID entity = world.CreateEntity(desc);

    TransformComponent transform;
    transform.SetPosition(position);
    transform.SetScale(Vector3(1.0f, 1.0f, 1.0f));
    world.AddComponent(entity, transform);

    SpriteRenderComponent sprite;
    sprite.texture = texture;
    sprite.textureName = textureName;
    sprite.size = Vector2(128.0f, 128.0f);
    sprite.sourceRect = Rect(0, 0,
                             static_cast<float>(texture ? texture->GetWidth() : 128),
                             static_cast<float>(texture ? texture->GetHeight() : 128));
    sprite.tintColor = Color::White();
    sprite.screenSpace = screenSpace;
    sprite.resourcesLoaded = true;

    if (!SpriteRenderLayer::ApplyLayer(layerName, sprite, sortOffset)) {
        sprite.layerID = screenSpace ? 800u : 700u;
        sprite.sortOrder = sortOffset;
    }

    world.AddComponent(entity, sprite);
}

std::shared_ptr<World> CreateWorld(Renderer* renderer) {
    auto world = std::make_shared<World>();
    world->Initialize();

    world->RegisterComponent<TransformComponent>();
    world->RegisterComponent<SpriteRenderComponent>();
    world->RegisterComponent<NameComponent>();

    world->RegisterSystem<TransformSystem>();
    world->RegisterSystem<SpriteRenderSystem>(renderer);

    world->PostInitialize();
    return world;
}

struct Scenario {
    std::string name;
    size_t expectedBatches;
    size_t expectedSprites = std::numeric_limits<size_t>::max();
    std::function<void(World&)> setup;
};

} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    Logger::GetInstance().SetLogToFile(false);
    Logger::GetInstance().Info("=== Sprite Batch Validation Test ===");

    auto renderer = std::make_unique<Renderer>();
    if (!renderer->Initialize("Sprite Batch Validation Test", 800, 600)) {
        Logger::GetInstance().Error("[SpriteBatchValidationTest] Renderer initialization failed");
        return -1;
    }
    renderer->SetBatchingMode(BatchingMode::GpuInstancing);

    AsyncResourceLoader::GetInstance().Initialize();

    const std::string baseTextureName = "sprite_batch_validation_texture_base";
    const std::string altTextureName = "sprite_batch_validation_texture_alt";

    auto baseTexture = TextureLoader::GetInstance().LoadTexture(baseTextureName, "textures/test.jpg", true);
    auto altTexture = TextureLoader::GetInstance().LoadTexture(altTextureName, "textures/test.jpg", true);

    if (!baseTexture || !altTexture) {
        Logger::GetInstance().Error("[SpriteBatchValidationTest] Failed to load required textures");
        AsyncResourceLoader::GetInstance().Shutdown();
        renderer->Shutdown();
        return -1;
    }

    struct ScenarioResult {
        std::string name;
        size_t expectedBatches = 0;
        size_t actualBatches = 0;
        bool batchPassed = false;
        size_t expectedSprites = std::numeric_limits<size_t>::max();
        size_t actualSprites = 0;
        bool spritePassed = true;
    };

    std::vector<Scenario> scenarios;
    scenarios.push_back(Scenario{
        "SingleTextureScreenSpace",
        1,
        std::numeric_limits<size_t>::max(),
        [&](World& world) {
            for (int i = 0; i < 12; ++i) {
                Vector3 position(40.0f + static_cast<float>(i) * 60.0f,
                                 80.0f + static_cast<float>(i % 3) * 60.0f,
                                 0.0f);
                SpawnSprite(world, baseTexture, baseTextureName, position, true, "ui.default", i);
            }
        }});

    scenarios.push_back(Scenario{
        "TwoTexturesSameLayer",
        2,
        std::numeric_limits<size_t>::max(),
        [&](World& world) {
            for (int i = 0; i < 6; ++i) {
                Vector3 position(50.0f + static_cast<float>(i) * 70.0f, 120.0f, 0.0f);
                SpawnSprite(world, baseTexture, baseTextureName, position, true, "ui.default", i);
            }
            for (int i = 0; i < 6; ++i) {
                Vector3 position(50.0f + static_cast<float>(i) * 70.0f, 220.0f, 0.0f);
                SpawnSprite(world, altTexture, altTextureName, position, true, "ui.default", i + 10);
            }
        }});

    scenarios.push_back(Scenario{
        "MixedScreenAndWorld",
        2,
        std::numeric_limits<size_t>::max(),
        [&](World& world) {
            for (int i = 0; i < 5; ++i) {
                Vector3 position(60.0f + static_cast<float>(i) * 80.0f, 140.0f, 0.0f);
                SpawnSprite(world, baseTexture, baseTextureName, position, true, "ui.default", i);
            }
            for (int i = 0; i < 5; ++i) {
                Vector3 position(-2.0f + static_cast<float>(i) * 1.2f, 0.0f, -1.0f);
                SpawnSprite(world, baseTexture, baseTextureName, position, false, "world.midground", i);
            }
        }});

    scenarios.push_back(Scenario{
        "DifferentLayersSameTexture",
        3,
        std::numeric_limits<size_t>::max(),
        [&](World& world) {
            SpawnSprite(world, baseTexture, baseTextureName, Vector3(120.0f, 150.0f, 0.0f), true, "ui.background", 0);
            SpawnSprite(world, baseTexture, baseTextureName, Vector3(220.0f, 150.0f, 0.0f), true, "ui.default", 0);
            SpawnSprite(world, baseTexture, baseTextureName, Vector3(320.0f, 150.0f, 0.0f), true, "ui.foreground", 0);
        }});

    scenarios.push_back(Scenario{
        "NineSliceSingleSprite",
        1,
        9,
        [&](World& world) {
            EntityDescriptor desc{};
            desc.name = "UI_Panel_NineSlice";
            EntityID entity = world.CreateEntity(desc);

            TransformComponent transform;
            transform.SetPosition(Vector3(320.0f, 240.0f, 0.0f));
            world.AddComponent(entity, transform);

            SpriteRenderComponent sprite;
            sprite.texture = baseTexture;
            sprite.textureName = baseTextureName;
            sprite.resourcesLoaded = true;
            sprite.screenSpace = true;
            sprite.size = Vector2(420.0f, 260.0f);
            sprite.sourceRect = Rect(0, 0,
                                     static_cast<float>(baseTexture->GetWidth()),
                                     static_cast<float>(baseTexture->GetHeight()));
            sprite.tintColor = Color(0.85f, 0.95f, 1.0f, 1.0f);
            sprite.nineSlice.borderPixels = Vector4(48.0f, 48.0f, 48.0f, 48.0f);
            sprite.snapToPixel = true;
            sprite.subPixelOffset = Vector2(0.5f, 0.0f);
            sprite.flipFlags = SpriteUI::SpriteFlipFlags::None;
            SpriteRenderLayer::ApplyLayer("ui.default", sprite, 0);

            world.AddComponent(entity, sprite);
        }});

    scenarios.push_back(Scenario{
        "MirroredPanelsSharedBatch",
        1,
        18,
        [&](World& world) {
            for (int i = 0; i < 2; ++i) {
                EntityDescriptor desc{};
                desc.name = (i == 0) ? "UI_Panel_FlipX" : "UI_Panel_FlipY";
                EntityID entity = world.CreateEntity(desc);

                TransformComponent transform;
                transform.SetPosition(Vector3(200.0f + i * 220.0f, 460.0f, 0.0f));
                world.AddComponent(entity, transform);

                SpriteRenderComponent sprite;
                sprite.texture = baseTexture;
                sprite.textureName = baseTextureName;
                sprite.resourcesLoaded = true;
                sprite.screenSpace = true;
                sprite.size = Vector2(320.0f, 180.0f);
                sprite.sourceRect = Rect(0, 0,
                                         static_cast<float>(baseTexture->GetWidth()),
                                         static_cast<float>(baseTexture->GetHeight()));
                sprite.tintColor = (i == 0) ? Color(1.0f, 0.8f, 0.8f, 1.0f) : Color(0.8f, 1.0f, 0.8f, 1.0f);
                sprite.nineSlice.borderPixels = Vector4(32.0f, 32.0f, 32.0f, 32.0f);
                sprite.snapToPixel = true;
                sprite.subPixelOffset = Vector2(0.0f, i == 0 ? 0.25f : -0.25f);
                sprite.flipFlags = (i == 0)
                    ? SpriteUI::SpriteFlipFlags::FlipX
                    : SpriteUI::SpriteFlipFlags::FlipY;
                SpriteRenderLayer::ApplyLayer("ui.default", sprite, i);

                world.AddComponent(entity, sprite);
            }
        }});

    std::vector<ScenarioResult> results;
    results.reserve(scenarios.size());

    for (const auto& scenario : scenarios) {
        auto world = CreateWorld(renderer.get());
        scenario.setup(*world);

        RunFrames(*world, *renderer, 5);

        auto* spriteSystem = world->GetSystem<SpriteRenderSystem>();
        size_t actualBatches = spriteSystem ? spriteSystem->GetLastBatchCount() : 0;
        size_t actualSprites = spriteSystem ? spriteSystem->GetLastSubmittedSpriteCount() : 0;

        ScenarioResult result{
            .name = scenario.name,
            .expectedBatches = scenario.expectedBatches,
            .actualBatches = actualBatches,
            .batchPassed = (actualBatches == scenario.expectedBatches),
            .expectedSprites = scenario.expectedSprites,
            .actualSprites = actualSprites,
            .spritePassed = (scenario.expectedSprites == std::numeric_limits<size_t>::max()) ||
                            (scenario.expectedSprites == actualSprites)
        };
        results.push_back(result);

        if (result.batchPassed) {
            Logger::GetInstance().InfoFormat("[SpriteBatchValidationTest] Scenario '%s' batch count OK (batches=%zu)",
                                             scenario.name.c_str(), actualBatches);
        } else {
            Logger::GetInstance().ErrorFormat("[SpriteBatchValidationTest] Scenario '%s' failed: expected %zu, actual %zu",
                                              scenario.name.c_str(), scenario.expectedBatches, actualBatches);
        }

        if (scenario.expectedSprites != std::numeric_limits<size_t>::max()) {
            if (result.spritePassed) {
                Logger::GetInstance().InfoFormat("[SpriteBatchValidationTest] Scenario '%s' sprite submissions OK (sprites=%zu)",
                                                 scenario.name.c_str(), actualSprites);
            } else {
                Logger::GetInstance().ErrorFormat("[SpriteBatchValidationTest] Scenario '%s' sprite submissions mismatch: expected %zu, actual %zu",
                                                  scenario.name.c_str(), scenario.expectedSprites, actualSprites);
            }
        }

        world->Shutdown();
    }

    bool allPassed = true;
    for (const auto& result : results) {
        if (!result.batchPassed || !result.spritePassed) {
            allPassed = false;
            break;
        }
    }

    if (allPassed) {
        Logger::GetInstance().Info("[SpriteBatchValidationTest] All batching scenarios passed.");
    } else {
        Logger::GetInstance().Error("[SpriteBatchValidationTest] Some batching scenarios failed.");
    }

    AsyncResourceLoader::GetInstance().Shutdown();
    renderer->Shutdown();
    TextureLoader::GetInstance().RemoveTexture(baseTextureName);
    TextureLoader::GetInstance().RemoveTexture(altTextureName);

    Logger::GetInstance().Info("=== Sprite Batch Validation Test Completed ===");
    return allPassed ? 0 : -1;
}


