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
 * @brief 阶段 4.2 接触约束（序列冲量 + 弹性 + 摩擦 + Warm Start）测试
 *
 * 覆盖目标（对应 Todolist 4.2 验收）：
 * 1. 无碰撞对时不应改写刚体状态（回归）
 * 2. 法向约束：非穿透 & 速度从靠近转向分离
 * 3. 切向约束：双切向摩擦有效抑制滑动
 * 4. 弹性：恢复系数带来可观反弹速度
 * 5. Warm Start：缓存冲量让下一帧更快收敛
 * 6. 位置修正：堆叠场景能够把穿透推回去
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

struct SceneContext {
    std::shared_ptr<World> world;
    EntityID ground;
    EntityID body;
    ContactManifold manifold;
    Vector3 initialPosition;
    Vector3 initialLinearVelocity;
};

static RigidBodyComponent MakeDynamicBox(float mass = 1.0f, float halfExtent = 0.5f) {
    RigidBodyComponent body;
    body.SetBodyType(RigidBodyComponent::BodyType::Dynamic);
    body.mass = mass;
    body.inverseMass = 1.0f / mass;
    body.centerOfMass = Vector3::Zero();
    float inertia = (1.0f / 12.0f) * mass * (halfExtent * halfExtent * 2.0f);
    body.inertiaTensor = Matrix3::Identity() * inertia;
    body.inverseInertiaTensor = Matrix3::Identity() * (1.0f / inertia);
    return body;
}

static ColliderComponent MakeBoxCollider(float friction, float restitution, const Vector3& halfExtents) {
    ColliderComponent collider = ColliderComponent::CreateBox(halfExtents);
    collider.material->friction = friction;
    collider.material->restitution = restitution;
    return collider;
}

static SceneContext CreateGroundContactScene(
    const Vector3& bodyPos,
    const Vector3& initialVel,
    float penetration,
    float friction,
    float restitution
) {
    SceneContext ctx{};
    ctx.world = std::make_shared<World>();
    RegisterPhysicsComponents(ctx.world);
    ctx.world->Initialize();

    ctx.ground = ctx.world->CreateEntity();
    ctx.body = ctx.world->CreateEntity();

    TransformComponent groundTransform;
    groundTransform.SetPosition(Vector3(0, 0, 0));
    ctx.world->AddComponent(ctx.ground, groundTransform);

    TransformComponent bodyTransform;
    bodyTransform.SetPosition(bodyPos);
    ctx.world->AddComponent(ctx.body, bodyTransform);

    RigidBodyComponent groundBody;
    groundBody.SetBodyType(RigidBodyComponent::BodyType::Static);
    ctx.world->AddComponent(ctx.ground, groundBody);

    RigidBodyComponent fallingBody = MakeDynamicBox();
    fallingBody.linearVelocity = initialVel;
    fallingBody.angularVelocity = Vector3::Zero();
    ctx.world->AddComponent(ctx.body, fallingBody);

    ColliderComponent groundCollider = MakeBoxCollider(friction, restitution, Vector3(10.0f, 0.5f, 10.0f));
    ColliderComponent bodyCollider = MakeBoxCollider(friction, restitution, Vector3(0.5f, 0.5f, 0.5f));
    ctx.world->AddComponent(ctx.ground, groundCollider);
    ctx.world->AddComponent(ctx.body, bodyCollider);

    ctx.manifold.SetNormal(Vector3::UnitY());
    ctx.manifold.AddContact(Vector3(0, 0.5f, 0), Vector3::Zero(), Vector3::Zero(), penetration);

    ctx.initialPosition = bodyPos;
    ctx.initialLinearVelocity = initialVel;
    return ctx;
}

static float ComputeNormalVelocity(const RigidBodyComponent& body, const RigidBodyComponent& ground, const Vector3& normal) {
    Vector3 rel = body.linearVelocity - ground.linearVelocity;
    return rel.dot(normal);
}

static float ComputeTangentialSpeed(const RigidBodyComponent& body, const RigidBodyComponent& ground, const Vector3& normal) {
    Vector3 rel = body.linearVelocity - ground.linearVelocity;
    Vector3 tangential = rel - normal * rel.dot(normal);
    return tangential.norm();
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
    SceneContext ctx = CreateGroundContactScene(
        Vector3(0, 0.55f, 0),
        Vector3(0.5f, -2.0f, 0.0f),
        0.05f,
        0.6f,
        0.0f
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(10);
    solver.SetPositionIterations(2);

    std::vector<CollisionPair> pairs;
    pairs.emplace_back(ctx.ground, ctx.body, ctx.manifold);

    const auto& groundBefore = ctx.world->GetComponent<RigidBodyComponent>(ctx.ground);
    const auto& fallingBefore = ctx.world->GetComponent<RigidBodyComponent>(ctx.body);
    Vector3 normal = ctx.manifold.normal;
    float initialNormalVel = ComputeNormalVelocity(fallingBefore, groundBefore, normal);
    float initialTangentialSpeed = ComputeTangentialSpeed(fallingBefore, groundBefore, normal);

    solver.Solve(1.0f / 60.0f, pairs);

    const auto& groundAfter = ctx.world->GetComponent<RigidBodyComponent>(ctx.ground);
    const auto& fallingAfter = ctx.world->GetComponent<RigidBodyComponent>(ctx.body);

    Vector3 relVel = fallingAfter.linearVelocity - groundAfter.linearVelocity;
    float solvedNormalVel = relVel.dot(normal);
    float solvedTangentialSpeed = (relVel - normal * solvedNormalVel).norm();

    TEST_ASSERT(solvedNormalVel > initialNormalVel + 1.0f,
                "法向相对速度应显著改善，从穿透转向分离");
    TEST_ASSERT(solvedNormalVel > -0.5f,
                "求解后法向速度应接近或大于零，表示碰撞已解决");
    TEST_ASSERT(solvedTangentialSpeed < initialTangentialSpeed * 0.9f,
                "摩擦应该明显减少切向速度");

    ctx.world->Shutdown();
    return true;
}

// ============================================================================
// 用例 3：双切向摩擦（覆盖两个正交切向）
// ============================================================================
bool Test_ConstraintSolver_TwoTangentFriction() {
    SceneContext ctx = CreateGroundContactScene(
        Vector3(0, 0.55f, 0),
        Vector3(1.2f, -2.5f, -0.8f),  // 同时包含 X/Z 切向分量
        0.04f,
        0.9f,
        0.0f
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(12);
    solver.SetPositionIterations(3);

    std::vector<CollisionPair> pairs;
    pairs.emplace_back(ctx.ground, ctx.body, ctx.manifold);

    const auto& groundBefore = ctx.world->GetComponent<RigidBodyComponent>(ctx.ground);
    const auto& bodyBefore = ctx.world->GetComponent<RigidBodyComponent>(ctx.body);
    Vector3 normal = ctx.manifold.normal;
    float initialNormalVel = ComputeNormalVelocity(bodyBefore, groundBefore, normal);
    float initialTangentialSpeed = ComputeTangentialSpeed(bodyBefore, groundBefore, normal);

    solver.Solve(1.0f / 60.0f, pairs);

    const auto& groundAfter = ctx.world->GetComponent<RigidBodyComponent>(ctx.ground);
    const auto& bodyAfter = ctx.world->GetComponent<RigidBodyComponent>(ctx.body);
    float solvedNormalVel = ComputeNormalVelocity(bodyAfter, groundAfter, normal);
    float solvedTangentialSpeed = ComputeTangentialSpeed(bodyAfter, groundAfter, normal);

    TEST_ASSERT(solvedNormalVel > initialNormalVel + 1.2f,
                "法向分离速度应明显提升（序列冲量生效）");
    TEST_ASSERT(solvedTangentialSpeed < initialTangentialSpeed * 0.7f,
                "两条切向方向的摩擦都应有效抑制滑动");

    ctx.world->Shutdown();
    return true;
}

// ============================================================================
// 用例 4：弹性恢复（高 restitution 带来反弹）
// ============================================================================
bool Test_ConstraintSolver_RestitutionBounce() {
    SceneContext ctx = CreateGroundContactScene(
        Vector3(0, 0.6f, 0),
        Vector3(0.0f, -5.0f, 0.0f),
        0.06f,
        0.2f,
        0.8f  // 高恢复
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(12);
    solver.SetPositionIterations(3);

    std::vector<CollisionPair> pairs;
    pairs.emplace_back(ctx.ground, ctx.body, ctx.manifold);

    const auto& groundBefore = ctx.world->GetComponent<RigidBodyComponent>(ctx.ground);
    const auto& bodyBefore = ctx.world->GetComponent<RigidBodyComponent>(ctx.body);
    Vector3 normal = ctx.manifold.normal;
    float initialNormalVel = ComputeNormalVelocity(bodyBefore, groundBefore, normal);

    std::cout << "=== Restitution Test Debug ===" << std::endl;
    std::cout << "Initial velocity: " << bodyBefore.linearVelocity.transpose() << std::endl;
    std::cout << "Initial normal velocity: " << initialNormalVel << std::endl;
    std::cout << "Restitution: 0.8" << std::endl;
    std::cout << "Normal: " << normal.transpose() << std::endl;

    solver.Solve(1.0f / 60.0f, pairs);

    const auto& groundAfter = ctx.world->GetComponent<RigidBodyComponent>(ctx.ground);
    const auto& bodyAfter = ctx.world->GetComponent<RigidBodyComponent>(ctx.body);
    float solvedNormalVel = ComputeNormalVelocity(bodyAfter, groundAfter, normal);

    std::cout << "After solve velocity: " << bodyAfter.linearVelocity.transpose() << std::endl;
    std::cout << "After solve normal velocity: " << solvedNormalVel << std::endl;
    std::cout << "Expected: > 1.5" << std::endl;
    std::cout << "==============================" << std::endl;

    TEST_ASSERT(solvedNormalVel > 1.5f,
                "高恢复系数应产生明显向上的反弹速度");
    TEST_ASSERT(solvedNormalVel > initialNormalVel + 5.0f,
                "反弹幅度应远大于入射速度的负向值");

    ctx.world->Shutdown();
    return true;
}

// ============================================================================
// 用例 5：Warm Start 复用上一帧冲量，加快收敛
// ============================================================================
bool Test_ConstraintSolver_WarmStart_CachesImpulses() {
    // 策略：使用0次迭代，只依靠Warm Start本身的效果
    // Warm solver应该在WarmStart阶段就改变速度
    // Cold solver 0次迭代不会有任何效果
    SceneContext warmCtx = CreateGroundContactScene(
        Vector3(0, 0.5f, 0),
        Vector3(0.0f, -3.0f, 0.0f),
        0.08f,
        0.5f,
        0.0f
    );

    ConstraintSolver warmSolver(warmCtx.world.get());
    warmSolver.SetSolverIterations(15);
    warmSolver.SetPositionIterations(2);

    std::vector<CollisionPair> pairsWarm;
    pairsWarm.emplace_back(warmCtx.ground, warmCtx.body, warmCtx.manifold);
    warmSolver.Solve(1.0f / 60.0f, pairsWarm);

    std::cout << "=== Warm Start Test Debug ===" << std::endl;
    const auto& bodyAfterFirstSolve = warmCtx.world->GetComponent<RigidBodyComponent>(warmCtx.body);
    std::cout << "After first solve (15 iter): " << bodyAfterFirstSolve.linearVelocity.transpose() << std::endl;

    auto& warmBody = warmCtx.world->GetComponent<RigidBodyComponent>(warmCtx.body);
    auto& warmTransform = warmCtx.world->GetComponent<TransformComponent>(warmCtx.body);
    warmBody.linearVelocity = warmCtx.initialLinearVelocity;
    warmBody.angularVelocity = Vector3::Zero();
    warmTransform.SetPosition(warmCtx.initialPosition);

    std::cout << "Reset to initial state: " << warmBody.linearVelocity.transpose() << std::endl;

    // 关键：0次迭代，只有WarmStart会执行（位置迭代也设为0以完全隔离）
    warmSolver.SetSolverIterations(0);
    warmSolver.SetPositionIterations(0);
    warmSolver.Solve(1.0f / 60.0f, pairsWarm);
    const auto& warmBodyAfter = warmCtx.world->GetComponent<RigidBodyComponent>(warmCtx.body);
    float warmNormalVel = ComputeNormalVelocity(warmBodyAfter, warmCtx.world->GetComponent<RigidBodyComponent>(warmCtx.ground), warmCtx.manifold.normal);

    std::cout << "Warm solve (0 iter): " << warmBodyAfter.linearVelocity.transpose() << std::endl;
    std::cout << "Warm normal velocity: " << warmNormalVel << std::endl;

    // Cold solver: 0次迭代不应改变任何东西
    SceneContext coldCtx = CreateGroundContactScene(
        warmCtx.initialPosition,
        warmCtx.initialLinearVelocity,
        0.08f,
        0.5f,
        0.0f
    );
    ConstraintSolver coldSolver(coldCtx.world.get());
    coldSolver.SetSolverIterations(0);
    coldSolver.SetPositionIterations(0);  // 也设为0
    std::vector<CollisionPair> pairsCold;
    pairsCold.emplace_back(coldCtx.ground, coldCtx.body, coldCtx.manifold);
    coldSolver.Solve(1.0f / 60.0f, pairsCold);
    const auto& coldBodyAfter = coldCtx.world->GetComponent<RigidBodyComponent>(coldCtx.body);
    float coldNormalVel = ComputeNormalVelocity(coldBodyAfter, coldCtx.world->GetComponent<RigidBodyComponent>(coldCtx.ground), coldCtx.manifold.normal);

    std::cout << "Cold solve (0 iter): " << coldBodyAfter.linearVelocity.transpose() << std::endl;
    std::cout << "Cold normal velocity: " << coldNormalVel << std::endl;
    std::cout << "Difference: " << (warmNormalVel - coldNormalVel) << std::endl;
    std::cout << "Expected: > 1.0" << std::endl;
    std::cout << "==============================" << std::endl;

    TEST_ASSERT(std::abs(coldNormalVel - (-3.0f)) < 0.01f,
                "Cold solver 0次迭代不应改变速度");
    TEST_ASSERT(warmNormalVel > coldNormalVel + 1.0f,
                "Warm Start 应在0次迭代时利用缓存冲量显著改善速度");

    warmCtx.world->Shutdown();
    coldCtx.world->Shutdown();
    return true;
}

// ============================================================================
// 用例 6：位置修正保证堆叠不穿透
// ============================================================================
bool Test_ConstraintSolver_PositionCorrection_Stacking() {
    SceneContext ctx = CreateGroundContactScene(
        Vector3(0, 0.9f, 0),    // 轻微穿透
        Vector3(0.0f, 0.0f, 0.0f),
        0.03f,
        0.6f,
        0.0f
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(8);
    solver.SetPositionIterations(6);

    std::vector<CollisionPair> pairs;
    pairs.emplace_back(ctx.ground, ctx.body, ctx.manifold);

    float yBefore = ctx.initialPosition.y();
    solver.Solve(1.0f / 60.0f, pairs);

    const auto& groundAfter = ctx.world->GetComponent<RigidBodyComponent>(ctx.ground);
    const auto& bodyAfterRB = ctx.world->GetComponent<RigidBodyComponent>(ctx.body);
    const auto& bodyAfterTr = ctx.world->GetComponent<TransformComponent>(ctx.body);
    float yAfter = bodyAfterTr.GetPosition().y();
    float solvedNormalVel = ComputeNormalVelocity(bodyAfterRB, groundAfter, ctx.manifold.normal);

    TEST_ASSERT(yAfter >= yBefore,
                "位置迭代应避免进一步下沉，堆叠应朝分离方向校正");
    TEST_ASSERT(solvedNormalVel >= -0.05f,
                "位置解算后法向速度不应继续指向穿透（应接近静止/分离）");

    ctx.world->Shutdown();
    return true;
}

// ============================================================================
// 主入口
// ============================================================================

int main() {
    RUN_TEST(Test_ConstraintSolver_NoPairs_NoChange);
    RUN_TEST(Test_ConstraintSolver_ResolveContactAndFriction);
    RUN_TEST(Test_ConstraintSolver_TwoTangentFriction);
    RUN_TEST(Test_ConstraintSolver_RestitutionBounce);
    RUN_TEST(Test_ConstraintSolver_WarmStart_CachesImpulses);
    RUN_TEST(Test_ConstraintSolver_PositionCorrection_Stacking);

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "测试总数: " << g_testCount << std::endl;
    std::cout << "通过: " << g_passedCount << std::endl;
    std::cout << "失败: " << g_failedCount << std::endl;

    return g_failedCount == 0 ? 0 : 1;
}

