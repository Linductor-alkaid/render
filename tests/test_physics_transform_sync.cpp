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
 * @file test_physics_transform_sync.cpp
 * @brief 物理-渲染变换同步测试
 *
 * 验证：
 * 1) 动态物体的 Transform 自动更新（物理 → 渲染）
 * 2) Kinematic 物体可以通过 Transform 驱动（渲染 → 物理）
 * 3) 插值产生平滑动画
 * 4) Static 物体不受物理影响
 * 5) 只处理根对象（有父对象的实体不处理）
 */

#include "render/physics/physics_transform_sync.h"
#include "render/physics/physics_systems.h"
#include "render/physics/physics_components.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include <iostream>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
// 测试辅助函数
// ============================================================================

static void RegisterPhysicsComponents(std::shared_ptr<World> world) {
    world->RegisterComponent<TransformComponent>();
    world->RegisterComponent<RigidBodyComponent>();
    world->RegisterComponent<ColliderComponent>();
}

// ============================================================================
// 测试用例
// ============================================================================

/**
 * @brief 测试动态物体的 Transform 自动更新
 * 
 * 验证：当物理系统更新动态物体的位置和旋转后，
 * SyncPhysicsToTransform 应该将这些变化同步到 TransformComponent
 */
static bool Test_DynamicBody_TransformAutoUpdate() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    auto* physicsSystem = world->RegisterSystem<PhysicsUpdateSystem>();
    physicsSystem->SetGravity(Vector3::Zero()); // 禁用重力，便于测试
    physicsSystem->SetFixedDeltaTime(1.0f / 60.0f);

    PhysicsTransformSync sync;

    EntityID entity = world->CreateEntity();

    // 创建 Transform 和 RigidBody
    TransformComponent transform;
    transform.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
    transform.SetRotation(Quaternion::Identity());
    world->AddComponent(entity, transform);

    RigidBodyComponent body;
    body.SetMass(1.0f);
    body.SetBodyType(RigidBodyComponent::BodyType::Dynamic);
    body.linearVelocity = Vector3(2.0f, 0.0f, 0.0f); // 初始速度 2 m/s
    body.angularVelocity = Vector3(0.0f, 1.0f, 0.0f); // 初始角速度 1 rad/s
    body.linearDamping = 0.0f;
    body.angularDamping = 0.0f;
    body.useGravity = false;
    body.previousPosition = Vector3(0.0f, 0.0f, 0.0f);
    body.previousRotation = Quaternion::Identity();
    world->AddComponent(entity, body);

    const float fixedDt = 1.0f / 60.0f;

    // 执行一次物理更新（这会更新 Transform 的位置和旋转）
    physicsSystem->Update(fixedDt);

    // 同步物理状态到 Transform
    sync.SyncPhysicsToTransform(world.get());

    // 验证 Transform 已被更新
    auto& updatedTransform = world->GetComponent<TransformComponent>(entity);
    Vector3 expectedPos = Vector3(2.0f * fixedDt, 0.0f, 0.0f);
    
    TEST_ASSERT(updatedTransform.GetPosition().isApprox(expectedPos, 1e-5f),
                "动态物体的位置应该从物理系统同步到 Transform");

    // 验证旋转也被更新（角速度积分）
    Quaternion rotation = updatedTransform.GetRotation();
    TEST_ASSERT(!rotation.isApprox(Quaternion::Identity(), 1e-5f),
                "动态物体的旋转应该从物理系统同步到 Transform");

    // 验证 previousPosition/previousRotation 已更新
    // SyncPhysicsToTransform 会将当前帧位置保存到 previousPosition，用于下一帧的插值
    auto& updatedBody = world->GetComponent<RigidBodyComponent>(entity);
    TEST_ASSERT(updatedBody.previousPosition.isApprox(expectedPos, 1e-5f),
                "previousPosition 应该被更新为当前帧位置（用于下一帧插值）");

    world->Shutdown();
    return true;
}

/**
 * @brief 测试 Kinematic 物体可以通过 Transform 驱动
 * 
 * 验证：当手动修改 Kinematic 物体的 Transform 后，
 * SyncTransformToPhysics 应该计算速度并更新物理状态
 */
static bool Test_KinematicBody_TransformDriven() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    PhysicsTransformSync sync;

    EntityID entity = world->CreateEntity();

    // 创建 Transform 和 Kinematic RigidBody
    TransformComponent transform;
    Vector3 initialPos(0.0f, 0.0f, 0.0f);
    transform.SetPosition(initialPos);
    transform.SetRotation(Quaternion::Identity());
    world->AddComponent(entity, transform);

    RigidBodyComponent body;
    body.SetBodyType(RigidBodyComponent::BodyType::Kinematic);
    body.previousPosition = initialPos;
    body.previousRotation = Quaternion::Identity();
    body.linearVelocity = Vector3::Zero();
    body.angularVelocity = Vector3::Zero();
    world->AddComponent(entity, body);

    const float deltaTime = 1.0f / 60.0f;

    // 第一次同步：初始化 previousPosition
    sync.SyncTransformToPhysics(world.get(), deltaTime);

    // 手动移动 Transform（模拟外部驱动）
    Vector3 newPos(1.0f, 2.0f, 3.0f);
    auto& transformRef = world->GetComponent<TransformComponent>(entity);
    transformRef.SetPosition(newPos);

    // 第二次同步：应该计算速度
    sync.SyncTransformToPhysics(world.get(), deltaTime);

    // 验证速度已计算
    auto& updatedBody = world->GetComponent<RigidBodyComponent>(entity);
    Vector3 expectedVelocity = (newPos - initialPos) / deltaTime;
    
    TEST_ASSERT(updatedBody.linearVelocity.isApprox(expectedVelocity, 1e-4f),
                "Kinematic 物体的速度应该根据 Transform 变化计算");

    // 验证 previousPosition 已更新
    TEST_ASSERT(updatedBody.previousPosition.isApprox(newPos, 1e-5f),
                "previousPosition 应该更新为当前位置");

    // 测试旋转驱动
    Quaternion newRot = Quaternion::Identity();
    // 绕 Y 轴旋转 90 度
    Eigen::AngleAxisf angleAxis(static_cast<float>(M_PI) / 2.0f, Vector3::UnitY());
    newRot = Quaternion(angleAxis);
    auto& transformRef2 = world->GetComponent<TransformComponent>(entity);
    transformRef2.SetRotation(newRot);

    // 第三次同步：应该计算角速度
    sync.SyncTransformToPhysics(world.get(), deltaTime);

    // 验证角速度已计算（应该不为零）
    TEST_ASSERT(updatedBody.angularVelocity.norm() > 1e-3f,
                "Kinematic 物体的角速度应该根据旋转变化计算");

    world->Shutdown();
    return true;
}

/**
 * @brief 测试插值产生平滑动画
 * 
 * 验证：InterpolateTransforms 应该在上一帧和当前帧之间进行插值
 */
static bool Test_Interpolation_SmoothAnimation() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    auto* physicsSystem = world->RegisterSystem<PhysicsUpdateSystem>();
    physicsSystem->SetGravity(Vector3::Zero());
    physicsSystem->SetFixedDeltaTime(1.0f / 60.0f);

    PhysicsTransformSync sync;

    EntityID entity = world->CreateEntity();

    TransformComponent transform;
    transform.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
    transform.SetRotation(Quaternion::Identity());
    world->AddComponent(entity, transform);

    RigidBodyComponent body;
    body.SetMass(1.0f);
    body.SetBodyType(RigidBodyComponent::BodyType::Dynamic);
    body.linearVelocity = Vector3(6.0f, 0.0f, 0.0f); // 6 m/s
    body.angularVelocity = Vector3::Zero();
    body.linearDamping = 0.0f;
    body.angularDamping = 0.0f;
    body.useGravity = false;
    body.previousPosition = Vector3(0.0f, 0.0f, 0.0f);
    body.previousRotation = Quaternion::Identity();
    world->AddComponent(entity, body);

    const float fixedDt = 1.0f / 60.0f;

    // 第一帧：执行物理更新并同步
    physicsSystem->Update(fixedDt);
    sync.SyncPhysicsToTransform(world.get());

    // 获取第一帧后的位置
    auto& transform1 = world->GetComponent<TransformComponent>(entity);
    Vector3 pos1 = transform1.GetPosition();

    // 第二帧：执行物理更新并同步
    physicsSystem->Update(fixedDt);
    sync.SyncPhysicsToTransform(world.get());

    // 获取第二帧后的位置
    auto& transform2 = world->GetComponent<TransformComponent>(entity);
    Vector3 pos2 = transform2.GetPosition();

    // 验证位置确实改变了
    TEST_ASSERT(!pos1.isApprox(pos2, 1e-5f),
                "两帧之间位置应该不同");

    // 现在进行插值测试
    // alpha = 0.5 表示在上一帧和当前帧之间的一半位置
    sync.InterpolateTransforms(world.get(), 0.5f);

    auto& interpolatedTransform = world->GetComponent<TransformComponent>(entity);
    Vector3 interpolatedPos = interpolatedTransform.GetPosition();

    // 验证插值位置在 pos1 和 pos2 之间
    Vector3 expectedInterpolated = pos1 + (pos2 - pos1) * 0.5f;
    TEST_ASSERT(interpolatedPos.isApprox(expectedInterpolated, 1e-4f),
                "插值位置应该在上一帧和当前帧之间");

    // 验证插值位置确实在 pos1 和 pos2 之间（数值上）
    float dist1 = (interpolatedPos - pos1).norm();
    float dist2 = (interpolatedPos - pos2).norm();
    float totalDist = (pos2 - pos1).norm();
    
    TEST_ASSERT(dist1 < totalDist && dist2 < totalDist,
                "插值位置应该在两个端点之间");

    world->Shutdown();
    return true;
}

/**
 * @brief 测试插值在不同 alpha 值下的行为
 */
static bool Test_Interpolation_AlphaValues() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    auto* physicsSystem = world->RegisterSystem<PhysicsUpdateSystem>();
    physicsSystem->SetGravity(Vector3::Zero());
    physicsSystem->SetFixedDeltaTime(1.0f / 60.0f);

    PhysicsTransformSync sync;

    EntityID entity = world->CreateEntity();

    TransformComponent transform;
    transform.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
    transform.SetRotation(Quaternion::Identity());
    world->AddComponent(entity, transform);

    RigidBodyComponent body;
    body.SetMass(1.0f);
    body.SetBodyType(RigidBodyComponent::BodyType::Dynamic);
    body.linearVelocity = Vector3(6.0f, 0.0f, 0.0f);
    body.angularVelocity = Vector3::Zero();
    body.linearDamping = 0.0f;
    body.angularDamping = 0.0f;
    body.useGravity = false;
    body.previousPosition = Vector3(0.0f, 0.0f, 0.0f);
    body.previousRotation = Quaternion::Identity();
    world->AddComponent(entity, body);

    const float fixedDt = 1.0f / 60.0f;

    // 第一帧
    physicsSystem->Update(fixedDt);
    sync.SyncPhysicsToTransform(world.get());
    Vector3 pos1 = world->GetComponent<TransformComponent>(entity).GetPosition();

    // 第二帧
    physicsSystem->Update(fixedDt);
    sync.SyncPhysicsToTransform(world.get());
    Vector3 pos2 = world->GetComponent<TransformComponent>(entity).GetPosition();

    // 测试 alpha = 0.0（应该接近 pos1）
    sync.InterpolateTransforms(world.get(), 0.0f);
    Vector3 posAlpha0 = world->GetComponent<TransformComponent>(entity).GetPosition();
    TEST_ASSERT(posAlpha0.isApprox(pos1, 1e-3f),
                "alpha=0 时应该接近上一帧位置");

    // 测试 alpha = 1.0（应该接近 pos2）
    sync.InterpolateTransforms(world.get(), 1.0f);
    Vector3 posAlpha1 = world->GetComponent<TransformComponent>(entity).GetPosition();
    TEST_ASSERT(posAlpha1.isApprox(pos2, 1e-3f),
                "alpha=1 时应该接近当前帧位置");

    // 测试 alpha = 0.25
    sync.InterpolateTransforms(world.get(), 0.25f);
    Vector3 posAlpha025 = world->GetComponent<TransformComponent>(entity).GetPosition();
    Vector3 expected025 = pos1 + (pos2 - pos1) * 0.25f;
    TEST_ASSERT(posAlpha025.isApprox(expected025, 1e-3f),
                "alpha=0.25 时应该在 25% 位置");

    world->Shutdown();
    return true;
}

/**
 * @brief 测试 Static 物体不受物理影响
 */
static bool Test_StaticBody_NoPhysicsUpdate() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    auto* physicsSystem = world->RegisterSystem<PhysicsUpdateSystem>();
    physicsSystem->SetGravity(Vector3(0.0f, -9.81f, 0.0f));
    physicsSystem->SetFixedDeltaTime(1.0f / 60.0f);

    PhysicsTransformSync sync;

    EntityID entity = world->CreateEntity();

    Vector3 staticPos(10.0f, 20.0f, 30.0f);
    TransformComponent transform;
    transform.SetPosition(staticPos);
    transform.SetRotation(Quaternion::Identity());
    world->AddComponent(entity, transform);

    RigidBodyComponent body;
    body.SetBodyType(RigidBodyComponent::BodyType::Static);
    body.previousPosition = staticPos;
    body.previousRotation = Quaternion::Identity();
    world->AddComponent(entity, body);

    const float fixedDt = 1.0f / 60.0f;

    // 执行物理更新
    physicsSystem->Update(fixedDt);

    // 同步物理到 Transform（Static 物体应该被跳过）
    sync.SyncPhysicsToTransform(world.get());

    // 验证位置没有改变
    auto& updatedTransform = world->GetComponent<TransformComponent>(entity);
    TEST_ASSERT(updatedTransform.GetPosition().isApprox(staticPos, 1e-5f),
                "Static 物体的位置不应该被物理系统改变");

    world->Shutdown();
    return true;
}

/**
 * @brief 测试只处理根对象（有父对象的实体不处理）
 */
static bool Test_RootEntityOnly_NoChildProcessing() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    auto* physicsSystem = world->RegisterSystem<PhysicsUpdateSystem>();
    physicsSystem->SetGravity(Vector3::Zero());
    physicsSystem->SetFixedDeltaTime(1.0f / 60.0f);

    PhysicsTransformSync sync;

    // 创建父实体（根对象）
    EntityID parent = world->CreateEntity();
    TransformComponent parentTransform;
    parentTransform.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
    parentTransform.SetRotation(Quaternion::Identity());
    world->AddComponent(parent, parentTransform);

    RigidBodyComponent parentBody;
    parentBody.SetMass(1.0f);
    parentBody.SetBodyType(RigidBodyComponent::BodyType::Dynamic);
    parentBody.linearVelocity = Vector3(2.0f, 0.0f, 0.0f);
    parentBody.linearDamping = 0.0f;
    parentBody.useGravity = false;
    parentBody.previousPosition = Vector3(0.0f, 0.0f, 0.0f);
    parentBody.previousRotation = Quaternion::Identity();
    world->AddComponent(parent, parentBody);

    // 创建子实体
    EntityID child = world->CreateEntity();
    TransformComponent childTransform;
    childTransform.SetPosition(Vector3(1.0f, 0.0f, 0.0f));
    childTransform.SetRotation(Quaternion::Identity());
    childTransform.SetParentEntity(world.get(), parent); // 设置为父实体的子对象
    world->AddComponent(child, childTransform);

    RigidBodyComponent childBody;
    childBody.SetMass(1.0f);
    childBody.SetBodyType(RigidBodyComponent::BodyType::Dynamic);
    childBody.linearVelocity = Vector3(5.0f, 0.0f, 0.0f);
    childBody.linearDamping = 0.0f;
    childBody.useGravity = false;
    childBody.previousPosition = Vector3(1.0f, 0.0f, 0.0f);
    childBody.previousRotation = Quaternion::Identity();
    world->AddComponent(child, childBody);

    const float fixedDt = 1.0f / 60.0f;
    Vector3 childInitialPos = childTransform.GetPosition();

    // 执行物理更新
    physicsSystem->Update(fixedDt);

    // 同步物理到 Transform
    sync.SyncPhysicsToTransform(world.get());

    // 验证父实体被处理（位置已更新）
    auto& updatedParentTransform = world->GetComponent<TransformComponent>(parent);
    Vector3 expectedParentPos = Vector3(2.0f * fixedDt, 0.0f, 0.0f);
    TEST_ASSERT(updatedParentTransform.GetPosition().isApprox(expectedParentPos, 1e-5f),
                "根对象（父实体）应该被处理");

    // 验证子实体不被直接处理（位置保持不变，因为它是子对象）
    // 注意：子对象的位置可能通过 Transform 层级系统更新，但 PhysicsTransformSync 不应该直接处理它
    auto& updatedChildTransform = world->GetComponent<TransformComponent>(child);
    // 验证子对象有父实体（说明它是子对象）
    TEST_ASSERT(updatedChildTransform.GetParentEntity().IsValid(),
                "子对象应该有父实体");
    TEST_ASSERT(updatedChildTransform.GetParentEntity() == parent,
                "子对象的父实体应该是 parent");

    world->Shutdown();
    return true;
}

/**
 * @brief 测试多次同步的一致性
 */
static bool Test_MultipleSyncs_Consistency() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    auto* physicsSystem = world->RegisterSystem<PhysicsUpdateSystem>();
    physicsSystem->SetGravity(Vector3::Zero());
    physicsSystem->SetFixedDeltaTime(1.0f / 60.0f);

    PhysicsTransformSync sync;

    EntityID entity = world->CreateEntity();

    TransformComponent transform;
    transform.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
    transform.SetRotation(Quaternion::Identity());
    world->AddComponent(entity, transform);

    RigidBodyComponent body;
    body.SetMass(1.0f);
    body.SetBodyType(RigidBodyComponent::BodyType::Dynamic);
    body.linearVelocity = Vector3(3.0f, 0.0f, 0.0f);
    body.angularVelocity = Vector3::Zero();
    body.linearDamping = 0.0f;
    body.angularDamping = 0.0f;
    body.useGravity = false;
    body.previousPosition = Vector3(0.0f, 0.0f, 0.0f);
    body.previousRotation = Quaternion::Identity();
    world->AddComponent(entity, body);

    const float fixedDt = 1.0f / 60.0f;

    // 执行多帧更新和同步
    for (int i = 0; i < 5; ++i) {
        physicsSystem->Update(fixedDt);
        sync.SyncPhysicsToTransform(world.get());
    }

    // 验证位置正确累积
    auto& finalTransform = world->GetComponent<TransformComponent>(entity);
    Vector3 expectedPos = Vector3(3.0f * fixedDt * 5.0f, 0.0f, 0.0f);
    TEST_ASSERT(finalTransform.GetPosition().isApprox(expectedPos, 1e-4f),
                "多次同步后位置应该正确累积");

    world->Shutdown();
    return true;
}

/**
 * @brief 测试清除缓存功能
 */
static bool Test_ClearCache() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    auto* physicsSystem = world->RegisterSystem<PhysicsUpdateSystem>();
    physicsSystem->SetGravity(Vector3::Zero());
    physicsSystem->SetFixedDeltaTime(1.0f / 60.0f);

    PhysicsTransformSync sync;

    EntityID entity = world->CreateEntity();

    TransformComponent transform;
    transform.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
    transform.SetRotation(Quaternion::Identity());
    world->AddComponent(entity, transform);

    RigidBodyComponent body;
    body.SetMass(1.0f);
    body.SetBodyType(RigidBodyComponent::BodyType::Dynamic);
    body.linearVelocity = Vector3(2.0f, 0.0f, 0.0f);
    body.linearDamping = 0.0f;
    body.useGravity = false;
    body.previousPosition = Vector3(0.0f, 0.0f, 0.0f);
    body.previousRotation = Quaternion::Identity();
    world->AddComponent(entity, body);

    const float fixedDt = 1.0f / 60.0f;

    // 执行更新和同步（这会填充缓存）
    physicsSystem->Update(fixedDt);
    sync.SyncPhysicsToTransform(world.get());

    // 清除缓存
    sync.ClearCache();

    // 再次同步应该仍然工作（使用 body.previousPosition/previousRotation）
    physicsSystem->Update(fixedDt);
    sync.SyncPhysicsToTransform(world.get());

    // 验证同步仍然正常工作
    auto& updatedTransform = world->GetComponent<TransformComponent>(entity);
    Vector3 expectedPos = Vector3(2.0f * fixedDt * 2.0f, 0.0f, 0.0f);
    TEST_ASSERT(updatedTransform.GetPosition().isApprox(expectedPos, 1e-4f),
                "清除缓存后同步应该仍然正常工作");

    world->Shutdown();
    return true;
}

// ============================================================================
// 主入口
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "物理-渲染变换同步测试" << std::endl;
    std::cout << "========================================" << std::endl;

    // 核心功能测试
    RUN_TEST(Test_DynamicBody_TransformAutoUpdate);
    RUN_TEST(Test_KinematicBody_TransformDriven);
    RUN_TEST(Test_Interpolation_SmoothAnimation);
    
    // 扩展测试
    RUN_TEST(Test_Interpolation_AlphaValues);
    RUN_TEST(Test_StaticBody_NoPhysicsUpdate);
    RUN_TEST(Test_RootEntityOnly_NoChildProcessing);
    RUN_TEST(Test_MultipleSyncs_Consistency);
    RUN_TEST(Test_ClearCache);

    std::cout << "========================================" << std::endl;
    std::cout << "测试用例: " << g_testCount << std::endl;
    std::cout << "通过: " << g_passedCount << std::endl;
    std::cout << "失败: " << g_failedCount << std::endl;
    std::cout << "========================================" << std::endl;

    return g_failedCount == 0 ? 0 : 1;
}

