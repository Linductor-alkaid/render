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

#include "render/robot/robot_components.h"
#include "render/ecs/system.h"
#include "render/ecs/entity.h"
#include <string>
#include <unordered_map>

namespace Render {
namespace ECS {

// 前向声明
class World;

namespace Robot {

/**
 * @brief 机器人控制系统
 * 
 * 负责实现关节的位置控制、速度控制和力矩控制
 * 优先级：10（在JointTFSystem之后，PhysicsSystem之前）
 */
class RobotControlSystem : public System {
public:
    RobotControlSystem() = default;
    ~RobotControlSystem() override = default;
    
    void OnCreate(World* world) override {
        System::OnCreate(world);
    }
    
    void Update(float deltaTime) override;
    
    [[nodiscard]] int GetPriority() const override { return 10; }  // 在JointTFSystem之后，PhysicsSystem之前
    
    // ==================== 控制模式设置 ====================
    
    /**
     * @brief 设置关节控制模式
     * @param robotEntity 机器人实体ID
     * @param jointName 关节名称
     * @param mode 控制模式
     */
    void SetJointControlMode(EntityID robotEntity, const std::string& jointName, 
                             JointComponent::ControlMode mode);
    
    // ==================== 目标值设置 ====================
    
    /**
     * @brief 设置关节目标位置
     * @param robotEntity 机器人实体ID
     * @param jointName 关节名称
     * @param position 目标位置（角度或距离）
     */
    void SetJointTargetPosition(EntityID robotEntity, const std::string& jointName, 
                                float position);
    
    /**
     * @brief 设置关节目标速度
     * @param robotEntity 机器人实体ID
     * @param jointName 关节名称
     * @param velocity 目标速度（rad/s 或 m/s）
     */
    void SetJointTargetVelocity(EntityID robotEntity, const std::string& jointName, 
                                float velocity);
    
    /**
     * @brief 设置关节目标力矩
     * @param robotEntity 机器人实体ID
     * @param jointName 关节名称
     * @param torque 目标力矩（N·m 或 N）
     */
    void SetJointTargetTorque(EntityID robotEntity, const std::string& jointName, 
                              float torque);
    
    // ==================== 批量设置 ====================
    
    /**
     * @brief 批量设置关节目标位置
     * @param robotEntity 机器人实体ID
     * @param positions 关节名称到位置的映射
     */
    void SetJointTargetPositions(EntityID robotEntity, 
                                 const std::unordered_map<std::string, float>& positions);
    
    /**
     * @brief 批量设置关节目标速度
     * @param robotEntity 机器人实体ID
     * @param velocities 关节名称到速度的映射
     */
    void SetJointTargetVelocities(EntityID robotEntity, 
                                  const std::unordered_map<std::string, float>& velocities);
    
    /**
     * @brief 批量设置关节目标力矩
     * @param robotEntity 机器人实体ID
     * @param torques 关节名称到力矩的映射
     */
    void SetJointTargetTorques(EntityID robotEntity, 
                               const std::unordered_map<std::string, float>& torques);
    
    // ==================== 状态查询 ====================
    
    /**
     * @brief 获取关节位置
     * @param robotEntity 机器人实体ID
     * @param jointName 关节名称
     * @return 关节位置，如果关节不存在返回0.0f
     */
    float GetJointPosition(EntityID robotEntity, const std::string& jointName) const;
    
    /**
     * @brief 获取关节速度
     * @param robotEntity 机器人实体ID
     * @param jointName 关节名称
     * @return 关节速度，如果关节不存在返回0.0f
     */
    float GetJointVelocity(EntityID robotEntity, const std::string& jointName) const;
    
    /**
     * @brief 获取关节实体ID（可能更新缓存）
     * @param robotEntity 机器人实体ID
     * @param jointName 关节名称
     * @return 关节实体ID，如果不存在返回Invalid
     * @note 此方法会更新内部缓存，因此不是const方法
     */
    EntityID GetJointEntity(EntityID robotEntity, const std::string& jointName);
    
    /**
     * @brief 查找关节实体ID（不更新缓存，const版本）
     * @param robotEntity 机器人实体ID
     * @param jointName 关节名称
     * @return 关节实体ID，如果不存在返回Invalid
     */
    EntityID FindJointEntity(EntityID robotEntity, const std::string& jointName) const;

private:
    /**
     * @brief 更新关节状态（从物理系统读取）
     * @param jointEntity 关节实体ID
     * @param jointComp 关节组件引用
     */
    void UpdateJointState(EntityID jointEntity, JointComponent& jointComp);
    
    /**
     * @brief 应用控制输出到关节
     * @param jointEntity 关节实体ID
     * @param jointComp 关节组件引用
     * @param deltaTime 时间步长
     */
    void ApplyControl(EntityID jointEntity, JointComponent& jointComp, float deltaTime);
    
    /**
     * @brief 计算位置控制输出（PD控制器）
     * @param jointComp 关节组件引用
     * @return 计算出的力矩/力
     */
    float ComputePositionControl(const JointComponent& jointComp) const;
    
    /**
     * @brief 计算速度控制输出（P控制器）
     * @param jointComp 关节组件引用
     * @return 计算出的力矩/力
     */
    float ComputeVelocityControl(const JointComponent& jointComp) const;
    
    /**
     * @brief 从物理系统获取关节速度
     * @param jointEntity 关节实体ID
     * @param jointComp 关节组件引用
     * @return 关节速度（rad/s 或 m/s）
     */
    float GetJointVelocityFromPhysics(EntityID jointEntity, const JointComponent& jointComp) const;
    
    /**
     * @brief 对刚体施加关节力矩/力
     * @param jointEntity 关节实体ID
     * @param jointComp 关节组件引用
     * @param torqueOrForce 力矩（旋转关节）或力（平移关节）
     */
    void ApplyJointTorqueOrForce(EntityID jointEntity, const JointComponent& jointComp, 
                                  float torqueOrForce);
    
    /**
     * @brief 根据关节实体查找机器人实体
     * @param jointEntity 关节实体ID
     * @return 机器人实体ID，如果不存在返回Invalid
     */
    EntityID FindRobotEntity(EntityID jointEntity) const;
    
    /**
     * @brief 将JointTF应用到link的TransformComponent（用于Kinematic模式）
     * @param robotEntity 机器人实体ID
     */
    void ApplyTFsToTransforms(EntityID robotEntity);
    
    // 关节实体映射（机器人实体 -> 关节名称 -> 关节实体ID）
    std::unordered_map<EntityID, std::unordered_map<std::string, EntityID>, EntityID::Hash> m_jointEntityMap;
};

} // namespace Robot
} // namespace ECS
} // namespace Render
