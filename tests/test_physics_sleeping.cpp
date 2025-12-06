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
 * @file test_physics_sleeping.cpp
 * @brief 阶段 3.4 休眠系统单元测试
 *
 * 覆盖点：
 * 1) 低动能累积 0.5s 后进入休眠；
 * 2) 施加力会唤醒刚体并重置计时器；
 * 3) 碰撞/岛屿唤醒：活跃物体撞击休眠物体会唤醒对方。
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

static void RegisterPhysicsComponents(const std::shared_ptr<World>& world) {
    world->RegisterComponent<TransformComponent>();
    world->RegisterComponent<RigidBodyComponent>();
    world->RegisterComponent<ColliderComponent>();
    world->RegisterComponent<ForceFieldComponent>();
}

// ============================================================================
// 测试用例
// ============================================================================

// 低动能累积 0.5s 后自动休眠
static bool Test_SleepAfterLowEnergy() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    auto* physicsSystem = world->RegisterSystem<PhysicsUpdateSystem>();
    physicsSystem->SetFixedDeltaTime(1.0f / 60.0f);
    physicsSystem->SetGravity(Vector3::Zero()); // 只测试休眠，不受重力影响

    EntityID entity = world->CreateEntity();

    TransformComponent transform;
    transform.SetPosition(Vector3::Zero());
    transform.SetRotation(Quaternion::Identity());
    world->AddComponent(entity, transform);

    RigidBodyComponent body;
    body.SetMass(1.0f);
    body.useGravity = false;
    world->AddComponent(entity, body);

    // 累积 >0.5s 固定步长
    for (int i = 0; i < 40; ++i) {
        physicsSystem->Update(1.0f / 60.0f);
    }

    auto& updatedBody = world->GetComponent<RigidBodyComponent>(entity);
    TEST_ASSERT(updatedBody.isSleeping, "低动能持续 0.5s 后应进入休眠");
    TEST_ASSERT(updatedBody.linearVelocity.isZero(1e-6f) && updatedBody.angularVelocity.isZero(1e-6f),
                "进入休眠时线/角速度应被清零");

    world->Shutdown();
    return true;
}

// 施加力会唤醒刚体并重置计时器
static bool Test_WakeUp_OnForceApplied() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    auto* physicsSystem = world->RegisterSystem<PhysicsUpdateSystem>();
    physicsSystem->SetFixedDeltaTime(1.0f / 60.0f);
    physicsSystem->SetGravity(Vector3::Zero());

    EntityID entity = world->CreateEntity();

    TransformComponent transform;
    transform.SetPosition(Vector3::Zero());
    transform.SetRotation(Quaternion::Identity());
    world->AddComponent(entity, transform);

    RigidBodyComponent body;
    body.SetMass(1.0f);
    body.isSleeping = true;
    body.sleepTimer = 0.6f;
    world->AddComponent(entity, body);

    physicsSystem->ApplyForce(entity, Vector3(5.0f, 0.0f, 0.0f));
    physicsSystem->Update(1.0f / 60.0f);

    auto& updatedBody = world->GetComponent<RigidBodyComponent>(entity);
    TEST_ASSERT(!updatedBody.isSleeping, "施加力后刚体应被唤醒");
    TEST_ASSERT(updatedBody.sleepTimer == 0.0f, "唤醒后休眠计时器应重置");

    world->Shutdown();
    return true;
}

// 活跃物体碰撞休眠物体时，碰撞/岛屿机制应唤醒休眠物体
static bool Test_Collision_WakesSleepingBody() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    auto* collisionSystem = world->RegisterSystem<CollisionDetectionSystem>();
    auto* physicsSystem = world->RegisterSystem<PhysicsUpdateSystem>();
    physicsSystem->SetFixedDeltaTime(1.0f / 60.0f);
    physicsSystem->SetGravity(Vector3::Zero());

    // 活跃刚体 A
    EntityID entityA = world->CreateEntity();
    TransformComponent transformA;
    transformA.SetPosition(Vector3::Zero());
    transformA.SetRotation(Quaternion::Identity());
    world->AddComponent(entityA, transformA);

    RigidBodyComponent bodyA;
    bodyA.SetMass(1.0f);
    bodyA.linearVelocity = Vector3(1.0f, 0.0f, 0.0f); // 有动能，视为活跃
    world->AddComponent(entityA, bodyA);

    ColliderComponent colliderA = ColliderComponent::CreateBox(Vector3(0.5f, 0.5f, 0.5f));
    world->AddComponent(entityA, colliderA);

    // 休眠刚体 B，与 A 重叠以触发碰撞
    EntityID entityB = world->CreateEntity();
    TransformComponent transformB;
    transformB.SetPosition(Vector3::Zero());
    transformB.SetRotation(Quaternion::Identity());
    world->AddComponent(entityB, transformB);

    RigidBodyComponent bodyB;
    bodyB.SetMass(1.0f);
    bodyB.isSleeping = true;
    bodyB.sleepTimer = 0.6f;
    world->AddComponent(entityB, bodyB);

    ColliderComponent colliderB = ColliderComponent::CreateBox(Vector3(0.5f, 0.5f, 0.5f));
    world->AddComponent(entityB, colliderB);

    collisionSystem->Update(0.0f);          // 先生成碰撞对
    physicsSystem->Update(1.0f / 60.0f);    // 再执行休眠检测与唤醒

    auto& updatedBodyB = world->GetComponent<RigidBodyComponent>(entityB);
    TEST_ASSERT(!updatedBodyB.isSleeping, "被活跃物体撞击后应被唤醒");
    TEST_ASSERT(updatedBodyB.sleepTimer == 0.0f, "唤醒后计时器应重置");

    world->Shutdown();
    return true;
}

// ============================================================================
// 主入口
// ============================================================================

int main() {
    RUN_TEST(Test_SleepAfterLowEnergy);
    RUN_TEST(Test_WakeUp_OnForceApplied);
    RUN_TEST(Test_Collision_WakesSleepingBody);

    std::cout << "==============================" << std::endl;
    std::cout << "测试用例: " << g_testCount << std::endl;
    std::cout << "通过: " << g_passedCount << std::endl;
    std::cout << "失败: " << g_failedCount << std::endl;
    std::cout << "==============================" << std::endl;

    return g_failedCount == 0 ? 0 : 1;
}
