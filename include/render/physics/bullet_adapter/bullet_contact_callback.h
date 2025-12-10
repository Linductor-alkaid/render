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

#include "render/physics/collision/contact_manifold.h"
#include "render/ecs/entity.h"
#include <BulletCollision/CollisionDispatch/btCollisionWorld.h>
#include <unordered_map>
#include <unordered_set>

// 前向声明
class btRigidBody;
class btCollisionObject;

namespace Render::Physics::BulletAdapter {

/**
 * @brief Bullet 碰撞回调类
 * 
 * 用于收集碰撞信息并触发碰撞事件
 */
class BulletContactCallback : public btCollisionWorld::ContactResultCallback {
public:
    /**
     * @brief 碰撞对信息
     */
    struct CollisionPair {
        ECS::EntityID entityA;
        ECS::EntityID entityB;
        ContactManifold manifold;
        
        CollisionPair() = default;
        CollisionPair(ECS::EntityID a, ECS::EntityID b, const ContactManifold& m)
            : entityA(a), entityB(b), manifold(m) {}
        
        // 用于哈希和比较
        bool operator==(const CollisionPair& other) const {
            return (entityA == other.entityA && entityB == other.entityB) ||
                   (entityA == other.entityB && entityB == other.entityA);
        }
    };
    
    /**
     * @brief 构造函数
     * @param entityToRigidBody 实体到刚体的映射
     */
    explicit BulletContactCallback(
        const std::unordered_map<ECS::EntityID, btRigidBody*, ECS::EntityID::Hash>& entityToRigidBody,
        const std::unordered_map<btRigidBody*, ECS::EntityID>& rigidBodyToEntity);
    
    /**
     * @brief 添加单个碰撞结果
     * 
     * 重写基类方法，收集碰撞信息
     */
    btScalar addSingleResult(btManifoldPoint& cp,
                            const btCollisionObjectWrapper* colObj0Wrap,
                            int partId0, int index0,
                            const btCollisionObjectWrapper* colObj1Wrap,
                            int partId1, int index1) override;
    
    /**
     * @brief 获取收集到的碰撞对
     */
    const std::vector<CollisionPair>& GetCollisionPairs() const { return m_collisionPairs; }
    
    /**
     * @brief 清空收集的碰撞对
     */
    void Clear() { m_collisionPairs.clear(); }
    
private:
    // 实体到刚体的映射（用于查找实体ID）
    const std::unordered_map<ECS::EntityID, btRigidBody*, ECS::EntityID::Hash>& m_entityToRigidBody;
    const std::unordered_map<btRigidBody*, ECS::EntityID>& m_rigidBodyToEntity;
    
    // 收集的碰撞对
    std::vector<CollisionPair> m_collisionPairs;
    
    // 临时存储：当前处理的碰撞对（用于合并同一对物体的多个接触点）
    std::unordered_map<uint64_t, CollisionPair> m_currentPairs;
    
    /**
     * @brief 计算碰撞对的哈希值
     */
    static uint64_t HashPair(ECS::EntityID a, ECS::EntityID b);
    
    /**
     * @brief 从 Bullet 接触点转换为 ContactManifold
     */
    static ContactManifold ConvertManifold(const btManifoldPoint& cp,
                                          const btCollisionObjectWrapper* colObj0Wrap,
                                          const btCollisionObjectWrapper* colObj1Wrap);
};

} // namespace Render::Physics::BulletAdapter

