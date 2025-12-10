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

// 前向声明
class btCollisionShape;

namespace Render::Physics::BulletAdapter {

/**
 * @brief 形状适配器
 * 
 * 负责将 ColliderComponent 转换为 btCollisionShape
 */
class BulletShapeAdapter {
public:
    /**
     * @brief 从 ColliderComponent 创建 btCollisionShape
     * @param collider 碰撞体组件
     * @return Bullet 碰撞形状，调用者负责管理生命周期
     */
    static btCollisionShape* CreateShape(const ColliderComponent& collider);
    
    /**
     * @brief 更新现有形状的参数
     * @param shape 现有形状（必须与 collider 的类型匹配）
     * @param collider 碰撞体组件
     */
    static void UpdateShape(btCollisionShape* shape, const ColliderComponent& collider);
    
    /**
     * @brief 释放形状（用于清理）
     * @param shape 要释放的形状
     */
    static void DestroyShape(btCollisionShape* shape);

private:
    // 禁止实例化（静态工具类）
    BulletShapeAdapter() = delete;
};

} // namespace Render::Physics::BulletAdapter

