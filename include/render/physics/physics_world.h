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

#include "physics_config.h"
#include "physics_components.h"
#include "physics_transform_sync.h"

#ifdef USE_BULLET_PHYSICS
// 前向声明 Bullet 适配器（避免循环依赖）
namespace Render::Physics::BulletAdapter {
class BulletWorldAdapter;
}
#endif

namespace Render {

namespace ECS {
class World;  // 前向声明
}

namespace Physics {

class CollisionDetectionSystem;
class PhysicsUpdateSystem;

/**
 * @brief 物理世界
 * 
 * 管理整个物理模拟系统的核心类
 * 
 * @note 这是一个初始框架，完整实现将在后续阶段添加
 */
class PhysicsWorld {
public:
    /**
     * @brief 构造函数
     * @param ecsWorld ECS 世界指针
     * @param config 物理配置
     */
    PhysicsWorld(ECS::World* ecsWorld, const PhysicsConfig& config = PhysicsConfig::Default());
    
    /**
     * @brief 析构函数
     * 
     * @note 需要在实现文件中定义，因为需要完整类型来删除 unique_ptr
     */
    ~PhysicsWorld();
    
    /**
     * @brief 物理步进（每帧调用）
     * @param deltaTime 帧时间（秒）
     */
    void Step(float deltaTime);
    
    /**
     * @brief 设置重力
     */
    void SetGravity(const Vector3& gravity) {
        m_config.gravity = gravity;
    }
    
    /**
     * @brief 获取重力
     */
    Vector3 GetGravity() const {
        return m_config.gravity;
    }
    
    /**
     * @brief 获取配置
     */
    const PhysicsConfig& GetConfig() const {
        return m_config;
    }

    /**
     * @brief 更新物理配置
     */
    void SetConfig(const PhysicsConfig& config) {
        m_config = config;
    }
    
    /**
     * @brief 插值变换（平滑渲染）
     * 
     * 在固定时间步长和渲染帧率之间进行插值
     * 应该在物理更新后、渲染前调用
     * 
     * @param alpha 插值因子 [0, 1]
     */
    void InterpolateTransforms(float alpha);
    
#ifdef USE_BULLET_PHYSICS
    /**
     * @brief 获取 Bullet 适配器（用于高级操作）
     * @return BulletWorldAdapter 指针，如果未启用 Bullet 则返回 nullptr
     */
    BulletAdapter::BulletWorldAdapter* GetBulletAdapter() {
        return m_bulletAdapter.get();
    }
    
    /**
     * @brief 获取 Bullet 适配器（常量版本）
     */
    const BulletAdapter::BulletWorldAdapter* GetBulletAdapter() const {
        return m_bulletAdapter.get();
    }
#endif
    
private:
    /**
     * @brief 原有实现（向后兼容，当不使用 Bullet 时使用）
     */
    void StepLegacy(float deltaTime);
    
#ifdef USE_BULLET_PHYSICS
    /**
     * @brief 同步 ECS 组件到 Bullet（添加/更新/移除刚体）
     */
    void SyncECSToBullet();
    
    /**
     * @brief 同步 Bullet 结果到 ECS（位置、旋转、速度等）
     */
    void SyncBulletToECS();
    
    /**
     * @brief TransformComponent变化事件处理
     * @param entity 实体ID
     * @param transformComp TransformComponent引用
     * 
     * @note 此方法在TransformComponent变化时被调用
     * @note 只同步Kinematic/Static物体，Dynamic物体由物理模拟驱动
     * 
     * @see 阶段三 3.2 变化处理逻辑
     */
    void OnTransformComponentChanged(ECS::EntityID entity, const ECS::TransformComponent& transformComp);
#endif
    
private:
    ECS::World* m_ecsWorld;
    PhysicsConfig m_config;
    
    // 物理-渲染同步器
    std::unique_ptr<PhysicsTransformSync> m_transformSync;
    
#ifdef USE_BULLET_PHYSICS
    // ==================== 3.1 条件编译支持 ====================
    // Bullet 物理引擎适配器（当 USE_BULLET_PHYSICS 定义时使用）
    std::unique_ptr<BulletAdapter::BulletWorldAdapter> m_bulletAdapter;
    
    // 标记是否使用 Bullet 后端
    bool m_useBulletBackend = true;
    
    // ==================== 3.1.1 Transform变化事件回调ID ====================
    // 用于取消注册TransformComponent变化事件回调
    uint64_t m_transformChangeCallbackId = 0;
    
    // ==================== 3.3.2 性能统计（可选，仅在DEBUG模式下使用）====================
    #ifdef DEBUG
    struct TransformSyncStats {
        size_t totalSyncs = 0;           ///< 总同步次数
        size_t kinematicSyncs = 0;       ///< Kinematic物体同步次数
        size_t staticSyncs = 0;          ///< Static物体同步次数
        size_t skippedDynamic = 0;       ///< 跳过的Dynamic物体数量
        size_t skippedNoRigidBody = 0;   ///< 跳过的不带刚体的实体数量
        
        void Reset() {
            totalSyncs = 0;
            kinematicSyncs = 0;
            staticSyncs = 0;
            skippedDynamic = 0;
            skippedNoRigidBody = 0;
        }
    };
    TransformSyncStats m_transformSyncStats;  ///< Transform同步统计信息
    #endif
#else
    // 原有实现保持不变（向后兼容）
#endif
};

} // namespace Physics
} // namespace Render

