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

#include "render/types.h"
#include "render/ecs/entity.h"
#include "render/math_utils.h"
#include <limits>

namespace Render {
namespace Physics {

/**
 * @brief 关节基础组件
 */
struct JointComponent {
    enum class JointType {
        Fixed,      // 固定关节
        Hinge,      // 铰链关节
        Distance,   // 距离关节
        Spring,     // 弹簧关节
        Slider      // 滑动关节
    };
    
    JointType type = JointType::Fixed;
    ECS::EntityID connectedBody;  // 连接的另一个刚体
    
    // 锚点（局部坐标）
    Vector3 localAnchorA = Vector3::Zero();
    Vector3 localAnchorB = Vector3::Zero();
    
    // 断裂阈值
    float breakForce = std::numeric_limits<float>::infinity();
    float breakTorque = std::numeric_limits<float>::infinity();
    bool isBroken = false;
    
    // 启用/禁用
    bool isEnabled = true;
    
    // 碰撞控制
    bool enableCollision = false;  // 连接的两个刚体是否参与碰撞
};

/**
 * @brief 固定关节数据
 */
struct FixedJointData {
    Quaternion relativeRotation = Quaternion::Identity();
};

/**
 * @brief 铰链关节数据
 */
struct HingeJointData {
    Vector3 localAxisA = Vector3::UnitZ();
    Vector3 localAxisB = Vector3::UnitZ();
    
    // 角度限制
    bool hasLimits = false;
    float limitMin = -MathUtils::PI;
    float limitMax = MathUtils::PI;
    float currentAngle = 0.0f;
    
    // 马达
    bool useMotor = false;
    float motorSpeed = 0.0f;       // 目标角速度
    float motorMaxForce = 100.0f;  // 最大马达力矩
};

/**
 * @brief 距离关节数据
 */
struct DistanceJointData {
    float restLength = 1.0f;
    bool hasLimits = false;
    float minDistance = 0.0f;
    float maxDistance = std::numeric_limits<float>::infinity();
};

/**
 * @brief 弹簧关节数据
 */
struct SpringJointData {
    float restLength = 1.0f;
    float stiffness = 100.0f;  // 刚度系数 k
    float damping = 10.0f;     // 阻尼系数 c
};

/**
 * @brief 滑动关节数据
 */
struct SliderJointData {
    Vector3 localAxis = Vector3::UnitX();
    
    bool hasLimits = false;
    float minDistance = -std::numeric_limits<float>::infinity();
    float maxDistance = std::numeric_limits<float>::infinity();
};

}  // namespace Physics
}  // namespace Render
