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
// 资源句柄
// ============================================================================

using ResourceHandle = uint32_t;
constexpr ResourceHandle INVALID_HANDLE = 0;

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

