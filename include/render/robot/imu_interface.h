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
#include "render/transform.h"
#include <memory>

namespace Render {
namespace Robot {

/**
 * @brief IMU数据
 */
struct IMUData {
    Vector3 acceleration;    // 加速度 (m/s²)
    Vector3 angularVelocity;   // 角速度 (rad/s)
    Quaternion orientation;    // 方向（可选，如果IMU提供）
    float timestamp;           // 时间戳（秒）
    
    IMUData()
        : acceleration(Vector3::Zero())
        , angularVelocity(Vector3::Zero())
        , orientation(Quaternion::Identity())
        , timestamp(0.0f) {}
};

/**
 * @brief IMU接口
 * 
 * 用于接收IMU数据并计算世界坐标系下的机器人姿态
 */
class IMUInterface {
public:
    IMUInterface();
    ~IMUInterface() = default;
    
    /**
     * @brief 更新IMU数据
     * @param data IMU数据
     */
    void UpdateIMUData(const IMUData& data);
    
    /**
     * @brief 获取世界坐标系下的机器人姿态
     * @return 机器人姿态变换的常量引用
     */
    const Transform& GetWorldPose() const { return m_worldPose; }
    
    /**
     * @brief 设置初始姿态
     * @param pose 初始姿态
     */
    void SetInitialPose(const Transform& pose);
    
    /**
     * @brief 重置姿态（恢复到初始姿态）
     */
    void Reset();
    
    /**
     * @brief 设置互补滤波器参数
     * @param alpha 加速度权重（0-1），越大越信任加速度计
     */
    void SetComplementaryFilterAlpha(float alpha) { m_alpha = alpha; }
    
    /**
     * @brief 更新姿态融合（需要在每帧调用）
     * @param deltaTime 时间间隔（秒）
     */
    void Update(float deltaTime);

private:
    /**
     * @brief 使用互补滤波器融合IMU数据
     * @param deltaTime 时间间隔
     */
    void FuseIMUData(float deltaTime);
    
    IMUData m_currentData;
    Transform m_worldPose;
    Transform m_initialPose;
    
    // 互补滤波器参数
    float m_alpha = 0.98f;  // 加速度计权重（0-1）
    
    // 积分状态
    Vector3 m_velocity;      // 速度（通过加速度积分）
    Vector3 m_position;      // 位置（通过速度积分）
    Quaternion m_orientation; // 方向（通过角速度积分）
    
    bool m_hasInitialPose = false;
    float m_lastTimestamp = 0.0f;
};

} // namespace Robot
} // namespace Render
