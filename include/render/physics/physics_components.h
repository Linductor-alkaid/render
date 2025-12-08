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
#include "render/physics/dynamics/joint_component.h"
#include <variant>
#include <memory>
#include <limits>

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
    Vector3 centerOfMass{0.0f, 0.0f, 0.0f};
    
    /// 惯性张量（局部空间）
    Matrix3 inertiaTensor = Matrix3::Identity();
    
    /// 逆惯性张量（局部空间），用于优化计算
    Matrix3 inverseInertiaTensor = Matrix3::Identity();
    
    // ==================== 运动状态 ====================
    
    /// 线速度 (m/s)
    Vector3 linearVelocity{0.0f, 0.0f, 0.0f};
    
    /// 角速度 (rad/s)
    Vector3 angularVelocity{0.0f, 0.0f, 0.0f};
    
    /// 累积的力 (N)
    Vector3 force{0.0f, 0.0f, 0.0f};
    
    /// 累积的扭矩 (N·m)
    Vector3 torque{0.0f, 0.0f, 0.0f};
    
    // ==================== 阻尼 ====================
    
    /// 线性阻尼 [0, 1]，模拟空气阻力等
    float linearDamping = 0.01f;
    
    /// 角阻尼 [0, 1]，模拟旋转阻力
    float angularDamping = 0.05f;
    
    // ==================== 速度约束 ====================
    
    /// 最大线速度（m/s），默认无限制
    float maxLinearSpeed = std::numeric_limits<float>::infinity();
    
    /// 最大角速度（rad/s），默认无限制
    float maxAngularSpeed = std::numeric_limits<float>::infinity();
    
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
    Vector3 previousPosition{0.0f, 0.0f, 0.0f};
    
    /// 上一帧的旋转
    Quaternion previousRotation{1.0f, 0.0f, 0.0f, 0.0f}; // w, x, y, z
    
    // ==================== 构造函数 ====================
    
    /**
     * @brief 默认构造函数，确保所有 Eigen 类型正确初始化
     */
    RigidBodyComponent() {
        // 显式初始化所有 Vector3 和 Matrix3（防止未初始化内存）
        centerOfMass.setZero();
        linearVelocity.setZero();
        angularVelocity.setZero();
        force.setZero();
        torque.setZero();
        previousPosition.setZero();
        
        // 初始化矩阵
        inertiaTensor.setIdentity();
        inverseInertiaTensor.setIdentity();
        
        // 初始化四元数
        previousRotation.setIdentity();
    }
    
    // ==================== 辅助方法 ====================
    
    /**
     * @brief 设置质量并自动计算逆质量
     * 
     * @param m 质量 (kg)，如果 <= 0 或物体为 Static，则设置为无限质量（inverseMass = 0）
     * 
     * @note 简单版本：只更新质量和逆质量，不修改惯性张量
     * @note 如果需要更新惯性张量，请在设置质量后调用 SetInertiaTensorFromShape()
     */
    void SetMass(float m) {
        if (type == BodyType::Static || m <= 0.0f) {
            // 静态物体或无效质量：无限质量
            mass = 0.0f;
            inverseMass = 0.0f;
            // 注意：不修改惯性张量，让 SetBodyType 或 SetInertiaTensor 来处理
        } else {
            // 更新质量
            mass = m;
            inverseMass = 1.0f / m;
            // 注意：不自动修改惯性张量
            // 惯性张量应该通过 SetInertiaTensorFromShape() 显式设置
        }
    }
    
    /**
     * @brief 设置惯性张量（局部空间）
     * 
     * @param inertia 惯性张量（对角矩阵或完整的 3x3 矩阵）
     * 
     * @note 对于常见形状，可以使用以下公式计算惯性张量：
     * - 球体：I = (2/5) * m * r²
     * - 立方体：I = (1/6) * m * size²
     * - 圆柱体：I_axis = (1/2) * m * r², I_perp = (1/12) * m * (3r² + h²)
     */
    void SetInertiaTensor(const Matrix3& inertia) {
        inertiaTensor = inertia;
        
        // 计算逆惯性张量
        if (type == BodyType::Static || mass <= 0.0f) {
            inverseInertiaTensor = Matrix3::Zero();
        } else {
            float determinant = inertiaTensor.determinant();
            if (std::abs(determinant) > 1e-6f) {
                inverseInertiaTensor = inertiaTensor.inverse();
            } else {
                // 回退到单位矩阵
                inverseInertiaTensor = Matrix3::Identity();
            }
        }
    }
    
    /**
     * @brief 根据几何形状自动设置惯性张量
     * 
     * @param shapeType 形状类型
     * @param dimensions 形状尺寸参数
     *                   - 球体: [radius, 0, 0]
     *                   - 立方体: [width, height, depth]
     *                   - 圆柱体: [radius, height, 0]
     */
    void SetInertiaTensorFromShape(const std::string& shapeType, const Vector3& dimensions) {
        Matrix3 inertia = Matrix3::Identity();
        
        if (shapeType == "sphere") {
            // 球体：I = (2/5) * m * r²
            float r = dimensions.x();
            float I = (2.0f / 5.0f) * mass * r * r;
            inertia = Matrix3::Identity() * I;
            
        } else if (shapeType == "box") {
            // 立方体：I_x = (1/12) * m * (h² + d²), 其他轴类似
            float w = dimensions.x();
            float h = dimensions.y();
            float d = dimensions.z();
            inertia(0, 0) = (1.0f / 12.0f) * mass * (h * h + d * d);
            inertia(1, 1) = (1.0f / 12.0f) * mass * (w * w + d * d);
            inertia(2, 2) = (1.0f / 12.0f) * mass * (w * w + h * h);
            
        } else if (shapeType == "cylinder") {
            // 圆柱体（Y 轴为对称轴）
            float r = dimensions.x();
            float h = dimensions.y();
            inertia(0, 0) = (1.0f / 12.0f) * mass * (3.0f * r * r + h * h);
            inertia(1, 1) = (1.0f / 2.0f) * mass * r * r;  // 沿轴
            inertia(2, 2) = (1.0f / 12.0f) * mass * (3.0f * r * r + h * h);
            
        } else {
            // 默认：单位立方体
            float I = (1.0f / 6.0f) * mass;
            inertia = Matrix3::Identity() * I;
        }
        
        SetInertiaTensor(inertia);
    }
    
    /**
     * @brief 设置刚体类型
     * 
     * @note 改变类型会自动更新质量相关属性
     */
    void SetBodyType(BodyType newType) {
        type = newType;
        
        // 静态物体：无限质量
        if (type == BodyType::Static) {
            inverseMass = 0.0f;
            inverseInertiaTensor = Matrix3::Zero();
            linearVelocity = Vector3::Zero();
            angularVelocity = Vector3::Zero();
        } 
        // 动态物体：恢复正常质量
        else if (type == BodyType::Dynamic && mass > 0.0f) {
            inverseMass = 1.0f / mass;
            if (!inertiaTensor.isZero()) {
                inverseInertiaTensor = inertiaTensor.inverse();
            }
        }
        // 运动学物体：保持质量但不受力影响（在物理更新中处理）
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
    
    /**
     * @brief 获取当前动能
     * 
     * @return 动能 (J)
     */
    float GetKineticEnergy() const {
        float linearKE = 0.5f * mass * linearVelocity.squaredNorm();
        float angularKE = 0.5f * angularVelocity.dot(inertiaTensor * angularVelocity);
        return linearKE + angularKE;
    }
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

// ============================================================================
// 力场组件
// ============================================================================

/**
 * @brief 力场组件
 * 
 * 在一定范围内对刚体施加力，模拟重力场、风场、径向力场、涡流等效果
 * 
 * 使用示例：
 * @code
 * // 创建一个向下的重力场
 * ForceFieldComponent gravityField;
 * gravityField.type = ForceFieldComponent::Type::Gravity;
 * gravityField.direction = Vector3(0, -1, 0);
 * gravityField.strength = 20.0f;
 * gravityField.radius = 10.0f;
 * 
 * // 创建一个径向吸引力场（黑洞效果）
 * ForceFieldComponent blackHole;
 * blackHole.type = ForceFieldComponent::Type::Radial;
 * blackHole.strength = -50.0f;  // 负值表示吸引
 * blackHole.radius = 15.0f;
 * blackHole.linearFalloff = false;  // 使用平方反比衰减
 * 
 * // 创建一个旋转的涡流场
 * ForceFieldComponent vortex;
 * vortex.type = ForceFieldComponent::Type::Vortex;
 * vortex.direction = Vector3(0, 1, 0);  // 旋转轴
 * vortex.strength = 30.0f;
 * vortex.radius = 8.0f;
 * @endcode
 */
struct ForceFieldComponent {
    /**
     * @brief 力场类型
     */
    enum class Type {
        /**
         * @brief 重力场/定向力场
         * 
         * 在指定方向上施加恒定的力
         * 使用 direction 和 strength 参数
         * 
         * 示例：模拟行星引力、风力
         */
        Gravity,
        
        /**
         * @brief 风场（与 Gravity 相同，语义上的区别）
         * 
         * 持续的定向力，通常用于模拟环境效果
         * 使用 direction 和 strength 参数
         */
        Wind,
        
        /**
         * @brief 径向力场
         * 
         * 从力场中心向外（或向内）的径向力
         * strength > 0: 排斥力（爆炸效果）
         * strength < 0: 吸引力（黑洞效果）
         * 
         * 力的方向：从力场中心指向物体（排斥）或相反（吸引）
         */
        Radial,
        
        /**
         * @brief 涡流场
         * 
         * 围绕指定轴旋转的力
         * 使用 direction 作为旋转轴
         * 力的方向：垂直于径向和旋转轴的切线方向
         * 
         * 示例：龙卷风、漩涡
         */
        Vortex
    };
    
    // ==================== 基本属性 ====================
    
    /// 力场类型
    Type type = Type::Gravity;
    
    /// 力场方向（用于 Gravity/Wind/Vortex）
    /// - Gravity/Wind: 力的方向
    /// - Vortex: 旋转轴方向（应为单位向量）
    /// - Radial: 不使用
    Vector3 direction = Vector3(0.0f, -1.0f, 0.0f);
    
    /// 力场强度
    /// - 单位：N（牛顿）每千克质量
    /// - 正值：排斥/推力
    /// - 负值：吸引/拉力
    float strength = 10.0f;
    
    // ==================== 影响范围 ====================
    
    /// 力场影响半径（世界空间单位）
    /// - 如果 radius <= 0，则影响整个场景（无限范围）
    /// - 如果 radius > 0，则仅影响半径内的物体
    float radius = 0.0f;
    
    /// 是否仅影响范围内的物体
    /// - true: 仅影响 radius 内的物体
    /// - false: 影响所有物体，但在 radius 外衰减
    bool affectOnlyInside = true;
    
    // ==================== 衰减设置 ====================
    
    /// 是否使用线性衰减
    /// - true: 线性衰减 falloff = 1 - (distance / radius)
    /// - false: 平方反比衰减 falloff = 1 / (1 + distance²)
    /// 
    /// 仅在 radius > 0 时生效
    bool linearFalloff = true;
    
    /// 最小衰减系数 [0, 1]
    /// 即使物体在力场中心，力也会乘以这个最小值
    /// 用于避免力场中心的力过大
    float minFalloff = 0.0f;
    
    // ==================== 层级过滤 ====================
    
    /// 力场影响的碰撞层（位掩码）
    /// 仅影响与该掩码匹配的物体
    /// 默认 0xFFFFFFFF 表示影响所有层
    uint32_t affectLayers = 0xFFFFFFFF;
    
    // ==================== 开关 ====================
    
    /// 力场是否启用
    bool enabled = true;
    
    // ==================== 辅助方法 ====================
    
    /**
     * @brief 创建重力场
     * 
     * @param dir 重力方向（会自动归一化）
     * @param str 重力强度
     * @param r 影响半径（0 表示无限）
     * @return 配置好的重力场组件
     */
    static ForceFieldComponent CreateGravityField(
        const Vector3& dir = Vector3(0.0f, -1.0f, 0.0f),
        float str = 9.81f,
        float r = 0.0f
    ) {
        ForceFieldComponent field;
        field.type = Type::Gravity;
        field.direction = dir.normalized();
        field.strength = str;
        field.radius = r;
        field.affectOnlyInside = (r > 0.0f);
        return field;
    }
    
    /**
     * @brief 创建风场
     * 
     * @param dir 风向（会自动归一化）
     * @param str 风力强度
     * @param r 影响半径（0 表示无限）
     * @return 配置好的风场组件
     */
    static ForceFieldComponent CreateWindField(
        const Vector3& dir,
        float str = 5.0f,
        float r = 0.0f
    ) {
        ForceFieldComponent field;
        field.type = Type::Wind;
        field.direction = dir.normalized();
        field.strength = str;
        field.radius = r;
        field.affectOnlyInside = (r > 0.0f);
        field.linearFalloff = true;
        return field;
    }
    
    /**
     * @brief 创建径向力场
     * 
     * @param str 力场强度（正值=排斥，负值=吸引）
     * @param r 影响半径
     * @param useSquareFalloff 是否使用平方反比衰减
     * @return 配置好的径向力场组件
     */
    static ForceFieldComponent CreateRadialField(
        float str = 10.0f,
        float r = 10.0f,
        bool useSquareFalloff = true
    ) {
        ForceFieldComponent field;
        field.type = Type::Radial;
        field.strength = str;
        field.radius = r;
        field.affectOnlyInside = true;
        field.linearFalloff = !useSquareFalloff;
        return field;
    }
    
    /**
     * @brief 创建涡流场
     * 
     * @param axis 旋转轴（会自动归一化）
     * @param str 旋转强度
     * @param r 影响半径
     * @return 配置好的涡流场组件
     */
    static ForceFieldComponent CreateVortexField(
        const Vector3& axis = Vector3(0.0f, 1.0f, 0.0f),
        float str = 15.0f,
        float r = 8.0f
    ) {
        ForceFieldComponent field;
        field.type = Type::Vortex;
        field.direction = axis.normalized();
        field.strength = str;
        field.radius = r;
        field.affectOnlyInside = true;
        field.linearFalloff = true;
        return field;
    }
    
    /**
     * @brief 创建爆炸力场（短时径向排斥力）
     * 
     * @param str 爆炸强度
     * @param r 爆炸半径
     * @return 配置好的爆炸力场组件
     */
    static ForceFieldComponent CreateExplosionField(
        float str = 100.0f,
        float r = 5.0f
    ) {
        ForceFieldComponent field;
        field.type = Type::Radial;
        field.strength = str;
        field.radius = r;
        field.affectOnlyInside = true;
        field.linearFalloff = true;
        return field;
    }
    
    /**
     * @brief 设置影响范围
     * 
     * @param r 半径
     * @param onlyInside 是否仅影响范围内
     */
    void SetRadius(float r, bool onlyInside = true) {
        radius = r;
        affectOnlyInside = onlyInside;
    }
    
    /**
     * @brief 设置衰减模式
     * 
     * @param linear true=线性衰减，false=平方反比衰减
     * @param minFalloffFactor 最小衰减系数 [0, 1]
     */
    void SetFalloff(bool linear, float minFalloffFactor = 0.0f) {
        linearFalloff = linear;
        minFalloff = std::max(0.0f, std::min(1.0f, minFalloffFactor));
    }
    
    /**
     * @brief 设置影响层级
     * 
     * @param layers 碰撞层位掩码
     */
    void SetAffectLayers(uint32_t layers) {
        affectLayers = layers;
    }
    
    /**
     * @brief 启用/禁用力场
     */
    void SetEnabled(bool enable) {
        enabled = enable;
    }
    
    /**
     * @brief 检查是否影响指定层级
     * 
     * @param layer 物体的碰撞层
     * @return true 如果力场影响该层级
     */
    bool AffectsLayer(uint32_t layer) const {
        return (affectLayers & (1u << layer)) != 0;
    }
};

// ============================================================================
// 关节组件
// ============================================================================

/**
 * @brief 物理关节组件
 * 
 * 包含关节基础信息和类型特定的数据
 * 
 * @note 包含 Eigen 类型，需要对齐
 */
struct PhysicsJointComponent {
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    
    JointComponent base;
    
    // 使用 variant 存储不同类型关节的专用数据
    std::variant<
        FixedJointData,
        HingeJointData,
        DistanceJointData,
        SpringJointData,
        SliderJointData
    > data;
    
    // 运行时数据（缓存）
    struct RuntimeData {
        Vector3 rA = Vector3::Zero();
        Vector3 rB = Vector3::Zero();
        Vector3 worldAxis = Vector3::UnitZ();
        Matrix3 invInertiaA = Matrix3::Zero();
        Matrix3 invInertiaB = Matrix3::Zero();
        
        // 累积冲量（用于 Warm Start）
        Vector3 accumulatedLinearImpulse = Vector3::Zero();
        Vector3 accumulatedAngularImpulse = Vector3::Zero();
        float accumulatedLimitImpulse = 0.0f;
        float accumulatedMotorImpulse = 0.0f;
    } runtime;
    
    /**
     * @brief 默认构造函数
     */
    PhysicsJointComponent() {
        base.type = JointComponent::JointType::Fixed;
        data = FixedJointData();
        runtime.rA.setZero();
        runtime.rB.setZero();
        runtime.worldAxis = Vector3::UnitZ();
        runtime.invInertiaA.setZero();
        runtime.invInertiaB.setZero();
        runtime.accumulatedLinearImpulse.setZero();
        runtime.accumulatedAngularImpulse.setZero();
        runtime.accumulatedLimitImpulse = 0.0f;
        runtime.accumulatedMotorImpulse = 0.0f;
    }
};

} // namespace Physics
} // namespace Render

