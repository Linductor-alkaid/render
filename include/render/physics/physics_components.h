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
#include <memory>

namespace Render {

// 前向声明
class Mesh;

namespace Physics {

// ============================================================================
// 物理材质
// ============================================================================

/**
 * @brief 物理材质
 * 
 * 定义物体表面的物理属性（摩擦、弹性等）
 */
struct PhysicsMaterial {
    /// 摩擦系数 [0, 1]，0 = 无摩擦，1 = 高摩擦
    float friction = 0.5f;
    
    /// 弹性系数（恢复系数）[0, 1]，0 = 完全非弹性，1 = 完全弹性
    float restitution = 0.3f;
    
    /// 密度 (kg/m³)，用于计算质量
    float density = 1.0f;
    
    /**
     * @brief 属性组合模式
     * 
     * 当两个物体碰撞时，如何组合它们的物理属性
     */
    enum class CombineMode {
        Average,   // 取平均值
        Minimum,   // 取最小值
        Maximum,   // 取最大值
        Multiply   // 相乘
    };
    
    CombineMode frictionCombine = CombineMode::Average;
    CombineMode restitutionCombine = CombineMode::Average;
    
    /**
     * @brief 组合两个材质的属性值
     */
    static float CombineValues(float a, float b, CombineMode mode) {
        switch (mode) {
            case CombineMode::Average:  return (a + b) * 0.5f;
            case CombineMode::Minimum:  return std::min(a, b);
            case CombineMode::Maximum:  return std::max(a, b);
            case CombineMode::Multiply: return a * b;
            default: return (a + b) * 0.5f;
        }
    }
    
    // 预定义材质
    static PhysicsMaterial Default() { return PhysicsMaterial(); }
    static PhysicsMaterial Rubber() { 
        PhysicsMaterial mat;
        mat.friction = 0.8f;
        mat.restitution = 0.9f;
        mat.density = 1.1f;
        return mat;
    }
    static PhysicsMaterial Ice() {
        PhysicsMaterial mat;
        mat.friction = 0.05f;
        mat.restitution = 0.1f;
        mat.density = 0.9f;
        return mat;
    }
    static PhysicsMaterial Metal() {
        PhysicsMaterial mat;
        mat.friction = 0.4f;
        mat.restitution = 0.3f;
        mat.density = 7.8f;
        return mat;
    }
};

// ============================================================================
// 刚体组件
// ============================================================================

/**
 * @brief 刚体组件
 * 
 * 为实体添加物理动力学行为
 * 
 * @note 包含 Eigen 类型，需要对齐
 */
struct RigidBodyComponent {
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    
    /**
     * @brief 刚体类型
     */
    enum class BodyType {
        Static,    // 静态物体：不移动，不受力影响（如地面、墙壁）
        Kinematic, // 运动学物体：可通过脚本移动，不受力影响（如移动平台）
        Dynamic    // 动态物体：受力影响，完整物理模拟（如球、箱子）
    };
    
    // ==================== 基本属性 ====================
    
    /// 刚体类型
    BodyType type = BodyType::Dynamic;
    
    /// 质量 (kg)
    float mass = 1.0f;
    
    /// 逆质量（1/mass），用于优化计算
    /// Static 物体的逆质量为 0
    float inverseMass = 1.0f;
    
    /// 质心（局部空间）
    Vector3 centerOfMass = Vector3::Zero();
    
    /// 惯性张量（局部空间）
    Matrix3 inertiaTensor = Matrix3::Identity();
    
    /// 逆惯性张量（局部空间），用于优化计算
    Matrix3 inverseInertiaTensor = Matrix3::Identity();
    
    // ==================== 运动状态 ====================
    
    /// 线速度 (m/s)
    Vector3 linearVelocity = Vector3::Zero();
    
    /// 角速度 (rad/s)
    Vector3 angularVelocity = Vector3::Zero();
    
    /// 累积的力 (N)
    Vector3 force = Vector3::Zero();
    
    /// 累积的扭矩 (N·m)
    Vector3 torque = Vector3::Zero();
    
    // ==================== 阻尼 ====================
    
    /// 线性阻尼 [0, 1]，模拟空气阻力等
    float linearDamping = 0.01f;
    
    /// 角阻尼 [0, 1]，模拟旋转阻力
    float angularDamping = 0.05f;
    
    // ==================== 约束 ====================
    
    /// 位置锁定（X, Y, Z 轴）
    bool lockPosition[3] = {false, false, false};
    
    /// 旋转锁定（X, Y, Z 轴）
    bool lockRotation[3] = {false, false, false};
    
    // ==================== 重力 ====================
    
    /// 是否受重力影响
    bool useGravity = true;
    
    /// 重力缩放因子
    float gravityScale = 1.0f;
    
    // ==================== 休眠 ====================
    
    /// 是否处于休眠状态
    bool isSleeping = false;
    
    /// 休眠阈值（动能）
    float sleepThreshold = 0.01f;
    
    /// 休眠计时器
    float sleepTimer = 0.0f;
    
    // ==================== 插值数据（用于渲染平滑）====================
    
    /// 上一帧的位置
    Vector3 previousPosition = Vector3::Zero();
    
    /// 上一帧的旋转
    Quaternion previousRotation = Quaternion::Identity();
    
    // ==================== 辅助方法 ====================
    
    /**
     * @brief 设置质量并自动计算逆质量
     */
    void SetMass(float m) {
        mass = m;
        inverseMass = (type == BodyType::Static || m <= 0.0f) ? 0.0f : (1.0f / m);
    }
    
    /**
     * @brief 唤醒刚体（从休眠状态）
     */
    void WakeUp() {
        isSleeping = false;
        sleepTimer = 0.0f;
    }
    
    /**
     * @brief 是否为静态物体
     */
    bool IsStatic() const { return type == BodyType::Static; }
    
    /**
     * @brief 是否为运动学物体
     */
    bool IsKinematic() const { return type == BodyType::Kinematic; }
    
    /**
     * @brief 是否为动态物体
     */
    bool IsDynamic() const { return type == BodyType::Dynamic; }
};

// ============================================================================
// 碰撞体组件
// ============================================================================

/**
 * @brief 碰撞体组件
 * 
 * 定义物体的碰撞形状
 * 
 * @note 包含 Eigen 类型，需要对齐
 */
struct ColliderComponent {
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    
    /**
     * @brief 碰撞形状类型
     */
    enum class ShapeType {
        Sphere,     // 球体
        Box,        // 盒体
        Capsule,    // 胶囊体
        Mesh,       // 网格（凹面）
        ConvexHull  // 凸包
    };
    
    /// 形状类型
    ShapeType shapeType = ShapeType::Box;
    
    /**
     * @brief 形状数据（使用 union 节省内存）
     * 
     * @note 使用简单类型避免 Eigen 对齐问题
     * @note 网格数据单独存储，因为 shared_ptr 不能放在 union 中
     */
    union ShapeData {
        // 球体数据
        struct { 
            float radius; 
        } sphere;
        
        // 盒体数据（使用 float 数组代替 Vector3）
        struct { 
            float halfExtents[3];  // 半尺寸 [x, y, z]
        } box;
        
        // 胶囊体数据
        struct { 
            float radius;  // 半径
            float height;  // 高度（中心线长度）
        } capsule;
        
        // 默认构造（初始化为盒体）
        ShapeData() {
            box.halfExtents[0] = 0.5f;
            box.halfExtents[1] = 0.5f;
            box.halfExtents[2] = 0.5f;
        }
    } shapeData;
    
    // 网格数据（单独存储）
    std::shared_ptr<Mesh> meshData;
    bool useConvexHull = false;
    
    // ==================== 局部变换 ====================
    
    /// 碰撞体中心偏移（相对于实体位置）
    Vector3 center = Vector3::Zero();
    
    /// 碰撞体旋转偏移
    Quaternion rotation = Quaternion::Identity();
    
    // ==================== 碰撞属性 ====================
    
    /// 是否为触发器（不产生物理响应，仅触发事件）
    bool isTrigger = false;
    
    /// 碰撞层（0-31）
    int collisionLayer = 0;
    
    /// 碰撞掩码（与哪些层碰撞）
    uint32_t collisionMask = 0xFFFFFFFF;
    
    // ==================== AABB 缓存 ====================
    
    /// 世界空间 AABB
    AABB worldAABB;
    
    /// AABB 是否需要更新
    bool aabbDirty = true;
    
    // ==================== 物理材质 ====================
    
    /// 物理材质（可共享）
    std::shared_ptr<PhysicsMaterial> material;
    
    // ==================== 构造函数 ====================
    
    ColliderComponent() {
        material = std::make_shared<PhysicsMaterial>(PhysicsMaterial::Default());
    }
    
    // ==================== 辅助方法 ====================
    
    /**
     * @brief 创建球体碰撞体
     */
    static ColliderComponent CreateSphere(float radius) {
        ColliderComponent collider;
        collider.shapeType = ShapeType::Sphere;
        collider.shapeData.sphere.radius = radius;
        return collider;
    }
    
    /**
     * @brief 创建盒体碰撞体
     */
    static ColliderComponent CreateBox(const Vector3& halfExtents) {
        ColliderComponent collider;
        collider.shapeType = ShapeType::Box;
        collider.shapeData.box.halfExtents[0] = halfExtents.x();
        collider.shapeData.box.halfExtents[1] = halfExtents.y();
        collider.shapeData.box.halfExtents[2] = halfExtents.z();
        return collider;
    }
    
    /**
     * @brief 获取盒体半尺寸
     */
    Vector3 GetBoxHalfExtents() const {
        if (shapeType == ShapeType::Box) {
            return Vector3(
                shapeData.box.halfExtents[0],
                shapeData.box.halfExtents[1],
                shapeData.box.halfExtents[2]
            );
        }
        return Vector3::Zero();
    }
    
    /**
     * @brief 创建胶囊体碰撞体
     */
    static ColliderComponent CreateCapsule(float radius, float height) {
        ColliderComponent collider;
        collider.shapeType = ShapeType::Capsule;
        collider.shapeData.capsule.radius = radius;
        collider.shapeData.capsule.height = height;
        return collider;
    }
};

} // namespace Physics
} // namespace Render

