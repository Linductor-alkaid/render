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
#include "render/physics/collision/collision_shapes.h"
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
        const SphereShape* sphereA = static_cast<const SphereShape*>(shapeA);
        const SphereShape* sphereB = static_cast<const SphereShape*>(shapeB);
        return SphereVsSphereCCD(
            posA0, sphereA->GetRadius(), velA,
            posB0, sphereB->GetRadius(), velB,
            dt, result
        );
    }
    
    // Sphere vs Box
    if (typeA == ShapeType::Sphere && typeB == ShapeType::Box) {
        const SphereShape* sphere = static_cast<const SphereShape*>(shapeA);
        const BoxShape* box = static_cast<const BoxShape*>(shapeB);
        return SphereVsBoxCCD(
            posA0, sphere->GetRadius(), velA,
            posB0, box->GetHalfExtents(), rotB0, velB,
            dt, result
        );
    }
    
    // Sphere vs Capsule
    if (typeA == ShapeType::Sphere && typeB == ShapeType::Capsule) {
        const SphereShape* sphere = static_cast<const SphereShape*>(shapeA);
        const CapsuleShape* capsule = static_cast<const CapsuleShape*>(shapeB);
        return SphereVsCapsuleCCD(
            posA0, sphere->GetRadius(), velA,
            posB0, capsule->GetRadius(), capsule->GetHeight(), rotB0, velB,
            dt, result
        );
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
    result.Reset();
    
    // 相对位置和速度
    Vector3 p0 = posA0 - posB0;
    Vector3 v = velA - velB;
    
    float rSum = radiusA + radiusB;
    float rSumSq = rSum * rSum;
    
    // 检查初始状态：如果已经相交，TOI = 0
    float distSq0 = p0.squaredNorm();
    if (distSq0 <= rSumSq) {
        float dist0 = std::sqrt(distSq0);
        result.collided = true;
        result.toi = 0.0f;
        if (dist0 > MathUtils::EPSILON) {
            result.collisionNormal = p0 / dist0;
        } else {
            // 如果两球完全重叠，使用默认法线
            result.collisionNormal = Vector3::UnitY();
        }
        result.collisionPoint = posB0 + result.collisionNormal * radiusB;
        result.penetration = rSum - dist0;
        return true;
    }
    
    // 二次方程：|p0 + v*t|² = rSum²
    // 展开：(p0·p0) + 2*(p0·v)*t + (v·v)*t² = rSum²
    // 即：at² + bt + c = 0
    float a = v.squaredNorm();
    
    // 如果速度为零或很小，不会碰撞
    if (a < MathUtils::EPSILON) {
        return false;
    }
    
    float b = 2.0f * p0.dot(v);
    float c = distSq0 - rSumSq;
    
    // 求解判别式
    float discriminant = b * b - 4.0f * a * c;
    
    if (discriminant < 0.0f) {
        return false;  // 无解，不相交
    }
    
    float sqrtD = std::sqrt(discriminant);
    float t1 = (-b - sqrtD) / (2.0f * a);
    float t2 = (-b + sqrtD) / (2.0f * a);
    
    // 选择 [0, dt] 范围内的最早碰撞时刻
    float toi = -1.0f;
    if (t1 >= 0.0f && t1 <= dt) {
        toi = t1;
    } else if (t2 >= 0.0f && t2 <= dt) {
        toi = t2;
    }
    
    if (toi < 0.0f) {
        return false;  // 碰撞发生在时间范围外
    }
    
    // 计算碰撞时的位置和法线
    Vector3 pAtTOI = posA0 + velA * toi;
    Vector3 pBtTOI = posB0 + velB * toi;
    Vector3 delta = pAtTOI - pBtTOI;
    float dist = delta.norm();
    
    if (dist < MathUtils::EPSILON) {
        // 如果距离为零（理论上不应该发生），使用默认法线
        result.collisionNormal = Vector3::UnitY();
    } else {
        result.collisionNormal = delta / dist;
    }
    
    result.collided = true;
    result.toi = toi / dt;  // 归一化到 [0, 1]
    result.collisionPoint = pBtTOI + result.collisionNormal * radiusB;
    result.penetration = 0.0f;  // CCD 在接触时刻停止，无穿透
    
    return true;
}

bool CCDDetector::SphereVsBoxCCD(
    const Vector3& spherePos0, float sphereRadius, const Vector3& sphereVel,
    const Vector3& boxCenter, const Vector3& boxHalfExtents,
    const Quaternion& boxRotation, const Vector3& boxVel,
    float dt,
    CCDResult& result
) {
    result.Reset();
    
    // 扩大盒体（各轴加上球半径）
    Vector3 expandedHalfExtents = boxHalfExtents + Vector3::Ones() * sphereRadius;
    
    // 球心轨迹线段
    Vector3 segmentStart = spherePos0;
    Vector3 segmentEnd = spherePos0 + sphereVel * dt;
    
    // 转换到盒体局部空间
    Matrix3 rotMatrix = boxRotation.toRotationMatrix();
    Matrix3 rotMatrixInv = rotMatrix.transpose();
    
    Vector3 localStart = rotMatrixInv * (segmentStart - boxCenter);
    Vector3 localEnd = rotMatrixInv * (segmentEnd - boxCenter);
    Vector3 localVel = rotMatrixInv * (sphereVel - boxVel);
    
    // 检测线段与 AABB 的碰撞
    Vector3 normal = Vector3::Zero();
    bool foundCollision = false;
    float tEnterMax = 0.0f;
    float tExitMin = dt;
    
    // 对每个轴进行检测
    for (int axis = 0; axis < 3; ++axis) {
        float axisMin = -expandedHalfExtents[axis];
        float axisMax = expandedHalfExtents[axis];
        
        float startVal = localStart[axis];
        float endVal = localEnd[axis];
        
        // 如果线段完全在盒体外，跳过
        if ((startVal < axisMin && endVal < axisMin) ||
            (startVal > axisMax && endVal > axisMax)) {
            return false;  // 完全不相交
        }
        
        // 计算进入和离开时间
        float tEnter = 0.0f;
        float tExit = dt;
        
        if (std::abs(localVel[axis]) > MathUtils::EPSILON) {
            tEnter = (axisMin - startVal) / localVel[axis];
            tExit = (axisMax - startVal) / localVel[axis];
            
            if (tEnter > tExit) std::swap(tEnter, tExit);
        } else {
            // 速度为零，检查是否在范围内
            if (startVal < axisMin || startVal > axisMax) {
                return false;  // 不在范围内
            }
            // 在范围内，tEnter = 0, tExit = dt
        }
        
        // 更新全局的进入/离开时间
        if (tEnter > tEnterMax) {
            tEnterMax = tEnter;
            // 确定碰撞法线
            normal = Vector3::Zero();
            normal[axis] = (startVal < 0.0f) ? -1.0f : 1.0f;
            foundCollision = true;
        }
        if (tExit < tExitMin) {
            tExitMin = tExit;
        }
    }
    
    // 检查是否有效碰撞
    if (!foundCollision || tEnterMax > tExitMin || tEnterMax < 0.0f || tEnterMax > dt) {
        return false;
    }
    
    // 转换法线到世界空间
    normal = rotMatrix * normal;
    normal.normalize();
    
    result.collided = true;
    result.toi = tEnterMax / dt;  // 归一化到 [0, 1]
    result.collisionPoint = spherePos0 + sphereVel * tEnterMax - normal * sphereRadius;
    result.collisionNormal = normal;
    result.penetration = 0.0f;
    
    return true;
}

bool CCDDetector::SphereVsCapsuleCCD(
    const Vector3& spherePos0, float sphereRadius, const Vector3& sphereVel,
    const Vector3& capsuleCenter, float capsuleRadius, float capsuleHeight,
    const Quaternion& capsuleRotation, const Vector3& capsuleVel,
    float dt,
    CCDResult& result
) {
    result.Reset();
    
    // 获取胶囊体中心线段
    Matrix3 rotMatrix = capsuleRotation.toRotationMatrix();
    Vector3 capsuleAxis = rotMatrix * Vector3::UnitY();
    float halfHeight = capsuleHeight * 0.5f;
    
    Vector3 capsuleSegStart = capsuleCenter - capsuleAxis * halfHeight;
    Vector3 capsuleSegEnd = capsuleCenter + capsuleAxis * halfHeight;
    
    // 球心轨迹
    Vector3 sphereStart = spherePos0;
    Vector3 sphereEnd = spherePos0 + sphereVel * dt;
    
    // 计算两条线段之间的最近距离
    // 使用参数化表示：
    // 胶囊线段：p(s) = capsuleSegStart + s * (capsuleSegEnd - capsuleSegStart)
    // 球心线段：q(t) = sphereStart + t * (sphereEnd - sphereStart)
    
    Vector3 d1 = capsuleSegEnd - capsuleSegStart;
    Vector3 d2 = sphereEnd - sphereStart;
    Vector3 r = sphereStart - capsuleSegStart;
    
    float a = d1.squaredNorm();
    float e = d2.squaredNorm();
    float f = d2.dot(r);
    
    float s = 0.0f, t = 0.0f;
    
    if (a > MathUtils::EPSILON && e > MathUtils::EPSILON) {
        float b = d1.dot(d2);
        float denom = a * e - b * b;
        
        if (std::abs(denom) > MathUtils::EPSILON) {
            s = MathUtils::Clamp((b * f - r.dot(d2) * e) / denom, 0.0f, 1.0f);
            t = (b * s + f) / e;
            
            if (t < 0.0f) {
                t = 0.0f;
                s = MathUtils::Clamp(-r.dot(d1) / a, 0.0f, 1.0f);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = MathUtils::Clamp((b - r.dot(d1)) / a, 0.0f, 1.0f);
            }
        }
    } else if (a <= MathUtils::EPSILON) {
        // 胶囊体退化为点
        s = 0.0f;
        t = MathUtils::Clamp(f / e, 0.0f, 1.0f);
    } else {
        // 球心轨迹退化为点
        t = 0.0f;
        s = MathUtils::Clamp(-r.dot(d1) / a, 0.0f, 1.0f);
    }
    
    // 计算最近点
    Vector3 closestOnCapsule = capsuleSegStart + d1 * s;
    Vector3 closestOnSphere = sphereStart + d2 * t;
    Vector3 delta = closestOnSphere - closestOnCapsule;
    float dist = delta.norm();
    
    float radiusSum = sphereRadius + capsuleRadius;
    
    if (dist >= radiusSum) {
        // 检查是否会在 [0, dt] 内碰撞
        // 需要计算距离变化率
        Vector3 relativeVel = sphereVel - capsuleVel;
        Vector3 deltaNorm;
        if (dist > MathUtils::EPSILON) {
            deltaNorm = delta / dist;
        } else {
            deltaNorm = Vector3::UnitY();
        }
        float approachRate = -deltaNorm.dot(relativeVel);
        
        if (approachRate <= 0.0f) {
            return false;  // 正在远离
        }
        
        // 计算碰撞时间
        float toi = (dist - radiusSum) / approachRate;
        
        if (toi < 0.0f || toi > dt) {
            return false;
        }
        
        result.collided = true;
        result.toi = toi / dt;  // 归一化到 [0, 1]
        result.collisionPoint = closestOnCapsule + deltaNorm * capsuleRadius;
        result.collisionNormal = deltaNorm;
        result.penetration = 0.0f;
        
        return true;
    }
    
    // 已经相交，TOI = 0
    Vector3 deltaNorm;
    if (dist > MathUtils::EPSILON) {
        deltaNorm = delta / dist;
    } else {
        deltaNorm = Vector3::UnitY();
    }
    result.collided = true;
    result.toi = 0.0f;
    result.collisionPoint = closestOnCapsule + deltaNorm * capsuleRadius;
    result.collisionNormal = deltaNorm;
    result.penetration = radiusSum - dist;
    
    return true;
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

