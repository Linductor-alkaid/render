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

#include "render/ecs/entity.h"
#include "render/application/event_bus.h"
#include "collision/contact_manifold.h"

namespace Render {
namespace Physics {

/**
 * @brief 碰撞开始事件
 * 
 * 当两个物体开始碰撞时触发
 */
struct CollisionEnterEvent : public Application::EventBase {
    ECS::EntityID entityA;
    ECS::EntityID entityB;
    ContactManifold manifold;
    
    CollisionEnterEvent() = default;
    CollisionEnterEvent(ECS::EntityID a, ECS::EntityID b, const ContactManifold& m)
        : entityA(a), entityB(b), manifold(m) {}
};

/**
 * @brief 碰撞持续事件
 * 
 * 当两个物体持续碰撞时每帧触发
 */
struct CollisionStayEvent : public Application::EventBase {
    ECS::EntityID entityA;
    ECS::EntityID entityB;
    ContactManifold manifold;
    
    CollisionStayEvent() = default;
    CollisionStayEvent(ECS::EntityID a, ECS::EntityID b, const ContactManifold& m)
        : entityA(a), entityB(b), manifold(m) {}
};

/**
 * @brief 碰撞结束事件
 * 
 * 当两个物体分离时触发
 */
struct CollisionExitEvent : public Application::EventBase {
    ECS::EntityID entityA;
    ECS::EntityID entityB;
    
    CollisionExitEvent() = default;
    CollisionExitEvent(ECS::EntityID a, ECS::EntityID b)
        : entityA(a), entityB(b) {}
};

/**
 * @brief 触发器进入事件
 * 
 * 当物体进入触发器区域时触发
 */
struct TriggerEnterEvent : public Application::EventBase {
    ECS::EntityID trigger;
    ECS::EntityID other;
    ContactManifold manifold;
    
    TriggerEnterEvent() = default;
    TriggerEnterEvent(ECS::EntityID t, ECS::EntityID o, const ContactManifold& m)
        : trigger(t), other(o), manifold(m) {}
};

/**
 * @brief 触发器停留事件
 * 
 * 当物体在触发器区域内时每帧触发
 */
struct TriggerStayEvent : public Application::EventBase {
    ECS::EntityID trigger;
    ECS::EntityID other;
    
    TriggerStayEvent() = default;
    TriggerStayEvent(ECS::EntityID t, ECS::EntityID o)
        : trigger(t), other(o) {}
};

/**
 * @brief 触发器退出事件
 * 
 * 当物体离开触发器区域时触发
 */
struct TriggerExitEvent : public Application::EventBase {
    ECS::EntityID trigger;
    ECS::EntityID other;
    
    TriggerExitEvent() = default;
    TriggerExitEvent(ECS::EntityID t, ECS::EntityID o)
        : trigger(t), other(o) {}
};

} // namespace Physics
} // namespace Render

