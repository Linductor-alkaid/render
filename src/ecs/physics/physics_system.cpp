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
    
    // 6. 更新统计信息
    m_stats.rigidBodyCount = rigidBodyEntities.size();
    m_stats.colliderCount = colliderEntities.size();
    m_stats.constraintCount = 0;  // 阶段一不支持约束
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
            // 需要在创建刚体时存储实体ID映射，这里暂时返回Invalid
            // 阶段一简化实现
            hit.entity = EntityID::Invalid();
        }
        
        hits.push_back(hit);
    }
    
    return hits;
}

std::vector<EntityID> PhysicsSystem::SphereCast(const Vector3& center, float radius) const {
    std::vector<EntityID> entities;
    
    // 阶段一简化实现，暂不支持球形检测
    // 需要在阶段二实现
    (void)center;
    (void)radius;
    
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
    
    btCollisionShape* shape = static_cast<btCollisionShape*>(collider.bulletCollisionShape);
    
    // 计算惯性
    btVector3 localInertia(0, 0, 0);
    float mass = 0.0f;
    
    if (rb.type == RigidBodyType::Dynamic) {
        mass = rb.mass;
        if (mass > 0.0f) {
            shape->calculateLocalInertia(mass, localInertia);
        }
    }
    
    // 创建运动状态
    Vector3 pos = transform.GetPosition();
    Quaternion rot = transform.GetRotation();
    btTransform startTransform = ToBullet(pos, rot);
    btDefaultMotionState* motionState = new btDefaultMotionState(startTransform);
    
    // 创建刚体构造信息
    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape, localInertia);
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
    
    // 添加到物理世界
    if (m_bulletWorld && m_physicsWorldEntity.IsValid() && 
        m_world->HasComponent<PhysicsWorldComponent>(m_physicsWorldEntity)) {
        auto& physicsWorld = m_world->GetComponent<PhysicsWorldComponent>(m_physicsWorldEntity);
        if (physicsWorld.bulletWorld) {
            btDiscreteDynamicsWorld* bulletWorld = static_cast<btDiscreteDynamicsWorld*>(physicsWorld.bulletWorld);
            bulletWorld->addRigidBody(bulletBody);
        }
    }
    
    rb.bulletRigidBody = bulletBody;
    
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
    
    // 从物理世界中移除
    if (m_bulletWorld) {
        btDiscreteDynamicsWorld* bulletWorld = static_cast<btDiscreteDynamicsWorld*>(m_bulletWorld);
        bulletWorld->removeRigidBody(bulletBody);
    }
    
    // 清理内存
    if (bulletBody->getMotionState()) {
        delete bulletBody->getMotionState();
    }
    delete bulletBody;
    
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

void PhysicsSystem::UpdateConstraints() {
    // 阶段一不支持约束
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
    
    Vector3 pos = transform.GetPosition();
    Quaternion rot = transform.GetRotation();
    btTransform btTrans = ToBullet(pos, rot);
    
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
    
    Vector3 pos;
    Quaternion rot;
    FromBullet(btTrans, pos, rot);
    
    transform.SetPosition(pos);
    transform.SetRotation(rot);
}

void PhysicsSystem::OnCollisionEnter(EntityID entityA, EntityID entityB, const Vector3& point, const Vector3& normal) {
    // 阶段一不支持碰撞回调
    (void)entityA;
    (void)entityB;
    (void)point;
    (void)normal;
}

void PhysicsSystem::OnCollisionExit(EntityID entityA, EntityID entityB) {
    // 阶段一不支持碰撞回调
    (void)entityA;
    (void)entityB;
}

void PhysicsSystem::OnTriggerEnter(EntityID entityA, EntityID entityB) {
    // 阶段一不支持触发器回调
    (void)entityA;
    (void)entityB;
}

void PhysicsSystem::OnTriggerExit(EntityID entityA, EntityID entityB) {
    // 阶段一不支持触发器回调
    (void)entityA;
    (void)entityB;
}

} // namespace ECS
} // namespace Render
