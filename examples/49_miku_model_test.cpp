#include <render/renderer.h>
#include <render/logger.h>
#include <render/shader_cache.h>
#include <render/model_loader.h>
#include <render/resource_manager.h>
#include <render/ecs/world.h>
#include <render/ecs/components.h>
#include <render/ecs/systems.h>
#include <render/math_utils.h>

#include <SDL3/SDL.h>

#include <algorithm>
#include <memory>

using namespace Render;
using namespace Render::ECS;

namespace {

struct SceneConfig {
    Vector3 cameraPosition{0.0f, 1.8f, 5.5f};
    Vector3 cameraTarget{0.0f, 1.5f, 0.0f};
    Vector3 lightPosition{4.0f, 6.0f, 4.0f};
    Color ambientColor{0.2f, 0.2f, 0.25f, 1.0f};
    Color diffuseColor{1.0f, 1.0f, 1.0f, 1.0f};
    Color specularColor{0.6f, 0.6f, 0.6f, 1.0f};
    float shininess = 48.0f;
};

} // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Logger::GetInstance().Info("[MikuModelTest] === Model Rendering | Miku Demo ===");

    Renderer* renderer = Renderer::Create();
    if (!renderer->Initialize("Miku Model Test", 1600, 900)) {
        Logger::GetInstance().Error("[MikuModelTest] Failed to initialize renderer");
        return -1;
    }
    renderer->SetClearColor(Color(0.08f, 0.08f, 0.12f, 1.0f));
    renderer->SetVSync(true);
    if (auto context = renderer->GetContext()) {
        SDL_SetWindowRelativeMouseMode(context->GetWindow(), true);
    }

    auto& shaderCache = ShaderCache::GetInstance();
    auto phongShader = shaderCache.LoadShader(
        "miku_material_phong",
        "shaders/material_phong.vert",
        "shaders/material_phong.frag"
    );
    if (!phongShader || !phongShader->IsValid()) {
        Logger::GetInstance().Error("[MikuModelTest] Failed to load Phong shader");
        Renderer::Destroy(renderer);
        return -1;
    }

    SceneConfig sceneConfig{};

    ModelLoadOptions modelOptions;
    modelOptions.autoUpload = true;
    modelOptions.registerModel = true;
    modelOptions.registerMeshes = true;
    modelOptions.registerMaterials = true;
    modelOptions.resourcePrefix = "miku_demo";
    modelOptions.shaderOverride = phongShader;
    modelOptions.basePath = "models/miku";

    Logger::GetInstance().Info("[MikuModelTest] Loading Miku model...");
    auto loadResult = ModelLoader::LoadFromFile("models/miku/v4c5.0short.pmx", "miku_demo", modelOptions);
    if (!loadResult.model) {
        Logger::GetInstance().Error("[MikuModelTest] Failed to load miku model");
        Renderer::Destroy(renderer);
        return -1;
    }

    auto model = loadResult.model;
    Logger::GetInstance().InfoFormat(
        "[MikuModelTest] Model loaded, parts=%zu, meshes=%zu, materials=%zu",
        model->GetPartCount(),
        loadResult.meshResourceNames.size(),
        loadResult.materialResourceNames.size()
    );

    auto world = std::make_shared<World>();
    world->Initialize();

    world->RegisterComponent<TransformComponent>();
    world->RegisterComponent<ModelComponent>();
    world->RegisterComponent<CameraComponent>();
    world->RegisterComponent<NameComponent>();
    world->RegisterComponent<ActiveComponent>();

    world->RegisterSystem<TransformSystem>();
    world->RegisterSystem<CameraSystem>();
    world->RegisterSystem<UniformSystem>(renderer);
    world->RegisterSystem<ModelRenderSystem>(renderer);

    world->PostInitialize();

    EntityID cameraEntity = world->CreateEntity({ .name = "MainCamera", .active = true });
    TransformComponent cameraTransform;
    cameraTransform.SetPosition(sceneConfig.cameraPosition);
    cameraTransform.LookAt(sceneConfig.cameraTarget);
    world->AddComponent(cameraEntity, cameraTransform);

    CameraComponent cameraComp;
    cameraComp.camera = std::make_shared<Camera>();
    cameraComp.camera->SetPerspective(55.0f, static_cast<float>(renderer->GetWidth()) / renderer->GetHeight(), 0.1f, 200.0f);
    cameraComp.active = true;
    world->AddComponent(cameraEntity, cameraComp);

    EntityID modelEntity = world->CreateEntity({ .name = "Miku", .active = true });
    TransformComponent modelTransform;
    modelTransform.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
    modelTransform.SetRotation(MathUtils::FromEulerDegrees(0.0f, 180.0f, 0.0f));
    modelTransform.SetScale(1.0f);
    world->AddComponent(modelEntity, modelTransform);

    ModelComponent modelComp;
    modelComp.modelName = loadResult.modelName;
    modelComp.loadOptions = modelOptions;
    modelComp.SetModel(model);
    modelComp.registeredMeshNames = loadResult.meshResourceNames;
    modelComp.registeredMaterialNames = loadResult.materialResourceNames;
    modelComp.castShadows = true;
    modelComp.receiveShadows = true;
    world->AddComponent(modelEntity, modelComp);

    bool running = true;
    Uint64 prevTicks = SDL_GetTicks();
    float accumTime = 0.0f;
    Vector3 cameraPosition = sceneConfig.cameraPosition;
    Vector3 toTarget = (sceneConfig.cameraTarget - sceneConfig.cameraPosition).normalized();
    float cameraYaw = MathUtils::RadiansToDegrees(std::atan2(toTarget.x(), -toTarget.z()));
    float cameraPitch = MathUtils::RadiansToDegrees(std::asin(std::clamp(toTarget.y(), -1.0f, 1.0f)));
    bool mouseCaptured = true;

    Logger::GetInstance().Info("[MikuModelTest] Controls: ESC to exit");
    Logger::GetInstance().Info("[MikuModelTest] Controls: WASD 前后左右, Q/E 上下, Shift 加速, 鼠标视角, Tab 捕获/释放鼠标");

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_TAB) {
                mouseCaptured = !mouseCaptured;
                if (auto context = renderer->GetContext()) {
                    SDL_SetWindowRelativeMouseMode(context->GetWindow(), mouseCaptured);
                }
            }
            if (mouseCaptured && event.type == SDL_EVENT_MOUSE_MOTION) {
                constexpr float sensitivity = 0.15f;
                cameraYaw -= static_cast<float>(event.motion.xrel) * sensitivity;
                cameraPitch -= static_cast<float>(event.motion.yrel) * sensitivity;
                cameraPitch = std::clamp(cameraPitch, -89.0f, 89.0f);
            }
        }

        Uint64 currentTicks = SDL_GetTicks();
        float deltaTime = static_cast<float>(currentTicks - prevTicks) / 1000.0f;
        prevTicks = currentTicks;
        deltaTime = std::min(deltaTime, 0.033f);
        accumTime += deltaTime;

        const bool* keyboard = SDL_GetKeyboardState(nullptr);
        float moveSpeed = (keyboard[SDL_SCANCODE_LSHIFT] ? 6.0f : 3.0f) * deltaTime;

        const float yawRad = MathUtils::DegreesToRadians(cameraYaw);
        const float pitchRad = MathUtils::DegreesToRadians(cameraPitch);

        Quaternion yawRotation = MathUtils::AngleAxis(yawRad, Vector3::UnitY());
        Quaternion pitchRotation = MathUtils::AngleAxis(pitchRad, Vector3::UnitX());
        Quaternion viewRotation = yawRotation * pitchRotation;

        Vector3 front = viewRotation * (-Vector3::UnitZ());
        front.normalize();
        Vector3 right = front.cross(Vector3::UnitY()).normalized();
        Vector3 up = right.cross(front).normalized();

        if (keyboard[SDL_SCANCODE_W]) cameraPosition -= front * moveSpeed;
        if (keyboard[SDL_SCANCODE_S]) cameraPosition += front * moveSpeed;
        if (keyboard[SDL_SCANCODE_A]) cameraPosition -= right * moveSpeed;
        if (keyboard[SDL_SCANCODE_D]) cameraPosition += right * moveSpeed;
        if (keyboard[SDL_SCANCODE_Q]) cameraPosition -= Vector3::UnitY() * moveSpeed;
        if (keyboard[SDL_SCANCODE_E]) cameraPosition += Vector3::UnitY() * moveSpeed;

        auto& cameraTransformComp = world->GetComponent<TransformComponent>(cameraEntity);
        cameraTransformComp.SetPosition(cameraPosition);
        cameraTransformComp.SetRotation(viewRotation);

        auto& mikuTransform = world->GetComponent<TransformComponent>(modelEntity);
        Quaternion baseRotation = MathUtils::FromEulerDegrees(0.0f, 180.0f, 0.0f);
        Quaternion spin = MathUtils::FromEulerDegrees(0.0f, std::sin(accumTime * 0.6f) * 15.0f, 0.0f);
        mikuTransform.SetRotation(baseRotation * spin);

        renderer->BeginFrame();
        renderer->Clear();

        if (auto uniformMgr = phongShader->GetUniformManager()) {
            uniformMgr->SetVector3("uLightPos", sceneConfig.lightPosition);
            uniformMgr->SetColor("uAmbientColor", sceneConfig.ambientColor);
            uniformMgr->SetColor("uDiffuseColor", sceneConfig.diffuseColor);
            uniformMgr->SetColor("uSpecularColor", sceneConfig.specularColor);
            uniformMgr->SetFloat("uShininess", sceneConfig.shininess);
            if (uniformMgr->HasUniform("uUseVertexColor")) {
                uniformMgr->SetBool("uUseVertexColor", false);
            }
        }

        world->Update(deltaTime);
        renderer->FlushRenderQueue();

        renderer->EndFrame();
        renderer->Present();

        SDL_Delay(16);
    }

    world->Shutdown();
    Renderer::Destroy(renderer);

    Logger::GetInstance().Info("[MikuModelTest] Shutdown complete");
    return 0;
}


