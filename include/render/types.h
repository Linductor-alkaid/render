#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <Eigen/Core>
#include <Eigen/Geometry>

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

