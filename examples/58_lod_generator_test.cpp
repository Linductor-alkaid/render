#include <render/renderer.h>
#include <render/logger.h>
#include <render/shader_cache.h>
#include <render/model_loader.h>
#include <render/mesh_loader.h>
#include <render/resource_manager.h>
#include <render/ecs/world.h>
#include <render/ecs/components.h>
#include <render/ecs/systems.h>
#include <render/lod_generator.h>
#include <render/lod_system.h>
#include <render/math_utils.h>

#include <SDL3/SDL.h>
#include <glad/glad.h>

#include <algorithm>
#include <memory>
#include <vector>

using namespace Render;
using namespace Render::ECS;

namespace {

struct SceneConfig {
    Vector3 cameraPosition{0.0f, 1.8f, 8.0f};
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

    Logger::GetInstance().Info("[LODGeneratorTest] === LOD Generator Test | Miku Model ===");

    Renderer* renderer = Renderer::Create();
    if (!renderer->Initialize("LOD Generator Test", 1600, 900)) {
        Logger::GetInstance().Error("[LODGeneratorTest] Failed to initialize renderer");
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
        Logger::GetInstance().Error("[LODGeneratorTest] Failed to load Phong shader");
        Renderer::Destroy(renderer);
        return -1;
    }

    SceneConfig sceneConfig{};

    ModelLoadOptions modelOptions;
    modelOptions.autoUpload = true;
    modelOptions.registerModel = true;
    modelOptions.registerMeshes = true;
    modelOptions.registerMaterials = true;
    modelOptions.resourcePrefix = "miku_lod_test";
    modelOptions.shaderOverride = phongShader;
    modelOptions.basePath = "models/miku";

    Logger::GetInstance().Info("[LODGeneratorTest] Loading Miku model...");
    auto loadResult = ModelLoader::LoadFromFile("models/miku/v4c5.0short.pmx", "miku_lod_test", modelOptions);
    if (!loadResult.model) {
        Logger::GetInstance().Error("[LODGeneratorTest] Failed to load miku model");
        Renderer::Destroy(renderer);
        return -1;
    }

    auto model = loadResult.model;
    Logger::GetInstance().InfoFormat(
        "[LODGeneratorTest] Model loaded, parts=%zu, meshes=%zu, materials=%zu",
        model->GetPartCount(),
        loadResult.meshResourceNames.size(),
        loadResult.materialResourceNames.size()
    );

    // 提取模型中的第一个网格用于 LOD 生成测试
    Ref<Mesh> sourceMesh = nullptr;
    model->AccessParts([&](const std::vector<ModelPart>& parts) {
        if (!parts.empty() && parts[0].mesh) {
            sourceMesh = parts[0].mesh;
        }
    });

    if (!sourceMesh) {
        Logger::GetInstance().Error("[LODGeneratorTest] Failed to extract mesh from model");
        Renderer::Destroy(renderer);
        return -1;
    }

    Logger::GetInstance().InfoFormat(
        "[LODGeneratorTest] Source mesh: %zu vertices, %zu triangles",
        sourceMesh->GetVertexCount(),
        sourceMesh->GetTriangleCount()
    );

    // 生成 Model 的 LOD 级别
    Logger::GetInstance().Info("[LODGeneratorTest] Generating LOD levels for entire model...");
    auto lodOptions = LODGenerator::GetRecommendedOptions(sourceMesh);
    
    // 为整个 Model 生成 LOD 级别
    auto lodModels = LODGenerator::GenerateModelLODLevels(model, lodOptions);
    
    if (lodModels.empty() || !lodModels[0] || !lodModels[1] || !lodModels[2] || !lodModels[3]) {
        Logger::GetInstance().Error("[LODGeneratorTest] Failed to generate model LOD levels");
        Renderer::Destroy(renderer);
        return -1;
    }
    
    // 打印统计信息
    for (int lod = 0; lod <= 3; ++lod) {
        if (lodModels[lod]) {
            auto stats = lodModels[lod]->GetStatistics();
            Logger::GetInstance().InfoFormat(
                "[LODGeneratorTest] LOD%d: %zu parts, %zu vertices, %zu triangles",
                lod, stats.meshCount, stats.vertexCount, stats.indexCount / 3
            );
        }
    }
    
    // 保存 Model 的所有 LOD 级别到文件
    Logger::GetInstance().Info("[LODGeneratorTest] Saving model LOD meshes to files...");
    std::string outputDir = "output/lod_meshes";
    std::string basePath = outputDir + "/miku";
    if (LODGenerator::SaveModelLODToFiles(model, basePath, lodOptions)) {
        Logger::GetInstance().InfoFormat("[LODGeneratorTest] Successfully saved all LOD meshes to %s", basePath.c_str());
    } else {
        Logger::GetInstance().Warning("[LODGeneratorTest] Some LOD meshes failed to save");
    }
    
    // 加载保存的 LOD 网格（测试加载功能）
    Logger::GetInstance().Info("[LODGeneratorTest] Loading saved LOD meshes...");
    std::vector<Ref<Mesh>> loadedLODMeshes(4);
    
    // 检查模型有多少部分
    size_t partCount = model->GetPartCount();
    Logger::GetInstance().InfoFormat("[LODGeneratorTest] Model has %zu parts", partCount);
    
    // 加载第一个部分的 LOD 网格（用于测试）
    if (partCount > 0) {
        std::string partName = "part0";  // 默认名称
        model->AccessParts([&](const std::vector<ModelPart>& parts) {
            if (!parts[0].name.empty()) {
                partName = parts[0].name;
                // 清理文件名中的非法字符
                std::replace(partName.begin(), partName.end(), '/', '_');
                std::replace(partName.begin(), partName.end(), '\\', '_');
                std::replace(partName.begin(), partName.end(), ':', '_');
            }
        });
        
        for (int lod = 0; lod <= 3; ++lod) {
            std::string filepath;
            if (partCount == 1) {
                filepath = basePath + "_lod" + std::to_string(lod) + ".obj";
            } else {
                filepath = basePath + "_" + partName + "_lod" + std::to_string(lod) + ".obj";
            }
            
            // 使用 MeshLoader 加载 OBJ 文件
            auto& resMgr = ResourceManager::GetInstance();
            if (resMgr.HasMesh(filepath)) {
                loadedLODMeshes[lod] = resMgr.GetMesh(filepath);
                Logger::GetInstance().InfoFormat(
                    "[LODGeneratorTest] Loaded LOD%d from cache: %s (%zu triangles)",
                    lod, filepath.c_str(), loadedLODMeshes[lod] ? loadedLODMeshes[lod]->GetTriangleCount() : 0
                );
            } else {
                // 尝试从文件加载
                auto meshes = MeshLoader::LoadFromFile(filepath);
                if (!meshes.empty() && meshes[0]) {
                    loadedLODMeshes[lod] = meshes[0];
                    resMgr.RegisterMesh(filepath, meshes[0]);
                    Logger::GetInstance().InfoFormat(
                        "[LODGeneratorTest] Loaded LOD%d from file: %s (%zu triangles)",
                        lod, filepath.c_str(), loadedLODMeshes[lod]->GetTriangleCount()
                    );
                } else {
                    Logger::GetInstance().WarningFormat(
                        "[LODGeneratorTest] Failed to load LOD%d from: %s",
                        lod, filepath.c_str()
                    );
                }
            }
        }
    }
    
    // 使用生成的 LOD 模型（而不是单个网格）
    // 提取第一个部分的网格用于渲染测试
    std::vector<Ref<Mesh>> allLODMeshes(4);
    for (int lod = 0; lod <= 3; ++lod) {
        if (lodModels[lod]) {
            lodModels[lod]->AccessParts([&](const std::vector<ModelPart>& parts) {
                if (!parts.empty() && parts[0].mesh) {
                    allLODMeshes[lod] = parts[0].mesh;
                }
            });
        }
        // 如果加载的文件存在，优先使用加载的网格（验证加载功能）
        if (loadedLODMeshes[lod]) {
            allLODMeshes[lod] = loadedLODMeshes[lod];
            Logger::GetInstance().InfoFormat(
                "[LODGeneratorTest] Using loaded LOD%d mesh (%zu triangles) for rendering",
                lod, allLODMeshes[lod]->GetTriangleCount()
            );
        }
    }

    auto world = std::make_shared<World>();
    world->Initialize();

    world->RegisterComponent<TransformComponent>();
    world->RegisterComponent<ModelComponent>();
    world->RegisterComponent<LODComponent>();
    world->RegisterComponent<CameraComponent>();
    world->RegisterComponent<NameComponent>();
    world->RegisterComponent<ActiveComponent>();

    world->RegisterSystem<TransformSystem>();
    world->RegisterSystem<CameraSystem>();
    world->RegisterSystem<UniformSystem>(renderer);
    world->RegisterSystem<ModelRenderSystem>(renderer);

    world->PostInitialize();

    // 创建相机
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

    // 注意：为了测试不同LOD级别的视觉效果，我们不注册LOD更新系统
    // 而是直接在每个实体上设置固定的LOD级别
    // 如果需要测试自动LOD切换，可以取消注释下面的代码
    /*
    // 创建 LOD 更新系统
    struct LODUpdateSystemImpl : public System {
        void Update(float deltaTime) override {
            (void)deltaTime;
            if (!m_world) return;
            
            // 获取主相机位置
            Vector3 cameraPosition = Vector3::Zero();
            auto camEntities = m_world->Query<CameraComponent, TransformComponent>();
            if (!camEntities.empty()) {
                auto& camTransform = m_world->GetComponent<TransformComponent>(camEntities[0]);
                if (camTransform.transform) {
                    cameraPosition = camTransform.transform->GetWorldPosition();
                }
            }
            
            // 获取所有有 LODComponent 的实体
            auto entities = m_world->Query<LODComponent, TransformComponent>();
            if (entities.empty()) return;
            
            // 获取当前帧 ID
            static uint64_t frameId = 0;
            frameId++;
            
            // 批量计算 LOD
            std::vector<EntityID> entityList(entities.begin(), entities.end());
            LODSelector::BatchCalculateLOD(entityList, m_world, cameraPosition, frameId);
        }
        
        int GetPriority() const override { return 95; }  // 在 MeshRenderSystem 之前运行
    };
    
    world->RegisterSystem<LODUpdateSystemImpl>();
    */

    // 创建 4 个实体，分别直接使用 LOD0, LOD1, LOD2, LOD3 的完整模型
    // 不使用自动LOD切换，每个实体固定显示对应LOD级别的模型
    std::vector<EntityID> lodEntities;

    // 确保所有LOD模型都已生成
    if (lodModels.size() < 4 || !lodModels[0] || !lodModels[1] || !lodModels[2] || !lodModels[3]) {
        Logger::GetInstance().Error("[LODGeneratorTest] Not all LOD models are available");
        Renderer::Destroy(renderer);
        return -1;
    }

    for (size_t i = 0; i <= 3; ++i) {
        EntityID entity = world->CreateEntity({ 
            .name = "Miku_LOD" + std::to_string(i), 
            .active = true 
        });

        // 设置位置（横向排列，间距更大）
        TransformComponent transform;
        transform.SetPosition(Vector3((i - 1.5f) * 8.0f, 0.0f, 0.0f));  // 间距从3.0增加到5.0
        transform.SetRotation(MathUtils::FromEulerDegrees(0.0f, 180.0f, 0.0f));
        transform.SetScale(1.0f);
        world->AddComponent(entity, transform);

        // 设置模型组件：直接使用对应LOD级别的模型
        // LOD0实体使用lodModels[0]，LOD1实体使用lodModels[1]，以此类推
        ModelComponent modelComp;
        modelComp.SetModel(lodModels[i]);  // 直接使用对应LOD级别的模型
        modelComp.visible = true;
        modelComp.castShadows = true;
        modelComp.receiveShadows = true;
        modelComp.layerID = 0;
        modelComp.renderPriority = 0;
        world->AddComponent(entity, modelComp);

        // 不添加LODComponent，禁用自动LOD切换
        // 每个实体固定显示对应LOD级别的模型，不会根据距离自动切换

        lodEntities.push_back(entity);
    }

    Logger::GetInstance().Info("[LODGeneratorTest] Created 4 entities (LOD0, LOD1, LOD2, LOD3)");
    Logger::GetInstance().Info("[LODGeneratorTest] Controls: ESC to exit");
    Logger::GetInstance().Info("[LODGeneratorTest] Controls: WASD 前后左右, Q/E 上下, Shift 加速, 鼠标视角, Tab 捕获/释放鼠标");
    Logger::GetInstance().Info("[LODGeneratorTest] Controls: 按 G 键切换网格渲染模式（线框/填充）");

    bool running = true;
    Uint64 prevTicks = SDL_GetTicks();
    float accumTime = 0.0f;
    Vector3 cameraPosition = sceneConfig.cameraPosition;
    Vector3 toTarget = (sceneConfig.cameraTarget - sceneConfig.cameraPosition).normalized();
    float cameraYaw = MathUtils::RadiansToDegrees(std::atan2(toTarget.x(), -toTarget.z()));
    float cameraPitch = MathUtils::RadiansToDegrees(std::asin(std::clamp(toTarget.y(), -1.0f, 1.0f)));
    bool mouseCaptured = true;
    bool wireframeMode = true;  // 默认启用线框模式

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
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_G) {
                wireframeMode = !wireframeMode;
                Logger::GetInstance().InfoFormat(
                    "[LODGeneratorTest] 网格渲染模式: %s",
                    wireframeMode ? "线框模式" : "填充模式"
                );
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

        if (keyboard[SDL_SCANCODE_W]) cameraPosition += front * moveSpeed;
        if (keyboard[SDL_SCANCODE_S]) cameraPosition -= front * moveSpeed;
        if (keyboard[SDL_SCANCODE_A]) cameraPosition -= right * moveSpeed;
        if (keyboard[SDL_SCANCODE_D]) cameraPosition += right * moveSpeed;
        if (keyboard[SDL_SCANCODE_Q]) cameraPosition -= Vector3::UnitY() * moveSpeed;
        if (keyboard[SDL_SCANCODE_E]) cameraPosition += Vector3::UnitY() * moveSpeed;

        auto& cameraTransformComp = world->GetComponent<TransformComponent>(cameraEntity);
        cameraTransformComp.SetPosition(cameraPosition);
        cameraTransformComp.SetRotation(viewRotation);

        // 让所有模型旋转
        for (size_t i = 0; i < lodEntities.size(); ++i) {
            auto& transform = world->GetComponent<TransformComponent>(lodEntities[i]);
            Quaternion baseRotation = MathUtils::FromEulerDegrees(0.0f, 180.0f, 0.0f);
            Quaternion spin = MathUtils::FromEulerDegrees(0.0f, std::sin(accumTime * 0.6f) * 15.0f, 0.0f);
            transform.SetRotation(baseRotation * spin);
        }

        renderer->BeginFrame();
        renderer->Clear();

        // 根据按键切换网格渲染模式（线框/填充）
        if (wireframeMode) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glLineWidth(1.0f);  // 设置线宽
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

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

    Logger::GetInstance().Info("[LODGeneratorTest] Shutdown complete");
    return 0;
}

