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

namespace Render {
namespace ECS {
class World;
class TransformComponent;
}  // namespace ECS

namespace Physics {

struct CollisionPair;
struct RigidBodyComponent;
struct ColliderComponent;

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

    void PrepareConstraints(float dt, const std::vector<CollisionPair>& pairs);
    void WarmStart();
    void SolveVelocityConstraints();
    void SolvePositionConstraints(float dt);

private:
    ECS::World* m_world = nullptr;
    std::vector<ContactConstraint> m_contactConstraints;
    int m_solverIterations = 10;
    int m_positionIterations = 4;
};

}  // namespace Physics
}  // namespace Render

