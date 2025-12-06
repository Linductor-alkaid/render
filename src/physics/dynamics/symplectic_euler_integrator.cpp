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
#include "render/physics/dynamics/symplectic_euler_integrator.h"
#include <algorithm>

namespace Render {
namespace Physics {

void SymplecticEulerIntegrator::IntegrateVelocity(RigidBodyComponent& body,
                                                  const ECS::TransformComponent* transform,
                                                  float dt) const {
    // 跳过不需要积分的刚体
    if (body.IsStatic() || body.IsKinematic() || body.isSleeping) {
        body.force = Vector3::Zero();
        body.torque = Vector3::Zero();
        return;
    }

    // 线速度积分：v = v0 + a * dt，a = F * invM
    if (body.inverseMass > 0.0f) {
        Vector3 acceleration = body.force * body.inverseMass;
        body.linearVelocity += acceleration * dt;
    }

    // 线性阻尼
    if (body.linearDamping > 0.0f) {
        float dampingFactor = std::pow(std::max(0.0f, 1.0f - body.linearDamping), dt);
        body.linearVelocity *= dampingFactor;
    }

    ApplyLinearConstraints(body);

    // 角速度积分：ω = ω0 + α * dt，α = I^-1 * τ
    if (transform) {
        Quaternion rotation = transform->GetRotation();
        Matrix3 rotationMatrix = rotation.toRotationMatrix();
        Matrix3 worldInvInertia = rotationMatrix * body.inverseInertiaTensor * rotationMatrix.transpose();
        Vector3 angularAcceleration = worldInvInertia * body.torque;
        body.angularVelocity += angularAcceleration * dt;
    }

    // 角阻尼
    if (body.angularDamping > 0.0f) {
        float dampingFactor = std::pow(std::max(0.0f, 1.0f - body.angularDamping), dt);
        body.angularVelocity *= dampingFactor;
    }

    ApplyAngularConstraints(body);

    // 积分后清空力/扭矩，等待下一帧重新累积
    body.force = Vector3::Zero();
    body.torque = Vector3::Zero();
}

void SymplecticEulerIntegrator::IntegratePosition(RigidBodyComponent& body,
                                                  ECS::TransformComponent& transform,
                                                  float dt) const {
    // 跳过不需要积分的刚体
    if (body.IsStatic() || body.IsKinematic() || body.isSleeping) {
        return;
    }

    // 保存上一帧状态以便插值
    body.previousPosition = transform.GetPosition();
    body.previousRotation = transform.GetRotation();

    // 位置积分：x = x0 + v * dt
    Vector3 currentPosition = transform.GetPosition();
    Vector3 newPosition = currentPosition + body.linearVelocity * dt;

    // 位置锁定
    if (body.lockPosition[0]) newPosition.x() = currentPosition.x();
    if (body.lockPosition[1]) newPosition.y() = currentPosition.y();
    if (body.lockPosition[2]) newPosition.z() = currentPosition.z();

    transform.SetPosition(newPosition);

    // 旋转积分：q' = q * Δq，Δq 由角速度计算
    Vector3 angularVelocity = body.angularVelocity;
    float angle = angularVelocity.norm();

    // 如果所有轴都锁定，跳过旋转更新
    if (body.lockRotation[0] && body.lockRotation[1] && body.lockRotation[2]) {
        return;
    }

    if (angle > 0.001f) {
        Vector3 axis = angularVelocity / angle;
        float deltaAngle = angle * dt;

        Quaternion deltaRotation = MathUtils::AngleAxis(deltaAngle, axis);
        Quaternion newRotation = transform.GetRotation() * deltaRotation;
        newRotation.normalize();

        transform.SetRotation(newRotation);
    }
}

void SymplecticEulerIntegrator::ApplyLinearConstraints(RigidBodyComponent& body) {
    // 轴向锁定
    if (body.lockPosition[0]) body.linearVelocity.x() = 0.0f;
    if (body.lockPosition[1]) body.linearVelocity.y() = 0.0f;
    if (body.lockPosition[2]) body.linearVelocity.z() = 0.0f;

    // 最大速度限制
    if (std::isfinite(body.maxLinearSpeed) && body.maxLinearSpeed > 0.0f) {
        float maxSpeedSq = body.maxLinearSpeed * body.maxLinearSpeed;
        float speedSq = body.linearVelocity.squaredNorm();
        if (speedSq > maxSpeedSq && speedSq > 0.0f) {
            body.linearVelocity = body.linearVelocity.normalized() * body.maxLinearSpeed;
        }
    }
}

void SymplecticEulerIntegrator::ApplyAngularConstraints(RigidBodyComponent& body) {
    // 旋转轴锁定
    if (body.lockRotation[0]) body.angularVelocity.x() = 0.0f;
    if (body.lockRotation[1]) body.angularVelocity.y() = 0.0f;
    if (body.lockRotation[2]) body.angularVelocity.z() = 0.0f;

    // 最大角速度限制
    if (std::isfinite(body.maxAngularSpeed) && body.maxAngularSpeed > 0.0f) {
        float maxSpeedSq = body.maxAngularSpeed * body.maxAngularSpeed;
        float speedSq = body.angularVelocity.squaredNorm();
        if (speedSq > maxSpeedSq && speedSq > 0.0f) {
            body.angularVelocity = body.angularVelocity.normalized() * body.maxAngularSpeed;
        }
    }
}

} // namespace Physics
} // namespace Render
