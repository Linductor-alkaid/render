# Bullet Physics 迁移方案文档

> **版本**: v1.0  
> **日期**: 2025-12-10  
> **目标**: 将自定义物理引擎替换为 Bullet Physics，保持 API 向后兼容

---

## 目录

1. [概述](#概述)
2. [设计目标](#设计目标)
3. [架构设计](#架构设计)
4. [API 映射关系](#api-映射关系)
5. [数据转换层](#数据转换层)
6. [实现步骤](#实现步骤)
7. [CMake 集成](#cmake-集成)
8. [测试策略](#测试策略)
9. [迁移检查清单](#迁移检查清单)
10. [常见问题](#常见问题)

---

## 概述

### 背景

当前项目使用自定义物理引擎实现，包括：
- 碰撞检测系统（SAT、GJK/EPA）
- 刚体动力学（半隐式欧拉积分）
- 约束求解器（序列冲量法）
- ECS 集成

为了提升物理模拟的稳定性和性能，计划将底层实现替换为 Bullet Physics 3，同时保持现有的 API 接口不变，实现零破坏迁移。

### Bullet Physics 简介

Bullet Physics 是一个开源的物理模拟库，广泛应用于游戏开发和电影制作：
- ✅ 成熟的碰撞检测算法
- ✅ 高性能的约束求解器
- ✅ 支持连续碰撞检测（CCD）
- ✅ 多线程优化
- ✅ 丰富的关节类型
- ✅ 活跃的社区支持

---

## 设计目标

### 核心原则

1. **零破坏迁移**：现有代码无需修改即可使用 Bullet 后端
2. **API 兼容性**：保持所有现有 API 接口不变
3. **行为一致性**：物理行为与原有引擎保持一致
4. **性能提升**：利用 Bullet 的优化提升性能
5. **可扩展性**：保留未来扩展的可能性

### 成功标准

- ✅ 所有现有测试通过
- ✅ 物理行为视觉上无明显差异
- ✅ 性能不低于原有实现（理想情况下提升）
- ✅ 代码编译无警告
- ✅ API 文档无需更新

---

## 架构设计

### 整体架构

```
┌─────────────────────────────────────────────────────────┐
│              现有 API 层（保持不变）                      │
│  RigidBodyComponent, ColliderComponent, PhysicsWorld   │
└─────────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────┐
│              适配层（Adapter Layer）                      │
│  - BulletPhysicsAdapter                                 │
│  - BulletShapeAdapter                                    │
│  - BulletRigidBodyAdapter                                │
│  - BulletWorldAdapter                                    │
└─────────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────┐
│              数据转换层（Conversion Layer）               │
│  - EigenToBullet (Vector3, Quaternion, Matrix3)         │
│  - BulletToEigen                                         │
└─────────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────┐
│              Bullet Physics 3                            │
│  - btDiscreteDynamicsWorld                               │
│  - btRigidBody                                           │
│  - btCollisionShape                                      │
│  - btSequentialImpulseConstraintSolver                  │
└─────────────────────────────────────────────────────────┘
```

### 适配层设计

#### 1. BulletWorldAdapter

封装 `btDiscreteDynamicsWorld`，提供与 `PhysicsWorld` 相同的接口。

```cpp
class BulletWorldAdapter {
private:
    std::unique_ptr<btDiscreteDynamicsWorld> m_bulletWorld;
    std::unique_ptr<btBroadphaseInterface> m_broadphase;
    std::unique_ptr<btCollisionDispatcher> m_dispatcher;
    std::unique_ptr<btConstraintSolver> m_solver;
    std::unique_ptr<btCollisionConfiguration> m_collisionConfig;
    
    // 实体到 Bullet 刚体的映射
    std::unordered_map<ECS::EntityID, btRigidBody*, ECS::EntityID::Hash> m_entityToRigidBody;
    std::unordered_map<btRigidBody*, ECS::EntityID> m_rigidBodyToEntity;
    
public:
    void Step(float deltaTime);
    void SetGravity(const Vector3& gravity);
    // ... 其他接口
};
```

#### 2. BulletRigidBodyAdapter

封装 `btRigidBody`，桥接 `RigidBodyComponent` 和 Bullet 刚体。

```cpp
class BulletRigidBodyAdapter {
private:
    btRigidBody* m_bulletBody;
    ECS::EntityID m_entity;
    
public:
    // 同步 RigidBodyComponent -> btRigidBody
    void SyncToBullet(const RigidBodyComponent& component);
    
    // 同步 btRigidBody -> RigidBodyComponent
    void SyncFromBullet(RigidBodyComponent& component);
};
```

#### 3. BulletShapeAdapter

封装 `btCollisionShape`，桥接 `ColliderComponent` 和 Bullet 形状。

```cpp
class BulletShapeAdapter {
public:
    static btCollisionShape* CreateShape(const ColliderComponent& collider);
    static void UpdateShape(btCollisionShape* shape, const ColliderComponent& collider);
};
```

---

## API 映射关系

### 核心类映射

| 现有类 | Bullet 类 | 说明 |
|--------|----------|------|
| `PhysicsWorld` | `btDiscreteDynamicsWorld` | 物理世界 |
| `RigidBodyComponent` | `btRigidBody` | 刚体 |
| `ColliderComponent` | `btCollisionShape` | 碰撞形状 |
| `PhysicsMaterial` | `btMaterial` (通过 `btCollisionObject::setFriction/restitution`) | 物理材质 |
| `PhysicsConfig` | `btDiscreteDynamicsWorld` 配置参数 | 物理配置 |
| `ForceFieldComponent` | 自定义实现（通过 `btRigidBody::applyForce`） | 力场 |
| `PhysicsJointComponent` | `btTypedConstraint` (各种关节类型) | 关节约束 |

### 形状类型映射

| 现有形状类型 | Bullet 形状类 | 说明 |
|-------------|--------------|------|
| `ShapeType::Sphere` | `btSphereShape` | 球体 |
| `ShapeType::Box` | `btBoxShape` | 盒体 |
| `ShapeType::Capsule` | `btCapsuleShape` | 胶囊体 |
| `ShapeType::ConvexHull` | `btConvexHullShape` | 凸包 |
| `ShapeType::Mesh` | `btBvhTriangleMeshShape` | 凹面网格 |

### 刚体类型映射

| 现有 BodyType | Bullet 标志 | 说明 |
|--------------|------------|------|
| `BodyType::Static` | `CF_STATIC_OBJECT` | 静态物体，质量 = 0 |
| `BodyType::Kinematic` | `CF_KINEMATIC_OBJECT` | 运动学物体，质量 = 0 |
| `BodyType::Dynamic` | 默认（无特殊标志） | 动态物体，质量 > 0 |

### 属性映射

#### RigidBodyComponent → btRigidBody

| 现有属性 | Bullet 方法/属性 | 说明 |
|---------|-----------------|------|
| `mass` | `btRigidBody::getMass()` / `setMassProps()` | 质量 |
| `inverseMass` | `btRigidBody::getInvMass()` | 逆质量（自动计算） |
| `linearVelocity` | `btRigidBody::getLinearVelocity()` / `setLinearVelocity()` | 线速度 |
| `angularVelocity` | `btRigidBody::getAngularVelocity()` / `setAngularVelocity()` | 角速度 |
| `force` | `btRigidBody::applyForce()` | 力（累积） |
| `torque` | `btRigidBody::applyTorque()` | 扭矩（累积） |
| `linearDamping` | `btRigidBody::setDamping()` | 线性阻尼 |
| `angularDamping` | `btRigidBody::setDamping()` | 角阻尼 |
| `useGravity` | `btRigidBody::setGravity()` / `CF_DISABLE_GRAVITY` | 是否受重力 |
| `gravityScale` | 自定义实现（在应用重力时乘以） | 重力缩放 |
| `isSleeping` | `btRigidBody::getActivationState()` | 休眠状态 |
| `lockPosition[3]` | `btRigidBody::setLinearFactor()` | 位置锁定 |
| `lockRotation[3]` | `btRigidBody::setAngularFactor()` | 旋转锁定 |
| `inertiaTensor` | `btRigidBody::getMassProps()` | 惯性张量 |
| `useCCD` | `btRigidBody::setCcdMotionThreshold()` | 连续碰撞检测 |

#### ColliderComponent → btCollisionShape

| 现有属性 | Bullet 方法/属性 | 说明 |
|---------|-----------------|------|
| `shapeType` | `btCollisionShape::getShapeType()` | 形状类型 |
| `shapeData.sphere.radius` | `btSphereShape::getRadius()` | 球体半径 |
| `shapeData.box.halfExtents` | `btBoxShape::getHalfExtentsWithoutMargin()` | 盒体半尺寸 |
| `shapeData.capsule.radius` | `btCapsuleShape::getRadius()` | 胶囊半径 |
| `shapeData.capsule.height` | `btCapsuleShape::getHalfHeight()` * 2 | 胶囊高度 |
| `meshData` | `btBvhTriangleMeshShape` | 网格数据 |
| `center` | `btTransform` (局部偏移) | 碰撞体中心偏移 |
| `rotation` | `btTransform` (局部旋转) | 碰撞体旋转偏移 |
| `isTrigger` | `btCollisionObject::setCollisionFlags(CF_NO_CONTACT_RESPONSE)` | 触发器 |
| `collisionLayer` | `btCollisionObject::getBroadphaseHandle()->m_collisionFilterGroup` | 碰撞层 |
| `collisionMask` | `btCollisionObject::getBroadphaseHandle()->m_collisionFilterMask` | 碰撞掩码 |

#### PhysicsMaterial → Bullet 材质

| 现有属性 | Bullet 方法/属性 | 说明 |
|---------|-----------------|------|
| `friction` | `btCollisionObject::setFriction()` | 摩擦系数 |
| `restitution` | `btCollisionObject::setRestitution()` | 弹性系数 |
| `density` | 用于计算质量（不直接映射） | 密度 |
| `frictionCombine` | `btManifoldPoint::m_combinedFriction` (自定义计算) | 摩擦组合模式 |
| `restitutionCombine` | `btManifoldPoint::m_combinedRestitution` (自定义计算) | 弹性组合模式 |

#### PhysicsConfig → Bullet 配置

| 现有属性 | Bullet 方法/属性 | 说明 |
|---------|-----------------|------|
| `gravity` | `btDiscreteDynamicsWorld::setGravity()` | 重力 |
| `fixedDeltaTime` | `btDiscreteDynamicsWorld::stepSimulation(deltaTime)` | 固定时间步长 |
| `solverIterations` | `btDiscreteDynamicsWorld::getSolverInfo().m_numIterations` | 求解器迭代次数 |
| `positionIterations` | `btDiscreteDynamicsWorld::getSolverInfo().m_numIterations` | 位置迭代次数 |
| `enableCCD` | `btDiscreteDynamicsWorld::getDispatchInfo().m_useContinuous` | 启用 CCD |
| `enableSleeping` | `btDiscreteDynamicsWorld::getDispatchInfo().m_enableSPU` | 启用休眠 |
| `sleepThreshold` | `btRigidBody::setSleepingThresholds()` | 休眠阈值 |

#### PhysicsJointComponent → btTypedConstraint

| 现有关节类型 | Bullet 约束类 | 说明 |
|------------|--------------|------|
| `JointType::Fixed` | `btFixedConstraint` | 固定关节 |
| `JointType::Hinge` | `btHingeConstraint` | 铰链关节 |
| `JointType::Distance` | `btPoint2PointConstraint` 或 `btGeneric6DofSpringConstraint` | 距离关节 |
| `JointType::Spring` | `btGeneric6DofSpringConstraint` | 弹簧关节 |
| `JointType::Slider` | `btSliderConstraint` | 滑动关节 |

**关节属性映射**：

| 现有属性 | Bullet 方法/属性 | 说明 |
|---------|-----------------|------|
| `localAnchorA` / `localAnchorB` | `btTypedConstraint::getFrameOffsetA/B()` | 局部锚点 |
| `connectedBody` | `btTypedConstraint::getRigidBodyB()` | 连接的刚体 |
| `breakForce` / `breakTorque` | `btTypedConstraint::setBreakingImpulseThreshold()` | 断裂阈值 |
| `isEnabled` | `btTypedConstraint::setEnabled()` | 启用/禁用 |
| `enableCollision` | `btTypedConstraint::setDbgDrawSize()` (通过 `btDiscreteDynamicsWorld::getPairCache()`) | 碰撞控制 |

**铰链关节特定属性**：

| 现有属性 | Bullet 方法/属性 | 说明 |
|---------|-----------------|------|
| `localAxisA` / `localAxisB` | `btHingeConstraint::getFrameOffsetA/B()` | 旋转轴 |
| `hasLimits` | `btHingeConstraint::setLimit()` | 角度限制 |
| `limitMin` / `limitMax` | `btHingeConstraint::setLimit(min, max)` | 角度范围 |
| `useMotor` | `btHingeConstraint::enableMotor()` | 启用马达 |
| `motorSpeed` | `btHingeConstraint::setMotorTargetVelocity()` | 目标角速度 |
| `motorMaxForce` | `btHingeConstraint::setMaxMotorImpulse()` | 最大马达力矩 |

**距离关节特定属性**：

| 现有属性 | Bullet 方法/属性 | 说明 |
|---------|-----------------|------|
| `restLength` | `btPoint2PointConstraint` 或自定义计算 | 静止长度 |
| `minDistance` / `maxDistance` | `btGeneric6DofSpringConstraint::setLimit()` | 距离限制 |

**弹簧关节特定属性**：

| 现有属性 | Bullet 方法/属性 | 说明 |
|---------|-----------------|------|
| `restLength` | `btGeneric6DofSpringConstraint::setEquilibriumPoint()` | 平衡长度 |
| `stiffness` | `btGeneric6DofSpringConstraint::setStiffness()` | 刚度系数 |
| `damping` | `btGeneric6DofSpringConstraint::setDamping()` | 阻尼系数 |

**滑动关节特定属性**：

| 现有属性 | Bullet 方法/属性 | 说明 |
|---------|-----------------|------|
| `localAxis` | `btSliderConstraint::getFrameOffsetA()` | 滑动轴 |
| `hasLimits` | `btSliderConstraint::setLowerLinLimit()` / `setUpperLinLimit()` | 距离限制 |
| `minDistance` / `maxDistance` | `btSliderConstraint::setLowerLinLimit()` / `setUpperLinLimit()` | 滑动范围 |

#### ForceFieldComponent → Bullet 实现

`ForceFieldComponent` 在 Bullet 中没有直接对应，需要通过自定义回调实现：

| 现有属性 | Bullet 实现方式 | 说明 |
|---------|----------------|------|
| `type` | 自定义 `btActionInterface` 或每帧遍历应用力 | 力场类型 |
| `direction` | `btRigidBody::applyForce()` | 力方向 |
| `strength` | `btRigidBody::applyForce()` | 力强度 |
| `radius` | 距离检测后应用力 | 影响范围 |
| `affectOnlyInside` | 距离检测逻辑 | 范围控制 |
| `linearFalloff` | 自定义衰减计算 | 衰减模式 |

**实现方式**：

```cpp
// 在 BulletWorldAdapter 中实现力场系统
class BulletForceFieldSystem {
    void ApplyForceFields(btDiscreteDynamicsWorld* world, const std::vector<ForceFieldComponent>& fields) {
        for (auto& field : fields) {
            if (!field.enabled) continue;
            
            // 遍历所有刚体
            for (int i = 0; i < world->getNumCollisionObjects(); ++i) {
                btRigidBody* body = btRigidBody::upcast(world->getCollisionObjectArray()[i]);
                if (!body || body->isStaticObject()) continue;
                
                // 计算力场对刚体的影响
                Vector3 force = CalculateForceField(field, body);
                body->applyForce(ToBullet(force), btVector3(0, 0, 0));
            }
        }
    }
};
```

---

## 数据转换层

### 概述

数据转换层负责在 Eigen 类型（项目使用）和 Bullet 类型之间进行转换。Bullet 使用自己的数学库（`btVector3`, `btQuaternion`, `btMatrix3x3`），而项目使用 Eigen。

### 转换函数实现

#### 1. Vector3 转换

```cpp
// Eigen::Vector3f -> btVector3
inline btVector3 ToBullet(const Vector3& v) {
    return btVector3(v.x(), v.y(), v.z());
}

// btVector3 -> Eigen::Vector3f
inline Vector3 FromBullet(const btVector3& v) {
    return Vector3(v.x(), v.y(), v.z());
}
```

#### 2. Quaternion 转换

```cpp
// Eigen::Quaternionf -> btQuaternion
// 注意：Eigen 四元数顺序为 (w, x, y, z)，Bullet 为 (x, y, z, w)
inline btQuaternion ToBullet(const Quaternion& q) {
    return btQuaternion(q.x(), q.y(), q.z(), q.w());
}

// btQuaternion -> Eigen::Quaternionf
inline Quaternion FromBullet(const btQuaternion& q) {
    return Quaternion(q.w(), q.x(), q.y(), q.z());  // Eigen: (w, x, y, z)
}
```

#### 3. Matrix3 转换

```cpp
// Eigen::Matrix3f -> btMatrix3x3
inline btMatrix3x3 ToBullet(const Matrix3& m) {
    return btMatrix3x3(
        m(0, 0), m(0, 1), m(0, 2),
        m(1, 0), m(1, 1), m(1, 2),
        m(2, 0), m(2, 1), m(2, 2)
    );
}

// btMatrix3x3 -> Eigen::Matrix3f
inline Matrix3 FromBullet(const btMatrix3x3& m) {
    Matrix3 result;
    result << m[0][0], m[0][1], m[0][2],
              m[1][0], m[1][1], m[1][2],
              m[2][0], m[2][1], m[2][2];
    return result;
}
```

#### 4. Transform 转换

```cpp
// Eigen Transform -> btTransform
inline btTransform ToBullet(const Vector3& pos, const Quaternion& rot) {
    btTransform transform;
    transform.setOrigin(ToBullet(pos));
    transform.setRotation(ToBullet(rot));
    return transform;
}

// btTransform -> Eigen Transform
inline void FromBullet(const btTransform& transform, Vector3& pos, Quaternion& rot) {
    pos = FromBullet(transform.getOrigin());
    rot = FromBullet(transform.getRotation());
}
```

### 转换层头文件结构

```cpp
// include/render/physics/bullet_adapter/eigen_to_bullet.h
#pragma once

#include "render/types.h"
#include <LinearMath/btVector3.h>
#include <LinearMath/btQuaternion.h>
#include <LinearMath/btMatrix3x3.h>
#include <LinearMath/btTransform.h>

namespace Render::Physics::BulletAdapter {

// Vector3 转换
btVector3 ToBullet(const Vector3& v);
Vector3 FromBullet(const btVector3& v);

// Quaternion 转换
btQuaternion ToBullet(const Quaternion& q);
Quaternion FromBullet(const btQuaternion& q);

// Matrix3 转换
btMatrix3x3 ToBullet(const Matrix3& m);
Matrix3 FromBullet(const btMatrix3x3& m);

// Transform 转换
btTransform ToBullet(const Vector3& pos, const Quaternion& rot);
void FromBullet(const btTransform& transform, Vector3& pos, Quaternion& rot);

} // namespace Render::Physics::BulletAdapter
```

---

## 实现步骤

### 阶段一：基础适配层（1-2 周）

#### 1.1 创建适配层目录结构

```
src/physics/bullet_adapter/
├── eigen_to_bullet.cpp
├── eigen_to_bullet.h
├── bullet_world_adapter.h
├── bullet_world_adapter.cpp
├── bullet_rigid_body_adapter.h
├── bullet_rigid_body_adapter.cpp
├── bullet_shape_adapter.h
└── bullet_shape_adapter.cpp
```

#### 1.2 实现数据转换层

参考上面的数据转换层实现。

#### 1.3 实现形状适配器

创建 `BulletShapeAdapter`，负责将 `ColliderComponent` 转换为 `btCollisionShape`。

#### 1.4 实现刚体适配器

创建 `BulletRigidBodyAdapter`，负责在 `RigidBodyComponent` 和 `btRigidBody` 之间同步数据。

### 阶段二：世界适配器（2-3 周）

#### 2.1 实现 BulletWorldAdapter

封装 `btDiscreteDynamicsWorld`，提供与 `PhysicsWorld` 相同的接口。

#### 2.2 实现实体管理

维护 ECS 实体到 Bullet 刚体的映射关系。

### 阶段三：集成到 PhysicsWorld（1-2 周）

#### 3.1 修改 PhysicsWorld 以支持 Bullet 后端

在 `PhysicsWorld` 中添加条件编译，支持选择使用自定义引擎或 Bullet：

```cpp
// include/render/physics/physics_world.h
class PhysicsWorld {
private:
#ifdef USE_BULLET_PHYSICS
    std::unique_ptr<BulletAdapter::BulletWorldAdapter> m_bulletAdapter;
#else
    // 原有实现
    std::unique_ptr<PhysicsTransformSync> m_transformSync;
#endif
    
public:
    void Step(float deltaTime);
    // ... 其他接口保持不变
};
```

#### 3.2 实现 PhysicsWorld::Step() 的 Bullet 版本

```cpp
void PhysicsWorld::Step(float deltaTime) {
#ifdef USE_BULLET_PHYSICS
    // 1. 同步 ECS 组件到 Bullet
    SyncComponentsToBullet();
    
    // 2. Bullet 物理步进
    m_bulletAdapter->Step(deltaTime);
    
    // 3. 同步 Bullet 结果到 ECS 组件
    SyncBulletToComponents();
    
    // 4. 插值变换
    InterpolateTransforms(GetInterpolationAlpha());
#else
    // 原有实现
    // ...
#endif
}
```

### 阶段四：测试和优化（2-3 周）

#### 4.1 单元测试

为每个适配器编写单元测试，确保数据转换正确。

#### 4.2 集成测试

运行现有物理测试，确保行为一致。

#### 4.3 性能测试

对比自定义引擎和 Bullet 的性能，优化热点路径。

---

## CMake 集成

### 方案一：使用项目内的 Bullet（推荐）

项目已经在 `third_party/bullet3` 目录下有 Bullet 源码，可以直接作为子项目编译。

#### 1. 在 CMakeLists.txt 中添加 Bullet

```cmake
# 物理引擎选项
option(ENABLE_PHYSICS "Enable physics engine" ON)
option(USE_BULLET_PHYSICS "Use Bullet Physics instead of custom engine" OFF)

if(ENABLE_PHYSICS AND USE_BULLET_PHYSICS)
    # 添加 Bullet 作为子项目
    set(BULLET_ROOT ${CMAKE_SOURCE_DIR}/third_party/bullet3)
    
    if(EXISTS ${BULLET_ROOT}/CMakeLists.txt)
        # 配置 Bullet 构建选项
        set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "Build Bullet demos" FORCE)
        set(BUILD_BULLET3 OFF CACHE BOOL "Build Bullet3" FORCE)
        set(BUILD_EXTRAS OFF CACHE BOOL "Build Bullet extras" FORCE)
        set(BUILD_UNIT_TESTS OFF CACHE BOOL "Build Bullet unit tests" FORCE)
        set(BUILD_CPU_DEMOS OFF CACHE BOOL "Build Bullet CPU demos" FORCE)
        set(USE_GRAPHICAL_BENCHMARK OFF CACHE BOOL "Use graphical benchmark" FORCE)
        
        # 添加 Bullet 子目录
        add_subdirectory(${BULLET_ROOT} ${CMAKE_BINARY_DIR}/bullet3 EXCLUDE_FROM_ALL)
        
        # 设置 Bullet 库
        set(BULLET_LIBRARIES
            BulletDynamics
            BulletCollision
            LinearMath
        )
        
        # 设置包含目录
        set(BULLET_INCLUDE_DIRS
            ${BULLET_ROOT}/src
        )
        
        message(STATUS "Bullet Physics enabled (using subproject)")
    else()
        message(FATAL_ERROR "Bullet Physics source not found at ${BULLET_ROOT}")
    endif()
endif()
```

#### 2. 在物理模块的 CMakeLists.txt 中链接

```cmake
# src/physics/CMakeLists.txt
if(USE_BULLET_PHYSICS)
    target_compile_definitions(Physics PRIVATE USE_BULLET_PHYSICS)
    target_link_libraries(Physics PRIVATE ${BULLET_LIBRARIES})
    target_include_directories(Physics PRIVATE ${BULLET_INCLUDE_DIRS})
endif()
```

### 方案二：使用系统安装的 Bullet

如果系统已安装 Bullet，可以使用 `find_package`：

```cmake
if(USE_BULLET_PHYSICS)
    find_package(Bullet REQUIRED)
    if(Bullet_FOUND)
        target_link_libraries(Physics PRIVATE ${BULLET_LIBRARIES})
        target_include_directories(Physics PRIVATE ${BULLET_INCLUDE_DIRS})
        message(STATUS "Bullet Physics found: ${BULLET_LIBRARIES}")
    else()
        message(FATAL_ERROR "Bullet Physics not found. Please install it or use subproject.")
    endif()
endif()
```

### 编译选项

在配置 CMake 时启用 Bullet：

```bash
cmake -DUSE_BULLET_PHYSICS=ON ..
```

---

## 测试策略

### 单元测试

#### 1. 数据转换测试

```cpp
TEST(BulletAdapter, Vector3Conversion) {
    Vector3 eigenVec(1.0f, 2.0f, 3.0f);
    btVector3 bulletVec = BulletAdapter::ToBullet(eigenVec);
    Vector3 back = BulletAdapter::FromBullet(bulletVec);
    
    EXPECT_FLOAT_EQ(eigenVec.x(), back.x());
    EXPECT_FLOAT_EQ(eigenVec.y(), back.y());
    EXPECT_FLOAT_EQ(eigenVec.z(), back.z());
}
```

#### 2. 形状创建测试

```cpp
TEST(BulletAdapter, ShapeCreation) {
    auto collider = ColliderComponent::CreateSphere(1.0f);
    btCollisionShape* shape = BulletShapeAdapter::CreateShape(collider);
    
    ASSERT_NE(shape, nullptr);
    EXPECT_EQ(shape->getShapeType(), SPHERE_SHAPE_PROXYTYPE);
    
    auto* sphere = static_cast<btSphereShape*>(shape);
    EXPECT_FLOAT_EQ(sphere->getRadius(), 1.0f);
}
```

### 集成测试

#### 1. 物理行为一致性测试

运行现有的物理测试套件，确保行为一致：

```cpp
TEST(PhysicsWorld, BulletBackendConsistency) {
    // 使用自定义引擎运行测试
    PhysicsWorld customWorld(ecsWorld.get(), config);
    // ... 运行测试场景
    
    // 使用 Bullet 后端运行相同测试
    #ifdef USE_BULLET_PHYSICS
    PhysicsWorld bulletWorld(ecsWorld.get(), config);
    // ... 运行相同测试场景
    
    // 比较结果（允许小的数值误差）
    EXPECT_NEAR(customResult, bulletResult, 0.01f);
    #endif
}
```

#### 2. 性能基准测试

对比两个后端的性能：

```cpp
BENCHMARK(PhysicsWorld, CustomEngine) {
    PhysicsWorld world(ecsWorld.get(), config);
    for (int i = 0; i < 1000; ++i) {
        world.Step(0.016f);
    }
}

BENCHMARK(PhysicsWorld, BulletBackend) {
    #ifdef USE_BULLET_PHYSICS
    PhysicsWorld world(ecsWorld.get(), config);
    for (int i = 0; i < 1000; ++i) {
        world.Step(0.016f);
    }
    #endif
}
```

### 回归测试

运行所有现有测试，确保没有破坏性变更：

```bash
# 使用自定义引擎
cmake -DUSE_BULLET_PHYSICS=OFF ..
make
ctest

# 使用 Bullet 后端
cmake -DUSE_BULLET_PHYSICS=ON ..
make
ctest
```

---

## 迁移检查清单

### 阶段一：基础适配层

- [ ] **数据转换层**
  - [ ] 实现 `EigenToBullet` 转换函数（Vector3, Quaternion, Matrix3, Transform）
  - [ ] 编写单元测试验证转换正确性
  - [ ] 处理四元数顺序差异（Eigen: w,x,y,z vs Bullet: x,y,z,w）

- [ ] **形状适配器**
  - [ ] 实现 `BulletShapeAdapter::CreateShape()` 支持所有形状类型
  - [ ] 实现形状更新逻辑（动态修改形状参数）
  - [ ] 处理形状的局部变换（center, rotation）
  - [ ] 实现形状内存管理（共享形状优化）

- [ ] **刚体适配器**
  - [ ] 实现 `BulletRigidBodyAdapter::SyncToBullet()`
  - [ ] 实现 `BulletRigidBodyAdapter::SyncFromBullet()`
  - [ ] 处理刚体类型转换（Static/Kinematic/Dynamic）
  - [ ] 处理质量、惯性张量同步
  - [ ] 处理速度约束（lockPosition, lockRotation）

### 阶段二：世界适配器

- [ ] **BulletWorldAdapter 核心功能**
  - [ ] 初始化 Bullet 世界（broadphase, dispatcher, solver, config）
  - [ ] 实现 `Step()` 方法
  - [ ] 实现实体到刚体的映射管理
  - [ ] 实现刚体的添加/移除

- [ ] **碰撞检测集成**
  - [ ] 实现碰撞层和掩码过滤
  - [ ] 实现触发器检测
  - [ ] 实现碰撞事件回调
  - [ ] 同步碰撞结果到 ECS 组件

- [ ] **物理材质处理**
  - [ ] 实现摩擦系数同步
  - [ ] 实现弹性系数同步
  - [ ] 实现材质组合模式（Average/Minimum/Maximum/Multiply）

### 阶段三：PhysicsWorld 集成

- [ ] **条件编译支持**
  - [ ] 在 `PhysicsWorld` 中添加 `USE_BULLET_PHYSICS` 条件编译
  - [ ] 保持原有实现可用（向后兼容）
  - [ ] 实现统一的接口层

- [ ] **同步机制**
  - [ ] 实现 ECS → Bullet 同步（每帧开始）
  - [ ] 实现 Bullet → ECS 同步（每帧结束）
  - [ ] 实现 Kinematic 物体驱动
  - [ ] 实现插值变换（平滑渲染）

- [ ] **力场系统**
  - [ ] 实现力场应用逻辑
  - [ ] 实现距离检测和衰减计算
  - [ ] 支持所有力场类型（Gravity, Wind, Radial, Vortex）

### 阶段四：关节约束

- [ ] **关节适配器**
  - [ ] 实现 `BulletJointAdapter` 基类
  - [ ] 实现固定关节（`btFixedConstraint`）
  - [ ] 实现铰链关节（`btHingeConstraint`）
  - [ ] 实现距离关节（`btPoint2PointConstraint` 或 `btGeneric6DofSpringConstraint`）
  - [ ] 实现弹簧关节（`btGeneric6DofSpringConstraint`）
  - [ ] 实现滑动关节（`btSliderConstraint`）

- [ ] **关节特性**
  - [ ] 实现角度/距离限制
  - [ ] 实现关节马达
  - [ ] 实现关节断裂检测
  - [ ] 实现关节启用/禁用

### 阶段五：测试和优化

- [ ] **单元测试**
  - [ ] 数据转换测试
  - [ ] 形状创建测试
  - [ ] 刚体同步测试
  - [ ] 关节功能测试

- [ ] **集成测试**
  - [ ] 物理行为一致性测试
  - [ ] 性能基准测试
  - [ ] 回归测试（所有现有测试通过）

- [ ] **性能优化**
  - [ ] 形状共享优化
  - [ ] 同步操作批处理
  - [ ] 内存池管理
  - [ ] 多线程优化（如果适用）

---

## 常见问题

### 1. 四元数顺序问题

**问题**：Eigen 使用 (w, x, y, z) 顺序，Bullet 使用 (x, y, z, w) 顺序。

**解决方案**：在转换函数中正确处理顺序：

```cpp
// Eigen → Bullet
btQuaternion ToBullet(const Quaternion& q) {
    return btQuaternion(q.x(), q.y(), q.z(), q.w());
}

// Bullet → Eigen
Quaternion FromBullet(const btQuaternion& q) {
    return Quaternion(q.w(), q.x(), q.y(), q.z());  // Eigen: (w, x, y, z)
}
```

### 2. 形状共享优化

**问题**：相同参数的形状应该共享，避免重复创建。

**解决方案**：使用形状缓存：

```cpp
class BulletShapeAdapter {
private:
    static std::unordered_map<ShapeKey, btCollisionShape*, ShapeKeyHash> s_shapeCache;
    
public:
    static btCollisionShape* CreateShape(const ColliderComponent& collider) {
        ShapeKey key = CreateShapeKey(collider);
        if (s_shapeCache.find(key) != s_shapeCache.end()) {
            return s_shapeCache[key];
        }
        
        btCollisionShape* shape = CreateNewShape(collider);
        s_shapeCache[key] = shape;
        return shape;
    }
};
```

### 3. 刚体类型转换

**问题**：Bullet 的刚体类型通过标志位设置，需要正确映射。

**解决方案**：

```cpp
void BulletRigidBodyAdapter::SyncToBullet(const RigidBodyComponent& component) {
    int flags = m_bulletBody->getCollisionFlags();
    
    switch (component.type) {
        case BodyType::Static:
            flags |= CF_STATIC_OBJECT;
            m_bulletBody->setMassProps(0.0f, btVector3(0, 0, 0));
            break;
        case BodyType::Kinematic:
            flags |= CF_KINEMATIC_OBJECT;
            m_bulletBody->setMassProps(0.0f, btVector3(0, 0, 0));
            break;
        case BodyType::Dynamic:
            flags &= ~(CF_STATIC_OBJECT | CF_KINEMATIC_OBJECT);
            // 设置正常质量
            break;
    }
    
    m_bulletBody->setCollisionFlags(flags);
}
```

### 4. 重力缩放实现

**问题**：Bullet 没有直接的 `gravityScale` 属性。

**解决方案**：在应用重力时手动乘以缩放因子：

```cpp
void BulletRigidBodyAdapter::ApplyGravity(const Vector3& worldGravity) {
    if (m_component.useGravity) {
        Vector3 scaledGravity = worldGravity * m_component.gravityScale;
        m_bulletBody->setGravity(ToBullet(scaledGravity));
    } else {
        m_bulletBody->setGravity(btVector3(0, 0, 0));
    }
}
```

### 5. 位置/旋转锁定

**问题**：Bullet 使用 `setLinearFactor()` 和 `setAngularFactor()` 实现锁定。

**解决方案**：

```cpp
void BulletRigidBodyAdapter::SyncConstraints() {
    btVector3 linearFactor(1, 1, 1);
    btVector3 angularFactor(1, 1, 1);
    
    if (m_component.lockPosition[0]) linearFactor.setX(0);
    if (m_component.lockPosition[1]) linearFactor.setY(0);
    if (m_component.lockPosition[2]) linearFactor.setZ(0);
    
    if (m_component.lockRotation[0]) angularFactor.setX(0);
    if (m_component.lockRotation[1]) angularFactor.setY(0);
    if (m_component.lockRotation[2]) angularFactor.setZ(0);
    
    m_bulletBody->setLinearFactor(linearFactor);
    m_bulletBody->setAngularFactor(angularFactor);
}
```

### 6. 材质组合模式

**问题**：Bullet 默认使用平均值，需要自定义组合逻辑。

**解决方案**：实现自定义接触回调：

```cpp
class CustomContactCallback : public btManifoldResult {
    void addContactPoint(const btVector3& normalOnBInWorld,
                        const btVector3& pointInWorld,
                        btScalar depth) override {
        // 获取两个物体的材质
        PhysicsMaterial* matA = GetMaterial(bodyA);
        PhysicsMaterial* matB = GetMaterial(bodyB);
        
        // 计算组合后的摩擦和弹性
        float friction = PhysicsMaterial::CombineValues(
            matA->friction, matB->friction, matA->frictionCombine);
        float restitution = PhysicsMaterial::CombineValues(
            matA->restitution, matB->restitution, matA->restitutionCombine);
        
        // 设置到接触点
        m_manifoldPoint.m_combinedFriction = friction;
        m_manifoldPoint.m_combinedRestitution = restitution;
        
        btManifoldResult::addContactPoint(normalOnBInWorld, pointInWorld, depth);
    }
};
```

### 7. 触发器实现

**问题**：触发器不产生物理响应，只触发事件。

**解决方案**：

```cpp
void BulletShapeAdapter::SetupTrigger(btCollisionObject* obj, bool isTrigger) {
    if (isTrigger) {
        obj->setCollisionFlags(obj->getCollisionFlags() | CF_NO_CONTACT_RESPONSE);
    } else {
        obj->setCollisionFlags(obj->getCollisionFlags() & ~CF_NO_CONTACT_RESPONSE);
    }
}
```

### 8. 连续碰撞检测（CCD）

**问题**：高速物体可能穿透，需要启用 CCD。

**解决方案**：

```cpp
void BulletRigidBodyAdapter::SetupCCD(const RigidBodyComponent& component) {
    if (component.useCCD || 
        component.linearVelocity.norm() > component.ccdVelocityThreshold) {
        // 设置 CCD 运动阈值（相对于形状尺寸）
        float ccdThreshold = CalculateCCDThreshold(component);
        m_bulletBody->setCcdMotionThreshold(ccdThreshold);
        m_bulletBody->setCcdSweptSphereRadius(0.1f);  // 扫描球半径
    }
}
```

### 9. 休眠系统

**问题**：Bullet 的休眠机制与自定义实现可能不同。

**解决方案**：同步休眠状态：

```cpp
void BulletRigidBodyAdapter::SyncSleepState(RigidBodyComponent& component) {
    int activationState = m_bulletBody->getActivationState();
    component.isSleeping = (activationState == ISLAND_SLEEPING);
    
    if (component.isSleeping) {
        // 同步速度（休眠物体速度为 0）
        component.linearVelocity = Vector3::Zero();
        component.angularVelocity = Vector3::Zero();
    }
}
```

### 10. 性能优化建议

1. **形状共享**：相同参数的形状只创建一次，多个刚体共享
2. **批处理同步**：避免每帧逐个同步，使用批处理操作
3. **减少不必要的同步**：只在数据变化时同步
4. **使用 Bullet 的优化特性**：如 `btDbvtBroadphase`、多线程求解器等
5. **内存池**：重用 Bullet 对象，避免频繁分配/释放

---

## 实现细节补充

### 同步时机

物理更新流程：

```
1. 渲染 → 物理同步（Kinematic/Static 物体）
   - 从 TransformComponent 读取位置/旋转
   - 同步到 btRigidBody

2. 应用力场
   - 遍历所有 ForceFieldComponent
   - 计算并应用力到相关刚体

3. Bullet 物理步进
   - btDiscreteDynamicsWorld::stepSimulation(deltaTime)

4. 物理 → 渲染同步（Dynamic 物体）
   - 从 btRigidBody 读取位置/旋转
   - 同步到 TransformComponent

5. 插值变换
   - 计算插值因子 alpha
   - 插值上一帧和当前帧的变换
```

### 碰撞事件处理

Bullet 通过回调机制处理碰撞事件：

```cpp
class BulletContactCallback : public btCollisionWorld::ContactResultCallback {
    bool needsCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const override {
        // 检查碰撞层和掩码
        return CheckCollisionLayers(proxy0, proxy1);
    }
    
    btScalar addSingleResult(btManifoldPoint& cp,
                             const btCollisionObjectWrapper* colObj0Wrap,
                             int partId0, int index0,
                             const btCollisionObjectWrapper* colObj1Wrap,
                             int partId1, int index1) override {
        // 处理碰撞事件
        ProcessCollisionEvent(cp, colObj0Wrap, colObj1Wrap);
        return 0;
    }
};
```

### 关节断裂检测

在每帧物理更新后检查关节是否断裂：

```cpp
void BulletJointAdapter::CheckBreakage() {
    if (m_joint->isBroken()) {
        return;  // 已经断裂
    }
    
    // 计算关节上的力和扭矩
    btVector3 force = CalculateJointForce();
    btVector3 torque = CalculateJointTorque();
    
    if (force.length() > m_component.breakForce ||
        torque.length() > m_component.breakTorque) {
        // 标记为断裂
        m_component.isBroken = true;
        m_world->removeConstraint(m_joint);
    }
}
```

---

## 参考资料

- [Bullet Physics 官方文档](https://pybullet.org/Bullet/BulletFull/)
- [Bullet Physics GitHub](https://github.com/bulletphysics/bullet3)
- [Bullet Physics 用户手册](https://github.com/bulletphysics/bullet3/blob/master/docs/Bullet_User_Manual.pdf)
- [Bullet Physics API 参考](https://pybullet.org/Bullet/BulletFull/annotated.html)

---

**文档版本**: v1.1  
**最后更新**: 2025-12-10  
**维护者**: Linductor
```
