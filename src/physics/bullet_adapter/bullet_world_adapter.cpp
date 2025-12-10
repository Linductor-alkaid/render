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
#include "render/physics/bullet_adapter/bullet_world_adapter.h"
#include "render/physics/bullet_adapter/eigen_to_bullet.h"
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>

namespace Render::Physics::BulletAdapter {

BulletWorldAdapter::BulletWorldAdapter(const PhysicsConfig& config) {
    // ==================== 2.1.1 世界初始化 ====================
    
    // 创建碰撞配置（使用默认配置）
    m_collisionConfig = std::make_unique<btDefaultCollisionConfiguration>();
    
    // 创建碰撞分发器
    m_dispatcher = std::make_unique<btCollisionDispatcher>(m_collisionConfig.get());
    
    // 创建粗检测（使用 DBVT 算法，性能较好）
    m_broadphase = std::make_unique<btDbvtBroadphase>();
    
    // 创建约束求解器（使用序列冲量法）
    m_solver = std::make_unique<btSequentialImpulseConstraintSolver>();
    
    // 创建物理世界
    m_bulletWorld = std::make_unique<btDiscreteDynamicsWorld>(
        m_dispatcher.get(),
        m_broadphase.get(),
        m_solver.get(),
        m_collisionConfig.get()
    );
    
    // ==================== 2.1.2 世界配置同步 ====================
    SyncConfig(config);
}

BulletWorldAdapter::~BulletWorldAdapter() {
    // ==================== 清理 ====================
    // 注意：需要先移除所有刚体，然后才能销毁世界
    // 因为 Bullet 会在析构时检查是否有残留的刚体
    
    // 清除所有映射（刚体会在世界析构时自动清理）
    m_entityToRigidBody.clear();
    m_rigidBodyToEntity.clear();
    
    // 智能指针会自动清理，但需要按顺序销毁
    // 先销毁世界（会清理所有刚体）
    m_bulletWorld.reset();
    // 然后销毁其他组件
    m_solver.reset();
    m_broadphase.reset();
    m_dispatcher.reset();
    m_collisionConfig.reset();
}

void BulletWorldAdapter::Step(float deltaTime) {
    // ==================== 2.1.3 Step() 方法 ====================
    if (!m_bulletWorld) {
        return;
    }
    
    // Bullet 的 stepSimulation 支持固定时间步长和最大子步数
    // 参数说明：
    // - deltaTime: 实际时间步长
    // - maxSubSteps: 最大子步数（防止螺旋死亡）
    // - fixedTimeStep: 固定时间步长（用于稳定物理模拟）
    
    // 从配置中获取固定时间步长和最大子步数
    float fixedTimeStep = m_config.fixedDeltaTime;
    int maxSubSteps = m_config.maxSubSteps;
    
    // 执行物理步进
    // Bullet 会自动将 deltaTime 分割为多个 fixedTimeStep 子步
    // 例如：如果 deltaTime = 0.05s, fixedTimeStep = 0.016s, maxSubSteps = 5
    // Bullet 会执行 3 个子步（0.016 + 0.016 + 0.018），剩余时间会累积到下一帧
    m_bulletWorld->stepSimulation(deltaTime, maxSubSteps, fixedTimeStep);
}

void BulletWorldAdapter::SetGravity(const Vector3& gravity) {
    if (m_bulletWorld) {
        m_bulletWorld->setGravity(ToBullet(gravity));
    }
}

Vector3 BulletWorldAdapter::GetGravity() const {
    if (m_bulletWorld) {
        return FromBullet(m_bulletWorld->getGravity());
    }
    return Vector3::Zero();
}

void BulletWorldAdapter::SyncConfig(const PhysicsConfig& config) {
    // ==================== 2.1.2 世界配置同步 ====================
    if (!m_bulletWorld) {
        return;
    }
    
    // 同步重力
    m_bulletWorld->setGravity(ToBullet(config.gravity));
    
    // 同步求解器迭代次数
    // Bullet 使用 btContactSolverInfo 来配置求解器参数
    btContactSolverInfo& solverInfo = m_bulletWorld->getSolverInfo();
    solverInfo.m_numIterations = config.solverIterations;
    
    // 注意：Bullet 的 positionIterations 和 velocityIterations 是分开的
    // 但我们的配置中只有 solverIterations 和 positionIterations
    // Bullet 使用 m_numIterations 作为速度迭代次数
    // 位置迭代次数通过 m_numIterations 和 m_splitImpulse 控制
    // 这里简化处理：将 positionIterations 作为额外的位置修正迭代
    // 实际上 Bullet 的位置迭代是通过 split impulse 实现的
    if (config.positionIterations > 0) {
        solverInfo.m_splitImpulse = true;
        solverInfo.m_numIterations = config.solverIterations;
        // Bullet 的位置修正迭代次数通常等于速度迭代次数
        // 如果需要更精确的控制，可以调整其他参数
    }
    
    // 同步 CCD 设置
    // Bullet 的 CCD 通过 getDispatchInfo() 配置
    btDispatcherInfo& dispatchInfo = m_bulletWorld->getDispatchInfo();
    dispatchInfo.m_useContinuous = config.enableCCD;
    
    // 同步休眠系统
    // Bullet 的休眠系统通过全局设置控制
    // 注意：Bullet 的休眠是自动的，我们只需要确保它被启用
    // 实际上，Bullet 默认启用休眠，这里主要是确保配置一致
    // 可以通过设置每个刚体的休眠阈值来控制（在刚体适配器中处理）
    
    // 保存配置以便 Step() 方法使用
    m_config = config;
}

void BulletWorldAdapter::AddRigidBodyMapping(ECS::EntityID entity, btRigidBody* rigidBody) {
    // ==================== 2.1.4 实体到刚体映射 ====================
    if (!rigidBody || !entity.IsValid()) {
        return;
    }
    
    // 检查是否已存在映射
    auto it = m_entityToRigidBody.find(entity);
    if (it != m_entityToRigidBody.end()) {
        // 如果已存在，先移除旧的映射
        btRigidBody* oldBody = it->second;
        if (oldBody != rigidBody) {
            m_rigidBodyToEntity.erase(oldBody);
        }
    }
    
    // 添加双向映射
    m_entityToRigidBody[entity] = rigidBody;
    m_rigidBodyToEntity[rigidBody] = entity;
}

void BulletWorldAdapter::RemoveRigidBodyMapping(ECS::EntityID entity) {
    // ==================== 2.1.4 实体到刚体映射 ====================
    auto it = m_entityToRigidBody.find(entity);
    if (it != m_entityToRigidBody.end()) {
        btRigidBody* rigidBody = it->second;
        
        // 移除双向映射
        m_rigidBodyToEntity.erase(rigidBody);
        m_entityToRigidBody.erase(it);
    }
}

void BulletWorldAdapter::RemoveRigidBodyMapping(btRigidBody* rigidBody) {
    // ==================== 2.1.4 实体到刚体映射 ====================
    if (!rigidBody) {
        return;
    }
    
    auto it = m_rigidBodyToEntity.find(rigidBody);
    if (it != m_rigidBodyToEntity.end()) {
        ECS::EntityID entity = it->second;
        
        // 移除双向映射
        m_entityToRigidBody.erase(entity);
        m_rigidBodyToEntity.erase(it);
    }
}

btRigidBody* BulletWorldAdapter::GetRigidBody(ECS::EntityID entity) const {
    auto it = m_entityToRigidBody.find(entity);
    if (it != m_entityToRigidBody.end()) {
        return it->second;
    }
    return nullptr;
}

ECS::EntityID BulletWorldAdapter::GetEntity(btRigidBody* rigidBody) const {
    if (!rigidBody) {
        return ECS::EntityID::Invalid();
    }
    
    auto it = m_rigidBodyToEntity.find(rigidBody);
    if (it != m_rigidBodyToEntity.end()) {
        return it->second;
    }
    return ECS::EntityID::Invalid();
}

} // namespace Render::Physics::BulletAdapter

