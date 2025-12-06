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

#include "render/application/app_module.h"
#include "render/physics/physics_config.h"
#include <memory>

namespace Render {

namespace Physics {
class PhysicsWorld;  // 前向声明
}

namespace Application {

/**
 * @brief 物理引擎模块
 * 
 * 负责管理物理世界的生命周期和更新
 * 
 * @note 这是阶段一的基础实现，完整功能将在后续阶段添加
 */
class PhysicsModule final : public AppModule {
public:
    /**
     * @brief 构造函数
     * @param config 物理配置（可选）
     */
    explicit PhysicsModule(const Physics::PhysicsConfig& config = Physics::PhysicsConfig::Default());
    
    /**
     * @brief 析构函数
     */
    ~PhysicsModule() override;
    
    // ==================== AppModule 接口 ====================
    
    std::string_view Name() const override { return "PhysicsModule"; }
    
    ModuleDependencies Dependencies() const override { 
        return {}; // 暂无依赖，未来可能依赖 CoreRenderModule
    }
    
    int Priority(ModulePhase phase) const override;
    
    void OnRegister(ECS::World& world, AppContext& ctx) override;
    void OnUnregister(ECS::World& world, AppContext& ctx) override;
    
    void OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx) override;
    void OnPostFrame(const FrameUpdateArgs& frame, AppContext& ctx) override;
    
    // ==================== 物理引擎接口 ====================
    
    /**
     * @brief 获取物理世界
     */
    Physics::PhysicsWorld* GetPhysicsWorld() const { return m_physicsWorld.get(); }
    
    /**
     * @brief 启用/禁用物理模拟
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    
    /**
     * @brief 是否启用物理模拟
     */
    bool IsEnabled() const { return m_enabled; }
    
    /**
     * @brief 设置物理配置
     */
    void SetConfig(const Physics::PhysicsConfig& config);
    
    /**
     * @brief 获取物理配置
     */
    const Physics::PhysicsConfig& GetConfig() const;
    
private:
    /**
     * @brief 注册物理组件到 ECS
     */
    void RegisterPhysicsComponents(ECS::World& world);
    
    /**
     * @brief 注册物理系统到 ECS
     */
    void RegisterPhysicsSystems(ECS::World& world);
    
    // ==================== 成员变量 ====================
    
    /// 物理配置
    Physics::PhysicsConfig m_config;
    
    /// 物理世界（延迟初始化）
    std::unique_ptr<Physics::PhysicsWorld> m_physicsWorld;
    
    /// 是否启用物理模拟
    bool m_enabled = true;
    
    /// 是否已注册
    bool m_registered = false;
    
    /// 累积时间（用于固定时间步长）
    float m_accumulator = 0.0f;
};

} // namespace Application
} // namespace Render

