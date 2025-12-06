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
#include "collision_shapes.h"
#include "contact_manifold.h"
#include <array>

namespace Render {
namespace Physics {

/**
 * @brief GJK (Gilbert-Johnson-Keerthi) 算法
 * 
 * 用于检测两个凸形状是否相交
 * 基于闵可夫斯基差（Minkowski Difference）和单纯形搜索
 */
class GJK {
public:
    /**
     * @brief 单纯形（最多4个点）
     */
    struct Simplex {
        std::array<Vector3, 4> points;
        int count = 0;
        
        void Push(const Vector3& point) {
            if (count < 4) {
                points[count++] = point;
            }
        }
        
        Vector3& operator[](int i) { return points[i]; }
        const Vector3& operator[](int i) const { return points[i]; }
        
        int Size() const { return count; }
        
        void Clear() { count = 0; }
    };
    
    /**
     * @brief GJK 碰撞检测
     * @param shapeA 形状 A
     * @param posA 位置 A
     * @param rotA 旋转 A
     * @param shapeB 形状 B
     * @param posB 位置 B
     * @param rotB 旋转 B
     * @return 是否相交
     */
    static bool Intersects(
        const CollisionShape* shapeA, const Vector3& posA, const Quaternion& rotA,
        const CollisionShape* shapeB, const Vector3& posB, const Quaternion& rotB
    );
    
    /**
     * @brief GJK 碰撞检测并计算穿透信息（使用 EPA）
     * @param shapeA 形状 A
     * @param posA 位置 A
     * @param rotA 旋转 A
     * @param shapeB 形状 B
     * @param posB 位置 B
     * @param rotB 旋转 B
     * @param manifold 输出：接触流形
     * @return 是否相交
     */
    static bool IntersectsWithManifold(
        const CollisionShape* shapeA, const Vector3& posA, const Quaternion& rotA,
        const CollisionShape* shapeB, const Vector3& posB, const Quaternion& rotB,
        ContactManifold& manifold
    );
    
private:
public:  // 改为 public，供 EPA 使用
    /**
     * @brief 计算闵可夫斯基差的支撑点
     * @param direction 方向
     * @return 支撑点
     */
    static Vector3 Support(
        const CollisionShape* shapeA, const Vector3& posA, const Quaternion& rotA,
        const CollisionShape* shapeB, const Vector3& posB, const Quaternion& rotB,
        const Vector3& direction
    );

private:
    
    /**
     * @brief 更新单纯形并获取新的搜索方向
     * @param simplex 单纯形
     * @param direction 输出：新的搜索方向
     * @return 是否包含原点
     */
    static bool UpdateSimplex(Simplex& simplex, Vector3& direction);
    
    /**
     * @brief 处理线段情况（2个点）
     */
    static bool DoLine(Simplex& simplex, Vector3& direction);
    
    /**
     * @brief 处理三角形情况（3个点）
     */
    static bool DoTriangle(Simplex& simplex, Vector3& direction);
    
    /**
     * @brief 处理四面体情况（4个点）
     */
    static bool DoTetrahedron(Simplex& simplex, Vector3& direction);
    
    /**
     * @brief 最大迭代次数
     */
    static constexpr int MAX_ITERATIONS = 64;
};

/**
 * @brief EPA (Expanding Polytope Algorithm) 算法
 * 
 * 用于计算两个相交凸形状的穿透深度和法线
 */
class EPA {
public:
    /**
     * @brief 计算穿透信息
     * @param simplex GJK 返回的单纯形
     * @param shapeA 形状 A
     * @param posA 位置 A
     * @param rotA 旋转 A
     * @param shapeB 形状 B
     * @param posB 位置 B
     * @param rotB 旋转 B
     * @param manifold 输出：接触流形
     * @return 是否成功
     */
    static bool ComputePenetration(
        const GJK::Simplex& simplex,
        const CollisionShape* shapeA, const Vector3& posA, const Quaternion& rotA,
        const CollisionShape* shapeB, const Vector3& posB, const Quaternion& rotB,
        ContactManifold& manifold
    );
    
private:
    static constexpr int MAX_ITERATIONS = 64;
    static constexpr float EPSILON = 1e-6f;
};

} // namespace Physics
} // namespace Render

