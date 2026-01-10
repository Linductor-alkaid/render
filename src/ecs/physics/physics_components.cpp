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

#include "render/ecs/physics/physics_components.h"
#include <btBulletDynamicsCommon.h>

namespace Render {
namespace ECS {

// ============================================================
// RigidBodyComponent 便捷方法实现
// ============================================================

void RigidBodyComponent::ApplyForce(const Vector3& force) {
    if (!bulletRigidBody) {
        return;
    }
    
    btRigidBody* body = static_cast<btRigidBody*>(bulletRigidBody);
    btVector3 btForce(force.x(), force.y(), force.z());
    body->applyCentralForce(btForce);
}

void RigidBodyComponent::ApplyImpulse(const Vector3& impulse) {
    if (!bulletRigidBody) {
        return;
    }
    
    btRigidBody* body = static_cast<btRigidBody*>(bulletRigidBody);
    btVector3 btImpulse(impulse.x(), impulse.y(), impulse.z());
    body->applyCentralImpulse(btImpulse);
}

void RigidBodyComponent::ApplyTorque(const Vector3& torque) {
    if (!bulletRigidBody) {
        return;
    }
    
    btRigidBody* body = static_cast<btRigidBody*>(bulletRigidBody);
    btVector3 btTorque(torque.x(), torque.y(), torque.z());
    body->applyTorque(btTorque);
}

void RigidBodyComponent::SetVelocity(const Vector3& linear, const Vector3& angular) {
    linearVelocity = linear;
    angularVelocity = angular;
    
    if (!bulletRigidBody) {
        return;
    }
    
    btRigidBody* body = static_cast<btRigidBody*>(bulletRigidBody);
    btVector3 btLinear(linear.x(), linear.y(), linear.z());
    btVector3 btAngular(angular.x(), angular.y(), angular.z());
    body->setLinearVelocity(btLinear);
    body->setAngularVelocity(btAngular);
}

void RigidBodyComponent::ClearForces() {
    if (!bulletRigidBody) {
        return;
    }
    
    btRigidBody* body = static_cast<btRigidBody*>(bulletRigidBody);
    body->clearForces();
}

} // namespace ECS
} // namespace Render
