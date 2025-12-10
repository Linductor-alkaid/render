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

#include "render/physics/physics_components.h"
#include "render/ecs/entity.h"
#include <BulletCollision/CollisionDispatch/btManifoldResult.h>
#include <unordered_map>

// 前向声明
class btRigidBody;
class btCollisionObject;

namespace Render::Physics::BulletAdapter {

/**
 * @brief Bullet 材质组合回调
 * 
 * 用于在接触点创建时应用材质组合模式（摩擦和弹性的组合）
 */
class BulletMaterialCallback {
public:
    /**
     * @brief 构造函数
     * @param entityToRigidBody 实体到刚体的映射
     * @param rigidBodyToEntity 刚体到实体的映射
     */
    BulletMaterialCallback(
        const std::unordered_map<ECS::EntityID, btRigidBody*, ECS::EntityID::Hash>& entityToRigidBody,
        const std::unordered_map<btRigidBody*, ECS::EntityID>& rigidBodyToEntity);
    
    /**
     * @brief 处理接触点（应用材质组合）
     * 
     * 这个函数应该被设置为 Bullet 的全局回调
     * 
     * @param cp 接触点
     * @param colObj0 碰撞对象0
     * @param colObj1 碰撞对象1
     * @return 是否处理成功
     */
    bool ProcessContactPoint(btManifoldPoint& cp,
                            const btCollisionObject* colObj0,
                            const btCollisionObject* colObj1) const;
    
    /**
     * @brief 获取材质（从实体ID）
     */
    static std::shared_ptr<PhysicsMaterial> GetMaterial(ECS::EntityID entity);
    
    /**
     * @brief 设置材质获取函数（用于从ECS获取材质）
     */
    static void SetMaterialGetter(std::function<std::shared_ptr<PhysicsMaterial>(ECS::EntityID)> getter);
    
private:
    // 实体到刚体的映射（用于查找实体ID）
    const std::unordered_map<ECS::EntityID, btRigidBody*, ECS::EntityID::Hash>& m_entityToRigidBody;
    const std::unordered_map<btRigidBody*, ECS::EntityID>& m_rigidBodyToEntity;
    
    // 材质获取函数（静态，用于从ECS获取材质）
    static std::function<std::shared_ptr<PhysicsMaterial>(ECS::EntityID)> s_materialGetter;
};

} // namespace Render::Physics::BulletAdapter

