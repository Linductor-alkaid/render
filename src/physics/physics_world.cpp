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
#include "render/physics/physics_world.h"
#include "render/physics/physics_systems.h"
#include "render/physics/physics_transform_sync.h"
#include "render/ecs/world.h"
#include <memory>

namespace Render {
namespace Physics {

PhysicsWorld::PhysicsWorld(ECS::World* ecsWorld, const PhysicsConfig& config)
    : m_ecsWorld(ecsWorld), m_config(config) {
    // 创建物理-渲染同步器
    m_transformSync = std::make_unique<PhysicsTransformSync>();
}

void PhysicsWorld::Step(float deltaTime) {
    if (!m_ecsWorld) {
        return;
    }

    // 1. 渲染 → 物理同步（Kinematic/Static 物体）
    if (m_transformSync) {
        m_transformSync->SyncTransformToPhysics(m_ecsWorld, deltaTime);
    }

    // 2. 物理更新
    auto* physicsSystem = m_ecsWorld->GetSystem<PhysicsUpdateSystem>();
    if (physicsSystem) {
        physicsSystem->SetGravity(m_config.gravity);
        physicsSystem->SetFixedDeltaTime(m_config.fixedDeltaTime);
        physicsSystem->SetSolverIterations(m_config.solverIterations);
        physicsSystem->SetPositionIterations(m_config.positionIterations);
        physicsSystem->Update(deltaTime);
    }

    // 3. 物理 → 渲染同步（动态物体）
    if (m_transformSync) {
        m_transformSync->SyncPhysicsToTransform(m_ecsWorld);
    }

    // 4. 插值变换（平滑渲染）
    if (physicsSystem && m_transformSync) {
        float alpha = physicsSystem->GetInterpolationAlpha();
        m_transformSync->InterpolateTransforms(m_ecsWorld, alpha);
    }
}

/**
 * @brief 获取插值因子并执行插值
 * 
 * 这个方法应该在物理更新后、渲染前调用
 * 用于在固定时间步长和渲染帧率之间进行平滑插值
 */
void PhysicsWorld::InterpolateTransforms(float alpha) {
    if (m_transformSync) {
        m_transformSync->InterpolateTransforms(m_ecsWorld, alpha);
    }
}

} // namespace Physics
} // namespace Render

