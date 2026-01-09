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

#include "render/robot/imu_interface.h"
#include "render/math_utils.h"
#include <cmath>

namespace Render {
namespace Robot {

IMUInterface::IMUInterface()
    : m_velocity(Vector3::Zero())
    , m_position(Vector3::Zero())
    , m_orientation(Quaternion::Identity())
    , m_worldPose()
    , m_initialPose()
{
}

void IMUInterface::UpdateIMUData(const IMUData& data) {
    m_currentData = data;
}

void IMUInterface::SetInitialPose(const Transform& pose) {
    // Transform不可拷贝，需要手动设置
    m_initialPose.SetPosition(pose.GetPosition());
    m_initialPose.SetRotation(pose.GetRotation());
    m_initialPose.SetScale(pose.GetScale());
    
    m_worldPose.SetPosition(pose.GetPosition());
    m_worldPose.SetRotation(pose.GetRotation());
    m_worldPose.SetScale(pose.GetScale());
    
    m_position = pose.GetPosition();
    m_orientation = pose.GetRotation();
    m_hasInitialPose = true;
}

void IMUInterface::Reset() {
    if (m_hasInitialPose) {
        m_worldPose.SetPosition(m_initialPose.GetPosition());
        m_worldPose.SetRotation(m_initialPose.GetRotation());
        m_worldPose.SetScale(m_initialPose.GetScale());
        m_position = m_initialPose.GetPosition();
        m_orientation = m_initialPose.GetRotation();
        m_velocity = Vector3::Zero();
    } else {
        m_worldPose.SetPosition(Vector3::Zero());
        m_worldPose.SetRotation(Quaternion::Identity());
        m_worldPose.SetScale(Vector3::Ones());
        m_position = Vector3::Zero();
        m_orientation = Quaternion::Identity();
        m_velocity = Vector3::Zero();
    }
}

void IMUInterface::Update(float deltaTime) {
    if (deltaTime <= 0.0f) {
        return;
    }
    
    FuseIMUData(deltaTime);
    
    // 更新世界姿态
    m_worldPose.SetPosition(m_position);
    m_worldPose.SetRotation(m_orientation);
    m_worldPose.SetScale(Vector3::Ones());
}

void IMUInterface::FuseIMUData(float deltaTime) {
    // 简单的互补滤波器实现
    // 注意：这是一个简化的实现，实际应用中可能需要更复杂的融合算法
    
    // 1. 从加速度计估计方向（假设加速度主要是重力）
    Vector3 accel = m_currentData.acceleration;
    float accelNorm = accel.norm();
    
    if (accelNorm > 0.001f) {
        // 归一化加速度（假设是重力方向）
        Vector3 gravityDir = accel / accelNorm;
        
        // 从重力方向估计姿态（简化：只考虑pitch和roll）
        // 注意：这里需要根据实际坐标系调整
        float pitch = std::asin(-gravityDir.y());
        float roll = std::atan2(gravityDir.x(), gravityDir.z());
        
        Quaternion accelOrientation = Quaternion(
            Eigen::AngleAxisf(roll, Vector3::UnitX()) *
            Eigen::AngleAxisf(pitch, Vector3::UnitY())
        );
        
        // 2. 从陀螺仪积分得到方向
        Vector3 angularVel = m_currentData.angularVelocity;
        Quaternion gyroDelta = Quaternion(
            Eigen::AngleAxisf(angularVel.norm() * deltaTime, angularVel.normalized())
        );
        m_orientation = m_orientation * gyroDelta;
        m_orientation.normalize();
        
        // 3. 互补滤波器融合
        // alpha接近1：更信任陀螺仪（短期稳定）
        // alpha接近0：更信任加速度计（长期稳定）
        m_orientation = Quaternion::Identity().slerp(1.0f - m_alpha, m_orientation) *
                       Quaternion::Identity().slerp(m_alpha, accelOrientation);
        m_orientation.normalize();
    } else {
        // 如果没有有效的加速度数据，只使用陀螺仪
        Vector3 angularVel = m_currentData.angularVelocity;
        Quaternion gyroDelta = Quaternion(
            Eigen::AngleAxisf(angularVel.norm() * deltaTime, angularVel.normalized())
        );
        m_orientation = m_orientation * gyroDelta;
        m_orientation.normalize();
    }
    
    // 4. 积分加速度得到速度和位置（简化实现，实际需要更复杂的处理）
    // 注意：这里假设加速度是世界坐标系下的，实际可能需要转换
    Vector3 worldAccel = m_orientation * accel;
    
    // 减去重力（假设Z轴向上）
    worldAccel.z() -= 9.81f;  // 重力加速度
    
    // 积分得到速度
    m_velocity += worldAccel * deltaTime;
    
    // 积分得到位置
    m_position += m_velocity * deltaTime;
    
    // 简单的阻尼（防止速度无限增长）
    m_velocity *= 0.99f;
}

} // namespace Robot
} // namespace Render
