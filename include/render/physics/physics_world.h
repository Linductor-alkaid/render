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

#include "physics_config.h"
#include "physics_components.h"

namespace Render {

namespace ECS {
class World;  // 前向声明
}

namespace Physics {

class CollisionDetectionSystem;
class PhysicsUpdateSystem;

/**
 * @brief 物理世界
 * 
 * 管理整个物理模拟系统的核心类
 * 
 * @note 这是一个初始框架，完整实现将在后续阶段添加
 */
class PhysicsWorld {
public:
    /**
     * @brief 构造函数
     * @param ecsWorld ECS 世界指针
     * @param config 物理配置
     */
    PhysicsWorld(ECS::World* ecsWorld, const PhysicsConfig& config = PhysicsConfig::Default())
        : m_ecsWorld(ecsWorld), m_config(config) {}
    
    /**
     * @brief 析构函数
     */
    ~PhysicsWorld() = default;
    
    /**
     * @brief 物理步进（每帧调用）
     * @param deltaTime 帧时间（秒）
     */
    void Step(float deltaTime);
    
    /**
     * @brief 设置重力
     */
    void SetGravity(const Vector3& gravity) {
        m_config.gravity = gravity;
    }
    
    /**
     * @brief 获取重力
     */
    Vector3 GetGravity() const {
        return m_config.gravity;
    }
    
    /**
     * @brief 获取配置
     */
    const PhysicsConfig& GetConfig() const {
        return m_config;
    }

    /**
     * @brief 更新物理配置
     */
    void SetConfig(const PhysicsConfig& config) {
        m_config = config;
    }
    
private:
    ECS::World* m_ecsWorld;
    PhysicsConfig m_config;
    
    // 更多成员变量将在后续阶段添加
};

} // namespace Physics
} // namespace Render

