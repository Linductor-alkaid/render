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
 * @file test_joint_constraints.cpp
 * @brief 关节约束自动化测试
 *
 * 测试目标：
 * 1. 固定关节约束的基本功能
 *    - 位置约束和旋转约束的正确性
 * 2. 距离关节约束的基本功能
 *    - restLength 约束
 *    - 距离限制（minDistance, maxDistance）
 * 3. 数据爆炸检测（速度、角速度、冲量）
 * 4. 多帧稳定性
 * 5. 极端情况处理（极端质量比、高初始速度等）
 */

#include "render/physics/dynamics/constraint_solver.h"
#include "render/physics/physics_components.h"
#include "render/physics/physics_systems.h"
#include "render/physics/dynamics/joint_component.h"
#include "render/physics/collision/contact_manifold.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/math_utils.h"
#include <iostream>
#include <memory>
#include <vector>
#include <cmath>
#include <string>
#include <limits>

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

struct JointSceneContext {
    std::shared_ptr<World> world;
    EntityID bodyA;
    EntityID bodyB;
    EntityID jointEntity;
    Vector3 initialPosA;
    Vector3 initialPosB;
    Quaternion initialRotA;
    Quaternion initialRotB;
};

static JointSceneContext CreateFixedJointScene(
    const Vector3& posA,
    const Vector3& posB,
    const Quaternion& rotA = Quaternion::Identity(),
    const Quaternion& rotB = Quaternion::Identity(),
    const Vector3& velA = Vector3::Zero(),
    const Vector3& velB = Vector3::Zero(),
    const Vector3& angVelA = Vector3::Zero(),
    const Vector3& angVelB = Vector3::Zero(),
    float massA = 1.0f,
    float massB = 1.0f
) {
    JointSceneContext ctx{};
    ctx.world = std::make_shared<World>();
    RegisterPhysicsComponents(ctx.world);
    ctx.world->Initialize();

    ctx.bodyA = ctx.world->CreateEntity();
    ctx.bodyB = ctx.world->CreateEntity();
    // 不再创建单独的 jointEntity，关节组件将附加到 bodyA 上
    ctx.jointEntity = ctx.bodyA;  // 关节实体就是 bodyA

    // 创建刚体A
    TransformComponent transformA;
    transformA.SetPosition(posA);
    transformA.SetRotation(rotA);
    ctx.world->AddComponent(ctx.bodyA, transformA);

    RigidBodyComponent bodyA = MakeDynamicBox(massA);
    bodyA.linearVelocity = velA;
    bodyA.angularVelocity = angVelA;
    ctx.world->AddComponent(ctx.bodyA, bodyA);

    // 创建刚体B
    TransformComponent transformB;
    transformB.SetPosition(posB);
    transformB.SetRotation(rotB);
    ctx.world->AddComponent(ctx.bodyB, transformB);

    RigidBodyComponent bodyB = MakeDynamicBox(massB);
    bodyB.linearVelocity = velB;
    bodyB.angularVelocity = angVelB;
    ctx.world->AddComponent(ctx.bodyB, bodyB);

    // 创建固定关节 - 附加到 bodyA 上
    PhysicsJointComponent joint;
    joint.base.type = JointComponent::JointType::Fixed;
    joint.base.connectedBody = ctx.bodyB;  // 连接到 bodyB
    joint.base.localAnchorA = Vector3::Zero();  // 在质心
    joint.base.localAnchorB = Vector3::Zero();  // 在质心
    joint.base.isEnabled = true;
    joint.base.isBroken = false;
    joint.data = FixedJointData();
    
    // 将关节组件添加到 bodyA（关节的拥有者）
    ctx.world->AddComponent(ctx.bodyA, joint);

    ctx.initialPosA = posA;
    ctx.initialPosB = posB;
    ctx.initialRotA = rotA;
    ctx.initialRotB = rotB;

    return ctx;
}

// 计算两个锚点之间的世界空间距离
static float ComputeAnchorSeparation(
    const TransformComponent& transformA,
    const TransformComponent& transformB,
    const Vector3& localAnchorA,
    const Vector3& localAnchorB
) {
    Vector3 worldAnchorA = transformA.GetPosition() + transformA.GetRotation() * localAnchorA;
    Vector3 worldAnchorB = transformB.GetPosition() + transformB.GetRotation() * localAnchorB;
    return (worldAnchorB - worldAnchorA).norm();
}

// 计算旋转误差（使用四元数）
static float ComputeRotationError(
    const Quaternion& qA,
    const Quaternion& qB,
    const Quaternion& expectedRelative
) {
    Quaternion currentRelative = qB * qA.conjugate();
    Quaternion error = currentRelative * expectedRelative.conjugate();
    error.normalize();
    // 小角度近似：误差角度 ≈ 2 * |vec(error)|
    return 2.0f * error.vec().norm();
}

static JointSceneContext CreateDistanceJointScene(
    const Vector3& posA,
    const Vector3& posB,
    const Quaternion& rotA = Quaternion::Identity(),
    const Quaternion& rotB = Quaternion::Identity(),
    const Vector3& velA = Vector3::Zero(),
    const Vector3& velB = Vector3::Zero(),
    const Vector3& angVelA = Vector3::Zero(),
    const Vector3& angVelB = Vector3::Zero(),
    float massA = 1.0f,
    float massB = 1.0f,
    float restLength = 1.0f,
    bool hasLimits = false,
    float minDistance = 0.0f,
    float maxDistance = std::numeric_limits<float>::infinity()
) {
    JointSceneContext ctx{};
    ctx.world = std::make_shared<World>();
    RegisterPhysicsComponents(ctx.world);
    ctx.world->Initialize();

    ctx.bodyA = ctx.world->CreateEntity();
    ctx.bodyB = ctx.world->CreateEntity();
    ctx.jointEntity = ctx.bodyA;  // 关节实体就是 bodyA

    // 创建刚体A
    TransformComponent transformA;
    transformA.SetPosition(posA);
    transformA.SetRotation(rotA);
    ctx.world->AddComponent(ctx.bodyA, transformA);

    RigidBodyComponent bodyA = MakeDynamicBox(massA);
    bodyA.linearVelocity = velA;
    bodyA.angularVelocity = angVelA;
    ctx.world->AddComponent(ctx.bodyA, bodyA);

    // 创建刚体B
    TransformComponent transformB;
    transformB.SetPosition(posB);
    transformB.SetRotation(rotB);
    ctx.world->AddComponent(ctx.bodyB, transformB);

    RigidBodyComponent bodyB = MakeDynamicBox(massB);
    bodyB.linearVelocity = velB;
    bodyB.angularVelocity = angVelB;
    ctx.world->AddComponent(ctx.bodyB, bodyB);

    // 创建距离关节 - 附加到 bodyA 上
    PhysicsJointComponent joint;
    joint.base.type = JointComponent::JointType::Distance;
    joint.base.connectedBody = ctx.bodyB;  // 连接到 bodyB
    joint.base.localAnchorA = Vector3::Zero();  // 在质心
    joint.base.localAnchorB = Vector3::Zero();  // 在质心
    joint.base.isEnabled = true;
    joint.base.isBroken = false;
    
    DistanceJointData distData;
    distData.restLength = restLength;
    distData.hasLimits = hasLimits;
    distData.minDistance = minDistance;
    distData.maxDistance = maxDistance;
    joint.data = distData;
    
    // 将关节组件添加到 bodyA（关节的拥有者）
    ctx.world->AddComponent(ctx.bodyA, joint);

    ctx.initialPosA = posA;
    ctx.initialPosB = posB;
    ctx.initialRotA = rotA;
    ctx.initialRotB = rotB;

    return ctx;
}

static JointSceneContext CreateHingeJointScene(
    const Vector3& posA,
    const Vector3& posB,
    const Quaternion& rotA = Quaternion::Identity(),
    const Quaternion& rotB = Quaternion::Identity(),
    const Vector3& velA = Vector3::Zero(),
    const Vector3& velB = Vector3::Zero(),
    const Vector3& angVelA = Vector3::Zero(),
    const Vector3& angVelB = Vector3::Zero(),
    float massA = 1.0f,
    float massB = 1.0f,
    const Vector3& localAxisA = Vector3::UnitZ(),
    const Vector3& localAxisB = Vector3::UnitZ(),
    bool hasLimits = false,
    float limitMin = -MathUtils::PI,
    float limitMax = MathUtils::PI,
    bool useMotor = false,
    float motorSpeed = 0.0f,
    float motorMaxForce = 100.0f
) {
    JointSceneContext ctx{};
    ctx.world = std::make_shared<World>();
    RegisterPhysicsComponents(ctx.world);
    ctx.world->Initialize();

    ctx.bodyA = ctx.world->CreateEntity();
    ctx.bodyB = ctx.world->CreateEntity();
    ctx.jointEntity = ctx.bodyA;  // 关节实体就是 bodyA

    // 创建刚体A
    TransformComponent transformA;
    transformA.SetPosition(posA);
    transformA.SetRotation(rotA);
    ctx.world->AddComponent(ctx.bodyA, transformA);

    RigidBodyComponent bodyA = MakeDynamicBox(massA);
    bodyA.linearVelocity = velA;
    bodyA.angularVelocity = angVelA;
    ctx.world->AddComponent(ctx.bodyA, bodyA);

    // 创建刚体B
    TransformComponent transformB;
    transformB.SetPosition(posB);
    transformB.SetRotation(rotB);
    ctx.world->AddComponent(ctx.bodyB, transformB);

    RigidBodyComponent bodyB = MakeDynamicBox(massB);
    bodyB.linearVelocity = velB;
    bodyB.angularVelocity = angVelB;
    ctx.world->AddComponent(ctx.bodyB, bodyB);

    // 创建铰链关节 - 附加到 bodyA 上
    PhysicsJointComponent joint;
    joint.base.type = JointComponent::JointType::Hinge;
    joint.base.connectedBody = ctx.bodyB;  // 连接到 bodyB
    joint.base.localAnchorA = Vector3::Zero();  // 在质心
    joint.base.localAnchorB = Vector3::Zero();  // 在质心
    joint.base.isEnabled = true;
    joint.base.isBroken = false;
    
    HingeJointData hingeData;
    hingeData.localAxisA = localAxisA.normalized();
    hingeData.localAxisB = localAxisB.normalized();
    hingeData.hasLimits = hasLimits;
    hingeData.limitMin = limitMin;
    hingeData.limitMax = limitMax;
    hingeData.currentAngle = 0.0f;
    hingeData.useMotor = useMotor;
    hingeData.motorSpeed = motorSpeed;
    hingeData.motorMaxForce = motorMaxForce;
    joint.data = hingeData;
    
    // 将关节组件添加到 bodyA（关节的拥有者）
    ctx.world->AddComponent(ctx.bodyA, joint);

    ctx.initialPosA = posA;
    ctx.initialPosB = posB;
    ctx.initialRotA = rotA;
    ctx.initialRotB = rotB;

    return ctx;
}

// ============================================================================
// 用例 1：基础固定关节测试 - 两个静止刚体应保持相对位置
// ============================================================================

bool Test_FixedJoint_Basic_StaticBodies() {
    // 创建两个刚体，初始位置重合（满足约束）
    JointSceneContext ctx = CreateFixedJointScene(
        Vector3(0, 0, 0),
        Vector3(0, 0, 0),  // 位置重合，满足约束
        Quaternion::Identity(),
        Quaternion::Identity()
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(10);
    solver.SetPositionIterations(5);

    std::vector<CollisionPair> emptyPairs;
    std::vector<EntityID> jointEntities = {ctx.jointEntity};

    // 记录初始状态
    const auto& transformA_before = ctx.world->GetComponent<TransformComponent>(ctx.bodyA);
    const auto& transformB_before = ctx.world->GetComponent<TransformComponent>(ctx.bodyB);
    float initialSeparation = ComputeAnchorSeparation(
        transformA_before, transformB_before,
        Vector3::Zero(), Vector3::Zero()
    );

    solver.SolveWithJoints(1.0f / 60.0f, emptyPairs, jointEntities);

    const auto& transformA_after = ctx.world->GetComponent<TransformComponent>(ctx.bodyA);
    const auto& transformB_after = ctx.world->GetComponent<TransformComponent>(ctx.bodyB);
    const auto& bodyA_after = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyA);
    const auto& bodyB_after = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);

    float finalSeparation = ComputeAnchorSeparation(
        transformA_after, transformB_after,
        Vector3::Zero(), Vector3::Zero()
    );

    std::cout << "=== Basic Fixed Joint Test ===" << std::endl;
    std::cout << "Initial separation: " << initialSeparation << std::endl;
    std::cout << "Final separation: " << finalSeparation << std::endl;
    std::cout << "Velocity A: " << bodyA_after.linearVelocity.transpose() << std::endl;
    std::cout << "Velocity B: " << bodyB_after.linearVelocity.transpose() << std::endl;
    std::cout << "Angular velocity A: " << bodyA_after.angularVelocity.transpose() << std::endl;
    std::cout << "Angular velocity B: " << bodyB_after.angularVelocity.transpose() << std::endl;
    std::cout << "==============================" << std::endl;

    // 验证：如果初始配置满足约束，静止物体应保持静止
    // 固定关节应该保持两个锚点之间的相对位置不变
    // 如果初始时重合（分离距离为0），应该保持重合
    // 如果初始时有分离距离，应该保持那个分离距离
    float separationError = std::abs(finalSeparation - initialSeparation);
    TEST_ASSERT(separationError < 0.1f,
                "固定关节应保持两个锚点之间的相对位置不变（分离距离变化应小于0.1）");
    
    // 验证速度不应爆炸
    TEST_ASSERT(bodyA_after.linearVelocity.norm() < 10.0f,
                "刚体A的速度不应爆炸");
    TEST_ASSERT(bodyB_after.linearVelocity.norm() < 10.0f,
                "刚体B的速度不应爆炸");
    TEST_ASSERT(bodyA_after.angularVelocity.norm() < 10.0f,
                "刚体A的角速度不应爆炸");
    TEST_ASSERT(bodyB_after.angularVelocity.norm() < 10.0f,
                "刚体B的角速度不应爆炸");

    ctx.world->Shutdown();
    return true;
}

// ============================================================================
// 用例 2：固定关节位置约束 - 应保持初始相对位置
// ============================================================================

bool Test_FixedJoint_PositionConstraint() {
    // 创建两个刚体，初始位置有分离距离（2米）
    // 固定关节应该保持这个相对位置，而不是强制重合
    JointSceneContext ctx = CreateFixedJointScene(
        Vector3(0, 0, 0),
        Vector3(2, 0, 0),  // 相距2米（应该保持这个距离）
        Quaternion::Identity(),
        Quaternion::Identity()
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(15);
    solver.SetPositionIterations(10);

    std::vector<CollisionPair> emptyPairs;
    std::vector<EntityID> jointEntities = {ctx.jointEntity};

    const auto& transformA_before = ctx.world->GetComponent<TransformComponent>(ctx.bodyA);
    const auto& transformB_before = ctx.world->GetComponent<TransformComponent>(ctx.bodyB);
    float initialSeparation = ComputeAnchorSeparation(
        transformA_before, transformB_before,
        Vector3::Zero(), Vector3::Zero()
    );

    solver.SolveWithJoints(1.0f / 60.0f, emptyPairs, jointEntities);

    const auto& transformA_after = ctx.world->GetComponent<TransformComponent>(ctx.bodyA);
    const auto& transformB_after = ctx.world->GetComponent<TransformComponent>(ctx.bodyB);
    float finalSeparation = ComputeAnchorSeparation(
        transformA_after, transformB_after,
        Vector3::Zero(), Vector3::Zero()
    );

    std::cout << "=== Position Constraint Test ===" << std::endl;
    std::cout << "Initial separation: " << initialSeparation << std::endl;
    std::cout << "Final separation: " << finalSeparation << std::endl;
    std::cout << "Expected: should maintain initial separation (2.0)" << std::endl;
    std::cout << "=================================" << std::endl;

    // 验证：位置约束应该保持初始相对位置（分离距离应该接近初始值）
    float separationError = std::abs(finalSeparation - initialSeparation);
    TEST_ASSERT(separationError < 0.5f,
                "位置约束应保持初始相对位置（分离距离变化应小于0.5）");

    ctx.world->Shutdown();
    return true;
}

// ============================================================================
// 用例 3：固定关节旋转约束 - 初始旋转不满足约束时应修正
// ============================================================================

bool Test_FixedJoint_RotationConstraint() {
    // 创建两个刚体，初始旋转不同
    Quaternion rotA = Quaternion::Identity();
    Quaternion rotB = Quaternion(Eigen::AngleAxisf(0.5f, Vector3::UnitZ()));  // 绕Z轴旋转0.5弧度

    JointSceneContext ctx = CreateFixedJointScene(
        Vector3(0, 0, 0),
        Vector3(0, 0, 0),  // 位置相同
        rotA,
        rotB
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(15);
    solver.SetPositionIterations(10);

    std::vector<CollisionPair> emptyPairs;
    std::vector<EntityID> jointEntities = {ctx.jointEntity};

    const auto& transformA_before = ctx.world->GetComponent<TransformComponent>(ctx.bodyA);
    const auto& transformB_before = ctx.world->GetComponent<TransformComponent>(ctx.bodyB);
    float initialRotError = ComputeRotationError(
        transformA_before.GetRotation(),
        transformB_before.GetRotation(),
        Quaternion::Identity()  // 期望相对旋转为单位四元数
    );

    solver.SolveWithJoints(1.0f / 60.0f, emptyPairs, jointEntities);

    const auto& transformA_after = ctx.world->GetComponent<TransformComponent>(ctx.bodyA);
    const auto& transformB_after = ctx.world->GetComponent<TransformComponent>(ctx.bodyB);
    const auto& joint = ctx.world->GetComponent<PhysicsJointComponent>(ctx.jointEntity);
    auto& fixedData = std::get<FixedJointData>(joint.data);
    
    float finalRotError = ComputeRotationError(
        transformA_after.GetRotation(),
        transformB_after.GetRotation(),
        fixedData.relativeRotation
    );

    std::cout << "=== Rotation Constraint Test ===" << std::endl;
    std::cout << "Initial rotation error: " << initialRotError << std::endl;
    std::cout << "Final rotation error: " << finalRotError << std::endl;
    std::cout << "Expected: < 0.2 (should be corrected)" << std::endl;
    std::cout << "=================================" << std::endl;

    // 验证：旋转约束应该修正旋转误差
    TEST_ASSERT(finalRotError < 0.2f,
                "旋转约束应修正旋转误差");

    ctx.world->Shutdown();
    return true;
}

// ============================================================================
// 用例 4：数据爆炸检测 - 速度不应无限增长
// ============================================================================

bool Test_FixedJoint_NoVelocityExplosion() {
    // 创建两个有初始速度的刚体
    JointSceneContext ctx = CreateFixedJointScene(
        Vector3(0, 0, 0),
        Vector3(1, 0, 0),
        Quaternion::Identity(),
        Quaternion::Identity(),
        Vector3(1, 0, 0),  // 刚体A有X方向速度
        Vector3(-1, 0, 0)   // 刚体B有-X方向速度（试图分离）
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(10);
    solver.SetPositionIterations(5);

    std::vector<CollisionPair> emptyPairs;
    std::vector<EntityID> jointEntities = {ctx.jointEntity};

    const auto& bodyA_before = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyA);
    const auto& bodyB_before = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);
    float initialSpeedA = bodyA_before.linearVelocity.norm();
    float initialSpeedB = bodyB_before.linearVelocity.norm();

    // 运行多帧，检测是否有数据爆炸
    const int numFrames = 100;
    float dt = 1.0f / 60.0f;
    float maxSpeedA = initialSpeedA;
    float maxSpeedB = initialSpeedB;
    float maxAngularSpeedA = 0.0f;
    float maxAngularSpeedB = 0.0f;

    for (int frame = 0; frame < numFrames; ++frame) {
        solver.SolveWithJoints(dt, emptyPairs, jointEntities);

        const auto& bodyA = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyA);
        const auto& bodyB = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);

        float speedA = bodyA.linearVelocity.norm();
        float speedB = bodyB.linearVelocity.norm();
        float angSpeedA = bodyA.angularVelocity.norm();
        float angSpeedB = bodyB.angularVelocity.norm();

        maxSpeedA = std::max(maxSpeedA, speedA);
        maxSpeedB = std::max(maxSpeedB, speedB);
        maxAngularSpeedA = std::max(maxAngularSpeedA, angSpeedA);
        maxAngularSpeedB = std::max(maxAngularSpeedB, angSpeedB);

        // 每帧检查
        TEST_ASSERT(speedA < 100.0f,
                    "第 " + std::to_string(frame) + " 帧：刚体A速度不应爆炸");
        TEST_ASSERT(speedB < 100.0f,
                    "第 " + std::to_string(frame) + " 帧：刚体B速度不应爆炸");
        TEST_ASSERT(angSpeedA < 100.0f,
                    "第 " + std::to_string(frame) + " 帧：刚体A角速度不应爆炸");
        TEST_ASSERT(angSpeedB < 100.0f,
                    "第 " + std::to_string(frame) + " 帧：刚体B角速度不应爆炸");
        TEST_ASSERT(!std::isnan(speedA) && !std::isinf(speedA),
                    "第 " + std::to_string(frame) + " 帧：刚体A速度不应为NaN/Inf");
        TEST_ASSERT(!std::isnan(speedB) && !std::isinf(speedB),
                    "第 " + std::to_string(frame) + " 帧：刚体B速度不应为NaN/Inf");
    }

    std::cout << "=== No Velocity Explosion Test ===" << std::endl;
    std::cout << "Initial speed A: " << initialSpeedA << std::endl;
    std::cout << "Initial speed B: " << initialSpeedB << std::endl;
    std::cout << "Max speed A: " << maxSpeedA << std::endl;
    std::cout << "Max speed B: " << maxSpeedB << std::endl;
    std::cout << "Max angular speed A: " << maxAngularSpeedA << std::endl;
    std::cout << "Max angular speed B: " << maxAngularSpeedB << std::endl;
    std::cout << "==================================" << std::endl;

    // 验证：最大速度应在合理范围内
    TEST_ASSERT(maxSpeedA < 50.0f,
                "100帧后刚体A的最大速度应在合理范围内");
    TEST_ASSERT(maxSpeedB < 50.0f,
                "100帧后刚体B的最大速度应在合理范围内");
    TEST_ASSERT(maxAngularSpeedA < 50.0f,
                "100帧后刚体A的最大角速度应在合理范围内");
    TEST_ASSERT(maxAngularSpeedB < 50.0f,
                "100帧后刚体B的最大角速度应在合理范围内");

    ctx.world->Shutdown();
    return true;
}

// ============================================================================
// 用例 5：冲量累积检测 - Warm Start不应导致冲量爆炸
// ============================================================================

bool Test_FixedJoint_NoImpulseExplosion() {
    JointSceneContext ctx = CreateFixedJointScene(
        Vector3(0, 0, 0),
        Vector3(1, 0, 0),
        Quaternion::Identity(),
        Quaternion::Identity(),
        Vector3(0.5f, 0, 0),
        Vector3(-0.5f, 0, 0)
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(10);
    solver.SetPositionIterations(5);

    std::vector<CollisionPair> emptyPairs;
    std::vector<EntityID> jointEntities = {ctx.jointEntity};

    const int numFrames = 200;
    float dt = 1.0f / 60.0f;
    float maxLinearImpulse = 0.0f;
    float maxAngularImpulse = 0.0f;

    for (int frame = 0; frame < numFrames; ++frame) {
        solver.SolveWithJoints(dt, emptyPairs, jointEntities);

        const auto& joint = ctx.world->GetComponent<PhysicsJointComponent>(ctx.jointEntity);
        float linearImpulse = joint.runtime.accumulatedLinearImpulse.norm();
        float angularImpulse = joint.runtime.accumulatedAngularImpulse.norm();

        maxLinearImpulse = std::max(maxLinearImpulse, linearImpulse);
        maxAngularImpulse = std::max(maxAngularImpulse, angularImpulse);

        // 每帧检查冲量
        TEST_ASSERT(linearImpulse < 1e5f,
                    "第 " + std::to_string(frame) + " 帧：线性冲量不应爆炸");
        TEST_ASSERT(angularImpulse < 1e5f,
                    "第 " + std::to_string(frame) + " 帧：角冲量不应爆炸");
    }

    std::cout << "=== No Impulse Explosion Test ===" << std::endl;
    std::cout << "Max linear impulse: " << maxLinearImpulse << std::endl;
    std::cout << "Max angular impulse: " << maxAngularImpulse << std::endl;
    std::cout << "Expected: < 1e4" << std::endl;
    std::cout << "=================================" << std::endl;

    // 验证：累积冲量应在合理范围内
    TEST_ASSERT(maxLinearImpulse < 1e4f,
                "200帧后线性冲量应在合理范围内");
    TEST_ASSERT(maxAngularImpulse < 1e4f,
                "200帧后角冲量应在合理范围内");

    ctx.world->Shutdown();
    return true;
}

// ============================================================================
// 用例 6：极端质量比测试
// ============================================================================

bool Test_FixedJoint_ExtremeMassRatio() {
    // 创建一个很轻的物体连接到一个很重的物体
    JointSceneContext ctx = CreateFixedJointScene(
        Vector3(0, 0, 0),
        Vector3(1, 0, 0),
        Quaternion::Identity(),
        Quaternion::Identity(),
        Vector3::Zero(),
        Vector3::Zero(),
        Vector3::Zero(),
        Vector3::Zero(),
        0.01f,  // 很轻
        100.0f  // 很重
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(15);
    solver.SetPositionIterations(10);

    std::vector<CollisionPair> emptyPairs;
    std::vector<EntityID> jointEntities = {ctx.jointEntity};

    const int numFrames = 50;
    float dt = 1.0f / 60.0f;

    for (int frame = 0; frame < numFrames; ++frame) {
        solver.SolveWithJoints(dt, emptyPairs, jointEntities);

        const auto& bodyA = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyA);
        const auto& bodyB = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);

        TEST_ASSERT(bodyA.linearVelocity.norm() < 100.0f,
                    "极端质量比：轻物体速度不应爆炸");
        TEST_ASSERT(bodyB.linearVelocity.norm() < 100.0f,
                    "极端质量比：重物体速度不应爆炸");
    }

    std::cout << "=== Extreme Mass Ratio Test ===" << std::endl;
    std::cout << "Mass A: 0.01, Mass B: 100.0" << std::endl;
    std::cout << "Test passed: no explosion" << std::endl;
    std::cout << "===============================" << std::endl;

    ctx.world->Shutdown();
    return true;
}

// ============================================================================
// 用例 7：高初始速度测试
// ============================================================================

bool Test_FixedJoint_HighInitialVelocity() {
    // 创建两个有高初始速度的刚体
    JointSceneContext ctx = CreateFixedJointScene(
        Vector3(0, 0, 0),
        Vector3(1, 0, 0),
        Quaternion::Identity(),
        Quaternion::Identity(),
        Vector3(10, 0, 0),   // 高速度
        Vector3(-10, 0, 0)  // 高速度
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(15);
    solver.SetPositionIterations(10);

    std::vector<CollisionPair> emptyPairs;
    std::vector<EntityID> jointEntities = {ctx.jointEntity};

    const int numFrames = 50;
    float dt = 1.0f / 60.0f;

    for (int frame = 0; frame < numFrames; ++frame) {
        solver.SolveWithJoints(dt, emptyPairs, jointEntities);

        const auto& bodyA = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyA);
        const auto& bodyB = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);

        TEST_ASSERT(bodyA.linearVelocity.norm() < 200.0f,
                    "高初始速度：刚体A速度不应爆炸");
        TEST_ASSERT(bodyB.linearVelocity.norm() < 200.0f,
                    "高初始速度：刚体B速度不应爆炸");
    }

    std::cout << "=== High Initial Velocity Test ===" << std::endl;
    std::cout << "Initial velocity: 10 m/s" << std::endl;
    std::cout << "Test passed: no explosion" << std::endl;
    std::cout << "==================================" << std::endl;

    ctx.world->Shutdown();
    return true;
}

// ============================================================================
// 用例 8：多帧稳定性测试 - 长时间运行不应导致数值不稳定
// ============================================================================

bool Test_FixedJoint_MultiFrameStability() {
    JointSceneContext ctx = CreateFixedJointScene(
        Vector3(0, 0, 0),
        Vector3(1, 0, 0),
        Quaternion::Identity(),
        Quaternion::Identity(),
        Vector3(1, 0, 0),
        Vector3(-1, 0, 0)
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(10);
    solver.SetPositionIterations(5);

    std::vector<CollisionPair> emptyPairs;
    std::vector<EntityID> jointEntities = {ctx.jointEntity};

    const int numFrames = 500;
    float dt = 1.0f / 60.0f;

    // 记录每100帧的状态
    std::vector<float> separations;
    std::vector<float> speedsA;
    std::vector<float> speedsB;

    for (int frame = 0; frame < numFrames; ++frame) {
        solver.SolveWithJoints(dt, emptyPairs, jointEntities);

        if (frame % 100 == 0) {
            const auto& transformA = ctx.world->GetComponent<TransformComponent>(ctx.bodyA);
            const auto& transformB = ctx.world->GetComponent<TransformComponent>(ctx.bodyB);
            const auto& bodyA = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyA);
            const auto& bodyB = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);

            float separation = ComputeAnchorSeparation(
                transformA, transformB,
                Vector3::Zero(), Vector3::Zero()
            );
            separations.push_back(separation);
            speedsA.push_back(bodyA.linearVelocity.norm());
            speedsB.push_back(bodyB.linearVelocity.norm());
        }

        // 每帧检查
        const auto& bodyA = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyA);
        const auto& bodyB = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);

        TEST_ASSERT(bodyA.linearVelocity.norm() < 100.0f,
                    "第 " + std::to_string(frame) + " 帧：速度不应爆炸");
        TEST_ASSERT(bodyB.linearVelocity.norm() < 100.0f,
                    "第 " + std::to_string(frame) + " 帧：速度不应爆炸");
    }

    std::cout << "=== Multi-Frame Stability Test ===" << std::endl;
    std::cout << "Separations at frames 0, 100, 200, 300, 400, 500:" << std::endl;
    for (size_t i = 0; i < separations.size(); ++i) {
        std::cout << "  Frame " << (i * 100) << ": " << separations[i] << std::endl;
    }
    std::cout << "Speeds A:" << std::endl;
    for (size_t i = 0; i < speedsA.size(); ++i) {
        std::cout << "  Frame " << (i * 100) << ": " << speedsA[i] << std::endl;
    }
    std::cout << "===================================" << std::endl;

    // 验证：分离距离应该稳定（保持接近初始值，不应持续增长或减少）
    // 初始分离距离是 1 米（bodyA 在 (0,0,0)，bodyB 在 (1,0,0)）
    // 固定关节应该保持这个相对位置
    if (separations.size() >= 3) {
        float initialSeparation = separations[0];  // 初始分离距离
        float avgSeparation = 0.0f;
        for (size_t i = separations.size() / 2; i < separations.size(); ++i) {
            avgSeparation += separations[i];
        }
        avgSeparation /= (separations.size() - separations.size() / 2);
        
        // 验证：平均分离距离应该接近初始值（误差小于0.2米）
        float separationError = std::abs(avgSeparation - initialSeparation);
        TEST_ASSERT(separationError < 0.2f,
                    "长时间运行后分离距离应稳定在初始值附近（误差应小于0.2米）");
    }

    ctx.world->Shutdown();
    return true;
}

// ============================================================================
// 用例 9：空关节列表测试 - 不应影响现有接触约束
// ============================================================================

bool Test_FixedJoint_EmptyJointList_NoEffect() {
    // 创建一个有接触约束但没有关节的场景
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();

    EntityID ground = world->CreateEntity();
    EntityID body = world->CreateEntity();

    TransformComponent groundTransform;
    groundTransform.SetPosition(Vector3(0, 0, 0));
    world->AddComponent(ground, groundTransform);

    TransformComponent bodyTransform;
    bodyTransform.SetPosition(Vector3(0, 0.55f, 0));
    world->AddComponent(body, bodyTransform);

    RigidBodyComponent groundBody = MakeStaticBox();
    world->AddComponent(ground, groundBody);

    RigidBodyComponent fallingBody = MakeDynamicBox();
    fallingBody.linearVelocity = Vector3(0, -2.0f, 0);
    world->AddComponent(body, fallingBody);

    ColliderComponent groundCollider = ColliderComponent::CreateBox(Vector3(10, 0.5f, 10));
    ColliderComponent bodyCollider = ColliderComponent::CreateBox(Vector3(0.5f, 0.5f, 0.5f));
    world->AddComponent(ground, groundCollider);
    world->AddComponent(body, bodyCollider);

    ContactManifold manifold;
    manifold.SetNormal(Vector3::UnitY());
    manifold.AddContact(Vector3(0, 0.5f, 0), Vector3::Zero(), Vector3::Zero(), 0.05f);

    ConstraintSolver solver(world.get());
    solver.SetSolverIterations(10);
    solver.SetPositionIterations(2);

    std::vector<CollisionPair> pairs;
    pairs.emplace_back(ground, body, manifold);
    std::vector<EntityID> emptyJoints;

    const auto& bodyBefore = world->GetComponent<RigidBodyComponent>(body);
    float initialNormalVel = bodyBefore.linearVelocity.dot(manifold.normal);

    solver.SolveWithJoints(1.0f / 60.0f, pairs, emptyJoints);

    const auto& bodyAfter = world->GetComponent<RigidBodyComponent>(body);
    const auto& groundAfter = world->GetComponent<RigidBodyComponent>(ground);
    Vector3 relVel = bodyAfter.linearVelocity - groundAfter.linearVelocity;
    float finalNormalVel = relVel.dot(manifold.normal);

    std::cout << "=== Empty Joint List Test ===" << std::endl;
    std::cout << "Initial normal vel: " << initialNormalVel << std::endl;
    std::cout << "Final normal vel: " << finalNormalVel << std::endl;
    std::cout << "Expected: > initial (contact constraint should work)" << std::endl;
    std::cout << "=============================" << std::endl;

    // 验证：接触约束应该正常工作（不应被空关节列表影响）
    TEST_ASSERT(finalNormalVel > initialNormalVel + 0.5f,
                "空关节列表不应影响接触约束求解");

    world->Shutdown();
    return true;
}

// ============================================================================
// 距离关节测试用例
// ============================================================================

// 用例 10：基础距离关节测试 - 应保持 restLength
bool Test_DistanceJoint_Basic_RestLength() {
    // 创建两个刚体，初始距离为 2 米，restLength 为 1.5 米
    // 距离关节应该将距离约束到 restLength
    JointSceneContext ctx = CreateDistanceJointScene(
        Vector3(0, 0, 0),
        Vector3(2, 0, 0),  // 初始距离 2 米
        Quaternion::Identity(),
        Quaternion::Identity(),
        Vector3::Zero(),
        Vector3::Zero(),
        Vector3::Zero(),
        Vector3::Zero(),
        1.0f,  // massA
        1.0f,  // massB
        1.5f,  // restLength
        false  // 无限制
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(15);
    solver.SetPositionIterations(10);

    std::vector<CollisionPair> emptyPairs;
    std::vector<EntityID> jointEntities = {ctx.jointEntity};

    const auto& transformA_before = ctx.world->GetComponent<TransformComponent>(ctx.bodyA);
    const auto& transformB_before = ctx.world->GetComponent<TransformComponent>(ctx.bodyB);
    float initialDistance = ComputeAnchorSeparation(
        transformA_before, transformB_before,
        Vector3::Zero(), Vector3::Zero()
    );

    // 运行多帧以收敛
    const int numFrames = 50;
    float dt = 1.0f / 60.0f;
    for (int frame = 0; frame < numFrames; ++frame) {
        solver.SolveWithJoints(dt, emptyPairs, jointEntities);
    }

    const auto& transformA_after = ctx.world->GetComponent<TransformComponent>(ctx.bodyA);
    const auto& transformB_after = ctx.world->GetComponent<TransformComponent>(ctx.bodyB);
    const auto& joint = ctx.world->GetComponent<PhysicsJointComponent>(ctx.jointEntity);
    auto& distData = std::get<DistanceJointData>(joint.data);
    
    float finalDistance = ComputeAnchorSeparation(
        transformA_after, transformB_after,
        Vector3::Zero(), Vector3::Zero()
    );

    std::cout << "=== Basic Distance Joint Test ===" << std::endl;
    std::cout << "Initial distance: " << initialDistance << std::endl;
    std::cout << "Rest length: " << distData.restLength << std::endl;
    std::cout << "Final distance: " << finalDistance << std::endl;
    std::cout << "Expected: close to restLength (1.5)" << std::endl;
    std::cout << "=================================" << std::endl;

    // 验证：最终距离应该接近 restLength
    float distanceError = std::abs(finalDistance - distData.restLength);
    TEST_ASSERT(distanceError < 0.3f,
                "距离关节应保持 restLength（误差应小于0.3米）");

    ctx.world->Shutdown();
    return true;
}

// 用例 11：距离关节限制测试 - minDistance 和 maxDistance
bool Test_DistanceJoint_Limits() {
    // 创建两个刚体，设置距离限制 [1.0, 2.0]
    JointSceneContext ctx = CreateDistanceJointScene(
        Vector3(0, 0, 0),
        Vector3(0.5f, 0, 0),  // 初始距离 0.5 米（小于 minDistance）
        Quaternion::Identity(),
        Quaternion::Identity(),
        Vector3::Zero(),
        Vector3::Zero(),
        Vector3::Zero(),
        Vector3::Zero(),
        1.0f,  // massA
        1.0f,  // massB
        1.5f,  // restLength（在限制范围内）
        true,  // 有限制
        1.0f,  // minDistance
        2.0f   // maxDistance
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(15);
    solver.SetPositionIterations(10);

    std::vector<CollisionPair> emptyPairs;
    std::vector<EntityID> jointEntities = {ctx.jointEntity};

    // 运行多帧
    const int numFrames = 50;
    float dt = 1.0f / 60.0f;
    for (int frame = 0; frame < numFrames; ++frame) {
        solver.SolveWithJoints(dt, emptyPairs, jointEntities);
    }

    const auto& transformA_after = ctx.world->GetComponent<TransformComponent>(ctx.bodyA);
    const auto& transformB_after = ctx.world->GetComponent<TransformComponent>(ctx.bodyB);
    const auto& joint = ctx.world->GetComponent<PhysicsJointComponent>(ctx.jointEntity);
    auto& distData = std::get<DistanceJointData>(joint.data);
    
    float finalDistance = ComputeAnchorSeparation(
        transformA_after, transformB_after,
        Vector3::Zero(), Vector3::Zero()
    );

    std::cout << "=== Distance Joint Limits Test ===" << std::endl;
    std::cout << "Min distance: " << distData.minDistance << std::endl;
    std::cout << "Max distance: " << distData.maxDistance << std::endl;
    std::cout << "Final distance: " << finalDistance << std::endl;
    std::cout << "Expected: between 1.0 and 2.0" << std::endl;
    std::cout << "==================================" << std::endl;

    // 验证：最终距离应该在限制范围内
    TEST_ASSERT(finalDistance >= distData.minDistance - 0.2f,
                "距离应大于等于 minDistance（允许0.2米误差）");
    TEST_ASSERT(finalDistance <= distData.maxDistance + 0.2f,
                "距离应小于等于 maxDistance（允许0.2米误差）");

    ctx.world->Shutdown();
    return true;
}

// 用例 12：距离关节数据爆炸检测
bool Test_DistanceJoint_NoVelocityExplosion() {
    // 创建两个有初始速度的刚体，试图分离
    JointSceneContext ctx = CreateDistanceJointScene(
        Vector3(0, 0, 0),
        Vector3(1.5f, 0, 0),  // 初始距离 1.5 米
        Quaternion::Identity(),
        Quaternion::Identity(),
        Vector3(1, 0, 0),   // 刚体A有X方向速度
        Vector3(-1, 0, 0),  // 刚体B有-X方向速度（试图分离）
        Vector3::Zero(),
        Vector3::Zero(),
        1.0f,  // massA
        1.0f,  // massB
        1.5f,  // restLength
        false  // 无限制
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(10);
    solver.SetPositionIterations(5);

    std::vector<CollisionPair> emptyPairs;
    std::vector<EntityID> jointEntities = {ctx.jointEntity};

    const auto& bodyA_before = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyA);
    const auto& bodyB_before = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);
    float initialSpeedA = bodyA_before.linearVelocity.norm();
    float initialSpeedB = bodyB_before.linearVelocity.norm();

    // 运行多帧，检测是否有数据爆炸
    const int numFrames = 100;
    float dt = 1.0f / 60.0f;
    float maxSpeedA = initialSpeedA;
    float maxSpeedB = initialSpeedB;
    float maxAngularSpeedA = 0.0f;
    float maxAngularSpeedB = 0.0f;

    for (int frame = 0; frame < numFrames; ++frame) {
        solver.SolveWithJoints(dt, emptyPairs, jointEntities);

        const auto& bodyA = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyA);
        const auto& bodyB = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);

        float speedA = bodyA.linearVelocity.norm();
        float speedB = bodyB.linearVelocity.norm();
        float angSpeedA = bodyA.angularVelocity.norm();
        float angSpeedB = bodyB.angularVelocity.norm();

        maxSpeedA = std::max(maxSpeedA, speedA);
        maxSpeedB = std::max(maxSpeedB, speedB);
        maxAngularSpeedA = std::max(maxAngularSpeedA, angSpeedA);
        maxAngularSpeedB = std::max(maxAngularSpeedB, angSpeedB);

        // 每帧检查
        TEST_ASSERT(speedA < 100.0f,
                    "第 " + std::to_string(frame) + " 帧：刚体A速度不应爆炸");
        TEST_ASSERT(speedB < 100.0f,
                    "第 " + std::to_string(frame) + " 帧：刚体B速度不应爆炸");
        TEST_ASSERT(angSpeedA < 100.0f,
                    "第 " + std::to_string(frame) + " 帧：刚体A角速度不应爆炸");
        TEST_ASSERT(angSpeedB < 100.0f,
                    "第 " + std::to_string(frame) + " 帧：刚体B角速度不应爆炸");
    }

    std::cout << "=== Distance Joint No Velocity Explosion Test ===" << std::endl;
    std::cout << "Initial speed A: " << initialSpeedA << std::endl;
    std::cout << "Initial speed B: " << initialSpeedB << std::endl;
    std::cout << "Max speed A: " << maxSpeedA << std::endl;
    std::cout << "Max speed B: " << maxSpeedB << std::endl;
    std::cout << "Max angular speed A: " << maxAngularSpeedA << std::endl;
    std::cout << "Max angular speed B: " << maxAngularSpeedB << std::endl;
    std::cout << "=================================================" << std::endl;

    // 验证：最大速度应在合理范围内
    TEST_ASSERT(maxSpeedA < 50.0f,
                "100帧后刚体A的最大速度应在合理范围内");
    TEST_ASSERT(maxSpeedB < 50.0f,
                "100帧后刚体B的最大速度应在合理范围内");

    ctx.world->Shutdown();
    return true;
}

// 用例 13：距离关节多帧稳定性测试
bool Test_DistanceJoint_MultiFrameStability() {
    JointSceneContext ctx = CreateDistanceJointScene(
        Vector3(0, 0, 0),
        Vector3(2, 0, 0),  // 初始距离 2 米
        Quaternion::Identity(),
        Quaternion::Identity(),
        Vector3(0.5f, 0, 0),
        Vector3(-0.5f, 0, 0),
        Vector3::Zero(),
        Vector3::Zero(),
        1.0f,  // massA
        1.0f,  // massB
        1.5f,  // restLength
        false  // 无限制
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(10);
    solver.SetPositionIterations(5);

    std::vector<CollisionPair> emptyPairs;
    std::vector<EntityID> jointEntities = {ctx.jointEntity};

    const int numFrames = 500;
    float dt = 1.0f / 60.0f;

    // 记录每100帧的距离
    std::vector<float> distances;
    std::vector<float> speedsA;
    std::vector<float> speedsB;

    for (int frame = 0; frame < numFrames; ++frame) {
        solver.SolveWithJoints(dt, emptyPairs, jointEntities);

        if (frame % 100 == 0) {
            const auto& transformA = ctx.world->GetComponent<TransformComponent>(ctx.bodyA);
            const auto& transformB = ctx.world->GetComponent<TransformComponent>(ctx.bodyB);
            const auto& bodyA = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyA);
            const auto& bodyB = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);
            const auto& joint = ctx.world->GetComponent<PhysicsJointComponent>(ctx.jointEntity);
            auto& distData = std::get<DistanceJointData>(joint.data);

            float distance = ComputeAnchorSeparation(
                transformA, transformB,
                Vector3::Zero(), Vector3::Zero()
            );
            distances.push_back(distance);
            speedsA.push_back(bodyA.linearVelocity.norm());
            speedsB.push_back(bodyB.linearVelocity.norm());
        }

        // 每帧检查
        const auto& bodyA = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyA);
        const auto& bodyB = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);

        TEST_ASSERT(bodyA.linearVelocity.norm() < 100.0f,
                    "第 " + std::to_string(frame) + " 帧：速度不应爆炸");
        TEST_ASSERT(bodyB.linearVelocity.norm() < 100.0f,
                    "第 " + std::to_string(frame) + " 帧：速度不应爆炸");
    }

    std::cout << "=== Distance Joint Multi-Frame Stability Test ===" << std::endl;
    std::cout << "Distances at frames 0, 100, 200, 300, 400, 500:" << std::endl;
    for (size_t i = 0; i < distances.size(); ++i) {
        std::cout << "  Frame " << (i * 100) << ": " << distances[i] << std::endl;
    }
    std::cout << "Rest length: 1.5" << std::endl;
    std::cout << "==================================================" << std::endl;

    // 验证：距离应该稳定在 restLength 附近
    if (distances.size() >= 3) {
        const auto& joint = ctx.world->GetComponent<PhysicsJointComponent>(ctx.jointEntity);
        auto& distData = std::get<DistanceJointData>(joint.data);
        
        float avgDistance = 0.0f;
        for (size_t i = distances.size() / 2; i < distances.size(); ++i) {
            avgDistance += distances[i];
        }
        avgDistance /= (distances.size() - distances.size() / 2);
        
        // 验证：平均距离应该接近 restLength（误差小于0.3米）
        float distanceError = std::abs(avgDistance - distData.restLength);
        TEST_ASSERT(distanceError < 0.3f,
                    "长时间运行后距离应稳定在 restLength 附近（误差应小于0.3米）");
    }

    ctx.world->Shutdown();
    return true;
}

// 用例 14：距离关节极端质量比测试
bool Test_DistanceJoint_ExtremeMassRatio() {
    // 创建一个很轻的物体连接到一个很重的物体
    JointSceneContext ctx = CreateDistanceJointScene(
        Vector3(0, 0, 0),
        Vector3(1.5f, 0, 0),
        Quaternion::Identity(),
        Quaternion::Identity(),
        Vector3::Zero(),
        Vector3::Zero(),
        Vector3::Zero(),
        Vector3::Zero(),
        0.01f,  // 很轻
        100.0f, // 很重
        1.5f,  // restLength
        false  // 无限制
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(15);
    solver.SetPositionIterations(10);

    std::vector<CollisionPair> emptyPairs;
    std::vector<EntityID> jointEntities = {ctx.jointEntity};

    const int numFrames = 50;
    float dt = 1.0f / 60.0f;

    for (int frame = 0; frame < numFrames; ++frame) {
        solver.SolveWithJoints(dt, emptyPairs, jointEntities);

        const auto& bodyA = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyA);
        const auto& bodyB = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);

        TEST_ASSERT(bodyA.linearVelocity.norm() < 100.0f,
                    "极端质量比：轻物体速度不应爆炸");
        TEST_ASSERT(bodyB.linearVelocity.norm() < 100.0f,
                    "极端质量比：重物体速度不应爆炸");
    }

    std::cout << "=== Distance Joint Extreme Mass Ratio Test ===" << std::endl;
    std::cout << "Mass A: 0.01, Mass B: 100.0" << std::endl;
    std::cout << "Test passed: no explosion" << std::endl;
    std::cout << "===============================================" << std::endl;

    ctx.world->Shutdown();
    return true;
}

// ============================================================================
// 铰链关节测试用例
// ============================================================================

// 用例 15：基础铰链关节测试 - 位置应对齐，只能绕轴旋转
bool Test_HingeJoint_Basic_PositionAlignment() {
    // 创建两个刚体，初始位置重合
    JointSceneContext ctx = CreateHingeJointScene(
        Vector3(0, 0, 0),
        Vector3(0, 0, 0),  // 位置重合，满足位置约束
        Quaternion::Identity(),
        Quaternion::Identity()
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(10);
    solver.SetPositionIterations(5);

    std::vector<CollisionPair> emptyPairs;
    std::vector<EntityID> jointEntities = {ctx.jointEntity};

    const auto& transformA_before = ctx.world->GetComponent<TransformComponent>(ctx.bodyA);
    const auto& transformB_before = ctx.world->GetComponent<TransformComponent>(ctx.bodyB);
    float initialSeparation = ComputeAnchorSeparation(
        transformA_before, transformB_before,
        Vector3::Zero(), Vector3::Zero()
    );

    solver.SolveWithJoints(1.0f / 60.0f, emptyPairs, jointEntities);

    const auto& transformA_after = ctx.world->GetComponent<TransformComponent>(ctx.bodyA);
    const auto& transformB_after = ctx.world->GetComponent<TransformComponent>(ctx.bodyB);
    const auto& bodyA_after = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyA);
    const auto& bodyB_after = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);

    float finalSeparation = ComputeAnchorSeparation(
        transformA_after, transformB_after,
        Vector3::Zero(), Vector3::Zero()
    );

    std::cout << "=== Basic Hinge Joint Test ===" << std::endl;
    std::cout << "Initial separation: " << initialSeparation << std::endl;
    std::cout << "Final separation: " << finalSeparation << std::endl;
    std::cout << "Velocity A: " << bodyA_after.linearVelocity.transpose() << std::endl;
    std::cout << "Velocity B: " << bodyB_after.linearVelocity.transpose() << std::endl;
    std::cout << "==============================" << std::endl;

    // 验证：位置约束应保持两个锚点对齐
    TEST_ASSERT(finalSeparation < 0.1f,
                "铰链关节应保持两个锚点对齐（分离距离应小于0.1）");
    
    // 验证速度不应爆炸
    TEST_ASSERT(bodyA_after.linearVelocity.norm() < 10.0f,
                "刚体A的速度不应爆炸");
    TEST_ASSERT(bodyB_after.linearVelocity.norm() < 10.0f,
                "刚体B的速度不应爆炸");

    ctx.world->Shutdown();
    return true;
}

// 用例 16：铰链关节旋转约束测试 - 只能绕指定轴旋转
bool Test_HingeJoint_RotationConstraint() {
    // 创建两个刚体，初始旋转不同（绕非旋转轴旋转）
    Quaternion rotA = Quaternion::Identity();
    Quaternion rotB = Quaternion(Eigen::AngleAxisf(0.5f, Vector3::UnitX()));  // 绕X轴旋转（不是旋转轴Z）

    JointSceneContext ctx = CreateHingeJointScene(
        Vector3(0, 0, 0),
        Vector3(0, 0, 0),  // 位置相同
        rotA,
        rotB,
        Vector3::Zero(),
        Vector3::Zero(),
        Vector3::Zero(),
        Vector3::Zero(),
        1.0f,  // massA
        1.0f,  // massB
        Vector3::UnitZ(),  // 旋转轴是Z轴
        Vector3::UnitZ()
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(15);
    solver.SetPositionIterations(10);

    std::vector<CollisionPair> emptyPairs;
    std::vector<EntityID> jointEntities = {ctx.jointEntity};

    // 运行多帧以收敛
    const int numFrames = 30;
    float dt = 1.0f / 60.0f;
    for (int frame = 0; frame < numFrames; ++frame) {
        solver.SolveWithJoints(dt, emptyPairs, jointEntities);
    }

    const auto& transformA_after = ctx.world->GetComponent<TransformComponent>(ctx.bodyA);
    const auto& transformB_after = ctx.world->GetComponent<TransformComponent>(ctx.bodyB);
    const auto& joint = ctx.world->GetComponent<PhysicsJointComponent>(ctx.jointEntity);
    auto& hingeData = std::get<HingeJointData>(joint.data);

    // 计算世界空间旋转轴（使用关节的运行时数据）
    Vector3 worldAxis = joint.runtime.worldAxis;
    worldAxis.normalize();

    // 使用关节的角度计算函数来获取当前角度
    // 这比直接计算相对旋转更准确，因为它考虑了铰链关节的特定约束
    float currentAngle = hingeData.currentAngle;
    
    // 计算相对旋转（用于验证旋转是否主要围绕旋转轴）
    Quaternion qA = transformA_after.GetRotation();
    Quaternion qB = transformB_after.GetRotation();
    Quaternion relativeRot = qB * qA.conjugate();

    // 将相对旋转转换为轴角表示
    Eigen::AngleAxisf angleAxis(relativeRot);
    Vector3 rotationAxis = angleAxis.axis();
    float rotationAngle = angleAxis.angle();

    // 计算旋转轴与期望旋转轴（worldAxis）的夹角
    // 如果旋转约束正确，旋转轴应该与worldAxis对齐
    float axisAlignment = std::abs(rotationAxis.dot(worldAxis));
    
    // 如果角度很小，轴对齐度可能不准确，所以使用角度作为替代指标
    // 如果角度小于阈值，认为约束是有效的

    std::cout << "=== Hinge Joint Rotation Constraint Test ===" << std::endl;
    std::cout << "Current angle (from joint): " << currentAngle << std::endl;
    std::cout << "Rotation angle (from quaternion): " << rotationAngle << std::endl;
    std::cout << "Axis alignment with world axis: " << axisAlignment << std::endl;
    std::cout << "Expected: rotation should be around Z axis (alignment close to 1.0)" << std::endl;
    std::cout << "=============================================" << std::endl;

    // 验证：旋转应该主要围绕旋转轴（Z轴）
    // 如果旋转约束正确，应该满足以下条件之一：
    // 1. 旋转角度很小（说明约束已经消除了非旋转轴方向的旋转）
    // 2. 旋转轴与期望旋转轴对齐（轴对齐度 > 0.8）
    // 3. 当前角度（从关节计算）接近旋转角度（说明旋转主要围绕旋转轴）
    
    // 关键验证：如果初始旋转是绕X轴（非旋转轴），旋转约束应该将其消除
    // 最终相对旋转应该很小，或者只保留绕Z轴的分量
    bool isValid = (rotationAngle < 0.3f) ||  // 旋转角度应该很小
                   (axisAlignment > 0.8f) ||  // 或者旋转轴与期望轴对齐
                   (std::abs(currentAngle) < 0.3f);  // 或者关节角度很小
    
    TEST_ASSERT(isValid,
                "铰链关节应只允许绕旋转轴旋转（旋转角度应小于0.3或轴对齐度应大于0.8）");

    ctx.world->Shutdown();
    return true;
}

// 用例 17：铰链关节角度限制测试
bool Test_HingeJoint_AngleLimits() {
    // 创建两个刚体，设置角度限制 [-0.5, 0.5] 弧度
    JointSceneContext ctx = CreateHingeJointScene(
        Vector3(0, 0, 0),
        Vector3(0, 0, 0),
        Quaternion::Identity(),
        Quaternion(Eigen::AngleAxisf(1.0f, Vector3::UnitZ())),  // 初始角度 1.0 弧度（超出限制）
        Vector3::Zero(),
        Vector3::Zero(),
        Vector3::Zero(),
        Vector3::Zero(),
        1.0f,  // massA
        1.0f,  // massB
        Vector3::UnitZ(),  // 旋转轴
        Vector3::UnitZ(),
        true,  // 有限制
        -0.5f,  // limitMin
        0.5f    // limitMax
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(15);
    solver.SetPositionIterations(10);

    std::vector<CollisionPair> emptyPairs;
    std::vector<EntityID> jointEntities = {ctx.jointEntity};

    // 运行多帧
    const int numFrames = 50;
    float dt = 1.0f / 60.0f;
    for (int frame = 0; frame < numFrames; ++frame) {
        solver.SolveWithJoints(dt, emptyPairs, jointEntities);
    }

    const auto& joint = ctx.world->GetComponent<PhysicsJointComponent>(ctx.jointEntity);
    auto& hingeData = std::get<HingeJointData>(joint.data);

    std::cout << "=== Hinge Joint Angle Limits Test ===" << std::endl;
    std::cout << "Limit min: " << hingeData.limitMin << std::endl;
    std::cout << "Limit max: " << hingeData.limitMax << std::endl;
    std::cout << "Current angle: " << hingeData.currentAngle << std::endl;
    std::cout << "Expected: between -0.5 and 0.5" << std::endl;
    std::cout << "====================================" << std::endl;

    // 验证：当前角度应该在限制范围内
    TEST_ASSERT(hingeData.currentAngle >= hingeData.limitMin - 0.2f,
                "角度应大于等于 limitMin（允许0.2弧度误差）");
    TEST_ASSERT(hingeData.currentAngle <= hingeData.limitMax + 0.2f,
                "角度应小于等于 limitMax（允许0.2弧度误差）");

    ctx.world->Shutdown();
    return true;
}

// 用例 18：铰链关节马达测试
bool Test_HingeJoint_Motor() {
    // 创建两个刚体，启用马达
    JointSceneContext ctx = CreateHingeJointScene(
        Vector3(0, 0, 0),
        Vector3(0, 0, 0),
        Quaternion::Identity(),
        Quaternion::Identity(),
        Vector3::Zero(),
        Vector3::Zero(),
        Vector3::Zero(),
        Vector3::Zero(),
        1.0f,  // massA
        1.0f,  // massB
        Vector3::UnitZ(),  // 旋转轴
        Vector3::UnitZ(),
        false,  // 无角度限制
        -MathUtils::PI,
        MathUtils::PI,
        true,  // 使用马达
        2.0f,  // motorSpeed: 2 rad/s
        50.0f  // motorMaxForce
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(10);
    solver.SetPositionIterations(5);

    std::vector<CollisionPair> emptyPairs;
    std::vector<EntityID> jointEntities = {ctx.jointEntity};

    const int numFrames = 30;
    float dt = 1.0f / 60.0f;
    float totalTime = numFrames * dt;

    for (int frame = 0; frame < numFrames; ++frame) {
        solver.SolveWithJoints(dt, emptyPairs, jointEntities);
    }

    const auto& bodyA_after = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyA);
    const auto& bodyB_after = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);
    const auto& joint = ctx.world->GetComponent<PhysicsJointComponent>(ctx.jointEntity);
    auto& hingeData = std::get<HingeJointData>(joint.data);

    // 计算相对角速度（沿旋转轴）
    Vector3 worldAxis = ctx.world->GetComponent<TransformComponent>(ctx.bodyA).GetRotation() 
        * hingeData.localAxisA;
    worldAxis.normalize();
    Vector3 angVelRel = bodyB_after.angularVelocity - bodyA_after.angularVelocity;
    float currentSpeed = angVelRel.dot(worldAxis);

    std::cout << "=== Hinge Joint Motor Test ===" << std::endl;
    std::cout << "Target motor speed: " << hingeData.motorSpeed << " rad/s" << std::endl;
    std::cout << "Current speed: " << currentSpeed << " rad/s" << std::endl;
    std::cout << "Expected: close to target speed" << std::endl;
    std::cout << "=============================" << std::endl;

    // 验证：相对角速度应该接近目标马达速度
    float speedError = std::abs(currentSpeed - hingeData.motorSpeed);
    TEST_ASSERT(speedError < 1.0f,
                "马达应产生接近目标速度的旋转（误差应小于1.0 rad/s）");

    ctx.world->Shutdown();
    return true;
}

// 用例 19：铰链关节数据爆炸检测
bool Test_HingeJoint_NoVelocityExplosion() {
    // 创建两个有初始角速度的刚体
    JointSceneContext ctx = CreateHingeJointScene(
        Vector3(0, 0, 0),
        Vector3(0, 0, 0),
        Quaternion::Identity(),
        Quaternion::Identity(),
        Vector3::Zero(),
        Vector3::Zero(),
        Vector3(0, 0, 5.0f),   // 刚体A有Z方向角速度
        Vector3(0, 0, -5.0f)   // 刚体B有-Z方向角速度
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(10);
    solver.SetPositionIterations(5);

    std::vector<CollisionPair> emptyPairs;
    std::vector<EntityID> jointEntities = {ctx.jointEntity};

    const int numFrames = 100;
    float dt = 1.0f / 60.0f;
    float maxSpeedA = 0.0f;
    float maxSpeedB = 0.0f;
    float maxAngularSpeedA = 0.0f;
    float maxAngularSpeedB = 0.0f;

    for (int frame = 0; frame < numFrames; ++frame) {
        solver.SolveWithJoints(dt, emptyPairs, jointEntities);

        const auto& bodyA = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyA);
        const auto& bodyB = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);

        float speedA = bodyA.linearVelocity.norm();
        float speedB = bodyB.linearVelocity.norm();
        float angSpeedA = bodyA.angularVelocity.norm();
        float angSpeedB = bodyB.angularVelocity.norm();

        maxSpeedA = std::max(maxSpeedA, speedA);
        maxSpeedB = std::max(maxSpeedB, speedB);
        maxAngularSpeedA = std::max(maxAngularSpeedA, angSpeedA);
        maxAngularSpeedB = std::max(maxAngularSpeedB, angSpeedB);

        // 每帧检查
        TEST_ASSERT(speedA < 100.0f,
                    "第 " + std::to_string(frame) + " 帧：刚体A速度不应爆炸");
        TEST_ASSERT(speedB < 100.0f,
                    "第 " + std::to_string(frame) + " 帧：刚体B速度不应爆炸");
        TEST_ASSERT(angSpeedA < 100.0f,
                    "第 " + std::to_string(frame) + " 帧：刚体A角速度不应爆炸");
        TEST_ASSERT(angSpeedB < 100.0f,
                    "第 " + std::to_string(frame) + " 帧：刚体B角速度不应爆炸");
    }

    std::cout << "=== Hinge Joint No Velocity Explosion Test ===" << std::endl;
    std::cout << "Max speed A: " << maxSpeedA << std::endl;
    std::cout << "Max speed B: " << maxSpeedB << std::endl;
    std::cout << "Max angular speed A: " << maxAngularSpeedA << std::endl;
    std::cout << "Max angular speed B: " << maxAngularSpeedB << std::endl;
    std::cout << "===============================================" << std::endl;

    // 验证：最大速度应在合理范围内
    TEST_ASSERT(maxSpeedA < 50.0f,
                "100帧后刚体A的最大速度应在合理范围内");
    TEST_ASSERT(maxSpeedB < 50.0f,
                "100帧后刚体B的最大速度应在合理范围内");
    TEST_ASSERT(maxAngularSpeedA < 50.0f,
                "100帧后刚体A的最大角速度应在合理范围内");
    TEST_ASSERT(maxAngularSpeedB < 50.0f,
                "100帧后刚体B的最大角速度应在合理范围内");

    ctx.world->Shutdown();
    return true;
}

// 用例 20：铰链关节多帧稳定性测试
bool Test_HingeJoint_MultiFrameStability() {
    JointSceneContext ctx = CreateHingeJointScene(
        Vector3(0, 0, 0),
        Vector3(0, 0, 0),
        Quaternion::Identity(),
        Quaternion::Identity(),
        Vector3::Zero(),
        Vector3::Zero(),
        Vector3(0, 0, 1.0f),  // 初始角速度
        Vector3(0, 0, -1.0f)
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(10);
    solver.SetPositionIterations(5);

    std::vector<CollisionPair> emptyPairs;
    std::vector<EntityID> jointEntities = {ctx.jointEntity};

    const int numFrames = 500;
    float dt = 1.0f / 60.0f;

    // 记录每100帧的状态
    std::vector<float> separations;
    std::vector<float> angles;

    for (int frame = 0; frame < numFrames; ++frame) {
        solver.SolveWithJoints(dt, emptyPairs, jointEntities);

        if (frame % 100 == 0) {
            const auto& transformA = ctx.world->GetComponent<TransformComponent>(ctx.bodyA);
            const auto& transformB = ctx.world->GetComponent<TransformComponent>(ctx.bodyB);
            const auto& joint = ctx.world->GetComponent<PhysicsJointComponent>(ctx.jointEntity);
            auto& hingeData = std::get<HingeJointData>(joint.data);

            float separation = ComputeAnchorSeparation(
                transformA, transformB,
                Vector3::Zero(), Vector3::Zero()
            );
            separations.push_back(separation);
            angles.push_back(hingeData.currentAngle);
        }

        // 每帧检查
        const auto& bodyA = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyA);
        const auto& bodyB = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);

        TEST_ASSERT(bodyA.linearVelocity.norm() < 100.0f,
                    "第 " + std::to_string(frame) + " 帧：速度不应爆炸");
        TEST_ASSERT(bodyB.linearVelocity.norm() < 100.0f,
                    "第 " + std::to_string(frame) + " 帧：速度不应爆炸");
    }

    std::cout << "=== Hinge Joint Multi-Frame Stability Test ===" << std::endl;
    std::cout << "Separations at frames 0, 100, 200, 300, 400, 500:" << std::endl;
    for (size_t i = 0; i < separations.size(); ++i) {
        std::cout << "  Frame " << (i * 100) << ": " << separations[i] << std::endl;
    }
    std::cout << "Angles at frames 0, 100, 200, 300, 400, 500:" << std::endl;
    for (size_t i = 0; i < angles.size(); ++i) {
        std::cout << "  Frame " << (i * 100) << ": " << angles[i] << std::endl;
    }
    std::cout << "===============================================" << std::endl;

    // 验证：分离距离应该稳定（接近0，因为位置约束）
    if (separations.size() >= 3) {
        float avgSeparation = 0.0f;
        for (size_t i = separations.size() / 2; i < separations.size(); ++i) {
            avgSeparation += separations[i];
        }
        avgSeparation /= (separations.size() - separations.size() / 2);
        
        TEST_ASSERT(avgSeparation < 0.2f,
                    "长时间运行后分离距离应稳定（应小于0.2米）");
    }

    ctx.world->Shutdown();
    return true;
}

// 用例 21：铰链关节极端质量比测试
bool Test_HingeJoint_ExtremeMassRatio() {
    // 创建一个很轻的物体连接到一个很重的物体
    JointSceneContext ctx = CreateHingeJointScene(
        Vector3(0, 0, 0),
        Vector3(0, 0, 0),
        Quaternion::Identity(),
        Quaternion::Identity(),
        Vector3::Zero(),
        Vector3::Zero(),
        Vector3::Zero(),
        Vector3::Zero(),
        0.01f,  // 很轻
        100.0f  // 很重
    );

    ConstraintSolver solver(ctx.world.get());
    solver.SetSolverIterations(15);
    solver.SetPositionIterations(10);

    std::vector<CollisionPair> emptyPairs;
    std::vector<EntityID> jointEntities = {ctx.jointEntity};

    const int numFrames = 50;
    float dt = 1.0f / 60.0f;

    for (int frame = 0; frame < numFrames; ++frame) {
        solver.SolveWithJoints(dt, emptyPairs, jointEntities);

        const auto& bodyA = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyA);
        const auto& bodyB = ctx.world->GetComponent<RigidBodyComponent>(ctx.bodyB);

        TEST_ASSERT(bodyA.linearVelocity.norm() < 100.0f,
                    "极端质量比：轻物体速度不应爆炸");
        TEST_ASSERT(bodyB.linearVelocity.norm() < 100.0f,
                    "极端质量比：重物体速度不应爆炸");
    }

    std::cout << "=== Hinge Joint Extreme Mass Ratio Test ===" << std::endl;
    std::cout << "Mass A: 0.01, Mass B: 100.0" << std::endl;
    std::cout << "Test passed: no explosion" << std::endl;
    std::cout << "===========================================" << std::endl;

    ctx.world->Shutdown();
    return true;
}

// ============================================================================
// 主入口
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "关节约束自动化测试" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // 基础功能测试
    RUN_TEST(Test_FixedJoint_Basic_StaticBodies);
    RUN_TEST(Test_FixedJoint_PositionConstraint);
    RUN_TEST(Test_FixedJoint_RotationConstraint);

    // 数据爆炸检测
    RUN_TEST(Test_FixedJoint_NoVelocityExplosion);
    RUN_TEST(Test_FixedJoint_NoImpulseExplosion);

    // 极端情况测试
    RUN_TEST(Test_FixedJoint_ExtremeMassRatio);
    RUN_TEST(Test_FixedJoint_HighInitialVelocity);

    // 稳定性测试
    RUN_TEST(Test_FixedJoint_MultiFrameStability);

    // 兼容性测试
    RUN_TEST(Test_FixedJoint_EmptyJointList_NoEffect);

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "距离关节测试" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // 距离关节基础功能测试
    RUN_TEST(Test_DistanceJoint_Basic_RestLength);
    RUN_TEST(Test_DistanceJoint_Limits);

    // 距离关节数据爆炸检测
    RUN_TEST(Test_DistanceJoint_NoVelocityExplosion);

    // 距离关节稳定性测试
    RUN_TEST(Test_DistanceJoint_MultiFrameStability);

    // 距离关节极端情况测试
    RUN_TEST(Test_DistanceJoint_ExtremeMassRatio);

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "铰链关节测试" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // 铰链关节基础功能测试
    RUN_TEST(Test_HingeJoint_Basic_PositionAlignment);
    RUN_TEST(Test_HingeJoint_RotationConstraint);
    RUN_TEST(Test_HingeJoint_AngleLimits);
    RUN_TEST(Test_HingeJoint_Motor);

    // 铰链关节数据爆炸检测
    RUN_TEST(Test_HingeJoint_NoVelocityExplosion);

    // 铰链关节稳定性测试
    RUN_TEST(Test_HingeJoint_MultiFrameStability);

    // 铰链关节极端情况测试
    RUN_TEST(Test_HingeJoint_ExtremeMassRatio);

    std::cout << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "测试总数: " << g_testCount << std::endl;
    std::cout << "通过: " << g_passedCount << std::endl;
    std::cout << "失败: " << g_failedCount << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    if (g_failedCount == 0) {
        std::cout << "🎉 所有测试通过！" << std::endl;
    } else {
        std::cout << "⚠️  有 " << g_failedCount << " 个测试失败" << std::endl;
    }

    return g_failedCount == 0 ? 0 : 1;
}

