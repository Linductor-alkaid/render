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

#include "render/physics/physics_config.h"
#include "render/ecs/entity.h"
#include "render/physics/bullet_adapter/bullet_contact_callback.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// 前向声明
class btDiscreteDynamicsWorld;
class btBroadphaseInterface;
class btCollisionDispatcher;
class btConstraintSolver;
class btCollisionConfiguration;
class btRigidBody;
class btCollisionShape;

// 前向声明组件（避免循环依赖）
namespace Render::Physics {
struct RigidBodyComponent;
struct ColliderComponent;
}

namespace Render::Application {
class EventBus;
}

namespace Render::Physics::BulletAdapter {

/**
 * @brief Bullet 世界适配器
 * 
 * 封装 btDiscreteDynamicsWorld，提供与 PhysicsWorld 相同的接口
 */
class BulletWorldAdapter {
public:
    /**
     * @brief 构造函数
     * @param config 物理配置
     */
    explicit BulletWorldAdapter(const PhysicsConfig& config);
    
    /**
     * @brief 析构函数
     */
    ~BulletWorldAdapter();
    
    // 禁止拷贝
    BulletWorldAdapter(const BulletWorldAdapter&) = delete;
    BulletWorldAdapter& operator=(const BulletWorldAdapter&) = delete;
    
    /**
     * @brief 执行物理步进
     * @param deltaTime 时间步长
     */
    void Step(float deltaTime);
    
    /**
     * @brief 设置重力
     * @param gravity 重力向量
     */
    void SetGravity(const Vector3& gravity);
    
    /**
     * @brief 获取重力
     */
    Vector3 GetGravity() const;
    
    /**
     * @brief 获取 Bullet 世界指针（用于高级操作）
     */
    btDiscreteDynamicsWorld* GetBulletWorld() const { return m_bulletWorld.get(); }
    
    /**
     * @brief 同步配置到 Bullet 世界
     * @param config 物理配置
     */
    void SyncConfig(const PhysicsConfig& config);
    
    // ==================== 2.1.4 实体到刚体映射 ====================
    
    /**
     * @brief 添加实体到刚体的映射
     * @param entity 实体 ID
     * @param rigidBody Bullet 刚体指针
     */
    void AddRigidBodyMapping(ECS::EntityID entity, btRigidBody* rigidBody);
    
    /**
     * @brief 移除实体到刚体的映射（通过实体 ID）
     * @param entity 实体 ID
     */
    void RemoveRigidBodyMapping(ECS::EntityID entity);
    
    /**
     * @brief 移除实体到刚体的映射（通过刚体指针）
     * @param rigidBody Bullet 刚体指针
     */
    void RemoveRigidBodyMapping(btRigidBody* rigidBody);
    
    /**
     * @brief 根据实体 ID 获取刚体指针
     * @param entity 实体 ID
     * @return Bullet 刚体指针，如果不存在则返回 nullptr
     */
    btRigidBody* GetRigidBody(ECS::EntityID entity) const;
    
    /**
     * @brief 根据刚体指针获取实体 ID
     * @param rigidBody Bullet 刚体指针
     * @return 实体 ID，如果不存在则返回 Invalid()
     */
    ECS::EntityID GetEntity(btRigidBody* rigidBody) const;
    
    // ==================== 2.2 实体管理 ====================
    
    /**
     * @brief 添加刚体到物理世界
     * @param entity 实体 ID
     * @param rigidBody 刚体组件
     * @param collider 碰撞体组件
     * @return 是否成功添加
     */
    bool AddRigidBody(ECS::EntityID entity, 
                      const RigidBodyComponent& rigidBody,
                      const ColliderComponent& collider);
    
    /**
     * @brief 从物理世界移除刚体
     * @param entity 实体 ID
     * @return 是否成功移除
     */
    bool RemoveRigidBody(ECS::EntityID entity);
    
    /**
     * @brief 更新刚体（检测组件变化并同步）
     * @param entity 实体 ID
     * @param rigidBody 刚体组件
     * @param collider 碰撞体组件
     * @return 是否成功更新
     */
    bool UpdateRigidBody(ECS::EntityID entity,
                         const RigidBodyComponent& rigidBody,
                         const ColliderComponent& collider);

    // ==================== 2.3 碰撞检测集成 ====================
    
    /**
     * @brief 设置事件总线（用于发送碰撞事件）
     * @param eventBus 事件总线指针
     */
    void SetEventBus(Application::EventBus* eventBus);
    
    /**
     * @brief 获取当前帧的碰撞对
     * @return 碰撞对列表
     */
    const std::vector<BulletContactCallback::CollisionPair>& GetCollisionPairs() const;

private:
    // ==================== 2.3.3 碰撞事件回调 ====================
    
    /**
     * @brief 收集碰撞信息
     */
    void CollectCollisions();
    
    /**
     * @brief 发送碰撞事件（Enter/Stay/Exit）
     */
    void SendCollisionEvents();
    
    /**
     * @brief 计算碰撞对的哈希值
     */
    static uint64_t HashPair(ECS::EntityID a, ECS::EntityID b);

private:
    std::unique_ptr<btDiscreteDynamicsWorld> m_bulletWorld;
    std::unique_ptr<btBroadphaseInterface> m_broadphase;
    std::unique_ptr<btCollisionDispatcher> m_dispatcher;
    std::unique_ptr<btConstraintSolver> m_solver;
    std::unique_ptr<btCollisionConfiguration> m_collisionConfig;
    
    // 实体到 Bullet 刚体的映射
    std::unordered_map<ECS::EntityID, btRigidBody*, ECS::EntityID::Hash> m_entityToRigidBody;
    std::unordered_map<btRigidBody*, ECS::EntityID> m_rigidBodyToEntity;
    
    // 实体到形状的映射（用于管理形状生命周期）
    std::unordered_map<ECS::EntityID, btCollisionShape*, ECS::EntityID::Hash> m_entityToShape; // 实体到形状的映射
    std::unordered_map<btCollisionShape*, std::unordered_set<ECS::EntityID, ECS::EntityID::Hash>> m_shapeToEntities; // 形状到实体的反向映射（用于跟踪共享）
    
    // 保存配置（用于 Step() 方法中的固定时间步长等参数）
    PhysicsConfig m_config;
    
    // ==================== 2.3 碰撞检测集成 ====================
    
    // 事件总线（用于发送碰撞事件）
    Application::EventBus* m_eventBus = nullptr;
    
    // 碰撞回调对象
    std::unique_ptr<BulletContactCallback> m_contactCallback;
    
    // 当前帧的碰撞对
    std::vector<BulletContactCallback::CollisionPair> m_currentCollisionPairs;
    
    // 上一帧的碰撞对（用于检测 Enter/Stay/Exit）
    // 使用完整的碰撞对信息，而不仅仅是哈希值，以便在 Exit 时能够恢复实体ID
    std::vector<BulletContactCallback::CollisionPair> m_previousCollisionPairs;
};

} // namespace Render::Physics::BulletAdapter

