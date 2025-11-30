/**
 * @file 35_ecs_comprehensive_test.cpp
 * @brief ECS 系统综合功能测试（已更新安全性改进）
 * 
 * 测试内容：
 * - 所有新增 ECS 系统（WindowSystem、GeometrySystem、UniformSystem、ResourceCleanupSystem）
 * - 材质属性覆盖（diffuse color、metallic、roughness、opacity）
 * - 多纹理支持（textureOverrides）
 * - 视锥体裁剪统计
 * - 透明物体排序
 * - 几何形状生成（Cube、Sphere、Cylinder 等）
 * - 渲染 20 个几何形状测试样例（不同材质覆盖）
 * 
 * 安全性更新（2025-11-06）：
 * - ✅ 增强实体有效性检查（IsValidEntity）
 * - ✅ 组件访问前增加 HasComponent 验证
 * - ✅ 完善异常处理（区分 std::out_of_range 和其他异常）
 * - ✅ 系统获取增加空指针检查和异常保护
 * - ✅ 遵循 ECS 安全性最佳实践（参见 docs/ECS_SAFETY_IMPROVEMENTS.md）
 */

#include <SDL3/SDL.h>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <fstream>
#include <cstdlib>
#include <ctime>

#include "render/renderer.h"
#include "render/shader_cache.h"
#include "render/material.h"
#include "render/logger.h"
#include "render/resource_manager.h"
#include "render/async_resource_loader.h"
#include "render/ecs/world.h"
#include "render/ecs/systems.h"
#include "render/ecs/components.h"
#include "render/math_utils.h"
#include "render/lod_system.h"  // LOD 系统支持

using namespace Render;
using namespace Render::ECS;

// ============================================================
// LOD 更新系统（在渲染前更新 LOD 级别）
// ============================================================
class LODUpdateSystem : public System {
public:
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
        
        // 获取当前帧 ID（使用简单的计数器）
        static uint64_t frameId = 0;
        frameId++;
        
        // 批量计算 LOD
        LODSelector::BatchCalculateLOD(entities, m_world, cameraPosition, frameId);
    }
    
    [[nodiscard]] int GetPriority() const override { return 95; }  // 在 MeshRenderSystem 之前运行
};

// ============================================================
// 第一人称相机控制系统（直接更新Transform组件）
// ============================================================
class FirstPersonCameraSystem : public System {
private:
    // 输入状态
    bool m_moveForward = false;
    bool m_moveBackward = false;
    bool m_moveLeft = false;
    bool m_moveRight = false;
    bool m_moveUp = false;
    bool m_moveDown = false;
    
    // 旋转角度
    float m_yaw = 0.0f;    // 偏航角（度）
    float m_pitch = 0.0f;  // 俯仰角（度）
    
    float m_moveSpeed = 10.0f;
    float m_mouseSensitivity = 0.15f;
    
public:
    void Update(float deltaTime) override {
        if (!m_world) return;
        
        auto entities = m_world->Query<CameraComponent, TransformComponent>();
        if (entities.empty()) return;
        
        if (!m_world->IsValidEntity(entities[0])) return;
        if (!m_world->HasComponent<TransformComponent>(entities[0])) return;
        
        try {
            auto& transform = m_world->GetComponent<TransformComponent>(entities[0]);
            
            // 计算旋转
            Quaternion rotation = MathUtils::FromEulerDegrees(m_pitch, m_yaw, 0);
            
            // 计算移动方向（基于当前朝向）
            Vector3 forward = rotation * Vector3(0, 0, -1);  // 前方向
            Vector3 right = rotation * Vector3(1, 0, 0);     // 右方向
            Vector3 up = Vector3(0, -1, 0);                   // 世界上方向
            
            Vector3 velocity(0, 0, 0);
            
            // WASD移动（参考20测试：W前进，S后退）
            if (m_moveForward) velocity += forward;
            if (m_moveBackward) velocity -= forward;
            if (m_moveLeft) velocity -= right;
            if (m_moveRight) velocity += right;
            
            // QE上下移动
            if (m_moveUp) velocity += up;
            if (m_moveDown) velocity -= up;
            
            // 归一化速度并应用
            if (velocity.norm() > 0.001f) {
                velocity.normalize();
                Vector3 newPos = transform.GetPosition() + velocity * m_moveSpeed * deltaTime;
                transform.SetPosition(newPos);
            }
            
            // 更新旋转
            transform.SetRotation(rotation);
            
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat("[FirstPersonCameraSystem] Error: %s", e.what());
        }
    }
    
    // 鼠标控制
    void OnMouseMove(float deltaX, float deltaY) {
        m_yaw -= deltaX * m_mouseSensitivity;
        m_pitch -= deltaY * m_mouseSensitivity;
        
        // 限制俯仰角
        m_pitch = std::max(-89.0f, std::min(89.0f, m_pitch));
    }
    
    // 键盘控制
    void SetMoveForward(bool active) { m_moveForward = active; }
    void SetMoveBackward(bool active) { m_moveBackward = active; }
    void SetMoveLeft(bool active) { m_moveLeft = active; }
    void SetMoveRight(bool active) { m_moveRight = active; }
    void SetMoveUp(bool active) { m_moveUp = active; }
    void SetMoveDown(bool active) { m_moveDown = active; }
    
    int GetPriority() const override { return 2; }  // 在CameraSystem(5)之前执行
};

// ============================================================
// 动态测试系统（动态改变材质属性）- 已禁用以避免并发问题
// ============================================================
class DynamicMaterialTestSystem : public System {
public:
    void Update(float deltaTime) override {
        // 暂时禁用动态材质变化，避免并发访问问题
        // 材质属性覆盖在创建时已经设置好了
        return;
        
        /*
        if (!m_world) return;
        
        static float time = 0.0f;
        static float lastChangeTime = -999.0f;  // 上次改变时间
        time += deltaTime;
        
        // 每 3 秒改变一次测试样例的材质属性
        if (time - lastChangeTime >= 3.0f) {
            lastChangeTime = time;
            
            auto entities = m_world->Query<MeshRenderComponent>();
            
            for (size_t i = 0; i < entities.size() && i < 20; ++i) {
                auto& mesh = m_world->GetComponent<MeshRenderComponent>(entities[i]);
                
                // 根据索引设置不同的材质属性
                float hue = (i * 18.0f) / 360.0f;  // 每个样例相差 18 度色相
                float r = std::abs(std::sin(hue * 6.28f + time));
                float g = std::abs(std::sin((hue + 0.33f) * 6.28f + time));
                float b = std::abs(std::sin((hue + 0.67f) * 6.28f + time));
                
                mesh.SetDiffuseColor(Color(r, g, b, 1.0f));
            }
        }
        */
    }
    
    int GetPriority() const override { return 25; }  // 在 ResourceLoadingSystem 之后
};

// ============================================================
// 主函数
// ============================================================
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    // 初始化随机数种子
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    
    Logger::GetInstance().Info("=== ECS Miku渲染压力测试 ===");
    Logger::GetInstance().Info("测试内容：");
    Logger::GetInstance().Info("  - 使用ECS系统渲染100个Miku模型（2500个网格）");
    Logger::GetInstance().Info("  - 大规模场景压力测试");
    Logger::GetInstance().Info("  - 轨道相机控制，围绕场景中心旋转");
    Logger::GetInstance().Info("  - 渐进式加载，分帧创建实体");
    
    // ============================================================
    // 1. 初始化渲染器
    // ============================================================
    auto renderer = std::make_unique<Renderer>();
    if (!renderer->Initialize("ECS Miku压力测试 - 100个实例", 1920, 1080)) {
        Logger::GetInstance().Error("Failed to initialize renderer");
        return -1;
    }
    Logger::GetInstance().Info("✓ 渲染器初始化成功");
    
    // ==================== 禁用 LOD 实例化渲染（35测试不需要）====================
    // 注意：Renderer默认启用LOD实例化，但对于35测试，我们需要禁用它以使用传统渲染路径
    renderer->SetLODInstancingEnabled(false);
    Logger::GetInstance().Info("✓ LOD实例化渲染已禁用（使用传统渲染路径）");
    
    auto renderState = renderer->GetRenderState();
    renderState->SetDepthTest(true);
    renderState->SetCullFace(CullFace::Back);
    renderState->SetBlendMode(BlendMode::Alpha);  // 支持透明
    renderState->SetClearColor(Color(0.1f, 0.1f, 0.15f, 1.0f));
    
    // ============================================================
    // 2. 初始化资源管理器和异步加载器
    // ============================================================
    auto& resourceManager = ResourceManager::GetInstance();
    auto& shaderCache = ShaderCache::GetInstance();
    auto& asyncLoader = AsyncResourceLoader::GetInstance();
    asyncLoader.Initialize(4);  // 4 个工作线程
    Logger::GetInstance().Info("✓ 资源管理器初始化成功（4 个工作线程）");
    
    // ============================================================
    // 2.5 创建着色器和加载miku模型（参考20测试）
    // ============================================================
    Logger::GetInstance().Info("加载着色器...");
    
    // 加载Phong着色器（用于miku模型）
    auto phongShader = shaderCache.LoadShader("material_phong", 
        "shaders/material_phong.vert", 
        "shaders/material_phong.frag");
    
    if (!phongShader) {
        Logger::GetInstance().Warning("无法加载Phong着色器，尝试基础着色器...");
        phongShader = shaderCache.LoadShader("basic", 
            "shaders/basic.vert", 
            "shaders/basic.frag");
    }
    
    if (!phongShader) {
        Logger::GetInstance().Error("无法加载任何着色器，程序无法继续");
        return -1;
    }
    
    resourceManager.RegisterShader("phong", phongShader);
    Logger::GetInstance().Info("✓ 着色器加载完成");
    
    // 准备异步加载miku模型
    Logger::GetInstance().Info("准备异步加载miku模型...");
    std::vector<std::string> modelPaths = {
        "models/miku/v4c5.0short.pmx",
        "models/miku/v4c5.0.pmx",
        "../models/miku/v4c5.0short.pmx",
        "../models/miku/v4c5.0.pmx"
    };
    
    std::string selectedModelPath;
    std::string textureBasePath;
    
    // 找到一个存在的模型文件路径
    for (const auto& path : modelPaths) {
        // 简单检查：尝试读取文件判断是否存在
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
        Logger::GetInstance().Error("未找到miku模型文件");
        return -1;
    }
    
    // 渐进式加载模型（在渲染循环中分批加载）
    Logger::GetInstance().Info("准备渐进式加载miku模型...");
    Logger::GetInstance().Info("将在渲染循环中加载，避免阻塞主线程");
    
    // 存储加载状态
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
    };
    auto loadState = std::make_shared<ProgressiveLoadState>();
    loadState->modelPath = selectedModelPath;
    loadState->texturePath = textureBasePath;
    loadState->shader = phongShader;
    
    // ============================================================
    // 3. 创建 ECS World
    // ============================================================
    auto world = std::make_shared<World>();
    world->Initialize();
    Logger::GetInstance().Info("✓ ECS World 初始化成功");
    
    // ============================================================
    // 4. 注册组件
    // ============================================================
    world->RegisterComponent<TransformComponent>();
    world->RegisterComponent<MeshRenderComponent>();
    world->RegisterComponent<SpriteRenderComponent>();  // ✅ 修复：ResourceLoadingSystem 需要
    world->RegisterComponent<CameraComponent>();
    world->RegisterComponent<LightComponent>();
    world->RegisterComponent<GeometryComponent>();  // 新增：几何形状组件
    world->RegisterComponent<LODComponent>();  // LOD 组件
    Logger::GetInstance().Info("✓ 组件注册完成");
    
    // ============================================================
    // 5. 注册系统（按优先级顺序）
    // ============================================================
    Logger::GetInstance().Info("注册系统...");
    
    // 新增系统
    world->RegisterSystem<WindowSystem>(renderer.get());              // 优先级 3
    Logger::GetInstance().Info("  ✓ WindowSystem (优先级 3)");
    
    world->RegisterSystem<CameraSystem>();                             // 优先级 5
    Logger::GetInstance().Info("  ✓ CameraSystem (优先级 5)");
    
    world->RegisterSystem<FirstPersonCameraSystem>();                  // 优先级 2
    Logger::GetInstance().Info("  ✓ FirstPersonCameraSystem (优先级 2)");
    
    world->RegisterSystem<TransformSystem>();                          // 优先级 10
    Logger::GetInstance().Info("  ✓ TransformSystem (优先级 10)");
    
    world->RegisterSystem<GeometrySystem>();                           // 优先级 15（新增）
    Logger::GetInstance().Info("  ✓ GeometrySystem (优先级 15) - 新增");
    
    world->RegisterSystem<ResourceLoadingSystem>(&asyncLoader);        // 优先级 20
    Logger::GetInstance().Info("  ✓ ResourceLoadingSystem (优先级 20)");
    
    world->RegisterSystem<DynamicMaterialTestSystem>();                // 优先级 25
    Logger::GetInstance().Info("  ✓ DynamicMaterialTestSystem (优先级 25)");
    
    world->RegisterSystem<LightSystem>(renderer.get());                // 优先级 50
    Logger::GetInstance().Info("  ✓ LightSystem (优先级 50)");
    
    world->RegisterSystem<UniformSystem>(renderer.get());              // 优先级 90（新增）
    Logger::GetInstance().Info("  ✓ UniformSystem (优先级 90) - 新增");
    
    world->RegisterSystem<LODUpdateSystem>();                          // 优先级 95（LOD 更新）
    Logger::GetInstance().Info("  ✓ LODUpdateSystem (优先级 95) - LOD 支持");
    
    world->RegisterSystem<MeshRenderSystem>(renderer.get());           // 优先级 100
    Logger::GetInstance().Info("  ✓ MeshRenderSystem (优先级 100)");
    
    world->RegisterSystem<ResourceCleanupSystem>(60.0f, 60);           // 优先级 1000（新增）
    Logger::GetInstance().Info("  ✓ ResourceCleanupSystem (优先级 1000) - 新增");
    
    // 后初始化（允许系统间相互引用）
    world->PostInitialize();
    Logger::GetInstance().Info("✓ 系统后初始化完成");
    
    // ============================================================
    // 6. 创建相机（参考20测试的设置）
    // ============================================================
    EntityID camera = world->CreateEntity({
        .name = "MainCamera",
        .active = true,
        .tags = {"camera", "main"}
    });
    
    // 先添加Transform组件
    world->AddComponent<TransformComponent>(camera, TransformComponent());
    auto& cameraTransform = world->GetComponent<TransformComponent>(camera);
    cameraTransform.SetPosition(Vector3(0.0f, 10.0f, 20.0f));  // 与20测试相同
    cameraTransform.LookAt(Vector3(0.0f, 8.0f, 0.0f));  // 看向模型头部位置
    
    // 添加Camera组件
    CameraComponent cameraComp;
    cameraComp.camera = std::make_shared<Camera>();
    cameraComp.camera->SetPerspective(60.0f, 16.0f/9.0f, 0.01f, 1000.0f);  // 近裁剪面0.01
    cameraComp.active = true;
    cameraComp.depth = 0;
    cameraComp.clearDepth = true;
    world->AddComponent(camera, cameraComp);
    
    Logger::GetInstance().Info("✓ 主相机创建完成（与20测试相同设置）");
    
    // ============================================================
    // 7. 创建光源
    // ============================================================
    EntityID light = world->CreateEntity({.name = "DirectionalLight"});
    
    // 先添加Transform组件
    world->AddComponent<TransformComponent>(light, TransformComponent());
    auto& lightTransform = world->GetComponent<TransformComponent>(light);
    lightTransform.SetPosition(Vector3(10.0f, 15.0f, 10.0f));  // 与20测试相同的光源位置
    lightTransform.SetRotation(MathUtils::FromEulerDegrees(45, 30, 0));
    
    // 添加Light组件
    LightComponent lightComp;
    lightComp.type = LightType::Directional;
    lightComp.color = Color(1.0f, 1.0f, 0.95f);
    lightComp.intensity = 1.2f;
    lightComp.enabled = true;
    world->AddComponent(light, lightComp);
    
    // 输出光源信息
    Logger::GetInstance().Info("✓ 定向光源创建完成");
    Logger::GetInstance().InfoFormat("  位置: (通过旋转计算)");
    Logger::GetInstance().InfoFormat("  颜色: (%.2f, %.2f, %.2f)", lightComp.color.r, lightComp.color.g, lightComp.color.b);
    Logger::GetInstance().InfoFormat("  强度: %.2f", lightComp.intensity);
    
    // ============================================================
    // 8. 等待创建miku模型实体（在渲染循环中处理）
    // ============================================================
    // 注意：实体创建将在主循环中完成，因为需要在OpenGL线程中上传GPU资源
    Logger::GetInstance().Info("✓ Miku模型实体将在渲染循环中创建");
    
    // 不再创建测试样例，只渲染miku模型
    
    // 不再创建额外的几何形状
    
    // ============================================================
    // 11. 主循环
    // ============================================================
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("进入主循环...");
    Logger::GetInstance().Info("控制说明:");
    Logger::GetInstance().Info("  W/S          - 前进/后退");
    Logger::GetInstance().Info("  A/D          - 左移/右移");
    Logger::GetInstance().Info("  Q/E          - 下降/上升");
    Logger::GetInstance().Info("  鼠标移动     - 旋转视角（第一人称）");
    Logger::GetInstance().Info("  F1           - 显示统计信息");
    Logger::GetInstance().Info("  ESC          - 退出程序");
    Logger::GetInstance().Info("========================================");
    
    // 启用相对鼠标模式（第一人称相机控制）
    SDL_SetWindowRelativeMouseMode(renderer->GetContext()->GetWindow(), true);
    
    bool running = true;
    uint64_t lastTime = SDL_GetPerformanceCounter();
    uint64_t frequency = SDL_GetPerformanceFrequency();
    
    float fpsTimer = 0.0f;
    int frameCount = 0;
    float lastFPS = 0.0f;
    
    // 异步加载状态跟踪
    bool entitiesCreated = false;
    
    while (running) {
        // ==================== 处理异步任务 ====================
        size_t processedTasks = asyncLoader.ProcessCompletedTasks(20);
        if (processedTasks > 0) {
            Logger::GetInstance().DebugFormat("本帧处理异步任务: %zu", processedTasks);
        }
        
        // ==================== 异步加载模型并渐进式创建实体 ====================
        if (!entitiesCreated) {
            if (!loadState->loadStarted) {
                Logger::GetInstance().Info("========================================");
                Logger::GetInstance().Info("开始异步加载模型...");
                Logger::GetInstance().Info("========================================");
                loadState->loadStarted = true;
                
                ModelLoadOptions options;
                options.flipUVs = true;
                options.autoUpload = true;
                options.basePath = loadState->texturePath;
                options.resourcePrefix = "async_miku";
                options.shaderOverride = loadState->shader;
                options.registerModel = true;
                options.registerMeshes = true;
                options.registerMaterials = true;
                options.updateDependencyGraph = true;
                
                auto weakState = std::weak_ptr<ProgressiveLoadState>(loadState);
                loadState->task = asyncLoader.LoadModelAsync(
                    loadState->modelPath,
                    "async_miku_model",
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
                                            std::string meshName = "async_miku_mesh_" + std::to_string(index);
                                            std::string materialName = "async_miku_material_" + std::to_string(index);
                                            
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
                                state->partsLoaded = 0;
                                
                                Logger::GetInstance().InfoFormat("✓ 模型异步加载完成，共 %zu 个部件", 
                                    state->meshNames.size());
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
            
            if (loadState->resourcesReady && !loadState->loadComplete && running) {
                if (!loadState->model) {
                    Logger::GetInstance().Error("模型指针为空，无法创建实体");
                    running = false;
                }
                
                const size_t partCount = loadState->meshNames.size();
                if (running && (partCount == 0 || loadState->materialNames.size() != partCount)) {
                    Logger::GetInstance().Error("异步加载结果缺少资源名称，无法创建实体");
                    running = false;
                }
                
                if (running) {
                    const size_t mikuCount = 100;
                    const size_t partsPerFrame = 10;
                    const size_t totalParts = partCount * mikuCount;
                    
                    size_t startIdx = loadState->partsLoaded;
                    size_t endIdx = std::min(startIdx + partsPerFrame, totalParts);
                    
                    // 存储每个miku的位置和旋转（避免重复计算）
                    static std::vector<Vector3> mikuPositions(mikuCount);
                    static std::vector<float> mikuRotations(mikuCount);
                    static bool positionsInitialized = false;
                    
                    if (!positionsInitialized) {
                        for (size_t i = 0; i < mikuCount; ++i) {
                            size_t layer = i / 20;
                            size_t posInLayer = i % 20;
                            float angle = (posInLayer * 360.0f / 20) * 3.14159f / 180.0f;
                            float radius = 10.0f + layer * 15.0f + (rand() % 10);
                            float x = radius * std::cos(angle) + (rand() % 6 - 3);
                            float z = radius * std::sin(angle) + (rand() % 6 - 3);
                            float y = (rand() % 5) - 2.0f;
                            
                            mikuPositions[i] = Vector3(x, y, z);
                            mikuRotations[i] = (rand() % 360) * 3.14159f / 180.0f;
                        }
                        positionsInitialized = true;
                        Logger::GetInstance().Info("✓ 已初始化100个Miku的随机位置和旋转");
                    }
                    
                    auto createEntityForPart = [&](size_t globalIdx) {
                        size_t mikuIdx = globalIdx / partCount;
                        size_t partIdx = globalIdx % partCount;
                        
                        Vector3 position = mikuPositions[mikuIdx];
                        float rotY = mikuRotations[mikuIdx];
                        
                        std::string entityName = "Miku_" + std::to_string(mikuIdx) + "_Part_" + std::to_string(partIdx);
                        EntityID mikuPart = world->CreateEntity({
                            .name = entityName,
                            .active = true,
                            .tags = {"miku", "model"}
                        });
                        
                        world->AddComponent<TransformComponent>(mikuPart, TransformComponent());
                        auto& transform = world->GetComponent<TransformComponent>(mikuPart);
                        transform.SetPosition(position);
                        transform.SetRotation(MathUtils::FromEulerDegrees(0, rotY * 180.0f / 3.14159f, 0));
                        transform.SetScale(0.08f);
                        
                        MeshRenderComponent meshComp;
                        meshComp.meshName = loadState->meshNames[partIdx];
                        meshComp.materialName = loadState->materialNames[partIdx];
                        meshComp.mesh = resourceManager.GetMesh(meshComp.meshName);
                        meshComp.material = resourceManager.GetMaterial(meshComp.materialName);
                        meshComp.resourcesLoaded = true;
                        meshComp.visible = true;
                        meshComp.castShadows = true;
                        meshComp.receiveShadows = true;
                        world->AddComponent(mikuPart, meshComp);
                        
                        // ==================== 添加 LOD 支持 ====================
                        LODComponent lodComp;
                        lodComp.config.enabled = true;
                        // 配置 LOD 距离阈值（根据场景大小调整）
                        // 距离 < 50: LOD0, 50-150: LOD1, 150-500: LOD2, 500-1000: LOD3, >1000: Culled
                        lodComp.config.distanceThresholds = {50.0f, 150.0f, 500.0f, 1000.0f};
                        lodComp.config.transitionDistance = 10.0f;  // 平滑过渡距离
                        lodComp.config.boundingBoxScale = 1.0f;
                        lodComp.config.textureStrategy = TextureLODStrategy::UseMipmap;  // 使用 mipmap
                        world->AddComponent(mikuPart, lodComp);
                    };
                    
                    for (size_t idx = startIdx; idx < endIdx; ++idx) {
                        createEntityForPart(idx);
                        loadState->partsLoaded++;
                    }
                    
                    Logger::GetInstance().InfoFormat("  创建进度: %zu / %zu (%.1f%%) - 实例 %zu",
                        loadState->partsLoaded,
                        totalParts,
                        totalParts > 0 ? (100.0f * loadState->partsLoaded) / totalParts : 0.0f,
                        mikuCount);
                    
                    if (loadState->partsLoaded >= totalParts) {
                        loadState->loadComplete = true;
                        entitiesCreated = true;
                        Logger::GetInstance().Info("========================================");
                        Logger::GetInstance().InfoFormat("✓ 压力测试场景创建完成（%zu 个Miku，共 %zu 个实体）", 
                            mikuCount, loadState->partsLoaded);
                        
                        auto camEntities = world->Query<CameraComponent, TransformComponent>();
                        if (!camEntities.empty()) {
                            auto& camTransform = world->GetComponent<TransformComponent>(camEntities[0]);
                            Vector3 camPos = camTransform.GetPosition();
                            Logger::GetInstance().InfoFormat("[调试] 相机位置: (%.1f, %.1f, %.1f)", 
                                camPos.x(), camPos.y(), camPos.z());
                        }
                        
                        auto lightEntities = world->Query<LightComponent, TransformComponent>();
                        if (!lightEntities.empty()) {
                            auto& lightTrans = world->GetComponent<TransformComponent>(lightEntities[0]);
                            Vector3 lightPos = lightTrans.GetPosition();
                            Logger::GetInstance().InfoFormat("[调试] 光源位置: (%.1f, %.1f, %.1f)", 
                                lightPos.x(), lightPos.y(), lightPos.z());
                        }
                        
                        Logger::GetInstance().Info("========================================");
                    }
                }
            }
        }
        
        // ==================== 时间计算 ====================
        uint64_t currentTime = SDL_GetPerformanceCounter();
        float deltaTime = static_cast<float>(currentTime - lastTime) / static_cast<float>(frequency);
        lastTime = currentTime;
        
        // FPS 计算
        fpsTimer += deltaTime;
        frameCount++;
        if (fpsTimer >= 1.0f) {
            lastFPS = frameCount / fpsTimer;
            frameCount = 0;
            fpsTimer = 0.0f;
        }
        
        // ==================== 事件处理 ====================
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
                
                // WASD QE 移动控制
                try {
                    auto* fpsCameraSys = world->GetSystem<FirstPersonCameraSystem>();
                    if (fpsCameraSys) {
                        switch (event.key.key) {
                            case SDLK_W: fpsCameraSys->SetMoveForward(true); break;
                            case SDLK_S: fpsCameraSys->SetMoveBackward(true); break;
                            case SDLK_A: fpsCameraSys->SetMoveLeft(true); break;
                            case SDLK_D: fpsCameraSys->SetMoveRight(true); break;
                            case SDLK_Q: fpsCameraSys->SetMoveDown(true); break;
                            case SDLK_E: fpsCameraSys->SetMoveUp(true); break;
                        }
                    }
                } catch (...) {}
                
                // F1 - 打印统计信息（避免在 Update 中获取系统，可能死锁）
                if (event.key.key == SDLK_F1) {
                    Logger::GetInstance().Info("========================================");
                    Logger::GetInstance().Info("统计信息");
                    Logger::GetInstance().Info("========================================");
                    
                    world->PrintStatistics();
                    
                    // 获取资源管理器统计（不涉及 World 锁）
                    const auto& resStats = resourceManager.GetStats();
                    Logger::GetInstance().InfoFormat("资源管理器统计:");
                    Logger::GetInstance().InfoFormat("  网格: %zu", resStats.meshCount);
                    Logger::GetInstance().InfoFormat("  纹理: %zu", resStats.textureCount);
                    Logger::GetInstance().InfoFormat("  材质: %zu", resStats.materialCount);
                    Logger::GetInstance().InfoFormat("  着色器: %zu", resStats.shaderCount);
                    Logger::GetInstance().InfoFormat("  总内存: %.2f MB", resStats.totalMemory / (1024.0f * 1024.0f));
                    
                    Logger::GetInstance().Info("========================================");
                    Logger::GetInstance().Info("提示：更详细的渲染和清理统计请查看日志输出");
                    Logger::GetInstance().Info("========================================");
                }
                
                // 移除了F2和F3的功能
            }
            
            // 按键释放
            else if (event.type == SDL_EVENT_KEY_UP) {
                try {
                    auto* fpsCameraSys = world->GetSystem<FirstPersonCameraSystem>();
                    if (fpsCameraSys) {
                        switch (event.key.key) {
                            case SDLK_W: fpsCameraSys->SetMoveForward(false); break;
                            case SDLK_S: fpsCameraSys->SetMoveBackward(false); break;
                            case SDLK_A: fpsCameraSys->SetMoveLeft(false); break;
                            case SDLK_D: fpsCameraSys->SetMoveRight(false); break;
                            case SDLK_Q: fpsCameraSys->SetMoveDown(false); break;
                            case SDLK_E: fpsCameraSys->SetMoveUp(false); break;
                        }
                    }
                } catch (...) {}
            }
            
            // 鼠标移动控制第一人称视角
            else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                try {
                    auto* fpsCameraSys = world->GetSystem<FirstPersonCameraSystem>();
                    if (fpsCameraSys) {
                        fpsCameraSys->OnMouseMove(event.motion.xrel, event.motion.yrel);
                    }
                } catch (const std::exception& e) {
                    Logger::GetInstance().ErrorFormat("鼠标控制失败: %s", e.what());
                }
            }
        }
        
        // ==================== 显示相机位置（调试） ====================
        try {
            static int frameCounter = 0;
            if (frameCounter++ % 60 == 0) {
                auto camEntities = world->Query<CameraComponent, TransformComponent>();
                if (!camEntities.empty()) {
                    auto& camTransform = world->GetComponent<TransformComponent>(camEntities[0]);
                    Vector3 camPos = camTransform.GetPosition();
                    Logger::GetInstance().InfoFormat("[相机] 位置: (%.1f, %.1f, %.1f)", 
                        camPos.x(), camPos.y(), camPos.z());
                }
            }
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat("Camera debug error: %s", e.what());
        }
        
        // ==================== 更新 World ====================
        // ✅ 安全性修复：添加异常保护，防止系统错误中断程序
        try {
            world->Update(deltaTime);
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat("World update error: %s", e.what());
            // 继续运行，不中断主循环
        }
        
        // ==================== 渲染 ====================
        // ✅ 安全性修复：渲染操作也需要异常保护
        try {
            renderer->BeginFrame();
            renderer->Clear();
            renderer->FlushRenderQueue();
            renderer->EndFrame();
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat("Rendering error: %s", e.what());
            // 尝试结束当前帧
            try {
                renderer->EndFrame();
            } catch (...) {
                // 忽略EndFrame的错误
            }
        }
        
        // 显示 FPS 和说明（在窗口标题中）
        if (frameCount % 30 == 0) {  // 每 30 帧更新一次标题
            size_t entityCount = world->GetEntityManager().GetActiveEntityCount();
            std::string title = "ECS Miku压力测试(100个) | FPS: " + 
                              std::to_string(static_cast<int>(lastFPS)) + 
                              " | 实体: " + std::to_string(entityCount) +
                              " | WASD移动 鼠标旋转";
            renderer->SetWindowTitle(title);
        }
        
        renderer->Present();
    }
    
    // ============================================================
    // 12. 清理
    // ============================================================
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("清理资源...");
    
    // ✅ 安全性修复：打印统计信息时添加异常保护
    try {
        world->PrintStatistics();
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("打印统计信息失败: %s", e.what());
    }
    
    // ✅ 安全性更新：使用 try-catch 保护系统获取和访问
    try {
        auto* meshSystem = world->GetSystem<MeshRenderSystem>();
        if (meshSystem) {
            const auto& stats = meshSystem->GetStats();
            Logger::GetInstance().InfoFormat("最终渲染统计:");
            Logger::GetInstance().InfoFormat("  可见网格: %zu", stats.visibleMeshes);
            Logger::GetInstance().InfoFormat("  剔除网格: %zu", stats.culledMeshes);
            Logger::GetInstance().InfoFormat("  绘制调用: %zu", stats.drawCalls);
        } else {
            Logger::GetInstance().Warning("MeshRenderSystem 未找到，跳过统计输出");
        }
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("获取渲染统计失败: %s", e.what());
    }
    
    // ✅ 安全性修复：清理操作添加异常保护，确保所有资源都被尝试清理
    try {
        world->Shutdown();
        Logger::GetInstance().Info("✓ ECS World 清理完成");
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("World清理异常: %s", e.what());
    }
    
    try {
        renderer->Shutdown();
        Logger::GetInstance().Info("✓ 渲染器清理完成");
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("渲染器清理异常: %s", e.what());
    }
    
    try {
        asyncLoader.Shutdown();
        Logger::GetInstance().Info("✓ 异步加载器清理完成");
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("异步加载器清理异常: %s", e.what());
    }
    
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("=== ECS Miku压力测试完成 ===");
    Logger::GetInstance().Info("========================================");
    
    return 0;
}


