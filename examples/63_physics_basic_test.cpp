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
#include "render/ecs/physics/physics_components.h"
#include "render/ecs/physics/physics_system.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <vector>
#include <random>
#include <string>

using namespace Render;
using namespace Render::ECS;

namespace {

struct PhysicsEntities {
    EntityID ground;
    std::vector<EntityID> spheres;
};

// 生成不重叠的随机位置
Vector3 GenerateNonOverlappingPosition(
    const std::vector<Vector3>& existingPositions,
    float minSeparation,
    float spreadRadius,
    float startHeight,
    std::mt19937& gen,
    int maxAttempts = 50)
{
    std::uniform_real_distribution<float> angleDist(0.0f, MathUtils::TWO_PI);
    std::uniform_real_distribution<float> radiusDist(2.0f, spreadRadius);
    std::uniform_real_distribution<float> heightDist(0.0f, 5.0f);
    
    Vector3 newPos;
    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        float angle = angleDist(gen);
        float r = radiusDist(gen);
        float x = std::cos(angle) * r;
        float z = std::sin(angle) * r;
        float y = startHeight + heightDist(gen);
        newPos = Vector3(x, y, z);
        
        // 检查与已有位置的距离
        bool tooClose = false;
        for (const auto& pos : existingPositions) {
            if ((newPos - pos).norm() < minSeparation) {
                tooClose = true;
                break;
            }
        }
        
        if (!tooClose) {
            return newPos;
        }
    }
    
    // 如果找不到合适位置，返回一个稍微远一点的位置
    Logger::GetInstance().Warning("Could not find non-overlapping position, using fallback");
    return newPos;
}

void RegisterResources(ResourceManager& resMgr, 
                       const std::shared_ptr<Material>& groundMat,
                       const std::vector<std::shared_ptr<Material>>& sphereMats,
                       const Ref<Mesh>& groundMesh, 
                       const Ref<Mesh>& sphereMesh) 
{
    resMgr.RegisterMaterial("physics_ground_mat", groundMat);
    for (size_t i = 0; i < sphereMats.size(); ++i) {
        resMgr.RegisterMaterial("physics_sphere_mat_" + std::to_string(i), sphereMats[i]);
    }
    resMgr.RegisterMesh("physics_ground_mesh", groundMesh);
    resMgr.RegisterMesh("physics_sphere_mesh", sphereMesh);
}

PhysicsEntities CreateScene(World& world, 
                             const Ref<Mesh>& groundMesh, 
                             const Ref<Mesh>& sphereMesh,
                             const std::shared_ptr<Material>& groundMat,
                             const std::vector<std::shared_ptr<Material>>& sphereMats) 
{
    PhysicsEntities entities{};

    // ==================== 创建地面 ====================
    Logger::GetInstance().Info("Creating ground...");
    EntityID ground = world.CreateEntity({ .name = "Ground" });
    
    TransformComponent groundTransform;
    groundTransform.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
    groundTransform.SetScale(Vector3(1.0f, 1.0f, 1.0f));
    world.AddComponent(ground, groundTransform);

    MeshRenderComponent groundRender;
    groundRender.mesh = groundMesh;
    groundRender.meshName = "physics_ground_mesh";
    groundRender.material = groundMat;
    groundRender.materialName = "physics_ground_mat";
    groundRender.resourcesLoaded = true;
    groundRender.receiveShadows = true;
    world.AddComponent(ground, groundRender);

    ColliderComponent groundCollider;
    groundCollider.SetBox(Vector3(30.0f, 1.0f, 30.0f));
    world.AddComponent(ground, groundCollider);

    RigidBodyComponent groundRigidBody;
    groundRigidBody.type = RigidBodyType::Static;
    groundRigidBody.mass = 0.0f;
    groundRigidBody.friction = 0.5f;
    groundRigidBody.restitution = 0.3f;
    world.AddComponent(ground, groundRigidBody);
    
    entities.ground = ground;
    Logger::GetInstance().Info("Ground created successfully");

    // ==================== 创建多个掉落球体 ====================
    const int numSpheres = 8;
    const float spreadRadius = 5.0f;      // 分布半径5米
    const float startHeight = 12.0f;      // 起始高度12米
    const float minSeparation = 2.5f;     // 最小间距2.5米（避免初始碰撞）
    
    Logger::GetInstance().Info("Creating " + std::to_string(numSpheres) + " spheres...");
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::vector<Vector3> positions;
    
    for (int i = 0; i < numSpheres; ++i) {
        // 生成不重叠的位置
        Vector3 newPos = GenerateNonOverlappingPosition(
            positions, minSeparation, spreadRadius, startHeight, gen
        );
        positions.push_back(newPos);
        
        // 创建球体实体
        EntityID sphere = world.CreateEntity({ 
            .name = "FallingSphere_" + std::to_string(i) 
        });
        
        // Transform组件（必须在添加刚体之前设置位置）
        TransformComponent sphereTransform;
        sphereTransform.SetPosition(newPos);
        sphereTransform.SetScale(Vector3(1.0f, 1.0f, 1.0f));  // 明确设置缩放向量
        world.AddComponent(sphere, sphereTransform);

        // 渲染组件
        MeshRenderComponent sphereRender;
        sphereRender.mesh = sphereMesh;  // 共享mesh引用是安全的
        sphereRender.meshName = "physics_sphere_mesh";
        sphereRender.material = sphereMats[i % sphereMats.size()];
        sphereRender.materialName = "physics_sphere_mat_" + std::to_string(i % sphereMats.size());
        sphereRender.resourcesLoaded = true;
        sphereRender.visible = true;  // 确保可见
        sphereRender.castShadows = true;
        sphereRender.receiveShadows = true;
        world.AddComponent(sphere, sphereRender);

        // 碰撞体组件
        ColliderComponent sphereCollider;
        sphereCollider.SetSphere(1.0f);  // 半径1.0米
        world.AddComponent(sphere, sphereCollider);

        // 刚体组件（添加在Transform之后，确保Transform已设置）
        RigidBodyComponent sphereRigidBody;
        sphereRigidBody.type = RigidBodyType::Dynamic;
        sphereRigidBody.mass = 5.0f + (i % 3) * 2.0f;  // 质量: 5kg, 7kg, 9kg
        sphereRigidBody.friction = 0.5f;
        sphereRigidBody.restitution = 0.6f + (i % 3) * 0.1f;  // 弹性: 0.6, 0.7, 0.8
        sphereRigidBody.useGravity = true;
        sphereRigidBody.syncMode = RigidBodyComponent::SyncMode::PhysicsToTransform;
        // 初始位置应该从Transform组件中读取，但为了保险，也可以显式设置
        world.AddComponent(sphere, sphereRigidBody);
        
        // 验证组件是否正确添加
        auto& verifyTransform = world.GetComponent<TransformComponent>(sphere);
        auto& verifyRender = world.GetComponent<MeshRenderComponent>(sphere);
        Vector3 verifyPos = verifyTransform.GetPosition();
        
        bool hasMesh = verifyRender.mesh != nullptr;
        bool hasMaterial = verifyRender.material != nullptr;
        
        Logger::GetInstance().Info("Sphere " + std::to_string(i) + 
            " - Pos: (" + 
            std::to_string(verifyPos.x()) + ", " + 
            std::to_string(verifyPos.y()) + ", " + 
            std::to_string(verifyPos.z()) + 
            ") - Mesh: " + std::string(hasMesh ? "OK" : "NULL") +
            " - Material: " + std::string(hasMaterial ? "OK" : "NULL") +
            " - Visible: " + std::string(verifyRender.visible ? "true" : "false") +
            " - ResourcesLoaded: " + std::string(verifyRender.resourcesLoaded ? "true" : "false"));
        
        entities.spheres.push_back(sphere);
        
        Logger::GetInstance().Info("Sphere " + std::to_string(i) + 
            " created at (" + 
            std::to_string(newPos.x()) + ", " + 
            std::to_string(newPos.y()) + ", " + 
            std::to_string(newPos.z()) + ")");
    }
    
    Logger::GetInstance().Info("All spheres created successfully");

    // ==================== 添加方向光 ====================
    EntityID sun = world.CreateEntity({ .name = "SunLight" });
    
    TransformComponent sunTransform;
    sunTransform.SetPosition(Vector3(-10.0f, 15.0f, 8.0f));
    sunTransform.transform->LookAt(Vector3(0.0f, 0.0f, 0.0f));
    world.AddComponent(sun, sunTransform);

    LightComponent sunLight;
    sunLight.type = LightType::Directional;
    sunLight.color = Color(1.0f, 0.97f, 0.9f, 1.0f);
    sunLight.intensity = 1.2f;
    sunLight.castShadows = true;
    sunLight.enabled = true;
    world.AddComponent(sun, sunLight);

    Logger::GetInstance().Info("Directional light created");

    return entities;
}

void SetupCamera(World& world) {
    Logger::GetInstance().Info("Setting up camera...");
    
    EntityID cameraEntity = world.CreateEntity({ .name = "MainCamera" });

    TransformComponent cameraTransform;
    // 相机位置更高更远，确保能看到所有球体
    cameraTransform.SetPosition(Vector3(0.0f, 10.0f, 20.0f));
    cameraTransform.transform->LookAt(Vector3(0.0f, 5.0f, 0.0f));
    world.AddComponent(cameraEntity, cameraTransform);

    CameraComponent cameraComp;
    cameraComp.camera = std::make_shared<Camera>();
    cameraComp.camera->SetPerspective(75.0f, 16.0f / 9.0f, 0.1f, 200.0f);
    cameraComp.active = true;
    world.AddComponent(cameraEntity, cameraComp);
    
    Logger::GetInstance().Info("Camera setup complete");
}

void ResetSpheres(World& world, 
                  const PhysicsEntities& entities,
                  bool& needsResetSync,
                  int& resetSyncFrames) 
{
    Logger::GetInstance().Info("Resetting all spheres...");
    
    const int numSpheres = static_cast<int>(entities.spheres.size());
    const float spreadRadius = 5.0f;
    const float startHeight = 12.0f;
    const float minSeparation = 2.5f;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::vector<Vector3> positions;
    
    for (int i = 0; i < numSpheres; ++i) {
        // 生成新的不重叠位置
        Vector3 newPos = GenerateNonOverlappingPosition(
            positions, minSeparation, spreadRadius, startHeight, gen
        );
        positions.push_back(newPos);
        
        auto& sphereTransform = world.GetComponent<TransformComponent>(entities.spheres[i]);
        auto& sphereRb = world.GetComponent<RigidBodyComponent>(entities.spheres[i]);
        
        // 清除物理速度
        sphereRb.linearVelocity = Vector3(0, 0, 0);
        sphereRb.angularVelocity = Vector3(0, 0, 0);
        
        // 设置新位置
        sphereTransform.SetPosition(newPos);
        
        // 切换到Transform驱动物理模式
        sphereRb.syncMode = RigidBodyComponent::SyncMode::TransformToPhysics;
        sphereRb.needsSync = true;
        
        Logger::GetInstance().Info("Sphere " + std::to_string(i) + 
            " reset to (" + 
            std::to_string(newPos.x()) + ", " + 
            std::to_string(newPos.y()) + ", " + 
            std::to_string(newPos.z()) + ")");
    }
    
    // 设置标志，在几帧后恢复物理驱动模式
    needsResetSync = true;
    resetSyncFrames = 2;
    
    Logger::GetInstance().Info("Reset initiated, will complete in 2 frames");
}

void CompleteSyncReset(World& world, 
                       const PhysicsEntities& entities,
                       bool& needsResetSync) 
{
    Logger::GetInstance().Info("Completing sync reset...");
    
    for (auto sphereId : entities.spheres) {
        auto& sphereRb = world.GetComponent<RigidBodyComponent>(sphereId);
        sphereRb.syncMode = RigidBodyComponent::SyncMode::PhysicsToTransform;
    }
    
    needsResetSync = false;
    Logger::GetInstance().Info("Reset complete - physics simulation resumed");
}

void PrintDebugInfo(World& world, const PhysicsEntities& entities) {
    Logger::GetInstance().Info("=== Debug Info ===");
    
    for (size_t i = 0; i < entities.spheres.size(); ++i) {
        auto& trans = world.GetComponent<TransformComponent>(entities.spheres[i]);
        auto& rb = world.GetComponent<RigidBodyComponent>(entities.spheres[i]);
        
        Vector3 pos = trans.GetPosition();
        Vector3 vel = rb.linearVelocity;
        
        Logger::GetInstance().Info(
            "Sphere " + std::to_string(i) + 
            " - Pos: (" + std::to_string(pos.x()) + ", " + 
            std::to_string(pos.y()) + ", " + std::to_string(pos.z()) + 
            ") - Vel: (" + std::to_string(vel.x()) + ", " + 
            std::to_string(vel.y()) + ", " + std::to_string(vel.z()) + ")"
        );
    }
    
    Logger::GetInstance().Info("==================");
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Logger::GetInstance().Info("=== Physics Engine Basic Test ===");
    Logger::GetInstance().Info("Controls:");
    Logger::GetInstance().Info("  ESC - Exit");
    Logger::GetInstance().Info("  R   - Reset all spheres");
    Logger::GetInstance().Info("  D   - Print debug info");
    Logger::GetInstance().Info("  G   - Toggle gravity");

    // ==================== 初始化渲染器 ====================
    Renderer* renderer = Renderer::Create();
    if (!renderer) {
        Logger::GetInstance().Error("Failed to create renderer instance");
        return 1;
    }

    if (!renderer->Initialize("63_physics_basic_test", 1280, 720)) {
        Logger::GetInstance().Error("Failed to initialize renderer");
        Renderer::Destroy(renderer);
        return 1;
    }

    renderer->SetVSync(true);
    renderer->SetClearColor(0.05f, 0.06f, 0.1f, 1.0f);
    
    // 禁用LOD实例化渲染，因为每个球体使用不同的材质
    // LOD实例化渲染会将相同mesh+相同material的实体分组，但这里每个球体材质不同
    // 禁用后使用传统渲染方式，确保所有球体都能正确渲染
    // renderer->SetLODInstancingEnabled(false);
    Logger::GetInstance().Info("LOD instancing disabled for physics test (each sphere has different material)");

    // ==================== 加载着色器 ====================
    auto& shaderCache = ShaderCache::GetInstance();
    auto phongShader = shaderCache.LoadShader(
        "lighting_phong", 
        "shaders/material_phong.vert", 
        "shaders/material_phong.frag"
    );
    
    if (!phongShader) {
        Logger::GetInstance().Error("Failed to load Phong shader");
        Renderer::Destroy(renderer);
        return 1;
    }

    // ==================== 创建材质 ====================
    // 地面材质（深灰色）
    auto groundMaterial = std::make_shared<Material>();
    groundMaterial->SetName("PhysicsGround");
    groundMaterial->SetShader(phongShader);
    groundMaterial->SetDiffuseColor(Color(0.25f, 0.3f, 0.35f, 1.0f));
    groundMaterial->SetAmbientColor(Color(0.15f, 0.18f, 0.2f, 1.0f));
    groundMaterial->SetSpecularColor(Color(0.05f, 0.05f, 0.05f, 1.0f));
    groundMaterial->SetShininess(6.0f);

    // 球体材质（多种颜色）
    std::vector<std::shared_ptr<Material>> sphereMaterials;
    std::vector<Color> sphereColors = {
        Color(0.85f, 0.2f, 0.2f, 1.0f),   // 红色
        Color(0.2f, 0.85f, 0.2f, 1.0f),   // 绿色
        Color(0.2f, 0.2f, 0.85f, 1.0f),   // 蓝色
        Color(0.85f, 0.85f, 0.2f, 1.0f),  // 黄色
        Color(0.85f, 0.2f, 0.85f, 1.0f),  // 紫色
        Color(0.2f, 0.85f, 0.85f, 1.0f),  // 青色
        Color(0.85f, 0.5f, 0.2f, 1.0f),   // 橙色
        Color(0.5f, 0.2f, 0.85f, 1.0f),   // 靛色
    };
    
    for (size_t i = 0; i < sphereColors.size(); ++i) {
        auto sphereMat = std::make_shared<Material>();
        sphereMat->SetName("PhysicsSphere_" + std::to_string(i));
        sphereMat->SetShader(phongShader);
        sphereMat->SetDiffuseColor(sphereColors[i]);
        sphereMat->SetAmbientColor(Color(
            sphereColors[i].r * 0.3f, 
            sphereColors[i].g * 0.3f, 
            sphereColors[i].b * 0.3f, 
            sphereColors[i].a
        ));
        sphereMat->SetSpecularColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
        sphereMat->SetShininess(48.0f);
        sphereMaterials.push_back(sphereMat);
    }

    // ==================== 创建网格 ====================
    auto groundMesh = MeshLoader::CreatePlane(30.0f, 30.0f, 10, 10, Color::White());
    auto sphereMesh = MeshLoader::CreateSphere(1.0f, 48, 24, Color::White());

    // ==================== 注册资源 ====================
    auto& resMgr = ResourceManager::GetInstance();
    RegisterResources(resMgr, groundMaterial, sphereMaterials, groundMesh, sphereMesh);

    // ==================== 初始化ECS世界 ====================
    World world;
    world.Initialize();

    // 注册组件
    world.RegisterComponent<TransformComponent>();
    world.RegisterComponent<MeshRenderComponent>();
    world.RegisterComponent<CameraComponent>();
    world.RegisterComponent<LightComponent>();
    world.RegisterComponent<ActiveComponent>();
    world.RegisterComponent<RigidBodyComponent>();
    world.RegisterComponent<ColliderComponent>();
    world.RegisterComponent<PhysicsWorldComponent>();

    // 注册系统
    world.RegisterSystem<TransformSystem>();
    world.RegisterSystem<CameraSystem>();
    world.RegisterSystem<LightSystem>(renderer);
    world.RegisterSystem<UniformSystem>(renderer);
    auto* physicsSystem = world.RegisterSystem<PhysicsSystem>();
    world.RegisterSystem<MeshRenderSystem>(renderer);
    
    world.PostInitialize();

    // ==================== 创建物理世界 ====================
    EntityID physicsWorldEntity = physicsSystem->CreatePhysicsWorld();
    if (!physicsWorldEntity.IsValid()) {
        Logger::GetInstance().Error("Failed to create physics world");
        Renderer::Destroy(renderer);
        return 1;
    }
    Logger::GetInstance().Info("Physics world created successfully");

    // ==================== 创建场景 ====================
    SetupCamera(world);
    PhysicsEntities entities = CreateScene(
        world, groundMesh, sphereMesh, groundMaterial, sphereMaterials
    );
    
    // 验证所有球体都已正确创建
    Logger::GetInstance().Info("=== Verifying all spheres ===");
    for (size_t i = 0; i < entities.spheres.size(); ++i) {
        EntityID sphereId = entities.spheres[i];
        if (!sphereId.IsValid()) {
            Logger::GetInstance().Error("Sphere " + std::to_string(i) + " has invalid EntityID!");
            continue;
        }
        
        bool hasTransform = world.HasComponent<TransformComponent>(sphereId);
        bool hasRender = world.HasComponent<MeshRenderComponent>(sphereId);
        bool hasCollider = world.HasComponent<ColliderComponent>(sphereId);
        bool hasRigidBody = world.HasComponent<RigidBodyComponent>(sphereId);
        
        if (hasTransform && hasRender) {
            auto& transform = world.GetComponent<TransformComponent>(sphereId);
            auto& render = world.GetComponent<MeshRenderComponent>(sphereId);
            Vector3 pos = transform.GetPosition();
            
            Logger::GetInstance().Info("Sphere " + std::to_string(i) + 
                " - Valid: " + std::string(sphereId.IsValid() ? "Yes" : "No") +
                " - Transform: " + std::string(hasTransform ? "Yes" : "No") +
                " - Render: " + std::string(hasRender ? "Yes" : "No") +
                " - Collider: " + std::string(hasCollider ? "Yes" : "No") +
                " - RigidBody: " + std::string(hasRigidBody ? "Yes" : "No") +
                " - Mesh: " + std::string(render.mesh != nullptr ? "OK" : "NULL") +
                " - Material: " + std::string(render.material != nullptr ? "OK" : "NULL") +
                " - Visible: " + std::string(render.visible ? "true" : "false") +
                " - ResourcesLoaded: " + std::string(render.resourcesLoaded ? "true" : "false") +
                " - Pos: (" + std::to_string(pos.x()) + ", " + std::to_string(pos.y()) + ", " + std::to_string(pos.z()) + ")");
        } else {
            Logger::GetInstance().Error("Sphere " + std::to_string(i) + 
                " missing components - Transform: " + std::string(hasTransform ? "Yes" : "No") +
                ", Render: " + std::string(hasRender ? "Yes" : "No"));
        }
    }
    Logger::GetInstance().Info("=== Verification complete ===");

    // ==================== 主循环变量 ====================
    bool running = true;
    uint64_t lastTicks = SDL_GetTicks();
    float timeAccumulator = 0.0f;
    int frameCount = 0;
    float fpsTimer = 0.0f;
    
    bool needsResetSync = false;
    int resetSyncFrames = 0;
    bool gravityEnabled = true;

    Logger::GetInstance().Info("Entering main loop...");

    // ==================== 主循环 ====================
    while (running) {
        // ========== 事件处理 ==========
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            
            if (event.type == SDL_EVENT_KEY_DOWN) {
                switch (event.key.key) {
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                        
                    case SDLK_R:
                        ResetSpheres(world, entities, needsResetSync, resetSyncFrames);
                        break;
                        
                    case SDLK_D:
                        PrintDebugInfo(world, entities);
                        break;
                        
                    case SDLK_G:
                        gravityEnabled = !gravityEnabled;
                        for (auto sphereId : entities.spheres) {
                            auto& rb = world.GetComponent<RigidBodyComponent>(sphereId);
                            rb.useGravity = gravityEnabled;
                        }
                        Logger::GetInstance().Info(
                            "Gravity " + std::string(gravityEnabled ? "enabled" : "disabled")
                        );
                        break;
                }
            }
        }

        // ========== 时间更新 ==========
        uint64_t currentTicks = SDL_GetTicks();
        float deltaTime = static_cast<float>(currentTicks - lastTicks) / 1000.0f;
        lastTicks = currentTicks;
        
        // 限制最大时间步长，防止物理爆炸
        if (deltaTime > 0.1f) {
            deltaTime = 0.1f;
        }
        
        timeAccumulator += deltaTime;
        fpsTimer += deltaTime;
        frameCount++;
        
        // 每秒打印FPS
        if (fpsTimer >= 1.0f) {
            Logger::GetInstance().Info("FPS: " + std::to_string(frameCount));
            frameCount = 0;
            fpsTimer = 0.0f;
        }

        // ========== 处理重置同步 ==========
        if (needsResetSync && resetSyncFrames > 0) {
            resetSyncFrames--;
            if (resetSyncFrames == 0) {
                CompleteSyncReset(world, entities, needsResetSync);
            }
        }

        // ========== 更新世界 ==========
        renderer->BeginFrame();
        renderer->Clear();

        world.Update(deltaTime);
        renderer->FlushRenderQueue();

        renderer->EndFrame();
        renderer->Present();
    }

    // ==================== 清理 ====================
    Logger::GetInstance().Info("Shutting down...");
    world.Shutdown();
    Renderer::Destroy(renderer);
    Logger::GetInstance().Info("=== Physics Engine Basic Test Finished ===");
    
    return 0;
}