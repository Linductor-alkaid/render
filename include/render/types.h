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

#include <cstdint>
#include <string>
#include <memory>
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <Eigen/StdVector>

namespace Render {

// ============================================================================
// 数学类型定义（使用 Eigen）
// ============================================================================

using Vector2 = Eigen::Vector2f;
using Vector3 = Eigen::Vector3f;
using Vector4 = Eigen::Vector4f;

using Matrix3 = Eigen::Matrix3f;
using Matrix4 = Eigen::Matrix4f;

using Quaternion = Eigen::Quaternionf;

// ============================================================================
// 颜色类型
// ============================================================================

struct Color {
    float r, g, b, a;
    
    Color() : r(1.0f), g(1.0f), b(1.0f), a(1.0f) {}
    Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
    
    static Color White() { return Color(1.0f, 1.0f, 1.0f, 1.0f); }
    static Color Black() { return Color(0.0f, 0.0f, 0.0f, 1.0f); }
    static Color Red() { return Color(1.0f, 0.0f, 0.0f, 1.0f); }
    static Color Green() { return Color(0.0f, 1.0f, 0.0f, 1.0f); }
    static Color Blue() { return Color(0.0f, 0.0f, 1.0f, 1.0f); }
    static Color Yellow() { return Color(1.0f, 1.0f, 0.0f, 1.0f); }
    static Color Cyan() { return Color(0.0f, 1.0f, 1.0f, 1.0f); }
    static Color Magenta() { return Color(1.0f, 0.0f, 1.0f, 1.0f); }
    static Color Clear() { return Color(0.0f, 0.0f, 0.0f, 0.0f); }
    
    Vector4 ToVector4() const { return Vector4(r, g, b, a); }
};

// ============================================================================
// 矩形类型
// ============================================================================

struct Rect {
    float x, y, width, height;
    
    Rect() : x(0), y(0), width(0), height(0) {}
    Rect(float x, float y, float width, float height) 
        : x(x), y(y), width(width), height(height) {}
};

// ============================================================================
// Sprite 动画播放模式
// ============================================================================

enum class SpritePlaybackMode {
    Loop,      ///< 循环播放（到末尾后从头开始）
    Once,      ///< 播放一次（到末尾后停在最后一帧）
    PingPong   ///< 往返播放（正向播放到末尾后反向播放）
};

// ============================================================================
// 轴对齐包围盒
// ============================================================================

struct AABB {
    Vector3 min;
    Vector3 max;
    
    AABB() : min(Vector3::Zero()), max(Vector3::Zero()) {}
    AABB(const Vector3& min, const Vector3& max) : min(min), max(max) {}
    
    Vector3 GetCenter() const { return (min + max) * 0.5f; }
    Vector3 GetExtents() const { return (max - min) * 0.5f; }
    Vector3 GetSize() const { return max - min; }
    
    bool Contains(const Vector3& point) const {
        return point.x() >= min.x() && point.x() <= max.x() &&
               point.y() >= min.y() && point.y() <= max.y() &&
               point.z() >= min.z() && point.z() <= max.z();
    }
    
    bool Intersects(const AABB& other) const {
        return (min.x() <= other.max.x() && max.x() >= other.min.x()) &&
               (min.y() <= other.max.y() && max.y() >= other.min.y()) &&
               (min.z() <= other.max.z() && max.z() >= other.min.z());
    }
    
    // 扩展包围盒以包含另一个 AABB
    void Merge(const AABB& other) {
        min = min.cwiseMin(other.min);
        max = max.cwiseMax(other.max);
    }
    
    // 扩展包围盒以包含一个点
    void Expand(const Vector3& point) {
        min = min.cwiseMin(point);
        max = max.cwiseMax(point);
    }
    
    // 获取表面积
    float GetSurfaceArea() const {
        Vector3 size = GetSize();
        return 2.0f * (size.x() * size.y() + size.y() * size.z() + size.z() * size.x());
    }
};

// ============================================================================
// 定向包围盒 (Oriented Bounding Box)
// ============================================================================

struct OBB {
    Vector3 center;         // 中心点
    Vector3 halfExtents;    // 半尺寸
    Quaternion orientation; // 旋转方向
    
    OBB() : center(Vector3::Zero()), halfExtents(Vector3::Ones()), 
            orientation(Quaternion::Identity()) {}
    
    OBB(const Vector3& center, const Vector3& halfExtents, const Quaternion& orientation)
        : center(center), halfExtents(halfExtents), orientation(orientation) {}
    
    // 从 AABB 和变换创建 OBB
    static OBB FromAABB(const AABB& aabb, const Quaternion& rotation = Quaternion::Identity()) {
        return OBB(aabb.GetCenter(), aabb.GetExtents(), rotation);
    }
    
    // 转换为轴对齐包围盒（保守估计）
    AABB GetAABB() const {
        // 计算旋转后的范围
        Matrix3 rotMatrix = orientation.toRotationMatrix();
        Vector3 absRotMatrix = rotMatrix.cwiseAbs() * halfExtents;
        
        return AABB(center - absRotMatrix, center + absRotMatrix);
    }
    
    // 获取 OBB 的 8 个顶点
    void GetVertices(Vector3 vertices[8]) const {
        Matrix3 rotMatrix = orientation.toRotationMatrix();
        
        for (int i = 0; i < 8; ++i) {
            Vector3 offset(
                (i & 1) ? halfExtents.x() : -halfExtents.x(),
                (i & 2) ? halfExtents.y() : -halfExtents.y(),
                (i & 4) ? halfExtents.z() : -halfExtents.z()
            );
            vertices[i] = center + rotMatrix * offset;
        }
    }
};

// ============================================================================
// 平面
// ============================================================================

struct Plane {
    Vector3 normal;  // 平面法向量（单位向量）
    float distance;  // 原点到平面的距离
    
    Plane() : normal(Vector3::UnitY()), distance(0.0f) {}
    Plane(const Vector3& normal, float distance) : normal(normal), distance(distance) {}
    Plane(const Vector3& normal, const Vector3& point) 
        : normal(normal), distance(normal.dot(point)) {}
    Plane(const Vector3& p1, const Vector3& p2, const Vector3& p3) {
        Vector3 v1 = p2 - p1;
        Vector3 v2 = p3 - p1;
        normal = v1.cross(v2).normalized();
        distance = normal.dot(p1);
    }
    
    // 计算点到平面的距离（带符号）
    // 平面方程: normal.dot(point) = distance (点在平面上)
    float GetDistance(const Vector3& point) const {
        return normal.dot(point) - distance;
    }
    
    // 判断点在平面的哪一侧
    bool IsOnPositiveSide(const Vector3& point) const {
        return GetDistance(point) > 0.0f;
    }
};

// ============================================================================
// 射线
// ============================================================================

struct Ray {
    Vector3 origin;     // 射线起点
    Vector3 direction;  // 射线方向（单位向量）
    
    Ray() : origin(Vector3::Zero()), direction(Vector3::UnitZ()) {}
    Ray(const Vector3& origin, const Vector3& direction) 
        : origin(origin), direction(direction.normalized()) {}
    
    // 获取射线上的点
    Vector3 GetPoint(float t) const {
        return origin + direction * t;
    }
    
    // 与平面相交检测
    bool IntersectPlane(const Plane& plane, float& t) const {
        float denom = plane.normal.dot(direction);
        if (std::abs(denom) < 1e-6f) {
            return false; // 射线与平面平行
        }
        t = (plane.distance - plane.normal.dot(origin)) / denom;
        return t >= 0.0f; // 只考虑射线方向的交点
    }
    
    // 与 AABB 相交检测
    bool IntersectAABB(const AABB& aabb, float& tMin, float& tMax) const {
        tMin = 0.0f;
        tMax = std::numeric_limits<float>::max();
        
        for (int i = 0; i < 3; ++i) {
            if (std::abs(direction[i]) < 1e-6f) {
                // 射线与该轴平行
                if (origin[i] < aabb.min[i] || origin[i] > aabb.max[i]) {
                    return false;
                }
            } else {
                float ood = 1.0f / direction[i];
                float t1 = (aabb.min[i] - origin[i]) * ood;
                float t2 = (aabb.max[i] - origin[i]) * ood;
                
                if (t1 > t2) std::swap(t1, t2);
                
                tMin = std::max(tMin, t1);
                tMax = std::min(tMax, t2);
                
                if (tMin > tMax) {
                    return false;
                }
            }
        }
        
        return true;
    }
};

// ============================================================================
// 射线投射结果（用于物理查询）
// ============================================================================

namespace ECS { struct EntityID; }  // 前向声明

struct RaycastHit {
    ECS::EntityID* entity = nullptr;  // 击中的实体（使用指针避免包含整个 entity.h）
    Vector3 point = Vector3::Zero();  // 击中点（世界空间）
    Vector3 normal = Vector3::Zero(); // 击中面法线（世界空间）
    float distance = 0.0f;            // 从射线起点到击中点的距离
    
    RaycastHit() = default;
    
    // 是否有效（击中了物体）
    bool IsValid() const { return entity != nullptr; }
};

// ============================================================================
// 智能指针类型别名
// ============================================================================

template<typename T>
using Ref = std::shared_ptr<T>;

template<typename T>
using Scope = std::unique_ptr<T>;

template<typename T, typename... Args>
Ref<T> CreateRef(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
Scope<T> CreateScope(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

} // namespace Render

