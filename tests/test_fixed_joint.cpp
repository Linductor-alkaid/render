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
 * @file test_fixed_joint.cpp
 * @brief Fixed Joint 自动化测试
 *
 * 覆盖目标（对应 Todolist 1.3 验收）：
 * 1. Fixed Joint 成功约束两个刚体的位置
 * 2. 施加外力后刚体不分离
 * 3. 旋转约束有效，两个刚体保持相对旋转
 * 4. 极端质量比测试（1:1000）不崩溃
 * 5. 长时间运行稳定性测试
 */

#include "render/physics/dynamics/constraint_solver.h"
#include "render/physics/physics_components.h"
#include "render/physics/dynamics/joint_component.h"
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
// 简易测试框架
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
    world->RegisterComponent<PhysicsJointComponent>();
}

struct FixedJointSceneContext {
    std::shared_ptr<World> world;
    EntityID bodyA;
    EntityID bodyB;
    EntityID jointEntity;
    Vector3 initialPositionA;
    Vector3 initialPositionB;
    Quaternion initialRotationA;
    Quaternion initialRotationB;
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

static RigidBodyComponent MakeStaticBox() {
    RigidBodyComponent body;
    body.SetBodyType(RigidBodyComponent::BodyType::Static);
    return body;
}

static FixedJointSceneContext CreateFixedJointScene(
    const Vector3& posA,
    const Vector3& posB,
    float massA = 1.0f,
    float massB = 1.0f,
    const Quaternion& rotA = Quaternion::Identity(),
    const Quaternion& rotB = Quaternion::Identity()
) {
    FixedJointSceneContext ctx{};
    ctx.world = std::make_shared<World>();
    RegisterPhysicsComponents(ctx.world);
    ctx.world->Initialize();

    // 创建两个刚体实体
    ctx.bodyA = ctx.world->CreateEntity();
    ctx.bodyB = ctx.world->CreateEntity();
    
    // 创建关节实体（使用bodyA作为关节实体）
    ctx.jointEntity = ctx.bodyA;

    // 设置变换
    TransformComponent transformA;
    transformA.SetPosition(posA);
    transformA.SetRotation(rotA);
    ctx.world->AddComponent(ctx.bodyA, transformA);

    TransformComponent transformB;
    transformB.SetPosition(posB);
    transformB.SetRotation(rotB);
    ctx.world->AddComponent(ctx.bodyB, transformB);

    // 设置刚体
    RigidBodyComponent bodyA = MakeDynamicBox(massA);
    RigidBodyComponent bodyB = MakeDynamicBox(massB);
    ctx.world->AddComponent(ctx.bodyA, bodyA);
    ctx.world->AddComponent(ctx.bodyB, bodyB);

    // 创建关节组件
    PhysicsJointComponent jointComp;
    jointComp.base.type = JointComponent::JointType::Fixed;
    jointComp.base.connectedBody = ctx.bodyB;
    
    // 设置锚点（局部坐标）
    // 锚点设置在各自质心位置
    jointComp.base.localAnchorA = Vector3::Zero();
    jointComp.base.localAnchorB = Vector3::Zero();
    
    // 初始化固定关节数据
    jointComp.data = FixedJointData();
    auto& fixedData = std::get<FixedJointData>(jointComp.data);
    // 相对旋转会在PrepareJointConstraints中自动初始化
    
    ctx.world->AddComponent(ctx.jointEntity, jointComp);

    ctx.initialPositionA = posA;
    ctx.initialPositionB = posB;
    ctx.initialRotationA = rotA;
    ctx.initialRotationB = rotB;

    return ctx;
}

// ============================================================================
// 用例 1：固定关节约束位置 - 两个刚体应保持相对位置
// ============================================================================

bool Test_FixedJoint_PositionConstraint() {
    FixedJointSceneContext ctx = CreateFixedJointScene(
        Vector3(0, 1, 0),
        Vector3(0, 2, 0),
        1.0f,
        1.0f
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(10);
    solver.SetPositionIterations(4);

    // 给bodyB一个向下的速度
    auto& bodyB = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);
    bodyB.linearVelocity = Vector3(0, -5.0f, 0);

    // 运行求解
    std::vector<CollisionPair> emptyPairs;
    std::vector<ECS::EntityID> jointEntities = {ctx.jointEntity};
    
    const float dt = 1.0f / 60.0f;
    solver.SolveWithJoints(dt, emptyPairs, jointEntities);

    // 检查位置约束
    const auto& transformA = ctx.world->GetComponent<TransformComponent>(ctx.bodyA);
    const auto& transformB = ctx.world->GetComponent<TransformComponent>(ctx.bodyB);
    
    Vector3 worldAnchorA = transformA.GetPosition() + transformA.GetRotation() * Vector3::Zero();
    Vector3 worldAnchorB = transformB.GetPosition() + transformB.GetRotation() * Vector3::Zero();
    Vector3 separation = worldAnchorB - worldAnchorA;

    // 经过求解后，锚点距离应该很小
    TEST_ASSERT(separation.norm() < 0.1f,
                "固定关节应约束两个锚点位置，距离应小于0.1");

    ctx.world->Shutdown();
    return true;
}

// ============================================================================
// 用例 2：旋转约束 - 两个刚体应保持相对旋转
// ============================================================================

bool Test_FixedJoint_RotationConstraint() {
    Quaternion rotA = Quaternion::Identity();
    Quaternion rotB(Eigen::AngleAxisf(0.5f, Vector3::UnitZ()));  // 绕Z轴旋转0.5弧度
    
    FixedJointSceneContext ctx = CreateFixedJointScene(
        Vector3(0, 1, 0),
        Vector3(0, 2, 0),
        1.0f,
        1.0f,
        rotA,
        rotB
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(10);
    solver.SetPositionIterations(4);

    // 给bodyB一个角速度
    auto& bodyB = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);
    bodyB.angularVelocity = Vector3(0, 0, 2.0f);  // 绕Z轴旋转

    // 运行多帧求解
    std::vector<CollisionPair> emptyPairs;
    std::vector<ECS::EntityID> jointEntities = {ctx.jointEntity};
    
    const float dt = 1.0f / 60.0f;
    for (int i = 0; i < 10; ++i) {
        solver.SolveWithJoints(dt, emptyPairs, jointEntities);
    }

    // 检查相对旋转是否保持
    const auto& transformA = ctx.world->GetComponent<TransformComponent>(ctx.bodyA);
    const auto& transformB = ctx.world->GetComponent<TransformComponent>(ctx.bodyB);
    const auto& jointComp = ctx.world->GetComponent<PhysicsJointComponent>(ctx.jointEntity);
    const auto& fixedData = std::get<FixedJointData>(jointComp.data);
    
    Quaternion currentRelative = transformB.GetRotation() * transformA.GetRotation().conjugate();
    Quaternion expectedRelative = fixedData.relativeRotation;
    
    // 计算四元数误差（角度差）
    Quaternion error = currentRelative * expectedRelative.conjugate();
    float angleError = 2.0f * std::acos(std::abs(error.w()));
    
    TEST_ASSERT(angleError < 0.2f,
                "固定关节应保持相对旋转，角度误差应小于0.2弧度");

    ctx.world->Shutdown();
    return true;
}

// ============================================================================
// 用例 3：极端质量比测试（1:1000）
// ============================================================================

bool Test_FixedJoint_ExtremeMassRatio() {
    FixedJointSceneContext ctx = CreateFixedJointScene(
        Vector3(0, 1, 0),
        Vector3(0, 2, 0),
        1000.0f,  // 重物体
        1.0f      // 轻物体
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(10);
    solver.SetPositionIterations(4);

    // 给轻物体一个向下的速度
    auto& bodyB = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);
    bodyB.linearVelocity = Vector3(0, -10.0f, 0);

    // 运行求解
    std::vector<CollisionPair> emptyPairs;
    std::vector<ECS::EntityID> jointEntities = {ctx.jointEntity};
    
    const float dt = 1.0f / 60.0f;
    
    // 检查不应崩溃
    bool crashed = false;
    try {
        for (int i = 0; i < 100; ++i) {
            solver.SolveWithJoints(dt, emptyPairs, jointEntities);
            
            // 检查速度是否爆炸
            const auto& bodyA = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyA);
            const auto& bodyBAfter = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);
            
            float speedA = bodyA.linearVelocity.norm();
            float speedB = bodyBAfter.linearVelocity.norm();
            
            if (speedA > 1000.0f || speedB > 1000.0f) {
                std::cerr << "速度爆炸: A=" << speedA << ", B=" << speedB << std::endl;
                crashed = true;
                break;
            }
        }
    } catch (...) {
        crashed = true;
    }

    TEST_ASSERT(!crashed,
                "极端质量比（1:1000）不应导致崩溃或速度爆炸");

    ctx.world->Shutdown();
    return true;
}

// ============================================================================
// 用例 4：施加外力后刚体不分离
// ============================================================================

bool Test_FixedJoint_WithExternalForce() {
    FixedJointSceneContext ctx = CreateFixedJointScene(
        Vector3(0, 1, 0),
        Vector3(0, 2, 0),
        1.0f,
        1.0f
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(10);
    solver.SetPositionIterations(4);

    // 给两个刚体施加相反的外力
    auto& bodyA = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyA);
    auto& bodyB = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);
    
    bodyA.force = Vector3(10.0f, 0, 0);  // 向右
    bodyB.force = Vector3(-10.0f, 0, 0);  // 向左

    // 应用力（简化：直接改变速度）
    const float dt = 1.0f / 60.0f;
    bodyA.linearVelocity += bodyA.force * bodyA.inverseMass * dt;
    bodyB.linearVelocity += bodyB.force * bodyB.inverseMass * dt;

    // 运行求解
    std::vector<CollisionPair> emptyPairs;
    std::vector<ECS::EntityID> jointEntities = {ctx.jointEntity};
    
    solver.SolveWithJoints(dt, emptyPairs, jointEntities);

    // 检查锚点距离
    const auto& transformA = ctx.world->GetComponent<TransformComponent>(ctx.bodyA);
    const auto& transformB = ctx.world->GetComponent<TransformComponent>(ctx.bodyB);
    
    Vector3 worldAnchorA = transformA.GetPosition() + transformA.GetRotation() * Vector3::Zero();
    Vector3 worldAnchorB = transformB.GetPosition() + transformB.GetRotation() * Vector3::Zero();
    Vector3 separation = worldAnchorB - worldAnchorA;

    TEST_ASSERT(separation.norm() < 0.15f,
                "施加外力后，固定关节应保持两个刚体连接，锚点距离应小于0.15");

    ctx.world->Shutdown();
    return true;
}

// ============================================================================
// 用例 5：长时间运行稳定性测试
// ============================================================================

bool Test_FixedJoint_LongTermStability() {
    FixedJointSceneContext ctx = CreateFixedJointScene(
        Vector3(0, 1, 0),
        Vector3(0, 2, 0),
        1.0f,
        1.0f
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(10);
    solver.SetPositionIterations(4);

    // 给bodyB初始速度
    auto& bodyB = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);
    bodyB.linearVelocity = Vector3(1.0f, -2.0f, 0.5f);
    bodyB.angularVelocity = Vector3(0.5f, 0.3f, -0.2f);

    // 运行长时间模拟（相当于1秒，60fps）
    std::vector<CollisionPair> emptyPairs;
    std::vector<ECS::EntityID> jointEntities = {ctx.jointEntity};
    
    const float dt = 1.0f / 60.0f;
    float maxSeparation = 0.0f;
    float maxVelocity = 0.0f;
    
    for (int i = 0; i < 60; ++i) {
        solver.SolveWithJoints(dt, emptyPairs, jointEntities);
        
        const auto& transformA = ctx.world->GetComponent<TransformComponent>(ctx.bodyA);
        const auto& transformB = ctx.world->GetComponent<TransformComponent>(ctx.bodyB);
        const auto& bodyAAfter = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyA);
        const auto& bodyBAfter = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);
        
        Vector3 worldAnchorA = transformA.GetPosition() + transformA.GetRotation() * Vector3::Zero();
        Vector3 worldAnchorB = transformB.GetPosition() + transformB.GetRotation() * Vector3::Zero();
        Vector3 separation = worldAnchorB - worldAnchorA;
        
        maxSeparation = std::max(maxSeparation, separation.norm());
        maxVelocity = std::max(maxVelocity, bodyAAfter.linearVelocity.norm());
        maxVelocity = std::max(maxVelocity, bodyBAfter.linearVelocity.norm());
        
        // 检查速度爆炸
        if (maxVelocity > 100.0f) {
            std::cerr << "速度爆炸在帧 " << i << ": " << maxVelocity << std::endl;
            TEST_ASSERT(false, "长时间运行不应发生速度爆炸");
        }
    }

    TEST_ASSERT(maxSeparation < 0.2f,
                "长时间运行后，锚点距离应保持稳定（小于0.2）");
    TEST_ASSERT(maxVelocity < 50.0f,
                "长时间运行后，速度应保持合理范围（小于50 m/s）");

    ctx.world->Shutdown();
    return true;
}

// ============================================================================
// 用例 6：Warm Start 效果验证
// ============================================================================

bool Test_FixedJoint_WarmStart() {
    FixedJointSceneContext ctx = CreateFixedJointScene(
        Vector3(0, 1, 0),
        Vector3(0, 2, 0),
        1.0f,
        1.0f
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(10);
    solver.SetPositionIterations(4);

    // 给bodyB初始速度
    auto& bodyB = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);
    bodyB.linearVelocity = Vector3(0, -5.0f, 0);

    std::vector<CollisionPair> emptyPairs;
    std::vector<ECS::EntityID> jointEntities = {ctx.jointEntity};
    
    const float dt = 1.0f / 60.0f;
    
    // 第一帧求解
    solver.SolveWithJoints(dt, emptyPairs, jointEntities);
    
    const auto& jointComp1 = ctx.world->GetComponent<PhysicsJointComponent>(ctx.jointEntity);
    Vector3 impulseAfterFirst = jointComp1.runtime.accumulatedLinearImpulse;
    
    // 检查是否累积了冲量
    TEST_ASSERT(impulseAfterFirst.norm() > 0.01f,
                "第一帧求解后应累积冲量用于Warm Start");
    
    // 第二帧（Warm Start应该生效）
    bodyB.linearVelocity = Vector3(0, -5.0f, 0);  // 重置速度
    solver.SolveWithJoints(dt, emptyPairs, jointEntities);
    
    const auto& bodyBAfter = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);
    
    // Warm Start应该立即影响速度
    float speedAfter = bodyBAfter.linearVelocity.norm();
    
    TEST_ASSERT(speedAfter < 4.5f,
                "Warm Start应在第二帧立即影响速度，使速度变化");

    ctx.world->Shutdown();
    return true;
}

// ============================================================================
// 主入口
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Fixed Joint 自动化测试" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    RUN_TEST(Test_FixedJoint_PositionConstraint);
    RUN_TEST(Test_FixedJoint_RotationConstraint);
    RUN_TEST(Test_FixedJoint_ExtremeMassRatio);
    RUN_TEST(Test_FixedJoint_WithExternalForce);
    RUN_TEST(Test_FixedJoint_LongTermStability);
    RUN_TEST(Test_FixedJoint_WarmStart);

    std::cout << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "测试总数: " << g_testCount << std::endl;
    std::cout << "通过: " << g_passedCount << std::endl;
    std::cout << "失败: " << g_failedCount << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    return g_failedCount == 0 ? 0 : 1;
}
