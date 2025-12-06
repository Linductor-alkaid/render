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
 * 3) 插值在高帧率下使运动平滑。
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

// ============================================================================
// 主入口
// ============================================================================

int main() {
    RUN_TEST(Test_FixedStep_AccumulatorConsistency);
    RUN_TEST(Test_FrameRateIndependence_TotalTimeMatches);
    RUN_TEST(Test_RenderInterpolation_SmoothMotion);
    RUN_TEST(Test_PhysicsUpdate_Flow_GravityAndAABB);
    RUN_TEST(Test_RenderScenario_FrameDropInterpolationAndAABB);

    std::cout << "==============================" << std::endl;
    std::cout << "测试用例: " << g_testCount << std::endl;
    std::cout << "通过: " << g_passedCount << std::endl;
    std::cout << "失败: " << g_failedCount << std::endl;
    std::cout << "==============================" << std::endl;

    return g_failedCount == 0 ? 0 : 1;
}
