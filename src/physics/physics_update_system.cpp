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
#include "render/physics/physics_systems.h"
#include "render/physics/physics_utils.h"
#include "render/physics/dynamics/force_accumulator.h"
#include "render/physics/collision/ccd_detector.h"
#include "render/physics/collision/ccd_broad_phase.h"
#include "render/physics/collision/collision_shapes.h"
#include "render/math_utils.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/logger.h"
#include <cmath>
#include <algorithm>
#include <unordered_set>
#include <queue>

namespace Render {
namespace Physics {

PhysicsUpdateSystem::PhysicsUpdateSystem() {
    m_gravity = Vector3(0.0f, -9.81f, 0.0f);
    m_fixedDeltaTime = 1.0f / 60.0f;
    m_accumulator = 0.0f;
    m_physicsTime = 0.0f;
    // 注意：m_world 在 OnCreate 中设置，这里不设置
}

void PhysicsUpdateSystem::Update(float deltaTime) {
    if (!m_world) {
        return;
    }
    
    // 先恢复上一帧物理解算结果，避免插值写回污染
    RestoreSimulatedTransforms();
    
    // 固定时间步长更新
    m_accumulator += deltaTime;
    
    // 限制最大步数，防止螺旋死亡
    const int maxSubSteps = 5;
    int subSteps = 0;
    
    while (m_accumulator >= m_fixedDeltaTime && subSteps < maxSubSteps) {
        FixedUpdate(m_fixedDeltaTime);
        m_accumulator -= m_fixedDeltaTime;
        m_physicsTime += m_fixedDeltaTime;
        subSteps++;
    }
    
    // 缓存物理解算后的真实状态（供下帧恢复）
    CacheSimulatedTransforms();
    
    // 应用插值（用于测试和直接调用 Update 的场景）
    ApplyInterpolation();
}

void PhysicsUpdateSystem::FixedUpdate(float dt) {
    // 1. 应用力和重力
    ApplyForces(dt);
    
    // 2. 积分速度
    IntegrateVelocity(dt);
    
    // 3. 检测需要 CCD 的物体
    std::vector<ECS::EntityID> ccdCandidates = DetectCCDCandidates(dt);
    
    // 4. 根据配置决定使用 CCD 还是标准积分
    if (!ccdCandidates.empty() && m_config.enableCCD) {
        // 使用 CCD 路径积分
        IntegrateWithCCD(dt, ccdCandidates);
    } else {
        // 标准积分（DCD）
        IntegratePosition(dt);
    }
    
    // 5. 碰撞结果处理
    ResolveCollisions(dt);
    
    // 6. 约束求解
    SolveConstraints(dt);
    
    // 7. 休眠检测
    UpdateSleepingState(dt);
    
    // 8. 更新 AABB
    UpdateAABBs();
    
    // 注意：碰撞检测和约束求解在 CollisionDetectionSystem 中处理
}

void PhysicsUpdateSystem::ApplyForces(float dt) {
    // 查询所有具有 RigidBodyComponent 的实体
    auto entities = m_world->Query<ECS::TransformComponent, RigidBodyComponent>();
    
    // 查询所有力场
    auto forceFieldEntities = m_world->Query<ECS::TransformComponent, ForceFieldComponent>();
    
    for (ECS::EntityID entity : entities) {
        if (!m_world->HasComponent<RigidBodyComponent>(entity) ||
            !m_world->HasComponent<ECS::TransformComponent>(entity)) {
            continue;
        }
        
        try {
            auto& body = m_world->GetComponent<RigidBodyComponent>(entity);
            auto& transform = m_world->GetComponent<ECS::TransformComponent>(entity);
            
            // 跳过静态和运动学物体
            if (body.IsStatic() || body.IsKinematic()) {
                continue;
            }
            
            // 跳过休眠物体
            if (body.isSleeping) {
                continue;
            }
            
            // 创建力累加器
            ForceAccumulator accumulator;
            
            // 应用全局重力
            if (body.useGravity && body.inverseMass > 0.0f) {
                Vector3 gravityForce = m_gravity * body.mass * body.gravityScale;
                accumulator.AddForce(gravityForce);
            }
            
            // 应用刚体组件中累积的力/扭矩
            accumulator.AddForce(body.force);
            accumulator.AddTorque(body.torque);
            
            // 应用力场
            Vector3 bodyPosition = transform.GetPosition();
            for (ECS::EntityID fieldEntity : forceFieldEntities) {
                if (!m_world->HasComponent<ForceFieldComponent>(fieldEntity) ||
                    !m_world->HasComponent<ECS::TransformComponent>(fieldEntity)) {
                    continue;
                }
                
                auto& field = m_world->GetComponent<ForceFieldComponent>(fieldEntity);
                auto& fieldTransform = m_world->GetComponent<ECS::TransformComponent>(fieldEntity);
                Vector3 fieldPosition = fieldTransform.GetPosition();
                
                Vector3 fieldForce = ApplyForceField(field, fieldPosition, body, bodyPosition);
                accumulator.AddForce(fieldForce);
            }
            
            // 将累加后的力/扭矩写回刚体，供积分使用
            body.force = accumulator.GetTotalForce();
            body.torque = accumulator.GetTotalTorque();
            
            // 应用冲量带来的速度增量（立即生效）
            const Vector3 linearImpulse = accumulator.GetLinearImpulse();
            const Vector3 angularImpulse = accumulator.GetAngularImpulse();
            if (!linearImpulse.isZero(1e-6f)) {
                body.linearVelocity += linearImpulse;
            }
            if (!angularImpulse.isZero(1e-6f)) {
                body.angularVelocity += angularImpulse;
            }
            
        } catch (...) {
            // 忽略组件访问错误
        }
    }
}

Vector3 PhysicsUpdateSystem::ApplyForceField(
    const ForceFieldComponent& field,
    const Vector3& fieldPosition,
    const RigidBodyComponent& body,
    const Vector3& bodyPosition
) {
    // 力场禁用时不产生任何力
    if (!field.enabled) {
        return Vector3::Zero();
    }

    Vector3 toBody = bodyPosition - fieldPosition;
    float distance = toBody.norm();
    
    // 检查是否在影响范围内
    if (field.radius > 0.0f) {
        if (field.affectOnlyInside && distance > field.radius) {
            return Vector3::Zero();  // 在范围外，不影响
        }
    }
    
    // 计算力场强度（考虑衰减）
    float strength = field.strength;
    if (field.radius > 0.0f && distance > 0.0f) {
        if (field.linearFalloff) {
            // 线性衰减
            float falloff = 1.0f - (distance / field.radius);
            strength *= std::max(0.0f, falloff);
        } else {
            // 平方反比衰减（类似重力）
            float falloff = 1.0f / (1.0f + distance * distance);
            strength *= falloff;
        }
    }
    
    // 根据力场类型计算力
    Vector3 force = Vector3::Zero();
    
    switch (field.type) {
        case ForceFieldComponent::Type::Gravity:
        case ForceFieldComponent::Type::Wind: {
            // 方向力
            force = field.direction * strength * body.mass;
            break;
        }
        
        case ForceFieldComponent::Type::Radial: {
            // 径向力
            if (distance > 0.001f) {
                Vector3 direction = toBody / distance;
                force = direction * strength * body.mass;
            }
            break;
        }
        
        case ForceFieldComponent::Type::Vortex: {
            // 涡流力（垂直于径向和轴的方向）
            if (distance > 0.001f) {
                Vector3 radial = toBody / distance;
                Vector3 tangent = radial.cross(field.direction).normalized();
                force = tangent * strength * body.mass;
            }
            break;
        }
    }
    
    return force;
}

void PhysicsUpdateSystem::IntegrateVelocity(float dt) {
    // 查询所有具有 RigidBodyComponent 的实体
    auto entities = m_world->Query<ECS::TransformComponent, RigidBodyComponent>();
    
    for (ECS::EntityID entity : entities) {
        if (!m_world->HasComponent<RigidBodyComponent>(entity) ||
            !m_world->HasComponent<ECS::TransformComponent>(entity)) {
            continue;
        }
        
        try {
            auto& body = m_world->GetComponent<RigidBodyComponent>(entity);
            auto& transform = m_world->GetComponent<ECS::TransformComponent>(entity);
            m_integrator.IntegrateVelocity(body, &transform, dt);
        } catch (...) {
            // 忽略组件访问错误
        }
    }
}

void PhysicsUpdateSystem::IntegratePosition(float dt) {
    // 查询所有具有 RigidBodyComponent 和 TransformComponent 的实体
    auto entities = m_world->Query<ECS::TransformComponent, RigidBodyComponent>();
    
    for (ECS::EntityID entity : entities) {
        if (!m_world->HasComponent<RigidBodyComponent>(entity) ||
            !m_world->HasComponent<ECS::TransformComponent>(entity)) {
            continue;
        }
        
        try {
            auto& body = m_world->GetComponent<RigidBodyComponent>(entity);
            auto& transform = m_world->GetComponent<ECS::TransformComponent>(entity);
            
            m_integrator.IntegratePosition(body, transform, dt);
        } catch (...) {
            // 忽略组件访问错误
        }
    }
}

void PhysicsUpdateSystem::UpdateAABBs() {
    // 查询所有具有 ColliderComponent 的实体
    auto entities = m_world->Query<ECS::TransformComponent, ColliderComponent>();
    
    for (ECS::EntityID entity : entities) {
        if (!m_world->HasComponent<ColliderComponent>(entity) ||
            !m_world->HasComponent<ECS::TransformComponent>(entity)) {
            continue;
        }
        
        try {
            auto& collider = m_world->GetComponent<ColliderComponent>(entity);
            auto& transform = m_world->GetComponent<ECS::TransformComponent>(entity);
            
            // 更新 AABB
            collider.worldAABB = PhysicsUtils::ComputeWorldAABB(collider, *transform.transform);
            collider.aabbDirty = false;
            
        } catch (...) {
            // 忽略组件访问错误
        }
    }
}

void PhysicsUpdateSystem::RestoreSimulatedTransforms() {
    if (!m_world) {
        return;
    }
    
    auto entities = m_world->Query<ECS::TransformComponent, RigidBodyComponent>();
    for (ECS::EntityID entity : entities) {
        if (!m_world->HasComponent<ECS::TransformComponent>(entity) ||
            !m_world->HasComponent<RigidBodyComponent>(entity)) {
            continue;
        }
        
        try {
            auto& transform = m_world->GetComponent<ECS::TransformComponent>(entity);
            auto it = m_simulatedTransforms.find(entity);
            if (it == m_simulatedTransforms.end()) {
                SimulatedTransformState state;
                state.position = transform.GetPosition();
                state.rotation = transform.GetRotation();
                m_simulatedTransforms[entity] = state;
            } else {
                transform.SetPosition(it->second.position);
                transform.SetRotation(it->second.rotation);
            }
        } catch (...) {
            // 忽略组件访问错误
        }
    }
}

void PhysicsUpdateSystem::CacheSimulatedTransforms() {
    if (!m_world) {
        return;
    }
    
    std::unordered_map<ECS::EntityID, SimulatedTransformState, ECS::EntityID::Hash> newCache;
    auto entities = m_world->Query<ECS::TransformComponent, RigidBodyComponent>();
    for (ECS::EntityID entity : entities) {
        if (!m_world->HasComponent<ECS::TransformComponent>(entity)) {
            continue;
        }
        
        try {
            auto& transform = m_world->GetComponent<ECS::TransformComponent>(entity);
            SimulatedTransformState state;
            state.position = transform.GetPosition();
            state.rotation = transform.GetRotation();
            newCache[entity] = state;
        } catch (...) {
            // 忽略组件访问错误
        }
    }
    
    m_simulatedTransforms.swap(newCache);
}

float PhysicsUpdateSystem::GetInterpolationAlpha() const {
    if (m_fixedDeltaTime > 0.0f) {
        // 当时间余量耗尽时直接使用最新物理解算结果，避免回退到上一帧
        if (m_accumulator <= 1e-6f) {
            return 1.0f;
        } else {
            return std::clamp(m_accumulator / m_fixedDeltaTime, 0.0f, 1.0f);
        }
    }
    return 1.0f;
}

void PhysicsUpdateSystem::ResolveCollisions(float dt) {
    if (!m_world) {
        return;
    }

    CollisionDetectionSystem* collisionSystem = m_world->GetSystem<CollisionDetectionSystem>();
    if (!collisionSystem) {
        return;
    }

    // 使用最新积分后的变换进行碰撞检测，供后续约束解算与唤醒逻辑使用
    collisionSystem->Update(dt);

    const auto& collisionPairs = collisionSystem->GetCollisionPairs();
    if (collisionPairs.empty()) {
        return;
    }

    // 碰撞即时唤醒，避免休眠刚体在求解阶段被跳过
    for (const auto& pair : collisionPairs) {
        if (!m_world->HasComponent<ColliderComponent>(pair.entityA) ||
            !m_world->HasComponent<ColliderComponent>(pair.entityB) ||
            !m_world->HasComponent<RigidBodyComponent>(pair.entityA) ||
            !m_world->HasComponent<RigidBodyComponent>(pair.entityB)) {
            continue;
        }

        try {
            auto& colliderA = m_world->GetComponent<ColliderComponent>(pair.entityA);
            auto& colliderB = m_world->GetComponent<ColliderComponent>(pair.entityB);

            // 触发器不参与解算也不唤醒对方
            if (colliderA.isTrigger || colliderB.isTrigger) {
                continue;
            }

            auto& bodyA = m_world->GetComponent<RigidBodyComponent>(pair.entityA);
            auto& bodyB = m_world->GetComponent<RigidBodyComponent>(pair.entityB);

            if (bodyA.IsDynamic() && bodyA.isSleeping) {
                bodyA.WakeUp();
            }
            if (bodyB.IsDynamic() && bodyB.isSleeping) {
                bodyB.WakeUp();
            }
        } catch (...) {
            // 忽略组件访问错误
        }
    }
}

void PhysicsUpdateSystem::SolveConstraints(float dt) {
    if (!m_world) {
        return;
    }

    CollisionDetectionSystem* collisionSystem = m_world->GetSystem<CollisionDetectionSystem>();
    if (!collisionSystem) {
        return;
    }

    const auto& collisionPairs = collisionSystem->GetCollisionPairs();
    
    // 确保 world 已设置（在 OnCreate 中设置）
    if (m_world) {
        m_constraintSolver.SetWorld(m_world);
        m_constraintSolver.SetSolverIterations(m_solverIterations);
        m_constraintSolver.SetPositionIterations(m_positionIterations);
    }
    
    // 收集所有拥有关节组件的实体
    std::vector<ECS::EntityID> jointEntities;
    auto jointQuery = m_world->Query<PhysicsJointComponent>();
    for (ECS::EntityID entity : jointQuery) {
        if (m_world->HasComponent<PhysicsJointComponent>(entity)) {
            jointEntities.push_back(entity);
        }
    }
    
    // 如果有关节，使用带关节的求解方法；否则使用普通求解方法
    if (!jointEntities.empty()) {
        m_constraintSolver.SolveWithJoints(dt, collisionPairs, jointEntities);
    } else if (!collisionPairs.empty()) {
        m_constraintSolver.Solve(dt, collisionPairs);
    }
}

void PhysicsUpdateSystem::UpdateSleepingState(float dt) {
    if (!m_world) {
        return;
    }

    using EntityID = ECS::EntityID;
    const float sleepDelay = 0.5f;  // 0.5 秒低动能后进入休眠

    // 预取碰撞对，用于碰撞/岛屿唤醒
    CollisionDetectionSystem* collisionSystem = m_world->GetSystem<CollisionDetectionSystem>();
    const std::vector<CollisionPair>* collisionPairs = collisionSystem ? &collisionSystem->GetCollisionPairs() : nullptr;

    // 构建碰撞邻接表，便于岛屿唤醒
    std::unordered_map<EntityID, std::vector<EntityID>, EntityID::Hash> adjacency;
    std::unordered_set<EntityID, EntityID::Hash> wakeSeeds;
    std::unordered_set<EntityID, EntityID::Hash> wokenThisFrame;

    if (collisionPairs) {
        for (const auto& pair : *collisionPairs) {
            if (!m_world->HasComponent<ColliderComponent>(pair.entityA) ||
                !m_world->HasComponent<ColliderComponent>(pair.entityB)) {
                continue;
            }

            try {
                auto& colliderA = m_world->GetComponent<ColliderComponent>(pair.entityA);
                auto& colliderB = m_world->GetComponent<ColliderComponent>(pair.entityB);

                // 触发器不参与物理解算，也不用于唤醒
                if (colliderA.isTrigger || colliderB.isTrigger) {
                    continue;
                }

                adjacency[pair.entityA].push_back(pair.entityB);
                adjacency[pair.entityB].push_back(pair.entityA);

                auto& bodyA = m_world->GetComponent<RigidBodyComponent>(pair.entityA);
                auto& bodyB = m_world->GetComponent<RigidBodyComponent>(pair.entityB);

                bool activeA = bodyA.IsDynamic() && (!bodyA.isSleeping || bodyA.GetKineticEnergy() >= bodyA.sleepThreshold);
                bool activeB = bodyB.IsDynamic() && (!bodyB.isSleeping || bodyB.GetKineticEnergy() >= bodyB.sleepThreshold);

                // 碰撞唤醒：活跃物体撞击休眠物体
                if (activeA && bodyB.IsDynamic() && bodyB.isSleeping) {
                    wakeSeeds.insert(pair.entityB);
                }
                if (activeB && bodyA.IsDynamic() && bodyA.isSleeping) {
                    wakeSeeds.insert(pair.entityA);
                }
                
                // 检查：如果物体已经在 ResolveCollisions 中被唤醒（isSleeping == false），
                // 且对方是活跃的，且 sleepTimer 为 0（说明刚被唤醒），
                // 应该标记为唤醒以确保 sleepTimer 被重置为 0
                // 注意：这里检查 sleepTimer == 0.0f，对于新创建的刚体，它们不在碰撞对中，
                // 所以这个检查是安全的；对于已经在运行一段时间的活跃刚体，它们的 sleepTimer 
                // 可能已经累积，所以不会被误判
                if (activeA && bodyB.IsDynamic() && !bodyB.isSleeping && bodyB.sleepTimer == 0.0f) {
                    wokenThisFrame.insert(pair.entityB);
                }
                if (activeB && bodyA.IsDynamic() && !bodyA.isSleeping && bodyA.sleepTimer == 0.0f) {
                    wokenThisFrame.insert(pair.entityA);
                }
            } catch (...) {
                // 忽略组件访问错误
            }
        }
    }

    // 查询所有刚体
    auto entities = m_world->Query<ECS::TransformComponent, RigidBodyComponent>();

    // 外力/扭矩直接唤醒
    for (EntityID entity : entities) {
        if (!m_world->HasComponent<RigidBodyComponent>(entity)) {
            continue;
        }

        try {
            auto& body = m_world->GetComponent<RigidBodyComponent>(entity);
            if (!body.IsDynamic()) {
                continue;
            }

            if (!body.force.isZero(1e-6f) || !body.torque.isZero(1e-6f)) {
                wakeSeeds.insert(entity);
                wokenThisFrame.insert(entity);
            }
        } catch (...) {
            // 忽略组件访问错误
        }
    }

    // 先将所有唤醒种子唤醒
    for (const auto& entity : wakeSeeds) {
        if (!m_world->HasComponent<RigidBodyComponent>(entity)) {
            continue;
        }
        try {
            auto& body = m_world->GetComponent<RigidBodyComponent>(entity);
            if (body.IsDynamic() && body.isSleeping) {
                body.WakeUp();
                wokenThisFrame.insert(entity);
            }
        } catch (...) {
            // 忽略组件访问错误
        }
    }

    // 岛屿唤醒：将种子通过碰撞邻接传播
    std::queue<EntityID> queue;
    std::unordered_set<EntityID, EntityID::Hash> visited;
    for (const auto& seed : wakeSeeds) {
        if (visited.insert(seed).second) {
            queue.push(seed);
        }
    }

    while (!queue.empty()) {
        EntityID current = queue.front();
        queue.pop();

        auto it = adjacency.find(current);
        if (it == adjacency.end()) {
            continue;
        }

        for (EntityID neighbor : it->second) {
            if (visited.find(neighbor) != visited.end()) {
                continue;
            }

            if (!m_world->HasComponent<RigidBodyComponent>(neighbor)) {
                continue;
            }

            try {
                auto& neighborBody = m_world->GetComponent<RigidBodyComponent>(neighbor);
                if (!neighborBody.IsDynamic()) {
                    continue;
                }

                if (neighborBody.isSleeping) {
                    neighborBody.WakeUp();
                    wokenThisFrame.insert(neighbor);
                }

                visited.insert(neighbor);
                queue.push(neighbor);
            } catch (...) {
                // 忽略组件访问错误
            }
        }
    }

    // 最终休眠检测：低动能持续一段时间后进入休眠
    for (EntityID entity : entities) {
        if (!m_world->HasComponent<RigidBodyComponent>(entity)) {
            continue;
        }

        try {
            auto& body = m_world->GetComponent<RigidBodyComponent>(entity);

            if (!body.IsDynamic()) {
                continue;
            }

            bool wokeThisFrame = wokenThisFrame.find(entity) != wokenThisFrame.end();

            float kineticEnergy = body.GetKineticEnergy();
            float linearSpeedSq = body.linearVelocity.squaredNorm();
            float angularSpeedSq = body.angularVelocity.squaredNorm();
            const float motionEpsilon = 1e-8f;

            // 高动能或被唤醒：重置计时
            if (kineticEnergy >= body.sleepThreshold) {
                body.WakeUp();
                continue;
            }

            // 当前帧被唤醒（碰撞/外力/岛屿传播）：保持清零计时
            if (wokeThisFrame) {
                body.isSleeping = false;
                body.sleepTimer = 0.0f;
                // 确保计时器保持为 0，即使后续逻辑也不会修改
                continue;
            }

            // 仍有可察觉运动：保持清零计时
            if (linearSpeedSq > motionEpsilon || angularSpeedSq > motionEpsilon) {
                body.isSleeping = false;
                body.sleepTimer = 0.0f;
                continue;
            }

            // 低动能累积计时
            body.sleepTimer += dt;

            if (body.sleepTimer >= sleepDelay) {
                body.sleepTimer = sleepDelay;
                body.isSleeping = true;
                body.linearVelocity = Vector3::Zero();
                body.angularVelocity = Vector3::Zero();
                body.force = Vector3::Zero();
                body.torque = Vector3::Zero();
            } else {
                body.isSleeping = false;
            }
        } catch (...) {
            // 忽略组件访问错误
        }
    }
}

void PhysicsUpdateSystem::ApplyForce(ECS::EntityID entity, const Vector3& force) {
    if (!m_world || !m_world->HasComponent<RigidBodyComponent>(entity)) {
        return;
    }
    
    try {
        auto& body = m_world->GetComponent<RigidBodyComponent>(entity);
        body.force += force;
        body.WakeUp();  // 唤醒刚体
    } catch (...) {
        // 忽略错误
    }
}

void PhysicsUpdateSystem::ApplyForceAtPoint(ECS::EntityID entity, const Vector3& force, const Vector3& point) {
    if (!m_world || !m_world->HasComponent<RigidBodyComponent>(entity) ||
        !m_world->HasComponent<ECS::TransformComponent>(entity)) {
        return;
    }
    
    try {
        auto& body = m_world->GetComponent<RigidBodyComponent>(entity);
        auto& transform = m_world->GetComponent<ECS::TransformComponent>(entity);
        
        Vector3 centerOfMass = transform.GetPosition() + body.centerOfMass;
        Vector3 r = point - centerOfMass;
        Vector3 torque = r.cross(force);
        
        body.force += force;
        body.torque += torque;
        body.WakeUp();  // 唤醒刚体
    } catch (...) {
        // 忽略错误
    }
}

void PhysicsUpdateSystem::ApplyTorque(ECS::EntityID entity, const Vector3& torque) {
    if (!m_world || !m_world->HasComponent<RigidBodyComponent>(entity)) {
        return;
    }
    
    try {
        auto& body = m_world->GetComponent<RigidBodyComponent>(entity);
        body.torque += torque;
        body.WakeUp();  // 唤醒刚体
    } catch (...) {
        // 忽略错误
    }
}

void PhysicsUpdateSystem::ApplyImpulse(ECS::EntityID entity, const Vector3& impulse) {
    if (!m_world || !m_world->HasComponent<RigidBodyComponent>(entity)) {
        return;
    }
    
    try {
        auto& body = m_world->GetComponent<RigidBodyComponent>(entity);
        
        // 冲量直接改变速度：Δv = impulse / m = impulse * inverseMass
        body.linearVelocity += impulse * body.inverseMass;
        body.WakeUp();  // 唤醒刚体
    } catch (...) {
        // 忽略错误
    }
}

void PhysicsUpdateSystem::ApplyImpulseAtPoint(ECS::EntityID entity, const Vector3& impulse, const Vector3& point) {
    if (!m_world || !m_world->HasComponent<RigidBodyComponent>(entity) ||
        !m_world->HasComponent<ECS::TransformComponent>(entity)) {
        return;
    }
    
    try {
        auto& body = m_world->GetComponent<RigidBodyComponent>(entity);
        auto& transform = m_world->GetComponent<ECS::TransformComponent>(entity);
        
        // 应用线性冲量
        body.linearVelocity += impulse * body.inverseMass;
        
        // 计算角冲量
        Vector3 centerOfMass = transform.GetPosition() + body.centerOfMass;
        Vector3 r = point - centerOfMass;
        Vector3 angularImpulse = r.cross(impulse);
        
        // 将角冲量转换为角速度变化
        Quaternion rotation = transform.GetRotation();
        Matrix3 rotationMatrix = rotation.toRotationMatrix();
        Matrix3 worldInvInertia = rotationMatrix * body.inverseInertiaTensor * rotationMatrix.transpose();
        
        body.angularVelocity += worldInvInertia * angularImpulse;
        body.WakeUp();  // 唤醒刚体
    } catch (...) {
        // 忽略错误
    }
}

void PhysicsUpdateSystem::ApplyAngularImpulse(ECS::EntityID entity, const Vector3& angularImpulse) {
    if (!m_world || !m_world->HasComponent<RigidBodyComponent>(entity) ||
        !m_world->HasComponent<ECS::TransformComponent>(entity)) {
        return;
    }
    
    try {
        auto& body = m_world->GetComponent<RigidBodyComponent>(entity);
        auto& transform = m_world->GetComponent<ECS::TransformComponent>(entity);
        
        // 将角冲量转换为角速度变化
        Quaternion rotation = transform.GetRotation();
        Matrix3 rotationMatrix = rotation.toRotationMatrix();
        Matrix3 worldInvInertia = rotationMatrix * body.inverseInertiaTensor * rotationMatrix.transpose();
        
        body.angularVelocity += worldInvInertia * angularImpulse;
        body.WakeUp();  // 唤醒刚体
    } catch (...) {
        // 忽略错误
    }
}

void PhysicsUpdateSystem::ApplyInterpolation() {
    if (!m_world) {
        return;
    }
    
    float alpha = GetInterpolationAlpha();
    float t = std::clamp(alpha, 0.0f, 1.0f);
    
    // 如果 alpha 接近 1.0，不需要插值，直接使用物理解算结果
    if (t >= 1.0f - 1e-6f) {
        return;
    }
    
    // 查询所有具有 RigidBodyComponent 和 TransformComponent 的实体
    auto entities = m_world->Query<ECS::TransformComponent, RigidBodyComponent>();
    
    for (ECS::EntityID entity : entities) {
        if (!m_world->HasComponent<ECS::TransformComponent>(entity) ||
            !m_world->HasComponent<RigidBodyComponent>(entity)) {
            continue;
        }
        
        try {
            auto& transform = m_world->GetComponent<ECS::TransformComponent>(entity);
            auto& body = m_world->GetComponent<RigidBodyComponent>(entity);
            
            // 只处理动态物体（Kinematic/Static 不需要插值）
            if (!body.IsDynamic()) {
                continue;
            }
            
            // 只处理根对象（无父实体的对象）
            if (transform.GetParentEntity().IsValid()) {
                continue;
            }
            
            // 获取上一帧和当前帧的状态
            // previousPosition 在 IntegratePosition 中被设置为积分前的位置
            Vector3 previousPos = body.previousPosition;
            Quaternion previousRot = body.previousRotation;
            
            // 从缓存获取当前物理解算后的状态
            Vector3 currentPos;
            Quaternion currentRot;
            
            auto it = m_simulatedTransforms.find(entity);
            if (it != m_simulatedTransforms.end()) {
                currentPos = it->second.position;
                currentRot = it->second.rotation;
            } else {
                // 如果没有缓存，使用当前 Transform（这种情况不应该发生）
                currentPos = transform.GetPosition();
                currentRot = transform.GetRotation();
            }
            
            // 进行插值：在 previousPosition 和 currentPosition 之间插值
            Vector3 interpolatedPos = previousPos + (currentPos - previousPos) * t;
            Quaternion interpolatedRot = previousRot.slerp(t, currentRot);
            
            // 更新 Transform
            transform.SetPosition(interpolatedPos);
            transform.SetRotation(interpolatedRot);
            
        } catch (...) {
            // 忽略组件访问错误
        }
    }
}

// ============================================================================
// CCD 集成方法
// ============================================================================

std::vector<ECS::EntityID> PhysicsUpdateSystem::DetectCCDCandidates(float dt) {
    std::vector<ECS::EntityID> candidates;
    
    if (!m_world) {
        return candidates;
    }
    
    auto entities = m_world->Query<ECS::TransformComponent, RigidBodyComponent, ColliderComponent>();
    
    int count = 0;
    for (ECS::EntityID entity : entities) {
        if (count >= m_config.maxCCDObjects) {
            break;  // 达到最大 CCD 对象数限制
        }
        
        if (!m_world->HasComponent<RigidBodyComponent>(entity) ||
            !m_world->HasComponent<ColliderComponent>(entity) ||
            !m_world->HasComponent<ECS::TransformComponent>(entity)) {
            continue;
        }
        
        try {
            auto& body = m_world->GetComponent<RigidBodyComponent>(entity);
            auto& collider = m_world->GetComponent<ColliderComponent>(entity);
            auto& transform = m_world->GetComponent<ECS::TransformComponent>(entity);
            
            bool shouldUseCCD = false;
            
            // 策略 1: 用户强制启用
            if (body.useCCD) {
                shouldUseCCD = true;
            }
            // 策略 2: 使用 CCDCandidateDetector 判断（速度/位移阈值）
            else if (CCDCandidateDetector::ShouldUseCCD(
                body, collider, dt,
                m_config.ccdVelocityThreshold,
                m_config.ccdDisplacementThreshold
            )) {
                shouldUseCCD = true;
            }
            // 策略 3: 薄片物体检测（地面、墙壁等容易穿透的物体）
            else if (CCDBroadPhase::IsThinObject(collider, transform)) {
                shouldUseCCD = true;
            }
            
            if (shouldUseCCD) {
                candidates.push_back(entity);
                count++;
            }
        } catch (...) {
            // 忽略组件访问错误
        }
    }
    
    return candidates;
}

void PhysicsUpdateSystem::IntegrateWithCCD(float dt, const std::vector<ECS::EntityID>& candidates) {
    if (!m_world) {
        return;
    }
    
    // 创建形状缓存（避免重复创建，性能优化）
    std::unordered_map<ECS::EntityID, std::unique_ptr<CollisionShape>, ECS::EntityID::Hash> shapeCache;
    
    // 预创建所有候选物体的形状（缓存优化）
    for (ECS::EntityID candidateEntity : candidates) {
        if (!m_world->HasComponent<ColliderComponent>(candidateEntity)) {
            continue;
        }
        
        try {
            auto& collider = m_world->GetComponent<ColliderComponent>(candidateEntity);
            std::unique_ptr<CollisionShape> shape = CreateShapeFromCollider(collider);
            if (shape) {
                shapeCache[candidateEntity] = std::move(shape);
            }
        } catch (...) {
            // 忽略组件访问错误
        }
    }
    
    // 对所有 CCD 候选物体进行检测
    for (ECS::EntityID candidateEntity : candidates) {
        if (!m_world->HasComponent<RigidBodyComponent>(candidateEntity) ||
            !m_world->HasComponent<ColliderComponent>(candidateEntity) ||
            !m_world->HasComponent<ECS::TransformComponent>(candidateEntity)) {
            continue;
        }
        
        try {
            auto& body = m_world->GetComponent<RigidBodyComponent>(candidateEntity);
            auto& collider = m_world->GetComponent<ColliderComponent>(candidateEntity);
            auto& transform = m_world->GetComponent<ECS::TransformComponent>(candidateEntity);
            
            // 保存上一帧位置（用于 CCD）
            body.previousPosition = transform.GetPosition();
            body.previousRotation = transform.GetRotation();
            
            // 从缓存获取候选物体的形状（性能优化）
            auto shapeIt = shapeCache.find(candidateEntity);
            if (shapeIt == shapeCache.end()) {
                // 缓存中没有，创建新的
                std::unique_ptr<CollisionShape> newShape = CreateShapeFromCollider(collider);
                if (!newShape) {
                    // 不支持该形状类型，使用标准积分
                    IntegratePositionToTime(candidateEntity, dt);
                    continue;
                }
                shapeCache[candidateEntity] = std::move(newShape);
                shapeIt = shapeCache.find(candidateEntity);
            }
            
            CollisionShape* candidateShape = shapeIt->second.get();
            
            // 获取候选物体的初始状态
            Vector3 posA0 = body.previousPosition;
            Vector3 velA = body.linearVelocity;
            Quaternion rotA0 = body.previousRotation;
            Vector3 angularVelA = body.angularVelocity;
            
            // 检测与所有其他物体的 CCD 碰撞
            struct CollisionInfo {
                CCDResult result;
                ECS::EntityID otherEntity;
            };
            std::vector<CollisionInfo> collisions;
            
            // 使用 Broad Phase CCD 筛选潜在碰撞对（如果启用）
            std::vector<ECS::EntityID> candidateList = {candidateEntity};
            std::vector<std::pair<ECS::EntityID, ECS::EntityID>> ccdPairs;
            
            if (m_config.enableBroadPhaseCCD) {
                // 使用 Broad Phase 快速筛选
                ccdPairs = CCDBroadPhase::FilterCCDPairs(candidateList, m_world, dt);
            } else {
                // 不使用 Broad Phase，检测所有物体
                auto allEntities = m_world->Query<ECS::TransformComponent, RigidBodyComponent, ColliderComponent>();
                for (ECS::EntityID otherEntity : allEntities) {
                    if (otherEntity != candidateEntity) {
                        ccdPairs.push_back({candidateEntity, otherEntity});
                    }
                }
            }
            
            // 对筛选后的对进行精确 CCD 检测
            for (const auto& pair : ccdPairs) {
                ECS::EntityID otherEntity = (pair.first == candidateEntity) ? pair.second : pair.first;
                
                if (!m_world->HasComponent<RigidBodyComponent>(otherEntity) ||
                    !m_world->HasComponent<ColliderComponent>(otherEntity) ||
                    !m_world->HasComponent<ECS::TransformComponent>(otherEntity)) {
                    continue;
                }
                
                try {
                    auto& otherBody = m_world->GetComponent<RigidBodyComponent>(otherEntity);
                    auto& otherCollider = m_world->GetComponent<ColliderComponent>(otherEntity);
                    auto& otherTransform = m_world->GetComponent<ECS::TransformComponent>(otherEntity);
                    
                    // 使用选择性 CCD 策略：检查是否应该进行 CCD
                    if (!CCDBroadPhase::ShouldPerformCCD(candidateEntity, otherEntity, m_world)) {
                        continue;
                    }
                    
                    // 从缓存获取其他物体的形状（性能优化）
                    CollisionShape* otherShape = nullptr;
                    auto otherShapeIt = shapeCache.find(otherEntity);
                    if (otherShapeIt == shapeCache.end()) {
                        // 缓存中没有，创建新的并缓存
                        std::unique_ptr<CollisionShape> newShape = CreateShapeFromCollider(otherCollider);
                        if (!newShape) {
                            continue;
                        }
                        otherShape = newShape.get();
                        shapeCache[otherEntity] = std::move(newShape);
                    } else {
                        otherShape = otherShapeIt->second.get();
                    }
                    
                    // 获取其他物体的状态
                    Vector3 posB0 = otherBody.previousPosition.isZero() ? 
                        otherTransform.GetPosition() : otherBody.previousPosition;
                    Vector3 velB = otherBody.linearVelocity;
                    Quaternion rotB0 = otherBody.previousRotation.isApprox(Quaternion::Identity()) ?
                        otherTransform.GetRotation() : otherBody.previousRotation;
                    Vector3 angularVelB = otherBody.angularVelocity;
                    
                    // 执行 CCD 检测
                    CCDResult result;
                    if (CCDDetector::Detect(
                        candidateShape, posA0, velA, rotA0, angularVelA,
                        otherShape, posB0, velB, rotB0, angularVelB,
                        dt, result
                    )) {
                        CollisionInfo info;
                        info.result = result;
                        info.otherEntity = otherEntity;
                        collisions.push_back(info);
                    }
                } catch (...) {
                    // 忽略组件访问错误
                }
            }
            
            if (!collisions.empty()) {
                // 找到最早的碰撞
                auto earliest = std::min_element(
                    collisions.begin(), collisions.end(),
                    [](const CollisionInfo& a, const CollisionInfo& b) {
                        return a.result.toi < b.result.toi;
                    }
                );
                
                // 积分到 TOI
                float toi = earliest->result.toi * dt;
                IntegratePositionToTime(candidateEntity, toi);
                
                // 处理碰撞
                HandleCCDCollision(candidateEntity, earliest->result, earliest->otherEntity);
                
                // 递归处理剩余时间（简化：剩余时间使用标准积分）
                if (toi < dt - MathUtils::EPSILON) {
                    float remainingTime = dt - toi;
                    IntegratePositionToTime(candidateEntity, remainingTime);
                }
            } else {
                // 无碰撞，使用标准积分
                IntegratePositionToTime(candidateEntity, dt);
            }
        } catch (...) {
            // 忽略组件访问错误
        }
    }
    
    // 对非 CCD 物体使用标准积分
    auto allEntities = m_world->Query<ECS::TransformComponent, RigidBodyComponent>();
    std::unordered_set<ECS::EntityID, ECS::EntityID::Hash> ccdSet(candidates.begin(), candidates.end());
    
    for (ECS::EntityID entity : allEntities) {
        if (ccdSet.find(entity) == ccdSet.end()) {
            // 非 CCD 物体，使用标准积分
            IntegratePositionToTime(entity, dt);
        }
    }
}

void PhysicsUpdateSystem::IntegratePositionToTime(ECS::EntityID entity, float dt) {
    if (!m_world) {
        return;
    }
    
    if (!m_world->HasComponent<RigidBodyComponent>(entity) ||
        !m_world->HasComponent<ECS::TransformComponent>(entity)) {
        return;
    }
    
    try {
        auto& body = m_world->GetComponent<RigidBodyComponent>(entity);
        auto& transform = m_world->GetComponent<ECS::TransformComponent>(entity);
        
        m_integrator.IntegratePosition(body, transform, dt);
    } catch (...) {
        // 忽略组件访问错误
    }
}

void PhysicsUpdateSystem::HandleCCDCollision(ECS::EntityID entity, const CCDResult& result, ECS::EntityID otherEntity) {
    if (!m_world) {
        return;
    }
    
    if (!m_world->HasComponent<RigidBodyComponent>(entity)) {
        return;
    }
    
    try {
        auto& body = m_world->GetComponent<RigidBodyComponent>(entity);
        
        // 存储 CCD 碰撞信息
        body.ccdCollision.occurred = true;
        body.ccdCollision.toi = result.toi;
        body.ccdCollision.collisionPoint = result.collisionPoint;
        body.ccdCollision.collisionNormal = result.collisionNormal;
        body.ccdCollision.otherEntity = otherEntity;
        
        // 简化的碰撞响应：停止在碰撞点
        // 实际应该使用更复杂的碰撞响应算法
        if (body.IsDynamic()) {
            // 将速度投影到碰撞法线方向，移除穿透分量
            float velocityAlongNormal = body.linearVelocity.dot(result.collisionNormal);
            if (velocityAlongNormal < 0.0f) {
                body.linearVelocity -= result.collisionNormal * velocityAlongNormal;
            }
        }
    } catch (...) {
        // 忽略组件访问错误
    }
}

// ============================================================================
// 辅助函数：从 ColliderComponent 创建 CollisionShape
// ============================================================================

std::unique_ptr<CollisionShape> PhysicsUpdateSystem::CreateShapeFromCollider(const ColliderComponent& collider) {
    switch (collider.shapeType) {
        case ColliderComponent::ShapeType::Sphere: {
            return std::make_unique<SphereShape>(collider.shapeData.sphere.radius);
        }
        case ColliderComponent::ShapeType::Box: {
            Vector3 halfExtents(
                collider.shapeData.box.halfExtents[0],
                collider.shapeData.box.halfExtents[1],
                collider.shapeData.box.halfExtents[2]
            );
            return std::make_unique<BoxShape>(halfExtents);
        }
        case ColliderComponent::ShapeType::Capsule: {
            return std::make_unique<CapsuleShape>(
                collider.shapeData.capsule.radius,
                collider.shapeData.capsule.height
            );
        }
        default:
            // 暂不支持 Mesh 和 ConvexHull
            return nullptr;
    }
}

} // namespace Physics
} // namespace Render
