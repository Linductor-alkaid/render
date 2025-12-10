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
#include "render/physics/bullet_adapter/bullet_shape_adapter.h"
#include "render/physics/bullet_adapter/eigen_to_bullet.h"
#include <BulletCollision/CollisionShapes/btSphereShape.h>
#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btCapsuleShape.h>
#include <BulletCollision/CollisionShapes/btConvexHullShape.h>
#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>

namespace Render::Physics::BulletAdapter {

btCollisionShape* BulletShapeAdapter::CreateShape(const ColliderComponent& collider) {
    // TODO: 阶段一 1.3 实现
    return nullptr;
}

void BulletShapeAdapter::UpdateShape(btCollisionShape* shape, const ColliderComponent& collider) {
    // TODO: 阶段一 1.3 实现
}

void BulletShapeAdapter::DestroyShape(btCollisionShape* shape) {
    // TODO: 阶段一 1.3 实现
    if (shape) {
        delete shape;
    }
}

} // namespace Render::Physics::BulletAdapter

