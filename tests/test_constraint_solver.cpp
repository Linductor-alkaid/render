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
 * @file test_constraint_solver.cpp
 * @brief 阶段 4.1 约束求解器框架自动化测试
 *
 * 覆盖目标：
 * 1. Solve 在没有碰撞对时保持状态不变（框架清理正确）
 * 2. Solve 对单个接触约束能产生合理的法向、切向冲量（解算流程有效）
 */

#include "render/physics/dynamics/constraint_solver.h"
#include "render/physics/physics_components.h"
#include "render/physics/collision/contact_manifold.h"
#include "render/physics/physics_systems.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include <iostream>
#include <memory>
#include <vector>
#include <cmath>

using namespace Render;
using namespace Render::Physics;
using namespace Render::ECS;

// ============================================================================
// 简易测试框架（与现有物理测试保持一致）
// ============================================================================

static int g_testCount = 0;
static int g_passedCount = 0;
static int g_failedCount = 0;

#define TEST_ASSERT(condition, message)           \
    do {                                          \
        g_testCount++;                            \
        if (!(condition)) {                       \
            std::cerr << "❌ 测试失败: " << message << std::endl; \
            std::cerr << "   位置: " << __FILE__ << ":" << __LINE__ << std::endl; \
            g_failedCount++;                      \
            return false;                         \
        }                                         \
        g_passedCount++;                          \
    } while(0)

#define RUN_TEST(test_func)                       \
    do {                                          \
        std::cout << "运行测试: " << #test_func << "..." << std::endl; \
        if (test_func()) {                        \
            std::cout << "✓ " << #test_func << " 通过" << std::endl; \
        } else {                                  \
            std::cout << "✗ " << #test_func << " 失败" << std::endl; \
        }                                         \
    } while(0)

// ============================================================================
// 测试辅助
// ============================================================================

void RegisterPhysicsComponents(std::shared_ptr<World> world) {
    world->RegisterComponent<TransformComponent>();
    world->RegisterComponent<ColliderComponent>();
    world->RegisterComponent<RigidBodyComponent>();
}

// ============================================================================
// 用例 1：空碰撞对不应改变状态
// ============================================================================

bool Test_ConstraintSolver_NoPairs_NoChange() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    ConstraintSolver solver(world.get());

    EntityID entity = world->CreateEntity();
    TransformComponent transform;
    transform.SetPosition(Vector3(0, 1, 0));
    world->AddComponent(entity, transform);

    RigidBodyComponent body;
    body.linearVelocity = Vector3(1.0f, 2.0f, 3.0f);
    body.angularVelocity = Vector3(0.5f, 0.0f, -0.25f);
    world->AddComponent(entity, body);

    ColliderComponent collider = ColliderComponent::CreateSphere(0.5f);
    world->AddComponent(entity, collider);

    const Vector3 initialLinear = body.linearVelocity;
    const Vector3 initialAngular = body.angularVelocity;

    std::vector<CollisionPair> emptyPairs;
    solver.Solve(1.0f / 60.0f, emptyPairs);

    const auto& bodyAfter = world->GetComponent<RigidBodyComponent>(entity);
    TEST_ASSERT((bodyAfter.linearVelocity - initialLinear).norm() < 1e-6f,
                "无碰撞对时线速度应保持不变");
    TEST_ASSERT((bodyAfter.angularVelocity - initialAngular).norm() < 1e-6f,
                "无碰撞对时角速度应保持不变");

    world->Shutdown();
    return true;
}

// ============================================================================
// 用例 2：单接触约束产生有效法向/切向解算
// ============================================================================

bool Test_ConstraintSolver_ResolveContactAndFriction() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    ConstraintSolver solver(world.get());
    solver.SetSolverIterations(10);
    solver.SetPositionIterations(2);

    // 创建地面和动态刚体
    EntityID ground = world->CreateEntity();
    EntityID dynamicBody = world->CreateEntity();

    TransformComponent groundTransform;
    groundTransform.SetPosition(Vector3(0, 0, 0));
    world->AddComponent(ground, groundTransform);

    TransformComponent bodyTransform;
    bodyTransform.SetPosition(Vector3(0, 0.55f, 0));  // 物体在地面上方
    world->AddComponent(dynamicBody, bodyTransform);

    // === 关键修复：正确初始化地面刚体 ===
    RigidBodyComponent groundBody;
    groundBody.SetBodyType(RigidBodyComponent::BodyType::Static);
    // 静态物体的质量属性应该已经在 SetBodyType 中设置
    // 确保 inverseMass = 0, inverseInertiaTensor = 0
    world->AddComponent(ground, groundBody);

    // === 关键修复：正确初始化动态刚体的所有物理属性 ===
    RigidBodyComponent fallingBody;
    fallingBody.SetBodyType(RigidBodyComponent::BodyType::Dynamic);
    fallingBody.mass = 1.0f;
    fallingBody.inverseMass = 1.0f;
    fallingBody.centerOfMass = Vector3::Zero();
    
    // 为单位立方体设置合理的惯性张量
    // I = (1/12) * m * (h^2 + d^2) for box
    float boxSize = 0.5f;
    float inertia = (1.0f / 12.0f) * fallingBody.mass * (boxSize * boxSize + boxSize * boxSize);
    fallingBody.inertiaTensor = Matrix3::Identity() * inertia;
    fallingBody.inverseInertiaTensor = Matrix3::Identity() * (1.0f / inertia);
    
    fallingBody.linearVelocity = Vector3(0.5f, -2.0f, 0.0f);  // 向下且有水平速度
    fallingBody.angularVelocity = Vector3::Zero();
    world->AddComponent(dynamicBody, fallingBody);

    ColliderComponent groundCollider = ColliderComponent::CreateBox(Vector3(10.0f, 0.5f, 10.0f));
    ColliderComponent bodyCollider = ColliderComponent::CreateBox(Vector3(0.5f, 0.5f, 0.5f));
    world->AddComponent(ground, groundCollider);
    world->AddComponent(dynamicBody, bodyCollider);

    // 构造接触流形（法线指向动态体，即 +Y 方向）
    ContactManifold manifold;
    manifold.SetNormal(Vector3::UnitY());  // 法线向上
    // 接触点在 y=0.5 处（地面顶部），穿透深度 0.05
    manifold.AddContact(Vector3(0, 0.5f, 0), Vector3::Zero(), Vector3::Zero(), 0.05f);

    std::vector<CollisionPair> pairs;
    pairs.emplace_back(ground, dynamicBody, manifold);

    // 记录初始状态
    const auto& groundBefore = world->GetComponent<RigidBodyComponent>(ground);
    const auto& fallingBefore = world->GetComponent<RigidBodyComponent>(dynamicBody);
    Vector3 normal = manifold.normal;
    float initialNormalVel = (fallingBefore.linearVelocity - groundBefore.linearVelocity).dot(normal);
    Vector3 initialRelVel = fallingBefore.linearVelocity - groundBefore.linearVelocity;
    float initialTangentialSpeed = (initialRelVel - normal * initialNormalVel).norm();

    std::cout << "初始状态:" << std::endl;
    std::cout << "  动态体速度: (" << fallingBefore.linearVelocity.x() << ", " 
              << fallingBefore.linearVelocity.y() << ", " 
              << fallingBefore.linearVelocity.z() << ")" << std::endl;
    std::cout << "  法向相对速度: " << initialNormalVel << " (负值表示接近)" << std::endl;
    std::cout << "  切向相对速度: " << initialTangentialSpeed << std::endl;

    // 执行约束求解
    solver.Solve(1.0f / 60.0f, pairs);

    const auto& groundAfter = world->GetComponent<RigidBodyComponent>(ground);
    const auto& fallingAfter = world->GetComponent<RigidBodyComponent>(dynamicBody);

    Vector3 relVel = fallingAfter.linearVelocity - groundAfter.linearVelocity;
    float solvedNormalVel = relVel.dot(normal);
    float solvedTangentialSpeed = (relVel - normal * solvedNormalVel).norm();

    std::cout << "求解后状态:" << std::endl;
    std::cout << "  动态体速度: (" << fallingAfter.linearVelocity.x() << ", " 
              << fallingAfter.linearVelocity.y() << ", " 
              << fallingAfter.linearVelocity.z() << ")" << std::endl;
    std::cout << "  法向相对速度: " << solvedNormalVel << " (正值表示分离)" << std::endl;
    std::cout << "  切向相对速度: " << solvedTangentialSpeed << std::endl;

    // 测试1：法向速度应该显著改善（从穿透转为分离或接近分离）
    // 初始法向速度是 -2.0（向下），求解后应该至少接近 0 或变正
    TEST_ASSERT(solvedNormalVel > initialNormalVel + 1.0f,
                "法向相对速度应显著改善，从穿透转向分离");
    
    // 测试2：理想情况下，法向速度应该变为非负（不再穿透）
    // 考虑到数值误差和 Baumgarte 稳定，我们接受接近 0 的值
    TEST_ASSERT(solvedNormalVel > -0.5f,
                "求解后法向速度应接近或大于零，表示碰撞已解决");

    // 测试3：摩擦应该减少切向滑动
    TEST_ASSERT(solvedTangentialSpeed <= initialTangentialSpeed + 1e-4f,
                "切向相对速度不应增大，应被摩擦抑制");
    
    // 测试4：摩擦应该产生明显效果
    TEST_ASSERT(solvedTangentialSpeed < initialTangentialSpeed * 0.9f,
                "摩擦应该明显减少切向速度");

    world->Shutdown();
    return true;
}

// ============================================================================
// 主入口
// ============================================================================

int main() {
    RUN_TEST(Test_ConstraintSolver_NoPairs_NoChange);
    RUN_TEST(Test_ConstraintSolver_ResolveContactAndFriction);

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "测试总数: " << g_testCount << std::endl;
    std::cout << "通过: " << g_passedCount << std::endl;
    std::cout << "失败: " << g_failedCount << std::endl;

    return g_failedCount == 0 ? 0 : 1;
}

