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
#include "render/physics/dynamics/symplectic_euler_integrator.h"
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
float ComputeAdaptiveBaumgarte(float penetration, float /* dt */) {
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
            
            // 仅在分离速度足够大且存在恢复系数时应用弹性恢复
            // 弹性碰撞公式：v_post = -e * v_pre
            // 其中v_pre是碰撞前的相对法向速度（负数表示接近）
            // v_post是碰撞后的相对法向速度（正数表示分离）
            if (point.restitution > MathUtils::EPSILON && relativeNormalVel < -kRestitutionVelocityThreshold) {
                // 计算碰撞后的目标相对法向速度：v_post = -e * v_pre
                // relativeNormalVel是负数（接近），所以v_post是正数（分离）
                float targetPostVel = -point.restitution * relativeNormalVel;
                
                // 限制目标速度，防止异常大的反弹
                targetPostVel = std::min(targetPostVel, kMaxVelocity * 0.5f);
                
                // restitutionBias存储目标相对速度
                point.restitutionBias = targetPostVel;
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

                // 法向冲量：处理位置修正和弹性恢复
                float normalRelVel = relativeVel.dot(point.normal);
                
                // 计算约束偏差
                // 标准约束：C_dot = vn + bias = 0
                // 对于弹性碰撞，我们希望达到目标相对速度
                float constraintError = normalRelVel + point.bias;
                
                // 如果有弹性恢复，调整约束误差以达到目标速度
                if (point.restitutionBias > MathUtils::EPSILON) {
                    // restitutionBias是目标相对法向速度（正数，分离方向）
                    // 如果当前速度还未达到目标，需要加速
                    // 如果已经超过目标，不应该再加速（防止能量增加）
                    if (normalRelVel < point.restitutionBias) {
                        // 需要加速到目标速度
                        constraintError = normalRelVel - point.restitutionBias + point.bias;
                    } else {
                        // 已经达到或超过目标速度，只使用位置修正（防止能量增加）
                        constraintError = normalRelVel + point.bias;
                    }
                }
                
                // 约束方程: lambda = -C_dot * mass
                float lambdaN = -constraintError * point.normalMass;

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
                
                // 应用速度限制，防止数值爆炸
                CheckAndClampVelocity(bodyA);
                CheckAndClampVelocity(bodyB);

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
            SolveWithJoints(subDt, pairs, jointEntities);
        }
        return;
    }

    // 清空约束
    Clear();
    
    // 准备约束（接触和关节）
    PrepareConstraints(dt, pairs);
    PrepareJointConstraints(dt, jointEntities);
    
    // Warm Start（使用上一帧的累积冲量）
    WarmStart();
    WarmStartJoints();
    
    // 在 Warm Start 之后，重置本帧的累积冲量（将在求解过程中重新累积）
    for (auto& constraint : m_jointConstraints) {
        if (constraint.joint) {
            constraint.joint->runtime.accumulatedLinearImpulse = Vector3::Zero();
            constraint.joint->runtime.accumulatedAngularImpulse = Vector3::Zero();
        }
    }
    
    // 速度约束迭代（交替求解接触和关节）
    for (int i = 0; i < m_solverIterations; ++i) {
        SolveVelocityConstraints();
        SolveJointVelocityConstraints(dt);
    }
    
    // 位置修正（修复位置误差）
    // 注意：在位置修正前，需要重新计算 rA 和 rB（因为速度求解可能改变了方向）
    for (auto& constraint : m_jointConstraints) {
        if (constraint.joint && constraint.transformA && constraint.transformB) {
            auto& bodyA = *constraint.bodyA;
            auto& bodyB = *constraint.bodyB;
            Vector3 comA = constraint.transformA->GetPosition() + bodyA.centerOfMass;
            Vector3 comB = constraint.transformB->GetPosition() + bodyB.centerOfMass;
            Vector3 worldAnchorA = constraint.transformA->GetPosition() 
                + constraint.transformA->GetRotation() * constraint.joint->base.localAnchorA;
            Vector3 worldAnchorB = constraint.transformB->GetPosition() 
                + constraint.transformB->GetRotation() * constraint.joint->base.localAnchorB;
            constraint.joint->runtime.rA = worldAnchorA - comA;
            constraint.joint->runtime.rB = worldAnchorB - comB;
        }
    }
    
    // 接触位置修正
    SolvePositionConstraints(dt);
    
    // 关节位置修正（包括初始错误的额外修正）
    bool needsInitialCorrection = false;
    for (const auto& constraint : m_jointConstraints) {
        if (constraint.joint && constraint.transformA && constraint.transformB) {
            // 检查位置误差
            Vector3 worldAnchorA = constraint.transformA->GetPosition() 
                + constraint.transformA->GetRotation() * constraint.joint->base.localAnchorA;
            Vector3 worldAnchorB = constraint.transformB->GetPosition() 
                + constraint.transformB->GetRotation() * constraint.joint->base.localAnchorB;
            Vector3 separation = worldAnchorB - worldAnchorA;
            if (separation.norm() > 0.05f) {  // 如果误差较大，需要初始修正
                needsInitialCorrection = true;
                break;
            }
        }
    }
    
    // 关节位置修正
    if (needsInitialCorrection) {
        // 进行额外的位置修正迭代（用于初始配置不满足约束的情况）
        for (int i = 0; i < m_positionIterations * 2; ++i) {
            SolveJointPositionConstraints(dt);
            
            // 每次迭代后更新 rA 和 rB
            for (auto& constraint : m_jointConstraints) {
                if (constraint.joint && constraint.transformA && constraint.transformB) {
                    auto& bodyA = *constraint.bodyA;
                    auto& bodyB = *constraint.bodyB;
                    Vector3 comA = constraint.transformA->GetPosition() + bodyA.centerOfMass;
                    Vector3 comB = constraint.transformB->GetPosition() + bodyB.centerOfMass;
                    Vector3 worldAnchorA = constraint.transformA->GetPosition() 
                        + constraint.transformA->GetRotation() * constraint.joint->base.localAnchorA;
                    Vector3 worldAnchorB = constraint.transformB->GetPosition() 
                        + constraint.transformB->GetRotation() * constraint.joint->base.localAnchorB;
                    constraint.joint->runtime.rA = worldAnchorA - comA;
                    constraint.joint->runtime.rB = worldAnchorB - comB;
                }
            }
        }
    } else {
        // 正常的位置修正迭代
        for (int i = 0; i < m_positionIterations; ++i) {
            SolveJointPositionConstraints(dt);
        }
    }
    
    // 缓存冲量（当前帧的累积冲量将用于下一帧的 Warm Start）
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
    float /* dt */,
    const std::vector<ECS::EntityID>& jointEntities
) {
    m_jointConstraints.clear();
    m_jointConstraints.reserve(jointEntities.size());
    
    for (auto entityID : jointEntities) {
        // entityID 现在是拥有关节组件的刚体（bodyA）
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
        
        // 验证两个刚体都存在且有必要的组件
        // entityID 是 bodyA（关节的拥有者）
        // connectedBody 是 bodyB（关节连接到的刚体）
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
        constraint.jointEntity = entityID;  // 拥有关节的实体
        constraint.entityA = entityID;      // bodyA
        constraint.entityB = connectedBody; // bodyB
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
        
        // 为 Fixed Joint 初始化相对位置和相对旋转（如果尚未初始化）
        if (base.type == JointComponent::JointType::Fixed) {
            auto& fixedData = std::get<FixedJointData>(jointComp.data);
            
            // 初始化相对位置（如果尚未初始化）
            // 检查 relativePosition 是否为零向量（默认值）
            if (fixedData.relativePosition.norm() < MathUtils::EPSILON) {
                // 使用已经计算好的 worldAnchorA 和 worldAnchorB
                fixedData.relativePosition = worldAnchorB - worldAnchorA;
            }
            
            // 初始化相对旋转（如果尚未初始化）
            // 使用小阈值检查是否为单位四元数
            if (std::abs(fixedData.relativeRotation.w() - 1.0f) < MathUtils::EPSILON &&
                fixedData.relativeRotation.vec().norm() < MathUtils::EPSILON) {
                // 初始化相对旋转：q_relative = qB * qA^-1
                Quaternion qA = constraint.transformA->GetRotation();
                Quaternion qB = constraint.transformB->GetRotation();
                fixedData.relativeRotation = qB * qA.conjugate();
            }
        }
        
        // 为 Distance Joint 初始化 restLength（如果尚未初始化）
        if (base.type == JointComponent::JointType::Distance) {
            auto& distData = std::get<DistanceJointData>(jointComp.data);
            
            // 如果 restLength 是默认值（1.0f），初始化为当前距离
            if (std::abs(distData.restLength - 1.0f) < MathUtils::EPSILON) {
                Vector3 separation = worldAnchorB - worldAnchorA;
                float currentDistance = separation.norm();
                if (currentDistance > MathUtils::EPSILON) {
                    distData.restLength = currentDistance;
                }
            }
        }
        
        // 为 Hinge Joint 预处理（计算旋转轴和垂直轴）
        if (base.type == JointComponent::JointType::Hinge) {
            auto& hingeData = std::get<HingeJointData>(jointComp.data);
            
            // 计算世界空间旋转轴（使用bodyA的局部轴）
            jointComp.runtime.worldAxis = (constraint.transformA->GetRotation() * hingeData.localAxisA).normalized();
            
            // 计算垂直于旋转轴的两个方向（用于旋转约束）
            // 这些将在速度约束求解时使用
        }
        
        m_jointConstraints.push_back(constraint);
    }
}

void ConstraintSolver::WarmStartJoints() {
    constexpr float decayFactor = 0.95f;
    constexpr float maxCachedImpulse = 1e4f;
    
    for (auto& constraint : m_jointConstraints) {
        if (!constraint.joint) {
            continue;
        }
        
        auto& joint = *constraint.joint;
        auto& bodyA = *constraint.bodyA;
        auto& bodyB = *constraint.bodyB;
        
        // 从 joint.runtime 读取上一帧的累积冲量（此时还没有被重置）
        Vector3 cachedLinearImpulse = joint.runtime.accumulatedLinearImpulse;
        Vector3 cachedAngularImpulse = joint.runtime.accumulatedAngularImpulse;
        
        // 衰减因子（防止过时冲量累积）
        cachedLinearImpulse *= decayFactor;
        cachedAngularImpulse *= decayFactor;
        
        // 重置异常值
        if (cachedLinearImpulse.norm() > maxCachedImpulse) {
            cachedLinearImpulse = Vector3::Zero();
        }
        if (cachedAngularImpulse.norm() > maxCachedImpulse) {
            cachedAngularImpulse = Vector3::Zero();
        }
        
        // 只有当冲量足够大时才应用（避免浮点误差累积）
        if (cachedLinearImpulse.norm() > MathUtils::EPSILON) {
            bodyA.linearVelocity -= cachedLinearImpulse * bodyA.inverseMass;
            bodyB.linearVelocity += cachedLinearImpulse * bodyB.inverseMass;
        }
        
        if (cachedAngularImpulse.norm() > MathUtils::EPSILON) {
            bodyA.angularVelocity -= joint.runtime.invInertiaA * cachedAngularImpulse;
            bodyB.angularVelocity += joint.runtime.invInertiaB * cachedAngularImpulse;
        }
    }
}

void ConstraintSolver::SolveFixedJointVelocity(
    JointConstraint& constraint,
    float dt
) {
    auto* joint = constraint.joint;
    auto& bodyA = *constraint.bodyA;
    auto& bodyB = *constraint.bodyB;
    auto& transformA = *constraint.transformA;
    auto& transformB = *constraint.transformB;
    
    // 获取 Fixed Joint 数据
    auto& fixedData = std::get<FixedJointData>(joint->data);
    
    // 1. 位置约束 - 约束两个锚点保持相对位置
    Vector3 worldAnchorA = transformA.GetPosition() 
        + transformA.GetRotation() * joint->base.localAnchorA;
    Vector3 worldAnchorB = transformB.GetPosition() 
        + transformB.GetRotation() * joint->base.localAnchorB;
    
    // 当前分离向量：从 A 指向 B
    Vector3 currentSeparation = worldAnchorB - worldAnchorA;
    // 期望的分离向量（相对位置）
    Vector3 expectedSeparation = fixedData.relativePosition;
    // 位置误差向量
    Vector3 positionError = currentSeparation - expectedSeparation;
    float errorMagnitude = positionError.norm();
    
    // 只有当位置误差足够大时才进行约束
    // 使用更小的阈值，因为我们有位置修正来处理大误差
    if (errorMagnitude > 1e-4f) {
        Vector3 normal = positionError / errorMagnitude;
        
        // 相对速度（B 相对于 A）
        Vector3 vA = bodyA.linearVelocity + bodyA.angularVelocity.cross(joint->runtime.rA);
        Vector3 vB = bodyB.linearVelocity + bodyB.angularVelocity.cross(joint->runtime.rB);
        Vector3 relVel = vB - vA;
        float normalVel = relVel.dot(normal);
        
        // 有效质量
        Vector3 rnA = joint->runtime.rA.cross(normal);
        Vector3 rnB = joint->runtime.rB.cross(normal);
        float K = bodyA.inverseMass + bodyB.inverseMass
                + rnA.dot(joint->runtime.invInertiaA * rnA)
                + rnB.dot(joint->runtime.invInertiaB * rnB)
                + kCFM;
        
        if (K < MathUtils::EPSILON) {
            return;
        }
        
        float effectiveMass = 1.0f / K;
        
        // 速度约束：只修正速度，不引入 Baumgarte bias
        // 位置误差由位置修正阶段处理
        // 这样可以避免给静止物体施加不必要的速度
        float velocityError = normalVel;
        
        // 计算冲量（仅消除相对速度，不添加 bias）
        float lambda = -velocityError * effectiveMass;
        
        // 冲量钳位
        float maxImpulseA = ComputeMaxImpulse(bodyA, dt);
        float maxImpulseB = ComputeMaxImpulse(bodyB, dt);
        float maxImpulse = std::max(maxImpulseA, maxImpulseB);
        lambda = MathUtils::Clamp(lambda, -maxImpulse, maxImpulse);
        
        if (std::isnan(lambda) || std::isinf(lambda)) {
            return;
        }
        
        Vector3 impulse = lambda * normal;
        
        // 应用冲量
        if (!bodyA.IsStatic() && !bodyA.IsKinematic()) {
            bodyA.linearVelocity -= impulse * bodyA.inverseMass;
            bodyA.angularVelocity -= joint->runtime.invInertiaA * joint->runtime.rA.cross(impulse);
        }
        
        if (!bodyB.IsStatic() && !bodyB.IsKinematic()) {
            bodyB.linearVelocity += impulse * bodyB.inverseMass;
            bodyB.angularVelocity += joint->runtime.invInertiaB * joint->runtime.rB.cross(impulse);
        }
        
        // 累积冲量
        joint->runtime.accumulatedLinearImpulse += impulse;
    }
    
    // 2. 旋转约束 - 约束相对旋转
    Quaternion qA = transformA.GetRotation();
    Quaternion qB = transformB.GetRotation();
    
    // 当前相对旋转
    Quaternion currentRelative = qB * qA.conjugate();
    
    // 旋转误差：当前相对旋转 与 期望相对旋转 的差
    Quaternion q_error = currentRelative * fixedData.relativeRotation.conjugate();
    q_error.normalize();
    
    // 将四元数误差转换为轴角表示（小角度近似）
    // 对于小角度，旋转向量 ≈ 2 * [x, y, z]
    Vector3 rotError = 2.0f * Vector3(q_error.x(), q_error.y(), q_error.z());
    float errorMag = rotError.norm();
    
    // 使用更小的阈值，因为我们有位置修正来处理大误差
    if (errorMag > 1e-4f) {
        // 归一化旋转轴
        Vector3 rotAxis = rotError / errorMag;
        
        // 相对角速度（B 相对于 A）
        Vector3 angVelRel = bodyB.angularVelocity - bodyA.angularVelocity;
        float angVelProj = angVelRel.dot(rotAxis);
        
        // 有效质量
        float K = rotAxis.dot((joint->runtime.invInertiaA + joint->runtime.invInertiaB) * rotAxis)
                + kCFM;
        
        if (K < MathUtils::EPSILON) {
            return;
        }
        
        float effectiveMass = 1.0f / K;
        
        // 速度约束：只修正角速度，不引入 Baumgarte bias
        // 角度误差由位置修正阶段处理
        float angVelError = angVelProj;
        
        // 计算角冲量（仅消除相对角速度）
        float lambda = -angVelError * effectiveMass;
        
        // 角冲量钳位
        constexpr float kMaxAngularVelocity = 2.0f * MathUtils::PI * 10.0f;
        float maxAngularImpulse = kMaxAngularVelocity * dt;
        lambda = MathUtils::Clamp(lambda, -maxAngularImpulse, maxAngularImpulse);
        
        if (std::isnan(lambda) || std::isinf(lambda)) {
            return;
        }
        
        Vector3 angularImpulse = lambda * rotAxis;
        
        // 应用角冲量
        if (!bodyA.IsStatic() && !bodyA.IsKinematic()) {
            bodyA.angularVelocity -= joint->runtime.invInertiaA * angularImpulse;
        }
        
        if (!bodyB.IsStatic() && !bodyB.IsKinematic()) {
            bodyB.angularVelocity += joint->runtime.invInertiaB * angularImpulse;
        }
        
        // 累积角冲量
        joint->runtime.accumulatedAngularImpulse += angularImpulse;
    }
    
    // 速度限制检查
    CheckAndClampVelocity(&bodyA);
    CheckAndClampVelocity(&bodyB);
}

void ConstraintSolver::SolveFixedJointPosition(
    JointConstraint& constraint,
    float dt
) {
    auto* joint = constraint.joint;
    auto& bodyA = *constraint.bodyA;
    auto& bodyB = *constraint.bodyB;
    
    // 获取 Fixed Joint 数据
    auto& fixedData = std::get<FixedJointData>(joint->data);
    
    // 跳过静态/运动学物体
    bool canMoveA = !bodyA.IsStatic() && !bodyA.IsKinematic();
    bool canMoveB = !bodyB.IsStatic() && !bodyB.IsKinematic();
    
    if (!canMoveA && !canMoveB) {
        return;
    }
    
    // 计算世界空间锚点位置
    Vector3 worldAnchorA = constraint.transformA->GetPosition() 
        + constraint.transformA->GetRotation() * joint->base.localAnchorA;
    Vector3 worldAnchorB = constraint.transformB->GetPosition() 
        + constraint.transformB->GetRotation() * joint->base.localAnchorB;
    
    // 当前分离向量
    Vector3 currentSeparation = worldAnchorB - worldAnchorA;
    // 期望的分离向量（相对位置）
    Vector3 expectedSeparation = fixedData.relativePosition;
    // 位置误差向量
    Vector3 positionError = currentSeparation - expectedSeparation;
    float errorMagnitude = positionError.norm();
    
    if (errorMagnitude <= MathUtils::EPSILON) {
        return;
    }
    
    Vector3 normal = positionError / errorMagnitude;
    
    // 有效质量
    Vector3 rnA = joint->runtime.rA.cross(normal);
    Vector3 rnB = joint->runtime.rB.cross(normal);
    float K = bodyA.inverseMass + bodyB.inverseMass
            + rnA.dot(joint->runtime.invInertiaA * rnA)
            + rnB.dot(joint->runtime.invInertiaB * rnB)
            + kCFM;
    
    if (K < MathUtils::EPSILON) {
        return;
    }
    
    float effectiveMass = 1.0f / K;
    
    // 位置修正系数
    float beta = 0.2f;
    if (errorMagnitude > 0.5f) {
        beta = 0.5f;
    } else if (errorMagnitude > 0.1f) {
        beta = 0.3f;
    }
    
    // 修正量：我们要消除位置误差，所以修正方向应该是 -normal * errorMagnitude
    // 但由于我们使用的是 lambda * normal 的形式，lambda 应该是负值
    float correction = -beta * errorMagnitude;
    float lambda = correction * effectiveMass;
    
    // 限制位置修正量
    float maxCorrection = 0.2f;
    lambda = MathUtils::Clamp(lambda, -maxCorrection * effectiveMass, maxCorrection * effectiveMass);
    
    Vector3 correctionImpulse = lambda * normal;
    
    if (correctionImpulse.hasNaN()) {
        return;
    }
    
    // 应用位置修正（注意：这里是伪冲量，直接修改位置）
    if (canMoveA) {
        Vector3 linearDeltaA = -correctionImpulse * bodyA.inverseMass;
        if (!linearDeltaA.hasNaN()) {
            constraint.transformA->SetPosition(
                constraint.transformA->GetPosition() + linearDeltaA);
        }
    }
    
    if (canMoveB) {
        Vector3 linearDeltaB = correctionImpulse * bodyB.inverseMass;
        if (!linearDeltaB.hasNaN()) {
            constraint.transformB->SetPosition(
                constraint.transformB->GetPosition() + linearDeltaB);
        }
    }
    
    // 更新 rA 和 rB
    Vector3 comA = constraint.transformA->GetPosition() + bodyA.centerOfMass;
    Vector3 comB = constraint.transformB->GetPosition() + bodyB.centerOfMass;
    worldAnchorA = constraint.transformA->GetPosition() 
        + constraint.transformA->GetRotation() * joint->base.localAnchorA;
    worldAnchorB = constraint.transformB->GetPosition() 
        + constraint.transformB->GetRotation() * joint->base.localAnchorB;
    joint->runtime.rA = worldAnchorA - comA;
    joint->runtime.rB = worldAnchorB - comB;
    
    // 旋转约束的位置修正
    Quaternion qA = constraint.transformA->GetRotation();
    Quaternion qB = constraint.transformB->GetRotation();
    Quaternion currentRelative = qB * qA.conjugate();
    Quaternion q_error = currentRelative * fixedData.relativeRotation.conjugate();
    q_error.normalize();
    
    Vector3 rotError = 2.0f * Vector3(q_error.x(), q_error.y(), q_error.z());
    float rotErrorMag = rotError.norm();
    
    if (rotErrorMag > MathUtils::EPSILON) {
        Vector3 rotAxis = rotError / rotErrorMag;
        
        // 限制旋转修正角度
        float maxRotCorrection = 0.3f;
        float rotCorrection = -beta * rotErrorMag;
        rotCorrection = MathUtils::Clamp(rotCorrection, -maxRotCorrection, maxRotCorrection);
        
        // 有效质量
        float K_rot = rotAxis.dot((joint->runtime.invInertiaA + joint->runtime.invInertiaB) * rotAxis)
                + kCFM;
        
        if (K_rot < MathUtils::EPSILON) {
            return;
        }
        
        float effectiveMass_rot = 1.0f / K_rot;
        float angularLambda = rotCorrection * effectiveMass_rot;
        Vector3 angularCorrection = angularLambda * rotAxis;
        
        if (angularCorrection.hasNaN()) {
            return;
        }
        
        // 应用旋转修正
        Vector3 angularDeltaA = -joint->runtime.invInertiaA * angularCorrection;
        Vector3 angularDeltaB = joint->runtime.invInertiaB * angularCorrection;
        
        auto ApplyAngularCorrection = [](ECS::TransformComponent* transform, const Vector3& angularDelta) {
            float angle = angularDelta.norm();
            if (angle <= MathUtils::EPSILON || !transform || std::isnan(angle) || std::isinf(angle)) {
                return;
            }
            Vector3 axis = angularDelta / angle;
            if (axis.hasNaN()) {
                return;
            }
            Quaternion delta(Eigen::AngleAxisf(angle, axis));
            Quaternion newRotation = (delta * transform->GetRotation()).normalized();
            transform->SetRotation(newRotation);
        };
        
        if (canMoveA && !angularDeltaA.hasNaN()) {
            ApplyAngularCorrection(constraint.transformA, angularDeltaA);
        }
        
        if (canMoveB && !angularDeltaB.hasNaN()) {
            ApplyAngularCorrection(constraint.transformB, angularDeltaB);
        }
        
        // 更新 rA 和 rB
        comA = constraint.transformA->GetPosition() + bodyA.centerOfMass;
        comB = constraint.transformB->GetPosition() + bodyB.centerOfMass;
        worldAnchorA = constraint.transformA->GetPosition() 
            + constraint.transformA->GetRotation() * joint->base.localAnchorA;
        worldAnchorB = constraint.transformB->GetPosition() 
            + constraint.transformB->GetRotation() * joint->base.localAnchorB;
        joint->runtime.rA = worldAnchorA - comA;
        joint->runtime.rB = worldAnchorB - comB;
    }
}

void ConstraintSolver::SolveDistanceJointVelocity(
    JointConstraint& constraint,
    float dt
) {
    auto* joint = constraint.joint;
    auto& bodyA = *constraint.bodyA;
    auto& bodyB = *constraint.bodyB;
    auto& transformA = *constraint.transformA;
    auto& transformB = *constraint.transformB;
    
    // 获取 Distance Joint 数据
    auto& distData = std::get<DistanceJointData>(joint->data);
    
    // 计算世界空间锚点位置
    Vector3 worldAnchorA = transformA.GetPosition() 
        + transformA.GetRotation() * joint->base.localAnchorA;
    Vector3 worldAnchorB = transformB.GetPosition() 
        + transformB.GetRotation() * joint->base.localAnchorB;
    
    Vector3 delta = worldAnchorB - worldAnchorA;
    float currentDistance = delta.norm();
    
    // 避免除零
    if (currentDistance < 1e-6f) {
        return;
    }
    
    Vector3 n = delta / currentDistance;
    
    // 约束方程：C = currentDistance - restLength
    float C = currentDistance - distData.restLength;
    
    // 距离限制处理
    if (distData.hasLimits) {
        if (currentDistance < distData.minDistance) {
            // 距离太近，需要推开
            C = currentDistance - distData.minDistance;
        } else if (currentDistance > distData.maxDistance) {
            // 距离太远，需要拉近
            C = currentDistance - distData.maxDistance;
        } else {
            // 在范围内，不施加约束
            return;
        }
    }
    
    // 计算相对速度
    Vector3 vA = bodyA.linearVelocity + bodyA.angularVelocity.cross(joint->runtime.rA);
    Vector3 vB = bodyB.linearVelocity + bodyB.angularVelocity.cross(joint->runtime.rB);
    Vector3 relVel = vB - vA;
    float normalVel = relVel.dot(n);
    
    // 有效质量（添加 CFM 软化）
    Vector3 rnA = joint->runtime.rA.cross(n);
    Vector3 rnB = joint->runtime.rB.cross(n);
    float K = bodyA.inverseMass + bodyB.inverseMass
            + rnA.dot(joint->runtime.invInertiaA * rnA)
            + rnB.dot(joint->runtime.invInertiaB * rnB)
            + kCFM;
    
    if (K < MathUtils::EPSILON) {
        return;
    }
    
    float effectiveMass = 1.0f / K;
    
    // Baumgarte 稳定化（自适应）
    float adaptiveBeta = ComputeAdaptiveBaumgarte(std::abs(C), dt);
    float bias = (adaptiveBeta / std::max(dt, MathUtils::EPSILON)) * C;
    
    // 计算冲量
    float lambda = -(normalVel + bias) * effectiveMass;
    
    // 冲量钳位
    float maxImpulseA = ComputeMaxImpulse(bodyA, dt);
    float maxImpulseB = ComputeMaxImpulse(bodyB, dt);
    float maxImpulse = std::max(maxImpulseA, maxImpulseB);
    lambda = MathUtils::Clamp(lambda, -maxImpulse, maxImpulse);
    
    if (std::isnan(lambda) || std::isinf(lambda)) {
        return;
    }
    
    // 单向约束处理（如果有限制）
    if (distData.hasLimits) {
        if (currentDistance < distData.minDistance) {
            // 只能推开，不能拉近（λ >= 0）
            float oldImpulse = joint->runtime.accumulatedLimitImpulse;
            joint->runtime.accumulatedLimitImpulse = std::max(oldImpulse + lambda, 0.0f);
            lambda = joint->runtime.accumulatedLimitImpulse - oldImpulse;
        } else if (currentDistance > distData.maxDistance) {
            // 只能拉近，不能推开（λ <= 0）
            float oldImpulse = joint->runtime.accumulatedLimitImpulse;
            joint->runtime.accumulatedLimitImpulse = std::min(oldImpulse + lambda, 0.0f);
            lambda = joint->runtime.accumulatedLimitImpulse - oldImpulse;
        }
    }
    
    // 应用冲量
    Vector3 impulse = lambda * n;
    
    if (!bodyA.IsStatic() && !bodyA.IsKinematic()) {
        bodyA.linearVelocity -= impulse * bodyA.inverseMass;
        bodyA.angularVelocity -= joint->runtime.invInertiaA * joint->runtime.rA.cross(impulse);
    }
    
    if (!bodyB.IsStatic() && !bodyB.IsKinematic()) {
        bodyB.linearVelocity += impulse * bodyB.inverseMass;
        bodyB.angularVelocity += joint->runtime.invInertiaB * joint->runtime.rB.cross(impulse);
    }
    
    // 累积冲量
    joint->runtime.accumulatedLinearImpulse += impulse;
    
    // 速度限制检查
    CheckAndClampVelocity(&bodyA);
    CheckAndClampVelocity(&bodyB);
}

void ConstraintSolver::SolveJointVelocityConstraints(float dt) {
    for (auto& constraint : m_jointConstraints) {
        if (!constraint.joint) {
            continue;
        }
        
        switch (constraint.joint->base.type) {
            case JointComponent::JointType::Fixed:
                SolveFixedJointVelocity(constraint, dt);
                break;
            case JointComponent::JointType::Distance:
                SolveDistanceJointVelocity(constraint, dt);
                break;
            case JointComponent::JointType::Hinge:
                SolveHingeJointVelocity(constraint, dt);
                break;
            // 其他类型将在后续阶段实现
            case JointComponent::JointType::Spring:
            case JointComponent::JointType::Slider:
                // 暂未实现
                break;
        }
    }
}

void ConstraintSolver::SolveDistanceJointPosition(
    JointConstraint& constraint,
    float dt
) {
    auto* joint = constraint.joint;
    auto& bodyA = *constraint.bodyA;
    auto& bodyB = *constraint.bodyB;
    
    // 获取 Distance Joint 数据
    auto& distData = std::get<DistanceJointData>(joint->data);
    
    // 跳过静态/运动学物体
    bool canMoveA = !bodyA.IsStatic() && !bodyA.IsKinematic();
    bool canMoveB = !bodyB.IsStatic() && !bodyB.IsKinematic();
    
    if (!canMoveA && !canMoveB) {
        return;
    }
    
    // 计算世界空间锚点位置
    Vector3 worldAnchorA = constraint.transformA->GetPosition() 
        + constraint.transformA->GetRotation() * joint->base.localAnchorA;
    Vector3 worldAnchorB = constraint.transformB->GetPosition() 
        + constraint.transformB->GetRotation() * joint->base.localAnchorB;
    
    Vector3 delta = worldAnchorB - worldAnchorA;
    float currentDistance = delta.norm();
    
    // 避免除零
    if (currentDistance < 1e-6f) {
        return;
    }
    
    Vector3 n = delta / currentDistance;
    
    // 计算距离误差
    float distanceError = 0.0f;
    bool needsCorrection = false;
    
    if (distData.hasLimits) {
        // 有距离限制的情况
        if (currentDistance < distData.minDistance) {
            distanceError = currentDistance - distData.minDistance;
            needsCorrection = true;
        } else if (currentDistance > distData.maxDistance) {
            distanceError = currentDistance - distData.maxDistance;
            needsCorrection = true;
        }
        // 在范围内不需要修正
    } else {
        // 无限制，约束到 restLength
        distanceError = currentDistance - distData.restLength;
        if (std::abs(distanceError) > MathUtils::EPSILON) {
            needsCorrection = true;
        }
    }
    
    if (!needsCorrection) {
        return;
    }
    
    // 有效质量
    Vector3 rnA = joint->runtime.rA.cross(n);
    Vector3 rnB = joint->runtime.rB.cross(n);
    float K = bodyA.inverseMass + bodyB.inverseMass
            + rnA.dot(joint->runtime.invInertiaA * rnA)
            + rnB.dot(joint->runtime.invInertiaB * rnB)
            + kCFM;
    
    if (K < MathUtils::EPSILON) {
        return;
    }
    
    float effectiveMass = 1.0f / K;
    
    // 位置修正系数（自适应）
    float beta = 0.2f;
    float absError = std::abs(distanceError);
    if (absError > 0.5f) {
        beta = 0.5f;
    } else if (absError > 0.1f) {
        beta = 0.3f;
    }
    
    // 修正量：消除距离误差
    float correction = -beta * distanceError;
    float lambda = correction * effectiveMass;
    
    // 限制位置修正量
    float maxCorrection = 0.2f;
    lambda = MathUtils::Clamp(lambda, -maxCorrection * effectiveMass, maxCorrection * effectiveMass);
    
    Vector3 correctionImpulse = lambda * n;
    
    if (correctionImpulse.hasNaN()) {
        return;
    }
    
    // 应用位置修正
    if (canMoveA) {
        Vector3 linearDeltaA = -correctionImpulse * bodyA.inverseMass;
        if (!linearDeltaA.hasNaN()) {
            constraint.transformA->SetPosition(
                constraint.transformA->GetPosition() + linearDeltaA);
        }
    }
    
    if (canMoveB) {
        Vector3 linearDeltaB = correctionImpulse * bodyB.inverseMass;
        if (!linearDeltaB.hasNaN()) {
            constraint.transformB->SetPosition(
                constraint.transformB->GetPosition() + linearDeltaB);
        }
    }
    
    // 更新 rA 和 rB
    Vector3 comA = constraint.transformA->GetPosition() + bodyA.centerOfMass;
    Vector3 comB = constraint.transformB->GetPosition() + bodyB.centerOfMass;
    worldAnchorA = constraint.transformA->GetPosition() 
        + constraint.transformA->GetRotation() * joint->base.localAnchorA;
    worldAnchorB = constraint.transformB->GetPosition() 
        + constraint.transformB->GetRotation() * joint->base.localAnchorB;
    joint->runtime.rA = worldAnchorA - comA;
    joint->runtime.rB = worldAnchorB - comB;
}

void ConstraintSolver::SolveJointPositionConstraints(float dt) {
    // 位置修正需要多次迭代才能收敛
    for (int iteration = 0; iteration < m_positionIterations; ++iteration) {
        for (auto& constraint : m_jointConstraints) {
            if (!constraint.joint) {
                continue;
            }
            
            switch (constraint.joint->base.type) {
                case JointComponent::JointType::Fixed:
                    SolveFixedJointPosition(constraint, dt);
                    break;
                case JointComponent::JointType::Distance:
                    SolveDistanceJointPosition(constraint, dt);
                    break;
                case JointComponent::JointType::Hinge:
                    SolveHingeJointPosition(constraint, dt);
                    break;
                // 其他类型将在后续阶段实现
                case JointComponent::JointType::Spring:
                case JointComponent::JointType::Slider:
                    // 暂未实现
                    break;
            }
        }
    }
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

float ConstraintSolver::CalculateHingeAngle(const JointConstraint& constraint) {
    if (!constraint.joint || constraint.joint->base.type != JointComponent::JointType::Hinge) {
        return 0.0f;
    }
    
    if (!constraint.transformA || !constraint.transformB) {
        return 0.0f;
    }
    
    auto& hingeData = std::get<HingeJointData>(constraint.joint->data);
    
    // 获取参考向量（垂直于局部轴）
    Vector3 refA = constraint.transformA->GetRotation() 
        * ChooseTangent(hingeData.localAxisA);
    Vector3 refB = constraint.transformB->GetRotation() 
        * ChooseTangent(hingeData.localAxisB);
    
    // 投影到垂直于worldAxis的平面
    Vector3 worldAxis = constraint.joint->runtime.worldAxis;
    refA -= worldAxis * worldAxis.dot(refA);
    refB -= worldAxis * worldAxis.dot(refB);
    
    float refANorm = refA.norm();
    float refBNorm = refB.norm();
    
    if (refANorm < MathUtils::EPSILON || refBNorm < MathUtils::EPSILON) {
        return 0.0f;
    }
    
    refA.normalize();
    refB.normalize();
    
    // 计算角度
    float cosAngle = refA.dot(refB);
    cosAngle = MathUtils::Clamp(cosAngle, -1.0f, 1.0f);  // 防止数值误差
    
    Vector3 cross = refA.cross(refB);
    float sinAngle = worldAxis.dot(cross);
    
    return std::atan2(sinAngle, cosAngle);
}

void ConstraintSolver::SolveHingeJointVelocity(
    JointConstraint& constraint,
    float dt
) {
    auto* joint = constraint.joint;
    auto& bodyA = *constraint.bodyA;
    auto& bodyB = *constraint.bodyB;
    auto& transformA = *constraint.transformA;
    auto& transformB = *constraint.transformB;
    
    // 获取 Hinge Joint 数据
    auto& hingeData = std::get<HingeJointData>(joint->data);
    
    // 计算世界空间旋转轴和垂直轴（如果尚未计算）
    Vector3 worldAxis = joint->runtime.worldAxis;
    Vector3 perp1 = ChooseTangent(worldAxis);
    Vector3 perp2 = worldAxis.cross(perp1).normalized();
    
    // 1. 位置约束 - 约束两个锚点对齐（同 Fixed Joint）
    Vector3 worldAnchorA = transformA.GetPosition() 
        + transformA.GetRotation() * joint->base.localAnchorA;
    Vector3 worldAnchorB = transformB.GetPosition() 
        + transformB.GetRotation() * joint->base.localAnchorB;
    
    Vector3 positionError = worldAnchorB - worldAnchorA;
    float errorMagnitude = positionError.norm();
    
    // 只有当位置误差足够大时才进行约束
    if (errorMagnitude > 1e-4f) {
        Vector3 normal = positionError / errorMagnitude;
        
        // 相对速度
        Vector3 vA = bodyA.linearVelocity + bodyA.angularVelocity.cross(joint->runtime.rA);
        Vector3 vB = bodyB.linearVelocity + bodyB.angularVelocity.cross(joint->runtime.rB);
        Vector3 relVel = vB - vA;
        float normalVel = relVel.dot(normal);
        
        // 有效质量（添加 CFM 软化）
        Vector3 rnA = joint->runtime.rA.cross(normal);
        Vector3 rnB = joint->runtime.rB.cross(normal);
        float K = bodyA.inverseMass + bodyB.inverseMass
                + rnA.dot(joint->runtime.invInertiaA * rnA)
                + rnB.dot(joint->runtime.invInertiaB * rnB)
                + kCFM;
        
        if (K > MathUtils::EPSILON) {
            float effectiveMass = 1.0f / K;
            
            // 速度约束：只修正速度，不引入 Baumgarte bias
            float velocityError = normalVel;
            float lambda = -velocityError * effectiveMass;
            
            // 冲量钳位
            float maxImpulseA = ComputeMaxImpulse(bodyA, dt);
            float maxImpulseB = ComputeMaxImpulse(bodyB, dt);
            float maxImpulse = std::max(maxImpulseA, maxImpulseB);
            lambda = MathUtils::Clamp(lambda, -maxImpulse, maxImpulse);
            
            if (!std::isnan(lambda) && !std::isinf(lambda)) {
                Vector3 impulse = lambda * normal;
                
                // 应用冲量
                if (!bodyA.IsStatic() && !bodyA.IsKinematic()) {
                    bodyA.linearVelocity -= impulse * bodyA.inverseMass;
                    bodyA.angularVelocity -= joint->runtime.invInertiaA * joint->runtime.rA.cross(impulse);
                }
                
                if (!bodyB.IsStatic() && !bodyB.IsKinematic()) {
                    bodyB.linearVelocity += impulse * bodyB.inverseMass;
                    bodyB.angularVelocity += joint->runtime.invInertiaB * joint->runtime.rB.cross(impulse);
                }
                
                // 累积冲量
                joint->runtime.accumulatedLinearImpulse += impulse;
            }
        }
    }
    
    // 2. 旋转约束 - 约束垂直于旋转轴的2个自由度
    // 计算当前角度（用于角度限制检查）
    float currentAngle = CalculateHingeAngle(constraint);
    hingeData.currentAngle = currentAngle;
    
    // 对两个垂直方向进行约束
    for (int i = 0; i < 2; ++i) {
        const Vector3& perp = (i == 0) ? perp1 : perp2;
        
        // 计算参考向量（从bodyA和bodyB的旋转中获取）
        // 使用垂直于旋转轴的方向作为参考
        Vector3 refA = transformA.GetRotation() * perp;
        Vector3 refB = transformB.GetRotation() * perp;
        
        // 投影到垂直于worldAxis的平面（确保参考向量在正确平面上）
        refA -= worldAxis * worldAxis.dot(refA);
        refB -= worldAxis * worldAxis.dot(refB);
        
        float refANorm = refA.norm();
        float refBNorm = refB.norm();
        
        if (refANorm < MathUtils::EPSILON || refBNorm < MathUtils::EPSILON) {
            continue;
        }
        
        refA.normalize();
        refB.normalize();
        
        // 简化的角度误差（通过参考向量的差异）
        // C表示两个参考向量在perp方向上的差异
        float C = (refA - refB).dot(perp);
        
        if (std::abs(C) < 1e-4f) {
            continue;
        }
        
        // 相对角速度
        Vector3 angVelRel = bodyB.angularVelocity - bodyA.angularVelocity;
        float JV = angVelRel.dot(perp);
        
        // 有效质量（添加 CFM 软化）
        float K = perp.dot((joint->runtime.invInertiaA + joint->runtime.invInertiaB) * perp)
                + kCFM;
        
        if (K < MathUtils::EPSILON) {
            continue;
        }
        
        float effectiveMass = 1.0f / K;
        
        // 速度约束：只修正角速度，不引入 Baumgarte bias
        // 位置误差由位置修正阶段处理
        float angVelError = JV;
        float lambda = -angVelError * effectiveMass;
        
        // 角冲量钳位
        constexpr float kMaxAngularVelocity = 2.0f * MathUtils::PI * 10.0f;
        float maxAngularImpulse = kMaxAngularVelocity * dt;
        lambda = MathUtils::Clamp(lambda, -maxAngularImpulse, maxAngularImpulse);
        
        if (std::isnan(lambda) || std::isinf(lambda)) {
            continue;
        }
        
        Vector3 angularImpulse = lambda * perp;
        
        // 应用角冲量
        if (!bodyA.IsStatic() && !bodyA.IsKinematic()) {
            bodyA.angularVelocity -= joint->runtime.invInertiaA * angularImpulse;
        }
        
        if (!bodyB.IsStatic() && !bodyB.IsKinematic()) {
            bodyB.angularVelocity += joint->runtime.invInertiaB * angularImpulse;
        }
        
        // 累积角冲量
        joint->runtime.accumulatedAngularImpulse += angularImpulse;
    }
    
    // 3. 角度限制（单向约束）
    if (hingeData.hasLimits) {
        if (currentAngle < hingeData.limitMin) {
            // 角度太小，需要推开（λ >= 0）
            float C = currentAngle - hingeData.limitMin;
            Vector3 angVelRel = bodyB.angularVelocity - bodyA.angularVelocity;
            float JV = angVelRel.dot(worldAxis);
            
            // 有效质量
            float K = worldAxis.dot((joint->runtime.invInertiaA + joint->runtime.invInertiaB) * worldAxis)
                    + kCFM;
            
            if (K > MathUtils::EPSILON) {
                float effectiveMass = 1.0f / K;
                
                // 自适应 Baumgarte
                float adaptiveBeta = ComputeAdaptiveBaumgarte(std::abs(C), dt);
                float bias = (adaptiveBeta / std::max(dt, MathUtils::EPSILON)) * C;
                
                float lambda = -(JV + bias) * effectiveMass;
                
                // 单向约束：只能推开，不能拉近（λ >= 0）
                float oldImpulse = joint->runtime.accumulatedLimitImpulse;
                joint->runtime.accumulatedLimitImpulse = std::max(oldImpulse + lambda, 0.0f);
                lambda = joint->runtime.accumulatedLimitImpulse - oldImpulse;
                
                if (!std::isnan(lambda) && !std::isinf(lambda)) {
                    Vector3 angularImpulse = lambda * worldAxis;
                    
                    if (!bodyA.IsStatic() && !bodyA.IsKinematic()) {
                        bodyA.angularVelocity -= joint->runtime.invInertiaA * angularImpulse;
                    }
                    
                    if (!bodyB.IsStatic() && !bodyB.IsKinematic()) {
                        bodyB.angularVelocity += joint->runtime.invInertiaB * angularImpulse;
                    }
                }
            }
        } else if (currentAngle > hingeData.limitMax) {
            // 角度太大，需要拉近（λ <= 0）
            float C = currentAngle - hingeData.limitMax;
            Vector3 angVelRel = bodyB.angularVelocity - bodyA.angularVelocity;
            float JV = angVelRel.dot(worldAxis);
            
            // 有效质量
            float K = worldAxis.dot((joint->runtime.invInertiaA + joint->runtime.invInertiaB) * worldAxis)
                    + kCFM;
            
            if (K > MathUtils::EPSILON) {
                float effectiveMass = 1.0f / K;
                
                // 自适应 Baumgarte
                float adaptiveBeta = ComputeAdaptiveBaumgarte(std::abs(C), dt);
                float bias = (adaptiveBeta / std::max(dt, MathUtils::EPSILON)) * C;
                
                float lambda = -(JV + bias) * effectiveMass;
                
                // 单向约束：只能拉近，不能推开（λ <= 0）
                float oldImpulse = joint->runtime.accumulatedLimitImpulse;
                joint->runtime.accumulatedLimitImpulse = std::min(oldImpulse + lambda, 0.0f);
                lambda = joint->runtime.accumulatedLimitImpulse - oldImpulse;
                
                if (!std::isnan(lambda) && !std::isinf(lambda)) {
                    Vector3 angularImpulse = lambda * worldAxis;
                    
                    if (!bodyA.IsStatic() && !bodyA.IsKinematic()) {
                        bodyA.angularVelocity -= joint->runtime.invInertiaA * angularImpulse;
                    }
                    
                    if (!bodyB.IsStatic() && !bodyB.IsKinematic()) {
                        bodyB.angularVelocity += joint->runtime.invInertiaB * angularImpulse;
                    }
                }
            }
        }
    }
    
    // 4. 马达
    if (hingeData.useMotor) {
        Vector3 angVelRel = bodyB.angularVelocity - bodyA.angularVelocity;
        float currentSpeed = angVelRel.dot(worldAxis);
        float speedError = hingeData.motorSpeed - currentSpeed;
        
        // 有效质量
        float K = worldAxis.dot((joint->runtime.invInertiaA + joint->runtime.invInertiaB) * worldAxis)
                + kCFM;
        
        if (K > MathUtils::EPSILON) {
            float motorMass = 1.0f / K;
            float lambda = speedError * motorMass;
            
            // 限制马达力矩
            float oldImpulse = joint->runtime.accumulatedMotorImpulse;
            joint->runtime.accumulatedMotorImpulse = MathUtils::Clamp(
                oldImpulse + lambda,
                -hingeData.motorMaxForce * dt,
                hingeData.motorMaxForce * dt
            );
            lambda = joint->runtime.accumulatedMotorImpulse - oldImpulse;
            
            if (!std::isnan(lambda) && !std::isinf(lambda)) {
                Vector3 angularImpulse = lambda * worldAxis;
                
                if (!bodyA.IsStatic() && !bodyA.IsKinematic()) {
                    bodyA.angularVelocity -= joint->runtime.invInertiaA * angularImpulse;
                }
                
                if (!bodyB.IsStatic() && !bodyB.IsKinematic()) {
                    bodyB.angularVelocity += joint->runtime.invInertiaB * angularImpulse;
                }
            }
        }
    }
    
    // 速度限制检查
    CheckAndClampVelocity(&bodyA);
    CheckAndClampVelocity(&bodyB);
}

void ConstraintSolver::SolveHingeJointPosition(
    JointConstraint& constraint,
    float dt
) {
    auto* joint = constraint.joint;
    auto& bodyA = *constraint.bodyA;
    auto& bodyB = *constraint.bodyB;
    
    // 获取 Hinge Joint 数据
    auto& hingeData = std::get<HingeJointData>(joint->data);
    
    // 跳过静态/运动学物体
    bool canMoveA = !bodyA.IsStatic() && !bodyA.IsKinematic();
    bool canMoveB = !bodyB.IsStatic() && !bodyB.IsKinematic();
    
    if (!canMoveA && !canMoveB) {
        return;
    }
    
    // 计算世界空间锚点位置
    Vector3 worldAnchorA = constraint.transformA->GetPosition() 
        + constraint.transformA->GetRotation() * joint->base.localAnchorA;
    Vector3 worldAnchorB = constraint.transformB->GetPosition() 
        + constraint.transformB->GetRotation() * joint->base.localAnchorB;
    
    // 位置修正（同 Fixed Joint）
    Vector3 positionError = worldAnchorB - worldAnchorA;
    float errorMagnitude = positionError.norm();
    
    if (errorMagnitude > MathUtils::EPSILON) {
        Vector3 normal = positionError / errorMagnitude;
        
        // 有效质量
        Vector3 rnA = joint->runtime.rA.cross(normal);
        Vector3 rnB = joint->runtime.rB.cross(normal);
        float K = bodyA.inverseMass + bodyB.inverseMass
                + rnA.dot(joint->runtime.invInertiaA * rnA)
                + rnB.dot(joint->runtime.invInertiaB * rnB)
                + kCFM;
        
        if (K > MathUtils::EPSILON) {
            float effectiveMass = 1.0f / K;
            
            // 位置修正系数（自适应）
            float beta = 0.2f;
            if (errorMagnitude > 0.5f) {
                beta = 0.5f;
            } else if (errorMagnitude > 0.1f) {
                beta = 0.3f;
            }
            
            float correction = -beta * errorMagnitude;
            float lambda = correction * effectiveMass;
            
            // 限制位置修正量
            float maxCorrection = 0.2f;
            lambda = MathUtils::Clamp(lambda, -maxCorrection * effectiveMass, maxCorrection * effectiveMass);
            
            Vector3 correctionImpulse = lambda * normal;
            
            if (!correctionImpulse.hasNaN()) {
                // 应用位置修正
                if (canMoveA) {
                    Vector3 linearDeltaA = -correctionImpulse * bodyA.inverseMass;
                    if (!linearDeltaA.hasNaN()) {
                        constraint.transformA->SetPosition(
                            constraint.transformA->GetPosition() + linearDeltaA);
                    }
                }
                
                if (canMoveB) {
                    Vector3 linearDeltaB = correctionImpulse * bodyB.inverseMass;
                    if (!linearDeltaB.hasNaN()) {
                        constraint.transformB->SetPosition(
                            constraint.transformB->GetPosition() + linearDeltaB);
                    }
                }
                
                // 更新 rA 和 rB
                Vector3 comA = constraint.transformA->GetPosition() + bodyA.centerOfMass;
                Vector3 comB = constraint.transformB->GetPosition() + bodyB.centerOfMass;
                worldAnchorA = constraint.transformA->GetPosition() 
                    + constraint.transformA->GetRotation() * joint->base.localAnchorA;
                worldAnchorB = constraint.transformB->GetPosition() 
                    + constraint.transformB->GetRotation() * joint->base.localAnchorB;
                joint->runtime.rA = worldAnchorA - comA;
                joint->runtime.rB = worldAnchorB - comB;
            }
        }
    }
    
    // 旋转约束的位置修正 - 消除垂直于旋转轴的旋转分量
    // 使用与速度约束相同的方法：约束两个垂直方向
    Vector3 worldAxis = joint->runtime.worldAxis;
    Vector3 perp1 = ChooseTangent(worldAxis);
    Vector3 perp2 = worldAxis.cross(perp1).normalized();
    
    // 对两个垂直方向进行位置修正
    for (int i = 0; i < 2; ++i) {
        const Vector3& perp = (i == 0) ? perp1 : perp2;
        
        // 计算参考向量（从bodyA和bodyB的旋转中获取）
        Vector3 refA = constraint.transformA->GetRotation() * perp;
        Vector3 refB = constraint.transformB->GetRotation() * perp;
        
        // 投影到垂直于worldAxis的平面
        refA -= worldAxis * worldAxis.dot(refA);
        refB -= worldAxis * worldAxis.dot(refB);
        
        float refANorm = refA.norm();
        float refBNorm = refB.norm();
        
        if (refANorm < MathUtils::EPSILON || refBNorm < MathUtils::EPSILON) {
            continue;
        }
        
        refA.normalize();
        refB.normalize();
        
        // 计算角度误差（通过参考向量的差异）
        float C = (refA - refB).dot(perp);
        
        if (std::abs(C) < 1e-4f) {
            continue;
        }
        
        // 有效质量
        float K = perp.dot((joint->runtime.invInertiaA + joint->runtime.invInertiaB) * perp)
                + kCFM;
        
        if (K < MathUtils::EPSILON) {
            continue;
        }
        
        float effectiveMass = 1.0f / K;
        
        // 位置修正系数（自适应）
        float beta = 0.2f;
        float absError = std::abs(C);
        if (absError > 0.5f) {
            beta = 0.5f;
        } else if (absError > 0.1f) {
            beta = 0.3f;
        }
        
        float correction = -beta * C;
        float lambda = correction * effectiveMass;
        
        // 限制旋转修正角度
        float maxRotCorrection = 0.3f;
        lambda = MathUtils::Clamp(lambda, -maxRotCorrection * effectiveMass, maxRotCorrection * effectiveMass);
        
        Vector3 angularCorrection = lambda * perp;
        
        if (!angularCorrection.hasNaN()) {
            // 应用旋转修正
            Vector3 angularDeltaA = -joint->runtime.invInertiaA * angularCorrection;
            Vector3 angularDeltaB = joint->runtime.invInertiaB * angularCorrection;
            
            auto ApplyAngularCorrection = [](ECS::TransformComponent* transform, const Vector3& angularDelta) {
                float angle = angularDelta.norm();
                if (angle <= MathUtils::EPSILON || !transform || std::isnan(angle) || std::isinf(angle)) {
                    return;
                }
                Vector3 axis = angularDelta / angle;
                if (axis.hasNaN()) {
                    return;
                }
                Quaternion delta(Eigen::AngleAxisf(angle, axis));
                Quaternion newRotation = (delta * transform->GetRotation()).normalized();
                transform->SetRotation(newRotation);
            };
            
            if (canMoveA && !angularDeltaA.hasNaN()) {
                ApplyAngularCorrection(constraint.transformA, angularDeltaA);
            }
            
            if (canMoveB && !angularDeltaB.hasNaN()) {
                ApplyAngularCorrection(constraint.transformB, angularDeltaB);
            }
            
            // 更新 rA 和 rB
            Vector3 comA = constraint.transformA->GetPosition() + bodyA.centerOfMass;
            Vector3 comB = constraint.transformB->GetPosition() + bodyB.centerOfMass;
            worldAnchorA = constraint.transformA->GetPosition() 
                + constraint.transformA->GetRotation() * joint->base.localAnchorA;
            worldAnchorB = constraint.transformB->GetPosition() 
                + constraint.transformB->GetRotation() * joint->base.localAnchorB;
            joint->runtime.rA = worldAnchorA - comA;
            joint->runtime.rB = worldAnchorB - comB;
        }
    }
    
    // 角度限制的位置修正（如果初始角度误差很大）
    if (hingeData.hasLimits) {
        // 重新计算当前角度（因为位置可能已经改变）
        float currentAngle = CalculateHingeAngle(constraint);
        hingeData.currentAngle = currentAngle;
        
        // 如果角度超出限制，进行位置修正
        if (currentAngle < hingeData.limitMin || currentAngle > hingeData.limitMax) {
            // 计算目标角度（限制在范围内）
            float targetAngle = MathUtils::Clamp(currentAngle, hingeData.limitMin, hingeData.limitMax);
            float angleError = currentAngle - targetAngle;
            
            if (std::abs(angleError) > MathUtils::EPSILON) {
                // 计算需要修正的旋转
                Vector3 worldAxis = joint->runtime.worldAxis;
                
                // 有效质量
                float K = worldAxis.dot((joint->runtime.invInertiaA + joint->runtime.invInertiaB) * worldAxis)
                        + kCFM;
                
                if (K > MathUtils::EPSILON) {
                    float effectiveMass = 1.0f / K;
                    
                    // 位置修正系数
                    float beta = 0.2f;
                    if (std::abs(angleError) > 0.5f) {
                        beta = 0.5f;
                    } else if (std::abs(angleError) > 0.1f) {
                        beta = 0.3f;
                    }
                    
                    float correction = -beta * angleError;
                    float lambda = correction * effectiveMass;
                    
                    // 限制旋转修正角度
                    float maxRotCorrection = 0.3f;
                    lambda = MathUtils::Clamp(lambda, -maxRotCorrection * effectiveMass, maxRotCorrection * effectiveMass);
                    
                    Vector3 angularCorrection = lambda * worldAxis;
                    
                    if (!angularCorrection.hasNaN()) {
                        // 应用旋转修正
                        Vector3 angularDeltaA = -joint->runtime.invInertiaA * angularCorrection;
                        Vector3 angularDeltaB = joint->runtime.invInertiaB * angularCorrection;
                        
                        auto ApplyAngularCorrection = [](ECS::TransformComponent* transform, const Vector3& angularDelta) {
                            float angle = angularDelta.norm();
                            if (angle <= MathUtils::EPSILON || !transform || std::isnan(angle) || std::isinf(angle)) {
                                return;
                            }
                            Vector3 axis = angularDelta / angle;
                            if (axis.hasNaN()) {
                                return;
                            }
                            Quaternion delta(Eigen::AngleAxisf(angle, axis));
                            Quaternion newRotation = (delta * transform->GetRotation()).normalized();
                            transform->SetRotation(newRotation);
                        };
                        
                        if (canMoveA && !angularDeltaA.hasNaN()) {
                            ApplyAngularCorrection(constraint.transformA, angularDeltaA);
                        }
                        
                        if (canMoveB && !angularDeltaB.hasNaN()) {
                            ApplyAngularCorrection(constraint.transformB, angularDeltaB);
                        }
                        
                        // 更新 rA 和 rB
                        Vector3 comA = constraint.transformA->GetPosition() + bodyA.centerOfMass;
                        Vector3 comB = constraint.transformB->GetPosition() + bodyB.centerOfMass;
                        worldAnchorA = constraint.transformA->GetPosition() 
                            + constraint.transformA->GetRotation() * joint->base.localAnchorA;
                        worldAnchorB = constraint.transformB->GetPosition() 
                            + constraint.transformB->GetRotation() * joint->base.localAnchorB;
                        joint->runtime.rA = worldAnchorA - comA;
                        joint->runtime.rB = worldAnchorB - comB;
                    }
                }
            }
        }
    }
}

}  // namespace Physics
}  // namespace Render

