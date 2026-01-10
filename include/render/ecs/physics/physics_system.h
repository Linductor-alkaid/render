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
#include "render/types.h"
#include <vector>
#include <unordered_map>
#include <set>
#include <utility>

namespace Render {
namespace ECS {

// 前向声明
class World;

/**
 * @brief 物理系统
 * 
 * 管理物理模拟，同步 Transform 和物理体状态
 * 优先级：15（在 TransformSystem 之后）
 */
class PhysicsSystem : public System {
public:
    PhysicsSystem();
    ~PhysicsSystem() override;
    
    void OnCreate(World* world) override;
    void OnDestroy() override;
    void Update(float deltaTime) override;
    [[nodiscard]] int GetPriority() const override { return 15; }
    
    // ==================== 物理世界管理 ====================
    
    /**
     * @brief 获取物理世界组件对应的实体
     * @return 物理世界实体ID，如果不存在返回 Invalid
     */
    [[nodiscard]] EntityID GetPhysicsWorldEntity() const;
    
    /**
     * @brief 设置物理世界实体
     * @param entity 物理世界实体ID
     * @return 成功返回 true
     */
    bool SetPhysicsWorldEntity(EntityID entity);
    
    /**
     * @brief 创建物理世界实体
     * @return 新创建的物理世界实体ID
     */
    EntityID CreatePhysicsWorld();
    
    // ==================== 物理模拟控制 ====================
    
    /**
     * @brief 启用/禁用物理模拟
     * @param enabled 是否启用
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    
    /**
     * @brief 获取是否启用
     */
    [[nodiscard]] bool IsEnabled() const { return m_enabled; }
    
    /**
     * @brief 设置重力
     * @param gravity 重力向量
     */
    void SetGravity(const Vector3& gravity);
    
    /**
     * @brief 获取重力
     */
    [[nodiscard]] Vector3 GetGravity() const;
    
    // ==================== 查询接口 ====================
    
    /**
     * @brief 射线检测
     * @param start 起点
     * @param end 终点
     * @return 碰撞结果列表
     */
    struct RaycastHit {
        EntityID entity;                ///< 碰撞的实体
        Vector3 point;                  ///< 碰撞点
        Vector3 normal;                 ///< 碰撞法线
        float distance;                 ///< 距离
    };
    
    std::vector<RaycastHit> Raycast(const Vector3& start, const Vector3& end) const;
    
    /**
     * @brief 球形检测
     * @param center 中心点
     * @param radius 半径
     * @return 碰撞的实体列表
     */
    std::vector<EntityID> SphereCast(const Vector3& center, float radius) const;
    
    // ==================== 统计信息 ====================
    
    struct PhysicsStats {
        size_t rigidBodyCount = 0;     ///< 刚体数量
        size_t colliderCount = 0;       ///< 碰撞体数量
        size_t constraintCount = 0;      ///< 约束数量
        float simulationTime = 0.0f;    ///< 模拟时间（ms）
        int stepCount = 0;               ///< 步数
    };
    
    [[nodiscard]] const PhysicsStats& GetStats() const { return m_stats; }
    
private:
    // ==================== 初始化/清理 ====================
    void InitializePhysicsWorld(EntityID entity);
    void ShutdownPhysicsWorld();
    
    // ==================== 组件同步 ====================
    void SyncTransformToPhysics(EntityID entity);
    void SyncPhysicsToTransform(EntityID entity);
    void UpdateCollider(EntityID entity);
    void UpdateConstraints();
    
    // ==================== 批量处理 ====================
    void BatchSyncTransformsToPhysics();
    void BatchSyncPhysicsToTransforms();
    
    // ==================== 物理体管理 ====================
    void CreateRigidBody(EntityID entity);
    void DestroyRigidBody(EntityID entity);
    void CreateCollider(EntityID entity);
    void DestroyCollider(EntityID entity);
    
    // ==================== 约束管理 ====================
    void CreateConstraint(EntityID entity);
    void DestroyConstraint(EntityID entity);
    
    // ==================== 碰撞检测 ====================
    void DetectCollisionsAndTriggers();
    
    // ==================== 碰撞回调 ====================
    void OnCollisionEnter(EntityID entityA, EntityID entityB, const Vector3& point, const Vector3& normal);
    void OnCollisionExit(EntityID entityA, EntityID entityB);
    void OnTriggerEnter(EntityID entityA, EntityID entityB);
    void OnTriggerExit(EntityID entityA, EntityID entityB);
    
    // ==================== 成员变量 ====================
    EntityID m_physicsWorldEntity = EntityID::Invalid();  ///< 物理世界实体ID
    bool m_enabled = true;                                 ///< 是否启用
    PhysicsStats m_stats;                                  ///< 统计信息
    
    // 实体ID映射（用于从Bullet对象查找EntityID）
    std::unordered_map<void*, EntityID> m_rigidBodyToEntity;  ///< Bullet刚体指针到EntityID的映射
    std::unordered_map<EntityID, void*, EntityID::Hash> m_entityToRigidBody;  ///< EntityID到Bullet刚体指针的映射
    
    // 碰撞状态跟踪（用于检测Enter/Exit）
    std::set<std::pair<EntityID, EntityID>> m_currentCollisions;  ///< 当前帧的碰撞对
    std::set<std::pair<EntityID, EntityID>> m_previousCollisions;  ///< 上一帧的碰撞对
    std::set<std::pair<EntityID, EntityID>> m_currentTriggers;  ///< 当前帧的触发器重叠对
    std::set<std::pair<EntityID, EntityID>> m_previousTriggers;  ///< 上一帧的触发器重叠对
    
    // Bullet 对象（通过 void* 存储，避免暴露 Bullet 头文件）
    void* m_bulletWorld = nullptr;                         ///< btDiscreteDynamicsWorld
    void* m_broadphase = nullptr;                          ///< btBroadphaseInterface
    void* m_dispatcher = nullptr;                          ///< btCollisionDispatcher
    void* m_solver = nullptr;                              ///< btConstraintSolver
    void* m_collisionConfig = nullptr;                     ///< btDefaultCollisionConfiguration
};

} // namespace ECS
} // namespace Render
