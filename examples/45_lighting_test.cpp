#include "render/renderer.h"
#include "render/logger.h"
#include "render/shader_cache.h"
#include "render/material.h"
#include "render/mesh_loader.h"
#include "render/resource_manager.h"
#include "render/math_utils.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/ecs/systems.h"
#include <SDL3/SDL.h>
#include <cmath>

using namespace Render;
using namespace Render::ECS;

namespace {

struct LightingEntities {
    EntityID pointLight;
    EntityID spotLight;
    EntityID centerpiece;
};

void RegisterResources(ResourceManager& resMgr, const std::shared_ptr<Material>& groundMat,
                       const std::shared_ptr<Material>& objectMat,
                       const Ref<Mesh>& groundMesh, const Ref<Mesh>& sphereMesh,
                       const Ref<Mesh>& columnMesh) {
    resMgr.RegisterMaterial("lighting_ground_mat", groundMat);
    resMgr.RegisterMaterial("lighting_object_mat", objectMat);
    resMgr.RegisterMesh("lighting_ground_mesh", groundMesh);
    resMgr.RegisterMesh("lighting_sphere_mesh", sphereMesh);
    resMgr.RegisterMesh("lighting_column_mesh", columnMesh);
}

LightingEntities CreateScene(World& world, const Ref<Mesh>& groundMesh, const Ref<Mesh>& sphereMesh,
                             const Ref<Mesh>& columnMesh,
                             const std::shared_ptr<Material>& groundMat,
                             const std::shared_ptr<Material>& objectMat) {
    LightingEntities entities{};

    EntityID ground = world.CreateEntity({ .name = "Ground" });
    TransformComponent groundTransform;
    groundTransform.SetPosition(Vector3(0.0f, -0.01f, 0.0f));
    groundTransform.SetScale(Vector3(1.0f, 1.0f, 1.0f));
    world.AddComponent(ground, groundTransform);

    MeshRenderComponent groundRender;
    groundRender.mesh = groundMesh;
    groundRender.meshName = "lighting_ground_mesh";
    groundRender.material = groundMat;
    groundRender.materialName = "lighting_ground_mat";
    groundRender.resourcesLoaded = true;
    groundRender.receiveShadows = true;
    world.AddComponent(ground, groundRender);

    EntityID centerpiece = world.CreateEntity({ .name = "Centerpiece" });
    TransformComponent centerTransform;
    centerTransform.SetPosition(Vector3(0.0f, 1.5f, 0.0f));
    centerTransform.SetScale(2.0f);
    world.AddComponent(centerpiece, centerTransform);

    MeshRenderComponent centerRender;
    centerRender.mesh = sphereMesh;
    centerRender.meshName = "lighting_sphere_mesh";
    centerRender.material = objectMat;
    centerRender.materialName = "lighting_object_mat";
    centerRender.resourcesLoaded = true;
    world.AddComponent(centerpiece, centerRender);
    entities.centerpiece = centerpiece;

    for (int i = 0; i < 4; ++i) {
        EntityID column = world.CreateEntity({ .name = "Column_" + std::to_string(i) });
        TransformComponent columnTransform;
        float angle = MathUtils::DegreesToRadians(90.0f * i);
        float radius = 6.0f;
        columnTransform.SetPosition(Vector3(std::cos(angle) * radius, 1.5f, std::sin(angle) * radius));
        columnTransform.SetScale(Vector3(0.6f, 3.0f, 0.6f));
        world.AddComponent(column, columnTransform);

        MeshRenderComponent columnRender;
        columnRender.mesh = columnMesh;
        columnRender.meshName = "lighting_column_mesh";
        columnRender.material = objectMat;
        columnRender.materialName = "lighting_object_mat";
        columnRender.resourcesLoaded = true;
        world.AddComponent(column, columnRender);
    }

    EntityID sun = world.CreateEntity({ .name = "SunLight" });
    TransformComponent sunTransform;
    sunTransform.SetPosition(Vector3(-5.0f, 10.0f, 4.0f));
    sunTransform.transform->LookAt(Vector3(0.0f, 0.0f, 0.0f));
    world.AddComponent(sun, sunTransform);

    LightComponent sunLight;
    sunLight.type = LightType::Directional;
    sunLight.color = Color(1.0f, 0.97f, 0.9f, 1.0f);
    sunLight.intensity = 1.2f;
    sunLight.castShadows = true;
    sunLight.enabled = true;
    world.AddComponent(sun, sunLight);

    EntityID point = world.CreateEntity({ .name = "PointLight" });
    TransformComponent pointTransform;
    pointTransform.SetPosition(Vector3(4.0f, 3.0f, 0.0f));
    world.AddComponent(point, pointTransform);

    LightComponent pointLight;
    pointLight.type = LightType::Point;
    pointLight.color = Color(1.0f, 0.6f, 0.3f, 1.0f);
    pointLight.intensity = 4.0f;
    pointLight.range = 12.0f;
    pointLight.attenuation = 0.12f;
    pointLight.enabled = true;
    world.AddComponent(point, pointLight);
    entities.pointLight = point;

    EntityID spot = world.CreateEntity({ .name = "SpotLight" });
    TransformComponent spotTransform;
    spotTransform.SetPosition(Vector3(-6.0f, 5.0f, 2.0f));
    spotTransform.transform->LookAt(Vector3(0.0f, 1.0f, 0.0f));
    world.AddComponent(spot, spotTransform);

    LightComponent spotLight;
    spotLight.type = LightType::Spot;
    spotLight.color = Color(0.35f, 0.6f, 1.0f, 1.0f);
    spotLight.intensity = 6.0f;
    spotLight.range = 15.0f;
    spotLight.attenuation = 0.18f;
    spotLight.innerConeAngle = 18.0f;
    spotLight.outerConeAngle = 28.0f;
    spotLight.enabled = true;
    world.AddComponent(spot, spotLight);
    entities.spotLight = spot;

    return entities;
}

void SetupCamera(World& world) {
    EntityID cameraEntity = world.CreateEntity({ .name = "MainCamera" });

    TransformComponent cameraTransform;
    cameraTransform.SetPosition(Vector3(0.0f, 6.0f, 16.0f));
    cameraTransform.transform->LookAt(Vector3(0.0f, 1.5f, 0.0f));
    world.AddComponent(cameraEntity, cameraTransform);

    CameraComponent cameraComp;
    cameraComp.camera = std::make_shared<Camera>();
    cameraComp.camera->SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 200.0f);
    cameraComp.active = true;
    world.AddComponent(cameraEntity, cameraComp);
}

} // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Logger::GetInstance().Info("=== Lighting System Test ===");

    Renderer* renderer = Renderer::Create();
    if (!renderer) {
        Logger::GetInstance().Error("Failed to create renderer instance");
        return 1;
    }

    if (!renderer->Initialize("45_lighting_test", 1280, 720)) {
        Logger::GetInstance().Error("Failed to initialize renderer");
        Renderer::Destroy(renderer);
        return 1;
    }

    renderer->SetVSync(true);
    renderer->SetClearColor(0.05f, 0.06f, 0.1f, 1.0f);

    auto& shaderCache = ShaderCache::GetInstance();
    auto phongShader = shaderCache.LoadShader("lighting_phong", "shaders/material_phong.vert", "shaders/material_phong.frag");
    if (!phongShader) {
        Logger::GetInstance().Error("Failed to load Phong shader");
        Renderer::Destroy(renderer);
        return 1;
    }

    auto groundMaterial = std::make_shared<Material>();
    groundMaterial->SetName("LightingGround");
    groundMaterial->SetShader(phongShader);
    groundMaterial->SetDiffuseColor(Color(0.25f, 0.3f, 0.35f, 1.0f));
    groundMaterial->SetAmbientColor(Color(0.15f, 0.18f, 0.2f, 1.0f));
    groundMaterial->SetSpecularColor(Color(0.05f, 0.05f, 0.05f, 1.0f));
    groundMaterial->SetShininess(6.0f);

    auto objectMaterial = std::make_shared<Material>();
    objectMaterial->SetName("LightingObject");
    objectMaterial->SetShader(phongShader);
    objectMaterial->SetDiffuseColor(Color(0.85f, 0.4f, 0.25f, 1.0f));
    objectMaterial->SetAmbientColor(Color(0.2f, 0.1f, 0.08f, 1.0f));
    objectMaterial->SetSpecularColor(Color(1.0f, 0.9f, 0.8f, 1.0f));
    objectMaterial->SetShininess(48.0f);

    auto groundMesh = MeshLoader::CreatePlane(30.0f, 30.0f, 6, 6, Color::White());
    auto sphereMesh = MeshLoader::CreateSphere(1.0f, 48, 24, Color::White());
    auto columnMesh = MeshLoader::CreateCylinder(0.5f, 0.5f, 3.0f, 24, Color::White());

    auto& resMgr = ResourceManager::GetInstance();
    RegisterResources(resMgr, groundMaterial, objectMaterial, groundMesh, sphereMesh, columnMesh);

    World world;
    world.Initialize();

    world.RegisterComponent<TransformComponent>();
    world.RegisterComponent<MeshRenderComponent>();
    world.RegisterComponent<CameraComponent>();
    world.RegisterComponent<LightComponent>();
    world.RegisterComponent<ActiveComponent>();

    world.RegisterSystem<TransformSystem>();
    world.RegisterSystem<CameraSystem>();
    world.RegisterSystem<LightSystem>(renderer);
    world.RegisterSystem<UniformSystem>(renderer);
    world.RegisterSystem<MeshRenderSystem>(renderer);
    world.PostInitialize();

    SetupCamera(world);
    LightingEntities entities = CreateScene(world, groundMesh, sphereMesh, columnMesh, groundMaterial, objectMaterial);

    bool running = true;
    uint64_t lastTicks = SDL_GetTicks();
    float timeAccumulator = 0.0f;

    Logger::GetInstance().Info("Controls: ESC to exit");

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

        uint64_t currentTicks = SDL_GetTicks();
        float deltaTime = static_cast<float>(currentTicks - lastTicks) / 1000.0f;
        lastTicks = currentTicks;
        timeAccumulator += deltaTime;

        auto& pointTransform = world.GetComponent<TransformComponent>(entities.pointLight);
        Vector3 pointPos(std::cos(timeAccumulator) * 5.0f, 2.5f + std::sin(timeAccumulator * 0.5f) * 0.5f, std::sin(timeAccumulator) * 5.0f);
        pointTransform.SetPosition(pointPos);

        auto& spotTransform = world.GetComponent<TransformComponent>(entities.spotLight);
        Vector3 spotPos(-5.0f, 4.5f, 2.0f + std::sin(timeAccumulator * 0.8f) * 3.0f);
        spotTransform.SetPosition(spotPos);
        spotTransform.transform->LookAt(Vector3(0.0f, 1.5f, 0.0f));

        auto& centerpieceTransform = world.GetComponent<TransformComponent>(entities.centerpiece);
        Quaternion rotation = MathUtils::FromEulerDegrees(0.0f, timeAccumulator * 35.0f, 0.0f);
        centerpieceTransform.SetRotation(rotation);

        renderer->BeginFrame();
        renderer->Clear();

        world.Update(deltaTime);
        renderer->FlushRenderQueue();

        renderer->EndFrame();
        renderer->Present();
    }

    world.Shutdown();
    Renderer::Destroy(renderer);
    Logger::GetInstance().Info("=== Lighting System Test Finished ===");
    return 0;
}
