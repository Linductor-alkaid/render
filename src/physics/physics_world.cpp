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
#include "render/physics/physics_world.h"
#include "render/physics/physics_systems.h"
#include "render/physics/physics_transform_sync.h"
#include "render/ecs/world.h"
#include <memory>

#ifdef USE_BULLET_PHYSICS
#include "render/physics/bullet_adapter/bullet_world_adapter.h"
#include "render/application/event_bus.h"
#endif

namespace Render {
namespace Physics {

PhysicsWorld::PhysicsWorld(ECS::World* ecsWorld, const PhysicsConfig& config)
    : m_ecsWorld(ecsWorld), m_config(config) {
    // ==================== 3.1.2 条件编译支持 ====================
    
#ifdef USE_BULLET_PHYSICS
    // 使用 Bullet 物理引擎后端
    m_useBulletBackend = true;
    m_bulletAdapter = std::make_unique<BulletAdapter::BulletWorldAdapter>(config);
    
    // 设置事件总线（如果 ECS World 有事件总线）
    // 注意：这里需要从 ECS World 获取事件总线，暂时留空，后续在 3.2 中实现
    // Application::EventBus* eventBus = ecsWorld->GetEventBus();
    // if (eventBus) {
    //     m_bulletAdapter->SetEventBus(eventBus);
    // }
    
    // 设置材质获取函数（用于从 ECS 获取材质）
    m_bulletAdapter->SetMaterialGetter([ecsWorld](ECS::EntityID entity) -> std::shared_ptr<PhysicsMaterial> {
        if (!ecsWorld) {
            return nullptr;
        }
        // 从 ECS 获取 ColliderComponent 并返回材质
        try {
            auto& collider = ecsWorld->GetComponent<ColliderComponent>(entity);
            if (collider.material) {
                return collider.material;
            }
        } catch (...) {
            // 组件不存在，返回 nullptr
        }
        return nullptr;
    });
#else
    // 使用原有实现（向后兼容）
    m_useBulletBackend = false;
#endif
    
    // 创建物理-渲染同步器（两种后端都需要）
    m_transformSync = std::make_unique<PhysicsTransformSync>();
}

PhysicsWorld::~PhysicsWorld() {
    // 析构函数需要完整类型来删除 unique_ptr
    // 在 cpp 文件中定义，确保 BulletWorldAdapter 的完整定义可见
#ifdef USE_BULLET_PHYSICS
    // m_bulletAdapter 会在 unique_ptr 析构时自动删除
    // 但需要确保此时 BulletWorldAdapter 的完整定义可见
    // 由于已经在 cpp 文件中包含了 bullet_world_adapter.h，所以完整类型可见
    m_bulletAdapter.reset();
#endif
    // m_transformSync 会在 unique_ptr 析构时自动删除
}

void PhysicsWorld::Step(float deltaTime) {
    if (!m_ecsWorld) {
        return;
    }

#ifdef USE_BULLET_PHYSICS
    // ==================== 3.1.2 使用 Bullet 后端 ====================
    if (m_useBulletBackend && m_bulletAdapter) {
        // 1. 渲染 → 物理同步（Kinematic/Static 物体）
        if (m_transformSync) {
            m_transformSync->SyncTransformToPhysics(m_ecsWorld, deltaTime);
        }
        
        // 2. 同步 ECS 组件到 Bullet（添加/更新/移除刚体）
        SyncECSToBullet();
        
        // 3. Bullet 物理更新
        m_bulletAdapter->Step(deltaTime);
        
        // 4. 同步 Bullet 结果到 ECS（位置、旋转、速度等）
        SyncBulletToECS();
        
        // 5. 物理 → 渲染同步（动态物体）
        if (m_transformSync) {
            m_transformSync->SyncPhysicsToTransform(m_ecsWorld);
        }
        
        // 6. 插值变换（平滑渲染）
        // 注意：Bullet 使用固定时间步长，插值在 BulletWorldAdapter 内部处理
        // 这里暂时不处理插值，后续在 3.2 中完善
    } else {
        // 回退到原有实现（不应该发生，但为了安全）
        StepLegacy(deltaTime);
    }
#else
    // ==================== 原有实现（向后兼容） ====================
    StepLegacy(deltaTime);
#endif
}

// 原有实现（向后兼容）
void PhysicsWorld::StepLegacy(float deltaTime) {
    if (!m_ecsWorld) {
        return;
    }

    // 1. 渲染 → 物理同步（Kinematic/Static 物体）
    if (m_transformSync) {
        m_transformSync->SyncTransformToPhysics(m_ecsWorld, deltaTime);
    }

    // 2. 物理更新
    auto* physicsSystem = m_ecsWorld->GetSystem<PhysicsUpdateSystem>();
    if (physicsSystem) {
        physicsSystem->SetGravity(m_config.gravity);
        physicsSystem->SetFixedDeltaTime(m_config.fixedDeltaTime);
        physicsSystem->SetSolverIterations(m_config.solverIterations);
        physicsSystem->SetPositionIterations(m_config.positionIterations);
        physicsSystem->Update(deltaTime);
    }

    // 3. 物理 → 渲染同步（动态物体）
    if (m_transformSync) {
        m_transformSync->SyncPhysicsToTransform(m_ecsWorld);
    }

    // 4. 插值变换（平滑渲染）
    if (physicsSystem && m_transformSync) {
        float alpha = physicsSystem->GetInterpolationAlpha();
        m_transformSync->InterpolateTransforms(m_ecsWorld, alpha);
    }
}

/**
 * @brief 获取插值因子并执行插值
 * 
 * 这个方法应该在物理更新后、渲染前调用
 * 用于在固定时间步长和渲染帧率之间进行平滑插值
 */
void PhysicsWorld::InterpolateTransforms(float alpha) {
    if (m_transformSync) {
        m_transformSync->InterpolateTransforms(m_ecsWorld, alpha);
    }
}

#ifdef USE_BULLET_PHYSICS
// ==================== 3.1.2 ECS 与 Bullet 同步方法 ====================

void PhysicsWorld::SyncECSToBullet() {
    if (!m_ecsWorld || !m_bulletAdapter) {
        return;
    }
    
    // 查询所有有 RigidBodyComponent 和 ColliderComponent 的实体
    auto entities = m_ecsWorld->Query<RigidBodyComponent, ColliderComponent>();
    
    for (ECS::EntityID entity : entities) {
        if (!m_ecsWorld->IsValidEntity(entity)) {
            continue;
        }
        
        try {
            auto& body = m_ecsWorld->GetComponent<RigidBodyComponent>(entity);
            auto& collider = m_ecsWorld->GetComponent<ColliderComponent>(entity);
            
            // 检查是否已在 Bullet 中
            if (m_bulletAdapter->HasRigidBody(entity)) {
                // 更新现有刚体
                m_bulletAdapter->UpdateRigidBody(entity, body, collider);
            } else {
                // 添加新刚体
                m_bulletAdapter->AddRigidBody(entity, body, collider);
            }
        } catch (...) {
            // 忽略组件访问错误
        }
    }
    
    // 检查是否有实体被移除（需要从 Bullet 中移除）
    // 注意：这需要维护一个实体集合，暂时简化处理
    // 后续在 3.2 中完善
}

void PhysicsWorld::SyncBulletToECS() {
    if (!m_ecsWorld || !m_bulletAdapter) {
        return;
    }
    
    // 查询所有有 RigidBodyComponent 的实体，从 Bullet 同步状态
    auto entities = m_ecsWorld->Query<RigidBodyComponent>();
    
    for (ECS::EntityID entity : entities) {
        if (!m_ecsWorld->IsValidEntity(entity)) {
            continue;
        }
        
        try {
            if (m_bulletAdapter->HasRigidBody(entity)) {
                auto& body = m_ecsWorld->GetComponent<RigidBodyComponent>(entity);
                // 从 Bullet 同步位置、旋转、速度等
                m_bulletAdapter->SyncToECS(entity, body);
            }
        } catch (...) {
            // 忽略组件访问错误
        }
    }
}
#endif

} // namespace Physics
} // namespace Render

