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
#pragma once

#include "render/types.h"
#include "render/ecs/entity.h"
#include <array>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace Render {
namespace ECS {
class World;
class TransformComponent;
}  // namespace ECS

namespace Physics {

struct CollisionPair;
struct RigidBodyComponent;
struct ColliderComponent;
struct PhysicsJointComponent;

/**
 * @brief 约束求解器（阶段 4.1 框架）
 *
 * 提供接触约束的预处理、速度/位置迭代接口。
 * 目前实现侧重框架与数据管线，后续可进一步补充高级特性。
 */
class ConstraintSolver {
public:
    ConstraintSolver() = default;
    explicit ConstraintSolver(ECS::World* world) : m_world(world) {}

    void SetWorld(ECS::World* world) { m_world = world; }

    void SetSolverIterations(int iterations);
    void SetPositionIterations(int iterations);

    /**
     * @brief 主入口：对碰撞对求解约束
     * @param dt 固定时间步
     * @param pairs 碰撞对列表
     */
    void Solve(float dt, const std::vector<CollisionPair>& pairs);
    
    /**
     * @brief 带关节的求解接口
     * @param dt 固定时间步
     * @param pairs 碰撞对列表
     * @param jointEntities 关节实体列表
     */
    void SolveWithJoints(
        float dt,
        const std::vector<CollisionPair>& pairs,
        const std::vector<ECS::EntityID>& jointEntities
    );

    /**
     * @brief 清理内部缓存
     */
    void Clear();

private:
    struct ContactConstraintPoint {
        Vector3 rA = Vector3::Zero();
        Vector3 rB = Vector3::Zero();
        Vector3 normal = Vector3::UnitY();
        Vector3 tangent1 = Vector3::UnitX();
        Vector3 tangent2 = Vector3::UnitZ();
        Vector3 localPointA = Vector3::Zero();
        Vector3 localPointB = Vector3::Zero();

        float penetration = 0.0f;
        float friction = 0.5f;
        float restitution = 0.0f;

        float normalMass = 0.0f;
        float tangentMass[2]{0.0f, 0.0f};
        float bias = 0.0f;             // Baumgarte 偏置
        float restitutionBias = 0.0f;  // 恢复系数偏置

        float normalImpulse = 0.0f;
        float tangentImpulse[2]{0.0f, 0.0f};
    };

    struct ContactConstraint {
        ECS::EntityID entityA;
        ECS::EntityID entityB;
        RigidBodyComponent* bodyA = nullptr;
        RigidBodyComponent* bodyB = nullptr;
        ECS::TransformComponent* transformA = nullptr;
        ECS::TransformComponent* transformB = nullptr;

        Matrix3 invInertiaA = Matrix3::Zero();
        Matrix3 invInertiaB = Matrix3::Zero();

        Vector3 normal = Vector3::UnitY();
        int contactCount = 0;
        std::array<ContactConstraintPoint, 4> points{};
    };

    struct CachedContactPoint {
        Vector3 localPointA = Vector3::Zero();
        Vector3 localPointB = Vector3::Zero();
        float normalImpulse = 0.0f;
        float tangentImpulse[2]{0.0f, 0.0f};
    };

    struct CachedContactManifold {
        int contactCount = 0;
        std::array<CachedContactPoint, 4> points{};
    };
    
    // 关节约束数据结构
    struct JointConstraint {
        ECS::EntityID jointEntity;
        ECS::EntityID entityA;
        ECS::EntityID entityB;
        PhysicsJointComponent* joint = nullptr;
        RigidBodyComponent* bodyA = nullptr;
        RigidBodyComponent* bodyB = nullptr;
        ECS::TransformComponent* transformA = nullptr;
        ECS::TransformComponent* transformB = nullptr;
    };

    void PrepareConstraints(float dt, const std::vector<CollisionPair>& pairs);
    void WarmStart();
    void SolveVelocityConstraints();
    void SolvePositionConstraints(float dt);
    void CacheImpulses();
    void SolveInternal(float dt, const std::vector<CollisionPair>& pairs);
    uint64_t HashPair(ECS::EntityID a, ECS::EntityID b) const;
    
    // 关节约束相关
    void PrepareJointConstraints(float dt, const std::vector<ECS::EntityID>& jointEntities);
    void WarmStartJoints();
    void SolveJointVelocityConstraints(float dt);
    void SolveJointPositionConstraints(float dt);
    void CacheJointImpulses();
    void CheckJointBreakage(float dt);
    
    // Fixed Joint 求解函数
    void SolveFixedJointVelocity(JointConstraint& constraint, float dt);
    void SolveFixedJointPosition(JointConstraint& constraint, float dt);
    
    // Distance Joint 求解函数
    void SolveDistanceJointVelocity(JointConstraint& constraint, float dt);
    void SolveDistanceJointPosition(JointConstraint& constraint, float dt);
    
    // Hinge Joint 求解函数
    void SolveHingeJointVelocity(JointConstraint& constraint, float dt);
    void SolveHingeJointPosition(JointConstraint& constraint, float dt);
    
    // 辅助函数
    static float CalculateHingeAngle(const JointConstraint& constraint);

    // 工具函数（保持在类内，便于访问私有类型）
    static Matrix3 ComputeWorldInvInertia(const RigidBodyComponent& body, const Quaternion& rotation);
    static Vector3 ChooseTangent(const Vector3& normal);
    static int FindMatchingContact(const ContactConstraintPoint& point, const CachedContactManifold& cache);

private:
    ECS::World* m_world = nullptr;
    std::vector<ContactConstraint> m_contactConstraints;
    std::unordered_map<uint64_t, CachedContactManifold> m_cachedImpulses;
    std::vector<JointConstraint> m_jointConstraints;
    int m_solverIterations = 10;
    int m_positionIterations = 4;
};

}  // namespace Physics
}  // namespace Render

