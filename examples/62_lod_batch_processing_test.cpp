/**
 * @file 62_lod_batch_processing_test.cpp
 * @brief LOD实例化渲染分批处理测试 - 测试分批处理机制解决卡死问题
 * 
 * 测试内容：
 * 1. 异步加载Miku模型（参考35测试）
 * 2. 生成LOD级别（参考58测试）
 * 3. 创建大量模型实例（100个Miku模型，每个25个Part = 2500个实例）
 * 4. 测试LOD实例化渲染的分批处理功能
 * 5. 对比启用/禁用分批处理的性能差异
 * 6. 验证分批处理是否解决了卡死崩溃问题
 * 
 * 控制说明：
 * - ESC: 退出
 * - WASD: 移动相机
 * - Q/E: 上升/下降
 * - 鼠标移动: 旋转视角
 * - I: 切换LOD实例化渲染
 * - B: 调整分批处理大小（10/50/100/200）
 * - F: 显示/隐藏统计信息
 */

#include <render/renderer.h>
#include <render/logger.h>
#include <render/shader_cache.h>
#include <render/model_loader.h>
#include <render/resource_manager.h>
#include <render/ecs/world.h>
#include <render/ecs/components.h>
#include <render/ecs/systems.h>
#include <render/lod_generator.h>
#include <render/async_resource_loader.h>
#include <render/math_utils.h>

#include <SDL3/SDL.h>
#include <glad/glad.h>

#include <algorithm>
#include <memory>
#include <vector>
#include <random>
#include <fstream>

using namespace Render;
using namespace Render::ECS;

namespace {

struct SceneConfig {
    Vector3 cameraPosition{0.0f, 10.0f, 30.0f};
    Vector3 cameraTarget{0.0f, 8.0f, 0.0f};
    Vector3 lightPosition{10.0f, 15.0f, 10.0f};
    Color ambientColor{0.2f, 0.2f, 0.25f, 1.0f};
    Color diffuseColor{1.0f, 1.0f, 1.0f, 1.0f};
    Color specularColor{0.6f, 0.6f, 0.6f, 1.0f};
    float shininess = 48.0f;
    
    // 测试配置
    int modelCount = 100;           // 模型数量（每个模型25个Part = 2500个实例）
    float gridSize = 50.0f;         // 网格大小
    bool enableInstancing = true;   // 是否启用LOD实例化渲染
    size_t batchSize = 100;         // 分批处理大小（每帧处理的实例数）
    bool showStats = true;          // 是否显示统计信息
};

// 渐进式加载状态
struct ProgressiveLoadState {
    std::string modelPath;
    std::string texturePath;
    Ref<Shader> shader;
    ModelPtr model;
    std::shared_ptr<ModelLoadTask> task;
    std::vector<std::string> meshNames;
    std::vector<std::string> materialNames;
    bool loadStarted = false;
    bool resourcesReady = false;
    bool loadComplete = false;
    bool loadFailed = false;
    std::string errorMessage;
    size_t partsLoaded = 0;
    
    // LOD相关
    std::vector<ModelPtr> lodModels;  // LOD0, LOD1, LOD2, LOD3
    bool lodGenerated = false;
};

} // namespace

int main(int argc, char* argv[]) {
    Logger::GetInstance().SetLogToFile(true);
    
    (void)argc;
    (void)argv;

    Logger::GetInstance().Info("[LODBatchProcessingTest] === LOD Batch Processing Test ===");
    Logger::GetInstance().Info("[LODBatchProcessingTest] Testing batch processing to solve freeze/crash issues");

    // 解析命令行参数
    SceneConfig sceneConfig{};
    if (argc > 1) {
        sceneConfig.modelCount = std::atoi(argv[1]);
        if (sceneConfig.modelCount <= 0) {
            sceneConfig.modelCount = 100;
        }
    }
    if (argc > 2) {
        sceneConfig.batchSize = static_cast<size_t>(std::atoi(argv[2]));
        if (sceneConfig.batchSize <= 0) {
            sceneConfig.batchSize = 100;
        }
    }

    Logger::GetInstance().InfoFormat(
        "[LODBatchProcessingTest] Configuration: models=%d, batchSize=%zu, instancing=%s",
        sceneConfig.modelCount,
        sceneConfig.batchSize,
        sceneConfig.enableInstancing ? "enabled" : "disabled"
    );

    // ============================================================
    // 1. 初始化Renderer
    // ============================================================
    Renderer* renderer = Renderer::Create();
    if (!renderer->Initialize("LOD Batch Processing Test", 1600, 900)) {
        Logger::GetInstance().Error("[LODBatchProcessingTest] Failed to initialize renderer");
        return -1;
    }
    renderer->SetClearColor(Color(0.05f, 0.05f, 0.1f, 1.0f));
    renderer->SetVSync(true);
    
    // 启用LOD实例化渲染
    renderer->SetLODInstancingEnabled(sceneConfig.enableInstancing);
    Logger::GetInstance().InfoFormat(
        "[LODBatchProcessingTest] LOD Instancing: %s",
        renderer->IsLODInstancingEnabled() ? "enabled" : "disabled"
    );

    if (auto context = renderer->GetContext()) {
        SDL_SetWindowRelativeMouseMode(context->GetWindow(), true);
    }

    // ============================================================
    // 2. 加载Shader
    // ============================================================
    auto& shaderCache = ShaderCache::GetInstance();
    auto phongShader = shaderCache.LoadShader(
        "material_phong",
        "shaders/material_phong.vert",
        "shaders/material_phong.frag"
    );
    if (!phongShader || !phongShader->IsValid()) {
        Logger::GetInstance().Error("[LODBatchProcessingTest] Failed to load Phong shader");
        Renderer::Destroy(renderer);
        return -1;
    }

    // ============================================================
    // 3. 查找模型文件
    // ============================================================
    std::vector<std::string> modelPaths = {
        "models/miku/v4c5.0short.pmx",
        "models/miku/miku.pmx",
        "models/miku/v4c5.0.pmx"
    };
    
    std::string selectedModelPath;
    std::string textureBasePath;
    
    for (const auto& path : modelPaths) {
        std::ifstream file(path);
        if (file.good()) {
            selectedModelPath = path;
            textureBasePath = path.substr(0, path.find_last_of("/\\") + 1);
            Logger::GetInstance().InfoFormat("找到模型文件: %s", path.c_str());
            Logger::GetInstance().InfoFormat("纹理基础路径: %s", textureBasePath.c_str());
            file.close();
            break;
        }
    }
    
    if (selectedModelPath.empty()) {
        Logger::GetInstance().Error("[LODBatchProcessingTest] 未找到miku模型文件");
        Renderer::Destroy(renderer);
        return -1;
    }

    // ============================================================
    // 4. 创建异步加载器
    // ============================================================
    auto& asyncLoader = AsyncResourceLoader::GetInstance();
    
    // 存储加载状态
    auto loadState = std::make_shared<ProgressiveLoadState>();
    loadState->modelPath = selectedModelPath;
    loadState->texturePath = textureBasePath;
    loadState->shader = phongShader;

    // ============================================================
    // 5. 创建ECS World
    // ============================================================
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

    // 获取ModelRenderSystem并配置分批处理
    auto* modelRenderSystem = world->GetSystem<ModelRenderSystem>();
    if (modelRenderSystem) {
        modelRenderSystem->SetLODInstancingBatchSize(sceneConfig.batchSize);
        Logger::GetInstance().InfoFormat(
            "[LODBatchProcessingTest] Batch size set to: %zu instances per frame",
            modelRenderSystem->GetLODInstancingBatchSize()
        );
    }

    // ============================================================
    // 6. 创建相机
    // ============================================================
    EntityID cameraEntity = world->CreateEntity({ .name = "MainCamera", .active = true });
    TransformComponent cameraTransform;
    cameraTransform.SetPosition(sceneConfig.cameraPosition);
    Vector3 toTargetInit = (sceneConfig.cameraTarget - sceneConfig.cameraPosition).normalized();
    float cameraYawInit = MathUtils::RadiansToDegrees(std::atan2(toTargetInit.x(), -toTargetInit.z()));
    float cameraPitchInit = MathUtils::RadiansToDegrees(std::asin(std::clamp(toTargetInit.y(), -1.0f, 1.0f)));
    Quaternion yawRotation = MathUtils::AngleAxis(MathUtils::DegreesToRadians(cameraYawInit), Vector3::UnitY());
    Quaternion pitchRotation = MathUtils::AngleAxis(MathUtils::DegreesToRadians(cameraPitchInit), Vector3::UnitX());
    cameraTransform.SetRotation(yawRotation * pitchRotation);
    world->AddComponent(cameraEntity, cameraTransform);

    CameraComponent cameraComp;
    cameraComp.camera = std::make_shared<Camera>();
    cameraComp.camera->SetPerspective(60.0f, static_cast<float>(renderer->GetWidth()) / renderer->GetHeight(), 0.1f, 1000.0f);
    cameraComp.active = true;
    world->AddComponent(cameraEntity, cameraComp);

    // ============================================================
    // 7. 创建光源
    // ============================================================
    EntityID light = world->CreateEntity({.name = "DirectionalLight"});
    world->AddComponent<TransformComponent>(light, TransformComponent());
    auto& lightTransform = world->GetComponent<TransformComponent>(light);
    lightTransform.SetPosition(sceneConfig.lightPosition);
    lightTransform.SetRotation(MathUtils::FromEulerDegrees(45, 30, 0));

    LightComponent lightComp;
    lightComp.type = LightType::Directional;
    lightComp.color = Color(1.0f, 1.0f, 0.95f);
    lightComp.intensity = 1.2f;
    lightComp.enabled = true;
    world->AddComponent(light, lightComp);

    // ============================================================
    // 8. 主循环
    // ============================================================
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("进入主循环...");
    Logger::GetInstance().Info("控制说明:");
    Logger::GetInstance().Info("  W/S          - 前进/后退");
    Logger::GetInstance().Info("  A/D          - 左移/右移");
    Logger::GetInstance().Info("  Q/E          - 下降/上升");
    Logger::GetInstance().Info("  鼠标移动     - 旋转视角");
    Logger::GetInstance().Info("  I            - 切换LOD实例化渲染");
    Logger::GetInstance().Info("  B            - 调整分批处理大小 (10/50/100/200)");
    Logger::GetInstance().Info("  F            - 显示/隐藏统计信息");
    Logger::GetInstance().Info("  ESC          - 退出程序");
    Logger::GetInstance().Info("========================================");

    bool running = true;
    uint64_t lastTime = SDL_GetPerformanceCounter();
    uint64_t frequency = SDL_GetPerformanceFrequency();

    float fpsTimer = 0.0f;
    int frameCount = 0;
    float lastFPS = 0.0f;

    // 实体创建状态
    bool entitiesCreated = false;
    std::vector<EntityID> modelEntities;
    modelEntities.reserve(sceneConfig.modelCount);

    // 相机控制
    Vector3 cameraPosition = sceneConfig.cameraPosition;
    Vector3 toTarget = (sceneConfig.cameraTarget - sceneConfig.cameraPosition).normalized();
    float cameraYaw = MathUtils::RadiansToDegrees(std::atan2(toTarget.x(), -toTarget.z()));
    float cameraPitch = MathUtils::RadiansToDegrees(std::asin(std::clamp(toTarget.y(), -1.0f, 1.0f)));
    bool mouseCaptured = true;

    // 分批处理大小选项
    std::vector<size_t> batchSizeOptions = {10, 50, 100, 200};
    size_t currentBatchSizeIndex = 2;  // 默认100

    while (running) {
        // ==================== 处理异步任务 ====================
        size_t processedTasks = asyncLoader.ProcessCompletedTasks(20);
        if (processedTasks > 0) {
            Logger::GetInstance().DebugFormat("本帧处理异步任务: %zu", processedTasks);
        }

        // ==================== 异步加载模型 ====================
        if (!loadState->loadStarted) {
            Logger::GetInstance().Info("========================================");
            Logger::GetInstance().Info("开始异步加载模型...");
            Logger::GetInstance().Info("========================================");
            loadState->loadStarted = true;
            
            ModelLoadOptions options;
            options.flipUVs = true;
            options.autoUpload = true;
            options.basePath = loadState->texturePath;
            options.resourcePrefix = "batch_test_miku";
            options.shaderOverride = loadState->shader;
            options.registerModel = true;
            options.registerMeshes = true;
            options.registerMaterials = true;
            options.updateDependencyGraph = true;
            
            auto weakState = std::weak_ptr<ProgressiveLoadState>(loadState);
            loadState->task = asyncLoader.LoadModelAsync(
                loadState->modelPath,
                "batch_test_miku_model",
                options,
                [weakState](const ModelLoadResult& result) {
                    if (auto state = weakState.lock()) {
                        if (result.IsSuccess()) {
                            state->model = result.resource;
                            state->meshNames = result.meshResourceNames;
                            state->materialNames = result.materialResourceNames;
                            
                            if (state->meshNames.empty() && state->model) {
                                Logger::GetInstance().Warning("异步加载未返回资源名称，执行手动注册");
                                auto& resMgr = ResourceManager::GetInstance();
                                size_t index = 0;
                                state->model->AccessParts([&](const std::vector<ModelPart>& parts) {
                                    state->meshNames.reserve(parts.size());
                                    state->materialNames.reserve(parts.size());
                                    for (const auto& part : parts) {
                                        std::string meshName = "batch_test_miku_mesh_" + std::to_string(index);
                                        std::string materialName = "batch_test_miku_material_" + std::to_string(index);
                                        
                                        if (part.mesh) {
                                            if (!resMgr.HasMesh(meshName)) {
                                                resMgr.RegisterMesh(meshName, part.mesh);
                                            }
                                            state->meshNames.push_back(meshName);
                                        } else {
                                            state->meshNames.push_back("");
                                        }
                                        
                                        if (part.material) {
                                            if (!resMgr.HasMaterial(materialName)) {
                                                resMgr.RegisterMaterial(materialName, part.material);
                                            }
                                            state->materialNames.push_back(materialName);
                                        } else {
                                            state->materialNames.push_back("");
                                        }
                                        ++index;
                                    }
                                });
                            }
                            
                            state->resourcesReady = true;
                            
                            Logger::GetInstance().InfoFormat("✓ 模型异步加载完成，共 %zu 个部件", 
                                state->meshNames.size());
                            
                            // 注意：LOD生成需要在主线程执行（需要OpenGL上下文）
                            // 将在主循环中处理
                        } else {
                            state->loadFailed = true;
                            state->errorMessage = result.errorMessage;
                            Logger::GetInstance().ErrorFormat("异步模型加载失败: %s", 
                                result.errorMessage.c_str());
                        }
                    }
                },
                50.0f);
            
            if (!loadState->task) {
                loadState->loadFailed = true;
                loadState->errorMessage = "无法提交异步模型加载任务";
                Logger::GetInstance().Error("无法提交异步模型加载任务");
            } else {
                Logger::GetInstance().InfoFormat("已提交异步模型加载任务: %s", 
                    loadState->modelPath.c_str());
            }
        }
        
        if (loadState->loadFailed) {
            Logger::GetInstance().ErrorFormat("模型加载失败，终止测试: %s",
                loadState->errorMessage.empty() ? "未知错误" : loadState->errorMessage.c_str());
            running = false;
        }

        // ==================== 生成LOD级别（在主线程中执行，需要OpenGL上下文） ====================
        if (loadState->resourcesReady && !loadState->lodGenerated && loadState->model) {
            Logger::GetInstance().Info("[LODBatchProcessingTest] 开始生成LOD级别...");
            
            // 从模型中提取第一个网格用于生成LOD选项
            Ref<Mesh> sourceMesh = nullptr;
            loadState->model->AccessParts([&](const std::vector<ModelPart>& parts) {
                if (!parts.empty() && parts[0].mesh) {
                    sourceMesh = parts[0].mesh;
                }
            });
            
            if (!sourceMesh) {
                Logger::GetInstance().Warning("[LODBatchProcessingTest] 无法从模型中提取网格，使用默认LOD选项");
                loadState->lodModels.resize(4);
                loadState->lodModels[0] = loadState->model;
                loadState->lodModels[1] = loadState->model;
                loadState->lodModels[2] = loadState->model;
                loadState->lodModels[3] = loadState->model;
                loadState->lodGenerated = true;
            } else {
                auto lodOptions = LODGenerator::GetRecommendedOptions(sourceMesh);
                loadState->lodModels = LODGenerator::GenerateModelLODLevels(loadState->model, lodOptions);
                
                if (!loadState->lodModels.empty() && loadState->lodModels[0] && 
                    loadState->lodModels[1] && loadState->lodModels[2] && loadState->lodModels[3]) {
                    loadState->lodGenerated = true;
                    Logger::GetInstance().Info("[LODBatchProcessingTest] ✓ LOD级别生成完成");
                    
                    // 打印LOD统计信息
                    for (int lod = 0; lod <= 3; ++lod) {
                        if (loadState->lodModels[lod]) {
                            auto stats = loadState->lodModels[lod]->GetStatistics();
                            Logger::GetInstance().InfoFormat(
                                "[LODBatchProcessingTest] LOD%d: %zu parts, %zu vertices, %zu triangles",
                                lod, stats.meshCount, stats.vertexCount, stats.indexCount / 3
                            );
                        }
                    }
                } else {
                    Logger::GetInstance().Warning("[LODBatchProcessingTest] LOD级别生成失败，使用原始模型");
                    loadState->lodModels.resize(4);
                    loadState->lodModels[0] = loadState->model;
                    loadState->lodModels[1] = loadState->model;
                    loadState->lodModels[2] = loadState->model;
                    loadState->lodModels[3] = loadState->model;
                    loadState->lodGenerated = true;
                }
            }
        }

        // ==================== 创建实体（模型加载完成且LOD生成完成后） ====================
        if (!entitiesCreated && loadState->resourcesReady && loadState->lodGenerated && loadState->model) {
            Logger::GetInstance().Info("========================================");
            Logger::GetInstance().InfoFormat("开始创建 %d 个模型实例...", sceneConfig.modelCount);
            Logger::GetInstance().Info("========================================");
            
            int gridWidth = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(sceneConfig.modelCount))));
            float spacing = sceneConfig.gridSize / static_cast<float>(gridWidth);
            float startX = -sceneConfig.gridSize * 0.5f;
            float startZ = -sceneConfig.gridSize * 0.5f;
            
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> posDist(-2.0f, 2.0f);
            std::uniform_real_distribution<float> rotDist(0.0f, 360.0f);
            std::uniform_real_distribution<float> scaleDist(0.8f, 1.2f);
            
            for (int i = 0; i < sceneConfig.modelCount; ++i) {
                EntityID entity = world->CreateEntity({ 
                    .name = "MikuModel_" + std::to_string(i), 
                    .active = true 
                });

                // 计算网格位置
                int row = i / gridWidth;
                int col = i % gridWidth;
                float x = startX + static_cast<float>(col) * spacing + posDist(gen);
                float z = startZ + static_cast<float>(row) * spacing + posDist(gen);
                float y = 0.0f;

                // 设置变换
                TransformComponent transform;
                transform.SetPosition(Vector3(x, y, z));
                transform.SetRotation(MathUtils::FromEulerDegrees(0.0f, rotDist(gen), 0.0f));
                transform.SetScale(scaleDist(gen));
                world->AddComponent(entity, transform);

                // 设置模型组件
                ModelComponent modelComp;
                modelComp.model = loadState->lodModels[0];  // 使用LOD0模型
                modelComp.visible = true;
                modelComp.layerID = 0;
                modelComp.castShadows = true;
                modelComp.receiveShadows = true;
                modelComp.resourcesLoaded = true;
                world->AddComponent(entity, modelComp);

                // 配置LOD组件
                LODComponent lodComp;
                lodComp.config.enabled = true;
                lodComp.config.distanceThresholds = {30.0f, 60.0f, 100.0f, 150.0f};
                lodComp.config.transitionDistance = 5.0f;
                
                // 配置LOD模型列表
                lodComp.config.lodModels.resize(4);
                lodComp.config.lodModels[0] = loadState->lodModels[0];  // LOD0
                lodComp.config.lodModels[1] = loadState->lodModels[1];  // LOD1
                lodComp.config.lodModels[2] = loadState->lodModels[2];  // LOD2
                lodComp.config.lodModels[3] = loadState->lodModels[3];  // LOD3
                
                world->AddComponent(entity, lodComp);

                modelEntities.push_back(entity);
            }
            
            entitiesCreated = true;
            Logger::GetInstance().InfoFormat(
                "[LODBatchProcessingTest] ✓ 所有 %zu 个模型实例创建完成",
                modelEntities.size()
            );
            
            // 计算总实例数（每个模型有多个Part）
            size_t totalParts = 0;
            if (loadState->model) {
                totalParts = loadState->model->GetPartCount() * modelEntities.size();
            }
            Logger::GetInstance().InfoFormat(
                "[LODBatchProcessingTest] 总实例数: %zu (每个模型 %zu 个Part × %zu 个模型)",
                totalParts,
                loadState->model ? loadState->model->GetPartCount() : 0,
                modelEntities.size()
            );
        }

        // ==================== 处理事件 ====================
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
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_I) {
                sceneConfig.enableInstancing = !sceneConfig.enableInstancing;
                renderer->SetLODInstancingEnabled(sceneConfig.enableInstancing);
                Logger::GetInstance().InfoFormat(
                    "[LODBatchProcessingTest] LOD Instancing: %s",
                    sceneConfig.enableInstancing ? "enabled" : "disabled"
                );
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_B) {
                // 循环切换分批处理大小
                currentBatchSizeIndex = (currentBatchSizeIndex + 1) % batchSizeOptions.size();
                sceneConfig.batchSize = batchSizeOptions[currentBatchSizeIndex];
                if (modelRenderSystem) {
                    modelRenderSystem->SetLODInstancingBatchSize(sceneConfig.batchSize);
                    sceneConfig.batchSize = modelRenderSystem->GetLODInstancingBatchSize();  // 同步实际值
                    Logger::GetInstance().InfoFormat(
                        "[LODBatchProcessingTest] Batch size changed to: %zu instances per frame",
                        sceneConfig.batchSize
                    );
                }
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F) {
                sceneConfig.showStats = !sceneConfig.showStats;
                Logger::GetInstance().InfoFormat(
                    "[LODBatchProcessingTest] Stats display: %s",
                    sceneConfig.showStats ? "enabled" : "disabled"
                );
            }
            if (mouseCaptured && event.type == SDL_EVENT_MOUSE_MOTION) {
                constexpr float sensitivity = 0.15f;
                cameraYaw -= static_cast<float>(event.motion.xrel) * sensitivity;
                cameraPitch -= static_cast<float>(event.motion.yrel) * sensitivity;
                cameraPitch = std::clamp(cameraPitch, -89.0f, 89.0f);
            }
        }

        // ==================== 更新相机 ====================
        uint64_t currentTime = SDL_GetPerformanceCounter();
        float deltaTime = static_cast<float>(currentTime - lastTime) / static_cast<float>(frequency);
        lastTime = currentTime;
        deltaTime = std::min(deltaTime, 0.033f);

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

        // ==================== 渲染 ====================
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

        // ==================== 统计信息 ====================
        frameCount++;
        fpsTimer += deltaTime;
        if (fpsTimer >= 1.0f) {
            lastFPS = static_cast<float>(frameCount) / fpsTimer;
            frameCount = 0;
            fpsTimer = 0.0f;
        }

        if (sceneConfig.showStats && modelRenderSystem) {
            const auto& stats = modelRenderSystem->GetStats();
            auto lodInstancingStats = renderer->GetLODInstancingStats();
            size_t pendingInstances = modelRenderSystem->GetPendingInstanceCount();
            size_t currentBatchSize = modelRenderSystem->GetLODInstancingBatchSize();
            
            // 每60帧输出一次统计信息
            if (frameCount % 60 == 0) {
                Logger::GetInstance().InfoFormat(
                    "[LODBatchProcessingTest] Frame %d | FPS: %.1f | Frame: %.3fms | "
                    "Visible: %zu | Culled: %zu | Parts: %zu | Renderables: %zu | "
                    "LOD: enabled=%zu, LOD0=%zu, LOD1=%zu, LOD2=%zu, LOD3=%zu | "
                    "Instancing: %s | Batch Size: %zu | Pending: %zu | "
                    "LOD Stats: groups=%zu, instances=%zu, drawCalls=%zu",
                    frameCount,
                    lastFPS,
                    deltaTime * 1000.0f,
                    stats.visibleModels,
                    stats.culledModels,
                    stats.submittedParts,
                    stats.submittedRenderables,
                    stats.lodEnabledEntities,
                    stats.lod0Count,
                    stats.lod1Count,
                    stats.lod2Count,
                    stats.lod3Count,
                    sceneConfig.enableInstancing ? "ON" : "OFF",
                    currentBatchSize,
                    pendingInstances,
                    lodInstancingStats.lodGroupCount,
                    lodInstancingStats.totalInstances,
                    lodInstancingStats.drawCalls
                );
            }
        }

        renderer->EndFrame();
        renderer->Present();

        SDL_Delay(1);
    }

    world->Shutdown();
    Renderer::Destroy(renderer);

    Logger::GetInstance().Info("[LODBatchProcessingTest] Shutdown complete");
    return 0;
}

