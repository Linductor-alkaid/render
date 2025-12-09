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
#include "collision/ccd_detector.h"
#include "dynamics/symplectic_euler_integrator.h"
#include "dynamics/constraint_solver.h"
#include "physics_config.h"
#include <vector>
#include <memory>
#include <unordered_set>
#include <unordered_map>

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

// ============================================================================
// 物理更新系统
// ============================================================================

/**
 * @brief 物理更新系统
 * 
 * 负责更新所有刚体的物理状态
 * 包括：应用力、重力、积分速度、积分位置等
 */
class PhysicsUpdateSystem : public ECS::System {
public:
    PhysicsUpdateSystem();
    ~PhysicsUpdateSystem() override = default;
    
    void Update(float deltaTime) override;
    int GetPriority() const override { return 200; }  // 在碰撞检测之后
    
    /**
     * @brief 设置重力
     */
    void SetGravity(const Vector3& gravity) {
        m_gravity = gravity;
    }
    
    /**
     * @brief 获取重力
     */
    Vector3 GetGravity() const {
        return m_gravity;
    }
    
    /**
     * @brief 设置固定时间步长
     */
    void SetFixedDeltaTime(float dt) {
        m_fixedDeltaTime = dt;
    }
    
    /**
     * @brief 获取固定时间步长
     */
    float GetFixedDeltaTime() const {
        return m_fixedDeltaTime;
    }
    
    /**
     * @brief 设置求解迭代次数
     */
    void SetSolverIterations(int iterations) {
        m_solverIterations = iterations;
    }
    
    /**
     * @brief 设置位置求解迭代次数
     */
    void SetPositionIterations(int iterations) {
        m_positionIterations = iterations;
    }
    
    /**
     * @brief 应用力到刚体
     */
    void ApplyForce(ECS::EntityID entity, const Vector3& force);
    
    /**
     * @brief 在指定点应用力到刚体
     */
    void ApplyForceAtPoint(ECS::EntityID entity, const Vector3& force, const Vector3& point);
    
    /**
     * @brief 应用扭矩到刚体
     */
    void ApplyTorque(ECS::EntityID entity, const Vector3& torque);
    
    /**
     * @brief 应用冲量到刚体
     */
    void ApplyImpulse(ECS::EntityID entity, const Vector3& impulse);
    
    /**
     * @brief 在指定点应用冲量到刚体
     */
    void ApplyImpulseAtPoint(ECS::EntityID entity, const Vector3& impulse, const Vector3& point);
    
    /**
     * @brief 应用角冲量到刚体
     */
    void ApplyAngularImpulse(ECS::EntityID entity, const Vector3& angularImpulse);
    
    /**
     * @brief 获取当前插值因子
     * 
     * 用于在固定时间步长和渲染帧率之间进行插值
     * @return 插值因子 [0, 1]，0 表示上一帧，1 表示当前帧
     */
    float GetInterpolationAlpha() const;
    
    /**
     * @brief 设置物理配置（包括 CCD 配置）
     */
    void SetConfig(const PhysicsConfig& config) {
        m_config = config;
    }
    
    /**
     * @brief 获取物理配置
     */
    const PhysicsConfig& GetConfig() const {
        return m_config;
    }
    
    /**
     * @brief 检测需要 CCD 的物体（用于测试）
     */
    std::vector<ECS::EntityID> DetectCCDCandidates(float dt);
    
private:
    /**
     * @brief 用于在渲染插值前后保存/恢复模拟结果
     */
    struct SimulatedTransformState {
        Vector3 position{0.0f, 0.0f, 0.0f};
        Quaternion rotation{1.0f, 0.0f, 0.0f, 0.0f};
    };
    
    /**
     * @brief 固定时间步长更新
     */
    void FixedUpdate(float dt);
    
    /**
     * @brief 应用重力和力场
     */
    void ApplyForces(float dt);
    
    /**
     * @brief 应用力场到刚体
     */
    /**
     * @brief 计算力场对刚体的作用力
     * @return 作用力（N）
     */
    Vector3 ApplyForceField(const ForceFieldComponent& field, 
                            const Vector3& fieldPosition,
                            const RigidBodyComponent& body,
                            const Vector3& bodyPosition);
    
    /**
     * @brief 积分速度
     */
    void IntegrateVelocity(float dt);
    
    /**
     * @brief 积分位置
     */
    void IntegratePosition(float dt);
    
    /**
     * @brief 使用 CCD 进行路径积分
     */
    void IntegrateWithCCD(float dt, const std::vector<ECS::EntityID>& candidates);
    
    /**
     * @brief 积分单个实体到指定时间
     */
    void IntegratePositionToTime(ECS::EntityID entity, float toi);
    
    /**
     * @brief 处理 CCD 碰撞
     */
    void HandleCCDCollision(ECS::EntityID entity, const CCDResult& result, ECS::EntityID otherEntity);
    
    /**
     * @brief 从 ColliderComponent 创建 CollisionShape（辅助函数）
     */
    static std::unique_ptr<CollisionShape> CreateShapeFromCollider(const ColliderComponent& collider);
    
    /**
     * @brief 更新 AABB
     */
    void UpdateAABBs();
    
    /**
     * @brief 恢复上次物理解算后的状态（避免插值污染）
     */
    void RestoreSimulatedTransforms();
    
    /**
     * @brief 缓存当前物理解算结果，供下一帧恢复
     */
    void CacheSimulatedTransforms();
    
    /**
     * @brief 处理碰撞结果的占位（后续实现）
     */
    void ResolveCollisions(float dt);
    
    /**
     * @brief 求解约束的占位（后续实现）
     */
    void SolveConstraints(float dt);
    
    /**
     * @brief 休眠检测的占位（后续实现）
     */
    void UpdateSleepingState(float dt);
    
    /**
     * @brief 应用插值（用于测试和直接调用 Update 的场景）
     */
    void ApplyInterpolation();
    
    Vector3 m_gravity = Vector3(0.0f, -9.81f, 0.0f);  // 全局重力
    float m_fixedDeltaTime = 1.0f / 60.0f;            // 固定时间步长
    float m_accumulator = 0.0f;                       // 时间累积器
    float m_physicsTime = 0.0f;                       // 物理时间
    SymplecticEulerIntegrator m_integrator;           // 积分器
    std::unordered_map<ECS::EntityID, SimulatedTransformState, ECS::EntityID::Hash> m_simulatedTransforms; // 物理解算缓存
    ConstraintSolver m_constraintSolver;             // 约束求解器
    int m_solverIterations = 10;
    int m_positionIterations = 4;
    PhysicsConfig m_config;                           // 物理配置（包含 CCD 配置）
};

} // namespace Physics
} // namespace Render

