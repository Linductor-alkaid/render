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
 * @file test_physics_update_system_interpolation.cpp
 * @brief 阶段 3.3 物理更新系统插值与固定步长测试
 *
 * 验证：
 * 1) 固定时间步长稳定性与累计行为；
 * 2) 渲染帧率变化不影响物理解算结果；
 * 3) 插值在高帧率下使运动平滑；
 * 4) 角速度积分与旋转积分正确性；
 * 5) 线性阻尼与角阻尼效果；
 * 6) 位置锁定与旋转锁定约束；
 * 7) 最大速度限制约束。
 */

#include "render/physics/physics_systems.h"
#include "render/physics/physics_components.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include <iostream>
#include <vector>

using namespace Render;
using namespace Render::Physics;
using namespace Render::ECS;

// ============================================================================
// 简易测试框架
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

static void RegisterPhysicsComponents(std::shared_ptr<World> world) {
    world->RegisterComponent<TransformComponent>();
    world->RegisterComponent<RigidBodyComponent>();
    world->RegisterComponent<ColliderComponent>();
    world->RegisterComponent<ForceFieldComponent>();
}

// 为给定时间序列执行 Update，返回指定实体的线速度
static Vector3 SimulateVelocities(const std::vector<float>& deltaTimes, const Vector3& gravity) {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    auto* system = world->RegisterSystem<PhysicsUpdateSystem>();
    system->SetGravity(gravity);
    system->SetFixedDeltaTime(1.0f / 60.0f);

    EntityID entity = world->CreateEntity();

    TransformComponent transform;
    transform.SetPosition(Vector3::Zero());
    transform.SetRotation(Quaternion::Identity());
    world->AddComponent(entity, transform);

    RigidBodyComponent body;
    body.SetMass(1.0f);
    body.linearDamping = 0.0f;
    body.angularDamping = 0.0f;
    body.useGravity = true;
    world->AddComponent(entity, body);

    for (float dt : deltaTimes) {
        system->Update(dt);
    }

    Vector3 velocity = world->GetComponent<RigidBodyComponent>(entity).linearVelocity;
    world->Shutdown();
    return velocity;
}

// ============================================================================
// 测试用例
// ============================================================================

// 验证固定步长累计：两次半步应等价于一次完整固定步长
static bool Test_FixedStep_AccumulatorConsistency() {
    const float fixedDt = 1.0f / 60.0f;
    Vector3 gravity(0.0f, -9.81f, 0.0f);

    Vector3 velocityTwoHalfSteps = SimulateVelocities({ fixedDt * 0.5f, fixedDt * 0.5f }, gravity);
    Vector3 velocityOneFullStep = SimulateVelocities({ fixedDt }, gravity);

    TEST_ASSERT(velocityTwoHalfSteps.isApprox(velocityOneFullStep, 1e-6f),
                "两次半步的物理解算结果应与一次完整步相同（固定步长稳定性）");
    return true;
}

// 验证帧率变化（小步数 vs 单步）不改变物理解算结果
static bool Test_FrameRateIndependence_TotalTimeMatches() {
    const float fixedDt = 1.0f / 60.0f;
    Vector3 gravity(0.0f, -9.81f, 0.0f);

    // 将 1/30 秒拆成 4 个较小步长与 2 个较小步长，对比一次 1/30
    float totalDt = 1.0f / 30.0f;
    std::vector<float> smallSteps(4, totalDt / 4.0f);
    std::vector<float> mediumSteps(2, totalDt / 2.0f);

    Vector3 vSmall = SimulateVelocities(smallSteps, gravity);
    Vector3 vMedium = SimulateVelocities(mediumSteps, gravity);
    Vector3 vSingle = SimulateVelocities({ totalDt }, gravity);

    TEST_ASSERT(vSmall.isApprox(vMedium, 1e-6f), "多步积分应一致（渲染帧率变化不影响物理）");
    TEST_ASSERT(vSmall.isApprox(vSingle, 1e-6f), "累计时间一致时，物理解算结果应一致");
    return true;
}

// 验证插值在未触发新固定步时产生平滑中间态
static bool Test_RenderInterpolation_SmoothMotion() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    auto* system = world->RegisterSystem<PhysicsUpdateSystem>();
    const float fixedDt = 1.0f / 60.0f;
    system->SetFixedDeltaTime(fixedDt);
    system->SetGravity(Vector3::Zero()); // 仅测试线性速度插值

    EntityID entity = world->CreateEntity();

    TransformComponent transform;
    transform.SetPosition(Vector3::Zero());
    transform.SetRotation(Quaternion::Identity());
    world->AddComponent(entity, transform);

    RigidBodyComponent body;
    body.SetMass(1.0f);
    body.linearDamping = 0.0f;
    body.angularDamping = 0.0f;
    body.useGravity = false;
    body.linearVelocity = Vector3(2.0f, 0.0f, 0.0f); // 预设速度，便于预期
    world->AddComponent(entity, body);

    // 第一次 Update：执行一次固定步，位置前进 2*dt，previousPosition 记录为 0
    system->Update(fixedDt);

    // 第二次 Update：只积累 0.25 固定步，不触发 FixedUpdate，只做插值
    system->Update(fixedDt * 0.25f);

    auto& interpTransform = world->GetComponent<TransformComponent>(entity);
    Vector3 expectedPos = Vector3(2.0f * fixedDt * 0.25f, 0.0f, 0.0f); // 0 -> 2*dt 的 25%

    TEST_ASSERT(interpTransform.GetPosition().isApprox(expectedPos, 1e-6f),
                "插值位置应位于上一帧与当前物理解算结果之间（t=0.25）");

    world->Shutdown();
    return true;
}

// 验证完整物理更新流程：重力 -> 速度积分 -> 位置积分 -> AABB 更新
static bool Test_PhysicsUpdate_Flow_GravityAndAABB() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    auto* system = world->RegisterSystem<PhysicsUpdateSystem>();
    const float fixedDt = 1.0f / 60.0f;
    system->SetFixedDeltaTime(fixedDt);
    system->SetGravity(Vector3(0.0f, -10.0f, 0.0f));

    EntityID entity = world->CreateEntity();

    TransformComponent transform;
    transform.SetPosition(Vector3(0.0f, 1.0f, 0.0f));
    transform.SetRotation(Quaternion::Identity());
    world->AddComponent(entity, transform);

    RigidBodyComponent body;
    body.SetMass(2.0f);               // 质量 2kg，重力加速度期望为 -10m/s²
    body.linearDamping = 0.0f;
    body.angularDamping = 0.0f;
    body.useGravity = true;
    world->AddComponent(entity, body);

    ColliderComponent collider = ColliderComponent::CreateBox(Vector3(0.5f, 0.5f, 0.5f));
    world->AddComponent(entity, collider);

    system->Update(fixedDt);

    auto& updatedBody = world->GetComponent<RigidBodyComponent>(entity);
    auto& updatedTransform = world->GetComponent<TransformComponent>(entity);
    auto& updatedCollider = world->GetComponent<ColliderComponent>(entity);

    Vector3 expectedVelocity(0.0f, -10.0f * fixedDt, 0.0f);
    TEST_ASSERT(updatedBody.linearVelocity.isApprox(expectedVelocity, 1e-6f),
                "速度积分应包含重力影响");

    Vector3 expectedPosition(0.0f, 1.0f + expectedVelocity.y() * fixedDt, 0.0f);
    TEST_ASSERT(updatedTransform.GetPosition().isApprox(expectedPosition, 1e-6f),
                "位置积分结果不符合期望");

    TEST_ASSERT(updatedBody.previousPosition.isApprox(Vector3(0.0f, 1.0f, 0.0f), 1e-6f),
                "previousPosition 应记录积分前的位置");

    Vector3 halfExtents(0.5f, 0.5f, 0.5f);
    Vector3 expectedMin = expectedPosition - halfExtents;
    Vector3 expectedMax = expectedPosition + halfExtents;
    TEST_ASSERT(updatedCollider.worldAABB.min.isApprox(expectedMin, 1e-6f) &&
                updatedCollider.worldAABB.max.isApprox(expectedMax, 1e-6f),
                "AABB 未随物理解算结果更新");

    TEST_ASSERT(updatedBody.force.isZero(1e-6f) && updatedBody.torque.isZero(1e-6f),
                "积分后应清空力与扭矩以便下一帧重新累积");

    world->Shutdown();
    return true;
}

// 验证渲染场景下（帧时间大于固定步）插值与 AABB 的一致性
static bool Test_RenderScenario_FrameDropInterpolationAndAABB() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    auto* system = world->RegisterSystem<PhysicsUpdateSystem>();
    const float fixedDt = 1.0f / 60.0f;
    system->SetFixedDeltaTime(fixedDt);
    system->SetGravity(Vector3::Zero()); // 只考察已有速度的插值

    EntityID entity = world->CreateEntity();

    TransformComponent transform;
    transform.SetPosition(Vector3::Zero());
    transform.SetRotation(Quaternion::Identity());
    world->AddComponent(entity, transform);

    RigidBodyComponent body;
    body.SetMass(1.0f);
    body.linearDamping = 0.0f;
    body.angularDamping = 0.0f;
    body.useGravity = false;
    body.linearVelocity = Vector3(3.0f, 0.0f, 0.0f); // 固定速度，便于预期
    world->AddComponent(entity, body);

    ColliderComponent collider = ColliderComponent::CreateBox(Vector3(0.5f, 0.5f, 0.5f));
    world->AddComponent(entity, collider);

    // 单帧 1.5 个固定步：应执行 1 次 FixedUpdate，余量 0.5 用于插值
    system->Update(fixedDt * 1.5f);

    auto& updatedBody = world->GetComponent<RigidBodyComponent>(entity);
    auto& updatedTransform = world->GetComponent<TransformComponent>(entity);
    auto& updatedCollider = world->GetComponent<ColliderComponent>(entity);

    Vector3 physicsPosition = Vector3(3.0f * fixedDt, 0.0f, 0.0f);          // 物理解算结果
    Vector3 renderInterpolated = physicsPosition * 0.5f;                    // alpha=0.5 插值
    TEST_ASSERT(updatedTransform.GetPosition().isApprox(renderInterpolated, 1e-6f),
                "渲染插值位置应基于上一物理解算状态与当前状态的 50%");

    TEST_ASSERT(updatedBody.linearVelocity.isApprox(Vector3(3.0f, 0.0f, 0.0f), 1e-6f),
                "线速度在无外力情况下应保持不变");

    Vector3 expectedMin = physicsPosition - Vector3(0.5f, 0.5f, 0.5f);
    Vector3 expectedMax = physicsPosition + Vector3(0.5f, 0.5f, 0.5f);
    TEST_ASSERT(updatedCollider.worldAABB.min.isApprox(expectedMin, 1e-6f) &&
                updatedCollider.worldAABB.max.isApprox(expectedMax, 1e-6f),
                "AABB 应对应物理解算后的真实位置，即使渲染插值在中间态");

    world->Shutdown();
    return true;
}

// 验证角速度积分：旋转应正确积分
static bool Test_AngularVelocity_Integration() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    auto* system = world->RegisterSystem<PhysicsUpdateSystem>();
    const float fixedDt = 1.0f / 60.0f;
    system->SetFixedDeltaTime(fixedDt);
    system->SetGravity(Vector3::Zero());

    EntityID entity = world->CreateEntity();

    TransformComponent transform;
    transform.SetPosition(Vector3::Zero());
    transform.SetRotation(Quaternion::Identity());
    world->AddComponent(entity, transform);

    RigidBodyComponent body;
    body.SetMass(1.0f);
    body.linearDamping = 0.0f;
    body.angularDamping = 0.0f;
    body.useGravity = false;
    // 设置绕 Y 轴旋转的角速度：1 rad/s
    body.angularVelocity = Vector3(0.0f, 1.0f, 0.0f);
    // 设置单位惯性张量（简化计算）
    body.inverseInertiaTensor = Matrix3::Identity();
    world->AddComponent(entity, body);

    // 执行一次固定步更新
    system->Update(fixedDt);

    auto& updatedBody = world->GetComponent<RigidBodyComponent>(entity);
    auto& updatedTransform = world->GetComponent<TransformComponent>(entity);

    // 验证角速度保持不变（无扭矩）
    TEST_ASSERT(updatedBody.angularVelocity.isApprox(Vector3(0.0f, 1.0f, 0.0f), 1e-6f),
                "无扭矩时角速度应保持不变");

    // 验证旋转已更新（不应是单位四元数）
    Quaternion rotation = updatedTransform.GetRotation();
    TEST_ASSERT(!rotation.isApprox(Quaternion::Identity(), 1e-6f),
                "角速度积分应导致旋转变化");

    // 验证 previousRotation 已记录
    TEST_ASSERT(updatedBody.previousRotation.isApprox(Quaternion::Identity(), 1e-6f),
                "previousRotation 应记录积分前的旋转");

    // 验证旋转角度：绕 Y 轴旋转 fixedDt 弧度
    // 从单位四元数旋转到新四元数，角度应为 fixedDt
    float angle = 2.0f * std::acos(std::max(-1.0f, std::min(1.0f, rotation.w())));
    TEST_ASSERT(std::abs(angle - fixedDt) < 1e-4f,
                "旋转角度应等于角速度乘以时间步长");

    world->Shutdown();
    return true;
}

// 验证角加速度积分：扭矩应产生角加速度
static bool Test_AngularAcceleration_Integration() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    auto* system = world->RegisterSystem<PhysicsUpdateSystem>();
    const float fixedDt = 1.0f / 60.0f;
    system->SetFixedDeltaTime(fixedDt);
    system->SetGravity(Vector3::Zero());

    EntityID entity = world->CreateEntity();

    TransformComponent transform;
    transform.SetPosition(Vector3::Zero());
    transform.SetRotation(Quaternion::Identity());
    world->AddComponent(entity, transform);

    RigidBodyComponent body;
    body.SetMass(1.0f);
    body.linearDamping = 0.0f;
    body.angularDamping = 0.0f;
    body.useGravity = false;
    body.angularVelocity = Vector3::Zero();
    // 设置单位惯性张量（简化计算：I^-1 = I）
    body.inverseInertiaTensor = Matrix3::Identity();
    // 施加绕 Y 轴的扭矩：1 N·m
    body.torque = Vector3(0.0f, 1.0f, 0.0f);
    world->AddComponent(entity, body);

    // 执行一次固定步更新
    system->Update(fixedDt);

    auto& updatedBody = world->GetComponent<RigidBodyComponent>(entity);

    // 验证角速度已更新：ω = ω0 + α * dt，α = I^-1 * τ
    // 对于单位惯性张量：α = τ，所以 ω = 0 + 1 * dt = dt
    Vector3 expectedAngularVelocity(0.0f, fixedDt, 0.0f);
    TEST_ASSERT(updatedBody.angularVelocity.isApprox(expectedAngularVelocity, 1e-5f),
                "角速度积分应包含扭矩影响：ω = ω0 + (I^-1 * τ) * dt");

    // 验证扭矩已清零
    TEST_ASSERT(updatedBody.torque.isZero(1e-6f),
                "积分后扭矩应被清零");

    world->Shutdown();
    return true;
}

// 验证线性阻尼：速度应按阻尼因子衰减
static bool Test_LinearDamping_VelocityDecay() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    auto* system = world->RegisterSystem<PhysicsUpdateSystem>();
    const float fixedDt = 1.0f / 60.0f;
    system->SetFixedDeltaTime(fixedDt);
    system->SetGravity(Vector3::Zero());

    EntityID entity = world->CreateEntity();

    TransformComponent transform;
    transform.SetPosition(Vector3::Zero());
    transform.SetRotation(Quaternion::Identity());
    world->AddComponent(entity, transform);

    RigidBodyComponent body;
    body.SetMass(1.0f);
    body.linearDamping = 0.1f; // 10% 阻尼
    body.angularDamping = 0.0f;
    body.useGravity = false;
    body.linearVelocity = Vector3(10.0f, 0.0f, 0.0f); // 初始速度 10 m/s
    world->AddComponent(entity, body);

    // 执行一次固定步更新
    system->Update(fixedDt);

    auto& updatedBody = world->GetComponent<RigidBodyComponent>(entity);

    // 验证速度已衰减：v = v0 * pow(1 - damping, dt)
    // 对于 damping = 0.1，dt = 1/60：v = 10 * pow(0.9, 1/60) ≈ 10 * 0.9983 ≈ 9.983
    float dampingFactor = std::pow(1.0f - body.linearDamping, fixedDt);
    Vector3 expectedVelocity = Vector3(10.0f, 0.0f, 0.0f) * dampingFactor;
    TEST_ASSERT(updatedBody.linearVelocity.isApprox(expectedVelocity, 1e-4f),
                "线性阻尼应使速度按 pow(1 - damping, dt) 衰减");

    world->Shutdown();
    return true;
}

// 验证角阻尼：角速度应按阻尼因子衰减
static bool Test_AngularDamping_VelocityDecay() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    auto* system = world->RegisterSystem<PhysicsUpdateSystem>();
    const float fixedDt = 1.0f / 60.0f;
    system->SetFixedDeltaTime(fixedDt);
    system->SetGravity(Vector3::Zero());

    EntityID entity = world->CreateEntity();

    TransformComponent transform;
    transform.SetPosition(Vector3::Zero());
    transform.SetRotation(Quaternion::Identity());
    world->AddComponent(entity, transform);

    RigidBodyComponent body;
    body.SetMass(1.0f);
    body.linearDamping = 0.0f;
    body.angularDamping = 0.15f; // 15% 角阻尼
    body.useGravity = false;
    body.angularVelocity = Vector3(0.0f, 5.0f, 0.0f); // 初始角速度 5 rad/s
    body.inverseInertiaTensor = Matrix3::Identity();
    world->AddComponent(entity, body);

    // 执行一次固定步更新
    system->Update(fixedDt);

    auto& updatedBody = world->GetComponent<RigidBodyComponent>(entity);

    // 验证角速度已衰减：ω = ω0 * pow(1 - damping, dt)
    float dampingFactor = std::pow(1.0f - body.angularDamping, fixedDt);
    Vector3 expectedAngularVelocity = Vector3(0.0f, 5.0f, 0.0f) * dampingFactor;
    TEST_ASSERT(updatedBody.angularVelocity.isApprox(expectedAngularVelocity, 1e-4f),
                "角阻尼应使角速度按 pow(1 - damping, dt) 衰减");

    world->Shutdown();
    return true;
}

// 验证位置锁定：锁定的轴不应移动
static bool Test_PositionLock_Constraint() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    auto* system = world->RegisterSystem<PhysicsUpdateSystem>();
    const float fixedDt = 1.0f / 60.0f;
    system->SetFixedDeltaTime(fixedDt);
    system->SetGravity(Vector3::Zero());

    EntityID entity = world->CreateEntity();

    Vector3 initialPos(1.0f, 2.0f, 3.0f);
    TransformComponent transform;
    transform.SetPosition(initialPos);
    transform.SetRotation(Quaternion::Identity());
    world->AddComponent(entity, transform);

    RigidBodyComponent body;
    body.SetMass(1.0f);
    body.linearDamping = 0.0f;
    body.angularDamping = 0.0f;
    body.useGravity = false;
    body.linearVelocity = Vector3(5.0f, 10.0f, 15.0f); // 三个方向都有速度
    // 锁定 Y 轴
    body.lockPosition[1] = true;
    world->AddComponent(entity, body);

    // 执行一次固定步更新
    system->Update(fixedDt);

    auto& updatedBody = world->GetComponent<RigidBodyComponent>(entity);
    auto& updatedTransform = world->GetComponent<TransformComponent>(entity);

    // 验证 Y 轴位置未改变
    TEST_ASSERT(std::abs(updatedTransform.GetPosition().y() - initialPos.y()) < 1e-6f,
                "锁定 Y 轴后，Y 位置不应改变");

    // 验证 X 和 Z 轴位置已更新
    Vector3 expectedPos = initialPos + Vector3(5.0f, 0.0f, 15.0f) * fixedDt;
    TEST_ASSERT(std::abs(updatedTransform.GetPosition().x() - expectedPos.x()) < 1e-5f &&
                std::abs(updatedTransform.GetPosition().z() - expectedPos.z()) < 1e-5f,
                "未锁定的轴应正常积分");

    // 验证 Y 轴速度被清零
    TEST_ASSERT(std::abs(updatedBody.linearVelocity.y()) < 1e-6f,
                "锁定轴的速度应被清零");

    world->Shutdown();
    return true;
}

// 验证旋转锁定：锁定的轴不应旋转
static bool Test_RotationLock_Constraint() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    auto* system = world->RegisterSystem<PhysicsUpdateSystem>();
    const float fixedDt = 1.0f / 60.0f;
    system->SetFixedDeltaTime(fixedDt);
    system->SetGravity(Vector3::Zero());

    EntityID entity = world->CreateEntity();

    TransformComponent transform;
    transform.SetPosition(Vector3::Zero());
    transform.SetRotation(Quaternion::Identity());
    world->AddComponent(entity, transform);

    RigidBodyComponent body;
    body.SetMass(1.0f);
    body.linearDamping = 0.0f;
    body.angularDamping = 0.0f;
    body.useGravity = false;
    body.angularVelocity = Vector3(1.0f, 2.0f, 3.0f); // 三个方向都有角速度
    body.inverseInertiaTensor = Matrix3::Identity();
    // 锁定 Y 轴旋转
    body.lockRotation[1] = true;
    world->AddComponent(entity, body);

    // 执行一次固定步更新
    system->Update(fixedDt);

    auto& updatedBody = world->GetComponent<RigidBodyComponent>(entity);

    // 验证 Y 轴角速度被清零
    TEST_ASSERT(std::abs(updatedBody.angularVelocity.y()) < 1e-6f,
                "锁定旋转轴后，该轴角速度应被清零");

    // 验证 X 和 Z 轴角速度保持不变（无扭矩）
    TEST_ASSERT(std::abs(updatedBody.angularVelocity.x() - 1.0f) < 1e-5f &&
                std::abs(updatedBody.angularVelocity.z() - 3.0f) < 1e-5f,
                "未锁定的旋转轴应正常保持角速度");

    world->Shutdown();
    return true;
}

// 验证最大线速度限制：超过限制的速度应被限制
static bool Test_MaxLinearSpeed_Constraint() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    auto* system = world->RegisterSystem<PhysicsUpdateSystem>();
    const float fixedDt = 1.0f / 60.0f;
    system->SetFixedDeltaTime(fixedDt);
    system->SetGravity(Vector3::Zero());

    EntityID entity = world->CreateEntity();

    TransformComponent transform;
    transform.SetPosition(Vector3::Zero());
    transform.SetRotation(Quaternion::Identity());
    world->AddComponent(entity, transform);

    RigidBodyComponent body;
    body.SetMass(1.0f);
    body.linearDamping = 0.0f;
    body.angularDamping = 0.0f;
    body.useGravity = false;
    // 设置超过限制的速度
    body.linearVelocity = Vector3(100.0f, 0.0f, 0.0f); // 100 m/s
    body.maxLinearSpeed = 50.0f; // 限制为 50 m/s
    world->AddComponent(entity, body);

    // 执行一次固定步更新
    system->Update(fixedDt);

    auto& updatedBody = world->GetComponent<RigidBodyComponent>(entity);

    // 验证速度已被限制
    float speed = updatedBody.linearVelocity.norm();
    TEST_ASSERT(speed <= body.maxLinearSpeed + 1e-5f,
                "线速度不应超过 maxLinearSpeed");

    // 验证速度方向保持不变
    Vector3 normalized = updatedBody.linearVelocity.normalized();
    Vector3 expectedDirection = Vector3(1.0f, 0.0f, 0.0f);
    TEST_ASSERT(normalized.isApprox(expectedDirection, 1e-5f),
                "速度限制应保持方向不变");

    world->Shutdown();
    return true;
}

// 验证最大角速度限制：超过限制的角速度应被限制
static bool Test_MaxAngularSpeed_Constraint() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    auto* system = world->RegisterSystem<PhysicsUpdateSystem>();
    const float fixedDt = 1.0f / 60.0f;
    system->SetFixedDeltaTime(fixedDt);
    system->SetGravity(Vector3::Zero());

    EntityID entity = world->CreateEntity();

    TransformComponent transform;
    transform.SetPosition(Vector3::Zero());
    transform.SetRotation(Quaternion::Identity());
    world->AddComponent(entity, transform);

    RigidBodyComponent body;
    body.SetMass(1.0f);
    body.linearDamping = 0.0f;
    body.angularDamping = 0.0f;
    body.useGravity = false;
    // 设置超过限制的角速度
    body.angularVelocity = Vector3(0.0f, 20.0f, 0.0f); // 20 rad/s
    body.maxAngularSpeed = 10.0f; // 限制为 10 rad/s
    body.inverseInertiaTensor = Matrix3::Identity();
    world->AddComponent(entity, body);

    // 执行一次固定步更新
    system->Update(fixedDt);

    auto& updatedBody = world->GetComponent<RigidBodyComponent>(entity);

    // 验证角速度已被限制
    float angularSpeed = updatedBody.angularVelocity.norm();
    TEST_ASSERT(angularSpeed <= body.maxAngularSpeed + 1e-5f,
                "角速度不应超过 maxAngularSpeed");

    // 验证角速度方向保持不变
    Vector3 normalized = updatedBody.angularVelocity.normalized();
    Vector3 expectedDirection = Vector3(0.0f, 1.0f, 0.0f);
    TEST_ASSERT(normalized.isApprox(expectedDirection, 1e-5f),
                "角速度限制应保持方向不变");

    world->Shutdown();
    return true;
}

// ============================================================================
// 主入口
// ============================================================================

int main() {
    RUN_TEST(Test_FixedStep_AccumulatorConsistency);
    RUN_TEST(Test_FrameRateIndependence_TotalTimeMatches);
    RUN_TEST(Test_RenderInterpolation_SmoothMotion);
    RUN_TEST(Test_PhysicsUpdate_Flow_GravityAndAABB);
    RUN_TEST(Test_RenderScenario_FrameDropInterpolationAndAABB);
    
    // 积分系统完整性测试
    RUN_TEST(Test_AngularVelocity_Integration);
    RUN_TEST(Test_AngularAcceleration_Integration);
    RUN_TEST(Test_LinearDamping_VelocityDecay);
    RUN_TEST(Test_AngularDamping_VelocityDecay);
    RUN_TEST(Test_PositionLock_Constraint);
    RUN_TEST(Test_RotationLock_Constraint);
    RUN_TEST(Test_MaxLinearSpeed_Constraint);
    RUN_TEST(Test_MaxAngularSpeed_Constraint);

    std::cout << "==============================" << std::endl;
    std::cout << "测试用例: " << g_testCount << std::endl;
    std::cout << "通过: " << g_passedCount << std::endl;
    std::cout << "失败: " << g_failedCount << std::endl;
    std::cout << "==============================" << std::endl;

    return g_failedCount == 0 ? 0 : 1;
}
