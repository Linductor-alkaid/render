#pragma once

#include "render/types.h"
#include "render/math_utils.h"

namespace Render {
namespace Physics {

/**
 * @brief 碰撞形状类型枚举
 */
enum class ShapeType {
    Sphere,
    Box,
    Capsule,
    Mesh,
    ConvexHull
};

/**
 * @brief 碰撞形状抽象基类
 * 
 * 定义所有碰撞形状的通用接口
 */
class CollisionShape {
public:
    virtual ~CollisionShape() = default;
    
    /**
     * @brief 获取形状类型
     */
    virtual ShapeType GetType() const = 0;
    
    /**
     * @brief 计算形状的轴对齐包围盒
     * @param position 世界位置
     * @param rotation 世界旋转
     * @param scale 世界缩放
     * @return AABB
     */
    virtual AABB ComputeAABB(const Vector3& position, 
                             const Quaternion& rotation,
                             const Vector3& scale) const = 0;
    
    /**
     * @brief 计算形状的体积
     * @return 体积 (m³)
     */
    virtual float ComputeVolume() const = 0;
    
    /**
     * @brief 计算形状的惯性张量（局部空间）
     * @param mass 质量 (kg)
     * @return 惯性张量
     */
    virtual Matrix3 ComputeInertiaTensor(float mass) const = 0;
    
    /**
     * @brief 获取形状的支撑点（用于 GJK 算法）
     * @param direction 方向向量
     * @return 该方向上最远的点
     */
    virtual Vector3 GetSupportPoint(const Vector3& direction) const = 0;
};

// ============================================================================
// 球体形状
// ============================================================================

/**
 * @brief 球体碰撞形状
 */
class SphereShape : public CollisionShape {
public:
    explicit SphereShape(float radius = 0.5f) 
        : m_radius(std::max(radius, 0.001f)) {}
    
    ShapeType GetType() const override { return ShapeType::Sphere; }
    
    float GetRadius() const { return m_radius; }
    void SetRadius(float radius) { m_radius = std::max(radius, 0.001f); }
    
    AABB ComputeAABB(const Vector3& position, 
                     const Quaternion& rotation,
                     const Vector3& scale) const override {
        (void)rotation;  // 球体旋转不影响 AABB
        
        float maxScale = scale.maxCoeff();
        float worldRadius = m_radius * maxScale;
        Vector3 extents = Vector3::Ones() * worldRadius;
        
        return AABB(position - extents, position + extents);
    }
    
    float ComputeVolume() const override {
        return (4.0f / 3.0f) * MathUtils::PI * m_radius * m_radius * m_radius;
    }
    
    Matrix3 ComputeInertiaTensor(float mass) const override {
        float inertia = (2.0f / 5.0f) * mass * m_radius * m_radius;
        return Matrix3::Identity() * inertia;
    }
    
    Vector3 GetSupportPoint(const Vector3& direction) const override {
        return MathUtils::SafeNormalize(direction) * m_radius;
    }
    
private:
    float m_radius;
};

// ============================================================================
// 盒体形状
// ============================================================================

/**
 * @brief 盒体碰撞形状（轴对齐）
 */
class BoxShape : public CollisionShape {
public:
    explicit BoxShape(const Vector3& halfExtents = Vector3(0.5f, 0.5f, 0.5f))
        : m_halfExtents(halfExtents.cwiseMax(0.001f)) {}
    
    ShapeType GetType() const override { return ShapeType::Box; }
    
    const Vector3& GetHalfExtents() const { return m_halfExtents; }
    void SetHalfExtents(const Vector3& halfExtents) {
        m_halfExtents = halfExtents.cwiseMax(0.001f);
    }
    
    AABB ComputeAABB(const Vector3& position, 
                     const Quaternion& rotation,
                     const Vector3& scale) const override {
        Vector3 scaledExtents = m_halfExtents.cwiseProduct(scale);
        
        // 如果有旋转，计算旋转后的 AABB
        if (!rotation.isApprox(Quaternion::Identity())) {
            Matrix3 rotMatrix = rotation.toRotationMatrix();
            Vector3 absRotMatrix = rotMatrix.cwiseAbs() * scaledExtents;
            return AABB(position - absRotMatrix, position + absRotMatrix);
        }
        
        // 无旋转的情况
        return AABB(position - scaledExtents, position + scaledExtents);
    }
    
    float ComputeVolume() const override {
        Vector3 size = m_halfExtents * 2.0f;
        return size.x() * size.y() * size.z();
    }
    
    Matrix3 ComputeInertiaTensor(float mass) const override {
        Vector3 size = m_halfExtents * 2.0f;
        float xx = (1.0f / 12.0f) * mass * (size.y() * size.y() + size.z() * size.z());
        float yy = (1.0f / 12.0f) * mass * (size.x() * size.x() + size.z() * size.z());
        float zz = (1.0f / 12.0f) * mass * (size.x() * size.x() + size.y() * size.y());
        
        Matrix3 tensor = Matrix3::Zero();
        tensor(0, 0) = xx;
        tensor(1, 1) = yy;
        tensor(2, 2) = zz;
        return tensor;
    }
    
    Vector3 GetSupportPoint(const Vector3& direction) const override {
        return Vector3(
            direction.x() > 0 ? m_halfExtents.x() : -m_halfExtents.x(),
            direction.y() > 0 ? m_halfExtents.y() : -m_halfExtents.y(),
            direction.z() > 0 ? m_halfExtents.z() : -m_halfExtents.z()
        );
    }
    
    /**
     * @brief 获取盒体的8个顶点（局部空间）
     */
    void GetVertices(Vector3 vertices[8]) const {
        for (int i = 0; i < 8; ++i) {
            vertices[i] = Vector3(
                (i & 1) ? m_halfExtents.x() : -m_halfExtents.x(),
                (i & 2) ? m_halfExtents.y() : -m_halfExtents.y(),
                (i & 4) ? m_halfExtents.z() : -m_halfExtents.z()
            );
        }
    }
    
private:
    Vector3 m_halfExtents;
};

// ============================================================================
// 胶囊体形状
// ============================================================================

/**
 * @brief 胶囊体碰撞形状
 * 
 * 胶囊体由一个圆柱体和两个半球组成
 * 高度是指中心线段的长度（不包括两端的半球）
 */
class CapsuleShape : public CollisionShape {
public:
    explicit CapsuleShape(float radius = 0.5f, float height = 1.0f)
        : m_radius(std::max(radius, 0.001f))
        , m_height(std::max(height, 0.001f)) {}
    
    ShapeType GetType() const override { return ShapeType::Capsule; }
    
    float GetRadius() const { return m_radius; }
    float GetHeight() const { return m_height; }
    
    void SetRadius(float radius) { m_radius = std::max(radius, 0.001f); }
    void SetHeight(float height) { m_height = std::max(height, 0.001f); }
    
    /**
     * @brief 获取中心线段的两个端点（局部空间）
     */
    void GetLineSegment(Vector3& pointA, Vector3& pointB) const {
        float halfHeight = m_height * 0.5f;
        pointA = Vector3(0, -halfHeight, 0);
        pointB = Vector3(0,  halfHeight, 0);
    }
    
    AABB ComputeAABB(const Vector3& position, 
                     const Quaternion& rotation,
                     const Vector3& scale) const override {
        (void)rotation;  // 简化处理，假设胶囊体沿 Y 轴
        
        float maxScale = scale.maxCoeff();
        float worldRadius = m_radius * maxScale;
        float worldHalfHeight = m_height * 0.5f * scale.y();
        
        Vector3 extents(worldRadius, worldHalfHeight + worldRadius, worldRadius);
        return AABB(position - extents, position + extents);
    }
    
    float ComputeVolume() const override {
        // V = π*r²*h + (4/3)*π*r³
        float cylinderVolume = MathUtils::PI * m_radius * m_radius * m_height;
        float sphereVolume = (4.0f / 3.0f) * MathUtils::PI * m_radius * m_radius * m_radius;
        return cylinderVolume + sphereVolume;
    }
    
    Matrix3 ComputeInertiaTensor(float mass) const override {
        // 简化为圆柱体近似
        float radiusSq = m_radius * m_radius;
        float heightSq = m_height * m_height;
        
        float xx = mass * (3.0f * radiusSq + heightSq) / 12.0f;
        float yy = mass * radiusSq / 2.0f;  // 沿高度轴
        float zz = xx;
        
        Matrix3 tensor = Matrix3::Zero();
        tensor(0, 0) = xx;
        tensor(1, 1) = yy;
        tensor(2, 2) = zz;
        return tensor;
    }
    
    Vector3 GetSupportPoint(const Vector3& direction) const override {
        // 获取线段端点
        Vector3 pointA, pointB;
        GetLineSegment(pointA, pointB);
        
        // 选择朝向方向的端点
        Vector3 center = (direction.y() > 0) ? pointB : pointA;
        
        // 加上球体半径
        return center + MathUtils::SafeNormalize(direction) * m_radius;
    }
    
private:
    float m_radius;
    float m_height;
};

// ============================================================================
// 形状工厂
// ============================================================================

/**
 * @brief 碰撞形状工厂
 * 
 * 提供便捷的形状创建方法
 */
class ShapeFactory {
public:
    static std::shared_ptr<SphereShape> CreateSphere(float radius = 0.5f) {
        return std::make_shared<SphereShape>(radius);
    }
    
    static std::shared_ptr<BoxShape> CreateBox(const Vector3& halfExtents = Vector3(0.5f, 0.5f, 0.5f)) {
        return std::make_shared<BoxShape>(halfExtents);
    }
    
    static std::shared_ptr<CapsuleShape> CreateCapsule(float radius = 0.5f, float height = 1.0f) {
        return std::make_shared<CapsuleShape>(radius, height);
    }
    
    /**
     * @brief 从 GeometryPreset 名称创建形状（用于调试可视化）
     */
    static std::shared_ptr<CollisionShape> FromPresetName(const std::string& name) {
        if (name == "geometry::sphere") {
            return CreateSphere();
        } else if (name == "geometry::cube") {
            return CreateBox();
        } else if (name == "geometry::capsule") {
            return CreateCapsule();
        }
        return nullptr;
    }
};

} // namespace Physics
} // namespace Render

