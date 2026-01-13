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

#include "render/robot/robot_model.h"
#include "render/ecs/entity.h"
#include "render/mesh.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace Render {
namespace ECS {

// 前向声明
class World;

namespace Robot {

/**
 * @brief 机器人根组件
 * 
 * 每个机器人实体都有一个RobotComponent，包含整个机器人模型信息
 */
struct RobotComponent {
    Ref<Render::Robot::RobotModel> model;  // 机器人模型
    EntityID baseLinkEntity;        // 基座实体ID
    std::unordered_map<std::string, EntityID> linkEntityMap;  // link名称 -> 实体ID映射
    
    RobotComponent() : baseLinkEntity(EntityID::Invalid()) {}
};

/**
 * @brief 关节组件
 * 
 * 每个关节实体都有一个JointComponent
 */
struct JointComponent {
    std::string jointName;
    std::string parentLink;
    std::string childLink;
    Render::Robot::JointType type;
    
    float position = 0.0f;  // 关节位置（角度或距离）
    Vector3 axis;           // 关节轴
    Render::Robot::JointLimits limits;
    
    EntityID parentLinkEntity;  // 父link实体ID
    EntityID childLinkEntity;  // 子link实体ID
    
    // ==================== 关节控制 ====================
    
    /**
     * @brief 关节控制模式
     */
    enum class ControlMode {
        Position,    ///< 位置控制（使用PD控制器，通过物理约束）
        Velocity,    ///< 速度控制（使用P控制器，通过物理约束）
        Torque,      ///< 力矩控制（直接施加力矩，通过物理约束）
        Kinematic    ///< 运动学控制（直接设置位置/速度，不通过物理模拟）
    };
    
    ControlMode controlMode = ControlMode::Position;  ///< 当前控制模式
    
    // 控制目标值
    float targetPosition = 0.0f;   ///< 目标位置（角度或距离）
    float targetVelocity = 0.0f;  ///< 目标速度（rad/s 或 m/s）
    float targetTorque = 0.0f;     ///< 目标力矩（N·m 或 N）
    
    // 位置控制参数（PD控制器）
    float positionKp = 100.0f;      ///< 位置比例增益
    float positionKd = 10.0f;      ///< 位置微分增益
    
    // 速度控制参数（P控制器）
    float velocityKp = 50.0f;      ///< 速度比例增益
    
    // 力矩限制
    float maxTorque = 100.0f;      ///< 最大力矩限制（N·m 或 N）
    
    // 当前状态（由RobotControlSystem更新）
    float currentPosition = 0.0f;  ///< 当前位置（角度或距离）
    float currentVelocity = 0.0f;  ///< 当前速度（rad/s 或 m/s）
    
    // 运动学状态（用于Kinematic模式）
    float kinematicPosition = 0.0f;  ///< 运动学位置（用于速度积分）
    float kinematicVelocity = 0.0f;  ///< 运动学速度（用于速度控制）
    
    JointComponent() 
        : type(Render::Robot::JointType::Unknown)
        , axis(Vector3::UnitZ())
        , parentLinkEntity(EntityID::Invalid())
        , childLinkEntity(EntityID::Invalid()) {}
};

/**
 * @brief 连杆组件
 * 
 * 每个link实体都有一个LinkComponent
 */
struct LinkComponent {
    std::string linkName;
    std::vector<std::string> childJoints;  // 子关节名称列表
    Ref<Mesh> visualMesh;  // 视觉网格（可选，可能有多个visual）
    std::vector<Ref<Mesh>> visualMeshes;  // 所有视觉网格
    bool isBaseLink = false;
    
    LinkComponent() = default;
};

} // namespace Robot
} // namespace ECS
} // namespace Render
