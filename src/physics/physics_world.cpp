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
#include "render/ecs/components.h"
#include "render/physics/physics_components.h"
#include "render/math_utils.h"
#include "render/logger.h"
#include <memory>
#include <unordered_set>

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
    
    // ==================== 3.1.2 注册TransformComponent变化事件回调 ====================
    if (m_ecsWorld) {
        m_transformChangeCallbackId = m_ecsWorld->GetComponentRegistry()
            .RegisterComponentChangeCallback<ECS::TransformComponent>(
                [this](ECS::EntityID entity, const ECS::TransformComponent& transformComp) {
                    OnTransformComponentChanged(entity, transformComp);
                }
            );
        
        Logger::GetInstance().DebugFormat(
            "[PhysicsWorld] 已注册TransformComponent变化事件回调，回调ID: %llu",
            static_cast<unsigned long long>(m_transformChangeCallbackId)
        );
    }
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
    // ==================== 3.1.3 取消注册TransformComponent变化事件回调 ====================
    if (m_ecsWorld && m_transformChangeCallbackId != 0) {
        try {
            m_ecsWorld->GetComponentRegistry()
                .UnregisterComponentChangeCallback(m_transformChangeCallbackId);
            
            Logger::GetInstance().DebugFormat(
                "[PhysicsWorld] 已取消注册TransformComponent变化事件回调，回调ID: %llu",
                static_cast<unsigned long long>(m_transformChangeCallbackId)
            );
        } catch (...) {
            // 忽略取消注册时的异常（ECS World可能已经部分销毁）
            Logger::GetInstance().Warning("[PhysicsWorld] 取消注册TransformComponent变化事件回调时发生异常");
        }
        m_transformChangeCallbackId = 0;
    }
    
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
        // ==================== 3.2.4 插值变换 ====================
        // Bullet 使用固定时间步长，需要在渲染前进行插值
        // 插值因子由固定时间步长和实际时间步长计算
        if (m_transformSync) {
            // 计算插值因子 alpha = (实际时间 - 已累积的固定时间步长) / 固定时间步长
            // 这里简化处理，使用固定时间步长的一半作为插值因子
            // 更精确的实现需要跟踪累积时间
            float fixedDeltaTime = m_config.fixedDeltaTime;
            float alpha = (fixedDeltaTime > 1e-6f) ? (deltaTime / fixedDeltaTime) : 1.0f;
            alpha = MathUtils::Clamp(alpha, 0.0f, 1.0f);
            m_transformSync->InterpolateTransforms(m_ecsWorld, alpha);
        }
        
        // ==================== 3.3.2 输出性能统计信息（定期）====================
        #ifdef DEBUG
        static int statsOutputCounter = 0;
        if (++statsOutputCounter >= 300) {  // 每300帧输出一次统计信息（假设60FPS，约5秒）
            if (m_transformSyncStats.totalSyncs > 0 || 
                m_transformSyncStats.skippedDynamic > 0 || 
                m_transformSyncStats.skippedNoRigidBody > 0) {
                Logger::GetInstance().InfoFormat(
                    "[PhysicsWorld] Transform同步统计（最近%d帧）："
                    "总同步=%zu, Kinematic=%zu, Static=%zu, "
                    "跳过Dynamic=%zu, 跳过无刚体=%zu",
                    statsOutputCounter,
                    m_transformSyncStats.totalSyncs,
                    m_transformSyncStats.kinematicSyncs,
                    m_transformSyncStats.staticSyncs,
                    m_transformSyncStats.skippedDynamic,
                    m_transformSyncStats.skippedNoRigidBody
                );
                // 重置统计（可选：如果想要累计统计，可以注释掉这行）
                m_transformSyncStats.Reset();
            }
            statsOutputCounter = 0;
        }
        #endif
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
    
    std::unordered_set<ECS::EntityID, ECS::EntityID::Hash> currentEntities;
    auto entities = m_ecsWorld->Query<RigidBodyComponent, ColliderComponent>();
    
    for (ECS::EntityID entity : entities) {
        if (!m_ecsWorld->IsValidEntity(entity)) {
            continue;
        }
        
        try {
            auto& body = m_ecsWorld->GetComponent<RigidBodyComponent>(entity);
            auto& collider = m_ecsWorld->GetComponent<ColliderComponent>(entity);
            
            currentEntities.insert(entity);
            
            // 获取TransformComponent（如果存在）
            Vector3 transformPosition;
            Quaternion transformRotation;
            bool hasTransform = false;
            if (m_ecsWorld->HasComponent<ECS::TransformComponent>(entity)) {
                auto& transform = m_ecsWorld->GetComponent<ECS::TransformComponent>(entity);
                transformPosition = transform.GetPosition();
                transformRotation = transform.GetRotation();
                hasTransform = true;
            }
            
            // 检查是否已在Bullet中
            bool wasNewlyAdded = false;
            
            if (m_bulletAdapter->HasRigidBody(entity)) {
                // 更新现有刚体
                m_bulletAdapter->UpdateRigidBody(entity, body, collider);
            } else {
                // 添加新刚体
                m_bulletAdapter->AddRigidBody(entity, body, collider);
                wasNewlyAdded = true;
            }
            
            // === 关键修复：确保Kinematic/Static物体的Transform总是同步 ===
            if (hasTransform) {
                // 对于Kinematic和Static物体，每次Step都必须同步Transform
                // 因为它们是ECS驱动的（不是物理模拟驱动的）
                if (body.type == RigidBodyComponent::BodyType::Kinematic || 
                    body.type == RigidBodyComponent::BodyType::Static ||
                    wasNewlyAdded) {
                    
                    // 添加调试输出
                    std::cout << "[SYNC] Syncing transform for entity " << entity.index 
                              << ", type: " << (int)body.type 
                              << ", position: (" << transformPosition.x() << ", "
                              << transformPosition.y() << ", " << transformPosition.z() << ")" 
                              << std::endl;
                    
                    // 同步到Bullet
                    m_bulletAdapter->SyncTransformToBullet(entity, transformPosition, transformRotation);
                }
            }
            
        } catch (...) {
            // 忽略组件访问错误
        }
    }
}

void PhysicsWorld::SyncBulletToECS() {
    // ==================== 3.2.2 Bullet → ECS 同步 ====================
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
                
                // 从 Bullet 同步 RigidBodyComponent（速度、力、扭矩等）
                m_bulletAdapter->SyncToECS(entity, body);
                
                // ==================== 3.2.2 同步 TransformComponent（Dynamic 物体） ====================
                // 只同步 Dynamic 物体的变换（Kinematic/Static 由 TransformComponent 驱动）
                if (body.type == RigidBodyComponent::BodyType::Dynamic) {
                    // 检查是否有 TransformComponent
                    if (m_ecsWorld->HasComponent<ECS::TransformComponent>(entity)) {
                        auto& transform = m_ecsWorld->GetComponent<ECS::TransformComponent>(entity);
                        
                        // 从 Bullet 同步位置和旋转
                        Vector3 position;
                        Quaternion rotation;
                        m_bulletAdapter->SyncTransformFromBullet(entity, position, rotation);
                        
                        // 更新 TransformComponent
                        transform.SetPosition(position);
                        transform.SetRotation(rotation);
                    }
                }
            }
        } catch (...) {
            // 忽略组件访问错误
        }
    }
}

// ==================== 3.2 TransformComponent变化事件处理 ====================

void PhysicsWorld::OnTransformComponentChanged(ECS::EntityID entity, 
                                                 const ECS::TransformComponent& transformComp) {
    if (!m_bulletAdapter || !m_ecsWorld) {
        return;
    }
    
    // 检查实体有效性
    if (!m_ecsWorld->IsValidEntity(entity)) {
        return;
    }
    
    // 检查实体是否有物理组件
    if (!m_ecsWorld->HasComponent<RigidBodyComponent>(entity)) {
        #ifdef DEBUG
        m_transformSyncStats.skippedNoRigidBody++;
        #endif
        return;  // 没有物理组件，不需要同步
    }
    
    try {
        auto& body = m_ecsWorld->GetComponent<RigidBodyComponent>(entity);
        
        // 只同步Kinematic/Static物体
        // Dynamic物体由物理模拟驱动，不应从ECS同步
        if (body.type == RigidBodyComponent::BodyType::Kinematic ||
            body.type == RigidBodyComponent::BodyType::Static) {
            
            // 检查是否已在Bullet中创建
            if (m_bulletAdapter->HasRigidBody(entity)) {
                // 立即同步到Bullet
                Vector3 position = transformComp.GetPosition();
                Quaternion rotation = transformComp.GetRotation();
                
                m_bulletAdapter->SyncTransformToBullet(entity, position, rotation);
                
                // ==================== 3.3.2 性能统计 ====================
                #ifdef DEBUG
                m_transformSyncStats.totalSyncs++;
                if (body.type == RigidBodyComponent::BodyType::Kinematic) {
                    m_transformSyncStats.kinematicSyncs++;
                } else if (body.type == RigidBodyComponent::BodyType::Static) {
                    m_transformSyncStats.staticSyncs++;
                }
                #endif
                
                // ==================== 3.3.1 调试日志 ====================
                #ifdef DEBUG
                Logger::GetInstance().DebugFormat(
                    "[PhysicsWorld] TransformComponent变化已同步到Bullet，实体: %u, 类型: %d, "
                    "位置: (%.3f, %.3f, %.3f), 旋转: (w:%.3f, x:%.3f, y:%.3f, z:%.3f)",
                    entity.index, static_cast<int>(body.type),
                    position.x(), position.y(), position.z(),
                    rotation.w(), rotation.x(), rotation.y(), rotation.z()
                );
                #endif
            }
            // 如果刚体尚未创建，会在下次Step()中创建
        } else {
            // Dynamic物体，不触发同步
            #ifdef DEBUG
            m_transformSyncStats.skippedDynamic++;
            #endif
        }
    } catch (...) {
        // 忽略组件访问错误（实体可能已被销毁）
        Logger::GetInstance().WarningFormat(
            "[PhysicsWorld] OnTransformComponentChanged处理实体 %u 时发生异常",
            entity.index
        );
    }
}
#endif

} // namespace Physics
} // namespace Render

