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
#include "render/physics/dynamics/joint_component.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/math_utils.h"
#include <algorithm>
#include <limits>
#include <cmath>

namespace Render {
namespace Physics {

namespace {
constexpr float kBaumgarte = 0.2f;
constexpr float kAllowedPenetration = 0.01f;
constexpr float kRestitutionVelocityThreshold = 1.0f;
constexpr float kContactMatchThresholdSq = 1e-4f;
constexpr float kCFM = 1e-8f;  // CFM (Constraint Force Mixing) 软化，防止有效质量接近零
constexpr float kMaxTimeStep = 1.0f / 30.0f;  // 最大时间步长 33ms
constexpr float kImpulseDecayFactor = 0.95f;  // Warm Start 冲量衰减因子
constexpr float kMaxCachedImpulse = 1e4f;  // 最大缓存冲量（超过视为异常）
constexpr float kMaxVelocity = 100.0f;  // 最大速度 100 m/s，防止爆炸
constexpr float kMaxAcceleration = 100.0f * 9.81f;  // 最大加速度 100g

/**
 * @brief 计算自适应 Baumgarte 系数
 * @param penetration 穿透深度
 * @param dt 时间步长
 * @return 自适应的 beta 值
 */
float ComputeAdaptiveBaumgarte(float penetration, float dt) {
    // 浅穿透使用标准值，深穿透降低β避免过度修正
    if (penetration < 0.1f) return 0.2f;
    if (penetration > 0.5f) return 0.1f;
    return MathUtils::Lerp(0.2f, 0.1f, (penetration - 0.1f) / 0.4f);
}

/**
 * @brief 计算最大允许冲量（基于质量和时间步）
 * @param body 刚体
 * @param dt 时间步长
 * @return 最大冲量值
 */
float ComputeMaxImpulse(const RigidBodyComponent& body, float dt) {
    float mass = body.inverseMass > MathUtils::EPSILON ? 1.0f / body.inverseMass : 1e10f;
    return mass * kMaxAcceleration * dt;
}

/**
 * @brief 检查并修正速度爆炸
 * @param body 刚体
 * @return 是否发生了速度爆炸
 */
bool CheckAndClampVelocity(RigidBodyComponent* body) {
    if (!body || body->IsStatic() || body->IsKinematic()) {
        return false;
    }
    
    bool hadExplosion = false;
    
    // 检查线速度
    float linearSpeed = body->linearVelocity.norm();
    if (linearSpeed > kMaxVelocity) {
        body->linearVelocity *= (kMaxVelocity / linearSpeed);
        hadExplosion = true;
    }
    
    // 检查角速度（限制为每秒10圈）
    constexpr float kMaxAngularVelocity = 2.0f * MathUtils::PI * 10.0f;
    float angularSpeed = body->angularVelocity.norm();
    if (angularSpeed > kMaxAngularVelocity) {
        body->angularVelocity *= (kMaxAngularVelocity / angularSpeed);
        hadExplosion = true;
    }
    
    return hadExplosion;
}

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

    // 时间步长保护：如果dt过大，分解为多个子步
    if (dt > kMaxTimeStep) {
        int subSteps = static_cast<int>(std::ceil(dt / kMaxTimeStep));
        float subDt = dt / static_cast<float>(subSteps);
        for (int i = 0; i < subSteps; ++i) {
            SolveInternal(subDt, pairs);
        }
        return;
    }

    SolveInternal(dt, pairs);
}

void ConstraintSolver::SolveInternal(float dt, const std::vector<CollisionPair>& pairs) {
    Clear();
    PrepareConstraints(dt, pairs);
    WarmStart();
    SolveVelocityConstraints();
    SolvePositionConstraints(dt);
    CacheImpulses();
    
    // 速度爆炸检测与修正
    bool hadExplosion = false;
    for (auto& constraint : m_contactConstraints) {
        if (CheckAndClampVelocity(constraint.bodyA)) {
            hadExplosion = true;
        }
        if (CheckAndClampVelocity(constraint.bodyB)) {
            hadExplosion = true;
        }
    }
    
    // 如果发生速度爆炸，清空所有缓存冲量（下一帧从头开始）
    if (hadExplosion) {
        m_cachedImpulses.clear();
    }
}

void ConstraintSolver::Clear() {
    m_contactConstraints.clear();
    m_jointConstraints.clear();
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

            // 有效质量：包含线性和角项，添加CFM软化防止除零
            Vector3 rnA = point.rA.cross(point.normal);
            Vector3 rnB = point.rB.cross(point.normal);
            float invMass = bodyA.inverseMass + bodyB.inverseMass
                + rnA.dot(constraint.invInertiaA * rnA)
                + rnB.dot(constraint.invInertiaB * rnB)
                + kCFM;  // CFM软化，防止有效质量接近零
            point.normalMass = 1.0f / invMass;

            // 切向有效质量，添加CFM软化
            Vector3 rtA1 = point.rA.cross(point.tangent1);
            Vector3 rtB1 = point.rB.cross(point.tangent1);
            float invMassT1 = bodyA.inverseMass + bodyB.inverseMass
                + rtA1.dot(constraint.invInertiaA * rtA1)
                + rtB1.dot(constraint.invInertiaB * rtB1)
                + kCFM;
            point.tangentMass[0] = 1.0f / invMassT1;

            Vector3 rtA2 = point.rA.cross(point.tangent2);
            Vector3 rtB2 = point.rB.cross(point.tangent2);
            float invMassT2 = bodyA.inverseMass + bodyB.inverseMass
                + rtA2.dot(constraint.invInertiaA * rtA2)
                + rtB2.dot(constraint.invInertiaB * rtB2)
                + kCFM;
            point.tangentMass[1] = 1.0f / invMassT2;

            // 自适应 Baumgarte stabilization for position correction
            float penetrationDepth = std::max(0.0f, point.penetration - kAllowedPenetration);
            float adaptiveBeta = ComputeAdaptiveBaumgarte(penetrationDepth, dt);
            point.bias = (adaptiveBeta * penetrationDepth) / std::max(dt, MathUtils::EPSILON);

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
                    // 应用衰减因子，防止过时冲量累积
                    float decayedNormal = cachedPoint.normalImpulse * kImpulseDecayFactor;
                    float decayedTangent1 = cachedPoint.tangentImpulse[0] * kImpulseDecayFactor;
                    float decayedTangent2 = cachedPoint.tangentImpulse[1] * kImpulseDecayFactor;
                    
                    // 重置异常值
                    if (std::abs(decayedNormal) < kMaxCachedImpulse) {
                        point.normalImpulse = std::max(0.0f, decayedNormal);
                    }
                    if (std::abs(decayedTangent1) < kMaxCachedImpulse) {
                        point.tangentImpulse[0] = decayedTangent1;
                    }
                    if (std::abs(decayedTangent2) < kMaxCachedImpulse) {
                        point.tangentImpulse[1] = decayedTangent2;
                    }
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
            Vector3 torqueA = point.rA.cross(totalImpulse);
            bodyA->angularVelocity -= constraint.invInertiaA * torqueA;

            bodyB->linearVelocity += totalImpulse * bodyB->inverseMass;
            Vector3 torqueB = point.rB.cross(totalImpulse);
            bodyB->angularVelocity += constraint.invInertiaB * torqueB;

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

                // 冲量钳位，防止数值爆炸
                float maxImpulseA = ComputeMaxImpulse(*bodyA, 1.0f / 60.0f);  // 假设60fps
                float maxImpulseB = ComputeMaxImpulse(*bodyB, 1.0f / 60.0f);
                float maxImpulse = std::max(maxImpulseA, maxImpulseB);
                lambdaN = MathUtils::Clamp(lambdaN, -maxImpulse, maxImpulse);

                float oldNormalImpulse = point.normalImpulse;
                point.normalImpulse = std::max(oldNormalImpulse + lambdaN, 0.0f);
                float appliedNormal = point.normalImpulse - oldNormalImpulse;

                Vector3 impulseN = appliedNormal * point.normal;
                bodyA->linearVelocity -= impulseN * bodyA->inverseMass;
                Vector3 torqueNA = point.rA.cross(impulseN);
                bodyA->angularVelocity -= constraint.invInertiaA * torqueNA;
                bodyB->linearVelocity += impulseN * bodyB->inverseMass;
                Vector3 torqueNB = point.rB.cross(impulseN);
                bodyB->angularVelocity += constraint.invInertiaB * torqueNB;

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
                Vector3 torqueT1A = point.rA.cross(impulseT1);
                bodyA->angularVelocity -= constraint.invInertiaA * torqueT1A;
                bodyB->linearVelocity += impulseT1 * bodyB->inverseMass;
                Vector3 torqueT1B = point.rB.cross(impulseT1);
                bodyB->angularVelocity += constraint.invInertiaB * torqueT1B;

                // 切向 2
                float tangentRelVel2 = relativeVel.dot(point.tangent2);
                float lambdaT2 = -tangentRelVel2 * point.tangentMass[1];
                float oldT2 = point.tangentImpulse[1];
                point.tangentImpulse[1] = MathUtils::Clamp(oldT2 + lambdaT2, -maxFriction, maxFriction);
                float appliedT2 = point.tangentImpulse[1] - oldT2;

                Vector3 impulseT2 = appliedT2 * point.tangent2;
                bodyA->linearVelocity -= impulseT2 * bodyA->inverseMass;
                Vector3 torqueT2A = point.rA.cross(impulseT2);
                bodyA->angularVelocity -= constraint.invInertiaA * torqueT2A;
                bodyB->linearVelocity += impulseT2 * bodyB->inverseMass;
                Vector3 torqueT2B = point.rB.cross(impulseT2);
                bodyB->angularVelocity += constraint.invInertiaB * torqueT2B;
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
                    Vector3 negativeImpulse = -correctionImpulse;
                    Vector3 torqueA = point.rA.cross(negativeImpulse);
                    Vector3 angularDelta = constraint.invInertiaA * torqueA;
                    Vector3 newPos = constraint.transformA->GetPosition() + linearDelta;
                    constraint.transformA->SetPosition(newPos);
                    ApplyAngularCorrection(constraint.transformA, angularDelta);
                }

                if (!bodyB->IsStatic() && !bodyB->IsKinematic()) {
                    Vector3 linearDelta = correctionImpulse * bodyB->inverseMass;
                    Vector3 torqueB = point.rB.cross(correctionImpulse);
                    Vector3 angularDelta = constraint.invInertiaB * torqueB;
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

void ConstraintSolver::SolveWithJoints(
    float dt,
    const std::vector<CollisionPair>& pairs,
    const std::vector<ECS::EntityID>& jointEntities
) {
    if (!m_world) {
        return;
    }

    // 时间步长保护：如果dt过大，分解为多个子步
    if (dt > kMaxTimeStep) {
        int subSteps = static_cast<int>(std::ceil(dt / kMaxTimeStep));
        float subDt = dt / static_cast<float>(subSteps);
        for (int i = 0; i < subSteps; ++i) {
            // 递归调用，但使用子时间步
            SolveWithJoints(subDt, pairs, jointEntities);
        }
        return;
    }

    // 内部实现：直接调用求解逻辑
    Clear();
    
    // 准备约束
    PrepareConstraints(dt, pairs);
    PrepareJointConstraints(dt, jointEntities);
    
    // Warm Start
    WarmStart();
    WarmStartJoints();
    
    // 速度约束迭代（交替求解接触和关节）
    for (int i = 0; i < m_solverIterations; ++i) {
        SolveVelocityConstraints();
        SolveJointVelocityConstraints(dt);
    }
    
    // 位置修正
    SolvePositionConstraints(dt);
    SolveJointPositionConstraints(dt);
    
    // 缓存冲量
    CacheImpulses();
    CacheJointImpulses();
    
    // 检测断裂
    CheckJointBreakage(dt);
    
    // 速度爆炸检测与修正
    bool hadExplosion = false;
    for (auto& constraint : m_contactConstraints) {
        if (CheckAndClampVelocity(constraint.bodyA)) {
            hadExplosion = true;
        }
        if (CheckAndClampVelocity(constraint.bodyB)) {
            hadExplosion = true;
        }
    }
    for (auto& constraint : m_jointConstraints) {
        if (CheckAndClampVelocity(constraint.bodyA)) {
            hadExplosion = true;
        }
        if (CheckAndClampVelocity(constraint.bodyB)) {
            hadExplosion = true;
        }
    }
    
    // 如果发生速度爆炸，清空所有缓存冲量（下一帧从头开始）
    if (hadExplosion) {
        m_cachedImpulses.clear();
        for (auto& constraint : m_jointConstraints) {
            if (constraint.joint) {
                constraint.joint->runtime.accumulatedLinearImpulse = Vector3::Zero();
                constraint.joint->runtime.accumulatedAngularImpulse = Vector3::Zero();
                constraint.joint->runtime.accumulatedLimitImpulse = 0.0f;
                constraint.joint->runtime.accumulatedMotorImpulse = 0.0f;
            }
        }
    }
}


void ConstraintSolver::PrepareJointConstraints(
    float dt,
    const std::vector<ECS::EntityID>& jointEntities
) {
    m_jointConstraints.clear();
    m_jointConstraints.reserve(jointEntities.size());
    
    for (auto entityID : jointEntities) {
        if (!m_world->HasComponent<PhysicsJointComponent>(entityID)) {
            continue;
        }
        
        auto& jointComp = m_world->GetComponent<PhysicsJointComponent>(entityID);
        
        // 跳过禁用或已断裂的关节
        if (!jointComp.base.isEnabled || jointComp.base.isBroken) {
            continue;
        }
        
        auto& base = jointComp.base;
        auto connectedBody = base.connectedBody;
        
        // 验证连接的刚体存在
        if (!m_world->IsValidEntity(entityID) || 
            !m_world->IsValidEntity(connectedBody)) {
            continue;
        }
        
        if (!m_world->HasComponent<RigidBodyComponent>(entityID) ||
            !m_world->HasComponent<RigidBodyComponent>(connectedBody) ||
            !m_world->HasComponent<ECS::TransformComponent>(entityID) ||
            !m_world->HasComponent<ECS::TransformComponent>(connectedBody)) {
            continue;
        }
        
        auto& bodyA = m_world->GetComponent<RigidBodyComponent>(entityID);
        auto& bodyB = m_world->GetComponent<RigidBodyComponent>(connectedBody);
        
        // 静态-静态不需要求解
        if ((bodyA.IsStatic() || bodyA.IsKinematic()) &&
            (bodyB.IsStatic() || bodyB.IsKinematic())) {
            continue;
        }
        
        JointConstraint constraint;
        constraint.jointEntity = entityID;
        constraint.entityA = entityID;
        constraint.entityB = connectedBody;
        constraint.joint = &jointComp;
        constraint.bodyA = &bodyA;
        constraint.bodyB = &bodyB;
        constraint.transformA = &m_world->GetComponent<ECS::TransformComponent>(entityID);
        constraint.transformB = &m_world->GetComponent<ECS::TransformComponent>(connectedBody);
        
        // 预计算世界空间逆惯性张量
        jointComp.runtime.invInertiaA = ComputeWorldInvInertia(
            bodyA, constraint.transformA->GetRotation());
        jointComp.runtime.invInertiaB = ComputeWorldInvInertia(
            bodyB, constraint.transformB->GetRotation());
        
        // 计算世界空间锚点和相对向量
        Vector3 comA = constraint.transformA->GetPosition() + bodyA.centerOfMass;
        Vector3 comB = constraint.transformB->GetPosition() + bodyB.centerOfMass;
        Vector3 worldAnchorA = constraint.transformA->GetPosition() 
            + constraint.transformA->GetRotation() * base.localAnchorA;
        Vector3 worldAnchorB = constraint.transformB->GetPosition() 
            + constraint.transformB->GetRotation() * base.localAnchorB;
        jointComp.runtime.rA = worldAnchorA - comA;
        jointComp.runtime.rB = worldAnchorB - comB;
        
        m_jointConstraints.push_back(constraint);
    }
}

void ConstraintSolver::WarmStartJoints() {
    constexpr float decayFactor = 0.95f;
    constexpr float maxCachedImpulse = 1e4f;
    
    for (auto& constraint : m_jointConstraints) {
        auto& joint = *constraint.joint;
        auto& bodyA = *constraint.bodyA;
        auto& bodyB = *constraint.bodyB;
        
        // 衰减因子（防止过时冲量累积）
        joint.runtime.accumulatedLinearImpulse *= decayFactor;
        joint.runtime.accumulatedAngularImpulse *= decayFactor;
        
        // 重置异常值
        if (joint.runtime.accumulatedLinearImpulse.norm() > maxCachedImpulse) {
            joint.runtime.accumulatedLinearImpulse = Vector3::Zero();
        }
        if (joint.runtime.accumulatedAngularImpulse.norm() > maxCachedImpulse) {
            joint.runtime.accumulatedAngularImpulse = Vector3::Zero();
        }
        
        // 应用缓存的线性冲量
        Vector3 linearImpulse = joint.runtime.accumulatedLinearImpulse;
        bodyA.linearVelocity -= linearImpulse * bodyA.inverseMass;
        bodyB.linearVelocity += linearImpulse * bodyB.inverseMass;
        
        // 应用缓存的角冲量
        Vector3 angularImpulse = joint.runtime.accumulatedAngularImpulse;
        bodyA.angularVelocity -= joint.runtime.invInertiaA * angularImpulse;
        bodyB.angularVelocity += joint.runtime.invInertiaB * angularImpulse;
    }
}

void ConstraintSolver::SolveJointVelocityConstraints(float dt) {
    // 目前为空实现，后续阶段会添加具体关节类型的求解逻辑
    // 阶段1.3将实现Fixed Joint，阶段1.4将实现Distance Joint
}

void ConstraintSolver::SolveJointPositionConstraints(float dt) {
    // 目前为空实现，后续阶段会添加具体关节类型的位置修正逻辑
}

void ConstraintSolver::CacheJointImpulses() {
    // 冲量已在求解过程中累积，无需额外操作
    // 如果需要衰减，可以在此处理
}

void ConstraintSolver::CheckJointBreakage(float dt) {
    if (dt <= MathUtils::EPSILON) {
        return;
    }
    
    for (auto& constraint : m_jointConstraints) {
        auto& joint = *constraint.joint;
        
        // 计算本帧施加的力（从累积冲量估算）
        // 注意：这里使用简化的估算，实际应该记录每帧的力
        Vector3 force = joint.runtime.accumulatedLinearImpulse / dt;
        Vector3 torque = joint.runtime.accumulatedAngularImpulse / dt;
        
        if (force.norm() > joint.base.breakForce ||
            torque.norm() > joint.base.breakTorque) {
            joint.base.isBroken = true;
            
            // 触发断裂事件（如果需要，可以在这里添加事件系统调用）
            // m_world->GetEventBus()->Emit<JointBrokenEvent>(...);
        }
    }
}

}  // namespace Physics
}  // namespace Render

