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

// 前向声明
class btRigidBody;
class btCollisionShape;

namespace Render::Physics::BulletAdapter {

/**
 * @brief 刚体适配器
 * 
 * 负责在 RigidBodyComponent 和 btRigidBody 之间同步数据
 */
class BulletRigidBodyAdapter {
public:
    /**
     * @brief 构造函数
     * @param bulletBody Bullet 刚体指针（不接管所有权）
     * @param entity 关联的 ECS 实体 ID
     */
    BulletRigidBodyAdapter(btRigidBody* bulletBody, ECS::EntityID entity);
    
    /**
     * @brief 析构函数
     */
    ~BulletRigidBodyAdapter() = default;
    
    /**
     * @brief 从 RigidBodyComponent 同步到 btRigidBody
     * @param component 刚体组件
     */
    void SyncToBullet(const RigidBodyComponent& component);
    
    /**
     * @brief 从 btRigidBody 同步到 RigidBodyComponent
     * @param component 刚体组件（将被修改）
     */
    void SyncFromBullet(RigidBodyComponent& component);
    
    /**
     * @brief 获取 Bullet 刚体指针
     */
    btRigidBody* GetBulletBody() const { return m_bulletBody; }
    
    /**
     * @brief 获取关联的 ECS 实体 ID
     */
    ECS::EntityID GetEntity() const { return m_entity; }

private:
    btRigidBody* m_bulletBody;  ///< Bullet 刚体指针（不拥有所有权）
    ECS::EntityID m_entity;     ///< 关联的 ECS 实体 ID
};

} // namespace Render::Physics::BulletAdapter

