# 连续碰撞检测（CCD）实现方案

> **文档版本**: v1.0  
> **创建日期**: 2025-12-09  
> **预计开发周期**: 4-6 周  
> **优先级**: 🔴 Critical（解决高速物体穿透问题）

---

## 📋 目录

1. [概述](#概述)
2. [问题分析](#问题分析)
3. [技术方案](#技术方案)
4. [实现细节](#实现细节)
5. [性能优化](#性能优化)
6. [集成方案](#集成方案)
7. [开发计划](#开发计划)
8. [测试方案](#测试方案)

---

## 概述

### 什么是连续碰撞检测（CCD）？

连续碰撞检测（Continuous Collision Detection）是一种在物理模拟中处理高速运动物体的技术。与离散碰撞检测（Discrete Collision Detection）不同，CCD 通过计算物体在两个时间点之间的运动轨迹，在碰撞发生的精确时刻停止运动，从而避免物体"穿透"（tunneling）其他物体。

### 为什么需要 CCD？

**离散碰撞检测的问题：**

```
t=0: 球在位置A (速度 v=100 m/s)
     ↓
t=1: 球在位置B (已经穿透墙壁)
     ↓
检测到碰撞，但为时已晚
```

**连续碰撞检测的解决方案：**

```
t=0: 球在位置A (速度 v=100 m/s)
     ↓
t=0.3: 检测到碰撞时间点（TOI）
     ↓
在 t=0.3 时停止并处理碰撞
```

### 应用场景

- **高速子弹**：子弹以高速度发射，需要精确碰撞检测
- **物理弹射**：弹球游戏中的高速球体
- **载具模拟**：赛车、飞机等高速运动的载具
- **薄壁障碍**：快速移动物体需要穿过薄墙或栅栏
- **小物体检测**：小的障碍物在高速运动时容易被穿透

---

## 问题分析

### 当前系统的限制

1. **离散碰撞检测（DCD）**
   - 在时间步长结束后检测碰撞
   - 如果物体在时间步内移动距离超过自身尺寸，可能完全穿透障碍物
   - 当前实现：`PhysicsUpdateSystem::FixedUpdate()` → `IntegratePosition()` → `ResolveCollisions()`

2. **穿透阈值问题**
   - 即使检测到穿透，也无法知道碰撞发生的精确时刻
   - 只能通过约束求解器修正位置，可能导致不自然的物理行为

3. **高速物体性能**
   - 测试用例 `Test_MultipleBounces_Validation` 失败可能与此相关
   - 高速物体在多次反弹中可能穿透地面

### 技术挑战

1. **TOI（Time of Impact）计算复杂度**
   - 不同形状组合需要不同的算法
   - Sphere-Sphere 相对简单
   - Box-Box、Capsule-Box 等组合较复杂

2. **性能开销**
   - CCD 比 DCD 计算量大 3-10 倍
   - 需要智能选择哪些物体启用 CCD

3. **数值精度**
   - 浮点误差可能导致 TOI 计算不准确
   - 需要稳健的数值方法

---

## 技术方案

### 架构设计

```
┌─────────────────────────────────────────────────┐
│         PhysicsUpdateSystem                      │
│  ┌──────────────────────────────────────────┐  │
│  │  Fast Moving Object Detection             │  │
│  │  - 检测需要 CCD 的物体                    │  │
│  │  - 计算扫描体积（Swept Volume）           │  │
│  └──────────────────────────────────────────┘  │
│                    ↓                            │
│  ┌──────────────────────────────────────────┐  │
│  │  CCD Pipeline                              │  │
│  │  1. Broad Phase CCD (AABB Sweep)          │  │
│  │  2. Narrow Phase CCD (Shape TOI)          │  │
│  │  3. Sub-step Integration                  │  │
│  └──────────────────────────────────────────┘  │
│                    ↓                            │
│  ┌──────────────────────────────────────────┐  │
│  │  Fallback to DCD                          │  │
│  │  - 低速物体使用标准 DCD                    │  │
│  └──────────────────────────────────────────┘  │
└─────────────────────────────────────────────────┘
```

### 核心算法

#### 1. 快速移动物体检测

**判断标准：**
- 速度阈值：`|velocity| > velocityThreshold`
- 位移阈值：`|velocity * dt| > shapeSize * thresholdRatio`
- 用户标记：`RigidBodyComponent::useCCD = true`

**实现：**
```cpp
bool ShouldUseCCD(const RigidBodyComponent& body, 
                  const ColliderComponent& collider,
                  float dt) {
    // 用户强制启用
    if (body.useCCD) return true;
    
    // 速度阈值检查
    float speed = body.linearVelocity.norm();
    if (speed < m_ccdVelocityThreshold) return false;
    
    // 位移检查
    float displacement = speed * dt;
    float shapeSize = ComputeShapeSize(collider);
    if (displacement > shapeSize * m_ccdDisplacementThreshold) {
        return true;
    }
    
    return false;
}
```

#### 2. TOI（Time of Impact）计算

**算法流程：**

```
输入：形状 A (初始位置 pA0, 速度 vA, 形状参数)
      形状 B (初始位置 pB0, 速度 vB, 形状参数)
      时间范围 [0, dt]

输出：TOI t ∈ [0, dt]，如果碰撞发生

1. 相对运动：vRel = vA - vB
2. 计算扫描体积
3. 求解 TOI 方程：distance(pA(t), pB(t)) = threshold
4. 返回最小 TOI（最早的碰撞时刻）
```

#### 3. 子步长积分

```
1. 检测到 TOI = t0 (0 < t0 < dt)
2. 积分到 t0：IntegratePosition(t0)
3. 处理碰撞：ResolveCollision(t0)
4. 更新速度：ApplyCollisionResponse()
5. 递归处理剩余时间 [t0, dt]
```

---

## 实现细节

### 阶段 1：基础架构

#### 1.1 扩展 RigidBodyComponent

```cpp
// 在 include/render/physics/physics_components.h 中添加

struct RigidBodyComponent {
    // ... 现有成员 ...
    
    /// 是否启用连续碰撞检测
    /// true: 强制启用 CCD（无论速度）
    /// false: 根据速度阈值自动判断
    bool useCCD = false;
    
    /// CCD 阈值（速度，m/s）
    /// 当 useCCD=false 时，速度超过此值自动启用 CCD
    float ccdVelocityThreshold = 10.0f;
    
    /// CCD 位移阈值（相对于形状尺寸的比例）
    /// 位移 = 速度 * dt，如果位移 > 形状尺寸 * 此值，启用 CCD
    float ccdDisplacementThreshold = 0.5f;
    
    /// 上一帧的位置（用于计算扫描体积）
    Vector3 previousPosition;
    Quaternion previousRotation;
    
    /// CCD 碰撞信息（如果发生 CCD 碰撞）
    struct CCDCollisionInfo {
        bool occurred = false;
        float toi = 0.0f;  // Time of Impact [0, 1]
        Vector3 collisionPoint;
        Vector3 collisionNormal;
        ECS::EntityID otherEntity;
    } ccdCollision;
};
```

#### 1.2 创建 CCD 检测器

**文件结构：**
```
include/render/physics/collision/
├── ccd_detector.h          # CCD 检测器接口
└── ccd_shapes.h            # CCD 形状算法

src/physics/collision/
├── ccd_detector.cpp        # CCD 检测器实现
└── ccd_shapes.cpp          # 各种形状的 CCD 算法
```

**接口设计：**
```cpp
// include/render/physics/collision/ccd_detector.h

namespace Render::Physics {

/**
 * @brief CCD 检测结果
 */
struct CCDResult {
    bool collided = false;
    float toi = 1.0f;  // Time of Impact [0, 1]
    Vector3 collisionPoint;
    Vector3 collisionNormal;
    float penetration = 0.0f;
};

/**
 * @brief 连续碰撞检测器
 */
class CCDDetector {
public:
    /**
     * @brief 检测两个形状在时间间隔内的碰撞
     * 
     * @param shapeA 形状 A
     * @param posA0 形状 A 初始位置
     * @param velA 形状 A 速度
     * @param rotA0 形状 A 初始旋转
     * @param angularVelA 形状 A 角速度
     * 
     * @param shapeB 形状 B
     * @param posB0 形状 B 初始位置
     * @param velB 形状 B 速度
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
    
private:
    // 分发到具体的形状组合算法
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
};

} // namespace Render::Physics
```

### 阶段 2：形状算法实现

#### 2.1 Sphere vs Sphere CCD

**算法：**
- 相对运动：`vRel = vA - vB`
- 相对位置：`pRel(t) = (pA0 - pB0) + vRel * t`
- 碰撞条件：`|pRel(t)| = rA + rB`
- TOI 方程：`|p0 + v*t|² = (rA + rB)²`

**实现：**
```cpp
bool SphereVsSphereCCD(
    const Vector3& posA0, float radiusA, const Vector3& velA,
    const Vector3& posB0, float radiusB, const Vector3& velB,
    float dt,
    CCDResult& result
) {
    Vector3 p0 = posA0 - posB0;
    Vector3 v = velA - velB;
    
    float rSum = radiusA + radiusB;
    float rSumSq = rSum * rSum;
    
    // 二次方程：|p0 + v*t|² = rSum²
    // 展开：(p0·p0) + 2*(p0·v)*t + (v·v)*t² = rSum²
    float a = v.squaredNorm();
    float b = 2.0f * p0.dot(v);
    float c = p0.squaredNorm() - rSumSq;
    
    // 求解 at² + bt + c = 0
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
        return false;
    }
    
    // 计算碰撞点和法线
    Vector3 pAtTOI = posA0 + velA * toi;
    Vector3 pBtTOI = posB0 + velB * toi;
    Vector3 delta = pAtTOI - pBtTOI;
    float dist = delta.norm();
    
    if (dist < MathUtils::EPSILON) {
        result.collisionNormal = Vector3::UnitY();
    } else {
        result.collisionNormal = delta / dist;
    }
    
    result.collided = true;
    result.toi = toi;
    result.collisionPoint = pBtTOI + result.collisionNormal * radiusB;
    result.penetration = 0.0f;  // CCD 在接触时刻停止，无穿透
    
    return true;
}
```

#### 2.2 Sphere vs Box CCD

**算法：**
- 将球体运动轨迹视为胶囊体（扫描体积）
- 检测胶囊体与盒体的碰撞
- 使用分离轴定理（SAT）的连续版本

**简化方案（推荐）：**
- 使用保守估计：扩大盒体尺寸（加上球半径）
- 检测球心轨迹与扩大盒体的碰撞
- 使用线段-盒体碰撞检测

```cpp
bool SphereVsBoxCCD(
    const Vector3& spherePos0, float sphereRadius, const Vector3& sphereVel,
    const Vector3& boxCenter, const Vector3& boxHalfExtents,
    const Quaternion& boxRotation, const Vector3& boxVel,
    float dt,
    CCDResult& result
) {
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
    float toi = 1.0f;
    Vector3 normal = Vector3::Zero();
    bool foundCollision = false;
    
    // 对每个轴进行检测
    for (int axis = 0; axis < 3; ++axis) {
        float axisMin = -expandedHalfExtents[axis];
        float axisMax = expandedHalfExtents[axis];
        
        float startVal = localStart[axis];
        float endVal = localEnd[axis];
        
        // 如果线段完全在盒体外，跳过
        if ((startVal < axisMin && endVal < axisMin) ||
            (startVal > axisMax && endVal > axisMax)) {
            continue;
        }
        
        // 计算进入和离开时间
        float tEnter = 0.0f;
        float tExit = 1.0f;
        
        if (std::abs(localVel[axis]) > MathUtils::EPSILON) {
            tEnter = (axisMin - startVal) / localVel[axis];
            tExit = (axisMax - startVal) / localVel[axis];
            
            if (tEnter > tExit) std::swap(tEnter, tExit);
        } else {
            // 速度为零，检查是否在范围内
            if (startVal < axisMin || startVal > axisMax) {
                continue;  // 不在范围内
            }
        }
        
        // 更新 TOI
        if (tEnter >= 0.0f && tEnter < toi) {
            toi = tEnter;
            
            // 确定碰撞法线
            normal = Vector3::Zero();
            normal[axis] = (startVal < 0.0f) ? -1.0f : 1.0f;
            normal = rotMatrix * normal;  // 转换到世界空间
            normal.normalize();
            
            foundCollision = true;
        }
    }
    
    if (!foundCollision || toi > dt) {
        return false;
    }
    
    result.collided = true;
    result.toi = toi;
    result.collisionPoint = spherePos0 + sphereVel * toi - normal * sphereRadius;
    result.collisionNormal = normal;
    result.penetration = 0.0f;
    
    return true;
}
```

#### 2.3 Sphere vs Capsule CCD

**算法：**
- 将球体轨迹视为胶囊体
- 检测两个胶囊体之间的碰撞
- 计算两条线段之间的最近距离随时间的变化

```cpp
bool SphereVsCapsuleCCD(
    const Vector3& spherePos0, float sphereRadius, const Vector3& sphereVel,
    const Vector3& capsuleCenter, float capsuleRadius, float capsuleHeight,
    const Quaternion& capsuleRotation, const Vector3& capsuleVel,
    float dt,
    CCDResult& result
) {
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
    // 胶囊线段：p(t) = capsuleSegStart + t * (capsuleSegEnd - capsuleSegStart)
    // 球心线段：q(s) = sphereStart + s * (sphereEnd - sphereStart)
    
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
        float approachRate = -delta.normalized().dot(relativeVel);
        
        if (approachRate <= 0.0f) {
            return false;  // 正在远离
        }
        
        // 计算碰撞时间
        float toi = (dist - radiusSum) / approachRate;
        
        if (toi < 0.0f || toi > dt) {
            return false;
        }
        
        result.collided = true;
        result.toi = toi;
        result.collisionPoint = closestOnCapsule + delta.normalized() * capsuleRadius;
        result.collisionNormal = delta.normalized();
        result.penetration = 0.0f;
        
        return true;
    }
    
    // 已经相交，TOI = 0
    result.collided = true;
    result.toi = 0.0f;
    result.collisionPoint = closestOnCapsule + delta.normalized() * capsuleRadius;
    result.collisionNormal = (dist > MathUtils::EPSILON) ? delta / dist : Vector3::UnitY();
    result.penetration = radiusSum - dist;
    
    return true;
}
```

#### 2.4 Box vs Box CCD

**算法：**
- 使用保守推进（Conservative Advancement）
- 基于分离轴定理（SAT）的连续版本
- 对每个分离轴，计算碰撞时间

**实现策略：**
- 简化方案：使用 AABB 扫描（扩大 Box A，检测 Box B 轨迹）
- 精确方案：使用 OBB 连续碰撞检测（更复杂）

```cpp
bool BoxVsBoxCCD(
    const Vector3& boxA0, const Vector3& boxAHalfExtents, 
    const Quaternion& boxARot0, const Vector3& boxAVel, const Vector3& boxAAngularVel,
    const Vector3& boxB0, const Vector3& boxBHalfExtents,
    const Quaternion& boxBRot0, const Vector3& boxBVel, const Vector3& boxBAngularVel,
    float dt,
    CCDResult& result
) {
    // 简化：假设旋转影响较小，使用线性运动
    // 精确实现需要考虑角速度
    
    Vector3 relativeVel = boxAVel - boxBVel;
    
    // 使用保守推进：逐步推进，检测碰撞
    const int maxIterations = 10;
    float currentTime = 0.0f;
    float stepSize = dt / maxIterations;
    
    for (int i = 0; i < maxIterations; ++i) {
        float testTime = currentTime + stepSize;
        
        // 计算当前时刻的位置
        Vector3 boxAPos = boxA0 + boxAVel * testTime;
        Vector3 boxBPos = boxB0 + boxBVel * testTime;
        
        // 使用离散碰撞检测
        ContactManifold manifold;
        bool collided = CollisionDetector::BoxVsBox(
            boxAPos, boxAHalfExtents, boxARot0,
            boxBPos, boxBHalfExtents, boxBRot0,
            manifold
        );
        
        if (collided) {
            // 二分查找精确的 TOI
            float low = currentTime;
            float high = testTime;
            float toi = testTime;
            
            for (int j = 0; j < 8; ++j) {
                float mid = (low + high) * 0.5f;
                Vector3 midAPos = boxA0 + boxAVel * mid;
                Vector3 midBPos = boxB0 + boxBVel * mid;
                
                ContactManifold midManifold;
                bool midCollided = CollisionDetector::BoxVsBox(
                    midAPos, boxAHalfExtents, boxARot0,
                    midBPos, boxBHalfExtents, boxBRot0,
                    midManifold
                );
                
                if (midCollided) {
                    toi = mid;
                    high = mid;
                } else {
                    low = mid;
                }
            }
            
            result.collided = true;
            result.toi = toi;
            
            // 计算碰撞时的位置和法线
            Vector3 collisionAPos = boxA0 + boxAVel * toi;
            Vector3 collisionBPos = boxB0 + boxBVel * toi;
            
            ContactManifold collisionManifold;
            CollisionDetector::BoxVsBox(
                collisionAPos, boxAHalfExtents, boxARot0,
                collisionBPos, boxBHalfExtents, boxBRot0,
                collisionManifold
            );
            
            result.collisionNormal = collisionManifold.normal;
            result.collisionPoint = collisionManifold.contacts[0].point;
            result.penetration = 0.0f;
            
            return true;
        }
        
        currentTime = testTime;
    }
    
    return false;
}
```

#### 2.5 Capsule vs Capsule / Capsule vs Box CCD

**实现策略：**
- Capsule vs Capsule：基于线段-线段距离计算
- Capsule vs Box：将 Capsule 简化为线段，使用线段-盒体碰撞

（具体实现类似上述方法，此处省略详细代码）

### 阶段 3：集成到物理系统

#### 3.1 修改 PhysicsUpdateSystem

```cpp
// src/physics/physics_update_system.cpp

void PhysicsUpdateSystem::FixedUpdate(float dt) {
    // 1. 应用力和重力
    ApplyForces(dt);
    
    // 2. 积分速度
    IntegrateVelocity(dt);
    
    // 3. 检测需要 CCD 的物体
    std::vector<CCDCandidate> ccdCandidates = DetectCCDCandidates(dt);
    
    if (!ccdCandidates.empty() && m_config.enableCCD) {
        // 4. CCD 路径积分
        IntegrateWithCCD(dt, ccdCandidates);
    } else {
        // 5. 标准积分（DCD）
        IntegratePosition(dt);
    }
    
    // 6. 碰撞结果处理
    ResolveCollisions(dt);
    
    // 7. 约束求解
    SolveConstraints(dt);
    
    // 8. 休眠检测
    UpdateSleepingState(dt);
    
    // 9. 更新 AABB
    UpdateAABBs();
}

/**
 * @brief 检测需要 CCD 的物体
 */
std::vector<CCDCandidate> PhysicsUpdateSystem::DetectCCDCandidates(float dt) {
    std::vector<CCDCandidate> candidates;
    auto entities = m_world->Query<ECS::TransformComponent, RigidBodyComponent, ColliderComponent>();
    
    for (ECS::EntityID entity : entities) {
        auto& body = m_world->GetComponent<RigidBodyComponent>(entity);
        auto& collider = m_world->GetComponent<ColliderComponent>(entity);
        
        if (ShouldUseCCD(body, collider, dt)) {
            CCDCandidate candidate;
            candidate.entity = entity;
            candidate.previousPosition = body.previousPosition;
            candidate.currentPosition = /* 计算预测位置 */;
            candidate.velocity = body.linearVelocity;
            candidate.angularVelocity = body.angularVelocity;
            candidates.push_back(candidate);
        }
    }
    
    return candidates;
}

/**
 * @brief 使用 CCD 进行路径积分
 */
void PhysicsUpdateSystem::IntegrateWithCCD(float dt, 
                                           const std::vector<CCDCandidate>& candidates) {
    // 对所有 CCD 候选物体进行检测
    for (const auto& candidate : candidates) {
        // 检测与所有其他物体的 CCD 碰撞
        std::vector<CCDResult> collisions = PerformCCDDetection(candidate, dt);
        
        if (!collisions.empty()) {
            // 找到最早的碰撞
            auto earliest = std::min_element(
                collisions.begin(), collisions.end(),
                [](const CCDResult& a, const CCDResult& b) {
                    return a.toi < b.toi;
                }
            );
            
            // 积分到 TOI
            float toi = earliest->toi * dt;
            IntegratePositionToTime(candidate.entity, toi);
            
            // 处理碰撞
            HandleCCDCollision(candidate.entity, *earliest);
            
            // 递归处理剩余时间
            if (toi < dt - MathUtils::EPSILON) {
                float remainingTime = dt - toi;
                // 更新速度后继续 CCD
                // （简化：剩余时间使用标准积分）
                IntegratePosition(candidate.entity, remainingTime);
            }
        } else {
            // 无碰撞，使用标准积分
            IntegratePosition(candidate.entity, dt);
        }
    }
    
    // 对非 CCD 物体使用标准积分
    // ...
}
```

#### 3.2 扩展 CollisionDetectionSystem

```cpp
// src/physics/collision/collision_detection_system.cpp

void CollisionDetectionSystem::Update(float deltaTime) {
    // ... 现有代码 ...
    
    // 如果启用 CCD，使用 CCD 检测
    if (m_ccdEnabled) {
        PerformCCDDetection(deltaTime);
    } else {
        PerformStandardDetection(deltaTime);
    }
    
    // ... 其余代码 ...
}
```

---

## 性能优化

### 1. 选择性 CCD

**策略：**
- 只对高速物体启用 CCD
- 使用速度阈值和位移阈值筛选
- 允许用户手动标记需要 CCD 的物体

**实现：**
```cpp
// 配置参数
struct CCDConfig {
    float velocityThreshold = 10.0f;      // m/s
    float displacementThreshold = 0.5f;   // 相对尺寸比例
    int maxCCDObjects = 50;               // 每帧最大 CCD 对象数
    bool enableBroadPhaseCCD = true;      // 使用粗检测加速
};
```

### 2. Broad Phase CCD

**AABB 扫描检测：**
- 计算物体的扫描 AABB（从 t=0 到 t=dt）
- 使用空间哈希快速筛选潜在碰撞对
- 只对 Broad Phase 筛选出的对进行精确 CCD

```cpp
AABB ComputeSweptAABB(const AABB& aabb0, const Vector3& velocity, float dt) {
    Vector3 min0 = aabb0.min;
    Vector3 max0 = aabb0.max;
    Vector3 min1 = min0 + velocity * dt;
    Vector3 max1 = max0 + velocity * dt;
    
    return AABB(
        min0.cwiseMin(min1),
        max0.cwiseMax(max1)
    );
}
```

### 3. 缓存优化

- 缓存上一帧的位置和旋转
- 重用计算结果（如果物体状态未变化）
- 使用对象池减少内存分配

### 4. 并行化

- Broad Phase CCD 可以并行执行
- 不同物体对的 CCD 检测可以并行
- 使用 OpenMP 或线程池

```cpp
#pragma omp parallel for
for (size_t i = 0; i < ccdPairs.size(); ++i) {
    PerformCCDDetection(ccdPairs[i]);
}
```

### 5. 数值优化

- 使用保守估计避免复杂计算
- 对简单形状（Sphere）使用解析解
- 对复杂形状使用迭代方法（二分查找）

---

## 集成方案

### 配置系统

```cpp
// include/render/physics/physics_config.h

struct PhysicsConfig {
    // ... 现有配置 ...
    
    /// CCD 配置
    struct {
        /// 是否启用 CCD
        bool enableCCD = false;
        
        /// 速度阈值（m/s）
        float velocityThreshold = 10.0f;
        
        /// 位移阈值（相对于形状尺寸）
        float displacementThreshold = 0.5f;
        
        /// 最大 CCD 对象数（性能限制）
        int maxCCDObjects = 50;
        
        /// 最大子步数（防止性能爆炸）
        int maxSubSteps = 5;
        
        /// 是否启用 Broad Phase CCD
        bool enableBroadPhaseCCD = true;
    } ccd;
};
```

### API 使用示例

```cpp
// 创建物理世界
auto world = std::make_shared<World>();
world->Initialize();

// 配置 CCD
PhysicsConfig config;
config.ccd.enableCCD = true;
config.ccd.velocityThreshold = 15.0f;

// 创建高速物体
EntityID bullet = world->CreateEntity();
auto& body = world->AddComponent<RigidBodyComponent>(bullet);
body.useCCD = true;  // 强制启用 CCD
body.linearVelocity = Vector3(100.0f, 0.0f, 0.0f);  // 高速
```

---

## 开发计划

### 阶段 1：基础架构（Week 1-2）

**目标：** 建立 CCD 系统框架

- [ ] **1.1** 扩展 `RigidBodyComponent`（添加 CCD 相关字段）
- [ ] **1.2** 创建 `CCDDetector` 类和接口
- [ ] **1.3** 实现 CCD 配置系统
- [ ] **1.4** 实现快速移动物体检测
- [ ] **1.5** 单元测试：基础架构测试

**交付物：**
- `include/render/physics/collision/ccd_detector.h`
- `src/physics/collision/ccd_detector.cpp`
- 更新的 `physics_components.h`
- 基础测试用例

### 阶段 2：Sphere CCD（Week 2-3）

**目标：** 实现 Sphere 相关的 CCD 算法

- [ ] **2.1** 实现 `SphereVsSphereCCD`
- [ ] **2.2** 实现 `SphereVsBoxCCD`
- [ ] **2.3** 实现 `SphereVsCapsuleCCD`
- [ ] **2.4** 单元测试：Sphere CCD 测试
- [ ] **2.5** 性能测试和优化

**交付物：**
- Sphere 相关 CCD 算法实现
- 测试用例：`test_ccd_sphere.cpp`
- 性能基准测试结果

### 阶段 3：Box 和 Capsule CCD（Week 3-4）

**目标：** 实现复杂形状的 CCD

- [ ] **3.1** 实现 `BoxVsBoxCCD`（简化版）
- [ ] **3.2** 实现 `CapsuleVsCapsuleCCD`
- [ ] **3.3** 实现 `CapsuleVsBoxCCD`
- [ ] **3.4** 单元测试：复杂形状 CCD
- [ ] **3.5** 边界情况处理

**交付物：**
- Box 和 Capsule CCD 算法实现
- 测试用例：`test_ccd_complex_shapes.cpp`
- 边界情况测试报告

### 阶段 4：系统集成（Week 4-5）

**目标：** 将 CCD 集成到物理更新系统

- [ ] **4.1** 修改 `PhysicsUpdateSystem::FixedUpdate()`
- [ ] **4.2** 实现 `IntegrateWithCCD()`
- [ ] **4.3** 实现子步长积分
- [ ] **4.4** 集成到 `CollisionDetectionSystem`
- [ ] **4.5** 端到端测试

**交付物：**
- 更新的 `physics_update_system.cpp`
- 更新的 `collision_detection_system.cpp`
- 集成测试用例
- 性能对比报告（CCD vs DCD）

### 阶段 5：优化和测试（Week 5-6）

**目标：** 性能优化和全面测试

- [ ] **5.1** 实现 Broad Phase CCD
- [ ] **5.2** 实现并行化优化
- [ ] **5.3** 缓存优化
- [ ] **5.4** 全面测试（包括 `Test_MultipleBounces_Validation`）
- [ ] **5.5** 文档更新

**交付物：**
- 性能优化代码
- 完整的测试套件
- API 文档更新
- 性能基准报告

### 里程碑检查点

**Week 2 检查点：**
- [ ] 基础架构完成
- [ ] Sphere CCD 算法实现
- [ ] 基础测试通过

**Week 4 检查点：**
- [ ] 所有形状 CCD 算法实现
- [ ] 系统集成完成
- [ ] 端到端测试通过

**Week 6 检查点：**
- [ ] 性能优化完成
- [ ] 所有测试通过（包括现有测试）
- [ ] 文档完整

---

## 测试方案

### 单元测试

#### 1. Sphere vs Sphere CCD

```cpp
TEST(CCD, SphereVsSphere) {
    // 测试场景：高速球体碰撞
    Vector3 posA0(0.0f, 0.0f, 0.0f);
    float radiusA = 0.5f;
    Vector3 velA(10.0f, 0.0f, 0.0f);
    
    Vector3 posB0(5.0f, 0.0f, 0.0f);
    float radiusB = 0.5f;
    Vector3 velB(0.0f, 0.0f, 0.0f);
    
    CCDResult result;
    bool collided = CCDDetector::SphereVsSphereCCD(
        posA0, radiusA, velA,
        posB0, radiusB, velB,
        1.0f, result
    );
    
    ASSERT_TRUE(collided);
    ASSERT_GT(result.toi, 0.0f);
    ASSERT_LT(result.toi, 1.0f);
    // 验证碰撞点在正确位置
}
```

#### 2. 穿透测试

```cpp
TEST(CCD, NoTunneling) {
    // 测试场景：高速球体不应该穿透薄墙
    // 使用 DCD 会穿透，CCD 应该检测到碰撞
    // ...
}
```

### 集成测试

#### 1. 高速反弹测试

```cpp
TEST(Physics, HighSpeedBounce) {
    // 重现 Test_MultipleBounces_Validation 场景
    // 使用 CCD 后应该能正确检测多次反弹
    // ...
}
```

#### 2. 性能测试

```cpp
TEST(CCD, Performance) {
    // 测试 CCD 性能开销
    // 对比 DCD 和 CCD 的帧时间
    // ...
}
```

### 边界情况测试

- 零速度物体
- 平行运动物体
- 已经相交的物体（TOI = 0）
- 非常大的时间步长
- 数值精度问题

---

## 参考资料

### 学术论文

1. **"Continuous Collision Detection"** - Erwin Coumans (Bullet Physics)
2. **"Conservative Advancement for Continuous Collision Detection"** - M. Tang et al.
3. **"Fast Continuous Collision Detection using Deforming Non-Penetration Filters"** - M. Tang et al.

### 开源实现参考

1. **Bullet Physics** - `btContinuousConvexCollision`
2. **Box2D** - `b2TimeOfImpact`
3. **PhysX** - Continuous Collision Detection API

### 算法参考

1. **Sphere-Sphere CCD**: 二次方程求解
2. **Sphere-Box CCD**: 线段-AABB 碰撞
3. **Box-Box CCD**: 保守推进 + 二分查找
4. **Swept Volume**: 扫描体积计算

---

## 附录

### A. 数学公式

#### Sphere-Sphere TOI

给定：
- 球 A：位置 `pA0`，速度 `vA`，半径 `rA`
- 球 B：位置 `pB0`，速度 `vB`，半径 `rB`

相对运动：
- `p0 = pA0 - pB0`
- `v = vA - vB`

碰撞条件：
```
|p0 + v*t| = rA + rB
```

二次方程：
```
(v·v)*t² + 2*(p0·v)*t + (p0·p0) - (rA + rB)² = 0
```

#### 线段-AABB 碰撞

给定线段 `L(t) = p0 + v*t`，AABB `[min, max]`

对每个轴 i：
```
t_enter[i] = (min[i] - p0[i]) / v[i]
t_exit[i] = (max[i] - p0[i]) / v[i]
```

TOI = max(t_enter)，如果 TOI < min(t_exit)

### B. 代码结构

```
include/render/physics/
├── collision/
│   ├── ccd_detector.h
│   └── ccd_shapes.h
└── physics_config.h (扩展)

src/physics/
├── collision/
│   ├── ccd_detector.cpp
│   └── ccd_shapes.cpp
└── physics_update_system.cpp (修改)

tests/
└── test_ccd_*.cpp
```

### C. 配置参数建议

```cpp
// 默认配置（平衡性能和准确性）
ccd.velocityThreshold = 10.0f;        // 10 m/s
ccd.displacementThreshold = 0.5f;     // 50% 形状尺寸
ccd.maxCCDObjects = 50;
ccd.maxSubSteps = 5;

// 高性能配置（更多对象）
ccd.maxCCDObjects = 100;
ccd.enableBroadPhaseCCD = true;

// 高精度配置（更严格的阈值）
ccd.velocityThreshold = 5.0f;
ccd.displacementThreshold = 0.3f;
```

---

## 总结

本方案提供了完整的 CCD 实现路径，包括：

1. **理论基础**：CCD 原理和必要性
2. **技术方案**：针对各种形状的算法实现
3. **系统集成**：与现有物理系统的无缝集成
4. **性能优化**：选择性 CCD、并行化等策略
5. **开发计划**：分阶段的实施路线图

通过实施本方案，物理引擎将能够：
- ✅ 正确处理高速运动物体
- ✅ 避免穿透问题
- ✅ 提高物理模拟的真实感
- ✅ 解决现有测试失败问题（如 `Test_MultipleBounces_Validation`）

**预计开发时间：** 4-6 周  
**优先级：** 🔴 Critical  
**依赖：** 现有碰撞检测系统、物理更新系统

---

**文档维护者：** Linductor 李朝宇  
**最后更新：** 2025-12-09
