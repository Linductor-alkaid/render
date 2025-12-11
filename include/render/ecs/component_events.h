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

#include "entity.h"  // EntityID
#include "render/types.h"  // Vector3, Quaternion
#include "components.h"  // TransformComponent
#include <typeindex>  // std::type_index

namespace Render {
namespace ECS {

/**
 * @brief 组件变化事件基类
 * 
 * 所有组件变化事件都继承自此基类，提供统一的接口
 * 用于在组件发生变化时通知订阅者
 */
struct ComponentChangeEvent {
    EntityID entity;                    ///< 发生变化的实体ID
    std::type_index componentType;     ///< 组件类型
    
    /**
     * @brief 构造函数
     * @param e 实体ID
     * @param t 组件类型
     */
    ComponentChangeEvent(EntityID e, std::type_index t)
        : entity(e), componentType(t) {}
    
    virtual ~ComponentChangeEvent() = default;
};

/**
 * @brief TransformComponent变化事件
 * 
 * 当TransformComponent的位置、旋转或缩放发生变化时触发
 * 包含变化后的新值，用于同步到物理引擎等系统
 */
struct TransformComponentChangeEvent : public ComponentChangeEvent {
    Vector3 position;          ///< 新位置
    Quaternion rotation;       ///< 新旋转
    Vector3 scale;             ///< 新缩放
    
    /**
     * @brief 构造函数
     * @param e 实体ID
     * @param pos 新位置
     * @param rot 新旋转
     * @param scl 新缩放
     */
    TransformComponentChangeEvent(EntityID e, 
                                  const Vector3& pos,
                                  const Quaternion& rot,
                                  const Vector3& scl)
        : ComponentChangeEvent(e, std::type_index(typeid(TransformComponent)))
        , position(pos)
        , rotation(rot)
        , scale(scl) {}
};

} // namespace ECS
} // namespace Render

