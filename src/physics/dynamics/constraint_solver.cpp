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
#include "render/physics/dynamics/constraint_solver.h"

#include "render/physics/physics_systems.h"
#include "render/physics/physics_components.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/math_utils.h"
#include <algorithm>

namespace Render {
namespace Physics {

namespace {
constexpr float kBaumgarte = 0.2f;
constexpr float kAllowedPenetration = 0.01f;
constexpr float kRestitutionVelocityThreshold = 1.0f;

Matrix3 ComputeWorldInvInertia(const RigidBodyComponent& body, const Quaternion& rotation) {
    Matrix3 rotationMatrix = rotation.toRotationMatrix();
    return rotationMatrix * body.inverseInertiaTensor * rotationMatrix.transpose();
}

Vector3 ChooseTangent(const Vector3& normal) {
    Vector3 tangent = normal.cross(Vector3::UnitX());
    if (tangent.squaredNorm() < 1e-4f) {
        tangent = normal.cross(Vector3::UnitY());
    }
    return tangent.normalized();
}
}  // namespace

void ConstraintSolver::SetSolverIterations(int iterations) {
    m_solverIterations = std::max(1, iterations);
}

void ConstraintSolver::SetPositionIterations(int iterations) {
    m_positionIterations = std::max(1, iterations);
}

void ConstraintSolver::Solve(float dt, const std::vector<CollisionPair>& pairs) {
    if (!m_world || pairs.empty()) {
        return;
    }

    Clear();
    PrepareConstraints(dt, pairs);
    WarmStart();
    SolveVelocityConstraints();
    SolvePositionConstraints(dt);
}

void ConstraintSolver::Clear() {
    m_contactConstraints.clear();
}

void ConstraintSolver::PrepareConstraints(float dt, const std::vector<CollisionPair>& pairs) {
    m_contactConstraints.clear();
    m_contactConstraints.reserve(pairs.size());

    for (const auto& pair : pairs) {
        if (!m_world->HasComponent<ColliderComponent>(pair.entityA) ||
            !m_world->HasComponent<ColliderComponent>(pair.entityB)) {
            continue;
        }

        auto& colliderA = m_world->GetComponent<ColliderComponent>(pair.entityA);
        auto& colliderB = m_world->GetComponent<ColliderComponent>(pair.entityB);

        // 触发器不参与解算
        if (colliderA.isTrigger || colliderB.isTrigger) {
            continue;
        }

        if (!m_world->HasComponent<RigidBodyComponent>(pair.entityA) ||
            !m_world->HasComponent<RigidBodyComponent>(pair.entityB) ||
            !m_world->HasComponent<ECS::TransformComponent>(pair.entityA) ||
            !m_world->HasComponent<ECS::TransformComponent>(pair.entityB)) {
            continue;
        }

        auto& bodyA = m_world->GetComponent<RigidBodyComponent>(pair.entityA);
        auto& bodyB = m_world->GetComponent<RigidBodyComponent>(pair.entityB);

        // 静态/运动学与静态/运动学不需要解算
        if ((bodyA.IsStatic() || bodyA.IsKinematic()) &&
            (bodyB.IsStatic() || bodyB.IsKinematic())) {
            continue;
        }

        auto& transformA = m_world->GetComponent<ECS::TransformComponent>(pair.entityA);
        auto& transformB = m_world->GetComponent<ECS::TransformComponent>(pair.entityB);

        ContactConstraint constraint;
        constraint.entityA = pair.entityA;
        constraint.entityB = pair.entityB;
        constraint.bodyA = &bodyA;
        constraint.bodyB = &bodyB;
        constraint.transformA = &transformA;
        constraint.transformB = &transformB;
        constraint.normal = pair.manifold.normal;
        constraint.contactCount = pair.manifold.contactCount;
        constraint.invInertiaA = ComputeWorldInvInertia(bodyA, transformA.GetRotation());
        constraint.invInertiaB = ComputeWorldInvInertia(bodyB, transformB.GetRotation());

        // 组合摩擦、恢复系数
        PhysicsMaterial matA = colliderA.material ? *colliderA.material : PhysicsMaterial::Default();
        PhysicsMaterial matB = colliderB.material ? *colliderB.material : PhysicsMaterial::Default();
        float frictionA = PhysicsMaterial::CombineValues(matA.friction, matB.friction, matA.frictionCombine);
        float frictionB = PhysicsMaterial::CombineValues(matA.friction, matB.friction, matB.frictionCombine);
        float restitutionA = PhysicsMaterial::CombineValues(matA.restitution, matB.restitution, matA.restitutionCombine);
        float restitutionB = PhysicsMaterial::CombineValues(matA.restitution, matB.restitution, matB.restitutionCombine);
        float combinedFriction = 0.5f * (frictionA + frictionB);
        float combinedRestitution = 0.5f * (restitutionA + restitutionB);

        for (int i = 0; i < constraint.contactCount && i < static_cast<int>(pair.manifold.contacts.size()); ++i) {
            const auto& cp = pair.manifold.contacts[i];
            auto& point = constraint.points[i];

            point.normal = constraint.normal;
            point.tangent1 = ChooseTangent(constraint.normal);
            point.tangent2 = point.normal.cross(point.tangent1);

            // 接触向量（相对质心）
            Vector3 worldPoint = cp.position;
            Vector3 comA = transformA.GetPosition() + bodyA.centerOfMass;
            Vector3 comB = transformB.GetPosition() + bodyB.centerOfMass;
            point.rA = worldPoint - comA;
            point.rB = worldPoint - comB;
            point.penetration = cp.penetration;
            point.friction = combinedFriction;
            point.restitution = combinedRestitution;

            // 有效质量：包含线性和角项
            Vector3 rnA = point.rA.cross(point.normal);
            Vector3 rnB = point.rB.cross(point.normal);
            float invMass = bodyA.inverseMass + bodyB.inverseMass
                + rnA.dot(constraint.invInertiaA * rnA)
                + rnB.dot(constraint.invInertiaB * rnB);
            point.normalMass = (invMass > MathUtils::EPSILON) ? 1.0f / invMass : 0.0f;

            // 切向有效质量
            Vector3 rtA1 = point.rA.cross(point.tangent1);
            Vector3 rtB1 = point.rB.cross(point.tangent1);
            float invMassT1 = bodyA.inverseMass + bodyB.inverseMass
                + rtA1.dot(constraint.invInertiaA * rtA1)
                + rtB1.dot(constraint.invInertiaB * rtB1);
            point.tangentMass[0] = (invMassT1 > MathUtils::EPSILON) ? 1.0f / invMassT1 : 0.0f;

            Vector3 rtA2 = point.rA.cross(point.tangent2);
            Vector3 rtB2 = point.rB.cross(point.tangent2);
            float invMassT2 = bodyA.inverseMass + bodyB.inverseMass
                + rtA2.dot(constraint.invInertiaA * rtA2)
                + rtB2.dot(constraint.invInertiaB * rtB2);
            point.tangentMass[1] = (invMassT2 > MathUtils::EPSILON) ? 1.0f / invMassT2 : 0.0f;

            // Baumgarte 偏置
            float penetrationDepth = std::max(0.0f, point.penetration - kAllowedPenetration);
            point.bias = -kBaumgarte * penetrationDepth / std::max(dt, MathUtils::EPSILON);

            // 恢复系数偏置
            Vector3 vA = bodyA.linearVelocity + bodyA.angularVelocity.cross(point.rA);
            Vector3 vB = bodyB.linearVelocity + bodyB.angularVelocity.cross(point.rB);
            float relativeNormalVel = (vB - vA).dot(point.normal);
            if (relativeNormalVel < -kRestitutionVelocityThreshold) {
                point.restitutionBias = -point.restitution * relativeNormalVel;
            } else {
                point.restitutionBias = 0.0f;
            }
        }

        m_contactConstraints.push_back(constraint);
    }
}

void ConstraintSolver::WarmStart() {
    for (auto& constraint : m_contactConstraints) {
        auto* bodyA = constraint.bodyA;
        auto* bodyB = constraint.bodyB;

        for (int i = 0; i < constraint.contactCount; ++i) {
            auto& point = constraint.points[i];

            Vector3 normalImpulse = point.normal * point.normalImpulse;
            Vector3 tangentImpulse1 = point.tangent1 * point.tangentImpulse[0];
            Vector3 tangentImpulse2 = point.tangent2 * point.tangentImpulse[1];
            Vector3 totalImpulse = normalImpulse + tangentImpulse1 + tangentImpulse2;

            // 应用到速度
            bodyA->linearVelocity -= totalImpulse * bodyA->inverseMass;
            bodyA->angularVelocity -= constraint.invInertiaA * (point.rA.cross(totalImpulse));

            bodyB->linearVelocity += totalImpulse * bodyB->inverseMass;
            bodyB->angularVelocity += constraint.invInertiaB * (point.rB.cross(totalImpulse));
        }
    }
}

void ConstraintSolver::SolveVelocityConstraints() {
    for (int iteration = 0; iteration < m_solverIterations; ++iteration) {
        for (auto& constraint : m_contactConstraints) {
            auto* bodyA = constraint.bodyA;
            auto* bodyB = constraint.bodyB;

            for (int i = 0; i < constraint.contactCount; ++i) {
                auto& point = constraint.points[i];

                Vector3 vA = bodyA->linearVelocity;
                Vector3 wA = bodyA->angularVelocity;
                Vector3 vB = bodyB->linearVelocity;
                Vector3 wB = bodyB->angularVelocity;

                Vector3 relativeVel = (vB + wB.cross(point.rB)) - (vA + wA.cross(point.rA));

                // 法向冲量
                float normalRelVel = relativeVel.dot(point.normal);
                float lambdaN = -(normalRelVel + point.bias + point.restitutionBias) * point.normalMass;

                float oldNormalImpulse = point.normalImpulse;
                point.normalImpulse = std::max(oldNormalImpulse + lambdaN, 0.0f);
                float appliedNormal = point.normalImpulse - oldNormalImpulse;

                Vector3 impulseN = appliedNormal * point.normal;
                bodyA->linearVelocity -= impulseN * bodyA->inverseMass;
                bodyA->angularVelocity -= constraint.invInertiaA * (point.rA.cross(impulseN));
                bodyB->linearVelocity += impulseN * bodyB->inverseMass;
                bodyB->angularVelocity += constraint.invInertiaB * (point.rB.cross(impulseN));

                // 更新相对速度用于摩擦
                vA = bodyA->linearVelocity;
                wA = bodyA->angularVelocity;
                vB = bodyB->linearVelocity;
                wB = bodyB->angularVelocity;
                relativeVel = (vB + wB.cross(point.rB)) - (vA + wA.cross(point.rA));

                float maxFriction = point.friction * point.normalImpulse;

                // 切向 1
                float tangentRelVel1 = relativeVel.dot(point.tangent1);
                float lambdaT1 = -tangentRelVel1 * point.tangentMass[0];
                float oldT1 = point.tangentImpulse[0];
                point.tangentImpulse[0] = MathUtils::Clamp(oldT1 + lambdaT1, -maxFriction, maxFriction);
                float appliedT1 = point.tangentImpulse[0] - oldT1;

                Vector3 impulseT1 = appliedT1 * point.tangent1;
                bodyA->linearVelocity -= impulseT1 * bodyA->inverseMass;
                bodyA->angularVelocity -= constraint.invInertiaA * (point.rA.cross(impulseT1));
                bodyB->linearVelocity += impulseT1 * bodyB->inverseMass;
                bodyB->angularVelocity += constraint.invInertiaB * (point.rB.cross(impulseT1));

                // 切向 2
                float tangentRelVel2 = relativeVel.dot(point.tangent2);
                float lambdaT2 = -tangentRelVel2 * point.tangentMass[1];
                float oldT2 = point.tangentImpulse[1];
                point.tangentImpulse[1] = MathUtils::Clamp(oldT2 + lambdaT2, -maxFriction, maxFriction);
                float appliedT2 = point.tangentImpulse[1] - oldT2;

                Vector3 impulseT2 = appliedT2 * point.tangent2;
                bodyA->linearVelocity -= impulseT2 * bodyA->inverseMass;
                bodyA->angularVelocity -= constraint.invInertiaA * (point.rA.cross(impulseT2));
                bodyB->linearVelocity += impulseT2 * bodyB->inverseMass;
                bodyB->angularVelocity += constraint.invInertiaB * (point.rB.cross(impulseT2));
            }
        }
    }
}

void ConstraintSolver::SolvePositionConstraints(float dt) {
    if (dt <= MathUtils::EPSILON) {
        return;
    }

    const float beta = 0.2f;  // 位置校正系数

    for (int iteration = 0; iteration < m_positionIterations; ++iteration) {
        for (auto& constraint : m_contactConstraints) {
            auto* bodyA = constraint.bodyA;
            auto* bodyB = constraint.bodyB;

            if (!constraint.transformA || !constraint.transformB) {
                continue;
            }

            for (int i = 0; i < constraint.contactCount; ++i) {
                auto& point = constraint.points[i];

                float penetrationDepth = std::max(0.0f, point.penetration - kAllowedPenetration);
                if (penetrationDepth <= MathUtils::EPSILON) {
                    continue;
                }

                float positionalLambda = -beta * penetrationDepth * point.normalMass;
                Vector3 correction = positionalLambda * point.normal;

                if (!bodyA->IsStatic() && !bodyA->IsKinematic()) {
                    Vector3 newPos = constraint.transformA->GetPosition() + correction * (-bodyA->inverseMass);
                    constraint.transformA->SetPosition(newPos);
                }

                if (!bodyB->IsStatic() && !bodyB->IsKinematic()) {
                    Vector3 newPos = constraint.transformB->GetPosition() + correction * bodyB->inverseMass;
                    constraint.transformB->SetPosition(newPos);
                }
            }
        }
    }
}

}  // namespace Physics
}  // namespace Render

