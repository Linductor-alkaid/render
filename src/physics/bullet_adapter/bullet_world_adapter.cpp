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
#include "render/physics/bullet_adapter/bullet_shape_adapter.h"
#include "render/physics/bullet_adapter/bullet_rigid_body_adapter.h"
#include "render/physics/physics_components.h"
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <BulletCollision/CollisionShapes/btCollisionShape.h>
#include <vector>

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
    
    if (!m_bulletWorld) {
        // 如果世界已经不存在，直接清理映射
        m_entityToRigidBody.clear();
        m_rigidBodyToEntity.clear();
        m_entityToShape.clear();
        m_shapeToEntities.clear();
        return;
    }
    
    // 清理所有通过 AddRigidBody 添加的刚体和形状
    // 注意：只清理在 m_entityToShape 中的实体（即通过 AddRigidBody 添加的）
    // AddRigidBodyMapping 添加的映射不应该在这里自动删除，因为它们可能由外部管理
    // 创建实体列表的副本（因为移除会修改映射）
    std::vector<ECS::EntityID> entitiesToRemove;
    entitiesToRemove.reserve(m_entityToShape.size());
    for (const auto& pair : m_entityToShape) {
        entitiesToRemove.push_back(pair.first);
    }
    
    // 移除所有通过 AddRigidBody 添加的刚体
    for (ECS::EntityID entity : entitiesToRemove) {
        // 检查刚体是否存在
        auto it = m_entityToRigidBody.find(entity);
        if (it == m_entityToRigidBody.end()) {
            continue;  // 已经被移除
        }
        
        btRigidBody* rigidBody = it->second;
        if (!rigidBody) {
            continue;  // 无效指针
        }
        
        // 先移除映射（在删除刚体之前）
        m_rigidBodyToEntity.erase(rigidBody);
        m_entityToRigidBody.erase(it);
        
        // 从物理世界移除（如果刚体在世界中）
        // 注意：Bullet 的 removeRigidBody 可以安全处理不在世界中的刚体
        try {
            if (m_bulletWorld) {
                m_bulletWorld->removeRigidBody(rigidBody);
            }
        } catch (...) {
            // 忽略异常（刚体可能不在世界中）
        }
        
        // 清理形状（检查是否被共享）
        auto shapeIt = m_entityToShape.find(entity);
        if (shapeIt != m_entityToShape.end()) {
            btCollisionShape* shape = shapeIt->second;
            if (shape) {
                // 从形状到实体的反向映射中移除该实体
                auto shapeToEntitiesIt = m_shapeToEntities.find(shape);
                if (shapeToEntitiesIt != m_shapeToEntities.end()) {
                    shapeToEntitiesIt->second.erase(entity);
                    // 如果该形状不再被任何实体使用，才删除它
                    if (shapeToEntitiesIt->second.empty()) {
                        BulletShapeAdapter::DestroyShape(shape);
                        m_shapeToEntities.erase(shapeToEntitiesIt);
                    }
                    // 否则，形状被其他实体共享，不删除
                } else {
                    // 如果不在反向映射中（不应该发生），使用 DestroyShape 的安全删除
                    BulletShapeAdapter::DestroyShape(shape);
                }
            }
            m_entityToShape.erase(shapeIt);
        }
        
        // 最后删除刚体
        delete rigidBody;
    }
    
    // 清除所有映射（应该已经为空）
    m_entityToRigidBody.clear();
    m_rigidBodyToEntity.clear();
    m_entityToShape.clear();
    m_shapeToEntities.clear();
    
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

bool BulletWorldAdapter::AddRigidBody(ECS::EntityID entity,
                                      const RigidBodyComponent& rigidBody,
                                      const ColliderComponent& collider) {
    // ==================== 2.2.1 实现刚体添加 ====================
    if (!m_bulletWorld || !entity.IsValid()) {
        return false;
    }
    
    // 检查是否已存在
    if (m_entityToRigidBody.find(entity) != m_entityToRigidBody.end()) {
        // 如果已存在，先移除旧的
        RemoveRigidBody(entity);
    }
    
    // 1. 创建碰撞形状
    btCollisionShape* shape = BulletShapeAdapter::CreateShape(collider);
    if (!shape) {
        return false;
    }
    
    // 2. 计算局部惯性
    btVector3 localInertia(0.0f, 0.0f, 0.0f);
    float mass = 0.0f;
    if (rigidBody.type == RigidBodyComponent::BodyType::Dynamic && rigidBody.mass > 0.0f) {
        mass = rigidBody.mass;
        // 从惯性张量提取对角线元素
        localInertia.setX(rigidBody.inertiaTensor(0, 0));
        localInertia.setY(rigidBody.inertiaTensor(1, 1));
        localInertia.setZ(rigidBody.inertiaTensor(2, 2));
        
        // 如果惯性张量无效，从形状计算
        if (localInertia.length2() < 1e-6f) {
            shape->calculateLocalInertia(mass, localInertia);
        }
    }
    
    // 3. 创建刚体构造信息
    btRigidBody::btRigidBodyConstructionInfo rbInfo(
        mass,
        nullptr,  // motionState（将在同步时设置变换）
        shape,
        localInertia
    );
    
    // 4. 创建 Bullet 刚体
    btRigidBody* bulletBody = new btRigidBody(rbInfo);
    if (!bulletBody) {
        BulletShapeAdapter::DestroyShape(shape);
        return false;
    }
    
    // 5. 同步刚体属性（使用适配器）
    BulletRigidBodyAdapter adapter(bulletBody, entity);
    adapter.SyncToBullet(rigidBody);
    
    // 6. 设置材质属性（摩擦和弹性）
    if (collider.material) {
        bulletBody->setFriction(collider.material->friction);
        bulletBody->setRestitution(collider.material->restitution);
    }
    
    // 7. 设置碰撞层和掩码
    if (bulletBody->getBroadphaseHandle()) {
        bulletBody->getBroadphaseHandle()->m_collisionFilterGroup = collider.collisionLayer;
        bulletBody->getBroadphaseHandle()->m_collisionFilterMask = collider.collisionMask;
    }
    
    // 8. 设置触发器标志
    if (collider.isTrigger) {
        int flags = bulletBody->getCollisionFlags();
        flags |= btCollisionObject::CF_NO_CONTACT_RESPONSE;
        bulletBody->setCollisionFlags(flags);
    }
    
    // 9. 添加到物理世界
    m_bulletWorld->addRigidBody(bulletBody);
    
    // 10. 更新映射
    AddRigidBodyMapping(entity, bulletBody);
    m_entityToShape[entity] = shape;
    
    // 更新形状到实体的反向映射（用于跟踪共享）
    m_shapeToEntities[shape].insert(entity);
    
    return true;
}

bool BulletWorldAdapter::RemoveRigidBody(ECS::EntityID entity) {
    // ==================== 2.2.2 实现刚体移除 ====================
    if (!m_bulletWorld || !entity.IsValid()) {
        return false;
    }
    
    // 查找刚体
    auto it = m_entityToRigidBody.find(entity);
    if (it == m_entityToRigidBody.end()) {
        return false;  // 不存在
    }
    
    btRigidBody* rigidBody = it->second;
    
    // 1. 从物理世界移除（如果刚体在世界中）
    // 注意：检查刚体是否在世界中，避免移除不存在的刚体导致问题
    // Bullet 的 removeRigidBody 可以安全处理不在世界中的刚体，但为了明确性，我们检查
    try {
        // 尝试移除刚体（如果不在世界中，Bullet 会安全处理）
        m_bulletWorld->removeRigidBody(rigidBody);
    } catch (...) {
        // 忽略异常（刚体可能不在世界中）
    }
    
    // 2. 获取并释放形状（检查是否被共享）
    auto shapeIt = m_entityToShape.find(entity);
    if (shapeIt != m_entityToShape.end()) {
        btCollisionShape* shape = shapeIt->second;
        
        // 从形状到实体的反向映射中移除该实体
        auto shapeToEntitiesIt = m_shapeToEntities.find(shape);
        if (shapeToEntitiesIt != m_shapeToEntities.end()) {
            shapeToEntitiesIt->second.erase(entity);
            
            // 如果该形状不再被任何实体使用，才删除它
            if (shapeToEntitiesIt->second.empty()) {
                BulletShapeAdapter::DestroyShape(shape);
                m_shapeToEntities.erase(shapeToEntitiesIt);
            }
            // 否则，形状被其他实体共享，不删除
        } else {
            // 如果不在反向映射中（不应该发生），使用 DestroyShape 的安全删除
            BulletShapeAdapter::DestroyShape(shape);
        }
        
        m_entityToShape.erase(shapeIt);
    }
    
    // 3. 释放刚体
    delete rigidBody;
    
    // 4. 清理映射
    RemoveRigidBodyMapping(entity);
    
    return true;
}

bool BulletWorldAdapter::UpdateRigidBody(ECS::EntityID entity,
                                         const RigidBodyComponent& rigidBody,
                                         const ColliderComponent& collider) {
    // ==================== 2.2.3 实现刚体更新检测 ====================
    if (!m_bulletWorld || !entity.IsValid()) {
        return false;
    }
    
    // 查找刚体
    auto it = m_entityToRigidBody.find(entity);
    if (it == m_entityToRigidBody.end()) {
        // 如果不存在，尝试添加
        return AddRigidBody(entity, rigidBody, collider);
    }
    
    btRigidBody* bulletBody = it->second;
    
    // 1. 检查形状是否需要更新
    auto shapeIt = m_entityToShape.find(entity);
    btCollisionShape* currentShape = (shapeIt != m_entityToShape.end()) ? shapeIt->second : nullptr;
    
    bool shapeChanged = false;
    if (!currentShape || BulletShapeAdapter::NeedsShapeUpdate(currentShape, collider)) {
        // 形状需要更新，创建新形状
        btCollisionShape* newShape = BulletShapeAdapter::UpdateShape(currentShape, collider);
        if (newShape && newShape != currentShape) {
            // 形状已改变，需要替换
            shapeChanged = true;
            
            // 保存旧形状
            btCollisionShape* oldShape = currentShape;
            
            // 更新形状引用
            m_entityToShape[entity] = newShape;
            
            // 更新刚体的形状
            bulletBody->setCollisionShape(newShape);
            
            // 重新计算惯性（如果质量改变）
            if (rigidBody.type == RigidBodyComponent::BodyType::Dynamic && rigidBody.mass > 0.0f) {
                btVector3 localInertia(0.0f, 0.0f, 0.0f);
                btVector3 localInertiaFromTensor(
                    rigidBody.inertiaTensor(0, 0),
                    rigidBody.inertiaTensor(1, 1),
                    rigidBody.inertiaTensor(2, 2)
                );
                
                if (localInertiaFromTensor.length2() < 1e-6f) {
                    newShape->calculateLocalInertia(rigidBody.mass, localInertia);
                } else {
                    localInertia = localInertiaFromTensor;
                }
                
                bulletBody->setMassProps(rigidBody.mass, localInertia);
            }
            
            // 释放旧形状（如果不同，并检查是否被共享）
            if (oldShape && oldShape != newShape) {
                // 从形状到实体的反向映射中移除该实体
                auto oldShapeIt = m_shapeToEntities.find(oldShape);
                if (oldShapeIt != m_shapeToEntities.end()) {
                    oldShapeIt->second.erase(entity);
                    // 如果旧形状不再被任何实体使用，删除它
                    if (oldShapeIt->second.empty()) {
                        BulletShapeAdapter::DestroyShape(oldShape);
                        m_shapeToEntities.erase(oldShapeIt);
                    }
                    // 否则，形状被其他实体共享，不删除
                } else {
                    // 如果不在反向映射中（不应该发生），使用 DestroyShape 的安全删除
                    BulletShapeAdapter::DestroyShape(oldShape);
                }
            }
            
            // 更新新形状到实体的反向映射
            if (newShape) {
                m_shapeToEntities[newShape].insert(entity);
            }
        }
    }
    
    // 2. 同步刚体属性
    BulletRigidBodyAdapter adapter(bulletBody, entity);
    adapter.SyncToBullet(rigidBody);
    
    // 3. 更新材质属性
    if (collider.material) {
        bulletBody->setFriction(collider.material->friction);
        bulletBody->setRestitution(collider.material->restitution);
    }
    
    // 4. 更新碰撞层和掩码
    if (bulletBody->getBroadphaseHandle()) {
        bulletBody->getBroadphaseHandle()->m_collisionFilterGroup = collider.collisionLayer;
        bulletBody->getBroadphaseHandle()->m_collisionFilterMask = collider.collisionMask;
    }
    
    // 5. 更新触发器标志
    int flags = bulletBody->getCollisionFlags();
    if (collider.isTrigger) {
        flags |= btCollisionObject::CF_NO_CONTACT_RESPONSE;
    } else {
        flags &= ~btCollisionObject::CF_NO_CONTACT_RESPONSE;
    }
    bulletBody->setCollisionFlags(flags);
    
    return true;
}

} // namespace Render::Physics::BulletAdapter

