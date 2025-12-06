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

#include "render/ecs/system.h"
#include "render/ecs/entity.h"
#include "render/application/event_bus.h"
#include "physics_components.h"
#include "physics_events.h"
#include "collision/contact_manifold.h"
#include "collision/broad_phase.h"
#include "collision/collision_detection.h"
#include <vector>
#include <memory>
#include <unordered_set>

namespace Render {
namespace Physics {

/**
 * @brief 碰撞对
 */
struct CollisionPair {
    ECS::EntityID entityA;
    ECS::EntityID entityB;
    ContactManifold manifold;
    
    CollisionPair() = default;
    CollisionPair(ECS::EntityID a, ECS::EntityID b, const ContactManifold& m)
        : entityA(a), entityB(b), manifold(m) {}
};

/**
 * @brief 碰撞检测系统
 * 
 * 负责检测场景中所有物理物体的碰撞
 * 整合粗检测和细检测
 */
class CollisionDetectionSystem : public ECS::System {
public:
    CollisionDetectionSystem();
    ~CollisionDetectionSystem() override = default;
    
    void Update(float deltaTime) override;
    int GetPriority() const override { return 100; }  // 在物理更新之前
    
    /**
     * @brief 获取当前帧的碰撞对
     */
    const std::vector<CollisionPair>& GetCollisionPairs() const {
        return m_collisionPairs;
    }
    
    /**
     * @brief 设置粗检测算法
     */
    void SetBroadPhase(std::unique_ptr<BroadPhase> broadPhase) {
        m_broadPhase = std::move(broadPhase);
    }
    
    /**
     * @brief 获取统计信息
     */
    struct Stats {
        size_t totalColliders = 0;
        size_t broadPhasePairs = 0;
        size_t narrowPhaseTests = 0;
        size_t actualCollisions = 0;
        float broadPhaseTime = 0.0f;
        float narrowPhaseTime = 0.0f;
    };
    
    const Stats& GetStats() const { return m_stats; }
    
    /**
     * @brief 设置事件总线（用于发送碰撞事件）
     */
    void SetEventBus(Application::EventBus* eventBus) {
        m_eventBus = eventBus;
    }
    
private:
    /**
     * @brief 检查两个碰撞体是否应该进行碰撞检测
     */
    bool ShouldCollide(const ColliderComponent* colliderA, 
                       const ColliderComponent* colliderB) const;
    
    /**
     * @brief 执行细检测
     */
    bool PerformNarrowPhaseDetection(
        ECS::EntityID entityA, const ColliderComponent* colliderA, const Vector3& posA, const Quaternion& rotA,
        ECS::EntityID entityB, const ColliderComponent* colliderB, const Vector3& posB, const Quaternion& rotB,
        ContactManifold& manifold
    );
    
    std::unique_ptr<BroadPhase> m_broadPhase;
    std::vector<CollisionPair> m_collisionPairs;
    std::vector<CollisionPair> m_previousCollisionPairs;  // 上一帧的碰撞对
    Application::EventBus* m_eventBus = nullptr;
    Stats m_stats;
    
    /**
     * @brief 发送碰撞事件
     */
    void SendCollisionEvents();
    
    /**
     * @brief 计算碰撞对哈希
     */
    static uint64_t HashPair(ECS::EntityID a, ECS::EntityID b) {
        if (a.index > b.index) std::swap(a, b);
        return (static_cast<uint64_t>(a.index) << 32) | static_cast<uint64_t>(b.index);
    }
};

} // namespace Physics
} // namespace Render

