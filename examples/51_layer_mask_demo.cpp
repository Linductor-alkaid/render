#include "render/renderer.h"
#include "render/logger.h"
#include "render/texture_loader.h"
#include "render/async_resource_loader.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/ecs/systems.h"
#include "render/ecs/entity.h"
#include "render/sprite/sprite_layer.h"
#include "render/math_utils.h"
#include "render/mesh_loader.h"
#include "render/material.h"
#include "render/shader_cache.h"

#include <SDL3/SDL.h>
#include <vector>

using namespace Render;
using namespace Render::ECS;

namespace {

TransformComponent MakeTransform(const Vector3& position, const Quaternion& rotation = Quaternion::Identity()) {
    TransformComponent transform;
    transform.SetPosition(position);
    transform.SetRotation(rotation);
    return transform;
}

SpriteRenderComponent MakeUiSprite(const Ref<Texture>& texture) {
    SpriteRenderComponent sprite;
    sprite.screenSpace = true;
    sprite.layerID = Layers::UI::Default.value;
    sprite.size = Vector2(220.0f, 90.0f);
    sprite.tintColor = Color(1.0f, 1.0f, 1.0f, 0.95f);
    sprite.texture = texture;
    sprite.textureName = "layer_mask_demo_ui";
    sprite.resourcesLoaded = (texture && texture->IsValid());
    sprite.asyncLoading = false;
    sprite.visible = texture != nullptr;
    return sprite;
}

void PrintInstructions() {
    Logger::GetInstance().Info("[LayerMaskDemo] Controls:");
    Logger::GetInstance().Info("  1 - Show world layer only");
    Logger::GetInstance().Info("  2 - Show UI layer only");
    Logger::GetInstance().Info("  3 - Show both world and UI layers");
    Logger::GetInstance().Info("  ESC - Exit");
}

} // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Logger::GetInstance().SetLogToConsole(true);
    Logger::GetInstance().SetLogToFile(false);

    Renderer* renderer = Renderer::Create();
    if (!renderer) {
        Logger::GetInstance().Error("[LayerMaskDemo] Failed to create renderer");
        return -1;
    }

    if (!renderer->Initialize("Layer Mask Demo", 1280, 720)) {
        Logger::GetInstance().Error("[LayerMaskDemo] Failed to initialize renderer");
        Renderer::Destroy(renderer);
        return -1;
    }

    // renderer->SetLODInstancingEnabled(false);

    AsyncResourceLoader::GetInstance().Initialize();

    World world;
    world.RegisterComponent<TransformComponent>();
    world.RegisterComponent<MeshRenderComponent>();
    world.RegisterComponent<SpriteRenderComponent>();
    world.RegisterComponent<CameraComponent>();
    world.Initialize();

    world.RegisterSystem<TransformSystem>();
    world.RegisterSystem<MeshRenderSystem>(renderer);
    world.RegisterSystem<SpriteRenderSystem>(renderer);
    world.RegisterSystem<CameraSystem>();
    world.RegisterSystem<UniformSystem>(renderer);
    world.PostInitialize();

    TextureLoader::GetInstance().RemoveTexture("layer_mask_demo_ui");
    constexpr int kUiTexSize = 64;
    std::vector<uint8_t> uiPixels(static_cast<size_t>(kUiTexSize) * kUiTexSize * 4u, 0);
    for (int y = 0; y < kUiTexSize; ++y) {
        for (int x = 0; x < kUiTexSize; ++x) {
            const float fx = static_cast<float>(x) / static_cast<float>(kUiTexSize - 1);
            const float fy = static_cast<float>(y) / static_cast<float>(kUiTexSize - 1);
            const size_t index = static_cast<size_t>(y) * kUiTexSize * 4u + static_cast<size_t>(x) * 4u;
            uiPixels[index + 0] = static_cast<uint8_t>(255.0f * (0.2f + 0.6f * fx));
            uiPixels[index + 1] = static_cast<uint8_t>(255.0f * (0.4f + 0.4f * fy));
            uiPixels[index + 2] = static_cast<uint8_t>(255.0f * 0.9f);
            uiPixels[index + 3] = 220;
        }
    }
    auto textureUi = TextureLoader::GetInstance().CreateTexture("layer_mask_demo_ui",
                                                                uiPixels.data(),
                                                                kUiTexSize,
                                                                kUiTexSize,
                                                                TextureFormat::RGBA,
                                                                true);
    if (!textureUi || !textureUi->IsValid()) {
        Logger::GetInstance().Warning("[LayerMaskDemo] UI texture creation failed, UI layer will be hidden");
    }

    EntityID uiEntity = world.CreateEntity();
    world.AddComponent(uiEntity, MakeTransform(Vector3(980.0f, 620.0f, 0.0f)));
    world.AddComponent(uiEntity, MakeUiSprite(textureUi));
    auto& uiSpriteComp = world.GetComponent<SpriteRenderComponent>(uiEntity);
    uiSpriteComp.visible = textureUi && textureUi->IsValid();

    Ref<Mesh> worldMesh = MeshLoader::CreateSphere(1.2f, 48, 24);
    if (!worldMesh) {
        Logger::GetInstance().Error("[LayerMaskDemo] Failed to create sphere mesh");
        AsyncResourceLoader::GetInstance().Shutdown();
        Renderer::Destroy(renderer);
        return -1;
    }
    worldMesh->Upload();

    auto basicShader = ShaderCache::GetInstance().LoadShader("layer_mask_demo_basic",
                                                             "shaders/basic.vert",
                                                             "shaders/basic.frag");
    if (!basicShader || !basicShader->IsValid()) {
        Logger::GetInstance().Error("[LayerMaskDemo] Failed to load basic shader");
        AsyncResourceLoader::GetInstance().Shutdown();
        Renderer::Destroy(renderer);
        return -1;
    }

    auto material = std::make_shared<Material>();
    material->SetName("layer_mask_demo_material");
    material->SetShader(basicShader);
    material->SetDiffuseColor(Color(0.25f, 0.7f, 1.0f, 1.0f));
    material->SetAmbientColor(Color(0.3f, 0.3f, 0.3f, 1.0f));
    material->SetSpecularColor(Color(0.0f, 0.0f, 0.0f, 1.0f));
    material->SetShininess(1.0f);
    material->SetCullFace(CullFace::Back);
    material->SetDepthTest(true);
    material->SetDepthWrite(true);
    material->SetColor("uColor", Color(0.25f, 0.7f, 1.0f, 1.0f));

    EntityID worldEntity = world.CreateEntity();
    world.AddComponent(worldEntity, MakeTransform(Vector3(0.0f, 0.0f, 0.0f)));
    MeshRenderComponent sphereRenderable;
    sphereRenderable.layerID = Layers::World::Midground.value;
    sphereRenderable.mesh = worldMesh;
    sphereRenderable.material = material;
    sphereRenderable.meshName = "layer_mask_demo_sphere";
    sphereRenderable.materialName = "layer_mask_demo_material";
    sphereRenderable.resourcesLoaded = true;
    sphereRenderable.asyncLoading = false;
    world.AddComponent(worldEntity, sphereRenderable);

    EntityID cameraEntity = world.CreateEntity();
    TransformComponent cameraTransform = MakeTransform(
        Vector3(0.0f, 0.0f, 5.0f),
        MathUtils::FromEulerDegrees(0.0f, 0.0f, 0.0f));
    world.AddComponent(cameraEntity, cameraTransform);

    CameraComponent cameraComp;
    cameraComp.camera = std::make_shared<Camera>();
    cameraComp.camera->SetPerspective(60.0f, 1280.0f / 720.0f, 0.1f, 100.0f);
    cameraComp.active = true;
    cameraComp.depth = 0;
    cameraComp.clearDepth = true;
    cameraComp.clearStencil = false;
    world.AddComponent(cameraEntity, cameraComp);

    auto* cameraSystem = world.GetSystem<CameraSystem>();
    if (cameraSystem) {
        cameraSystem->SetMainCamera(cameraEntity);
    }

    uint32_t worldMask = 0;
    uint32_t uiMask = 0;
    for (const auto& record : renderer->GetLayerRegistry().ListLayers()) {
        if (record.descriptor.id == Layers::World::Midground) {
            worldMask = 1u << record.descriptor.maskIndex;
        } else if (record.descriptor.id == Layers::UI::Default) {
            uiMask = 1u << record.descriptor.maskIndex;
        }
    }

    auto& cameraComponent = world.GetComponent<CameraComponent>(cameraEntity);
    cameraComponent.layerMask = worldMask | uiMask;

    PrintInstructions();

    bool running = true;
    float elapsed = 0.0f;
    bool uiVisible = uiSpriteComp.visible;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                switch (event.key.key) {
                case SDLK_ESCAPE:
                    running = false;
                    break;
                case 'u':
                case 'U': {
                    uiVisible = !uiVisible;
                    uiSpriteComp.visible = uiVisible;
                    Logger::GetInstance().InfoFormat("[LayerMaskDemo] UI visibility toggled to %s",
                                                     uiVisible ? "ON" : "OFF");
                    break;
                }
                case SDLK_1:
                    cameraComponent.layerMask = worldMask;
                    Logger::GetInstance().Info("[LayerMaskDemo] Showing world layer only");
                    break;
                case SDLK_2:
                    cameraComponent.layerMask = uiMask;
                    Logger::GetInstance().Info("[LayerMaskDemo] Showing UI layer only");
                    break;
                case SDLK_3:
                    cameraComponent.layerMask = worldMask | uiMask;
                    Logger::GetInstance().Info("[LayerMaskDemo] Showing both layers");
                    break;
                default:
                    break;
                }
            }
        }

        renderer->BeginFrame();
        renderer->Clear(true, true, false);
        world.Update(1.0f / 60.0f);
        elapsed += 1.0f / 60.0f;

        if (auto transform = world.GetComponent<TransformComponent>(worldEntity).transform) {
            float angleDeg = MathUtils::RadiansToDegrees(elapsed);
            transform->SetRotation(MathUtils::FromEulerDegrees(angleDeg * 0.6f, angleDeg, angleDeg * 0.3f));
        }

        renderer->FlushRenderQueue();
        renderer->EndFrame();
        renderer->Present();
        SDL_Delay(16);
    }

    world.Shutdown();
    AsyncResourceLoader::GetInstance().Shutdown();
    Renderer::Destroy(renderer);
    return 0;
}


