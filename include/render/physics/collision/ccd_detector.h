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
#include "render/physics/collision/collision_shapes.h"
#include "render/physics/physics_components.h"
#include <memory>

namespace Render {
namespace Physics {

/**
 * @brief CCD 检测结果
 */
struct CCDResult {
    /// 是否发生碰撞
    bool collided = false;
    
    /// Time of Impact [0, 1]，表示在时间步长内的碰撞时刻
    /// 0 表示在起始时刻碰撞，1 表示在结束时刻碰撞
    float toi = 1.0f;
    
    /// 碰撞点（世界空间）
    Vector3 collisionPoint{0.0f, 0.0f, 0.0f};
    
    /// 碰撞法线（从 B 指向 A，世界空间）
    Vector3 collisionNormal{0.0f, 0.0f, 0.0f};
    
    /// 穿透深度（如果已经相交）
    float penetration = 0.0f;
    
    /// 默认构造函数
    CCDResult() = default;
    
    /// 重置结果
    void Reset() {
        collided = false;
        toi = 1.0f;
        collisionPoint.setZero();
        collisionNormal.setZero();
        penetration = 0.0f;
    }
};

/**
 * @brief 连续碰撞检测器
 * 
 * 提供各种形状组合的连续碰撞检测功能
 */
class CCDDetector {
public:
    /**
     * @brief 检测两个形状在时间间隔内的碰撞
     * 
     * @param shapeA 形状 A
     * @param posA0 形状 A 初始位置（世界空间）
     * @param velA 形状 A 线速度
     * @param rotA0 形状 A 初始旋转
     * @param angularVelA 形状 A 角速度
     * 
     * @param shapeB 形状 B
     * @param posB0 形状 B 初始位置（世界空间）
     * @param velB 形状 B 线速度
     * @param rotB0 形状 B 初始旋转
     * @param angularVelB 形状 B 角速度
     * 
     * @param dt 时间步长
     * @param result 输出结果
     * 
     * @return true 如果发生碰撞
     */
    static bool Detect(
        const CollisionShape* shapeA,
        const Vector3& posA0, const Vector3& velA,
        const Quaternion& rotA0, const Vector3& angularVelA,
        const CollisionShape* shapeB,
        const Vector3& posB0, const Vector3& velB,
        const Quaternion& rotB0, const Vector3& angularVelB,
        float dt,
        CCDResult& result
    );
    
    // ============================================================================
    // 具体的形状组合算法（公共接口，用于测试和直接调用）
    // ============================================================================
    
    /**
     * @brief Sphere vs Sphere CCD 检测
     */
    static bool SphereVsSphereCCD(
        const Vector3& posA0, float radiusA, const Vector3& velA,
        const Vector3& posB0, float radiusB, const Vector3& velB,
        float dt,
        CCDResult& result
    );
    
    /**
     * @brief Sphere vs Box CCD 检测
     */
    static bool SphereVsBoxCCD(
        const Vector3& spherePos0, float sphereRadius, const Vector3& sphereVel,
        const Vector3& boxCenter, const Vector3& boxHalfExtents,
        const Quaternion& boxRotation, const Vector3& boxVel,
        float dt,
        CCDResult& result
    );
    
    /**
     * @brief Sphere vs Capsule CCD 检测
     */
    static bool SphereVsCapsuleCCD(
        const Vector3& spherePos0, float sphereRadius, const Vector3& sphereVel,
        const Vector3& capsuleCenter, float capsuleRadius, float capsuleHeight,
        const Quaternion& capsuleRotation, const Vector3& capsuleVel,
        float dt,
        CCDResult& result
    );
    
    /**
     * @brief Box vs Box CCD 检测（简化版）
     */
    static bool BoxVsBoxCCD(
        const Vector3& boxA0, const Vector3& boxAHalfExtents, 
        const Quaternion& boxARot0, const Vector3& boxAVel, const Vector3& boxAAngularVel,
        const Vector3& boxB0, const Vector3& boxBHalfExtents,
        const Quaternion& boxBRot0, const Vector3& boxBVel, const Vector3& boxBAngularVel,
        float dt,
        CCDResult& result
    );
    
    /**
     * @brief Capsule vs Capsule CCD 检测
     */
    static bool CapsuleVsCapsuleCCD(
        const Vector3& capsuleA0, float capsuleARadius, float capsuleAHeight,
        const Quaternion& capsuleARot0, const Vector3& capsuleAVel, const Vector3& capsuleAAngularVel,
        const Vector3& capsuleB0, float capsuleBRadius, float capsuleBHeight,
        const Quaternion& capsuleBRot0, const Vector3& capsuleBVel, const Vector3& capsuleBAngularVel,
        float dt,
        CCDResult& result
    );
    
    /**
     * @brief Capsule vs Box CCD 检测
     */
    static bool CapsuleVsBoxCCD(
        const Vector3& capsuleCenter, float capsuleRadius, float capsuleHeight,
        const Quaternion& capsuleRotation, const Vector3& capsuleVel, const Vector3& capsuleAngularVel,
        const Vector3& boxCenter, const Vector3& boxHalfExtents,
        const Quaternion& boxRotation, const Vector3& boxVel, const Vector3& boxAngularVel,
        float dt,
        CCDResult& result
    );
    
private:
    /**
     * @brief 分发到具体的形状组合算法
     */
    static bool Dispatch(
        ShapeType typeA, ShapeType typeB,
        const CollisionShape* shapeA, const CollisionShape* shapeB,
        const Vector3& posA0, const Vector3& velA,
        const Quaternion& rotA0, const Vector3& angularVelA,
        const Vector3& posB0, const Vector3& velB,
        const Quaternion& rotB0, const Vector3& angularVelB,
        float dt,
        CCDResult& result
    );
    
    // 注意：具体的算法实现在 public 部分，以便测试和直接调用
};

/**
 * @brief 快速移动物体检测工具
 * 
 * 判断物体是否应该使用 CCD
 */
class CCDCandidateDetector {
public:
    /**
     * @brief 判断物体是否应该使用 CCD
     * 
     * @param body 刚体组件
     * @param collider 碰撞体组件
     * @param dt 时间步长
     * @param velocityThreshold 速度阈值（m/s）
     * @param displacementThreshold 位移阈值（相对于形状尺寸的比例）
     * 
     * @return true 如果应该使用 CCD
     */
    static bool ShouldUseCCD(
        const RigidBodyComponent& body,
        const ColliderComponent& collider,
        float dt,
        float velocityThreshold = 10.0f,
        float displacementThreshold = 0.5f
    );
    
    /**
     * @brief 计算形状的尺寸（用于位移阈值判断）
     * 
     * @param collider 碰撞体组件
     * @return 形状的特征尺寸（球体=直径，盒体=最大边长，胶囊体=高度+2*半径）
     */
    static float ComputeShapeSize(const ColliderComponent& collider);
};

} // namespace Physics
} // namespace Render

