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
#include "render/physics/physics_components.h"
#include "render/physics/physics_systems.h"
#include "render/physics/physics_world.h"
#include "render/physics/physics_config.h"
#include "render/physics/physics_utils.h"
#include "render/physics/collision/broad_phase.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <random>

using namespace Render;
using namespace Render::ECS;
using namespace Render::Physics;
 
 namespace {
 
struct LightingEntities {
    EntityID pointLight;
    EntityID spotLight;
    EntityID centerpiece;
    // 注意：fallingSphere 不再存储在entities中，每次重置时重新创建
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
    // 地面物理位置：参考测试代码，使用y=0，碰撞体半高0.5，上表面在y=0.5
    // 渲染位置稍微向下（y=-0.01）用于视觉对齐
    groundTransform.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
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
    
    // 地面物理：静态刚体 + 盒子碰撞体
    // 参考测试代码：地面位置(0,0,0)，碰撞体半高度0.5，上表面在y=0.5，无center偏移
    ColliderComponent groundCollider = ColliderComponent::CreateBox(Vector3(15.0f, 0.5f, 15.0f));
    // 地面使用低弹性材质，避免小球过度弹跳
    auto groundPhysMaterial = std::make_shared<PhysicsMaterial>(PhysicsMaterial::Default());
    groundPhysMaterial->restitution = 0.1f;  // 低弹性
    groundPhysMaterial->friction = 0.7f;  // 高摩擦
    groundCollider.material = groundPhysMaterial;
    // 无center偏移，碰撞体中心在Transform位置(0,0,0)，半高0.5，上表面在y=0.5
    world.AddComponent(ground, groundCollider);
    
    RigidBodyComponent groundBody;
    groundBody.type = RigidBodyComponent::BodyType::Static;
    // 初始化静态刚体的previousPosition（用于插值，位置从Transform获取）
    groundBody.previousPosition = groundTransform.GetPosition();
    groundBody.previousRotation = groundTransform.GetRotation();
    world.AddComponent(ground, groundBody);
 
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
    
    // 中心球体物理：静态刚体 + 球体碰撞体（缩放2.0，所以半径是2.0）
    ColliderComponent centerCollider = ColliderComponent::CreateSphere(2.0f);
    centerCollider.material = std::make_shared<PhysicsMaterial>(PhysicsMaterial::Default());
    world.AddComponent(centerpiece, centerCollider);
    
    RigidBodyComponent centerBody;
    centerBody.type = RigidBodyComponent::BodyType::Static;
    world.AddComponent(centerpiece, centerBody);
 
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
        
        // 柱子物理：静态刚体 + 胶囊体碰撞体（半径0.5*0.6=0.3，高度3.0*3.0=9.0）
        // 注意：使用胶囊体近似圆柱体（胶囊体 = 圆柱体 + 两端半球）
        ColliderComponent columnCollider = ColliderComponent::CreateCapsule(0.3f, 9.0f);
        columnCollider.material = std::make_shared<PhysicsMaterial>(PhysicsMaterial::Default());
        world.AddComponent(column, columnCollider);
        
        RigidBodyComponent columnBody;
        columnBody.type = RigidBodyComponent::BodyType::Static;
        world.AddComponent(column, columnBody);
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

    // 注意：小球不在CreateScene中创建，而是在主循环中创建和重置

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
    world.RegisterComponent<RigidBodyComponent>();
    world.RegisterComponent<ColliderComponent>();

    // 创建物理配置和物理世界
    PhysicsConfig config = PhysicsConfig::Default();
    config.gravity = Vector3(0, -9.81f, 0);
    config.broadPhaseType = BroadPhaseType::SpatialHash;
    config.spatialHashCellSize = 5.0f;
    config.fixedDeltaTime = 1.0f / 60.0f;
    config.solverIterations = 20;  // 增加迭代次数，提高稳定性
    config.positionIterations = 8;  // 增加位置迭代次数，减少穿透
    
    // 启用 CCD（连续碰撞检测）
    config.enableCCD = true;
    config.ccdVelocityThreshold = 10.0f;       // 速度阈值 10 m/s
    config.ccdDisplacementThreshold = 0.5f;    // 位移阈值（相对尺寸比例）
    config.maxCCDObjects = 50;                  // 每帧最大 CCD 对象数
    config.enableBroadPhaseCCD = true;          // 启用 Broad Phase 加速
    
    PhysicsWorld physicsWorld(&world, config);

    // 注册物理系统
    auto* collisionSystem = world.RegisterSystem<CollisionDetectionSystem>();
    auto broadPhase = std::make_unique<SpatialHashBroadPhase>(config.spatialHashCellSize);
    collisionSystem->SetBroadPhase(std::move(broadPhase));
    
    auto* physicsSystem = world.RegisterSystem<PhysicsUpdateSystem>();
    physicsSystem->SetGravity(config.gravity);
    physicsSystem->SetFixedDeltaTime(config.fixedDeltaTime);
    physicsSystem->SetConfig(config);  // 应用 CCD 配置

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
    
    // 随机数生成器（用于随机生成小球位置）
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> xDist(-8.0f, 8.0f);   // X方向随机范围
    std::uniform_real_distribution<float> zDist(-8.0f, 8.0f);   // Z方向随机范围
    std::uniform_real_distribution<float> yDist(8.0f, 12.0f);   // Y方向随机范围（高度）

    Logger::GetInstance().Info("Controls: ESC to exit, R to reset falling sphere");
    
    // 当前小球实体ID（初始为无效）
    EntityID currentFallingSphere = EntityID::Invalid();
    
    // 创建新小球的函数
    auto CreateFallingSphere = [&](const Ref<Mesh>& sphereMesh, const std::shared_ptr<Material>& objectMat) -> EntityID {
        Vector3 randomPos(xDist(gen), yDist(gen), zDist(gen));
        
        EntityID fallingSphere = world.CreateEntity({ .name = "FallingSphere" });
        TransformComponent fallingSphereTransform;
        fallingSphereTransform.SetPosition(randomPos);
        fallingSphereTransform.SetScale(0.5f);  // 小球，缩放0.5
        world.AddComponent(fallingSphere, fallingSphereTransform);

        MeshRenderComponent fallingSphereRender;
        fallingSphereRender.mesh = sphereMesh;
        fallingSphereRender.meshName = "lighting_sphere_mesh";
        fallingSphereRender.material = objectMat;
        fallingSphereRender.materialName = "lighting_object_mat";
        fallingSphereRender.resourcesLoaded = true;
        world.AddComponent(fallingSphere, fallingSphereRender);
        
        // 小球物理：动态刚体 + 球体碰撞体（半径1.0*0.5=0.5）
        ColliderComponent fallingSphereCollider = ColliderComponent::CreateSphere(0.5f);
        // 使用低弹性材质，避免过度弹跳（橡胶材质restitution=0.9太高）
        auto sphereMaterial = std::make_shared<PhysicsMaterial>(PhysicsMaterial::Default());
        sphereMaterial->restitution = 0.2f;  // 低弹性，减少弹跳
        sphereMaterial->friction = 0.6f;  // 中等摩擦
        fallingSphereCollider.material = sphereMaterial;
        world.AddComponent(fallingSphere, fallingSphereCollider);
        
        RigidBodyComponent fallingSphereBody;
        fallingSphereBody.type = RigidBodyComponent::BodyType::Dynamic;
        PhysicsUtils::InitializeRigidBody(fallingSphereBody, fallingSphereCollider);
        // 初始化previousPosition用于插值
        fallingSphereBody.previousPosition = randomPos;
        fallingSphereBody.previousRotation = fallingSphereTransform.GetRotation();
        // 启用 CCD（高速下落物体）
        fallingSphereBody.useCCD = true;
        world.AddComponent(fallingSphere, fallingSphereBody);
        
        Logger::GetInstance().InfoFormat(
            "[Physics Demo] Created new falling sphere at position: (%.2f, %.2f, %.2f)",
            randomPos.x(), randomPos.y(), randomPos.z()
        );
        
        return fallingSphere;
    };
    
    // 重置小球：删除旧的，创建新的
    auto ResetFallingSphere = [&]() {
        // 删除旧的小球
        if (currentFallingSphere.IsValid() && world.IsValidEntity(currentFallingSphere)) {
            world.DestroyEntity(currentFallingSphere);
        }
        // 创建新的小球
        currentFallingSphere = CreateFallingSphere(sphereMesh, objectMaterial);
    };
    
    // 首次创建小球
    currentFallingSphere = CreateFallingSphere(sphereMesh, objectMaterial);
 
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
        
        // 使用键盘状态检查R键（参考其他测试）
        const bool* keyboard = SDL_GetKeyboardState(nullptr);
        static bool rKeyPressed = false;
        if (keyboard[SDL_SCANCODE_R] && !rKeyPressed) {
            ResetFallingSphere();
            rKeyPressed = true;
        } else if (!keyboard[SDL_SCANCODE_R]) {
            rKeyPressed = false;
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
        
        // 中心球体旋转（保持原有的旋转动画）
        auto& centerpieceTransform = world.GetComponent<TransformComponent>(entities.centerpiece);
        Quaternion rotation = MathUtils::FromEulerDegrees(0.0f, timeAccumulator * 35.0f, 0.0f);
        centerpieceTransform.SetRotation(rotation);

        // 重要：先更新碰撞检测系统（优先级100，会在物理更新之前执行）
        // CollisionDetectionSystem 需要在 PhysicsUpdateSystem 之前更新，以便提供碰撞对
        if (auto* collisionSystem = world.GetSystem<CollisionDetectionSystem>()) {
            collisionSystem->Update(deltaTime);
        }
        
        // 更新物理世界（自动处理同步和插值）
        // PhysicsWorld::Step 会调用 PhysicsUpdateSystem，它会使用 CollisionDetectionSystem 的碰撞对
        physicsWorld.Step(deltaTime);

        renderer->BeginFrame();
        renderer->Clear();

        // 更新其他ECS系统（渲染相关）
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
 