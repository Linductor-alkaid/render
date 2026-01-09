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
