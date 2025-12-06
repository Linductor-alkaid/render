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
#include "render/math_utils.h"
#include "contact_manifold.h"
#include "collision_shapes.h"

namespace Render {
namespace Physics {

/**
 * @brief 碰撞检测器
 * 
 * 提供各种形状之间的细检测算法
 */
class CollisionDetector {
public:
    // ==================== 球体碰撞检测 ====================
    
    /**
     * @brief 球体 vs 球体碰撞检测
     */
    static bool SphereVsSphere(
        const Vector3& centerA, float radiusA,
        const Vector3& centerB, float radiusB,
        ContactManifold& manifold
    );
    
    /**
     * @brief 球体 vs 盒体碰撞检测
     */
    static bool SphereVsBox(
        const Vector3& sphereCenter, float sphereRadius,
        const Vector3& boxCenter, const Vector3& boxHalfExtents,
        const Quaternion& boxRotation,
        ContactManifold& manifold
    );
    
    /**
     * @brief 球体 vs 胶囊体碰撞检测
     */
    static bool SphereVsCapsule(
        const Vector3& sphereCenter, float sphereRadius,
        const Vector3& capsuleCenter, float capsuleRadius, float capsuleHeight,
        const Quaternion& capsuleRotation,
        ContactManifold& manifold
    );
    
    // ==================== 盒体碰撞检测 ====================
    
    /**
     * @brief 盒体 vs 盒体碰撞检测（SAT 算法）
     */
    static bool BoxVsBox(
        const Vector3& centerA, const Vector3& halfExtentsA, const Quaternion& rotationA,
        const Vector3& centerB, const Vector3& halfExtentsB, const Quaternion& rotationB,
        ContactManifold& manifold
    );
    
    // ==================== 胶囊体碰撞检测 ====================
    
    /**
     * @brief 胶囊体 vs 胶囊体碰撞检测
     */
    static bool CapsuleVsCapsule(
        const Vector3& centerA, float radiusA, float heightA, const Quaternion& rotationA,
        const Vector3& centerB, float radiusB, float heightB, const Quaternion& rotationB,
        ContactManifold& manifold
    );
    
    /**
     * @brief 胶囊体 vs 盒体碰撞检测
     */
    static bool CapsuleVsBox(
        const Vector3& capsuleCenter, float capsuleRadius, float capsuleHeight, 
        const Quaternion& capsuleRotation,
        const Vector3& boxCenter, const Vector3& boxHalfExtents, const Quaternion& boxRotation,
        ContactManifold& manifold
    );
    
    // ==================== 辅助函数 ====================
    
    /**
     * @brief 计算点到线段的最近点
     */
    static Vector3 ClosestPointOnSegment(const Vector3& point, 
                                          const Vector3& segmentA, 
                                          const Vector3& segmentB);
    
    /**
     * @brief 计算两条线段之间的最近点
     */
    static void ClosestPointsBetweenSegments(
        const Vector3& p1, const Vector3& q1,
        const Vector3& p2, const Vector3& q2,
        float& s, float& t,
        Vector3& c1, Vector3& c2
    );
    
    /**
     * @brief 计算点到 OBB 的最近点
     */
    static Vector3 ClosestPointOnOBB(const Vector3& point,
                                      const Vector3& obbCenter,
                                      const Vector3& obbHalfExtents,
                                      const Quaternion& obbRotation);
};

// ============================================================================
// 碰撞检测分发器
// ============================================================================

/**
 * @brief 碰撞检测分发器
 * 
 * 根据形状类型自动分发到对应的碰撞检测函数
 */
class CollisionDispatcher {
public:
    /**
     * @brief 检测两个碰撞体是否碰撞
     * @param shapeA 形状 A
     * @param transformA 变换 A（位置、旋转、缩放）
     * @param shapeB 形状 B
     * @param transformB 变换 B
     * @param manifold 输出：接触流形
     * @return 是否发生碰撞
     */
    static bool Detect(
        const CollisionShape* shapeA, const Vector3& posA, const Quaternion& rotA, const Vector3& scaleA,
        const CollisionShape* shapeB, const Vector3& posB, const Quaternion& rotB, const Vector3& scaleB,
        ContactManifold& manifold
    );
    
private:
    // 内部分发函数
    static bool DispatchSphere(
        const SphereShape* sphere, const Vector3& pos, const Vector3& scale,
        const CollisionShape* other, const Vector3& otherPos, const Quaternion& otherRot, const Vector3& otherScale,
        ContactManifold& manifold, bool swapped
    );
    
    static bool DispatchBox(
        const BoxShape* box, const Vector3& pos, const Quaternion& rot, const Vector3& scale,
        const CollisionShape* other, const Vector3& otherPos, const Quaternion& otherRot, const Vector3& otherScale,
        ContactManifold& manifold, bool swapped
    );
    
    static bool DispatchCapsule(
        const CapsuleShape* capsule, const Vector3& pos, const Quaternion& rot, const Vector3& scale,
        const CollisionShape* other, const Vector3& otherPos, const Quaternion& otherRot, const Vector3& otherScale,
        ContactManifold& manifold, bool swapped
    );
};

} // namespace Physics
} // namespace Render

