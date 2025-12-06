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
/**
 * @file test_force_and_impulse_system.cpp
 * @brief 阶段 3.1 力与冲量系统测试
 *
 * 验证 ForceAccumulator 的累加行为，以及 PhysicsUpdateSystem
 * 对重力与冲量的处理是否符合预期。
 */

 #include "render/physics/dynamics/force_accumulator.h"
 #include "render/physics/dynamics/symplectic_euler_integrator.h"
 #include "render/physics/physics_systems.h"
 #include "render/physics/physics_components.h"
 #include "render/ecs/world.h"
 #include "render/ecs/components.h"
 #include <iostream>
 #include <cmath>
 
 using namespace Render;
 using namespace Render::Physics;
 using namespace Render::ECS;
 
 // ============================================================================
 // 简单测试框架
 // ============================================================================
 
 static int g_testCount = 0;
 static int g_passedCount = 0;
 static int g_failedCount = 0;
 
 #define TEST_ASSERT(condition, message) \
     do { \
         g_testCount++; \
         if (!(condition)) { \
             std::cerr << "❌ 测试失败: " << message << std::endl; \
             std::cerr << "   位置: " << __FILE__ << ":" << __LINE__ << std::endl; \
             g_failedCount++; \
             return false; \
         } \
         g_passedCount++; \
     } while(0)
 
 #define RUN_TEST(test_func) \
     do { \
         std::cout << "运行测试: " << #test_func << "..." << std::endl; \
         if (test_func()) { \
             std::cout << "✓ " << #test_func << " 通过" << std::endl; \
         } else { \
             std::cout << "✗ " << #test_func << " 失败" << std::endl; \
         } \
     } while(0)
 
 // ============================================================================
 // 测试辅助
 // ============================================================================
 
 void RegisterPhysicsComponents(std::shared_ptr<World> world) {
     world->RegisterComponent<TransformComponent>();
     world->RegisterComponent<RigidBodyComponent>();
     world->RegisterComponent<ColliderComponent>();
     world->RegisterComponent<ForceFieldComponent>();
 }
 
 // ============================================================================
 // ForceAccumulator 单元测试
 // ============================================================================
 
 bool Test_ForceAccumulator_AccumulationAndClear() {
     ForceAccumulator acc;
 
     acc.AddForce(Vector3(1.0f, 2.0f, 3.0f));
     acc.AddForceAtPoint(Vector3(0.0f, 1.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3::Zero());
     acc.AddTorque(Vector3(0.0f, 0.5f, 0.0f));
     acc.AddImpulse(Vector3(2.0f, 0.0f, 0.0f), 0.5f);  // Δv = (1,0,0)
     acc.AddAngularImpulse(Vector3(0.0f, 0.0f, 2.0f), Matrix3::Identity());
 
     Vector3 expectedForce = Vector3(1.0f, 3.0f, 3.0f);
     Vector3 expectedTorque = Vector3(0.0f, 0.5f, 1.0f); // r(1,0,0) x F(0,1,0) = (0,0,1)
     Vector3 expectedLinearImpulse = Vector3(1.0f, 0.0f, 0.0f);
     Vector3 expectedAngularImpulse = Vector3(0.0f, 0.0f, 2.0f);
 
     TEST_ASSERT(acc.GetTotalForce().isApprox(expectedForce, 1e-5f), "力累加结果错误");
     TEST_ASSERT(acc.GetTotalTorque().isApprox(expectedTorque, 1e-5f), "扭矩累加结果错误");
     TEST_ASSERT(acc.GetLinearImpulse().isApprox(expectedLinearImpulse, 1e-5f), "线性冲量累加结果错误");
     TEST_ASSERT(acc.GetAngularImpulse().isApprox(expectedAngularImpulse, 1e-5f), "角冲量累加结果错误");
 
     acc.Clear();
 
     TEST_ASSERT(acc.GetTotalForce().isZero(1e-6f), "清空后总力应为 0");
     TEST_ASSERT(acc.GetTotalTorque().isZero(1e-6f), "清空后总扭矩应为 0");
     TEST_ASSERT(acc.GetLinearImpulse().isZero(1e-6f), "清空后线性冲量应为 0");
     TEST_ASSERT(acc.GetAngularImpulse().isZero(1e-6f), "清空后角冲量应为 0");
 
     return true;
 }
 
// ============================================================================
// SymplecticEulerIntegrator 单元测试
// ============================================================================

bool Test_SymplecticEulerIntegrator_IntegrateVelocity_AppliesForceAndDamping() {
    SymplecticEulerIntegrator integrator;
    RigidBodyComponent body;
    TransformComponent transform;

    body.SetMass(2.0f); // inverseMass = 0.5
    body.linearDamping = 0.1f;
    body.angularDamping = 0.2f;
    body.force = Vector3(4.0f, 0.0f, 0.0f);   // a = 2 m/s²
    body.torque = Vector3(0.0f, 2.0f, 0.0f);  // α = 2 rad/s²

    const float dt = 1.0f;
    integrator.IntegrateVelocity(body, &transform, dt);

    float linearDampingFactor = std::pow(std::max(0.0f, 1.0f - body.linearDamping), dt);
    float angularDampingFactor = std::pow(std::max(0.0f, 1.0f - body.angularDamping), dt);

    Vector3 expectedLinear = Vector3(2.0f, 0.0f, 0.0f) * linearDampingFactor;
    Vector3 expectedAngular = Vector3(0.0f, 2.0f, 0.0f) * angularDampingFactor;

    TEST_ASSERT(body.linearVelocity.isApprox(expectedLinear, 1e-5f),
                "线速度积分或阻尼计算错误");
    TEST_ASSERT(body.angularVelocity.isApprox(expectedAngular, 1e-5f),
                "角速度积分或阻尼计算错误");
    TEST_ASSERT(body.force.isZero(1e-6f), "积分后力应被清零");
    TEST_ASSERT(body.torque.isZero(1e-6f), "积分后扭矩应被清零");

    return true;
}

bool Test_SymplecticEulerIntegrator_IntegrateVelocity_Constraints() {
    SymplecticEulerIntegrator integrator;
    RigidBodyComponent body;
    TransformComponent transform;

    body.SetMass(1.0f);
    body.linearDamping = 0.0f;
    body.angularDamping = 0.0f;
    body.maxLinearSpeed = 5.0f;
    body.maxAngularSpeed = 2.0f;
    body.lockPosition[1] = true;   // 锁定 Y 轴平移
    body.lockRotation[1] = true;   // 锁定 Y 轴旋转

    body.force = Vector3(10.0f, 10.0f, 0.0f); // 预期会触发锁定与限速
    body.torque = Vector3(0.0f, 5.0f, 5.0f);

    const float dt = 1.0f;
    integrator.IntegrateVelocity(body, &transform, dt);

    Vector3 expectedLinear(5.0f, 0.0f, 0.0f); // Y 轴锁定，线速度被限幅到 5
    Vector3 expectedAngular(0.0f, 0.0f, 2.0f); // Y 轴锁定后再按最大角速度截断

    TEST_ASSERT(body.linearVelocity.isApprox(expectedLinear, 1e-5f),
                "线速度锁定或限速约束失败");
    TEST_ASSERT(body.angularVelocity.isApprox(expectedAngular, 1e-5f),
                "角速度锁定或限速约束失败");

    return true;
}

bool Test_SymplecticEulerIntegrator_IntegratePosition_UpdatesTransform() {
    SymplecticEulerIntegrator integrator;
    RigidBodyComponent body;
    TransformComponent transform;

    transform.SetPosition(Vector3(1.0f, 1.0f, 1.0f));
    transform.SetRotation(Quaternion::Identity());

    body.linearVelocity = Vector3(2.0f, 3.0f, 0.0f);
    body.angularVelocity = Vector3(0.0f, 2.0f, 0.0f);
    body.lockPosition[1] = true; // 锁定 Y 轴平移

    const float dt = 0.5f;
    integrator.IntegratePosition(body, transform, dt);

    Vector3 expectedPosition(1.0f + body.linearVelocity.x() * dt, 1.0f, 1.0f);
    TEST_ASSERT(transform.GetPosition().isApprox(expectedPosition, 1e-5f),
                "位置积分或轴向锁定错误");

    float deltaAngle = body.angularVelocity.norm() * dt;
    Quaternion expectedRotation = MathUtils::AngleAxis(deltaAngle, Vector3(0.0f, 1.0f, 0.0f));
    TEST_ASSERT(transform.GetRotation().coeffs().isApprox(expectedRotation.coeffs(), 1e-5f),
                "旋转积分结果错误");

    TEST_ASSERT(body.previousPosition.isApprox(Vector3(1.0f, 1.0f, 1.0f), 1e-5f),
                "previousPosition 未正确保存");
    TEST_ASSERT(body.previousRotation.coeffs().isApprox(Quaternion::Identity().coeffs(), 1e-5f),
                "previousRotation 未正确保存");

    return true;
}

 // ============================================================================
 // PhysicsUpdateSystem 集成测试
 // ============================================================================
 
 bool Test_PhysicsUpdateSystem_AppliesGravity() {
     auto world = std::make_shared<World>();
     RegisterPhysicsComponents(world);
     world->Initialize();
 
     auto* system = world->RegisterSystem<PhysicsUpdateSystem>();
     system->SetGravity(Vector3(0.0f, -9.81f, 0.0f));
 
     EntityID entity = world->CreateEntity();
 
     TransformComponent transform;
     transform.SetPosition(Vector3::Zero());
     world->AddComponent(entity, transform);
 
     RigidBodyComponent body;
     body.SetMass(2.0f);
     body.linearDamping = 0.0f;
     body.angularDamping = 0.0f;
     body.useGravity = true;
     body.gravityScale = 1.5f;
     
     world->AddComponent(entity, body);
 
     const float dt = 1.0f / 60.0f;
     system->Update(dt);
 
     auto& updatedBody = world->GetComponent<RigidBodyComponent>(entity);
     Vector3 expectedVelocity = Vector3(0.0f, -9.81f * 1.5f * dt, 0.0f);
 
     TEST_ASSERT(updatedBody.linearVelocity.isApprox(expectedVelocity, 1e-4f),
                 "重力积分后的线速度不正确");
     TEST_ASSERT(updatedBody.force.isZero(1e-6f), "积分后力应被清零");
 
     world->Shutdown();
     return true;
 }
 
 bool Test_PhysicsUpdateSystem_ImpulseAffectsVelocityAndRotation() {
     auto world = std::make_shared<World>();
     RegisterPhysicsComponents(world);
     world->Initialize();
 
     auto* system = world->RegisterSystem<PhysicsUpdateSystem>();
 
     EntityID entity = world->CreateEntity();
 
     TransformComponent transform;
     transform.SetPosition(Vector3::Zero());
     world->AddComponent(entity, transform);
 
     RigidBodyComponent body;
     body.SetMass(2.0f);
     body.linearDamping = 0.0f;
     body.angularDamping = 0.0f;
     body.useGravity = false;
     world->AddComponent(entity, body);
 
     // 线性冲量
     system->ApplyImpulse(entity, Vector3(2.0f, 0.0f, 0.0f));
 
     // 同时施加线性与角冲量
     system->ApplyImpulseAtPoint(entity, Vector3(0.0f, 1.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f));
 
     auto& updatedBody = world->GetComponent<RigidBodyComponent>(entity);
 
     Vector3 expectedLinear = Vector3(1.0f, 0.5f, 0.0f); // (2,0,0)/2 + (0,1,0)/2
     Vector3 expectedAngular = Vector3(0.0f, 0.0f, 1.0f); // r(1,0,0) x impulse(0,1,0)
 
     TEST_ASSERT(updatedBody.linearVelocity.isApprox(expectedLinear, 1e-5f),
                 "冲量后的线速度不正确");
     TEST_ASSERT(updatedBody.angularVelocity.isApprox(expectedAngular, 1e-5f),
                 "冲量后的角速度不正确");
 
     world->Shutdown();
     return true;
 }
 
 // ============================================================================
 // ForceField 测试
 // ============================================================================
 
 bool Test_ForceField_GravityField() {
     auto world = std::make_shared<World>();
     RegisterPhysicsComponents(world);
     world->Initialize();
 
     auto* system = world->RegisterSystem<PhysicsUpdateSystem>();
     system->SetGravity(Vector3::Zero()); // 关闭全局重力
 
     // 创建重力场
     EntityID fieldEntity = world->CreateEntity();
     
     TransformComponent fieldTransform;
     fieldTransform.SetPosition(Vector3::Zero());
     world->AddComponent(fieldEntity, fieldTransform);
     
     ForceFieldComponent gravityField = ForceFieldComponent::CreateGravityField(
         Vector3(0.0f, -1.0f, 0.0f),
         20.0f,  // 强度 20 m/s²
         10.0f   // 半径 10m
     );
     world->AddComponent(fieldEntity, gravityField);
 
     // 创建测试物体（在力场范围内）
     EntityID entity = world->CreateEntity();
     
     TransformComponent transform;
     transform.SetPosition(Vector3(5.0f, 0.0f, 0.0f)); // 距离中心 5m
     world->AddComponent(entity, transform);
     
     RigidBodyComponent body;
     body.SetMass(1.0f);
     body.useGravity = false;
     body.linearDamping = 0.0f;
     world->AddComponent(entity, body);
 
     const float dt = 1.0f / 60.0f;
     system->Update(dt);
 
     auto& updatedBody = world->GetComponent<RigidBodyComponent>(entity);
     
     // 在距离 5m 处，线性衰减 = 1 - (5/10) = 0.5
     // 力 = 20 * 1.0 * 0.5 = 10 N
     // 加速度 = 10 m/s²
     // 速度变化 ≈ 10 * dt
     float expectedSpeed = 10.0f * dt * 0.5f; // 考虑衰减
     
     TEST_ASSERT(updatedBody.linearVelocity.y() < 0.0f, "物体应该向下运动");
     TEST_ASSERT(std::abs(updatedBody.linearVelocity.y()) > 1e-6f, "物体应该有明显的速度");
 
     world->Shutdown();
     return true;
 }
 
 bool Test_ForceField_RadialField() {
     auto world = std::make_shared<World>();
     RegisterPhysicsComponents(world);
     world->Initialize();
 
     auto* system = world->RegisterSystem<PhysicsUpdateSystem>();
     system->SetGravity(Vector3::Zero());
 
     // 创建径向吸引力场（黑洞）
     EntityID fieldEntity = world->CreateEntity();
     
     TransformComponent fieldTransform;
     fieldTransform.SetPosition(Vector3::Zero());
     world->AddComponent(fieldEntity, fieldTransform);
     
     ForceFieldComponent radialField = ForceFieldComponent::CreateRadialField(
         -30.0f,  // 负值表示吸引
         10.0f,   // 半径
         false    // 线性衰减
     );
     world->AddComponent(fieldEntity, radialField);
 
     // 创建测试物体
     EntityID entity = world->CreateEntity();
     
     TransformComponent transform;
     transform.SetPosition(Vector3(5.0f, 0.0f, 0.0f));
     world->AddComponent(entity, transform);
     
     RigidBodyComponent body;
     body.SetMass(1.0f);
     body.useGravity = false;
     body.linearDamping = 0.0f;
     world->AddComponent(entity, body);
 
     const float dt = 1.0f / 60.0f;
     system->Update(dt);
 
     auto& updatedBody = world->GetComponent<RigidBodyComponent>(entity);
     auto& updatedTransform = world->GetComponent<TransformComponent>(entity);
     
     // 计算力的方向：从物体指向中心
     Vector3 toCenter = Vector3::Zero() - Vector3(5.0f, 0.0f, 0.0f);
     Vector3 direction = toCenter.normalized();
     
     // 速度应该指向中心
     float dotProduct = updatedBody.linearVelocity.normalized().dot(direction);
     TEST_ASSERT(dotProduct > 0.9f, "物体应该被吸向力场中心");
 
     world->Shutdown();
     return true;
 }
 
 bool Test_ForceField_VortexField() {
     auto world = std::make_shared<World>();
     RegisterPhysicsComponents(world);
     world->Initialize();
 
     auto* system = world->RegisterSystem<PhysicsUpdateSystem>();
     system->SetGravity(Vector3::Zero());
 
     // 创建涡流场
     EntityID fieldEntity = world->CreateEntity();
     
     TransformComponent fieldTransform;
     fieldTransform.SetPosition(Vector3::Zero());
     world->AddComponent(fieldEntity, fieldTransform);
     
     ForceFieldComponent vortexField = ForceFieldComponent::CreateVortexField(
         Vector3(0.0f, 1.0f, 0.0f),  // 绕 Y 轴
         25.0f,                       // 强度
         8.0f                         // 半径
     );
     world->AddComponent(fieldEntity, vortexField);
 
     // 创建测试物体
     EntityID entity = world->CreateEntity();
     
     TransformComponent transform;
     transform.SetPosition(Vector3(5.0f, 0.0f, 0.0f));
     world->AddComponent(entity, transform);
     
     RigidBodyComponent body;
     body.SetMass(1.0f);
     body.useGravity = false;
     body.linearDamping = 0.0f;
     world->AddComponent(entity, body);
 
     const float dt = 1.0f / 60.0f;
     system->Update(dt);
 
     auto& updatedBody = world->GetComponent<RigidBodyComponent>(entity);
     
     // 涡流应该产生切向速度（垂直于径向）
     Vector3 radialDir = Vector3(5.0f, 0.0f, 0.0f).normalized();
     float radialComponent = std::abs(updatedBody.linearVelocity.dot(radialDir));
     
     // 速度应该主要在切向（Z 方向），而不是径向
     TEST_ASSERT(std::abs(updatedBody.linearVelocity.z()) > radialComponent, 
                 "涡流应该产生切向运动");
 
     world->Shutdown();
     return true;
 }
 
 bool Test_ForceField_EnableDisable() {
     auto world = std::make_shared<World>();
     RegisterPhysicsComponents(world);
     world->Initialize();
 
     auto* system = world->RegisterSystem<PhysicsUpdateSystem>();
     system->SetGravity(Vector3::Zero());
 
     // 创建力场
     EntityID fieldEntity = world->CreateEntity();
     
     TransformComponent fieldTransform;
     fieldTransform.SetPosition(Vector3::Zero());
     world->AddComponent(fieldEntity, fieldTransform);
     
     ForceFieldComponent field = ForceFieldComponent::CreateGravityField(
         Vector3(0.0f, -1.0f, 0.0f),
         20.0f,
         10.0f
     );
     field.SetEnabled(false); // 初始禁用
     world->AddComponent(fieldEntity, field);
 
     // 创建测试物体
     EntityID entity = world->CreateEntity();
     
     TransformComponent transform;
     transform.SetPosition(Vector3(5.0f, 0.0f, 0.0f));
     world->AddComponent(entity, transform);
     
     RigidBodyComponent body;
     body.SetMass(1.0f);
     body.useGravity = false;
     body.linearDamping = 0.0f;
     world->AddComponent(entity, body);
 
     const float dt = 1.0f / 60.0f;
     
     // 第一帧：力场禁用
     system->Update(dt);
     auto& updatedBody = world->GetComponent<RigidBodyComponent>(entity);
     TEST_ASSERT(updatedBody.linearVelocity.isZero(1e-6f), "禁用的力场不应产生力");
 
     // 启用力场
     auto& fieldComponent = world->GetComponent<ForceFieldComponent>(fieldEntity);
     fieldComponent.SetEnabled(true);
     
     // 第二帧：力场启用
     system->Update(dt);
     TEST_ASSERT(!updatedBody.linearVelocity.isZero(1e-6f), "启用的力场应产生力");
 
     world->Shutdown();
     return true;
 }
 
 bool Test_ForceField_OutOfRange() {
     auto world = std::make_shared<World>();
     RegisterPhysicsComponents(world);
     world->Initialize();
 
     auto* system = world->RegisterSystem<PhysicsUpdateSystem>();
     system->SetGravity(Vector3::Zero());
 
     // 创建有限范围的力场
     EntityID fieldEntity = world->CreateEntity();
     
     TransformComponent fieldTransform;
     fieldTransform.SetPosition(Vector3::Zero());
     world->AddComponent(fieldEntity, fieldTransform);
     
     ForceFieldComponent field = ForceFieldComponent::CreateGravityField(
         Vector3(0.0f, -1.0f, 0.0f),
         20.0f,
         5.0f   // 半径只有 5m
     );
     field.affectOnlyInside = true;
     world->AddComponent(fieldEntity, field);
 
     // 创建范围外的测试物体
     EntityID entity = world->CreateEntity();
     
     TransformComponent transform;
     transform.SetPosition(Vector3(10.0f, 0.0f, 0.0f)); // 距离 10m，超出范围
     world->AddComponent(entity, transform);
     
     RigidBodyComponent body;
     body.SetMass(1.0f);
     body.useGravity = false;
     body.linearDamping = 0.0f;
     world->AddComponent(entity, body);
 
     const float dt = 1.0f / 60.0f;
     system->Update(dt);
 
     auto& updatedBody = world->GetComponent<RigidBodyComponent>(entity);
     TEST_ASSERT(updatedBody.linearVelocity.isZero(1e-6f), 
                 "范围外的物体不应受到力场影响");
 
     world->Shutdown();
     return true;
 }
 
 // ============================================================================
 // 主入口
 // ============================================================================
 
 int main() {
     std::cout << "========================================" << std::endl;
     std::cout << "阶段 3.1 力与冲量系统测试" << std::endl;
     std::cout << "========================================" << std::endl;
 
     std::cout << "\n--- ForceAccumulator 测试 ---" << std::endl;
     RUN_TEST(Test_ForceAccumulator_AccumulationAndClear);
     
    std::cout << "\n--- 积分器测试 ---" << std::endl;
    RUN_TEST(Test_SymplecticEulerIntegrator_IntegrateVelocity_AppliesForceAndDamping);
    RUN_TEST(Test_SymplecticEulerIntegrator_IntegrateVelocity_Constraints);
    RUN_TEST(Test_SymplecticEulerIntegrator_IntegratePosition_UpdatesTransform);

     std::cout << "\n--- 基础物理测试 ---" << std::endl;
     RUN_TEST(Test_PhysicsUpdateSystem_AppliesGravity);
     RUN_TEST(Test_PhysicsUpdateSystem_ImpulseAffectsVelocityAndRotation);
     
     std::cout << "\n--- ForceField 测试 ---" << std::endl;
     RUN_TEST(Test_ForceField_GravityField);
     RUN_TEST(Test_ForceField_RadialField);
     RUN_TEST(Test_ForceField_VortexField);
     RUN_TEST(Test_ForceField_EnableDisable);
     RUN_TEST(Test_ForceField_OutOfRange);
 
     std::cout << "\n========================================" << std::endl;
     std::cout << "测试完成" << std::endl;
     std::cout << "========================================" << std::endl;
     std::cout << "总测试数: " << g_testCount << std::endl;
     std::cout << "通过: " << g_passedCount << " ✓" << std::endl;
     std::cout << "失败: " << g_failedCount << " ✗" << std::endl;
 
     if (g_failedCount == 0) {
         std::cout << "\n🎉 所有测试通过！" << std::endl;
         return 0;
     } else {
         std::cout << "\n❌ 有测试失败！" << std::endl;
         return 1;
     }
 }