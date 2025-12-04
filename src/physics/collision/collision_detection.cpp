#include "render/physics/collision/collision_detection.h"

namespace Render {
namespace Physics {

// ============================================================================
// 球体碰撞检测
// ============================================================================

bool CollisionDetector::SphereVsSphere(
    const Vector3& centerA, float radiusA,
    const Vector3& centerB, float radiusB,
    ContactManifold& manifold
) {
    Vector3 delta = centerB - centerA;
    float distSq = delta.squaredNorm();
    float radiusSum = radiusA + radiusB;
    float radiusSumSq = radiusSum * radiusSum;
    
    // 检测是否相交
    if (distSq >= radiusSumSq) {
        return false;  // 不相交
    }
    
    // 计算碰撞信息
    float dist = std::sqrt(distSq);
    
    // 处理两球重合的情况
    if (dist < MathUtils::EPSILON) {
        manifold.SetNormal(Vector3::UnitY());
        manifold.penetration = radiusSum;
        manifold.AddContact(centerA, radiusSum);
        return true;
    }
    
    // 计算法线（从 A 指向 B）
    Vector3 normal = delta / dist;
    manifold.SetNormal(normal);
    
    // 穿透深度
    float penetration = radiusSum - dist;
    manifold.penetration = penetration;
    
    // 接触点（在两球表面之间）
    Vector3 contactPoint = centerA + normal * radiusA;
    manifold.AddContact(contactPoint, penetration);
    
    return true;
}

bool CollisionDetector::SphereVsBox(
    const Vector3& sphereCenter, float sphereRadius,
    const Vector3& boxCenter, const Vector3& boxHalfExtents,
    const Quaternion& boxRotation,
    ContactManifold& manifold
) {
    // 计算球心在盒体局部空间中的最近点
    Vector3 closestPoint = ClosestPointOnOBB(sphereCenter, boxCenter, boxHalfExtents, boxRotation);
    
    // 检测距离
    Vector3 delta = sphereCenter - closestPoint;
    float distSq = delta.squaredNorm();
    float radiusSq = sphereRadius * sphereRadius;
    
    if (distSq >= radiusSq) {
        return false;  // 不相交
    }
    
    float dist = std::sqrt(distSq);
    
    // 球心在盒体内部的情况
    if (dist < MathUtils::EPSILON) {
        // 找到最近的面
        Matrix3 rotMatrix = boxRotation.toRotationMatrix();
        Vector3 localCenter = rotMatrix.transpose() * (sphereCenter - boxCenter);
        
        // 找到最近的轴
        Vector3 localExtents = boxHalfExtents - localCenter.cwiseAbs();
        int minAxis = 0;
        float minDist = localExtents.x();
        
        if (localExtents.y() < minDist) {
            minAxis = 1;
            minDist = localExtents.y();
        }
        if (localExtents.z() < minDist) {
            minAxis = 2;
            minDist = localExtents.z();
        }
        
        Vector3 localNormal = Vector3::Zero();
        localNormal[minAxis] = (localCenter[minAxis] > 0) ? 1.0f : -1.0f;
        
        Vector3 normal = rotMatrix * localNormal;
        manifold.SetNormal(normal);
        manifold.penetration = sphereRadius + minDist;
        manifold.AddContact(sphereCenter - normal * sphereRadius, manifold.penetration);
        return true;
    }
    
    // 正常碰撞情况
    Vector3 normal = delta / dist;
    manifold.SetNormal(normal);
    manifold.penetration = sphereRadius - dist;
    manifold.AddContact(closestPoint, manifold.penetration);
    
    return true;
}

bool CollisionDetector::SphereVsCapsule(
    const Vector3& sphereCenter, float sphereRadius,
    const Vector3& capsuleCenter, float capsuleRadius, float capsuleHeight,
    const Quaternion& capsuleRotation,
    ContactManifold& manifold
) {
    // 获取胶囊体的中心线段
    Matrix3 rotMatrix = capsuleRotation.toRotationMatrix();
    Vector3 capsuleAxis = rotMatrix * Vector3::UnitY();
    float halfHeight = capsuleHeight * 0.5f;
    
    Vector3 segmentA = capsuleCenter - capsuleAxis * halfHeight;
    Vector3 segmentB = capsuleCenter + capsuleAxis * halfHeight;
    
    // 计算球心到线段的最近点
    Vector3 closestPoint = ClosestPointOnSegment(sphereCenter, segmentA, segmentB);
    
    // 检测距离
    Vector3 delta = sphereCenter - closestPoint;
    float distSq = delta.squaredNorm();
    float radiusSum = sphereRadius + capsuleRadius;
    float radiusSumSq = radiusSum * radiusSum;
    
    if (distSq >= radiusSumSq) {
        return false;
    }
    
    float dist = std::sqrt(distSq);
    
    if (dist < MathUtils::EPSILON) {
        manifold.SetNormal(Vector3::UnitY());
        manifold.penetration = radiusSum;
        manifold.AddContact(closestPoint, radiusSum);
        return true;
    }
    
    Vector3 normal = delta / dist;
    manifold.SetNormal(normal);
    manifold.penetration = radiusSum - dist;
    manifold.AddContact(closestPoint + normal * capsuleRadius, manifold.penetration);
    
    return true;
}

// ============================================================================
// 盒体碰撞检测（SAT 算法简化版）
// ============================================================================

bool CollisionDetector::BoxVsBox(
    const Vector3& centerA, const Vector3& halfExtentsA, const Quaternion& rotationA,
    const Vector3& centerB, const Vector3& halfExtentsB, const Quaternion& rotationB,
    ContactManifold& manifold
) {
    // SAT (Separating Axis Theorem) 简化实现
    // 完整的 SAT 需要测试 15 个轴，这里先实现 AABB 版本
    
    // 如果两个盒体都是轴对齐的，使用简化检测
    bool isAAligned = rotationA.isApprox(Quaternion::Identity());
    bool isBAligned = rotationB.isApprox(Quaternion::Identity());
    
    if (isAAligned && isBAligned) {
        // AABB vs AABB 检测
        AABB aabbA(centerA - halfExtentsA, centerA + halfExtentsA);
        AABB aabbB(centerB - halfExtentsB, centerB + halfExtentsB);
        
        if (!aabbA.Intersects(aabbB)) {
            return false;
        }
        
        // 计算穿透深度和法线
        Vector3 delta = centerB - centerA;
        Vector3 overlap = (halfExtentsA + halfExtentsB) - delta.cwiseAbs();
        
        // 找到最小穿透轴
        int minAxis = 0;
        float minOverlap = overlap.x();
        
        if (overlap.y() < minOverlap) {
            minAxis = 1;
            minOverlap = overlap.y();
        }
        if (overlap.z() < minOverlap) {
            minAxis = 2;
            minOverlap = overlap.z();
        }
        
        Vector3 normal = Vector3::Zero();
        normal[minAxis] = (delta[minAxis] > 0) ? 1.0f : -1.0f;
        
        manifold.SetNormal(normal);
        manifold.penetration = minOverlap;
        manifold.AddContact(centerA + delta * 0.5f, minOverlap);
        
        return true;
    }
    
    // OBB vs OBB 完整 SAT 实现（将在后续优化）
    // 当前返回 false，表示未实现
    return false;
}

// ============================================================================
// 胶囊体碰撞检测
// ============================================================================

bool CollisionDetector::CapsuleVsCapsule(
    const Vector3& centerA, float radiusA, float heightA, const Quaternion& rotationA,
    const Vector3& centerB, float radiusB, float heightB, const Quaternion& rotationB,
    ContactManifold& manifold
) {
    // 获取两个胶囊体的中心线段
    Matrix3 rotMatrixA = rotationA.toRotationMatrix();
    Matrix3 rotMatrixB = rotationB.toRotationMatrix();
    
    Vector3 axisA = rotMatrixA * Vector3::UnitY();
    Vector3 axisB = rotMatrixB * Vector3::UnitY();
    
    float halfHeightA = heightA * 0.5f;
    float halfHeightB = heightB * 0.5f;
    
    Vector3 p1 = centerA - axisA * halfHeightA;
    Vector3 q1 = centerA + axisA * halfHeightA;
    Vector3 p2 = centerB - axisB * halfHeightB;
    Vector3 q2 = centerB + axisB * halfHeightB;
    
    // 计算两条线段之间的最近点
    float s, t;
    Vector3 c1, c2;
    ClosestPointsBetweenSegments(p1, q1, p2, q2, s, t, c1, c2);
    
    // 检测距离
    Vector3 delta = c2 - c1;
    float distSq = delta.squaredNorm();
    float radiusSum = radiusA + radiusB;
    float radiusSumSq = radiusSum * radiusSum;
    
    if (distSq >= radiusSumSq) {
        return false;
    }
    
    float dist = std::sqrt(distSq);
    
    if (dist < MathUtils::EPSILON) {
        manifold.SetNormal(Vector3::UnitY());
        manifold.penetration = radiusSum;
        manifold.AddContact(c1, radiusSum);
        return true;
    }
    
    Vector3 normal = delta / dist;
    manifold.SetNormal(normal);
    manifold.penetration = radiusSum - dist;
    manifold.AddContact(c1 + normal * radiusA, manifold.penetration);
    
    return true;
}

bool CollisionDetector::CapsuleVsBox(
    const Vector3& capsuleCenter, float capsuleRadius, float capsuleHeight, 
    const Quaternion& capsuleRotation,
    const Vector3& boxCenter, const Vector3& boxHalfExtents, const Quaternion& boxRotation,
    ContactManifold& manifold
) {
    // 简化实现：当前未实现
    // 将在后续优化阶段添加
    return false;
}

// ============================================================================
// 辅助函数实现
// ============================================================================

Vector3 CollisionDetector::ClosestPointOnSegment(
    const Vector3& point, 
    const Vector3& segmentA, 
    const Vector3& segmentB
) {
    Vector3 ab = segmentB - segmentA;
    float t = (point - segmentA).dot(ab) / ab.squaredNorm();
    t = MathUtils::Clamp(t, 0.0f, 1.0f);
    return segmentA + ab * t;
}

void CollisionDetector::ClosestPointsBetweenSegments(
    const Vector3& p1, const Vector3& q1,
    const Vector3& p2, const Vector3& q2,
    float& s, float& t,
    Vector3& c1, Vector3& c2
) {
    Vector3 d1 = q1 - p1;
    Vector3 d2 = q2 - p2;
    Vector3 r = p1 - p2;
    
    float a = d1.squaredNorm();
    float e = d2.squaredNorm();
    float f = d2.dot(r);
    
    const float epsilon = MathUtils::EPSILON;
    
    // 处理退化情况
    if (a <= epsilon && e <= epsilon) {
        s = t = 0.0f;
        c1 = p1;
        c2 = p2;
        return;
    }
    
    if (a <= epsilon) {
        s = 0.0f;
        t = MathUtils::Clamp(f / e, 0.0f, 1.0f);
    } else {
        float c = d1.dot(r);
        if (e <= epsilon) {
            t = 0.0f;
            s = MathUtils::Clamp(-c / a, 0.0f, 1.0f);
        } else {
            float b = d1.dot(d2);
            float denom = a * e - b * b;
            
            if (denom != 0.0f) {
                s = MathUtils::Clamp((b * f - c * e) / denom, 0.0f, 1.0f);
            } else {
                s = 0.0f;
            }
            
            t = (b * s + f) / e;
            
            if (t < 0.0f) {
                t = 0.0f;
                s = MathUtils::Clamp(-c / a, 0.0f, 1.0f);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = MathUtils::Clamp((b - c) / a, 0.0f, 1.0f);
            }
        }
    }
    
    c1 = p1 + d1 * s;
    c2 = p2 + d2 * t;
}

Vector3 CollisionDetector::ClosestPointOnOBB(
    const Vector3& point,
    const Vector3& obbCenter,
    const Vector3& obbHalfExtents,
    const Quaternion& obbRotation
) {
    // 将点转换到 OBB 局部空间
    Matrix3 rotMatrix = obbRotation.toRotationMatrix();
    Vector3 localPoint = rotMatrix.transpose() * (point - obbCenter);
    
    // 在局部空间中夹取到盒体范围内
    Vector3 closestLocal = localPoint.cwiseMax(-obbHalfExtents).cwiseMin(obbHalfExtents);
    
    // 转换回世界空间
    return obbCenter + rotMatrix * closestLocal;
}

// ============================================================================
// 碰撞检测分发器实现
// ============================================================================

bool CollisionDispatcher::Detect(
    const CollisionShape* shapeA, const Vector3& posA, const Quaternion& rotA, const Vector3& scaleA,
    const CollisionShape* shapeB, const Vector3& posB, const Quaternion& rotB, const Vector3& scaleB,
    ContactManifold& manifold
) {
    if (!shapeA || !shapeB) {
        return false;
    }
    
    manifold.Clear();
    
    // 根据形状类型分发
    ShapeType typeA = shapeA->GetType();
    ShapeType typeB = shapeB->GetType();
    
    // 确保 Sphere 在前（简化分发逻辑）
    if (typeA == ShapeType::Sphere) {
        return DispatchSphere(
            static_cast<const SphereShape*>(shapeA), posA, scaleA,
            shapeB, posB, rotB, scaleB,
            manifold, false
        );
    } else if (typeB == ShapeType::Sphere) {
        bool hit = DispatchSphere(
            static_cast<const SphereShape*>(shapeB), posB, scaleB,
            shapeA, posA, rotA, scaleA,
            manifold, true
        );
        if (hit) {
            manifold.SetNormal(-manifold.normal);  // 翻转法线
        }
        return hit;
    }
    
    // Box 分发
    if (typeA == ShapeType::Box) {
        return DispatchBox(
            static_cast<const BoxShape*>(shapeA), posA, rotA, scaleA,
            shapeB, posB, rotB, scaleB,
            manifold, false
        );
    } else if (typeB == ShapeType::Box) {
        bool hit = DispatchBox(
            static_cast<const BoxShape*>(shapeB), posB, rotB, scaleB,
            shapeA, posA, rotA, scaleA,
            manifold, true
        );
        if (hit) {
            manifold.SetNormal(-manifold.normal);
        }
        return hit;
    }
    
    // Capsule 分发
    if (typeA == ShapeType::Capsule && typeB == ShapeType::Capsule) {
        return DispatchCapsule(
            static_cast<const CapsuleShape*>(shapeA), posA, rotA, scaleA,
            shapeB, posB, rotB, scaleB,
            manifold, false
        );
    }
    
    return false;
}

bool CollisionDispatcher::DispatchSphere(
    const SphereShape* sphere, const Vector3& pos, const Vector3& scale,
    const CollisionShape* other, const Vector3& otherPos, const Quaternion& otherRot, const Vector3& otherScale,
    ContactManifold& manifold, bool swapped
) {
    float sphereRadius = sphere->GetRadius() * scale.maxCoeff();
    
    switch (other->GetType()) {
        case ShapeType::Sphere: {
            const SphereShape* otherSphere = static_cast<const SphereShape*>(other);
            float otherRadius = otherSphere->GetRadius() * otherScale.maxCoeff();
            return CollisionDetector::SphereVsSphere(
                pos, sphereRadius,
                otherPos, otherRadius,
                manifold
            );
        }
        
        case ShapeType::Box: {
            const BoxShape* box = static_cast<const BoxShape*>(other);
            Vector3 boxHalfExtents = box->GetHalfExtents().cwiseProduct(otherScale);
            return CollisionDetector::SphereVsBox(
                pos, sphereRadius,
                otherPos, boxHalfExtents, otherRot,
                manifold
            );
        }
        
        case ShapeType::Capsule: {
            const CapsuleShape* capsule = static_cast<const CapsuleShape*>(other);
            float capsuleRadius = capsule->GetRadius() * otherScale.maxCoeff();
            float capsuleHeight = capsule->GetHeight() * otherScale.y();
            return CollisionDetector::SphereVsCapsule(
                pos, sphereRadius,
                otherPos, capsuleRadius, capsuleHeight, otherRot,
                manifold
            );
        }
        
        default:
            return false;
    }
}

bool CollisionDispatcher::DispatchBox(
    const BoxShape* box, const Vector3& pos, const Quaternion& rot, const Vector3& scale,
    const CollisionShape* other, const Vector3& otherPos, const Quaternion& otherRot, const Vector3& otherScale,
    ContactManifold& manifold, bool swapped
) {
    Vector3 boxHalfExtents = box->GetHalfExtents().cwiseProduct(scale);
    
    switch (other->GetType()) {
        case ShapeType::Box: {
            const BoxShape* otherBox = static_cast<const BoxShape*>(other);
            Vector3 otherHalfExtents = otherBox->GetHalfExtents().cwiseProduct(otherScale);
            return CollisionDetector::BoxVsBox(
                pos, boxHalfExtents, rot,
                otherPos, otherHalfExtents, otherRot,
                manifold
            );
        }
        
        case ShapeType::Capsule: {
            const CapsuleShape* capsule = static_cast<const CapsuleShape*>(other);
            float capsuleRadius = capsule->GetRadius() * otherScale.maxCoeff();
            float capsuleHeight = capsule->GetHeight() * otherScale.y();
            return CollisionDetector::CapsuleVsBox(
                otherPos, capsuleRadius, capsuleHeight, otherRot,
                pos, boxHalfExtents, rot,
                manifold
            );
        }
        
        default:
            return false;
    }
}

bool CollisionDispatcher::DispatchCapsule(
    const CapsuleShape* capsule, const Vector3& pos, const Quaternion& rot, const Vector3& scale,
    const CollisionShape* other, const Vector3& otherPos, const Quaternion& otherRot, const Vector3& otherScale,
    ContactManifold& manifold, bool swapped
) {
    float capsuleRadius = capsule->GetRadius() * scale.maxCoeff();
    float capsuleHeight = capsule->GetHeight() * scale.y();
    
    switch (other->GetType()) {
        case ShapeType::Capsule: {
            const CapsuleShape* otherCapsule = static_cast<const CapsuleShape*>(other);
            float otherRadius = otherCapsule->GetRadius() * otherScale.maxCoeff();
            float otherHeight = otherCapsule->GetHeight() * otherScale.y();
            return CollisionDetector::CapsuleVsCapsule(
                pos, capsuleRadius, capsuleHeight, rot,
                otherPos, otherRadius, otherHeight, otherRot,
                manifold
            );
        }
        
        default:
            return false;
    }
}

} // namespace Physics
} // namespace Render

