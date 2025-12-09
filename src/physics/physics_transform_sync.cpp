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
#include "render/physics/physics_transform_sync.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/physics/physics_components.h"
#include "render/math_utils.h"
#include "render/logger.h"
#include <algorithm>

namespace Render {
namespace Physics {

void PhysicsTransformSync::SyncPhysicsToTransform(ECS::World* world) {
    if (!world) {
        return;
    }
    
    // 查询所有具有 RigidBodyComponent 和 TransformComponent 的实体
    auto entities = world->Query<ECS::TransformComponent, RigidBodyComponent>();
    
    for (ECS::EntityID entity : entities) {
        if (!world->HasComponent<ECS::TransformComponent>(entity) ||
            !world->HasComponent<RigidBodyComponent>(entity)) {
            continue;
        }
        
        try {
            auto& transform = world->GetComponent<ECS::TransformComponent>(entity);
            auto& body = world->GetComponent<RigidBodyComponent>(entity);
            
            // 只处理动态物体
            if (!body.IsDynamic()) {
                continue;
            }
            
            // 只处理根对象（无父实体的对象）
            // 子对象的变换由父对象通过 Transform 层级自动计算
            if (!IsRootEntity(world, entity)) {
                continue;
            }
            
            // 从 Transform 获取当前物理位置（已在积分器中更新）
            // 这里主要是确保 Transform 与物理状态一致
            Vector3 currentPos = transform.GetPosition();
            Quaternion currentRot = transform.GetRotation();
            
            // 将上一帧的 currentTransforms 复制到 previousTransforms
            // 用于下一帧的插值
            auto prevIt = m_currentTransforms.find(entity);
            if (prevIt != m_currentTransforms.end()) {
                m_previousTransforms[entity] = prevIt->second;
            } else {
                // 如果没有上一帧的缓存，使用 body.previousPosition/previousRotation
                CachedTransformState prevState;
                prevState.position = body.previousPosition;
                prevState.rotation = body.previousRotation;
                m_previousTransforms[entity] = prevState;
            }
            
            // 更新 RigidBodyComponent 中的 previousPosition/previousRotation
            // 用于下一帧的插值
            body.previousPosition = currentPos;
            body.previousRotation = currentRot;
            
            // 缓存当前状态用于插值
            CachedTransformState state;
            state.position = currentPos;
            state.rotation = currentRot;
            m_currentTransforms[entity] = state;
            
        } catch (...) {
            // 忽略组件访问错误
        }
    }
}

void PhysicsTransformSync::SyncTransformToPhysics(ECS::World* world, float deltaTime) {
    if (!world) {
        return;
    }
    
    // 查询所有具有 RigidBodyComponent 和 TransformComponent 的实体
    auto entities = world->Query<ECS::TransformComponent, RigidBodyComponent>();
    
    for (ECS::EntityID entity : entities) {
        if (!world->HasComponent<ECS::TransformComponent>(entity) ||
            !world->HasComponent<RigidBodyComponent>(entity)) {
            continue;
        }
        
        try {
            auto& transform = world->GetComponent<ECS::TransformComponent>(entity);
            auto& body = world->GetComponent<RigidBodyComponent>(entity);
            
            // 只处理 Kinematic 和 Static 物体
            if (body.IsDynamic()) {
                continue;
            }
            
            // 只处理根对象
            if (!IsRootEntity(world, entity)) {
                continue;
            }
            
            Vector3 currentPos = transform.GetPosition();
            Quaternion currentRot = transform.GetRotation();
            
            // 对于 Kinematic 物体，计算速度（基于位置变化）
            if (body.IsKinematic() && deltaTime > 1e-6f) {
                // 计算线性速度
                Vector3 positionDelta = currentPos - body.previousPosition;
                body.linearVelocity = positionDelta / deltaTime;
                
                // 计算角速度（基于旋转变化）
                // 使用四元数的对数映射：log(q) = (0, axis * angle)
                // 对于小角度：log(q) ≈ (0, 2 * (q.x, q.y, q.z) / q.w)
                Quaternion rotationDelta = currentRot * body.previousRotation.inverse();
                rotationDelta.normalize();
                
                // 使用 Eigen 的 AngleAxis 转换
                Eigen::AngleAxisf angleAxis(rotationDelta);
                if (deltaTime > 1e-6f && angleAxis.angle() > 1e-6f) {
                    body.angularVelocity = angleAxis.axis() * (angleAxis.angle() / deltaTime);
                } else {
                    body.angularVelocity = Vector3::Zero();
                }
            }
            
            // 更新 previousPosition/previousRotation
            body.previousPosition = currentPos;
            body.previousRotation = currentRot;
            
        } catch (...) {
            // 忽略组件访问错误
        }
    }
}

void PhysicsTransformSync::InterpolateTransforms(ECS::World* world, float alpha) {
    if (!world) {
        return;
    }
    
    float t = MathUtils::Clamp(alpha, 0.0f, 1.0f);
    
    // 查询所有具有 RigidBodyComponent 和 TransformComponent 的实体
    auto entities = world->Query<ECS::TransformComponent, RigidBodyComponent>();
    
    for (ECS::EntityID entity : entities) {
        if (!world->HasComponent<ECS::TransformComponent>(entity) ||
            !world->HasComponent<RigidBodyComponent>(entity)) {
            continue;
        }
        
        try {
            auto& transform = world->GetComponent<ECS::TransformComponent>(entity);
            auto& body = world->GetComponent<RigidBodyComponent>(entity);
            
            // 只处理动态物体（Kinematic/Static 不需要插值）
            if (!body.IsDynamic()) {
                continue;
            }
            
            // 只处理根对象
            if (!IsRootEntity(world, entity)) {
                continue;
            }
            
            // 获取上一帧和当前帧的状态
            // 优先使用缓存的变换状态，如果没有则使用 body 中的状态
            Vector3 previousPos;
            Quaternion previousRot;
            
            auto prevIt = m_previousTransforms.find(entity);
            if (prevIt != m_previousTransforms.end()) {
                previousPos = prevIt->second.position;
                previousRot = prevIt->second.rotation;
            } else {
                // 如果没有缓存，使用 body.previousPosition/previousRotation
                previousPos = body.previousPosition;
                previousRot = body.previousRotation;
            }
            
            // 从缓存或当前 Transform 获取当前状态
            Vector3 currentPos;
            Quaternion currentRot;
            
            auto it = m_currentTransforms.find(entity);
            if (it != m_currentTransforms.end()) {
                currentPos = it->second.position;
                currentRot = it->second.rotation;
            } else {
                // 如果没有缓存，使用当前 Transform
                currentPos = transform.GetPosition();
                currentRot = transform.GetRotation();
            }
            
            // 进行插值
            Vector3 interpolatedPos = MathUtils::Lerp(previousPos, currentPos, t);
            Quaternion interpolatedRot = MathUtils::Slerp(previousRot, currentRot, t);
            
            // 更新 Transform
            transform.SetPosition(interpolatedPos);
            transform.SetRotation(interpolatedRot);
            
        } catch (...) {
            // 忽略组件访问错误
        }
    }
}

void PhysicsTransformSync::ClearCache() {
    m_previousTransforms.clear();
    m_currentTransforms.clear();
}

bool PhysicsTransformSync::IsRootEntity(ECS::World* world, ECS::EntityID entity) const {
    if (!world || !world->HasComponent<ECS::TransformComponent>(entity)) {
        return false;
    }
    
    try {
        auto& transform = world->GetComponent<ECS::TransformComponent>(entity);
        // 检查是否有父实体
        return !transform.GetParentEntity().IsValid();
    } catch (...) {
        return false;
    }
}

} // namespace Physics
} // namespace Render

