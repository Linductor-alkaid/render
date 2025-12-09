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

#include "render/physics/collision/ccd_broad_phase.h"
#include "render/physics/physics_utils.h"
#include "render/physics/collision/broad_phase.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/math_utils.h"
#include <algorithm>
#include <cmath>

namespace Render {
namespace Physics {

AABB CCDBroadPhase::ComputeSweptAABB(const AABB& aabb0, const Vector3& velocity, float dt) {
    Vector3 min0 = aabb0.min;
    Vector3 max0 = aabb0.max;
    
    // 计算运动后的 AABB
    Vector3 min1 = min0 + velocity * dt;
    Vector3 max1 = max0 + velocity * dt;
    
    // 合并两个 AABB（包含整个运动轨迹）
    Vector3 sweptMin(
        std::min(min0.x(), min1.x()),
        std::min(min0.y(), min1.y()),
        std::min(min0.z(), min1.z())
    );
    Vector3 sweptMax(
        std::max(max0.x(), max1.x()),
        std::max(max0.y(), max1.y()),
        std::max(max0.z(), max1.z())
    );
    
    return AABB(sweptMin, sweptMax);
}

std::vector<std::pair<ECS::EntityID, ECS::EntityID>> CCDBroadPhase::FilterCCDPairs(
    const std::vector<ECS::EntityID>& candidates,
    ECS::World* world,
    float dt
) {
    std::vector<std::pair<ECS::EntityID, ECS::EntityID>> pairs;
    
    if (!world || candidates.empty()) {
        return pairs;
    }
    
    // 使用空间哈希进行快速筛选
    SpatialHashBroadPhase broadPhase(5.0f);  // 格子大小 5.0m
    
    // 收集所有 CCD 候选物体的扫描 AABB
    std::vector<std::pair<ECS::EntityID, AABB>> sweptAABBs;
    std::unordered_map<ECS::EntityID, Vector3, ECS::EntityID::Hash> velocities;
    
    for (ECS::EntityID candidate : candidates) {
        if (!world->HasComponent<RigidBodyComponent>(candidate) ||
            !world->HasComponent<ColliderComponent>(candidate) ||
            !world->HasComponent<ECS::TransformComponent>(candidate)) {
            continue;
        }
        
        try {
            auto& body = world->GetComponent<RigidBodyComponent>(candidate);
            auto& collider = world->GetComponent<ColliderComponent>(candidate);
            auto& transform = world->GetComponent<ECS::TransformComponent>(candidate);
            
            // 获取当前 AABB
            AABB currentAABB = collider.worldAABB;
            if (collider.aabbDirty && transform.transform) {
                Matrix4 worldMatrix = transform.transform->GetWorldMatrix();
                currentAABB = PhysicsUtils::ComputeWorldAABB(collider, worldMatrix);
            }
            
            // 计算扫描 AABB
            Vector3 velocity = body.linearVelocity;
            AABB sweptAABB = ComputeSweptAABB(currentAABB, velocity, dt);
            
            sweptAABBs.push_back({candidate, sweptAABB});
            velocities[candidate] = velocity;
        } catch (...) {
            // 忽略组件访问错误
        }
    }
    
    // 更新 Broad Phase
    broadPhase.Update(sweptAABBs);
    
    // 获取潜在碰撞对
    auto potentialPairs = broadPhase.DetectPairs();
    
    // 进一步筛选：检查扫描 AABB 是否真的相交
    for (const auto& pair : potentialPairs) {
        ECS::EntityID entityA = pair.first;
        ECS::EntityID entityB = pair.second;
        
        // 检查是否应该进行 CCD
        if (ShouldPerformCCD(entityA, entityB, world)) {
            pairs.push_back(pair);
        }
    }
    
    return pairs;
}

bool CCDBroadPhase::IsThinObject(const ColliderComponent& collider, const ECS::TransformComponent& transform) {
    // 计算物体的尺寸
    Vector3 size = Vector3::Zero();
    
    // 获取缩放
    Vector3 scale = transform.GetScale();
    
    switch (collider.shapeType) {
        case ColliderComponent::ShapeType::Box: {
            Vector3 halfExtents = collider.GetBoxHalfExtents();
            // 考虑缩放
            size = halfExtents.cwiseProduct(scale) * 2.0f;  // 全尺寸
            break;
        }
        case ColliderComponent::ShapeType::Sphere: {
            float radius = collider.shapeData.sphere.radius;
            float maxScale = scale.maxCoeff();
            float diameter = radius * 2.0f * maxScale;
            size = Vector3(diameter, diameter, diameter);
            break;
        }
        case ColliderComponent::ShapeType::Capsule: {
            float radius = collider.shapeData.capsule.radius;
            float height = collider.shapeData.capsule.height;
            float maxScale = scale.maxCoeff();
            size = Vector3(
                radius * 2.0f * maxScale,
                (height + radius * 2.0f) * maxScale,
                radius * 2.0f * maxScale
            );
            break;
        }
        default:
            return false;
    }
    
    // 计算尺寸比例
    float maxSize = size.maxCoeff();
    float minSize = size.minCoeff();
    
    // 如果最小尺寸小于最大尺寸的 10%，认为是薄片物体
    const float thinThreshold = 0.1f;
    if (maxSize < MathUtils::EPSILON) {
        return false;  // 避免除零
    }
    return (minSize / maxSize) < thinThreshold;
}

bool CCDBroadPhase::ShouldPerformCCD(
    ECS::EntityID entityA,
    ECS::EntityID entityB,
    ECS::World* world
) {
    if (!world) {
        return false;
    }
    
    if (!world->HasComponent<RigidBodyComponent>(entityA) ||
        !world->HasComponent<ColliderComponent>(entityA) ||
        !world->HasComponent<ECS::TransformComponent>(entityA) ||
        !world->HasComponent<RigidBodyComponent>(entityB) ||
        !world->HasComponent<ColliderComponent>(entityB) ||
        !world->HasComponent<ECS::TransformComponent>(entityB)) {
        return false;
    }
    
    try {
        auto& bodyA = world->GetComponent<RigidBodyComponent>(entityA);
        auto& colliderA = world->GetComponent<ColliderComponent>(entityA);
        auto& transformA = world->GetComponent<ECS::TransformComponent>(entityA);
        
        auto& bodyB = world->GetComponent<RigidBodyComponent>(entityB);
        auto& colliderB = world->GetComponent<ColliderComponent>(entityB);
        auto& transformB = world->GetComponent<ECS::TransformComponent>(entityB);
        
        // 策略 1: 如果任一物体强制启用 CCD，则进行检测
        if (bodyA.useCCD || bodyB.useCCD) {
            return true;
        }
        
        // 策略 2: 如果任一物体是高速物体，则进行检测
        float speedA = bodyA.linearVelocity.norm();
        float speedB = bodyB.linearVelocity.norm();
        const float highSpeedThreshold = 10.0f;  // m/s
        
        if (speedA >= highSpeedThreshold || speedB >= highSpeedThreshold) {
            return true;
        }
        
        // 策略 3: 如果任一物体是薄片物体，则进行检测
        if (IsThinObject(colliderA, transformA) || IsThinObject(colliderB, transformB)) {
            return true;
        }
        
        // 策略 4: 如果两个物体都是静态的，不需要 CCD
        if (bodyA.IsStatic() && bodyB.IsStatic()) {
            return false;
        }
        
        // 默认：对于动态物体，如果速度较高，进行 CCD
        // 这里可以根据需要调整策略
        return false;
    } catch (...) {
        return false;
    }
}

} // namespace Physics
} // namespace Render

