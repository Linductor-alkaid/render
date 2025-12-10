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
#include "render/physics/bullet_adapter/bullet_world_adapter.h"
#include "render/physics/bullet_adapter/eigen_to_bullet.h"
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>

namespace Render::Physics::BulletAdapter {

BulletWorldAdapter::BulletWorldAdapter(const PhysicsConfig& config) {
    // TODO: 阶段二 2.1 实现初始化
    // 创建 broadphase
    // 创建 collision configuration
    // 创建 dispatcher
    // 创建 solver
    // 创建 dynamics world
}

BulletWorldAdapter::~BulletWorldAdapter() {
    // TODO: 阶段二 2.1 实现清理
}

void BulletWorldAdapter::Step(float deltaTime) {
    // TODO: 阶段二 2.1 实现
    if (m_bulletWorld) {
        m_bulletWorld->stepSimulation(deltaTime);
    }
}

void BulletWorldAdapter::SetGravity(const Vector3& gravity) {
    // TODO: 阶段二 2.1 实现
    if (m_bulletWorld) {
        m_bulletWorld->setGravity(ToBullet(gravity));
    }
}

Vector3 BulletWorldAdapter::GetGravity() const {
    // TODO: 阶段二 2.1 实现
    if (m_bulletWorld) {
        return FromBullet(m_bulletWorld->getGravity());
    }
    return Vector3::Zero();
}

} // namespace Render::Physics::BulletAdapter

