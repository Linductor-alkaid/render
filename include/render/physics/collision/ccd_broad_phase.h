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
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/physics/physics_components.h"
#include <vector>
#include <unordered_set>

namespace Render {
namespace Physics {

/**
 * @brief CCD Broad Phase 工具类
 * 
 * 用于快速筛选可能发生 CCD 碰撞的物体对
 * 使用扫描 AABB 方法加速检测
 */
class CCDBroadPhase {
public:
    /**
     * @brief 计算扫描 AABB（从 t=0 到 t=dt）
     * 
     * @param aabb0 初始时刻的 AABB
     * @param velocity 速度向量
     * @param dt 时间步长
     * @return 扫描后的 AABB（包含整个运动轨迹）
     */
    static AABB ComputeSweptAABB(const AABB& aabb0, const Vector3& velocity, float dt);
    
    /**
     * @brief 使用 Broad Phase 筛选 CCD 候选对
     * 
     * @param candidates CCD 候选实体列表
     * @param world ECS World 指针
     * @param dt 时间步长
     * @return 筛选后的实体对列表（只包含可能碰撞的对）
     */
    static std::vector<std::pair<ECS::EntityID, ECS::EntityID>> FilterCCDPairs(
        const std::vector<ECS::EntityID>& candidates,
        ECS::World* world,
        float dt
    );
    
    /**
     * @brief 检测薄片物体（用于选择性 CCD）
     * 
     * 薄片物体是指一个维度远小于其他维度的物体
     * 例如：地面、墙壁等
     * 
     * @param collider 碰撞体组件
     * @param transform 变换组件
     * @return true 如果是薄片物体
     */
    static bool IsThinObject(const ColliderComponent& collider, const ECS::TransformComponent& transform);
    
    /**
     * @brief 判断是否应该对两个物体进行 CCD 检测
     * 
     * 选择性 CCD 策略：
     * 1. 高速物体 vs 任何物体 -> 需要 CCD
     * 2. 任何物体 vs 薄片物体 -> 需要 CCD
     * 3. 其他情况 -> 可选 CCD
     * 
     * @param entityA 实体 A
     * @param entityB 实体 B
     * @param world ECS World 指针
     * @return true 如果应该进行 CCD 检测
     */
    static bool ShouldPerformCCD(
        ECS::EntityID entityA,
        ECS::EntityID entityB,
        ECS::World* world
    );
};

} // namespace Physics
} // namespace Render

