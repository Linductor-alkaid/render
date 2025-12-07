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
#include <limits>

namespace Render {
namespace Physics {

namespace {
constexpr float kBaumgarte = 0.2f;
constexpr float kAllowedPenetration = 0.01f;
constexpr float kRestitutionVelocityThreshold = 1.0f;
constexpr float kContactMatchThresholdSq = 1e-4f;
}  // namespace

Matrix3 ConstraintSolver::ComputeWorldInvInertia(const RigidBodyComponent& body, const Quaternion& rotation) {
    Matrix3 rotationMatrix = rotation.toRotationMatrix();
    return rotationMatrix * body.inverseInertiaTensor * rotationMatrix.transpose();
}

Vector3 ConstraintSolver::ChooseTangent(const Vector3& normal) {
    Vector3 tangent = normal.cross(Vector3::UnitX());
    if (tangent.squaredNorm() < 1e-4f) {
        tangent = normal.cross(Vector3::UnitY());
    }
    return tangent.normalized();
}

int ConstraintSolver::FindMatchingContact(
    const ConstraintSolver::ContactConstraintPoint& point,
    const ConstraintSolver::CachedContactManifold& cache
) {
    int bestIndex = -1;
    float bestDistSq = std::numeric_limits<float>::max();

    for (int i = 0; i < cache.contactCount; ++i) {
        const auto& cached = cache.points[i];
        float distSq =
            (point.localPointA - cached.localPointA).squaredNorm() +
            (point.localPointB - cached.localPointB).squaredNorm();

        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestIndex = i;
        }
    }

    if (bestIndex >= 0 && bestDistSq <= kContactMatchThresholdSq) {
        return bestIndex;
    }
    return -1;
}

void ConstraintSolver::SetSolverIterations(int iterations) {
    m_solverIterations = std::max(0, iterations);
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
    CacheImpulses();
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

        uint64_t pairHash = HashPair(constraint.entityA, constraint.entityB);
        auto cacheIt = m_cachedImpulses.find(pairHash);

        for (int i = 0; i < constraint.contactCount && i < static_cast<int>(pair.manifold.contacts.size()); ++i) {
            const auto& cp = pair.manifold.contacts[i];
            auto& point = constraint.points[i];

            point.normal = constraint.normal;
            point.tangent1 = ChooseTangent(constraint.normal);
            point.tangent2 = point.normal.cross(point.tangent1);
            point.localPointA = cp.localPointA;
            point.localPointB = cp.localPointB;

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

            // Baumgarte stabilization for position correction
            float penetrationDepth = std::max(0.0f, point.penetration - kAllowedPenetration);
            point.bias = (kBaumgarte * penetrationDepth) / std::max(dt, MathUtils::EPSILON);

            // Restitution: calculate bounce velocity target
            Vector3 vA = bodyA.linearVelocity + bodyA.angularVelocity.cross(point.rA);
            Vector3 vB = bodyB.linearVelocity + bodyB.angularVelocity.cross(point.rB);
            float relativeNormalVel = (vB - vA).dot(point.normal);
            
            if (point.restitution > MathUtils::EPSILON && relativeNormalVel < -kRestitutionVelocityThreshold) {
                // 仅在存在恢复系数时计算反弹：目标速度 -(1+e)*vn
                point.restitutionBias = -(1.0f + point.restitution) * relativeNormalVel;
            } else {
                point.restitutionBias = 0.0f;
            }

            // Initialize impulses (will be overwritten by warm start if cache exists)
            point.normalImpulse = 0.0f;
            point.tangentImpulse[0] = 0.0f;
            point.tangentImpulse[1] = 0.0f;
            
            if (cacheIt != m_cachedImpulses.end()) {
                int matchIndex = FindMatchingContact(point, cacheIt->second);
                if (matchIndex >= 0) {
                    const auto& cachedPoint = cacheIt->second.points[matchIndex];
                    point.normalImpulse = std::max(0.0f, cachedPoint.normalImpulse);
                    point.tangentImpulse[0] = cachedPoint.tangentImpulse[0];
                    point.tangentImpulse[1] = cachedPoint.tangentImpulse[1];
                }
            }
        }

        m_contactConstraints.push_back(constraint);
    }
}

void ConstraintSolver::WarmStart() {
    // 仅当有实际缓存冲量时才应用
    // 这避免了0冲量时的无意义计算
    for (auto& constraint : m_contactConstraints) {
        auto* bodyA = constraint.bodyA;
        auto* bodyB = constraint.bodyB;

        for (int i = 0; i < constraint.contactCount; ++i) {
            auto& point = constraint.points[i];

            // 跳过零冲量点，避免浮点误差累积
            if (std::abs(point.normalImpulse) < MathUtils::EPSILON &&
                std::abs(point.tangentImpulse[0]) < MathUtils::EPSILON &&
                std::abs(point.tangentImpulse[1]) < MathUtils::EPSILON) {
                continue;
            }

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

                // 法向冲量：使用预计算的 restitutionBias 而不是动态计算
                float normalRelVel = relativeVel.dot(point.normal);
                // 约束方程: C = vn + bias - v_target, 其中 v_target = restitutionBias
                // 使用 point.bias（Baumgarte位置修正）减去 point.restitutionBias（弹性恢复目标速度）
                float lambdaN = -(normalRelVel + point.bias - point.restitutionBias) * point.normalMass;

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

    auto ApplyAngularCorrection = [](ECS::TransformComponent* transform, const Vector3& angularDelta) {
        float angle = angularDelta.norm();
        if (angle <= MathUtils::EPSILON || !transform) {
            return;
        }
        Vector3 axis = angularDelta / angle;
        Quaternion delta(Eigen::AngleAxisf(angle, axis));
        Quaternion newRotation = (delta * transform->GetRotation()).normalized();
        transform->SetRotation(newRotation);
    };

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

                float positionalLambda = beta * penetrationDepth * point.normalMass;
                Vector3 correctionImpulse = positionalLambda * point.normal;

                if (!bodyA->IsStatic() && !bodyA->IsKinematic()) {
                    Vector3 linearDelta = correctionImpulse * (-bodyA->inverseMass);
                    Vector3 angularDelta = constraint.invInertiaA * (point.rA.cross(-correctionImpulse));
                    Vector3 newPos = constraint.transformA->GetPosition() + linearDelta;
                    constraint.transformA->SetPosition(newPos);
                    ApplyAngularCorrection(constraint.transformA, angularDelta);
                }

                if (!bodyB->IsStatic() && !bodyB->IsKinematic()) {
                    Vector3 linearDelta = correctionImpulse * bodyB->inverseMass;
                    Vector3 angularDelta = constraint.invInertiaB * (point.rB.cross(correctionImpulse));
                    Vector3 newPos = constraint.transformB->GetPosition() + linearDelta;
                    constraint.transformB->SetPosition(newPos);
                    ApplyAngularCorrection(constraint.transformB, angularDelta);
                }
            }
        }
    }
}

void ConstraintSolver::CacheImpulses() {
    m_cachedImpulses.clear();
    for (const auto& constraint : m_contactConstraints) {
        uint64_t hash = HashPair(constraint.entityA, constraint.entityB);
        CachedContactManifold cache;
        cache.contactCount = constraint.contactCount;

        for (int i = 0; i < constraint.contactCount; ++i) {
            const auto& point = constraint.points[i];
            auto& cachedPoint = cache.points[i];
            cachedPoint.localPointA = point.localPointA;
            cachedPoint.localPointB = point.localPointB;
            cachedPoint.normalImpulse = point.normalImpulse;
            cachedPoint.tangentImpulse[0] = point.tangentImpulse[0];
            cachedPoint.tangentImpulse[1] = point.tangentImpulse[1];
        }

        m_cachedImpulses[hash] = cache;
    }
}

uint64_t ConstraintSolver::HashPair(ECS::EntityID a, ECS::EntityID b) const {
    if (a.index > b.index) {
        std::swap(a, b);
    }
    return (static_cast<uint64_t>(a.index) << 32) | static_cast<uint64_t>(b.index);
}

}  // namespace Physics
}  // namespace Render

