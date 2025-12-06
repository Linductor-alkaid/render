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
    
    // 【调试输出 1】检查默认值
    std::cout << "  [调试] 创建刚体后:" << std::endl;
    std::cout << "    mass = " << body.mass << std::endl;
    std::cout << "    inverseMass = " << body.inverseMass << std::endl;
    
    body.SetMass(2.0f);  // 显式设置质量为 2kg
    
    // 【调试输出 2】检查 SetMass 后的值
    std::cout << "  [调试] SetMass(2.0) 后:" << std::endl;
    std::cout << "    mass = " << body.mass << std::endl;
    std::cout << "    inverseMass = " << body.inverseMass << std::endl;
    
    body.linearDamping = 0.0f;   // 避免阻尼影响
    body.angularDamping = 0.0f;
    body.useGravity = true;
    body.gravityScale = 1.5f;
    
    world->AddComponent(entity, body);

    auto& bodyInWorld = world->GetComponent<RigidBodyComponent>(entity);
    bodyInWorld.linearVelocity.setZero();
    bodyInWorld.angularVelocity.setZero();

    // 【调试输出 3】检查添加到 world 后的值
    std::cout << "  [调试] AddComponent 后:" << std::endl;
    std::cout << "    mass = " << bodyInWorld.mass << std::endl;
    std::cout << "    inverseMass = " << bodyInWorld.inverseMass << std::endl;

    const float dt = 1.0f / 60.0f;
    
    // 【调试输出 4】Update 前的状态
    std::cout << "  [调试] Update 前:" << std::endl;
    std::cout << "    linearVelocity = " << bodyInWorld.linearVelocity.transpose() << std::endl;
    std::cout << "    force = " << bodyInWorld.force.transpose() << std::endl;
    
    system->Update(dt);  // 触发一次固定步长

    // 【调试输出 5】Update 后的状态
    auto& updatedBody = world->GetComponent<RigidBodyComponent>(entity);
    std::cout << "  [调试] Update 后:" << std::endl;
    std::cout << "    force (应为0) = " << updatedBody.force.transpose() << std::endl;
    std::cout << "    linearVelocity = " << updatedBody.linearVelocity.transpose() << std::endl;
    
    // 计算期望值
    Vector3 expectedVelocity = Vector3(0.0f, -9.81f * body.gravityScale * dt, 0.0f);
    std::cout << "  [期望] linearVelocity = " << expectedVelocity.transpose() << std::endl;
    
    // 详细的计算过程
    std::cout << "\n  [计算过程]:" << std::endl;
    std::cout << "    gravity = -9.81 m/s²" << std::endl;
    std::cout << "    gravityScale = " << body.gravityScale << std::endl;
    std::cout << "    mass = " << updatedBody.mass << " kg" << std::endl;
    std::cout << "    dt = " << dt << " s" << std::endl;
    
    float gravityForce = -9.81f * updatedBody.mass * body.gravityScale;
    std::cout << "    gravityForce = gravity * mass * scale = " << gravityForce << " N" << std::endl;
    
    float acceleration = gravityForce * updatedBody.inverseMass;
    std::cout << "    acceleration = force * inverseMass = " << acceleration << " m/s²" << std::endl;
    
    float velocityChange = acceleration * dt;
    std::cout << "    velocityChange = acceleration * dt = " << velocityChange << " m/s" << std::endl;

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
    body.SetMass(2.0f);          // 逆质量 = 0.5
    body.linearDamping = 0.0f;
    body.angularDamping = 0.0f;
    body.useGravity = false;     // 仅关注冲量
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
// 主入口
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "阶段 3.1 力与冲量系统测试" << std::endl;
    std::cout << "========================================" << std::endl;

    RUN_TEST(Test_ForceAccumulator_AccumulationAndClear);
    RUN_TEST(Test_PhysicsUpdateSystem_AppliesGravity);
    RUN_TEST(Test_PhysicsUpdateSystem_ImpulseAffectsVelocityAndRotation);

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
