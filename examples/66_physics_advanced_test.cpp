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

struct PhysicsTestEntities {
    EntityID ground;
    EntityID plane;
    std::vector<EntityID> capsules;
    std::vector<EntityID> cylinders;
    std::vector<EntityID> cones;
    EntityID triggerZone;
    EntityID constraintEntityA;
    EntityID constraintEntityB;
    std::vector<EntityID> raycastMarkers;
};

// 创建不同形状的物理对象
void CreateCapsule(World& world, const Vector3& pos, const Ref<Mesh>& mesh, 
                   const std::shared_ptr<Material>& mat, const std::string& name) {
    EntityID entity = world.CreateEntity({ .name = name });
    
    TransformComponent transform;
    transform.SetPosition(pos);
    transform.SetScale(Vector3(1.0f, 1.0f, 1.0f));
    world.AddComponent(entity, transform);
    
    MeshRenderComponent render;
    render.mesh = mesh;
    render.material = mat;
    render.resourcesLoaded = true;
    render.visible = true;
    world.AddComponent(entity, render);
    
    ColliderComponent collider;
    collider.SetCapsule(0.5f, 2.0f);  // 半径0.5，高度2.0
    world.AddComponent(entity, collider);
    
    RigidBodyComponent rb;
    rb.type = RigidBodyType::Dynamic;
    rb.mass = 5.0f;
    rb.friction = 0.5f;
    rb.restitution = 0.3f;
    world.AddComponent(entity, rb);
}

void CreateCylinder(World& world, const Vector3& pos, const Ref<Mesh>& mesh,
                    const std::shared_ptr<Material>& mat, const std::string& name) {
    EntityID entity = world.CreateEntity({ .name = name });
    
    TransformComponent transform;
    transform.SetPosition(pos);
    transform.SetScale(Vector3(1.0f, 1.0f, 1.0f));
    world.AddComponent(entity, transform);
    
    MeshRenderComponent render;
    render.mesh = mesh;
    render.material = mat;
    render.resourcesLoaded = true;
    render.visible = true;
    world.AddComponent(entity, render);
    
    ColliderComponent collider;
    collider.shape = ColliderShape::Cylinder;
    collider.cylinderSize = Vector3(0.5f, 1.0f, 0.5f);  // 半径0.5，高度2.0
    collider.needsUpdate = true;
    world.AddComponent(entity, collider);
    
    RigidBodyComponent rb;
    rb.type = RigidBodyType::Dynamic;
    rb.mass = 6.0f;
    rb.friction = 0.5f;
    rb.restitution = 0.4f;
    world.AddComponent(entity, rb);
}

void CreateCone(World& world, const Vector3& pos, const Ref<Mesh>& mesh,
                const std::shared_ptr<Material>& mat, const std::string& name) {
    EntityID entity = world.CreateEntity({ .name = name });
    
    TransformComponent transform;
    transform.SetPosition(pos);
    transform.SetScale(Vector3(1.0f, 1.0f, 1.0f));
    world.AddComponent(entity, transform);
    
    MeshRenderComponent render;
    render.mesh = mesh;
    render.material = mat;
    render.resourcesLoaded = true;
    render.visible = true;
    world.AddComponent(entity, render);
    
    ColliderComponent collider;
    collider.shape = ColliderShape::Cone;
    collider.cylinderSize = Vector3(0.6f, 1.5f, 0.6f);  // 半径0.6，高度1.5
    collider.needsUpdate = true;
    world.AddComponent(entity, collider);
    
    RigidBodyComponent rb;
    rb.type = RigidBodyType::Dynamic;
    rb.mass = 4.0f;
    rb.friction = 0.5f;
    rb.restitution = 0.5f;
    world.AddComponent(entity, rb);
}

void CreatePlane(World& world, const Vector3& pos, const Ref<Mesh>& mesh,
                 const std::shared_ptr<Material>& mat) {
    EntityID entity = world.CreateEntity({ .name = "Plane" });
    
    TransformComponent transform;
    transform.SetPosition(pos);
    transform.SetScale(Vector3(1.0f, 1.0f, 1.0f));
    world.AddComponent(entity, transform);
    
    MeshRenderComponent render;
    render.mesh = mesh;
    render.material = mat;
    render.resourcesLoaded = true;
    render.visible = true;
    world.AddComponent(entity, render);
    
    ColliderComponent collider;
    collider.shape = ColliderShape::Plane;
    collider.planeNormal = Vector3(0, 1, 0);  // 向上
    collider.planeConstant = 0.0f;
    collider.needsUpdate = true;
    world.AddComponent(entity, collider);
    
    RigidBodyComponent rb;
    rb.type = RigidBodyType::Static;
    rb.mass = 0.0f;
    world.AddComponent(entity, rb);
}

void CreateTriggerZone(World& world, const Vector3& pos, const Ref<Mesh>& mesh,
                       const std::shared_ptr<Material>& mat) {
    EntityID entity = world.CreateEntity({ .name = "TriggerZone" });
    
    TransformComponent transform;
    transform.SetPosition(pos);
    transform.SetScale(Vector3(1.0f, 1.0f, 1.0f));
    world.AddComponent(entity, transform);
    
    MeshRenderComponent render;
    render.mesh = mesh;
    render.material = mat;
    render.resourcesLoaded = true;
    render.visible = true;
    // 触发器使用半透明材质
    world.AddComponent(entity, render);
    
    ColliderComponent collider;
    collider.SetSphere(2.0f);
    collider.isTrigger = true;  // 设置为触发器
    world.AddComponent(entity, collider);
    
    // 触发器不需要RigidBodyComponent
}

void CreateConstraint(World& world, EntityID entityA, EntityID entityB,
                     const Vector3& pivotA, const Vector3& pivotB) {
    EntityID constraintEntity = world.CreateEntity({ .name = "Constraint" });
    
    ConstraintComponent constraint;
    constraint.type = ConstraintType::PointToPoint;
    constraint.connectedEntity = entityB;
    constraint.pivotA = pivotA;
    constraint.pivotB = pivotB;
    constraint.enabled = true;
    world.AddComponent(constraintEntity, constraint);
}

void CreateHingeConstraint(World& world, EntityID entityA, EntityID entityB,
                          const Vector3& pivotA, const Vector3& pivotB,
                          const Vector3& axisA, const Vector3& axisB) {
    EntityID constraintEntity = world.CreateEntity({ .name = "HingeConstraint" });
    
    ConstraintComponent constraint;
    constraint.type = ConstraintType::Hinge;
    constraint.connectedEntity = entityB;
    constraint.pivotA = pivotA;
    constraint.pivotB = pivotB;
    constraint.axisA = axisA;
    constraint.axisB = axisB;
    constraint.lowerLimit = -MathUtils::PI / 4.0f;  // -45度
    constraint.upperLimit = MathUtils::PI / 4.0f;   // +45度
    constraint.enabled = true;
    world.AddComponent(constraintEntity, constraint);
}

PhysicsTestEntities CreateAdvancedScene(World& world,
                                       const Ref<Mesh>& groundMesh,
                                       const Ref<Mesh>& capsuleMesh,
                                       const Ref<Mesh>& cylinderMesh,
                                       const Ref<Mesh>& coneMesh,
                                       const Ref<Mesh>& sphereMesh,
                                       const std::shared_ptr<Material>& groundMat,
                                       const std::shared_ptr<Material>& capsuleMat,
                                       const std::shared_ptr<Material>& cylinderMat,
                                       const std::shared_ptr<Material>& coneMat,
                                       const std::shared_ptr<Material>& triggerMat) {
    PhysicsTestEntities entities{};
    
    // ==================== 创建地面 ====================
    Logger::GetInstance().Info("Creating ground...");
    EntityID ground = world.CreateEntity({ .name = "Ground" });
    
    TransformComponent groundTransform;
    groundTransform.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
    groundTransform.SetScale(Vector3(1.0f, 1.0f, 1.0f));
    world.AddComponent(ground, groundTransform);
    
    MeshRenderComponent groundRender;
    groundRender.mesh = groundMesh;
    groundRender.material = groundMat;
    groundRender.resourcesLoaded = true;
    groundRender.visible = true;
    world.AddComponent(ground, groundRender);
    
    ColliderComponent groundCollider;
    groundCollider.SetBox(Vector3(30.0f, 1.0f, 30.0f));
    // 将碰撞体向下偏移0.5，使碰撞体上表面对齐到Y=0（与平面网格上表面对齐）
    groundCollider.offset = Vector3(0.0f, -0.5f, 0.0f);
    world.AddComponent(ground, groundCollider);
    
    RigidBodyComponent groundRb;
    groundRb.type = RigidBodyType::Static;
    groundRb.mass = 0.0f;
    world.AddComponent(ground, groundRb);
    
    entities.ground = ground;
    
    // ==================== 创建平面碰撞体 ====================
    Logger::GetInstance().Info("Creating plane collider...");
    auto planeMesh = MeshLoader::CreatePlane(10.0f, 10.0f, 1, 1, Color(0.8f, 0.8f, 0.9f, 1.0f));
    CreatePlane(world, Vector3(10.0f, 5.0f, 0.0f), planeMesh, groundMat);
    entities.plane = ground;  // 简化，使用ground作为标识
    
    // ==================== 创建胶囊体 ====================
    Logger::GetInstance().Info("Creating capsules...");
    for (int i = 0; i < 3; ++i) {
        Vector3 pos(-5.0f + i * 3.0f, 8.0f, -3.0f);
        EntityID capsule = world.CreateEntity({ .name = "Capsule_" + std::to_string(i) });
        
        TransformComponent transform;
        transform.SetPosition(pos);
        world.AddComponent(capsule, transform);
        
        MeshRenderComponent render;
        render.mesh = capsuleMesh;
        render.material = capsuleMat;
        render.resourcesLoaded = true;
        render.visible = true;
        world.AddComponent(capsule, render);
        
        ColliderComponent collider;
        collider.SetCapsule(0.5f, 2.0f);
        world.AddComponent(capsule, collider);
        
        RigidBodyComponent rb;
        rb.type = RigidBodyType::Dynamic;
        rb.mass = 5.0f;
        rb.friction = 0.5f;
        rb.restitution = 0.3f;
        world.AddComponent(capsule, rb);
        
        entities.capsules.push_back(capsule);
    }
    
    // ==================== 创建圆柱体 ====================
    Logger::GetInstance().Info("Creating cylinders...");
    for (int i = 0; i < 3; ++i) {
        Vector3 pos(-5.0f + i * 3.0f, 8.0f, 0.0f);
        EntityID cylinder = world.CreateEntity({ .name = "Cylinder_" + std::to_string(i) });
        
        TransformComponent transform;
        transform.SetPosition(pos);
        world.AddComponent(cylinder, transform);
        
        MeshRenderComponent render;
        render.mesh = cylinderMesh;
        render.material = cylinderMat;
        render.resourcesLoaded = true;
        render.visible = true;
        world.AddComponent(cylinder, render);
        
        ColliderComponent collider;
        collider.shape = ColliderShape::Cylinder;
        collider.cylinderSize = Vector3(0.5f, 1.0f, 0.5f);
        collider.needsUpdate = true;
        world.AddComponent(cylinder, collider);
        
        RigidBodyComponent rb;
        rb.type = RigidBodyType::Dynamic;
        rb.mass = 6.0f;
        rb.friction = 0.5f;
        rb.restitution = 0.4f;
        world.AddComponent(cylinder, rb);
        
        entities.cylinders.push_back(cylinder);
    }
    
    // ==================== 创建圆锥体 ====================
    Logger::GetInstance().Info("Creating cones...");
    for (int i = 0; i < 3; ++i) {
        Vector3 pos(-5.0f + i * 3.0f, 8.0f, 3.0f);
        EntityID cone = world.CreateEntity({ .name = "Cone_" + std::to_string(i) });
        
        TransformComponent transform;
        transform.SetPosition(pos);
        world.AddComponent(cone, transform);
        
        MeshRenderComponent render;
        render.mesh = coneMesh;
        render.material = coneMat;
        render.resourcesLoaded = true;
        render.visible = true;
        world.AddComponent(cone, render);
        
        ColliderComponent collider;
        collider.shape = ColliderShape::Cone;
        collider.cylinderSize = Vector3(0.6f, 1.5f, 0.6f);
        collider.needsUpdate = true;
        world.AddComponent(cone, collider);
        
        RigidBodyComponent rb;
        rb.type = RigidBodyType::Dynamic;
        rb.mass = 4.0f;
        rb.friction = 0.5f;
        rb.restitution = 0.5f;
        world.AddComponent(cone, rb);
        
        entities.cones.push_back(cone);
    }
    
    // ==================== 创建触发器区域 ====================
    Logger::GetInstance().Info("Creating trigger zone...");
    EntityID trigger = world.CreateEntity({ .name = "TriggerZone" });
    
    TransformComponent triggerTransform;
    triggerTransform.SetPosition(Vector3(0.0f, 3.0f, 0.0f));
    world.AddComponent(trigger, triggerTransform);
    
    MeshRenderComponent triggerRender;
    triggerRender.mesh = sphereMesh;
    triggerRender.material = triggerMat;
    triggerRender.resourcesLoaded = true;
    triggerRender.visible = true;
    world.AddComponent(trigger, triggerRender);
    
    ColliderComponent triggerCollider;
    triggerCollider.SetSphere(2.0f);
    triggerCollider.isTrigger = true;
    world.AddComponent(trigger, triggerCollider);
    
    entities.triggerZone = trigger;
    
    // ==================== 创建约束示例 ====================
    Logger::GetInstance().Info("Creating constraints...");
    
    // 创建两个用于约束的球体
    EntityID constraintA = world.CreateEntity({ .name = "ConstraintBodyA" });
    TransformComponent transformA;
    transformA.SetPosition(Vector3(5.0f, 10.0f, 5.0f));
    world.AddComponent(constraintA, transformA);
    
    MeshRenderComponent renderA;
    renderA.mesh = sphereMesh;
    renderA.material = capsuleMat;
    renderA.resourcesLoaded = true;
    renderA.visible = true;
    world.AddComponent(constraintA, renderA);
    
    ColliderComponent colliderA;
    colliderA.SetSphere(0.5f);
    world.AddComponent(constraintA, colliderA);
    
    RigidBodyComponent rbA;
    rbA.type = RigidBodyType::Dynamic;
    rbA.mass = 2.0f;
    world.AddComponent(constraintA, rbA);
    
    EntityID constraintB = world.CreateEntity({ .name = "ConstraintBodyB" });
    TransformComponent transformB;
    transformB.SetPosition(Vector3(7.0f, 10.0f, 5.0f));
    world.AddComponent(constraintB, transformB);
    
    MeshRenderComponent renderB;
    renderB.mesh = sphereMesh;
    renderB.material = cylinderMat;
    renderB.resourcesLoaded = true;
    renderB.visible = true;
    world.AddComponent(constraintB, renderB);
    
    ColliderComponent colliderB;
    colliderB.SetSphere(0.5f);
    world.AddComponent(constraintB, colliderB);
    
    RigidBodyComponent rbB;
    rbB.type = RigidBodyType::Dynamic;
    rbB.mass = 2.0f;
    world.AddComponent(constraintB, rbB);
    
    // 创建点对点约束
    // 约束组件添加到第一个实体（constraintA）上，连接到第二个实体（constraintB）
    ConstraintComponent constraint;
    constraint.type = ConstraintType::PointToPoint;
    constraint.connectedEntity = constraintB;
    constraint.pivotA = Vector3(0, 0, 0);
    constraint.pivotB = Vector3(0, 0, 0);
    constraint.enabled = true;
    world.AddComponent(constraintA, constraint);  // 将约束添加到第一个实体上
    
    entities.constraintEntityA = constraintA;
    entities.constraintEntityB = constraintB;
    
    Logger::GetInstance().Info("Advanced physics scene created successfully");
    
    return entities;
}

void SetupCamera(World& world) {
    EntityID cameraEntity = world.CreateEntity({ .name = "MainCamera" });
    
    TransformComponent cameraTransform;
    cameraTransform.SetPosition(Vector3(0.0f, 12.0f, 25.0f));
    cameraTransform.transform->LookAt(Vector3(0.0f, 5.0f, 0.0f));
    world.AddComponent(cameraEntity, cameraTransform);
    
    CameraComponent cameraComp;
    cameraComp.camera = std::make_shared<Camera>();
    cameraComp.camera->SetPerspective(75.0f, 16.0f / 9.0f, 0.1f, 200.0f);
    cameraComp.active = true;
    world.AddComponent(cameraEntity, cameraComp);
}

void PerformRaycast(World& world, PhysicsSystem* physicsSystem, 
                   const Vector3& start, const Vector3& end) {
    auto hits = physicsSystem->Raycast(start, end);
    
    Logger::GetInstance().Info("=== Raycast Results ===");
    Logger::GetInstance().Info("Start: (" + 
        std::to_string(start.x()) + ", " + 
        std::to_string(start.y()) + ", " + 
        std::to_string(start.z()) + ")");
    Logger::GetInstance().Info("End: (" + 
        std::to_string(end.x()) + ", " + 
        std::to_string(end.y()) + ", " + 
        std::to_string(end.z()) + ")");
    Logger::GetInstance().Info("Hits: " + std::to_string(hits.size()));
    
    for (size_t i = 0; i < hits.size(); ++i) {
        const auto& hit = hits[i];
        Logger::GetInstance().Info("Hit " + std::to_string(i) + 
            " - Entity: " + std::to_string(hit.entity.index) +
            ", Point: (" + 
            std::to_string(hit.point.x()) + ", " + 
            std::to_string(hit.point.y()) + ", " + 
            std::to_string(hit.point.z()) + ")" +
            ", Distance: " + std::to_string(hit.distance));
    }
    Logger::GetInstance().Info("========================");
}

void PerformSphereCast(World& world, PhysicsSystem* physicsSystem,
                       const Vector3& center, float radius) {
    auto entities = physicsSystem->SphereCast(center, radius);
    
    Logger::GetInstance().Info("=== SphereCast Results ===");
    Logger::GetInstance().Info("Center: (" + 
        std::to_string(center.x()) + ", " + 
        std::to_string(center.y()) + ", " + 
        std::to_string(center.z()) + ")");
    Logger::GetInstance().Info("Radius: " + std::to_string(radius));
    Logger::GetInstance().Info("Entities found: " + std::to_string(entities.size()));
    
    for (size_t i = 0; i < entities.size(); ++i) {
        Logger::GetInstance().Info("Entity " + std::to_string(i) + 
            ": " + std::to_string(entities[i].index));
    }
    Logger::GetInstance().Info("===========================");
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    Logger::GetInstance().Info("=== Physics Engine Advanced Test (Phase 2) ===");
    Logger::GetInstance().Info("Features:");
    Logger::GetInstance().Info("  - Extended Collider Shapes (Capsule, Cylinder, Cone, Plane)");
    Logger::GetInstance().Info("  - Constraint System (PointToPoint, Hinge, Generic6Dof)");
    Logger::GetInstance().Info("  - Collision Callbacks (OnCollisionEnter/Exit)");
    Logger::GetInstance().Info("  - Trigger System (OnTriggerEnter/Exit)");
    Logger::GetInstance().Info("  - Raycast and SphereCast Queries");
    Logger::GetInstance().Info("Controls:");
    Logger::GetInstance().Info("  ESC - Exit");
    Logger::GetInstance().Info("  R   - Perform Raycast");
    Logger::GetInstance().Info("  S   - Perform SphereCast");
    Logger::GetInstance().Info("  D   - Print Physics Stats");
    
    // ==================== 初始化渲染器 ====================
    Renderer* renderer = Renderer::Create();
    if (!renderer) {
        Logger::GetInstance().Error("Failed to create renderer");
        return 1;
    }
    
    if (!renderer->Initialize("64_physics_advanced_test", 1280, 720)) {
        Logger::GetInstance().Error("Failed to initialize renderer");
        Renderer::Destroy(renderer);
        return 1;
    }
    
    renderer->SetVSync(true);
    renderer->SetClearColor(0.05f, 0.06f, 0.1f, 1.0f);
    
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
    auto groundMat = std::make_shared<Material>();
    groundMat->SetName("Ground");
    groundMat->SetShader(phongShader);
    groundMat->SetDiffuseColor(Color(0.3f, 0.35f, 0.4f, 1.0f));
    groundMat->SetAmbientColor(Color(0.15f, 0.18f, 0.2f, 1.0f));
    groundMat->SetSpecularColor(Color(0.1f, 0.1f, 0.1f, 1.0f));
    groundMat->SetShininess(8.0f);
    
    auto capsuleMat = std::make_shared<Material>();
    capsuleMat->SetName("Capsule");
    capsuleMat->SetShader(phongShader);
    capsuleMat->SetDiffuseColor(Color(0.85f, 0.2f, 0.2f, 1.0f));
    capsuleMat->SetAmbientColor(Color(0.25f, 0.06f, 0.06f, 1.0f));
    capsuleMat->SetSpecularColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
    capsuleMat->SetShininess(64.0f);
    
    auto cylinderMat = std::make_shared<Material>();
    cylinderMat->SetName("Cylinder");
    cylinderMat->SetShader(phongShader);
    cylinderMat->SetDiffuseColor(Color(0.2f, 0.85f, 0.2f, 1.0f));
    cylinderMat->SetAmbientColor(Color(0.06f, 0.25f, 0.06f, 1.0f));
    cylinderMat->SetSpecularColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
    cylinderMat->SetShininess(64.0f);
    
    auto coneMat = std::make_shared<Material>();
    coneMat->SetName("Cone");
    coneMat->SetShader(phongShader);
    coneMat->SetDiffuseColor(Color(0.2f, 0.2f, 0.85f, 1.0f));
    coneMat->SetAmbientColor(Color(0.06f, 0.06f, 0.25f, 1.0f));
    coneMat->SetSpecularColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
    coneMat->SetShininess(64.0f);
    
    auto triggerMat = std::make_shared<Material>();
    triggerMat->SetName("Trigger");
    triggerMat->SetShader(phongShader);
    triggerMat->SetDiffuseColor(Color(0.85f, 0.85f, 0.2f, 0.3f));  // 半透明
    triggerMat->SetAmbientColor(Color(0.25f, 0.25f, 0.06f, 0.3f));
    triggerMat->SetSpecularColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
    triggerMat->SetShininess(32.0f);
    
    // ==================== 创建网格 ====================
    auto groundMesh = MeshLoader::CreatePlane(30.0f, 30.0f, 10, 10, Color::White());
    auto capsuleMesh = MeshLoader::CreateCapsule(0.5f, 2.0f, 16, 8, Color::White());
    auto cylinderMesh = MeshLoader::CreateCylinder(0.5f, 0.5f, 2.0f, 16, Color::White());  // radiusTop, radiusBottom, height, segments
    auto coneMesh = MeshLoader::CreateCone(0.6f, 1.5f, 16, Color::White());
    auto sphereMesh = MeshLoader::CreateSphere(2.0f, 32, 16, Color::White());
    
    // ==================== 注册资源 ====================
    auto& resMgr = ResourceManager::GetInstance();
    resMgr.RegisterMaterial("ground_mat", groundMat);
    resMgr.RegisterMaterial("capsule_mat", capsuleMat);
    resMgr.RegisterMaterial("cylinder_mat", cylinderMat);
    resMgr.RegisterMaterial("cone_mat", coneMat);
    resMgr.RegisterMaterial("trigger_mat", triggerMat);
    resMgr.RegisterMesh("ground_mesh", groundMesh);
    resMgr.RegisterMesh("capsule_mesh", capsuleMesh);
    resMgr.RegisterMesh("cylinder_mesh", cylinderMesh);
    resMgr.RegisterMesh("cone_mesh", coneMesh);
    resMgr.RegisterMesh("sphere_mesh", sphereMesh);
    
    // ==================== 初始化ECS世界 ====================
    World world;
    world.Initialize();
    
    world.RegisterComponent<TransformComponent>();
    world.RegisterComponent<MeshRenderComponent>();
    world.RegisterComponent<CameraComponent>();
    world.RegisterComponent<LightComponent>();
    world.RegisterComponent<RigidBodyComponent>();
    world.RegisterComponent<ColliderComponent>();
    world.RegisterComponent<ConstraintComponent>();
    world.RegisterComponent<PhysicsWorldComponent>();
    
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
    
    // ==================== 创建场景 ====================
    SetupCamera(world);
    
    // 添加方向光
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
    world.AddComponent(sun, sunLight);
    
    PhysicsTestEntities entities = CreateAdvancedScene(
        world, groundMesh, capsuleMesh, cylinderMesh, coneMesh, sphereMesh,
        groundMat, capsuleMat, cylinderMat, coneMat, triggerMat
    );
    
    // ==================== 主循环 ====================
    bool running = true;
    uint64_t lastTicks = SDL_GetTicks();
    float timeAccumulator = 0.0f;
    int frameCount = 0;
    float fpsTimer = 0.0f;
    
    Logger::GetInstance().Info("Entering main loop...");
    
    while (running) {
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
                        // 执行射线检测
                        PerformRaycast(world, physicsSystem, 
                                      Vector3(0.0f, 15.0f, 0.0f),
                                      Vector3(0.0f, -15.0f, 0.0f));
                        break;
                        
                    case SDLK_S:
                        // 执行球形检测
                        PerformSphereCast(world, physicsSystem,
                                         Vector3(0.0f, 5.0f, 0.0f),
                                         5.0f);
                        break;
                        
                    case SDLK_D:
                        // 打印物理统计信息
                        {
                            const auto& stats = physicsSystem->GetStats();
                            Logger::GetInstance().Info("=== Physics Stats ===");
                            Logger::GetInstance().Info("RigidBodies: " + std::to_string(stats.rigidBodyCount));
                            Logger::GetInstance().Info("Colliders: " + std::to_string(stats.colliderCount));
                            Logger::GetInstance().Info("Constraints: " + std::to_string(stats.constraintCount));
                            Logger::GetInstance().Info("Simulation Time: " + std::to_string(stats.simulationTime) + " ms");
                            Logger::GetInstance().Info("Step Count: " + std::to_string(stats.stepCount));
                            Logger::GetInstance().Info("====================");
                        }
                        break;
                }
            }
        }
        
        uint64_t currentTicks = SDL_GetTicks();
        float deltaTime = static_cast<float>(currentTicks - lastTicks) / 1000.0f;
        lastTicks = currentTicks;
        
        if (deltaTime > 0.1f) {
            deltaTime = 0.1f;
        }
        
        timeAccumulator += deltaTime;
        fpsTimer += deltaTime;
        frameCount++;
        
        if (fpsTimer >= 1.0f) {
            Logger::GetInstance().Info("FPS: " + std::to_string(frameCount));
            frameCount = 0;
            fpsTimer = 0.0f;
        }
        
        renderer->BeginFrame();
        renderer->Clear();
        
        world.Update(deltaTime);
        renderer->FlushRenderQueue();
        
        renderer->EndFrame();
        renderer->Present();
    }
    
    Logger::GetInstance().Info("Shutting down...");
    world.Shutdown();
    Renderer::Destroy(renderer);
    Logger::GetInstance().Info("=== Physics Engine Advanced Test Finished ===");
    
    return 0;
}
