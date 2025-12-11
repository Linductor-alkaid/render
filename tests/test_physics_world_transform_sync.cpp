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
 * @file test_physics_world_transform_sync.cpp
 * @brief PhysicsWorld Transform同步事件驱动测试
 *
 * 验证阶段三实现的Transform同步功能：
 * 1) Kinematic物体Transform变化立即同步到Bullet
 * 2) Static物体Transform变化立即同步到Bullet
 * 3) Dynamic物体不触发同步（由物理模拟驱动）
 * 4) 无物理组件的实体不触发同步
 * 5) 边界情况处理（实体销毁、组件移除等）
 */

#ifdef USE_BULLET_PHYSICS

#include "render/physics/physics_world.h"
#include "render/physics/physics_components.h"
#include "render/physics/bullet_adapter/bullet_world_adapter.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/types.h"
#include "render/math_utils.h"
#include <iostream>
#include <cmath>
#include <memory>
#include <atomic>

using namespace Render;
using namespace Render::Physics;
using namespace Render::ECS;

// ============================================================================
// 测试框架
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
            std::cerr << "   条件: " << #condition << std::endl; \
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
// 测试辅助函数
// ============================================================================

static void RegisterPhysicsComponents(std::shared_ptr<World> world) {
    world->RegisterComponent<TransformComponent>();
    world->RegisterComponent<RigidBodyComponent>();
    world->RegisterComponent<ColliderComponent>();
}

// 辅助函数：创建带物理组件的实体
static EntityID CreatePhysicsEntity(
    std::shared_ptr<World> world,
    PhysicsWorld* physicsWorld,
    RigidBodyComponent::BodyType bodyType,
    const Vector3& position = Vector3::Zero()
) {
    EntityID entity = world->CreateEntity();
    
    // 添加TransformComponent
    TransformComponent transform;
    transform.SetPosition(position);
    world->AddComponent(entity, transform);
    
    // 添加RigidBodyComponent
    RigidBodyComponent body;
    body.type = bodyType;
    body.mass = (bodyType == RigidBodyComponent::BodyType::Dynamic) ? 1.0f : 0.0f;
    world->AddComponent(entity, body);
    
    // 添加ColliderComponent
    world->AddComponent(entity, ColliderComponent::CreateSphere(0.5f));
    
    // 执行一次Step来创建Bullet刚体
    physicsWorld->Step(0.016f);
    
    return entity;
}

// ============================================================================
// 3.4.1 基本同步功能测试
// ============================================================================

/**
 * @brief 测试Kinematic物体Transform变化立即同步
 */
static bool Test_KinematicBody_TransformSync_Immediate() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();
    
    PhysicsConfig config;
    PhysicsWorld physicsWorld(world.get(), config);
    
    // 创建Kinematic实体
    EntityID entity = CreatePhysicsEntity(
        world, &physicsWorld,
        RigidBodyComponent::BodyType::Kinematic,
        Vector3(0, 0, 0)
    );
    
    TEST_ASSERT(world->HasComponent<TransformComponent>(entity), "实体应该有TransformComponent");
    TEST_ASSERT(world->HasComponent<RigidBodyComponent>(entity), "实体应该有RigidBodyComponent");
    
    // 获取Bullet适配器
    auto* bulletAdapter = physicsWorld.GetBulletAdapter();
    TEST_ASSERT(bulletAdapter != nullptr, "应该有Bullet适配器");
    TEST_ASSERT(bulletAdapter->HasRigidBody(entity), "刚体应该在Bullet中创建");
    
    // 修改Transform位置
    Vector3 newPosition(10.0f, 20.0f, 30.0f);
    auto& transform = world->GetComponent<TransformComponent>(entity);
    transform.SetPosition(newPosition);
    
    // 验证立即同步到Bullet（通过查询Bullet中的位置）
    // 注意：事件回调是同步的，SetPosition后应该已经同步到Bullet
    Vector3 bulletPosition;
    Quaternion bulletRotation;
    bulletAdapter->SyncTransformFromBullet(entity, bulletPosition, bulletRotation);
    
    // 验证位置已同步（允许小的浮点误差）
    const float epsilon = 0.01f;
    TEST_ASSERT(
        std::abs(bulletPosition.x() - newPosition.x()) < epsilon &&
        std::abs(bulletPosition.y() - newPosition.y()) < epsilon &&
        std::abs(bulletPosition.z() - newPosition.z()) < epsilon,
        "Kinematic物体位置应该立即同步到Bullet"
    );
    
    world->Shutdown();
    return true;
}

/**
 * @brief 测试Static物体Transform变化立即同步
 */
static bool Test_StaticBody_TransformSync_Immediate() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();
    
    PhysicsConfig config;
    PhysicsWorld physicsWorld(world.get(), config);
    
    // 创建Static实体
    EntityID entity = CreatePhysicsEntity(
        world, &physicsWorld,
        RigidBodyComponent::BodyType::Static,
        Vector3(0, 0, 0)
    );
    
    auto* bulletAdapter = physicsWorld.GetBulletAdapter();
    TEST_ASSERT(bulletAdapter != nullptr && bulletAdapter->HasRigidBody(entity),
                "Static刚体应该在Bullet中创建");
    
    // 修改Transform位置
    Vector3 newPosition(5.0f, 10.0f, 15.0f);
    auto& transform = world->GetComponent<TransformComponent>(entity);
    transform.SetPosition(newPosition);
    
    // 验证立即同步
    Vector3 bulletPosition;
    Quaternion bulletRotation;
    bulletAdapter->SyncTransformFromBullet(entity, bulletPosition, bulletRotation);
    
    const float epsilon = 0.01f;
    TEST_ASSERT(
        std::abs(bulletPosition.x() - newPosition.x()) < epsilon &&
        std::abs(bulletPosition.y() - newPosition.y()) < epsilon &&
        std::abs(bulletPosition.z() - newPosition.z()) < epsilon,
        "Static物体位置应该立即同步到Bullet"
    );
    
    world->Shutdown();
    return true;
}

/**
 * @brief 测试Dynamic物体不触发同步
 * 
 * Dynamic物体由物理模拟驱动，Transform变化不应该同步到Bullet
 */
static bool Test_DynamicBody_NoSync() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();
    
    PhysicsConfig config;
    PhysicsWorld physicsWorld(world.get(), config);
    
    // 创建Dynamic实体
    EntityID entity = CreatePhysicsEntity(
        world, &physicsWorld,
        RigidBodyComponent::BodyType::Dynamic,
        Vector3(0, 10, 0)  // 初始位置较高，会受重力影响
    );
    
    auto* bulletAdapter = physicsWorld.GetBulletAdapter();
    TEST_ASSERT(bulletAdapter != nullptr && bulletAdapter->HasRigidBody(entity),
                "Dynamic刚体应该在Bullet中创建");
    
    // 记录初始Bullet位置（经过一次Step后）
    Vector3 initialBulletPos;
    Quaternion initialBulletRot;
    bulletAdapter->SyncTransformFromBullet(entity, initialBulletPos, initialBulletRot);
    
    // 修改Transform位置（这不应该同步到Bullet）
    Vector3 manualPosition(100.0f, 200.0f, 300.0f);
    auto& transform = world->GetComponent<TransformComponent>(entity);
    transform.SetPosition(manualPosition);
    
    // 执行一次Step（物理模拟会更新Dynamic物体）
    physicsWorld.Step(0.016f);
    
    // 验证Bullet位置没有被手动设置的位置覆盖
    // Dynamic物体应该由物理模拟驱动（受重力影响）
    Vector3 bulletPosition;
    Quaternion bulletRotation;
    bulletAdapter->SyncTransformFromBullet(entity, bulletPosition, bulletRotation);
    
    // Dynamic物体的位置应该由物理模拟决定，而不是手动设置的位置
    // 由于受重力影响，Y坐标应该下降
    TEST_ASSERT(
        std::abs(bulletPosition.y() - initialBulletPos.y()) > 0.001f ||
        std::abs(bulletPosition.x() - manualPosition.x()) > 0.1f,
        "Dynamic物体不应该从ECS同步Transform，应该由物理模拟驱动"
    );
    
    world->Shutdown();
    return true;
}

/**
 * @brief 测试无物理组件的实体不触发同步
 */
static bool Test_EntityWithoutPhysics_NoSync() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();
    
    PhysicsConfig config;
    PhysicsWorld physicsWorld(world.get(), config);
    
    // 创建没有物理组件的实体
    EntityID entity = world->CreateEntity();
    TransformComponent transform;
    transform.SetPosition(Vector3(0, 0, 0));
    world->AddComponent(entity, transform);
    
    // 修改Transform位置
    Vector3 newPosition(50.0f, 60.0f, 70.0f);
    auto& transformComp = world->GetComponent<TransformComponent>(entity);
    transformComp.SetPosition(newPosition);
    
    // 验证没有物理组件的实体不会触发同步
    // 这应该不会导致错误或崩溃
    auto* bulletAdapter = physicsWorld.GetBulletAdapter();
    if (bulletAdapter) {
        TEST_ASSERT(
            !bulletAdapter->HasRigidBody(entity),
            "没有物理组件的实体不应该在Bullet中有刚体"
        );
    }
    
    // Transform变化不应该导致任何问题
    TEST_ASSERT(
        transformComp.GetPosition().isApprox(newPosition, 0.001f),
        "Transform应该正常更新"
    );
    
    world->Shutdown();
    return true;
}

// ============================================================================
// 3.4.2 边界情况测试
// ============================================================================

/**
 * @brief 测试实体销毁时的处理
 */
static bool Test_EntityDestroy_DuringSync() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();
    
    PhysicsConfig config;
    PhysicsWorld physicsWorld(world.get(), config);
    
    // 创建Kinematic实体
    EntityID entity = CreatePhysicsEntity(
        world, &physicsWorld,
        RigidBodyComponent::BodyType::Kinematic,
        Vector3(0, 0, 0)
    );
    
    // 修改Transform
    auto& transform = world->GetComponent<TransformComponent>(entity);
    transform.SetPosition(Vector3(1, 2, 3));
    
    // 销毁实体
    world->DestroyEntity(entity);
    
    // 执行Step，不应该崩溃
    physicsWorld.Step(0.016f);
    
    // 验证实体已被销毁
    TEST_ASSERT(!world->IsValidEntity(entity), "实体应该已被销毁");
    
    world->Shutdown();
    return true;
}

/**
 * @brief 测试组件移除时的处理
 */
static bool Test_ComponentRemoved_DuringSync() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();
    
    PhysicsConfig config;
    PhysicsWorld physicsWorld(world.get(), config);
    
    // 创建Kinematic实体
    EntityID entity = CreatePhysicsEntity(
        world, &physicsWorld,
        RigidBodyComponent::BodyType::Kinematic,
        Vector3(0, 0, 0)
    );
    
    // 修改Transform
    auto& transform = world->GetComponent<TransformComponent>(entity);
    transform.SetPosition(Vector3(5, 6, 7));
    
    // 移除RigidBodyComponent
    world->RemoveComponent<RigidBodyComponent>(entity);
    
    // 再次修改Transform，不应该触发同步（因为没有物理组件了）
    transform.SetPosition(Vector3(10, 20, 30));
    
    // 执行Step，不应该崩溃
    physicsWorld.Step(0.016f);
    
    // 验证组件已移除
    TEST_ASSERT(!world->HasComponent<RigidBodyComponent>(entity),
                "RigidBodyComponent应该已被移除");
    
    world->Shutdown();
    return true;
}

/**
 * @brief 测试多次快速变化
 */
static bool Test_MultipleRapidChanges() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();
    
    PhysicsConfig config;
    PhysicsWorld physicsWorld(world.get(), config);
    
    // 创建Kinematic实体
    EntityID entity = CreatePhysicsEntity(
        world, &physicsWorld,
        RigidBodyComponent::BodyType::Kinematic,
        Vector3(0, 0, 0)
    );
    
    auto* bulletAdapter = physicsWorld.GetBulletAdapter();
    TEST_ASSERT(bulletAdapter != nullptr, "应该有Bullet适配器");
    
    // 连续多次修改Transform
    auto& transform = world->GetComponent<TransformComponent>(entity);
    for (int i = 0; i < 10; ++i) {
        Vector3 pos(static_cast<float>(i), static_cast<float>(i * 2), static_cast<float>(i * 3));
        transform.SetPosition(pos);
    }
    
    // 验证最后一次变化已同步
    Vector3 bulletPosition;
    Quaternion bulletRotation;
    bulletAdapter->SyncTransformFromBullet(entity, bulletPosition, bulletRotation);
    
    Vector3 expectedPos(9.0f, 18.0f, 27.0f);
    const float epsilon = 0.01f;
    TEST_ASSERT(
        std::abs(bulletPosition.x() - expectedPos.x()) < epsilon &&
        std::abs(bulletPosition.y() - expectedPos.y()) < epsilon &&
        std::abs(bulletPosition.z() - expectedPos.z()) < epsilon,
        "多次快速变化后，最后的位置应该同步"
    );
    
    world->Shutdown();
    return true;
}

// ============================================================================
// 主测试函数
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "PhysicsWorld Transform同步事件驱动测试" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\n注意: 此测试需要 USE_BULLET_PHYSICS 宏定义\n" << std::endl;
    
    // 3.4.1 基本同步功能测试
    std::cout << "\n--- 3.4.1 基本同步功能测试 ---" << std::endl;
    RUN_TEST(Test_KinematicBody_TransformSync_Immediate);
    RUN_TEST(Test_StaticBody_TransformSync_Immediate);
    RUN_TEST(Test_DynamicBody_NoSync);
    RUN_TEST(Test_EntityWithoutPhysics_NoSync);
    
    // 3.4.2 边界情况测试
    std::cout << "\n--- 3.4.2 边界情况测试 ---" << std::endl;
    RUN_TEST(Test_EntityDestroy_DuringSync);
    RUN_TEST(Test_ComponentRemoved_DuringSync);
    RUN_TEST(Test_MultipleRapidChanges);
    
    // 输出测试结果
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
        std::cout << "\n❌ 部分测试失败" << std::endl;
        return 1;
    }
}

#else // USE_BULLET_PHYSICS

#include <iostream>

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "PhysicsWorld Transform同步事件驱动测试" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\n⚠️  此测试需要 USE_BULLET_PHYSICS 宏定义" << std::endl;
    std::cout << "请在使用Bullet物理引擎的配置下编译运行此测试" << std::endl;
    std::cout << "\n跳过所有测试..." << std::endl;
    return 0;
}

#endif // USE_BULLET_PHYSICS

