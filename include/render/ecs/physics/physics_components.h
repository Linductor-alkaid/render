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

#include "render/types.h"
#include "render/ecs/entity.h"
#include <string>

namespace Render {
namespace ECS {

// ============================================================
// 碰撞组常量（预定义）
// ============================================================

/**
 * @brief 预定义的碰撞组
 * 
 * 使用位掩码，可以组合多个组
 */
namespace CollisionGroups {
    constexpr uint16_t DEFAULT = 0x0001;      ///< 默认组
    constexpr uint16_t STATIC = 0x0002;       ///< 静态物体组
    constexpr uint16_t KINEMATIC = 0x0004;    ///< 运动学物体组
    constexpr uint16_t DYNAMIC = 0x0008;      ///< 动态物体组
    constexpr uint16_t PLAYER = 0x0010;       ///< 玩家组
    constexpr uint16_t ENEMY = 0x0020;        ///< 敌人组
    constexpr uint16_t PROJECTILE = 0x0040;   ///< 投射物组
    constexpr uint16_t TRIGGER = 0x0080;      ///< 触发器组
    constexpr uint16_t SENSOR = 0x0100;       ///< 传感器组
    constexpr uint16_t ALL = 0xFFFF;          ///< 所有组
    constexpr uint16_t NONE = 0x0000;         ///< 无组
}

// ============================================================
// 刚体类型
// ============================================================

/**
 * @brief 刚体类型
 */
enum class RigidBodyType {
    Static,      ///< 静态刚体（不受物理影响，但可以碰撞）
    Dynamic,     ///< 动态刚体（受物理影响）
    Kinematic    ///< 运动学刚体（不受物理影响，但可以移动）
};

// ============================================================
// RigidBody 组件
// ============================================================

/**
 * @brief 刚体组件
 * 
 * 为实体添加物理刚体特性
 * 需要配合 TransformComponent 使用
 */
struct RigidBodyComponent {
    // ==================== 基本属性 ====================
    RigidBodyType type = RigidBodyType::Dynamic;  ///< 刚体类型
    
    float mass = 1.0f;                             ///< 质量（kg）
    Vector3 linearVelocity{0, 0, 0};              ///< 线性速度
    Vector3 angularVelocity{0, 0, 0};             ///< 角速度
    
    // ==================== 物理属性 ====================
    float friction = 0.5f;                        ///< 摩擦系数（会被材质覆盖）
    float restitution = 0.0f;                    ///< 弹性系数（反弹，会被材质覆盖）
    float linearDamping = 0.0f;                   ///< 线性阻尼
    float angularDamping = 0.0f;                  ///< 角阻尼
    
    // ==================== 材质 ====================
    std::string materialName;                     ///< 材质名称（可选，如果指定则使用材质的属性）
    
    // ==================== 控制标志 ====================
    bool enabled = true;                          ///< 是否启用物理模拟
    bool useGravity = true;                        ///< 是否受重力影响
    bool isKinematic = false;                     ///< 是否为运动学模式（手动控制位置）
    
    // ==================== 同步控制 ====================
    /**
     * @brief 同步模式
     * 
     * - PhysicsToTransform: 物理模拟结果同步到 Transform（默认）
     * - TransformToPhysics: Transform 变化同步到物理体（用于运动学）
     * - Manual: 手动控制，不自动同步
     */
    enum class SyncMode {
        PhysicsToTransform,   ///< 物理驱动 Transform
        TransformToPhysics,   ///< Transform 驱动物理
        Manual                ///< 手动控制
    };
    
    SyncMode syncMode = SyncMode::PhysicsToTransform;
    
    // ==================== 内部状态 ====================
    void* bulletRigidBody = nullptr;              ///< Bullet 刚体指针（内部使用）
    bool needsSync = true;                         ///< 是否需要同步（内部使用）
    
    RigidBodyComponent() = default;
    
    // ==================== 便捷方法 ====================
    
    /**
     * @brief 应用力
     * @param force 力向量
     * @note 需要 PhysicsSystem 来处理
     */
    void ApplyForce(const Vector3& force);
    
    /**
     * @brief 应用冲量
     * @param impulse 冲量向量
     * @note 需要 PhysicsSystem 来处理
     */
    void ApplyImpulse(const Vector3& impulse);
    
    /**
     * @brief 应用扭矩
     * @param torque 扭矩向量
     * @note 需要 PhysicsSystem 来处理
     */
    void ApplyTorque(const Vector3& torque);
    
    /**
     * @brief 设置速度
     * @param linear 线性速度
     * @param angular 角速度
     */
    void SetVelocity(const Vector3& linear, const Vector3& angular);
    
    /**
     * @brief 清除所有力
     * @note 需要 PhysicsSystem 来处理
     */
    void ClearForces();
};

// ============================================================
// 碰撞体形状类型
// ============================================================

/**
 * @brief 碰撞体形状类型
 */
enum class ColliderShape {
    Box,          ///< 盒子
    Sphere,       ///< 球体
    Capsule,      ///< 胶囊体
    Cylinder,     ///< 圆柱体
    Cone,         ///< 圆锥体
    Mesh,         ///< 网格
    Plane         ///< 平面
};

// ============================================================
// Collider 组件
// ============================================================

/**
 * @brief 碰撞体组件
 * 
 * 定义实体的碰撞形状
 * 需要配合 RigidBodyComponent 使用（或单独使用作为触发器）
 */
struct ColliderComponent {
    // ==================== 形状属性 ====================
    ColliderShape shape = ColliderShape::Box;     ///< 碰撞体形状
    
    // Box 参数
    Vector3 boxSize{1, 1, 1};                      ///< 盒子尺寸
    
    // Sphere 参数
    float sphereRadius = 0.5f;                    ///< 球体半径
    
    // Capsule 参数
    float capsuleRadius = 0.5f;                   ///< 胶囊体半径
    float capsuleHeight = 1.0f;                   ///< 胶囊体高度
    
    // Cylinder/Cone 参数
    Vector3 cylinderSize{1, 1, 1};                ///< 圆柱/圆锥尺寸
    
    // Mesh 参数
    std::string meshName;                         ///< 网格资源名称（用于网格碰撞体）
    bool useConvexHull = true;                    ///< 是否使用凸包（否则使用三角网格）
    
    // Plane 参数
    Vector3 planeNormal{0, 1, 0};                  ///< 平面法线
    float planeConstant = 0.0f;                    ///< 平面常数
    
    // ==================== 偏移和旋转 ====================
    Vector3 offset{0, 0, 0};                       ///< 相对于 Transform 的偏移
    Quaternion rotation{1, 0, 0, 0};              ///< 相对于 Transform 的旋转
    
    // ==================== 触发器 ====================
    bool isTrigger = false;                       ///< 是否为触发器（不产生物理响应）
    
    // ==================== 材质 ====================
    std::string materialName;                     ///< 材质名称（可选，如果指定则使用材质的属性）
    
    // ==================== 碰撞过滤 ====================
    uint16_t collisionGroup = 0x0001;             ///< 碰撞组（位掩码）
    uint16_t collisionMask = 0xFFFF;              ///< 碰撞遮罩（与哪些组碰撞）
    
    // ==================== 内部状态 ====================
    void* bulletCollisionShape = nullptr;         ///< Bullet 碰撞形状指针（内部使用）
    bool needsUpdate = true;                      ///< 是否需要更新（内部使用）
    
    ColliderComponent() = default;
    
    // ==================== 便捷方法 ====================
    
    /**
     * @brief 设置盒子碰撞体
     */
    void SetBox(const Vector3& size) {
        shape = ColliderShape::Box;
        boxSize = size;
        needsUpdate = true;
    }
    
    /**
     * @brief 设置球体碰撞体
     */
    void SetSphere(float radius) {
        shape = ColliderShape::Sphere;
        sphereRadius = radius;
        needsUpdate = true;
    }
    
    /**
     * @brief 设置胶囊体碰撞体
     */
    void SetCapsule(float radius, float height) {
        shape = ColliderShape::Capsule;
        capsuleRadius = radius;
        capsuleHeight = height;
        needsUpdate = true;
    }
    
    /**
     * @brief 设置网格碰撞体
     */
    void SetMesh(const std::string& name, bool convexHull = true) {
        shape = ColliderShape::Mesh;
        meshName = name;
        useConvexHull = convexHull;
        needsUpdate = true;
    }
};

// ============================================================
// 约束类型
// ============================================================

/**
 * @brief 约束类型
 */
enum class ConstraintType {
    PointToPoint,     ///< 点对点约束
    Hinge,            ///< 铰链约束
    Slider,           ///< 滑动约束
    ConeTwist,        ///< 圆锥扭转约束
    Generic6Dof,      ///< 6自由度约束
    Generic6DofSpring ///< 6自由度弹簧约束
};

// ============================================================
// Constraint 组件
// ============================================================

/**
 * @brief 约束组件
 * 
 * 连接两个刚体的约束
 */
struct ConstraintComponent {
    ConstraintType type = ConstraintType::PointToPoint;
    
    EntityID connectedEntity = EntityID::Invalid();  ///< 连接的实体ID
    
    // 约束点（本地空间）
    Vector3 pivotA{0, 0, 0};                          ///< 实体A的约束点
    Vector3 pivotB{0, 0, 0};                          ///< 实体B的约束点
    
    // 约束轴（铰链、滑动等）
    Vector3 axisA{1, 0, 0};                           ///< 实体A的轴
    Vector3 axisB{1, 0, 0};                           ///< 实体B的轴
    
    // 限制（角度/距离）
    float lowerLimit = 0.0f;                          ///< 下限
    float upperLimit = 0.0f;                          ///< 上限
    
    // 弹簧参数（Generic6DofSpring）
    bool enableSpring = false;                        ///< 是否启用弹簧
    float springStiffness = 0.0f;                    ///< 弹簧刚度
    float springDamping = 0.0f;                       ///< 弹簧阻尼
    
    // ==================== 马达控制（用于关节驱动）====================
    
    /**
     * @brief 是否启用马达
     * 
     * 启用后，约束会尝试达到目标速度
     */
    bool useMotor = false;                            ///< 是否启用马达
    
    /**
     * @brief 目标速度
     * 
     * 对于旋转关节：角速度（rad/s）
     * 对于平移关节：线速度（m/s）
     */
    float motorTargetVelocity = 0.0f;                 ///< 目标速度（rad/s 或 m/s）
    
    /**
     * @brief 最大马达力/力矩
     * 
     * 对于旋转关节：最大力矩（N·m）
     * 对于平移关节：最大力（N）
     */
    float motorMaxForce = 0.0f;                      ///< 最大马达力/力矩（N 或 N·m）
    
    // ==================== 位置控制（用于位置控制模式）====================
    
    /**
     * @brief 是否启用位置控制
     * 
     * 启用后，使用PD控制器计算目标速度以达到目标位置
     */
    bool usePositionControl = false;                  ///< 是否启用位置控制
    
    /**
     * @brief 目标位置
     * 
     * 对于旋转关节：目标角度（rad）
     * 对于平移关节：目标距离（m）
     */
    float targetPosition = 0.0f;                      ///< 目标位置（角度或距离）
    
    /**
     * @brief 位置比例增益（PD控制器的Kp）
     */
    float positionKp = 100.0f;                       ///< 位置比例增益
    
    /**
     * @brief 位置微分增益（PD控制器的Kd）
     */
    float positionKd = 10.0f;                         ///< 位置微分增益
    
    bool enabled = true;                              ///< 是否启用
    
    void* bulletConstraint = nullptr;                 ///< Bullet 约束指针（内部使用）
    
    ConstraintComponent() = default;
};

// ============================================================
// PhysicsWorld 组件
// ============================================================

/**
 * @brief 物理世界组件
 * 
 * 标记实体为物理世界根节点
 * 一个 World 中应该只有一个实体拥有此组件
 */
struct PhysicsWorldComponent {
    Vector3 gravity{0, -9.81f, 0};      ///< 重力加速度
    float timeStep = 1.0f / 60.0f;      ///< 固定时间步长（秒）
    int maxSubSteps = 10;               ///< 最大子步数
    
    bool enabled = true;                ///< 是否启用物理模拟
    
    // 内部状态
    void* bulletWorld = nullptr;        ///< Bullet 世界指针（内部使用）
    
    PhysicsWorldComponent() = default;
};

} // namespace ECS
} // namespace Render
