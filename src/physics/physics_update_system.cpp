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
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/logger.h"
#include <cmath>

namespace Render {
namespace Physics {

PhysicsUpdateSystem::PhysicsUpdateSystem() {
    m_gravity = Vector3(0.0f, -9.81f, 0.0f);
    m_fixedDeltaTime = 1.0f / 60.0f;
    m_accumulator = 0.0f;
    m_physicsTime = 0.0f;
}

void PhysicsUpdateSystem::Update(float deltaTime) {
    if (!m_world) {
        return;
    }
    
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
}

void PhysicsUpdateSystem::FixedUpdate(float dt) {
    // 1. 应用力和重力
    ApplyForces(dt);
    
    // 2. 积分速度
    IntegrateVelocity(dt);
    
    // 3. 积分位置
    IntegratePosition(dt);
    
    // 4. 更新 AABB
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

} // namespace Physics
} // namespace Render
