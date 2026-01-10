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

#include "render/ecs/physics/physics_system.h"
#include "render/ecs/physics/physics_components.h"
#include "render/ecs/components.h"
#include "render/ecs/world.h"
#include "render/logger.h"
#include "render/types.h"

// Bullet 头文件
#include <btBulletDynamicsCommon.h>
#include <LinearMath/btVector3.h>
#include <LinearMath/btQuaternion.h>
#include <LinearMath/btTransform.h>
#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btSphereShape.h>
#include <BulletCollision/CollisionShapes/btStaticPlaneShape.h>
#include <BulletCollision/CollisionShapes/btCapsuleShape.h>
#include <BulletCollision/CollisionShapes/btCylinderShape.h>
#include <BulletCollision/CollisionShapes/btConeShape.h>
#include <BulletCollision/CollisionShapes/btConvexHullShape.h>
#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btTriangleIndexVertexArray.h>
#include <BulletCollision/CollisionShapes/btCompoundShape.h>
#include <BulletDynamics/ConstraintSolver/btPoint2PointConstraint.h>
#include <BulletDynamics/ConstraintSolver/btHingeConstraint.h>
#include <BulletDynamics/ConstraintSolver/btGeneric6DofConstraint.h>
#include "render/resource_manager.h"

namespace Render {
namespace ECS {

// ============================================================
// 类型转换辅助函数
// ============================================================

namespace {
// Vector3 转换
inline btVector3 ToBullet(const Vector3& v) {
    return btVector3(v.x(), v.y(), v.z());
}

inline Vector3 FromBullet(const btVector3& v) {
    return Vector3(v.x(), v.y(), v.z());
}

// Quaternion 转换（注意：Eigen 顺序为 (w, x, y, z)，Bullet 为 (x, y, z, w)）
inline btQuaternion ToBullet(const Quaternion& q) {
    return btQuaternion(q.x(), q.y(), q.z(), q.w());
}

inline Quaternion FromBullet(const btQuaternion& q) {
    return Quaternion(q.w(), q.x(), q.y(), q.z());
}

// Transform 转换
inline btTransform ToBullet(const Vector3& pos, const Quaternion& rot) {
    btTransform transform;
    transform.setOrigin(ToBullet(pos));
    transform.setRotation(ToBullet(rot));
    return transform;
}

inline void FromBullet(const btTransform& transform, Vector3& pos, Quaternion& rot) {
    pos = FromBullet(transform.getOrigin());
    rot = FromBullet(transform.getRotation());
}
} // namespace

// ============================================================
// PhysicsSystem 实现
// ============================================================

PhysicsSystem::PhysicsSystem() = default;

PhysicsSystem::~PhysicsSystem() {
    ShutdownPhysicsWorld();
}

void PhysicsSystem::OnCreate(World* world) {
    System::OnCreate(world);
    Logger::GetInstance().Info("[PhysicsSystem] Created");
}

void PhysicsSystem::OnDestroy() {
    ShutdownPhysicsWorld();
    Logger::GetInstance().Info("[PhysicsSystem] Destroyed");
}

void PhysicsSystem::Update(float deltaTime) {
    if (!m_world || !m_enabled) return;
    
    // 获取物理世界实体
    if (!m_physicsWorldEntity.IsValid()) {
        // 尝试查找现有的物理世界实体
        auto entities = m_world->Query<PhysicsWorldComponent>();
        if (!entities.empty()) {
            m_physicsWorldEntity = entities[0];
            InitializePhysicsWorld(m_physicsWorldEntity);
        } else {
            return;  // 没有物理世界，不执行模拟
        }
    }
    
    // 检查物理世界是否有效
    if (!m_world->HasComponent<PhysicsWorldComponent>(m_physicsWorldEntity)) {
        return;
    }
    
    auto& physicsWorld = m_world->GetComponent<PhysicsWorldComponent>(m_physicsWorldEntity);
    if (!physicsWorld.enabled || !physicsWorld.bulletWorld) {
        return;
    }
    
    btDiscreteDynamicsWorld* bulletWorld = static_cast<btDiscreteDynamicsWorld*>(physicsWorld.bulletWorld);
    
    // 1. 检测并创建新的物理体
    auto rigidBodyEntities = m_world->Query<RigidBodyComponent>();
    for (const auto& entity : rigidBodyEntities) {
        if (!m_world->HasComponent<ColliderComponent>(entity)) {
            continue;
        }
        
        auto& rb = m_world->GetComponent<RigidBodyComponent>(entity);
        auto& collider = m_world->GetComponent<ColliderComponent>(entity);
        
        // 如果还没有创建 Bullet 对象，则创建
        if (!rb.bulletRigidBody && !collider.bulletCollisionShape) {
            CreateRigidBody(entity);
        }
    }
    
    // 2. 更新碰撞体（如果需要）
    auto colliderEntities = m_world->Query<ColliderComponent>();
    for (const auto& entity : colliderEntities) {
        auto& collider = m_world->GetComponent<ColliderComponent>(entity);
        if (collider.needsUpdate && collider.bulletCollisionShape) {
            UpdateCollider(entity);
            collider.needsUpdate = false;
        }
    }
    
    // 3. 批量同步 Transform → Physics（TransformToPhysics 模式）
    BatchSyncTransformsToPhysics();
    
    // 4. 执行物理模拟
    if (bulletWorld) {
        auto startTime = std::chrono::high_resolution_clock::now();
        int numSteps = bulletWorld->stepSimulation(deltaTime, physicsWorld.maxSubSteps, physicsWorld.timeStep);
        auto endTime = std::chrono::high_resolution_clock::now();
        
        m_stats.simulationTime = std::chrono::duration<float, std::milli>(endTime - startTime).count();
        m_stats.stepCount = numSteps;
    }
    
    // 5. 批量同步 Physics → Transform（PhysicsToTransform 模式）
    BatchSyncPhysicsToTransforms();
    
    // 6. 更新约束
    UpdateConstraints();
    
    // 7. 检测碰撞和触发器
    DetectCollisionsAndTriggers();
    
    // 8. 更新统计信息
    m_stats.rigidBodyCount = rigidBodyEntities.size();
    m_stats.colliderCount = colliderEntities.size();
    
    auto constraintEntities = m_world->Query<ConstraintComponent>();
    m_stats.constraintCount = constraintEntities.size();
}

EntityID PhysicsSystem::GetPhysicsWorldEntity() const {
    return m_physicsWorldEntity;
}

bool PhysicsSystem::SetPhysicsWorldEntity(EntityID entity) {
    if (!m_world || !entity.IsValid()) {
        return false;
    }
    
    if (!m_world->HasComponent<PhysicsWorldComponent>(entity)) {
        Logger::GetInstance().Warning("[PhysicsSystem] Entity does not have PhysicsWorldComponent");
        return false;
    }
    
    // 关闭旧的物理世界（如果存在）
    if (m_physicsWorldEntity.IsValid() && m_physicsWorldEntity != entity) {
        ShutdownPhysicsWorld();
    }
    
    m_physicsWorldEntity = entity;
    InitializePhysicsWorld(entity);
    
    return true;
}

EntityID PhysicsSystem::CreatePhysicsWorld() {
    if (!m_world) {
        return EntityID::Invalid();
    }
    
    auto entity = m_world->CreateEntity();
    m_world->AddComponent<PhysicsWorldComponent>(entity, PhysicsWorldComponent{});
    
    m_physicsWorldEntity = entity;
    InitializePhysicsWorld(entity);
    
    Logger::GetInstance().InfoFormat("[PhysicsSystem] Created physics world entity: %u", entity.index);
    
    return entity;
}

void PhysicsSystem::SetGravity(const Vector3& gravity) {
    if (!m_physicsWorldEntity.IsValid() || !m_world) {
        return;
    }
    
    if (!m_world->HasComponent<PhysicsWorldComponent>(m_physicsWorldEntity)) {
        return;
    }
    
    auto& physicsWorld = m_world->GetComponent<PhysicsWorldComponent>(m_physicsWorldEntity);
    physicsWorld.gravity = gravity;
    
    if (physicsWorld.bulletWorld) {
        btDiscreteDynamicsWorld* bulletWorld = static_cast<btDiscreteDynamicsWorld*>(physicsWorld.bulletWorld);
        bulletWorld->setGravity(ToBullet(gravity));
    }
}

Vector3 PhysicsSystem::GetGravity() const {
    if (!m_physicsWorldEntity.IsValid() || !m_world) {
        return Vector3(0, -9.81f, 0);
    }
    
    if (!m_world->HasComponent<PhysicsWorldComponent>(m_physicsWorldEntity)) {
        return Vector3(0, -9.81f, 0);
    }
    
    const auto& physicsWorld = m_world->GetComponent<PhysicsWorldComponent>(m_physicsWorldEntity);
    return physicsWorld.gravity;
}

std::vector<PhysicsSystem::RaycastHit> PhysicsSystem::Raycast(const Vector3& start, const Vector3& end) const {
    std::vector<RaycastHit> hits;
    
    if (!m_physicsWorldEntity.IsValid() || !m_world) {
        return hits;
    }
    
    if (!m_world->HasComponent<PhysicsWorldComponent>(m_physicsWorldEntity)) {
        return hits;
    }
    
    const auto& physicsWorld = m_world->GetComponent<PhysicsWorldComponent>(m_physicsWorldEntity);
    if (!physicsWorld.bulletWorld) {
        return hits;
    }
    
    btDiscreteDynamicsWorld* bulletWorld = static_cast<btDiscreteDynamicsWorld*>(physicsWorld.bulletWorld);
    
    btVector3 btStart = ToBullet(start);
    btVector3 btEnd = ToBullet(end);
    
    btCollisionWorld::ClosestRayResultCallback rayCallback(btStart, btEnd);
    bulletWorld->rayTest(btStart, btEnd, rayCallback);
    
    if (rayCallback.hasHit()) {
        RaycastHit hit;
        hit.point = FromBullet(rayCallback.m_hitPointWorld);
        hit.normal = FromBullet(rayCallback.m_hitNormalWorld);
        hit.distance = (hit.point - start).norm();
        
        // 查找对应的实体（通过刚体指针）
        btRigidBody* hitBody = const_cast<btRigidBody*>(btRigidBody::upcast(rayCallback.m_collisionObject));
        if (hitBody) {
            // 通过映射表查找EntityID
            auto it = m_rigidBodyToEntity.find(hitBody);
            if (it != m_rigidBodyToEntity.end()) {
                hit.entity = it->second;
            } else {
                hit.entity = EntityID::Invalid();
            }
        } else {
            hit.entity = EntityID::Invalid();
        }
        
        hits.push_back(hit);
    }
    
    return hits;
}

std::vector<EntityID> PhysicsSystem::SphereCast(const Vector3& center, float radius) const {
    std::vector<EntityID> entities;
    
    if (!m_physicsWorldEntity.IsValid() || !m_world) {
        return entities;
    }
    
    if (!m_world->HasComponent<PhysicsWorldComponent>(m_physicsWorldEntity)) {
        return entities;
    }
    
    const auto& physicsWorld = m_world->GetComponent<PhysicsWorldComponent>(m_physicsWorldEntity);
    if (!physicsWorld.bulletWorld) {
        return entities;
    }
    
    btDiscreteDynamicsWorld* bulletWorld = static_cast<btDiscreteDynamicsWorld*>(physicsWorld.bulletWorld);
    btVector3 centerPos = ToBullet(center);
    
    // 遍历所有碰撞对象，检查是否在球形范围内
    for (int i = 0; i < bulletWorld->getNumCollisionObjects(); i++) {
        btCollisionObject* obj = bulletWorld->getCollisionObjectArray()[i];
        btRigidBody* body = btRigidBody::upcast(obj);
        
        if (body) {
            // 获取刚体的AABB
            btVector3 aabbMin, aabbMax;
            body->getCollisionShape()->getAabb(body->getWorldTransform(), aabbMin, aabbMax);
            
            // 计算AABB中心点到球形中心的距离
            btVector3 aabbCenter = (aabbMin + aabbMax) * 0.5f;
            btVector3 diff = aabbCenter - centerPos;
            float distance = diff.length();
            
            // 计算AABB的最大半径（从中心到最远角）
            btVector3 aabbHalfExtents = (aabbMax - aabbMin) * 0.5f;
            float aabbMaxRadius = aabbHalfExtents.length();
            
            // 如果距离小于半径+AABB最大半径，则可能重叠
            if (distance <= radius + aabbMaxRadius) {
                // 更精确的检测：检查AABB是否与球相交
                // 计算AABB到球心的最近点
                btVector3 closestPoint;
                closestPoint.setX(std::max(aabbMin.x(), std::min(centerPos.x(), aabbMax.x())));
                closestPoint.setY(std::max(aabbMin.y(), std::min(centerPos.y(), aabbMax.y())));
                closestPoint.setZ(std::max(aabbMin.z(), std::min(centerPos.z(), aabbMax.z())));
                
                btVector3 distVec = closestPoint - centerPos;
                float distToClosest = distVec.length();
                
                // 如果最近点在球内，则相交
                if (distToClosest <= radius) {
                    // 通过映射表查找EntityID
                    auto it = m_rigidBodyToEntity.find(body);
                    if (it != m_rigidBodyToEntity.end()) {
                        entities.push_back(it->second);
                    }
                }
            }
        }
    }
    
    return entities;
}

void PhysicsSystem::InitializePhysicsWorld(EntityID entity) {
    if (!m_world || !entity.IsValid()) {
        return;
    }
    
    if (!m_world->HasComponent<PhysicsWorldComponent>(entity)) {
        return;
    }
    
    auto& physicsWorld = m_world->GetComponent<PhysicsWorldComponent>(entity);
    
    if (physicsWorld.bulletWorld) {
        // 已经初始化
        return;
    }
    
    // 创建 Bullet 物理世界
    m_collisionConfig = new btDefaultCollisionConfiguration();
    m_dispatcher = new btCollisionDispatcher(static_cast<btCollisionConfiguration*>(m_collisionConfig));
    m_broadphase = new btDbvtBroadphase();
    m_solver = new btSequentialImpulseConstraintSolver();
    
    btDiscreteDynamicsWorld* bulletWorld = new btDiscreteDynamicsWorld(
        static_cast<btCollisionDispatcher*>(m_dispatcher),
        static_cast<btBroadphaseInterface*>(m_broadphase),
        static_cast<btConstraintSolver*>(m_solver),
        static_cast<btCollisionConfiguration*>(m_collisionConfig)
    );
    
    bulletWorld->setGravity(ToBullet(physicsWorld.gravity));
    
    physicsWorld.bulletWorld = bulletWorld;
    m_bulletWorld = bulletWorld;
    
    Logger::GetInstance().Info("[PhysicsSystem] Physics world initialized");
}

void PhysicsSystem::ShutdownPhysicsWorld() {
    if (!m_bulletWorld) {
        return;
    }
    
    btDiscreteDynamicsWorld* bulletWorld = static_cast<btDiscreteDynamicsWorld*>(m_bulletWorld);
    
    // 清理所有刚体
    if (bulletWorld && m_world) {
        for (int i = bulletWorld->getNumCollisionObjects() - 1; i >= 0; i--) {
            btCollisionObject* obj = bulletWorld->getCollisionObjectArray()[i];
            btRigidBody* body = btRigidBody::upcast(obj);
            if (body && body->getMotionState()) {
                delete body->getMotionState();
            }
            bulletWorld->removeCollisionObject(obj);
            delete obj;
        }
    }
    
    // 清理 Bullet 对象
    delete bulletWorld;
    delete static_cast<btSequentialImpulseConstraintSolver*>(m_solver);
    delete static_cast<btDbvtBroadphase*>(m_broadphase);
    delete static_cast<btCollisionDispatcher*>(m_dispatcher);
    delete static_cast<btDefaultCollisionConfiguration*>(m_collisionConfig);
    
    m_bulletWorld = nullptr;
    m_solver = nullptr;
    m_broadphase = nullptr;
    m_dispatcher = nullptr;
    m_collisionConfig = nullptr;
    
    // 清理组件中的指针
    if (m_world && m_physicsWorldEntity.IsValid()) {
        if (m_world->HasComponent<PhysicsWorldComponent>(m_physicsWorldEntity)) {
            auto& physicsWorld = m_world->GetComponent<PhysicsWorldComponent>(m_physicsWorldEntity);
            physicsWorld.bulletWorld = nullptr;
        }
    }
    
    // 清空映射表
    m_rigidBodyToEntity.clear();
    m_entityToRigidBody.clear();
    
    // 清空碰撞状态跟踪
    m_currentCollisions.clear();
    m_previousCollisions.clear();
    m_currentTriggers.clear();
    m_previousTriggers.clear();
    
    Logger::GetInstance().Info("[PhysicsSystem] Physics world shutdown");
}

void PhysicsSystem::CreateRigidBody(EntityID entity) {
    if (!m_world || !entity.IsValid()) {
        return;
    }
    
    if (!m_world->HasComponent<RigidBodyComponent>(entity) || 
        !m_world->HasComponent<ColliderComponent>(entity)) {
        return;
    }
    
    if (!m_world->HasComponent<TransformComponent>(entity)) {
        return;
    }
    
    auto& rb = m_world->GetComponent<RigidBodyComponent>(entity);
    auto& collider = m_world->GetComponent<ColliderComponent>(entity);
    auto& transform = m_world->GetComponent<TransformComponent>(entity);
    
    if (rb.bulletRigidBody) {
        // 已经创建
        return;
    }
    
    // 创建碰撞形状
    if (!collider.bulletCollisionShape) {
        CreateCollider(entity);
    }
    
    if (!collider.bulletCollisionShape) {
        Logger::GetInstance().WarningFormat("[PhysicsSystem] Failed to create collider for entity %u", entity.index);
        return;
    }
    
    btCollisionShape* baseShape = static_cast<btCollisionShape*>(collider.bulletCollisionShape);
    btCollisionShape* finalShape = baseShape;
    
    // 处理碰撞体的偏移和旋转
    // 检查是否需要使用复合形状（有偏移或旋转）
    bool hasOffset = collider.offset.norm() > 0.001f;
    bool hasRotation = (std::abs(collider.rotation.w() - 1.0f) > 0.001f || 
                       std::abs(collider.rotation.x()) > 0.001f || 
                       std::abs(collider.rotation.y()) > 0.001f || 
                       std::abs(collider.rotation.z()) > 0.001f);
    
    if (hasOffset || hasRotation) {
        // 使用复合形状来处理偏移和旋转
        btCompoundShape* compoundShape = new btCompoundShape();
        btTransform localTransform;
        localTransform.setIdentity();
        localTransform.setOrigin(ToBullet(collider.offset));
        localTransform.setRotation(ToBullet(collider.rotation));
        compoundShape->addChildShape(localTransform, baseShape);
        finalShape = compoundShape;
        // 注意：compoundShape不拥有baseShape，baseShape仍然由collider.bulletCollisionShape管理
        // 当刚体销毁时，Bullet会自动清理compoundShape，但我们需要手动删除它
    }
    
    // 计算惯性（使用最终形状）
    btVector3 localInertia(0, 0, 0);
    float mass = 0.0f;
    
    if (rb.type == RigidBodyType::Dynamic) {
        mass = rb.mass;
        if (mass > 0.0f) {
            finalShape->calculateLocalInertia(mass, localInertia);
        }
    }
    
    // 创建运动状态
    // 使用世界位置和旋转（考虑Transform的父子关系）
    // 注意：物理体需要世界空间的位置，而不是局部位置
    // 确保Transform的世界矩阵已更新（通过GetWorldMatrix会自动更新）
    Vector3 pos;
    Quaternion rot;
    
    // 从世界矩阵中提取位置和旋转
    Matrix4 worldMatrix = transform.GetWorldMatrix();
    pos = worldMatrix.block<3, 1>(0, 3);  // 提取位置（第4列的前3个元素）
    
    // 提取旋转（从世界矩阵的3x3部分）
    Matrix3 rotMatrix = worldMatrix.block<3, 3>(0, 0);
    // 移除缩放影响（归一化列向量）
    Vector3 scale(
        rotMatrix.col(0).norm(),
        rotMatrix.col(1).norm(),
        rotMatrix.col(2).norm()
    );
    if (scale.x() > 0.001f && scale.y() > 0.001f && scale.z() > 0.001f) {
        rotMatrix.col(0) /= scale.x();
        rotMatrix.col(1) /= scale.y();
        rotMatrix.col(2) /= scale.z();
    }
    rot = Quaternion(rotMatrix);
    
    btTransform startTransform = ToBullet(pos, rot);
    btDefaultMotionState* motionState = new btDefaultMotionState(startTransform);
    
    // 创建刚体构造信息（使用最终形状）
    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, finalShape, localInertia);
    rbInfo.m_friction = rb.friction;
    rbInfo.m_restitution = rb.restitution;
    rbInfo.m_linearDamping = rb.linearDamping;
    rbInfo.m_angularDamping = rb.angularDamping;
    
    // 创建刚体
    btRigidBody* bulletBody = new btRigidBody(rbInfo);
    
    // 设置刚体类型
    if (rb.type == RigidBodyType::Static) {
        bulletBody->setCollisionFlags(bulletBody->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
        bulletBody->setMassProps(0, btVector3(0, 0, 0));
    } else if (rb.type == RigidBodyType::Kinematic) {
        bulletBody->setCollisionFlags(bulletBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
        bulletBody->setActivationState(DISABLE_DEACTIVATION);
    }
    
    // 设置重力
    if (!rb.useGravity) {
        bulletBody->setGravity(btVector3(0, 0, 0));
    }
    
    // 设置启用状态
    if (!rb.enabled) {
        bulletBody->setActivationState(ISLAND_SLEEPING);
    }
    
    // 处理触发器
    if (collider.isTrigger) {
        bulletBody->setCollisionFlags(
            bulletBody->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE
        );
    }
    
    // 设置碰撞过滤
    int group = static_cast<int>(collider.collisionGroup);
    int mask = static_cast<int>(collider.collisionMask);
    
    // 添加到物理世界
    if (m_bulletWorld && m_physicsWorldEntity.IsValid() && 
        m_world->HasComponent<PhysicsWorldComponent>(m_physicsWorldEntity)) {
        auto& physicsWorld = m_world->GetComponent<PhysicsWorldComponent>(m_physicsWorldEntity);
        if (physicsWorld.bulletWorld) {
            btDiscreteDynamicsWorld* bulletWorld = static_cast<btDiscreteDynamicsWorld*>(physicsWorld.bulletWorld);
            bulletWorld->addRigidBody(bulletBody, group, mask);
        }
    }
    
    rb.bulletRigidBody = bulletBody;
    
    // 添加到映射表
    m_rigidBodyToEntity[bulletBody] = entity;
    m_entityToRigidBody[entity] = bulletBody;
    
    // 创建后立即同步一次，确保初始位置正确
    // 对于TransformToPhysics模式，需要从Transform同步到物理体
    if (rb.syncMode == RigidBodyComponent::SyncMode::TransformToPhysics) {
        SyncTransformToPhysics(entity);
    }
    
    Logger::GetInstance().InfoFormat("[PhysicsSystem] Created rigid body for entity %u", entity.index);
}

void PhysicsSystem::DestroyRigidBody(EntityID entity) {
    if (!m_world || !entity.IsValid()) {
        return;
    }
    
    if (!m_world->HasComponent<RigidBodyComponent>(entity)) {
        return;
    }
    
    auto& rb = m_world->GetComponent<RigidBodyComponent>(entity);
    
    if (!rb.bulletRigidBody) {
        return;
    }
    
    btRigidBody* bulletBody = static_cast<btRigidBody*>(rb.bulletRigidBody);
    
    // 检查是否使用了复合形状（需要在删除bulletBody之前检查）
    btCollisionShape* bodyShape = bulletBody->getCollisionShape();
    btCompoundShape* compoundShape = dynamic_cast<btCompoundShape*>(bodyShape);
    
    // 从物理世界中移除
    if (m_bulletWorld) {
        btDiscreteDynamicsWorld* bulletWorld = static_cast<btDiscreteDynamicsWorld*>(m_bulletWorld);
        bulletWorld->removeRigidBody(bulletBody);
    }
    
    // 从映射表中移除
    m_rigidBodyToEntity.erase(bulletBody);
    m_entityToRigidBody.erase(entity);
    
    // 清理内存
    if (bulletBody->getMotionState()) {
        delete bulletBody->getMotionState();
    }
    delete bulletBody;
    
    // 如果使用了复合形状，需要清理它
    // 注意：复合形状的子形状（baseShape）仍然由collider.bulletCollisionShape管理
    // 我们只删除compoundShape本身，不删除子形状
    if (compoundShape) {
        // 移除所有子形状（但不删除它们，因为它们由collider管理）
        for (int i = compoundShape->getNumChildShapes() - 1; i >= 0; i--) {
            compoundShape->removeChildShapeByIndex(i);
        }
        delete compoundShape;
    }
    
    rb.bulletRigidBody = nullptr;
}

void PhysicsSystem::CreateCollider(EntityID entity) {
    if (!m_world || !entity.IsValid()) {
        return;
    }
    
    if (!m_world->HasComponent<ColliderComponent>(entity)) {
        return;
    }
    
    auto& collider = m_world->GetComponent<ColliderComponent>(entity);
    
    if (collider.bulletCollisionShape) {
        // 已经创建，先清理旧的
        DestroyCollider(entity);
    }
    
    btCollisionShape* shape = nullptr;
    
    // 根据形状类型创建碰撞体
    switch (collider.shape) {
        case ColliderShape::Box: {
            btVector3 halfExtents(
                collider.boxSize.x() * 0.5f,
                collider.boxSize.y() * 0.5f,
                collider.boxSize.z() * 0.5f
            );
            shape = new btBoxShape(halfExtents);
            break;
        }
        
        case ColliderShape::Sphere: {
            shape = new btSphereShape(collider.sphereRadius);
            break;
        }
        
        case ColliderShape::Capsule: {
            // 胶囊体：默认使用Y轴（垂直方向）
            shape = new btCapsuleShape(collider.capsuleRadius, collider.capsuleHeight);
            break;
        }
        
        case ColliderShape::Cylinder: {
            // 圆柱体：使用半尺寸，默认Y轴
            btVector3 halfExtents(
                collider.cylinderSize.x() * 0.5f,
                collider.cylinderSize.y() * 0.5f,
                collider.cylinderSize.z() * 0.5f
            );
            shape = new btCylinderShape(halfExtents);
            break;
        }
        
        case ColliderShape::Cone: {
            // 圆锥体：radius 和 height，默认Y轴
            float radius = collider.cylinderSize.x() * 0.5f;  // 使用x作为半径
            float height = collider.cylinderSize.y();         // 使用y作为高度
            shape = new btConeShape(radius, height);
            break;
        }
        
        case ColliderShape::Plane: {
            // 平面：使用法线和常数
            btVector3 normal = ToBullet(collider.planeNormal.normalized());
            shape = new btStaticPlaneShape(normal, collider.planeConstant);
            break;
        }
        
        case ColliderShape::Mesh: {
            // Mesh碰撞体需要从ResourceManager加载
            if (collider.meshName.empty()) {
                Logger::GetInstance().WarningFormat(
                    "[PhysicsSystem] Mesh collider for entity %u has empty mesh name", entity.index
                );
                return;
            }
            
            auto& resMgr = ResourceManager::GetInstance();
            auto mesh = resMgr.GetMesh(collider.meshName);
            
            if (!mesh) {
                Logger::GetInstance().WarningFormat(
                    "[PhysicsSystem] Mesh '%s' not found for entity %u", 
                    collider.meshName.c_str(), entity.index
                );
                return;
            }
            
            if (collider.useConvexHull) {
                // 凸包碰撞体
                btConvexHullShape* convexShape = new btConvexHullShape();
                
                // 获取顶点数据
                mesh->AccessVertices([&](const std::vector<Vertex>& vertices) {
                    for (const auto& vertex : vertices) {
                        btVector3 point(
                            vertex.position.x(),
                            vertex.position.y(),
                            vertex.position.z()
                        );
                        convexShape->addPoint(point);
                    }
                });
                
                // 优化凸包形状
                if (convexShape->getNumPoints() > 0) {
                    convexShape->optimizeConvexHull();
                } else {
                    delete convexShape;
                    Logger::GetInstance().WarningFormat(
                        "[PhysicsSystem] Mesh '%s' has no vertices for convex hull (entity %u)",
                        collider.meshName.c_str(), entity.index
                    );
                    return;
                }
                
                shape = convexShape;
            } else {
                // 三角网格碰撞体（用于静态物体）
                btTriangleIndexVertexArray* indexVertexArrays = new btTriangleIndexVertexArray();
                
                // 获取顶点和索引数据
                std::vector<Vertex> vertices;
                std::vector<uint32_t> indices;
                
                mesh->AccessVertices([&](const std::vector<Vertex>& verts) {
                    vertices = verts;
                });
                
                mesh->AccessIndices([&](const std::vector<uint32_t>& inds) {
                    indices = inds;
                });
                
                if (indices.size() > 0 && vertices.size() > 0 && indices.size() % 3 == 0) {
                    size_t vertexCount = vertices.size();
                    size_t indexCount = indices.size();
                    int numTriangles = static_cast<int>(indexCount / 3);
                    
                    // 创建索引数组
                    int* indexArray = new int[indexCount];
                    for (size_t i = 0; i < indexCount; i++) {
                        indexArray[i] = static_cast<int>(indices[i]);
                    }
                    
                    // 创建顶点数组
                    btScalar* vertexArray = new btScalar[vertexCount * 3];
                    for (size_t i = 0; i < vertexCount; i++) {
                        vertexArray[i * 3 + 0] = vertices[i].position.x();
                        vertexArray[i * 3 + 1] = vertices[i].position.y();
                        vertexArray[i * 3 + 2] = vertices[i].position.z();
                    }
                    
                    // 设置索引和顶点数据
                    btIndexedMesh meshPart;
                    meshPart.m_numTriangles = numTriangles;
                    meshPart.m_triangleIndexBase = reinterpret_cast<const unsigned char*>(indexArray);
                    meshPart.m_triangleIndexStride = 3 * sizeof(int);
                    meshPart.m_numVertices = static_cast<int>(vertexCount);
                    meshPart.m_vertexBase = reinterpret_cast<const unsigned char*>(vertexArray);
                    meshPart.m_vertexStride = 3 * sizeof(btScalar);
                    meshPart.m_indexType = PHY_INTEGER;
                    meshPart.m_vertexType = PHY_FLOAT;
                    
                    indexVertexArrays->addIndexedMesh(meshPart);
                    
                    // 创建BVH三角网格形状
                    bool useQuantizedAabbCompression = true;
                    shape = new btBvhTriangleMeshShape(indexVertexArrays, useQuantizedAabbCompression);
                } else {
                    delete indexVertexArrays;
                    Logger::GetInstance().WarningFormat(
                        "[PhysicsSystem] Mesh '%s' has no valid geometry for triangle mesh (entity %u)",
                        collider.meshName.c_str(), entity.index
                    );
                    return;
                }
            }
            break;
        }
        
        default:
            Logger::GetInstance().WarningFormat(
                "[PhysicsSystem] Unsupported collider shape for entity %u", entity.index
            );
            return;
    }
    
    if (shape) {
        collider.bulletCollisionShape = shape;
        collider.needsUpdate = false;
    }
}

void PhysicsSystem::DestroyCollider(EntityID entity) {
    if (!m_world || !entity.IsValid()) {
        return;
    }
    
    if (!m_world->HasComponent<ColliderComponent>(entity)) {
        return;
    }
    
    auto& collider = m_world->GetComponent<ColliderComponent>(entity);
    
    if (!collider.bulletCollisionShape) {
        return;
    }
    
    btCollisionShape* shape = static_cast<btCollisionShape*>(collider.bulletCollisionShape);
    
    // 注意：如果形状被刚体使用，应该在销毁刚体时由 Bullet 自动清理
    // 这里只清理未被使用的形状
    if (m_world->HasComponent<RigidBodyComponent>(entity)) {
        auto& rb = m_world->GetComponent<RigidBodyComponent>(entity);
        if (rb.bulletRigidBody) {
            // 形状被刚体使用，不在这里清理
            collider.bulletCollisionShape = nullptr;
            return;
        }
    }
    
    delete shape;
    collider.bulletCollisionShape = nullptr;
}

void PhysicsSystem::UpdateCollider(EntityID entity) {
    // 重新创建碰撞体（简单实现）
    DestroyCollider(entity);
    CreateCollider(entity);
    
    // 如果有关联的刚体，需要重新创建刚体
    if (m_world && m_world->HasComponent<RigidBodyComponent>(entity)) {
        DestroyRigidBody(entity);
        CreateRigidBody(entity);
    }
}

void PhysicsSystem::CreateConstraint(EntityID entity) {
    if (!m_world || !entity.IsValid()) {
        return;
    }
    
    if (!m_world->HasComponent<ConstraintComponent>(entity)) {
        return;
    }
    
    auto& constraint = m_world->GetComponent<ConstraintComponent>(entity);
    
    if (constraint.bulletConstraint) {
        // 已经创建
        return;
    }
    
    // 约束需要两个刚体：当前实体和连接的实体
    if (!m_world->HasComponent<RigidBodyComponent>(entity)) {
        Logger::GetInstance().WarningFormat(
            "[PhysicsSystem] Constraint entity %u does not have RigidBodyComponent", entity.index
        );
        return;
    }
    
    auto& rbA = m_world->GetComponent<RigidBodyComponent>(entity);
    if (!rbA.bulletRigidBody) {
        Logger::GetInstance().WarningFormat(
            "[PhysicsSystem] Constraint entity %u has no bullet rigid body", entity.index
        );
        return;
    }
    
    btRigidBody* bodyA = static_cast<btRigidBody*>(rbA.bulletRigidBody);
    
    // 检查连接的实体
    if (!constraint.connectedEntity.IsValid()) {
        Logger::GetInstance().WarningFormat(
            "[PhysicsSystem] Constraint entity %u has invalid connected entity", entity.index
        );
        return;
    }
    
    if (!m_world->HasComponent<RigidBodyComponent>(constraint.connectedEntity)) {
        Logger::GetInstance().WarningFormat(
            "[PhysicsSystem] Connected entity %u does not have RigidBodyComponent", 
            constraint.connectedEntity.index
        );
        return;
    }
    
    auto& rbB = m_world->GetComponent<RigidBodyComponent>(constraint.connectedEntity);
    if (!rbB.bulletRigidBody) {
        Logger::GetInstance().WarningFormat(
            "[PhysicsSystem] Connected entity %u has no bullet rigid body", 
            constraint.connectedEntity.index
        );
        return;
    }
    
    btRigidBody* bodyB = static_cast<btRigidBody*>(rbB.bulletRigidBody);
    
    // 注意：Bullet约束的pivot点应该在刚体的本地坐标系中，不需要转换到世界空间
    // constraint.pivotA和pivotB已经是本地坐标系中的点，直接使用即可
    // Bullet会在内部处理坐标转换
    Vector3 pivotA = constraint.pivotA;
    Vector3 pivotB = constraint.pivotB;
    
    btTypedConstraint* bulletConstraint = nullptr;
    
    // 根据约束类型创建约束
    switch (constraint.type) {
        case ConstraintType::PointToPoint: {
            btPoint2PointConstraint* p2p = new btPoint2PointConstraint(
                *bodyA,
                *bodyB,
                ToBullet(pivotA),
                ToBullet(pivotB)
            );
            bulletConstraint = p2p;
            break;
        }
        
        case ConstraintType::Hinge: {
            // 铰链约束需要轴和锚点
            // pivot点应该在各自刚体的本地坐标系中
            btVector3 pivotInA = ToBullet(pivotA);
            btVector3 pivotInB = ToBullet(pivotB);
            btVector3 axisInA = ToBullet(constraint.axisA.normalized());
            btVector3 axisInB = ToBullet(constraint.axisB.normalized());
            
            btHingeConstraint* hinge = new btHingeConstraint(
                *bodyA,
                *bodyB,
                pivotInA,
                pivotInB,
                axisInA,
                axisInB
            );
            
            // 设置限制
            // 注意：Bullet的Hinge约束如果不设置限制，默认是完全锁定的！
            // 所以必须显式设置限制，即使范围很大
            // 如果lowerLimit和upperLimit都是0，说明没有设置限制，使用默认大范围
            if (constraint.lowerLimit == 0.0f && constraint.upperLimit == 0.0f) {
                // 没有设置限制，默认允许完整旋转（-π到π）
                hinge->setLimit(-SIMD_PI, SIMD_PI);
            } else {
                // 使用设置的限制值
                hinge->setLimit(constraint.lowerLimit, constraint.upperLimit);
            }
            
            bulletConstraint = hinge;
            break;
        }
        
        case ConstraintType::Generic6Dof: {
            // 6自由度约束
            btTransform frameInA, frameInB;
            frameInA.setIdentity();
            frameInB.setIdentity();
            
            frameInA.setOrigin(ToBullet(pivotA));
            frameInB.setOrigin(ToBullet(pivotB));
            
            // 设置旋转（基于轴）
            if (constraint.axisA.norm() > 0.001f) {
                btVector3 axis = ToBullet(constraint.axisA.normalized());
                btVector3 up(0, 1, 0);
                if (axis.dot(up) > 0.99f) {
                    up = btVector3(1, 0, 0);
                }
                btVector3 right = axis.cross(up).normalized();
                up = right.cross(axis).normalized();
                frameInA.getBasis().setValue(
                    right.x(), up.x(), axis.x(),
                    right.y(), up.y(), axis.y(),
                    right.z(), up.z(), axis.z()
                );
            }
            
            if (constraint.axisB.norm() > 0.001f) {
                btVector3 axis = ToBullet(constraint.axisB.normalized());
                btVector3 up(0, 1, 0);
                if (axis.dot(up) > 0.99f) {
                    up = btVector3(1, 0, 0);
                }
                btVector3 right = axis.cross(up).normalized();
                up = right.cross(axis).normalized();
                frameInB.getBasis().setValue(
                    right.x(), up.x(), axis.x(),
                    right.y(), up.y(), axis.y(),
                    right.z(), up.z(), axis.z()
                );
            }
            
            btGeneric6DofConstraint* dof6 = new btGeneric6DofConstraint(
                *bodyA,
                *bodyB,
                frameInA,
                frameInB,
                false  // useLinearReferenceFrameA
            );
            
            // 设置限制（简化：对所有轴设置相同限制）
            if (constraint.lowerLimit != 0.0f || constraint.upperLimit != 0.0f) {
                for (int i = 0; i < 6; i++) {
                    dof6->setLimit(i, constraint.lowerLimit, constraint.upperLimit);
                }
            }
            
            bulletConstraint = dof6;
            break;
        }
        
        default:
            Logger::GetInstance().WarningFormat(
                "[PhysicsSystem] Unsupported constraint type for entity %u", entity.index
            );
            return;
    }
    
    if (bulletConstraint) {
        // 设置启用状态
        bulletConstraint->setEnabled(constraint.enabled);
        
        // 添加到物理世界
        if (m_bulletWorld && m_physicsWorldEntity.IsValid() && 
            m_world->HasComponent<PhysicsWorldComponent>(m_physicsWorldEntity)) {
            auto& physicsWorld = m_world->GetComponent<PhysicsWorldComponent>(m_physicsWorldEntity);
            if (physicsWorld.bulletWorld) {
                btDiscreteDynamicsWorld* bulletWorld = static_cast<btDiscreteDynamicsWorld*>(physicsWorld.bulletWorld);
                bulletWorld->addConstraint(bulletConstraint, true);  // true = disable collisions between bodies
            }
        }
        
        constraint.bulletConstraint = bulletConstraint;
        
        Logger::GetInstance().InfoFormat(
            "[PhysicsSystem] Created constraint for entity %u (type: %d)", 
            entity.index, static_cast<int>(constraint.type)
        );
    }
}

void PhysicsSystem::DestroyConstraint(EntityID entity) {
    if (!m_world || !entity.IsValid()) {
        return;
    }
    
    if (!m_world->HasComponent<ConstraintComponent>(entity)) {
        return;
    }
    
    auto& constraint = m_world->GetComponent<ConstraintComponent>(entity);
    
    if (!constraint.bulletConstraint) {
        return;
    }
    
    btTypedConstraint* bulletConstraint = static_cast<btTypedConstraint*>(constraint.bulletConstraint);
    
    // 从物理世界中移除
    if (m_bulletWorld) {
        btDiscreteDynamicsWorld* bulletWorld = static_cast<btDiscreteDynamicsWorld*>(m_bulletWorld);
        bulletWorld->removeConstraint(bulletConstraint);
    }
    
    // 删除约束对象
    delete bulletConstraint;
    
    constraint.bulletConstraint = nullptr;
}

void PhysicsSystem::UpdateConstraints() {
    if (!m_world) return;
    
    // 查询所有约束组件
    auto constraintEntities = m_world->Query<ConstraintComponent>();
    
    for (const auto& entity : constraintEntities) {
        auto& constraint = m_world->GetComponent<ConstraintComponent>(entity);
        
        // 如果还没有创建约束，则创建
        if (!constraint.bulletConstraint) {
            // 检查两个实体是否都有刚体且已创建
            if (!m_world->HasComponent<RigidBodyComponent>(entity)) {
                continue;  // 第一个实体没有刚体，跳过
            }
            
            auto& rbA = m_world->GetComponent<RigidBodyComponent>(entity);
            if (!rbA.bulletRigidBody) {
                continue;  // 第一个实体的刚体还未创建，等待下一帧
            }
            
            if (!constraint.connectedEntity.IsValid() || 
                !m_world->HasComponent<RigidBodyComponent>(constraint.connectedEntity)) {
                continue;  // 第二个实体无效或没有刚体，跳过
            }
            
            auto& rbB = m_world->GetComponent<RigidBodyComponent>(constraint.connectedEntity);
            if (!rbB.bulletRigidBody) {
                continue;  // 第二个实体的刚体还未创建，等待下一帧
            }
            
            // 两个刚体都已创建，现在可以创建约束
            CreateConstraint(entity);
        } else {
            // 更新约束状态（如果启用状态改变）
            btTypedConstraint* bulletConstraint = static_cast<btTypedConstraint*>(constraint.bulletConstraint);
            bulletConstraint->setEnabled(constraint.enabled);
        }
    }
}

void PhysicsSystem::BatchSyncTransformsToPhysics() {
    if (!m_world) return;
    
    auto entities = m_world->Query<RigidBodyComponent>();
    
    for (const auto& entity : entities) {
        auto& rb = m_world->GetComponent<RigidBodyComponent>(entity);
        
        if (rb.syncMode == RigidBodyComponent::SyncMode::TransformToPhysics) {
            SyncTransformToPhysics(entity);
        }
    }
}

void PhysicsSystem::BatchSyncPhysicsToTransforms() {
    if (!m_world) return;
    
    auto entities = m_world->Query<RigidBodyComponent>();
    
    for (const auto& entity : entities) {
        auto& rb = m_world->GetComponent<RigidBodyComponent>(entity);
        
        if (rb.syncMode == RigidBodyComponent::SyncMode::PhysicsToTransform) {
            SyncPhysicsToTransform(entity);
        }
    }
}

void PhysicsSystem::SyncTransformToPhysics(EntityID entity) {
    if (!m_world || !entity.IsValid()) {
        return;
    }
    
    if (!m_world->HasComponent<RigidBodyComponent>(entity) ||
        !m_world->HasComponent<TransformComponent>(entity)) {
        return;
    }
    
    auto& rb = m_world->GetComponent<RigidBodyComponent>(entity);
    auto& transform = m_world->GetComponent<TransformComponent>(entity);
    
    if (!rb.bulletRigidBody) {
        return;
    }
    
    btRigidBody* bulletBody = static_cast<btRigidBody*>(rb.bulletRigidBody);
    
    // 使用世界位置和旋转（考虑Transform的父子关系）
    Matrix4 worldMatrix = transform.GetWorldMatrix();
    Vector3 worldPos = worldMatrix.block<3, 1>(0, 3);
    
    // 提取旋转（从世界矩阵的3x3部分）
    Matrix3 rotMatrix = worldMatrix.block<3, 3>(0, 0);
    // 移除缩放影响
    Vector3 scale(
        rotMatrix.col(0).norm(),
        rotMatrix.col(1).norm(),
        rotMatrix.col(2).norm()
    );
    if (scale.x() > 0.001f && scale.y() > 0.001f && scale.z() > 0.001f) {
        rotMatrix.col(0) /= scale.x();
        rotMatrix.col(1) /= scale.y();
        rotMatrix.col(2) /= scale.z();
    }
    Quaternion worldRot = Quaternion(rotMatrix);
    
    btTransform btTrans = ToBullet(worldPos, worldRot);
    
    bulletBody->setWorldTransform(btTrans);
    bulletBody->activate();
}

void PhysicsSystem::SyncPhysicsToTransform(EntityID entity) {
    if (!m_world || !entity.IsValid()) {
        return;
    }
    
    if (!m_world->HasComponent<RigidBodyComponent>(entity) ||
        !m_world->HasComponent<TransformComponent>(entity)) {
        return;
    }
    
    auto& rb = m_world->GetComponent<RigidBodyComponent>(entity);
    auto& transform = m_world->GetComponent<TransformComponent>(entity);
    
    if (!rb.bulletRigidBody) {
        return;
    }
    
    btRigidBody* bulletBody = static_cast<btRigidBody*>(rb.bulletRigidBody);
    btTransform btTrans;
    bulletBody->getMotionState()->getWorldTransform(btTrans);
    
    Vector3 worldPos;
    Quaternion worldRot;
    FromBullet(btTrans, worldPos, worldRot);
    
    // 将世界位置转换为局部位置（如果有父节点）
    Vector3 localPos = worldPos;
    Quaternion localRot = worldRot;
    
    // 检查是否有父节点
    if (transform.parentEntity.IsValid() && 
        m_world->HasComponent<TransformComponent>(transform.parentEntity)) {
        auto& parentTransform = m_world->GetComponent<TransformComponent>(transform.parentEntity);
        
        // 使用父节点的逆变换将世界位置转换为局部位置
        if (parentTransform.transform) {
            Matrix4 parentWorldMatrix = parentTransform.GetWorldMatrix();
            Matrix4 parentWorldMatrixInv = parentWorldMatrix.inverse();
            
            // 转换位置
            Vector4 worldPos4(worldPos.x(), worldPos.y(), worldPos.z(), 1.0f);
            Vector4 localPos4 = parentWorldMatrixInv * worldPos4;
            localPos = localPos4.head<3>();
            
            // 转换旋转（从世界矩阵的3x3部分提取旋转）
            Matrix3 parentRotMatrix = parentWorldMatrix.block<3, 3>(0, 0);
            // 移除缩放影响
            Vector3 parentScale(
                parentRotMatrix.col(0).norm(),
                parentRotMatrix.col(1).norm(),
                parentRotMatrix.col(2).norm()
            );
            if (parentScale.x() > 0.001f && parentScale.y() > 0.001f && parentScale.z() > 0.001f) {
                parentRotMatrix.col(0) /= parentScale.x();
                parentRotMatrix.col(1) /= parentScale.y();
                parentRotMatrix.col(2) /= parentScale.z();
            }
            Quaternion parentRot(parentRotMatrix);
            
            // 局部旋转 = 父旋转的逆 * 世界旋转
            localRot = parentRot.inverse() * worldRot;
        }
    }
    
    transform.SetPosition(localPos);
    transform.SetRotation(localRot);
}

void PhysicsSystem::DetectCollisionsAndTriggers() {
    if (!m_world || !m_bulletWorld) return;
    
    btDiscreteDynamicsWorld* bulletWorld = static_cast<btDiscreteDynamicsWorld*>(m_bulletWorld);
    btDispatcher* dispatcher = bulletWorld->getDispatcher();
    
    // 清空当前帧的碰撞和触发器集合
    m_currentCollisions.clear();
    m_currentTriggers.clear();
    
    // 遍历所有碰撞对
    int numManifolds = dispatcher->getNumManifolds();
    for (int i = 0; i < numManifolds; i++) {
        btPersistentManifold* contactManifold = dispatcher->getManifoldByIndexInternal(i);
        const btCollisionObject* objA = contactManifold->getBody0();
        const btCollisionObject* objB = contactManifold->getBody1();
        
        btRigidBody* bodyA = const_cast<btRigidBody*>(btRigidBody::upcast(objA));
        btRigidBody* bodyB = const_cast<btRigidBody*>(btRigidBody::upcast(objB));
        
        if (!bodyA || !bodyB) continue;
        
        // 查找对应的EntityID
        auto itA = m_rigidBodyToEntity.find(bodyA);
        auto itB = m_rigidBodyToEntity.find(bodyB);
        
        if (itA == m_rigidBodyToEntity.end() || itB == m_rigidBodyToEntity.end()) {
            continue;
        }
        
        EntityID entityA = itA->second;
        EntityID entityB = itB->second;
        
        // 确保entityA < entityB（用于集合去重）
        if (entityB < entityA) {
            std::swap(entityA, entityB);
            std::swap(bodyA, bodyB);
        }
        
        std::pair<EntityID, EntityID> collisionPair(entityA, entityB);
        
        // 检查是否是触发器
        bool isTriggerA = false;
        bool isTriggerB = false;
        
        if (m_world->HasComponent<ColliderComponent>(entityA)) {
            auto& colliderA = m_world->GetComponent<ColliderComponent>(entityA);
            isTriggerA = colliderA.isTrigger;
        }
        
        if (m_world->HasComponent<ColliderComponent>(entityB)) {
            auto& colliderB = m_world->GetComponent<ColliderComponent>(entityB);
            isTriggerB = colliderB.isTrigger;
        }
        
        if (isTriggerA || isTriggerB) {
            // 触发器重叠
            m_currentTriggers.insert(collisionPair);
        } else {
            // 普通碰撞
            int numContacts = contactManifold->getNumContacts();
            if (numContacts > 0) {
                m_currentCollisions.insert(collisionPair);
                
                // 获取第一个接触点的信息
                btManifoldPoint& pt = contactManifold->getContactPoint(0);
                Vector3 point = FromBullet(pt.getPositionWorldOnA());
                Vector3 normal = FromBullet(pt.m_normalWorldOnB);
                
                // 检查是否是新的碰撞（Enter）
                if (m_previousCollisions.find(collisionPair) == m_previousCollisions.end()) {
                    OnCollisionEnter(entityA, entityB, point, normal);
                }
            }
        }
    }
    
    // 检测碰撞退出
    for (const auto& pair : m_previousCollisions) {
        if (m_currentCollisions.find(pair) == m_currentCollisions.end()) {
            OnCollisionExit(pair.first, pair.second);
        }
    }
    
    // 检测触发器进入
    for (const auto& pair : m_currentTriggers) {
        if (m_previousTriggers.find(pair) == m_previousTriggers.end()) {
            OnTriggerEnter(pair.first, pair.second);
        }
    }
    
    // 检测触发器退出
    for (const auto& pair : m_previousTriggers) {
        if (m_currentTriggers.find(pair) == m_currentTriggers.end()) {
            OnTriggerExit(pair.first, pair.second);
        }
    }
    
    // 更新上一帧的状态
    m_previousCollisions = m_currentCollisions;
    m_previousTriggers = m_currentTriggers;
}

void PhysicsSystem::OnCollisionEnter(EntityID entityA, EntityID entityB, const Vector3& point, const Vector3& normal) {
    // 碰撞进入回调
    // 可以在这里添加事件系统调用或日志
    Logger::GetInstance().DebugFormat(
        "[PhysicsSystem] Collision Enter: entity %u <-> entity %u at (%f, %f, %f)",
        entityA.index, entityB.index, point.x(), point.y(), point.z()
    );
}

void PhysicsSystem::OnCollisionExit(EntityID entityA, EntityID entityB) {
    // 碰撞退出回调
    Logger::GetInstance().DebugFormat(
        "[PhysicsSystem] Collision Exit: entity %u <-> entity %u",
        entityA.index, entityB.index
    );
}

void PhysicsSystem::OnTriggerEnter(EntityID entityA, EntityID entityB) {
    // 触发器进入回调
    Logger::GetInstance().DebugFormat(
        "[PhysicsSystem] Trigger Enter: entity %u <-> entity %u",
        entityA.index, entityB.index
    );
}

void PhysicsSystem::OnTriggerExit(EntityID entityA, EntityID entityB) {
    // 触发器退出回调
    Logger::GetInstance().DebugFormat(
        "[PhysicsSystem] Trigger Exit: entity %u <-> entity %u",
        entityA.index, entityB.index
    );
}

} // namespace ECS
} // namespace Render
