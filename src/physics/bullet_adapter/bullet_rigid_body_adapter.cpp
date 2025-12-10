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
#include "render/physics/bullet_adapter/bullet_rigid_body_adapter.h"
#include "render/physics/bullet_adapter/eigen_to_bullet.h"
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <BulletCollision/CollisionShapes/btCollisionShape.h>
#include <BulletCollision/CollisionDispatch/btCollisionObject.h>
#include <algorithm>
#include <limits>

namespace Render::Physics::BulletAdapter {

BulletRigidBodyAdapter::BulletRigidBodyAdapter(btRigidBody* bulletBody, ECS::EntityID entity)
    : m_bulletBody(bulletBody)
    , m_entity(entity) {
}

void BulletRigidBodyAdapter::SyncToBullet(const RigidBodyComponent& component) {
    if (!m_bulletBody) {
        return;
    }
    
    // ==================== 1.4.2 刚体类型转换 ====================
    int flags = m_bulletBody->getCollisionFlags();
    
    // 清除之前的类型标志
    flags &= ~(btCollisionObject::CF_STATIC_OBJECT | btCollisionObject::CF_KINEMATIC_OBJECT);
    
    switch (component.type) {
        case RigidBodyComponent::BodyType::Static:
            flags |= btCollisionObject::CF_STATIC_OBJECT;
            break;
        case RigidBodyComponent::BodyType::Kinematic:
            flags |= btCollisionObject::CF_KINEMATIC_OBJECT;
            break;
        case RigidBodyComponent::BodyType::Dynamic:
            // 动态物体不需要特殊标志
            break;
    }
    m_bulletBody->setCollisionFlags(flags);
    
    // ==================== 1.4.3 质量属性同步 ====================
    btVector3 localInertia(0.0f, 0.0f, 0.0f);
    
    // 根据刚体类型设置质量
    float mass = 0.0f;
    if (component.type == RigidBodyComponent::BodyType::Dynamic && component.mass > 0.0f) {
        mass = component.mass;
        
        // 计算局部惯性（从惯性张量提取对角线元素）
        // Bullet 期望的是局部惯性向量（对角线元素）
        // 注意：Bullet 的惯性张量是相对于质心的，而我们的 inertiaTensor 也是相对于质心的
        localInertia.setX(component.inertiaTensor(0, 0));
        localInertia.setY(component.inertiaTensor(1, 1));
        localInertia.setZ(component.inertiaTensor(2, 2));
        
        // 如果惯性张量接近零或无效，使用默认值
        if (localInertia.length2() < 1e-6f) {
            // 尝试从形状计算惯性（如果形状存在）
            btCollisionShape* shape = m_bulletBody->getCollisionShape();
            if (shape) {
                shape->calculateLocalInertia(mass, localInertia);
            } else {
                // 回退到单位惯性
                localInertia.setValue(1.0f, 1.0f, 1.0f);
            }
        }
    }
    
    m_bulletBody->setMassProps(mass, localInertia);
    
    // 注意：setMassProps 可能会修改碰撞标志（特别是当质量为 0 时）
    // 因此需要在设置质量后重新设置碰撞标志，确保类型标志正确
    flags = m_bulletBody->getCollisionFlags();
    flags &= ~(btCollisionObject::CF_STATIC_OBJECT | btCollisionObject::CF_KINEMATIC_OBJECT);
    switch (component.type) {
        case RigidBodyComponent::BodyType::Static:
            flags |= btCollisionObject::CF_STATIC_OBJECT;
            break;
        case RigidBodyComponent::BodyType::Kinematic:
            flags |= btCollisionObject::CF_KINEMATIC_OBJECT;
            break;
        case RigidBodyComponent::BodyType::Dynamic:
            // 动态物体不需要特殊标志
            break;
    }
    m_bulletBody->setCollisionFlags(flags);
    
    // ==================== 1.4.4 速度约束同步 ====================
    // 位置锁定：使用 setLinearFactor
    btVector3 linearFactor(1.0f, 1.0f, 1.0f);
    if (component.lockPosition[0]) linearFactor.setX(0.0f);
    if (component.lockPosition[1]) linearFactor.setY(0.0f);
    if (component.lockPosition[2]) linearFactor.setZ(0.0f);
    m_bulletBody->setLinearFactor(linearFactor);
    
    // 旋转锁定：使用 setAngularFactor
    btVector3 angularFactor(1.0f, 1.0f, 1.0f);
    if (component.lockRotation[0]) angularFactor.setX(0.0f);
    if (component.lockRotation[1]) angularFactor.setY(0.0f);
    if (component.lockRotation[2]) angularFactor.setZ(0.0f);
    m_bulletBody->setAngularFactor(angularFactor);
    
    // 最大速度限制（需要在每帧手动检查，Bullet 不直接支持）
    // 这里只同步速度值，限制逻辑在 SyncFromBullet 中处理
    
    // ==================== 1.4.5 阻尼同步 ====================
    m_bulletBody->setDamping(component.linearDamping, component.angularDamping);
    
    // ==================== 1.4.6 重力同步 ====================
    // 注意：Bullet 的重力是在世界级别设置的，这里通过设置每个刚体的重力来控制
    // 如果 useGravity=false，将重力设置为零向量
    // gravityScale 需要在应用重力时手动处理（在世界适配器中）
    // 这里先设置为零，实际的重力会在世界适配器中根据 useGravity 和 gravityScale 设置
    if (!component.useGravity) {
        m_bulletBody->setGravity(btVector3(0.0f, 0.0f, 0.0f));
    }
    // 如果 useGravity=true，重力会在世界适配器中设置（乘以 gravityScale）
    
    // ==================== 1.4.7 CCD 同步 ====================
    bool shouldUseCCD = component.useCCD;
    
    // 如果未强制启用，根据速度阈值自动判断
    if (!shouldUseCCD) {
        float linearSpeed = component.linearVelocity.norm();
        if (linearSpeed > component.ccdVelocityThreshold) {
            shouldUseCCD = true;
        }
    }
    
    if (shouldUseCCD) {
        // 设置 CCD 运动阈值（相对于形状尺寸）
        // 使用一个合理的默认值，或者根据形状尺寸计算
        btCollisionShape* shape = m_bulletBody->getCollisionShape();
        if (shape) {
            // 计算形状的包围盒尺寸
            btVector3 aabbMin, aabbMax;
            shape->getAabb(btTransform::getIdentity(), aabbMin, aabbMax);
            btVector3 aabbSize = aabbMax - aabbMin;
            float shapeSize = std::max({aabbSize.x(), aabbSize.y(), aabbSize.z()});
            
            // CCD 阈值 = 形状尺寸 * 阈值比例
            float ccdThreshold = shapeSize * component.ccdDisplacementThreshold;
            m_bulletBody->setCcdMotionThreshold(ccdThreshold);
            
            // 设置扫描球半径（用于 CCD 检测）
            // 通常设置为形状尺寸的一小部分
            float sweptSphereRadius = shapeSize * 0.1f;
            m_bulletBody->setCcdSweptSphereRadius(sweptSphereRadius);
        } else {
            // 没有形状，使用默认值
            m_bulletBody->setCcdMotionThreshold(0.1f);
            m_bulletBody->setCcdSweptSphereRadius(0.1f);
        }
    } else {
        // 禁用 CCD
        m_bulletBody->setCcdMotionThreshold(0.0f);
        m_bulletBody->setCcdSweptSphereRadius(0.0f);
    }
    
    // ==================== 1.4.8 休眠状态同步 ====================
    // 设置休眠阈值
    // Bullet 使用线性和角速度阈值，我们使用动能阈值
    // 将动能阈值转换为速度阈值（简化处理）
    float linearThreshold = std::sqrt(component.sleepThreshold * 2.0f / std::max(component.mass, 0.001f));
    float angularThreshold = linearThreshold;  // 简化：使用相同的阈值
    m_bulletBody->setSleepingThresholds(linearThreshold, angularThreshold);
    
    // 同步休眠状态
    if (component.isSleeping) {
        m_bulletBody->setActivationState(ISLAND_SLEEPING);
    } else {
        // 唤醒刚体
        m_bulletBody->setActivationState(ACTIVE_TAG);
    }
    
    // ==================== 运动状态同步 ====================
    // 同步速度（注意：对于 Kinematic/Static 物体，速度可能由外部设置）
    if (component.type == RigidBodyComponent::BodyType::Dynamic) {
        m_bulletBody->setLinearVelocity(ToBullet(component.linearVelocity));
        m_bulletBody->setAngularVelocity(ToBullet(component.angularVelocity));
    } else if (component.type == RigidBodyComponent::BodyType::Kinematic) {
        // Kinematic 物体也可以有速度（用于插值）
        m_bulletBody->setLinearVelocity(ToBullet(component.linearVelocity));
        m_bulletBody->setAngularVelocity(ToBullet(component.angularVelocity));
    }
    
    // 同步力和扭矩（需要清除之前的累积，然后应用新的）
    // 注意：Bullet 的 applyForce/applyTorque 是累积的，所以需要先清除
    m_bulletBody->clearForces();
    if (component.force.squaredNorm() > 1e-6f) {
        m_bulletBody->applyCentralForce(ToBullet(component.force));
    }
    if (component.torque.squaredNorm() > 1e-6f) {
        m_bulletBody->applyTorque(ToBullet(component.torque));
    }
}

void BulletRigidBodyAdapter::SyncFromBullet(RigidBodyComponent& component) {
    if (!m_bulletBody) {
        return;
    }
    
    // ==================== 刚体类型同步 ====================
    int flags = m_bulletBody->getCollisionFlags();
    if (flags & btCollisionObject::CF_STATIC_OBJECT) {
        component.type = RigidBodyComponent::BodyType::Static;
    } else if (flags & btCollisionObject::CF_KINEMATIC_OBJECT) {
        component.type = RigidBodyComponent::BodyType::Kinematic;
    } else {
        component.type = RigidBodyComponent::BodyType::Dynamic;
    }
    
    // ==================== 质量属性同步 ====================
    float mass = m_bulletBody->getMass();
    component.mass = mass;
    if (mass > 0.0f) {
        component.inverseMass = 1.0f / mass;
    } else {
        component.inverseMass = 0.0f;
    }
    
    // 同步惯性张量（从 Bullet 的局部惯性向量重建）
    // 注意：Bullet 只存储对角线元素，我们假设是对角矩阵
    btVector3 localInertia = m_bulletBody->getLocalInertia();
    component.inertiaTensor.setZero();
    component.inertiaTensor(0, 0) = localInertia.x();
    component.inertiaTensor(1, 1) = localInertia.y();
    component.inertiaTensor(2, 2) = localInertia.z();
    
    // 计算逆惯性张量
    if (component.type == RigidBodyComponent::BodyType::Static || mass <= 0.0f) {
        component.inverseInertiaTensor = Matrix3::Zero();
    } else {
        component.inverseInertiaTensor.setZero();
        if (localInertia.x() > 1e-6f) component.inverseInertiaTensor(0, 0) = 1.0f / localInertia.x();
        if (localInertia.y() > 1e-6f) component.inverseInertiaTensor(1, 1) = 1.0f / localInertia.y();
        if (localInertia.z() > 1e-6f) component.inverseInertiaTensor(2, 2) = 1.0f / localInertia.z();
    }
    
    // ==================== 速度约束同步 ====================
    btVector3 linearFactor = m_bulletBody->getLinearFactor();
    component.lockPosition[0] = (linearFactor.x() < 0.5f);
    component.lockPosition[1] = (linearFactor.y() < 0.5f);
    component.lockPosition[2] = (linearFactor.z() < 0.5f);
    
    btVector3 angularFactor = m_bulletBody->getAngularFactor();
    component.lockRotation[0] = (angularFactor.x() < 0.5f);
    component.lockRotation[1] = (angularFactor.y() < 0.5f);
    component.lockRotation[2] = (angularFactor.z() < 0.5f);
    
    // ==================== 阻尼同步 ====================
    component.linearDamping = m_bulletBody->getLinearDamping();
    component.angularDamping = m_bulletBody->getAngularDamping();
    
    // ==================== 重力同步 ====================
    // 检查重力向量是否为零（表示不受重力）
    btVector3 gravity = m_bulletBody->getGravity();
    component.useGravity = (gravity.length2() > 1e-6f);
    // 注意：gravityScale 不在 Bullet 中存储，需要保持原值
    
    // ==================== CCD 同步 ====================
    float ccdThreshold = m_bulletBody->getCcdMotionThreshold();
    component.useCCD = (ccdThreshold > 1e-6f);
    // 注意：ccdVelocityThreshold 和 ccdDisplacementThreshold 不在 Bullet 中存储，需要保持原值
    
    // ==================== 休眠状态同步 ====================
    int activationState = m_bulletBody->getActivationState();
    component.isSleeping = (activationState == ISLAND_SLEEPING);
    
    // 同步休眠阈值（从速度阈值转换回动能阈值，简化处理）
    float linearThreshold = m_bulletBody->getLinearSleepingThreshold();
    float angularThreshold = m_bulletBody->getAngularSleepingThreshold();
    // 简化：使用线性阈值估算动能阈值
    component.sleepThreshold = 0.5f * component.mass * linearThreshold * linearThreshold;
    
    // ==================== 运动状态同步 ====================
    // 同步速度
    component.linearVelocity = FromBullet(m_bulletBody->getLinearVelocity());
    component.angularVelocity = FromBullet(m_bulletBody->getAngularVelocity());
    
    // 应用最大速度限制
    float linearSpeed = component.linearVelocity.norm();
    if (linearSpeed > component.maxLinearSpeed) {
        component.linearVelocity = component.linearVelocity.normalized() * component.maxLinearSpeed;
        m_bulletBody->setLinearVelocity(ToBullet(component.linearVelocity));
    }
    
    float angularSpeed = component.angularVelocity.norm();
    if (angularSpeed > component.maxAngularSpeed) {
        component.angularVelocity = component.angularVelocity.normalized() * component.maxAngularSpeed;
        m_bulletBody->setAngularVelocity(ToBullet(component.angularVelocity));
    }
    
    // 同步力和扭矩（从 Bullet 获取当前累积的力）
    // 注意：Bullet 的 getTotalForce/getTotalTorque 返回当前累积的力
    component.force = FromBullet(m_bulletBody->getTotalForce());
    component.torque = FromBullet(m_bulletBody->getTotalTorque());
}

} // namespace Render::Physics::BulletAdapter

