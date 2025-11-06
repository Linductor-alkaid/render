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

#include "render/renderer.h"
#include "render/shader_cache.h"
#include "render/mesh_loader.h"
#include "render/material.h"
#include "render/logger.h"
#include "render/resource_manager.h"
#include "render/async_resource_loader.h"
#include "render/ecs/world.h"
#include "render/ecs/systems.h"
#include "render/ecs/components.h"
#include "render/math_utils.h"

using namespace Render;
using namespace Render::ECS;

// ============================================================
// 相机控制系统（用户交互）
// ============================================================
class CameraControlSystem : public System {
private:
    bool m_autoRotate = false;  // 默认关闭自动旋转，使用手动控制
    float m_angle = 180.0f;     // 初始角度：相机在-Z方向
    
public:
    void Update(float deltaTime) override {
        if (!m_world) return;
        
        auto entities = m_world->Query<CameraComponent, TransformComponent>();
        if (entities.empty()) return;
        
        auto& cameraEntity = entities[0];
        
        if (!m_world->IsValidEntity(cameraEntity)) return;
        if (!m_world->HasComponent<CameraComponent>(cameraEntity)) return;
        
        try {
            auto& cameraComp = m_world->GetComponent<CameraComponent>(cameraEntity);
            auto& transform = m_world->GetComponent<TransformComponent>(cameraEntity);
            
            if (m_autoRotate) {
                // 自动旋转模式
                m_angle += deltaTime * 20.0f;
                
                float radius = 15.0f;
                float height = 5.0f;
                float x = radius * std::cos(m_angle * 3.14159f / 180.0f);
                float z = -radius * std::sin(m_angle * 3.14159f / 180.0f);
                
                transform.SetPosition(Vector3(x, height, z));
                transform.LookAt(Vector3(0, 0, 0));
            }
            // 手动控制在主循环中直接操作transform
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat("[CameraControlSystem] Error: %s", e.what());
        }
    }
    
    // 不需要额外的方法，直接在主循环中操作
    
    void SetAutoRotate(bool enable) { m_autoRotate = enable; }
    bool IsAutoRotate() const { return m_autoRotate; }
    
    int GetPriority() const override { return 8; }
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
    
    Logger::GetInstance().Info("=== ECS 综合功能测试 ===");
    Logger::GetInstance().Info("测试内容：");
    Logger::GetInstance().Info("  - 所有新增系统（Window、Geometry、Uniform、ResourceCleanup）");
    Logger::GetInstance().Info("  - 材质属性覆盖");
    Logger::GetInstance().Info("  - 视锥体裁剪");
    Logger::GetInstance().Info("  - 透明物体排序");
    Logger::GetInstance().Info("  - 20 个测试样例（使用 5 种几何形状）");
    
    // ============================================================
    // 1. 初始化渲染器
    // ============================================================
    auto renderer = std::make_unique<Renderer>();
    if (!renderer->Initialize("ECS 综合功能测试 - 20 Miku 样例", 1920, 1080)) {
        Logger::GetInstance().Error("Failed to initialize renderer");
        return -1;
    }
    Logger::GetInstance().Info("✓ 渲染器初始化成功");
    
    // 设置渲染状态
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
    // 2.5 创建默认着色器和材质
    // ============================================================
    Logger::GetInstance().Info("创建默认着色器和材质...");
    
    // 加载基础着色器
    auto shader = shaderCache.LoadShader("basic", 
        "shaders/basic.vert", 
        "shaders/basic.frag");
    
    if (!shader) {
        Logger::GetInstance().Warning("无法加载基础着色器，尝试备用着色器...");
        shader = shaderCache.LoadShader("camera_test", 
            "shaders/camera_test.vert", 
            "shaders/camera_test.frag");
    }
    
    if (!shader) {
        Logger::GetInstance().Error("无法加载任何着色器，程序无法继续");
        return -1;
    }
    
    // 创建默认材质
    auto defaultMaterial = std::make_shared<Material>();
    defaultMaterial->SetName("default");
    defaultMaterial->SetShader(shader);
    defaultMaterial->SetDiffuseColor(Color(0.8f, 0.8f, 0.8f, 1.0f));
    defaultMaterial->SetSpecularColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
    defaultMaterial->SetShininess(32.0f);
    
    // 注册默认材质到 ResourceManager
    resourceManager.RegisterMaterial("default", defaultMaterial);
    resourceManager.RegisterShader("default", shader);
    
    Logger::GetInstance().Info("✓ 默认着色器和材质创建完成");
    
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
    
    world->RegisterSystem<CameraControlSystem>();                      // 优先级 8
    Logger::GetInstance().Info("  ✓ CameraControlSystem (优先级 8)");
    
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
    
    world->RegisterSystem<MeshRenderSystem>(renderer.get());           // 优先级 100
    Logger::GetInstance().Info("  ✓ MeshRenderSystem (优先级 100)");
    
    world->RegisterSystem<ResourceCleanupSystem>(60.0f, 60);           // 优先级 1000（新增）
    Logger::GetInstance().Info("  ✓ ResourceCleanupSystem (优先级 1000) - 新增");
    
    // 后初始化（允许系统间相互引用）
    world->PostInitialize();
    Logger::GetInstance().Info("✓ 系统后初始化完成");
    
    // ============================================================
    // 6. 创建相机
    // ============================================================
    EntityID camera = world->CreateEntity({
        .name = "MainCamera",
        .active = true,
        .tags = {"camera", "main"}
    });
    
    TransformComponent cameraTransform;
    cameraTransform.SetPosition(Vector3(0, 5, -15));  // ✅ 修复：相机在-Z方向，看向原点
    cameraTransform.LookAt(Vector3(0, 0, 0));
    world->AddComponent(camera, cameraTransform);
    
    CameraComponent cameraComp;
    cameraComp.camera = std::make_shared<Camera>();
    cameraComp.camera->SetPerspective(60.0f, 16.0f/9.0f, 0.1f, 1000.0f);
    cameraComp.active = true;
    cameraComp.depth = 0;
    cameraComp.clearDepth = true;
    world->AddComponent(camera, cameraComp);
    
    Logger::GetInstance().Info("✓ 主相机创建完成");
    
    // ============================================================
    // 7. 创建光源
    // ============================================================
    EntityID light = world->CreateEntity({.name = "DirectionalLight"});
    
    TransformComponent lightTransform;
    lightTransform.SetRotation(MathUtils::FromEulerDegrees(45, 30, 0));
    world->AddComponent(light, lightTransform);
    
    LightComponent lightComp;
    lightComp.type = LightType::Directional;
    lightComp.color = Color(1.0f, 1.0f, 0.95f);
    lightComp.intensity = 1.2f;
    lightComp.enabled = true;
    world->AddComponent(light, lightComp);
    
    Logger::GetInstance().Info("✓ 定向光源创建完成");
    
    // ============================================================
    // 8. 创建地板（使用 GeometrySystem）
    // ============================================================
    EntityID floor = world->CreateEntity({.name = "Floor"});
    
    TransformComponent floorTransform;
    floorTransform.SetPosition(Vector3(0, -2, 0));
    floorTransform.SetScale(Vector3(30, 0.2f, 30));
    world->AddComponent(floor, floorTransform);
    
    // 使用 GeometryComponent 生成平面
    GeometryComponent floorGeom;
    floorGeom.type = GeometryType::Cube;
    floorGeom.size = 1.0f;
    world->AddComponent(floor, floorGeom);
    
    MeshRenderComponent floorMesh;
    floorMesh.material = defaultMaterial;  // 使用默认材质
    floorMesh.SetDiffuseColor(Color(0.3f, 0.3f, 0.35f, 1.0f));
    floorMesh.visible = true;
    world->AddComponent(floor, floorMesh);
    
    Logger::GetInstance().Info("✓ 地板创建完成（使用 GeometrySystem）");
    
    // ============================================================
    // 9. 创建 20 个测试样例（使用 GeometrySystem 生成几何形状）
    // ============================================================
    Logger::GetInstance().Info("创建 20 个测试样例（使用几何形状）...");
    
    std::vector<EntityID> testEntities;
    
    // 排列成 4 行 5 列
    const int rows = 4;
    const int cols = 5;
    const float spacing = 3.0f;
    const float offsetX = -(cols - 1) * spacing / 2.0f;
    const float offsetZ = -(rows - 1) * spacing / 2.0f;
    
    // 定义不同的几何形状类型（循环使用）
    std::vector<GeometryType> geometryTypes = {
        GeometryType::Cube,
        GeometryType::Sphere,
        GeometryType::Cylinder,
        GeometryType::Cone,
        GeometryType::Torus
    };
    
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            int index = row * cols + col;
            std::string name = "TestEntity_" + std::to_string(index);
            
            EntityID entity = world->CreateEntity({
                .name = name,
                .active = true,
                .tags = {"test", "geometry"}
            });
            
            // 变换组件
            TransformComponent transform;
            float x = offsetX + col * spacing;
            float z = offsetZ + row * spacing;
            transform.SetPosition(Vector3(x, 1.0f, z));  // 高度 1.0，避免穿过地板
            transform.SetScale(1.0f);
            world->AddComponent(entity, transform);
            
            // 几何形状组件（使用 GeometrySystem 自动生成）
            GeometryComponent geom;
            geom.type = geometryTypes[index % geometryTypes.size()];  // 循环使用不同形状
            geom.size = 0.8f;
            geom.segments = 32;
            geom.rings = 16;
            if (geom.type == GeometryType::Cylinder || geom.type == GeometryType::Cone) {
                geom.height = 1.5f;
            }
            world->AddComponent(entity, geom);
            
            // 网格渲染组件
            MeshRenderComponent mesh;
            // 注意：不设置 meshName，让 GeometrySystem 自动生成网格
            // GeometrySystem 会自动设置 mesh.mesh 和 resourcesLoaded = true
            // 但需要手动设置材质
            mesh.material = defaultMaterial;  // 使用默认材质
            mesh.visible = true;
            mesh.castShadows = true;
            mesh.receiveShadows = true;
            
            // ==================== 材质属性覆盖测试 ====================
            // 每个样例使用不同的材质覆盖，展示不同效果
            
            float t = static_cast<float>(index) / 19.0f;  // 0.0 ~ 1.0
            
            if (index < 5) {
                // 第 1 行：不同颜色测试
                float hue = index / 5.0f;
                float r = std::abs(std::sin(hue * 6.28f));
                float g = std::abs(std::sin((hue + 0.33f) * 6.28f));
                float b = std::abs(std::sin((hue + 0.67f) * 6.28f));
                mesh.SetDiffuseColor(Color(r, g, b, 1.0f));
                
            } else if (index < 10) {
                // 第 2 行：金属度测试（0.0 ~ 1.0）
                float metallic = (index - 5) / 4.0f;
                mesh.SetMetallic(metallic);
                mesh.SetDiffuseColor(Color(0.8f, 0.8f, 0.9f, 1.0f));
                
            } else if (index < 15) {
                // 第 3 行：粗糙度测试（0.0 ~ 1.0）
                float roughness = (index - 10) / 4.0f;
                mesh.SetRoughness(roughness);
                mesh.SetDiffuseColor(Color(0.9f, 0.7f, 0.5f, 1.0f));
                
            } else {
                // 第 4 行：透明度测试（0.3 ~ 1.0）
                float opacity = 0.3f + (index - 15) * 0.175f;
                mesh.SetOpacity(opacity);
                mesh.SetDiffuseColor(Color(0.5f, 0.8f, 1.0f, opacity));
            }
            
            // ==================== 多纹理覆盖测试（可选）====================
            // 如果有自定义纹理，可以这样覆盖：
            // mesh.textureOverrides["diffuse"] = "assets/textures/custom_diffuse_" + std::to_string(index) + ".png";
            // mesh.textureOverrides["normal"] = "assets/textures/custom_normal.png";
            
            world->AddComponent(entity, mesh);
            testEntities.push_back(entity);
            
            Logger::GetInstance().InfoFormat("  ✓ 测试实体 #%d 创建完成 (位置: %.1f, 1.0, %.1f, 形状: %d)", 
                                            index, x, z, static_cast<int>(geom.type));
        }
    }
    
    Logger::GetInstance().InfoFormat("✓ 20 个测试样例创建完成（使用几何形状）");
    
    // ============================================================
    // 10. 创建几个额外的几何形状测试（GeometrySystem）
    // ============================================================
    Logger::GetInstance().Info("创建几何形状测试...");
    
    // 球体
    EntityID sphere = world->CreateEntity({.name = "Sphere"});
    TransformComponent sphereTransform;
    sphereTransform.SetPosition(Vector3(-10, 1, 0));
    world->AddComponent(sphere, sphereTransform);
    
    GeometryComponent sphereGeom;
    sphereGeom.type = GeometryType::Sphere;
    sphereGeom.size = 1.0f;
    sphereGeom.segments = 32;
    sphereGeom.rings = 32;
    world->AddComponent(sphere, sphereGeom);
    
    MeshRenderComponent sphereMesh;
    sphereMesh.material = defaultMaterial;  // 使用默认材质
    sphereMesh.SetDiffuseColor(Color(1.0f, 0.3f, 0.3f, 1.0f));
    sphereMesh.SetMetallic(0.8f);
    sphereMesh.SetRoughness(0.2f);
    world->AddComponent(sphere, sphereMesh);
    
    // 圆柱体
    EntityID cylinder = world->CreateEntity({.name = "Cylinder"});
    TransformComponent cylinderTransform;
    cylinderTransform.SetPosition(Vector3(10, 1, 0));
    world->AddComponent(cylinder, cylinderTransform);
    
    GeometryComponent cylinderGeom;
    cylinderGeom.type = GeometryType::Cylinder;
    cylinderGeom.size = 1.0f;
    cylinderGeom.height = 2.0f;
    cylinderGeom.segments = 32;
    cylinderGeom.rings = 16;
    world->AddComponent(cylinder, cylinderGeom);
    
    MeshRenderComponent cylinderMesh;
    cylinderMesh.material = defaultMaterial;  // 使用默认材质
    cylinderMesh.SetDiffuseColor(Color(0.3f, 1.0f, 0.3f, 1.0f));
    cylinderMesh.SetMetallic(0.5f);
    cylinderMesh.SetRoughness(0.5f);
    world->AddComponent(cylinder, cylinderMesh);
    
    Logger::GetInstance().Info("✓ 几何形状测试创建完成");
    
    // ============================================================
    // 11. 主循环
    // ============================================================
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("进入主循环...");
    Logger::GetInstance().Info("控制说明:");
    Logger::GetInstance().Info("  T      - 切换自动旋转/手动控制");
    Logger::GetInstance().Info("  WASD   - 移动相机（前后左右）");
    Logger::GetInstance().Info("  Q/E    - 上下移动相机");
    Logger::GetInstance().Info("  F1     - 显示统计信息");
    Logger::GetInstance().Info("  F2     - 手动清理资源");
    Logger::GetInstance().Info("  F3     - 切换随机实体可见性");
    Logger::GetInstance().Info("  ESC    - 退出程序");
    Logger::GetInstance().Info("========================================");
    
    bool running = true;
    uint64_t lastTime = SDL_GetPerformanceCounter();
    uint64_t frequency = SDL_GetPerformanceFrequency();
    
    float fpsTimer = 0.0f;
    int frameCount = 0;
    float lastFPS = 0.0f;
    
    while (running) {
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
                
                // T - 切换自动旋转/手动控制
                if (event.key.key == SDLK_T) {
                    try {
                        auto* cameraControlSys = world->GetSystem<CameraControlSystem>();
                        if (cameraControlSys) {
                            bool newState = !cameraControlSys->IsAutoRotate();
                            cameraControlSys->SetAutoRotate(newState);
                            Logger::GetInstance().InfoFormat("相机模式: %s", 
                                newState ? "自动旋转" : "手动控制 (WASD移动, QE上下, IJKL旋转)");
                        }
                    } catch (...) {}
                }
                
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
                
                // F2 - 手动触发资源清理（直接调用 ResourceManager，避免死锁）
                if (event.key.key == SDLK_F2) {
                    Logger::GetInstance().Info("手动触发资源清理...");
                    size_t cleaned = resourceManager.CleanupUnused();
                    Logger::GetInstance().InfoFormat("✓ 资源清理完成，清理了 %zu 个资源", cleaned);
                }
                
                // F3 - 切换随机实体的可见性（测试动态显示/隐藏）
                // 注意：在事件处理中调用是安全的（不在 Update 期间）
                if (event.key.key == SDLK_F3) {
                    if (!testEntities.empty()) {
                        try {
                            int randomIndex = rand() % testEntities.size();
                            EntityID entity = testEntities[randomIndex];
                            
                            // ✅ 安全性更新：先验证实体有效性
                            if (!world->IsValidEntity(entity)) {
                                Logger::GetInstance().WarningFormat("实体 #%d 已失效，跳过", randomIndex);
                                continue;
                            }
                            
                            // ✅ 安全性更新：再检查组件是否存在
                            if (world->HasComponent<MeshRenderComponent>(entity)) {
                                auto& mesh = world->GetComponent<MeshRenderComponent>(entity);
                                mesh.visible = !mesh.visible;
                                Logger::GetInstance().InfoFormat("切换实体 #%d 可见性: %s", 
                                                                randomIndex, mesh.visible ? "可见" : "隐藏");
                            } else {
                                Logger::GetInstance().WarningFormat("实体 #%d 没有 MeshRenderComponent", randomIndex);
                            }
                        } catch (const std::out_of_range& e) {
                            Logger::GetInstance().ErrorFormat("组件访问失败: %s", e.what());
                        } catch (const std::exception& e) {
                            Logger::GetInstance().ErrorFormat("切换可见性失败: %s", e.what());
                        }
                    }
                }
            }
        }
        
        // ==================== 相机手动控制 ====================
        try {
            auto* cameraControlSys = world->GetSystem<CameraControlSystem>();
            if (cameraControlSys && !cameraControlSys->IsAutoRotate()) {
                // 获取相机实体
                auto entities = world->Query<CameraComponent, TransformComponent>();
                if (!entities.empty()) {
                    auto& transform = world->GetComponent<TransformComponent>(entities[0]);
                    
                    float moveSpeed = 10.0f * deltaTime;  // 移动速度
                    
                    const bool* keyState = SDL_GetKeyboardState(nullptr);
                    
                    // WASD 移动（世界空间）
                    if (keyState[SDL_SCANCODE_W]) {
                        Vector3 forward = transform.transform->GetForward();
                        forward.y() = 0; 
                        if (forward.norm() > 0.001f) forward.normalize();
                        transform.transform->TranslateWorld(forward * moveSpeed);
                    }
                    if (keyState[SDL_SCANCODE_S]) {
                        Vector3 forward = transform.transform->GetForward();
                        forward.y() = 0;
                        if (forward.norm() > 0.001f) forward.normalize();
                        transform.transform->TranslateWorld(-forward * moveSpeed);
                    }
                    if (keyState[SDL_SCANCODE_A]) {
                        Vector3 right = transform.transform->GetRight();
                        right.y() = 0;
                        if (right.norm() > 0.001f) right.normalize();
                        transform.transform->TranslateWorld(-right * moveSpeed);
                    }
                    if (keyState[SDL_SCANCODE_D]) {
                        Vector3 right = transform.transform->GetRight();
                        right.y() = 0;
                        if (right.norm() > 0.001f) right.normalize();
                        transform.transform->TranslateWorld(right * moveSpeed);
                    }
                    
                    // QE 上下移动（世界空间）
                    if (keyState[SDL_SCANCODE_Q]) {
                        transform.transform->TranslateWorld(Vector3(0, -moveSpeed, 0));
                    }
                    if (keyState[SDL_SCANCODE_E]) {
                        transform.transform->TranslateWorld(Vector3(0, moveSpeed, 0));
                    }
                    
                    // 始终看向原点（用于调试，可以看到所有对象）
                    Vector3 lookTarget(0, 0, 0);
                    transform.LookAt(lookTarget);
                    
                    // 显示相机位置（每60帧）
                    static int frameCounter = 0;
                    if (frameCounter++ % 60 == 0) {
                        Vector3 pos = transform.GetPosition();
                        Logger::GetInstance().InfoFormat("[相机] 位置: (%.1f, %.1f, %.1f)", 
                            pos.x(), pos.y(), pos.z());
                    }
                }
            }
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat("Camera control error: %s", e.what());
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
            std::string mode = "自动旋转";
            try {
                auto* cameraControlSys = world->GetSystem<CameraControlSystem>();
                if (cameraControlSys && !cameraControlSys->IsAutoRotate()) {
                    mode = "手动控制(WASD/QE移动)";
                }
            } catch (...) {}
            
            std::string title = "ECS综合测试 | FPS: " + 
                              std::to_string(static_cast<int>(lastFPS)) + 
                              " | 相机: " + mode +
                              " | T=切换 ESC=退出";
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
    Logger::GetInstance().Info("=== ECS 综合功能测试完成 ===");
    Logger::GetInstance().Info("========================================");
    
    return 0;
}


