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

#include "render/physics/physics_components.h"
#include "render/ecs/components.h"
#include "render/math_utils.h"
#include <cmath>

namespace Render {
namespace Physics {

/**
 * @brief 半隐式欧拉积分器
 *
 * 负责更新刚体的速度与位置，包含阻尼、速度限制与轴向锁定。
 */
class SymplecticEulerIntegrator {
public:
    /**
     * @brief 积分速度：先更新速度，再用于位置积分
     *
     * @param body 刚体组件
     * @param transform 变换组件（用于计算世界空间惯性张量，可为 nullptr）
     * @param dt 时间步长
     */
    void IntegrateVelocity(RigidBodyComponent& body,
                           const ECS::TransformComponent* transform,
                           float dt) const;

    /**
     * @brief 积分位置与旋转
     *
     * @param body 刚体组件
     * @param transform 变换组件
     * @param dt 时间步长
     */
    void IntegratePosition(RigidBodyComponent& body,
                           ECS::TransformComponent& transform,
                           float dt) const;

private:
    static void ApplyLinearConstraints(RigidBodyComponent& body);
    static void ApplyAngularConstraints(RigidBodyComponent& body);
};

} // namespace Physics
} // namespace Render
