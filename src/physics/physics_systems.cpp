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
#include "render/physics/physics_systems.h"
#include "render/physics/physics_utils.h"
#include "render/physics/collision/collision_shapes.h"
#include "render/physics/collision/gjk.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/logger.h"
#include <chrono>

namespace Render {
namespace Physics {

CollisionDetectionSystem::CollisionDetectionSystem() {
    // 默认使用空间哈希
    m_broadPhase = std::make_unique<SpatialHashBroadPhase>(5.0f);
}

void CollisionDetectionSystem::Update(float deltaTime) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    m_collisionPairs.clear();
    m_stats = Stats{};
    
    if (!m_world) {
        return;
    }
    
    // 收集所有碰撞体及其 AABB
    std::vector<std::pair<ECS::EntityID, AABB>> colliderEntities;
    
    // 查询所有具有 TransformComponent 和 ColliderComponent 的实体
    auto entities = m_world->Query<ECS::TransformComponent, Physics::ColliderComponent>();
    
    for (ECS::EntityID entity : entities) {
        if (!m_world->HasComponent<ECS::TransformComponent>(entity) ||
            !m_world->HasComponent<Physics::ColliderComponent>(entity)) {
            continue;
        }
        
        try {
            auto& transform = m_world->GetComponent<ECS::TransformComponent>(entity);
            auto& collider = m_world->GetComponent<ColliderComponent>(entity);
            
            // 更新 AABB
            if (collider.aabbDirty || transform.transform->IsDirty()) {
                collider.worldAABB = PhysicsUtils::ComputeWorldAABB(collider, *transform.transform);
                collider.aabbDirty = false;
            }
            
            colliderEntities.push_back({entity, collider.worldAABB});
            m_stats.totalColliders++;
        } catch (...) {
            // 忽略组件访问错误
        }
    }
    
    auto broadPhaseStart = std::chrono::high_resolution_clock::now();
    
    // 粗检测
    m_broadPhase->Update(colliderEntities);
    auto potentialPairs = m_broadPhase->DetectPairs();
    m_stats.broadPhasePairs = potentialPairs.size();
    
    auto broadPhaseEnd = std::chrono::high_resolution_clock::now();
    m_stats.broadPhaseTime = std::chrono::duration<float, std::milli>(broadPhaseEnd - broadPhaseStart).count();
    
    auto narrowPhaseStart = std::chrono::high_resolution_clock::now();
    
    // 细检测
    for (const auto& [entityA, entityB] : potentialPairs) {
        // 检查组件是否存在
        if (!m_world->HasComponent<Physics::ColliderComponent>(entityA) ||
            !m_world->HasComponent<Physics::ColliderComponent>(entityB) ||
            !m_world->HasComponent<ECS::TransformComponent>(entityA) ||
            !m_world->HasComponent<ECS::TransformComponent>(entityB)) {
            continue;
        }
        
        try {
            // 获取组件
            auto& colliderA = m_world->GetComponent<Physics::ColliderComponent>(entityA);
            auto& colliderB = m_world->GetComponent<Physics::ColliderComponent>(entityB);
            auto& transformA = m_world->GetComponent<ECS::TransformComponent>(entityA);
            auto& transformB = m_world->GetComponent<ECS::TransformComponent>(entityB);
            
            // 检查是否应该碰撞
            if (!ShouldCollide(&colliderA, &colliderB)) {
                continue;
            }
            
            m_stats.narrowPhaseTests++;
            
            // 执行细检测
            ContactManifold manifold;
            Vector3 posA = transformA.GetPosition();
            Quaternion rotA = transformA.GetRotation();
            Vector3 posB = transformB.GetPosition();
            Quaternion rotB = transformB.GetRotation();
            
            bool collided = PerformNarrowPhaseDetection(
                entityA, &colliderA, posA, rotA,
                entityB, &colliderB, posB, rotB,
                manifold
            );
            
            if (collided) {
                // 补充局部接触点坐标，便于后续约束求解
                for (int i = 0; i < manifold.contactCount; ++i) {
                    auto& cp = manifold.contacts[i];
                    cp.localPointA = rotA.conjugate() * (cp.position - posA);
                    cp.localPointB = rotB.conjugate() * (cp.position - posB);
                }

                m_collisionPairs.emplace_back(entityA, entityB, manifold);
                m_stats.actualCollisions++;
            }
        } catch (...) {
            // 忽略组件访问错误
        }
    }
    
    auto narrowPhaseEnd = std::chrono::high_resolution_clock::now();
    m_stats.narrowPhaseTime = std::chrono::duration<float, std::milli>(narrowPhaseEnd - narrowPhaseStart).count();
    
    // 发送碰撞事件
    if (m_eventBus) {
        SendCollisionEvents();
    }
    
    // 保存当前帧的碰撞对，用于下一帧判断 Enter/Stay/Exit
    m_previousCollisionPairs = m_collisionPairs;
}

void CollisionDetectionSystem::SendCollisionEvents() {
    // 构建当前帧碰撞对的哈希集合
    std::unordered_set<uint64_t> currentPairs;
    for (const auto& pair : m_collisionPairs) {
        currentPairs.insert(HashPair(pair.entityA, pair.entityB));
    }
    
    // 构建上一帧碰撞对的哈希集合
    std::unordered_set<uint64_t> previousPairs;
    for (const auto& pair : m_previousCollisionPairs) {
        previousPairs.insert(HashPair(pair.entityA, pair.entityB));
    }
    
    // 检测碰撞事件
    for (const auto& pair : m_collisionPairs) {
        uint64_t pairHash = HashPair(pair.entityA, pair.entityB);
        
        // 获取碰撞体组件，判断是否为触发器
        if (!m_world->HasComponent<Physics::ColliderComponent>(pair.entityA) ||
            !m_world->HasComponent<Physics::ColliderComponent>(pair.entityB)) {
            continue;
        }
        
        auto& colliderA = m_world->GetComponent<Physics::ColliderComponent>(pair.entityA);
        auto& colliderB = m_world->GetComponent<Physics::ColliderComponent>(pair.entityB);
        
        bool isTrigger = colliderA.isTrigger || colliderB.isTrigger;
        
        if (previousPairs.find(pairHash) == previousPairs.end()) {
            // 新的碰撞
            if (isTrigger) {
                ECS::EntityID trigger = colliderA.isTrigger ? pair.entityA : pair.entityB;
                ECS::EntityID other = (trigger == pair.entityA) ? pair.entityB : pair.entityA;
                m_eventBus->Publish(TriggerEnterEvent(trigger, other, pair.manifold));
            } else {
                m_eventBus->Publish(CollisionEnterEvent(pair.entityA, pair.entityB, pair.manifold));
            }
        } else {
            // 持续碰撞
            if (isTrigger) {
                ECS::EntityID trigger = colliderA.isTrigger ? pair.entityA : pair.entityB;
                ECS::EntityID other = (trigger == pair.entityA) ? pair.entityB : pair.entityA;
                m_eventBus->Publish(TriggerStayEvent(trigger, other));
            } else {
                m_eventBus->Publish(CollisionStayEvent(pair.entityA, pair.entityB, pair.manifold));
            }
        }
    }
    
    // 检测碰撞结束
    for (const auto& pair : m_previousCollisionPairs) {
        uint64_t pairHash = HashPair(pair.entityA, pair.entityB);
        
        if (currentPairs.find(pairHash) == currentPairs.end()) {
            // 碰撞结束
            bool hasA = m_world->HasComponent<Physics::ColliderComponent>(pair.entityA);
            bool hasB = m_world->HasComponent<Physics::ColliderComponent>(pair.entityB);
            
            if (!hasA || !hasB) continue;
            
            try {
                auto& colliderA = m_world->GetComponent<Physics::ColliderComponent>(pair.entityA);
                auto& colliderB = m_world->GetComponent<Physics::ColliderComponent>(pair.entityB);
                
                bool isTrigger = colliderA.isTrigger || colliderB.isTrigger;
                
                if (isTrigger) {
                    ECS::EntityID trigger = colliderA.isTrigger ? pair.entityA : pair.entityB;
                    ECS::EntityID other = (trigger == pair.entityA) ? pair.entityB : pair.entityA;
                    m_eventBus->Publish(TriggerExitEvent(trigger, other));
                } else {
                    m_eventBus->Publish(CollisionExitEvent(pair.entityA, pair.entityB));
                }
            } catch (...) {
                // 忽略错误
            }
        }
    }
}

bool CollisionDetectionSystem::ShouldCollide(
    const ColliderComponent* colliderA,
    const ColliderComponent* colliderB
) const {
    // 检查碰撞层
    bool layerMatch = (colliderA->collisionMask & (1u << colliderB->collisionLayer)) != 0 &&
                      (colliderB->collisionMask & (1u << colliderA->collisionLayer)) != 0;
    
    return layerMatch;
}

bool CollisionDetectionSystem::PerformNarrowPhaseDetection(
    ECS::EntityID entityA, const ColliderComponent* colliderA, const Vector3& posA, const Quaternion& rotA,
    ECS::EntityID entityB, const ColliderComponent* colliderB, const Vector3& posB, const Quaternion& rotB,
    ContactManifold& manifold
) {
    // 应用碰撞体的局部偏移
    Vector3 worldPosA = posA + rotA * colliderA->center;
    Vector3 worldPosB = posB + rotB * colliderB->center;
    Quaternion worldRotA = rotA * colliderA->rotation;
    Quaternion worldRotB = rotB * colliderB->rotation;
    
    // 根据形状类型选择检测方法
    ShapeType typeA = static_cast<ShapeType>(colliderA->shapeType);
    ShapeType typeB = static_cast<ShapeType>(colliderB->shapeType);
    
    // 球体碰撞检测
    if (typeA == ShapeType::Sphere && typeB == ShapeType::Sphere) {
        return CollisionDetector::SphereVsSphere(
            worldPosA, colliderA->shapeData.sphere.radius,
            worldPosB, colliderB->shapeData.sphere.radius,
            manifold
        );
    }
    
    if (typeA == ShapeType::Sphere && typeB == ShapeType::Box) {
        return CollisionDetector::SphereVsBox(
            worldPosA, colliderA->shapeData.sphere.radius,
            worldPosB, colliderB->GetBoxHalfExtents(), worldRotB,
            manifold
        );
    }
    
    if (typeA == ShapeType::Box && typeB == ShapeType::Sphere) {
        bool hit = CollisionDetector::SphereVsBox(
            worldPosB, colliderB->shapeData.sphere.radius,
            worldPosA, colliderA->GetBoxHalfExtents(), worldRotA,
            manifold
        );
        if (hit) {
            manifold.SetNormal(-manifold.normal);
        }
        return hit;
    }
    
    if (typeA == ShapeType::Box && typeB == ShapeType::Box) {
        return CollisionDetector::BoxVsBox(
            worldPosA, colliderA->GetBoxHalfExtents(), worldRotA,
            worldPosB, colliderB->GetBoxHalfExtents(), worldRotB,
            manifold
        );
    }
    
    if (typeA == ShapeType::Capsule && typeB == ShapeType::Capsule) {
        return CollisionDetector::CapsuleVsCapsule(
            worldPosA, colliderA->shapeData.capsule.radius, colliderA->shapeData.capsule.height, worldRotA,
            worldPosB, colliderB->shapeData.capsule.radius, colliderB->shapeData.capsule.height, worldRotB,
            manifold
        );
    }
    
    // 其他组合：使用 GJK（待实现形状对象）
    // 目前返回 false
    return false;
}

} // namespace Physics
} // namespace Render

