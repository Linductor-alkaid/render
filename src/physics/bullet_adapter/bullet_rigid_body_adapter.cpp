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
#include "render/physics/bullet_adapter/bullet_rigid_body_adapter.h"
#include "render/physics/bullet_adapter/eigen_to_bullet.h"
#include <BulletDynamics/Dynamics/btRigidBody.h>

namespace Render::Physics::BulletAdapter {

BulletRigidBodyAdapter::BulletRigidBodyAdapter(btRigidBody* bulletBody, ECS::EntityID entity)
    : m_bulletBody(bulletBody)
    , m_entity(entity) {
}

void BulletRigidBodyAdapter::SyncToBullet(const RigidBodyComponent& component) {
    // TODO: 阶段一 1.4 实现
}

void BulletRigidBodyAdapter::SyncFromBullet(RigidBodyComponent& component) {
    // TODO: 阶段一 1.4 实现
}

} // namespace Render::Physics::BulletAdapter

