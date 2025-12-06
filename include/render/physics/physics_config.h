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

namespace Render {
namespace Physics {

/**
 * @brief 粗检测算法类型
 */
enum class BroadPhaseType {
    SpatialHash,  // 空间哈希（适合动态场景）
    Octree,       // 八叉树（适合静态场景）
    BVH           // 层次包围盒（平衡性能）
};

/**
 * @brief 物理引擎配置
 * 
 * 包含物理世界的所有配置参数
 */
struct PhysicsConfig {
    // ==================== 重力设置 ====================
    
    /// 全局重力加速度 (m/s²)
    Vector3 gravity = Vector3(0.0f, -9.81f, 0.0f);
    
    // ==================== 时间步长 ====================
    
    /// 固定物理时间步长（秒）
    /// 默认 1/60 秒，确保物理稳定性
    float fixedDeltaTime = 1.0f / 60.0f;
    
    /// 最大子步数（防止螺旋死亡）
    /// 如果帧时间过长，限制物理更新次数
    int maxSubSteps = 5;
    
    // ==================== 求解器设置 ====================
    
    /// 速度约束求解迭代次数
    /// 越高越精确，但性能消耗越大
    int solverIterations = 10;
    
    /// 位置约束求解迭代次数
    /// 用于修正穿透
    int positionIterations = 4;
    
    // ==================== 粗检测设置 ====================
    
    /// 粗检测算法类型
    BroadPhaseType broadPhaseType = BroadPhaseType::SpatialHash;
    
    /// 空间哈希格子大小（米）
    /// 仅当 broadPhaseType = SpatialHash 时有效
    float spatialHashCellSize = 5.0f;
    
    // ==================== 高级特性开关 ====================
    
    /// 是否启用连续碰撞检测（CCD）
    /// 防止高速物体穿透，但性能消耗较大
    bool enableCCD = false;
    
    /// 是否启用休眠系统
    /// 静止物体自动休眠以节省 CPU
    bool enableSleeping = true;
    
    /// 休眠能量阈值
    /// 物体动能低于此值时开始计时休眠
    float sleepThreshold = 0.01f;
    
    /// 休眠时间（秒）
    /// 物体保持低能量状态超过此时间后进入休眠
    float sleepTime = 0.5f;
    
    // ==================== 性能设置 ====================
    
    /// 是否启用多线程优化
    bool enableMultithreading = true;
    
    /// 工作线程数（0 = 自动检测）
    int workerThreadCount = 0;
    
    // ==================== 调试设置 ====================
    
    /// 是否启用物理调试渲染
    bool enableDebugDraw = false;
    
    /// 是否显示碰撞体
    bool showColliders = true;
    
    /// 是否显示 AABB
    bool showAABB = false;
    
    /// 是否显示接触点
    bool showContacts = true;
    
    /// 是否显示速度向量
    bool showVelocity = false;
    
    // ==================== 辅助方法 ====================
    
    /**
     * @brief 获取默认配置
     */
    static PhysicsConfig Default() {
        return PhysicsConfig();
    }
    
    /**
     * @brief 获取高精度配置
     * 更多迭代次数，更高精度，但性能较低
     */
    static PhysicsConfig HighPrecision() {
        PhysicsConfig config;
        config.solverIterations = 20;
        config.positionIterations = 8;
        config.fixedDeltaTime = 1.0f / 120.0f;
        return config;
    }
    
    /**
     * @brief 获取高性能配置
     * 较少迭代次数，牺牲精度换取性能
     */
    static PhysicsConfig HighPerformance() {
        PhysicsConfig config;
        config.solverIterations = 6;
        config.positionIterations = 2;
        config.enableSleeping = true;
        return config;
    }
};

} // namespace Physics
} // namespace Render

