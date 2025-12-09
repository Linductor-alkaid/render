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

#include "render/physics/collision/ccd_detector.h"
#include "render/physics/physics_components.h"
#include "render/math_utils.h"
#include <algorithm>
#include <cmath>

namespace Render {
namespace Physics {

// ============================================================================
// CCDDetector 实现
// ============================================================================

bool CCDDetector::Detect(
    const CollisionShape* shapeA,
    const Vector3& posA0, const Vector3& velA,
    const Quaternion& rotA0, const Vector3& angularVelA,
    const CollisionShape* shapeB,
    const Vector3& posB0, const Vector3& velB,
    const Quaternion& rotB0, const Vector3& angularVelB,
    float dt,
    CCDResult& result
) {
    if (!shapeA || !shapeB || dt <= 0.0f) {
        result.Reset();
        return false;
    }
    
    // 重置结果
    result.Reset();
    
    // 分发到具体的形状组合算法
    return Dispatch(
        shapeA->GetType(), shapeB->GetType(),
        shapeA, shapeB,
        posA0, velA, rotA0, angularVelA,
        posB0, velB, rotB0, angularVelB,
        dt, result
    );
}

bool CCDDetector::Dispatch(
    ShapeType typeA, ShapeType typeB,
    const CollisionShape* shapeA, const CollisionShape* shapeB,
    const Vector3& posA0, const Vector3& velA,
    const Quaternion& rotA0, const Vector3& angularVelA,
    const Vector3& posB0, const Vector3& velB,
    const Quaternion& rotB0, const Vector3& angularVelB,
    float dt,
    CCDResult& result
) {
    // 确保 typeA <= typeB（减少组合数量）
    if (typeA > typeB) {
        std::swap(typeA, typeB);
        std::swap(shapeA, shapeB);
        // 交换位置和速度
        return Dispatch(
            typeA, typeB,
            shapeB, shapeA,
            posB0, velB, rotB0, angularVelB,
            posA0, velA, rotA0, angularVelA,
            dt, result
        );
    }
    
    // 根据形状类型分发
    // 注意：目前只实现基础框架，具体算法在后续阶段实现
    // 这里先返回 false，表示未实现
    
    // TODO: 在阶段 2-3 中实现具体的 CCD 算法
    // Sphere vs Sphere
    if (typeA == ShapeType::Sphere && typeB == ShapeType::Sphere) {
        // 将在阶段 2 实现
        return false;
    }
    
    // Sphere vs Box
    if (typeA == ShapeType::Sphere && typeB == ShapeType::Box) {
        // 将在阶段 2 实现
        return false;
    }
    
    // Sphere vs Capsule
    if (typeA == ShapeType::Sphere && typeB == ShapeType::Capsule) {
        // 将在阶段 2 实现
        return false;
    }
    
    // Box vs Box
    if (typeA == ShapeType::Box && typeB == ShapeType::Box) {
        // 将在阶段 3 实现
        return false;
    }
    
    // Capsule vs Capsule
    if (typeA == ShapeType::Capsule && typeB == ShapeType::Capsule) {
        // 将在阶段 3 实现
        return false;
    }
    
    // Capsule vs Box
    if (typeA == ShapeType::Capsule && typeB == ShapeType::Box) {
        // 将在阶段 3 实现
        return false;
    }
    
    // 其他组合暂不支持
    result.Reset();
    return false;
}

// ============================================================================
// 具体的 CCD 算法实现（占位符，将在后续阶段实现）
// ============================================================================

bool CCDDetector::SphereVsSphereCCD(
    const Vector3& posA0, float radiusA, const Vector3& velA,
    const Vector3& posB0, float radiusB, const Vector3& velB,
    float dt,
    CCDResult& result
) {
    // 将在阶段 2 实现
    (void)posA0; (void)radiusA; (void)velA;
    (void)posB0; (void)radiusB; (void)velB;
    (void)dt;
    result.Reset();
    return false;
}

bool CCDDetector::SphereVsBoxCCD(
    const Vector3& spherePos0, float sphereRadius, const Vector3& sphereVel,
    const Vector3& boxCenter, const Vector3& boxHalfExtents,
    const Quaternion& boxRotation, const Vector3& boxVel,
    float dt,
    CCDResult& result
) {
    // 将在阶段 2 实现
    (void)spherePos0; (void)sphereRadius; (void)sphereVel;
    (void)boxCenter; (void)boxHalfExtents; (void)boxRotation; (void)boxVel;
    (void)dt;
    result.Reset();
    return false;
}

bool CCDDetector::SphereVsCapsuleCCD(
    const Vector3& spherePos0, float sphereRadius, const Vector3& sphereVel,
    const Vector3& capsuleCenter, float capsuleRadius, float capsuleHeight,
    const Quaternion& capsuleRotation, const Vector3& capsuleVel,
    float dt,
    CCDResult& result
) {
    // 将在阶段 2 实现
    (void)spherePos0; (void)sphereRadius; (void)sphereVel;
    (void)capsuleCenter; (void)capsuleRadius; (void)capsuleHeight;
    (void)capsuleRotation; (void)capsuleVel;
    (void)dt;
    result.Reset();
    return false;
}

bool CCDDetector::BoxVsBoxCCD(
    const Vector3& boxA0, const Vector3& boxAHalfExtents, 
    const Quaternion& boxARot0, const Vector3& boxAVel, const Vector3& boxAAngularVel,
    const Vector3& boxB0, const Vector3& boxBHalfExtents,
    const Quaternion& boxBRot0, const Vector3& boxBVel, const Vector3& boxBAngularVel,
    float dt,
    CCDResult& result
) {
    // 将在阶段 3 实现
    (void)boxA0; (void)boxAHalfExtents; (void)boxARot0; (void)boxAVel; (void)boxAAngularVel;
    (void)boxB0; (void)boxBHalfExtents; (void)boxBRot0; (void)boxBVel; (void)boxBAngularVel;
    (void)dt;
    result.Reset();
    return false;
}

bool CCDDetector::CapsuleVsCapsuleCCD(
    const Vector3& capsuleA0, float capsuleARadius, float capsuleAHeight,
    const Quaternion& capsuleARot0, const Vector3& capsuleAVel, const Vector3& capsuleAAngularVel,
    const Vector3& capsuleB0, float capsuleBRadius, float capsuleBHeight,
    const Quaternion& capsuleBRot0, const Vector3& capsuleBVel, const Vector3& capsuleBAngularVel,
    float dt,
    CCDResult& result
) {
    // 将在阶段 3 实现
    (void)capsuleA0; (void)capsuleARadius; (void)capsuleAHeight;
    (void)capsuleARot0; (void)capsuleAVel; (void)capsuleAAngularVel;
    (void)capsuleB0; (void)capsuleBRadius; (void)capsuleBHeight;
    (void)capsuleBRot0; (void)capsuleBVel; (void)capsuleBAngularVel;
    (void)dt;
    result.Reset();
    return false;
}

bool CCDDetector::CapsuleVsBoxCCD(
    const Vector3& capsuleCenter, float capsuleRadius, float capsuleHeight,
    const Quaternion& capsuleRotation, const Vector3& capsuleVel, const Vector3& capsuleAngularVel,
    const Vector3& boxCenter, const Vector3& boxHalfExtents,
    const Quaternion& boxRotation, const Vector3& boxVel, const Vector3& boxAngularVel,
    float dt,
    CCDResult& result
) {
    // 将在阶段 3 实现
    (void)capsuleCenter; (void)capsuleRadius; (void)capsuleHeight;
    (void)capsuleRotation; (void)capsuleVel; (void)capsuleAngularVel;
    (void)boxCenter; (void)boxHalfExtents; (void)boxRotation; (void)boxVel; (void)boxAngularVel;
    (void)dt;
    result.Reset();
    return false;
}

// ============================================================================
// CCDCandidateDetector 实现
// ============================================================================

bool CCDCandidateDetector::ShouldUseCCD(
    const RigidBodyComponent& body,
    const ColliderComponent& collider,
    float dt,
    float velocityThreshold,
    float displacementThreshold
) {
    // 用户强制启用
    if (body.useCCD) {
        return true;
    }
    
    // 静态和运动学物体不需要 CCD
    if (body.type != RigidBodyComponent::BodyType::Dynamic) {
        return false;
    }
    
    // 速度阈值检查：速度超过阈值就启用 CCD
    float speed = body.linearVelocity.norm();
    if (speed >= velocityThreshold) {
        return true;
    }
    
    // 位移检查：即使速度不够，但位移超过阈值也启用 CCD
    float displacement = speed * dt;
    float shapeSize = ComputeShapeSize(collider);
    if (displacement > shapeSize * displacementThreshold) {
        return true;
    }
    
    return false;
}

float CCDCandidateDetector::ComputeShapeSize(const ColliderComponent& collider) {
    switch (collider.shapeType) {
        case ColliderComponent::ShapeType::Sphere: {
            return collider.shapeData.sphere.radius * 2.0f;  // 直径
        }
        
        case ColliderComponent::ShapeType::Box: {
            Vector3 halfExtents = collider.GetBoxHalfExtents();
            return halfExtents.maxCoeff() * 2.0f;  // 最大边长
        }
        
        case ColliderComponent::ShapeType::Capsule: {
            float radius = collider.shapeData.capsule.radius;
            float height = collider.shapeData.capsule.height;
            return height + radius * 2.0f;  // 总高度
        }
        
        case ColliderComponent::ShapeType::Mesh:
        case ColliderComponent::ShapeType::ConvexHull: {
            // 对于复杂形状，使用 AABB 的最大边长
            if (!collider.aabbDirty) {
                Vector3 size = collider.worldAABB.max - collider.worldAABB.min;
                // 检查 AABB 是否有效（min <= max）
                if (size.x() >= 0.0f && size.y() >= 0.0f && size.z() >= 0.0f) {
                    return size.maxCoeff();
                }
            }
            // 如果 AABB 无效，返回默认值
            return 1.0f;
        }
        
        default:
            return 1.0f;
    }
}

} // namespace Physics
} // namespace Render

