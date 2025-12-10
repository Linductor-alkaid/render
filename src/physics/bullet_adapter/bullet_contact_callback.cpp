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
#include "render/physics/bullet_adapter/bullet_contact_callback.h"
#include "render/physics/bullet_adapter/eigen_to_bullet.h"
#include <BulletCollision/CollisionDispatch/btCollisionObject.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <algorithm>

namespace Render::Physics::BulletAdapter {

BulletContactCallback::BulletContactCallback(
    const std::unordered_map<ECS::EntityID, btRigidBody*, ECS::EntityID::Hash>& entityToRigidBody,
    const std::unordered_map<btRigidBody*, ECS::EntityID>& rigidBodyToEntity)
    : m_entityToRigidBody(entityToRigidBody)
    , m_rigidBodyToEntity(rigidBodyToEntity) {
    // 设置碰撞过滤回调标志
    m_collisionFilterGroup = 0;
    m_collisionFilterMask = 0;
}

btScalar BulletContactCallback::addSingleResult(btManifoldPoint& cp,
                                                const btCollisionObjectWrapper* colObj0Wrap,
                                                int partId0, int index0,
                                                const btCollisionObjectWrapper* colObj1Wrap,
                                                int partId1, int index1) {
    // ==================== 2.3.3 碰撞事件回调 ====================
    
    // 获取碰撞对象
    const btCollisionObject* obj0 = colObj0Wrap->getCollisionObject();
    const btCollisionObject* obj1 = colObj1Wrap->getCollisionObject();
    
    if (!obj0 || !obj1) {
        return 0.0f;
    }
    
    // 转换为刚体（如果不是刚体，跳过）
    const btRigidBody* body0 = btRigidBody::upcast(obj0);
    const btRigidBody* body1 = btRigidBody::upcast(obj1);
    
    if (!body0 || !body1) {
        return 0.0f;
    }
    
    // 查找对应的实体ID
    auto it0 = m_rigidBodyToEntity.find(const_cast<btRigidBody*>(body0));
    auto it1 = m_rigidBodyToEntity.find(const_cast<btRigidBody*>(body1));
    
    if (it0 == m_rigidBodyToEntity.end() || it1 == m_rigidBodyToEntity.end()) {
        return 0.0f;  // 不是我们管理的实体
    }
    
    ECS::EntityID entityA = it0->second;
    ECS::EntityID entityB = it1->second;
    
    // 确保 entityA < entityB（用于一致性）
    if (entityB < entityA) {
        std::swap(entityA, entityB);
    }
    
    // 转换接触点信息
    ContactManifold manifold = ConvertManifold(cp, colObj0Wrap, colObj1Wrap);
    
    // 查找或创建碰撞对
    uint64_t pairHash = HashPair(entityA, entityB);
    auto pairIt = m_currentPairs.find(pairHash);
    
    if (pairIt == m_currentPairs.end()) {
        // 创建新的碰撞对
        CollisionPair pair(entityA, entityB, manifold);
        m_currentPairs[pairHash] = pair;
    } else {
        // 合并接触点到现有流形（最多4个点）
        CollisionPair& pair = pairIt->second;
        if (pair.manifold.contactCount < 4) {
            // 添加新的接触点
            const ContactPoint& newPoint = manifold.contacts[0];
            pair.manifold.AddContact(
                newPoint.position,
                newPoint.localPointA,
                newPoint.localPointB,
                newPoint.penetration
            );
        }
        // 更新法线和最大穿透深度
        if (manifold.penetration > pair.manifold.penetration) {
            pair.manifold.penetration = manifold.penetration;
            pair.manifold.normal = manifold.normal;
        }
    }
    
    return 0.0f;  // 返回0表示继续处理其他接触点
}

uint64_t BulletContactCallback::HashPair(ECS::EntityID a, ECS::EntityID b) {
    // 确保 a < b（用于一致性）
    if (b < a) {
        std::swap(a, b);
    }
    
    // 使用简单的哈希组合
    uint64_t hashA = static_cast<uint64_t>(a.index);
    uint64_t hashB = static_cast<uint64_t>(b.index);
    
    // 使用位操作组合哈希值
    return (hashA << 32) | hashB;
}

ContactManifold BulletContactCallback::ConvertManifold(const btManifoldPoint& cp,
                                                       const btCollisionObjectWrapper* colObj0Wrap,
                                                       const btCollisionObjectWrapper* colObj1Wrap) {
    ContactManifold manifold;
    
    // 获取碰撞法线（从物体A指向物体B）
    btVector3 normalWorldOnB = cp.m_normalWorldOnB;
    manifold.SetNormal(FromBullet(normalWorldOnB));
    
    // 获取接触点位置（世界空间）
    btVector3 pointWorldOnB = cp.getPositionWorldOnB();
    Vector3 contactPosition = FromBullet(pointWorldOnB);
    
    // 获取穿透深度（正值表示穿透）
    float penetration = cp.getDistance();
    
    // 获取局部空间接触点
    btVector3 localPointA = cp.m_localPointA;
    btVector3 localPointB = cp.m_localPointB;
    
    Vector3 localA = FromBullet(localPointA);
    Vector3 localB = FromBullet(localPointB);
    
    // 添加接触点
    manifold.AddContact(contactPosition, localA, localB, penetration);
    
    return manifold;
}

} // namespace Render::Physics::BulletAdapter

