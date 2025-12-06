# 物理引擎 API 参考手册

> **版本**: v1.7.0  
> **开发阶段**: 阶段 1-3 已完成（基础架构 + 碰撞检测 + 刚体动力学）  
> **最后更新**: 2025-12-06

## 目录

- [概述](#概述)
- [快速开始](#快速开始)
- [核心组件](#核心组件)
  - [RigidBodyComponent](#rigidbodycomponent)
  - [ColliderComponent](#collidercomponent)
  - [PhysicsMaterial](#physicsmaterial)
  - [ForceFieldComponent](#forcefieldcomponent)
- [物理世界](#物理世界)
  - [PhysicsWorld](#physicsworld)
  - [PhysicsConfig](#physicsconfig)
- [动力学系统](#动力学系统)
  - [ForceAccumulator](#forceaccumulator)
  - [SymplecticEulerIntegrator](#symplecticeulerintegrator)
  - [PhysicsUpdateSystem](#physicsupdatesystem)
- [碰撞检测系统](#碰撞检测系统)
  - [CollisionDetectionSystem](#collisiondetectionsystem)
  - [BroadPhase](#broadphase)
  - [CollisionDetector](#collisiondetector)
  - [CollisionDispatcher](#collisiondispatcher)
- [碰撞形状](#碰撞形状)
  - [CollisionShape](#collisionshape)
  - [SphereShape](#sphereshape)
  - [BoxShape](#boxshape)
  - [CapsuleShape](#capsuleshape)
- [接触流形](#接触流形)
  - [ContactManifold](#contactmanifold)
  - [ContactPoint](#contactpoint)
- [物理工具](#物理工具)
  - [PhysicsUtils](#physicsutils)
- [物理事件](#物理事件)
  - [碰撞事件](#碰撞事件)
  - [触发器事件](#触发器事件)
- [使用示例](#使用示例)

---

## 概述

RenderEngine 物理引擎是一个基于 ECS 架构的 3D 物理模拟系统。当前版本已完成基础架构、碰撞检测与刚体动力学，支持：

- ✅ 基础物理组件（刚体、碰撞体、材质）
- ✅ 多种碰撞形状（球体、盒体、胶囊体、网格）
- ✅ 粗检测系统（空间哈希、八叉树）
- ✅ 细检测系统（SAT、GJK/EPA）
- ✅ 碰撞事件系统
- ✅ 触发器系统
- ✅ 碰撞层和掩码
- ✅ 刚体动力学：力/扭矩/冲量、全局重力与力场、半隐式欧拉积分
- ✅ 休眠与渲染插值（平滑视觉，避免螺旋死亡）

**注意**: 约束求解、接触解算等功能将在后续阶段实现。

---

## 快速开始

### 1. 包含头文件

```cpp
#include "render/physics/physics_components.h"
#include "render/physics/physics_systems.h"
#include "render/physics/physics_world.h"
#include "render/physics/physics_config.h"
```

### 2. 创建物理世界

```cpp
using namespace Render;
using namespace Render::Physics;

// 创建 ECS 世界
auto world = std::make_shared<ECS::World>();
world->Initialize();

// 创建物理世界
PhysicsConfig config = PhysicsConfig::Default();
config.gravity = Vector3(0, -9.81f, 0);
config.broadPhaseType = BroadPhaseType::SpatialHash;
config.spatialHashCellSize = 5.0f;

PhysicsWorld physicsWorld(world.get(), config);
```

### 3. 创建物理实体

```cpp
// 创建实体
auto entity = world->CreateEntity();

// 添加变换组件
auto& transform = world->AddComponent<TransformComponent>(entity);
transform.position = Vector3(0, 10, 0);

// 添加碰撞体组件
auto& collider = world->AddComponent<ColliderComponent>(entity);
collider = ColliderComponent::CreateSphere(1.0f);
collider.material = std::make_shared<PhysicsMaterial>(PhysicsMaterial::Rubber());

// 添加刚体组件
auto& rigidBody = world->AddComponent<RigidBodyComponent>(entity);
rigidBody.type = RigidBodyComponent::BodyType::Dynamic;
rigidBody.mass = 1.0f;

// 自动计算质量和惯性张量
PhysicsUtils::InitializeRigidBody(rigidBody, collider);
```

### 4. 注册碰撞检测系统

```cpp
// 注册碰撞检测系统
auto* collisionSystem = world->RegisterSystem<CollisionDetectionSystem>();

// 设置粗检测算法
auto broadPhase = std::make_unique<SpatialHashBroadPhase>(5.0f);
collisionSystem->SetBroadPhase(std::move(broadPhase));

// 设置事件总线（用于接收碰撞事件）
Application::EventBus* eventBus = /* ... */;
collisionSystem->SetEventBus(eventBus);
```

### 5. 注册物理更新系统

```cpp
// 注册刚体动力学更新系统（在碰撞检测之后执行）
auto* physicsSystem = world->RegisterSystem<PhysicsUpdateSystem>();

// 配置重力与固定时间步
physicsSystem->SetGravity(config.gravity);
physicsSystem->SetFixedDeltaTime(config.fixedDeltaTime);
```

### 6. 更新物理系统

```cpp
// 在主循环中
float deltaTime = 0.016f;  // 60 FPS

// ECS 会按系统优先级顺序执行：碰撞检测 -> 刚体更新
world->Update(deltaTime);

// 获取碰撞对
const auto& collisionPairs = collisionSystem->GetCollisionPairs();
for (const auto& pair : collisionPairs) {
    std::cout << "碰撞: " << pair.entityA.index << " vs " 
              << pair.entityB.index << std::endl;
}
```

---

## 核心组件

### RigidBodyComponent

刚体组件，为实体添加物理动力学行为。

#### 头文件
```cpp
#include "render/physics/physics_components.h"
```

#### 成员变量

##### 基本属性

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `BodyType` | `type` | 刚体类型（Static/Kinematic/Dynamic） | `Dynamic` |
| `float` | `mass` | 质量 (kg) | `1.0f` |
| `float` | `inverseMass` | 逆质量（自动计算） | `1.0f` |
| `Vector3` | `centerOfMass` | 质心（局部空间） | `Vector3::Zero()` |
| `Matrix3` | `inertiaTensor` | 惯性张量（局部空间） | `Matrix3::Identity()` |
| `Matrix3` | `inverseInertiaTensor` | 逆惯性张量 | `Matrix3::Identity()` |

##### 运动状态

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `Vector3` | `linearVelocity` | 线速度 (m/s) | `Vector3::Zero()` |
| `Vector3` | `angularVelocity` | 角速度 (rad/s) | `Vector3::Zero()` |
| `Vector3` | `force` | 累积的力 (N) | `Vector3::Zero()` |
| `Vector3` | `torque` | 累积的扭矩 (N·m) | `Vector3::Zero()` |

##### 阻尼

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `float` | `linearDamping` | 线性阻尼 [0, 1] | `0.01f` |
| `float` | `angularDamping` | 角阻尼 [0, 1] | `0.05f` |

##### 约束

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `bool[3]` | `lockPosition` | 位置锁定（X, Y, Z 轴） | `{false, false, false}` |
| `bool[3]` | `lockRotation` | 旋转锁定（X, Y, Z 轴） | `{false, false, false}` |

##### 重力

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `bool` | `useGravity` | 是否受重力影响 | `true` |
| `float` | `gravityScale` | 重力缩放因子 | `1.0f` |

##### 休眠

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `bool` | `isSleeping` | 是否处于休眠状态 | `false` |
| `float` | `sleepThreshold` | 休眠阈值（动能） | `0.01f` |
| `float` | `sleepTimer` | 休眠计时器 | `0.0f` |

#### 枚举类型

```cpp
enum class BodyType {
    Static,    // 静态物体：不移动，不受力影响（如地面、墙壁）
    Kinematic, // 运动学物体：可通过脚本移动，不受力影响（如移动平台）
    Dynamic    // 动态物体：受力影响，完整物理模拟（如球、箱子）
};
```

#### 成员函数

##### `void SetMass(float m)`
设置质量并自动计算逆质量。

**参数**:
- `m`: 质量 (kg)

**示例**:
```cpp
rigidBody.SetMass(10.0f);
```

##### `void WakeUp()`
唤醒刚体（从休眠状态）。

**示例**:
```cpp
rigidBody.WakeUp();
```

##### `bool IsStatic() const`
是否为静态物体。

**返回值**: `true` 如果是静态物体。

##### `bool IsKinematic() const`
是否为运动学物体。

**返回值**: `true` 如果是运动学物体。

##### `bool IsDynamic() const`
是否为动态物体。

**返回值**: `true` 如果是动态物体。

#### 使用示例

```cpp
// 创建动态刚体
auto& rigidBody = world->AddComponent<RigidBodyComponent>(entity);
rigidBody.type = RigidBodyComponent::BodyType::Dynamic;
rigidBody.SetMass(5.0f);
rigidBody.useGravity = true;
rigidBody.linearDamping = 0.1f;

// 创建静态刚体（地面）
auto& groundBody = world->AddComponent<RigidBodyComponent>(groundEntity);
groundBody.type = RigidBodyComponent::BodyType::Static;

// 创建运动学刚体（移动平台）
auto& platformBody = world->AddComponent<RigidBodyComponent>(platformEntity);
platformBody.type = RigidBodyComponent::BodyType::Kinematic;
```

---

### ColliderComponent

碰撞体组件，定义物体的碰撞形状。

#### 头文件
```cpp
#include "render/physics/physics_components.h"
```

#### 成员变量

##### 形状

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `ShapeType` | `shapeType` | 形状类型 | `ShapeType::Box` |
| `ShapeData` | `shapeData` | 形状数据（union） | - |
| `std::shared_ptr<Mesh>` | `meshData` | 网格数据（Mesh/ConvexHull） | `nullptr` |
| `bool` | `useConvexHull` | 是否使用凸包 | `false` |

##### 局部变换

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `Vector3` | `center` | 碰撞体中心偏移 | `Vector3::Zero()` |
| `Quaternion` | `rotation` | 碰撞体旋转偏移 | `Quaternion::Identity()` |

##### 碰撞属性

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `bool` | `isTrigger` | 是否为触发器 | `false` |
| `int` | `collisionLayer` | 碰撞层（0-31） | `0` |
| `uint32_t` | `collisionMask` | 碰撞掩码 | `0xFFFFFFFF` |

##### AABB 缓存

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `AABB` | `worldAABB` | 世界空间 AABB | - |
| `bool` | `aabbDirty` | AABB 是否需要更新 | `true` |

##### 物理材质

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `std::shared_ptr<PhysicsMaterial>` | `material` | 物理材质 | `Default()` |

#### 枚举类型

```cpp
enum class ShapeType {
    Sphere,     // 球体
    Box,        // 盒体
    Capsule,    // 胶囊体
    Mesh,       // 网格（凹面）
    ConvexHull  // 凸包
};
```

#### 静态工厂方法

##### `static ColliderComponent CreateSphere(float radius)`
创建球体碰撞体。

**参数**:
- `radius`: 半径 (m)

**返回值**: `ColliderComponent` 对象

**示例**:
```cpp
auto collider = ColliderComponent::CreateSphere(1.0f);
```

##### `static ColliderComponent CreateBox(const Vector3& halfExtents)`
创建盒体碰撞体。

**参数**:
- `halfExtents`: 半尺寸 (m)

**返回值**: `ColliderComponent` 对象

**示例**:
```cpp
auto collider = ColliderComponent::CreateBox(Vector3(1.0f, 1.0f, 1.0f));
```

##### `static ColliderComponent CreateCapsule(float radius, float height)`
创建胶囊体碰撞体。

**参数**:
- `radius`: 半径 (m)
- `height`: 高度（中心线长度）(m)

**返回值**: `ColliderComponent` 对象

**示例**:
```cpp
auto collider = ColliderComponent::CreateCapsule(0.5f, 2.0f);
```

#### 成员函数

##### `Vector3 GetBoxHalfExtents() const`
获取盒体半尺寸。

**返回值**: 半尺寸向量（仅当 `shapeType == Box` 时有效）

#### 使用示例

```cpp
// 创建球体碰撞体
auto& collider = world->AddComponent<ColliderComponent>(entity);
collider = ColliderComponent::CreateSphere(1.0f);
collider.material = std::make_shared<PhysicsMaterial>(PhysicsMaterial::Rubber());
collider.collisionLayer = 1;
collider.collisionMask = 0xFFFFFFFF;  // 与所有层碰撞

// 创建盒体碰撞体（触发器）
auto& trigger = world->AddComponent<ColliderComponent>(triggerEntity);
trigger = ColliderComponent::CreateBox(Vector3(5.0f, 1.0f, 5.0f));
trigger.isTrigger = true;
trigger.collisionLayer = 2;
```

---

### PhysicsMaterial

物理材质，定义物体表面的物理属性。

#### 头文件
```cpp
#include "render/physics/physics_components.h"
```

#### 成员变量

| 类型 | 名称 | 说明 | 默认值 | 范围 |
|------|------|------|--------|------|
| `float` | `friction` | 摩擦系数 | `0.5f` | [0, 1] |
| `float` | `restitution` | 弹性系数（恢复系数） | `0.3f` | [0, 1] |
| `float` | `density` | 密度 (kg/m³) | `1.0f` | > 0 |
| `CombineMode` | `frictionCombine` | 摩擦组合模式 | `Average` | - |
| `CombineMode` | `restitutionCombine` | 弹性组合模式 | `Average` | - |

#### 枚举类型

```cpp
enum class CombineMode {
    Average,   // 取平均值
    Minimum,   // 取最小值
    Maximum,   // 取最大值
    Multiply   // 相乘
};
```

#### 静态方法

##### `static float CombineValues(float a, float b, CombineMode mode)`
组合两个材质的属性值。

**参数**:
- `a`: 第一个值
- `b`: 第二个值
- `mode`: 组合模式

**返回值**: 组合后的值

##### `static PhysicsMaterial Default()`
获取默认材质。

**返回值**: 默认材质对象

##### `static PhysicsMaterial Rubber()`
获取橡胶材质（高摩擦、高弹性）。

**返回值**: 橡胶材质对象

##### `static PhysicsMaterial Ice()`
获取冰材质（低摩擦、低弹性）。

**返回值**: 冰材质对象

##### `static PhysicsMaterial Metal()`
获取金属材质（中等摩擦、中等弹性、高密度）。

**返回值**: 金属材质对象

#### 使用示例

```cpp
// 使用默认材质
auto material = std::make_shared<PhysicsMaterial>(PhysicsMaterial::Default());

// 使用预设材质
auto rubberMaterial = std::make_shared<PhysicsMaterial>(PhysicsMaterial::Rubber());

// 自定义材质
auto customMaterial = std::make_shared<PhysicsMaterial>();
customMaterial->friction = 0.8f;
customMaterial->restitution = 0.5f;
customMaterial->density = 2.5f;
customMaterial->frictionCombine = PhysicsMaterial::CombineMode::Minimum;
customMaterial->restitutionCombine = PhysicsMaterial::CombineMode::Average;

// 应用到碰撞体
collider.material = customMaterial;
```

---

### ForceFieldComponent

力场组件，在一定范围内对刚体施加方向力、径向力或涡流力。

#### 头文件
```cpp
#include "render/physics/physics_components.h"
```

#### 核心字段

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `Type` | `type` | 力场类型：Gravity / Wind / Radial / Vortex | `Gravity` |
| `Vector3` | `direction` | 力方向或涡流轴（需归一化） | `(0,-1,0)` |
| `float` | `strength` | 强度（N/kg，正=推/排斥，负=拉/吸引） | `10.0f` |
| `float` | `radius` | 影响半径，`<=0` 表示无限范围 | `0.0f` |
| `bool` | `affectOnlyInside` | 是否仅影响半径内 | `true` |
| `bool` | `linearFalloff` | 衰减模式：线性 / 平方反比 | `true` |
| `float` | `minFalloff` | 最小衰减系数，避免中心力过大 | `0.0f` |
| `uint32_t` | `affectLayers` | 影响的碰撞层掩码 | `0xFFFFFFFF` |
| `bool` | `enabled` | 开关 | `true` |

#### 工厂方法
- `CreateGravityField(dir, strength, radius)`
- `CreateWindField(dir, strength, radius)`
- `CreateRadialField(strength, radius, useSquareFalloff)`
- `CreateVortexField(axis, strength, radius)`

#### 使用示例
```cpp
auto& field = world->AddComponent<ForceFieldComponent>(fieldEntity);
field = ForceFieldComponent::CreateRadialField(
    /* strength */ 25.0f,  /* radius */ 12.0f, /* useSquareFalloff */ true);
field.affectLayers = 1 << 0;  // 只影响第 0 层
```

---

## 物理世界

### PhysicsWorld

物理世界，管理整个物理模拟系统。

#### 头文件
```cpp
#include "render/physics/physics_world.h"
```

#### 构造函数

```cpp
PhysicsWorld(ECS::World* ecsWorld, const PhysicsConfig& config = PhysicsConfig::Default())
```

**参数**:
- `ecsWorld`: ECS 世界指针
- `config`: 物理配置（可选，默认使用 `PhysicsConfig::Default()`）

#### 成员函数

##### `void Step(float deltaTime)`
物理步进（每帧调用）。

**参数**:
- `deltaTime`: 帧时间（秒）

**注意**: 当前动力学更新由 `PhysicsUpdateSystem` 承担；`PhysicsWorld::Step` 主要作为包装入口，后续会接入约束与解算。

##### `void SetGravity(const Vector3& gravity)`
设置重力。

**参数**:
- `gravity`: 重力加速度向量 (m/s²)

**示例**:
```cpp
physicsWorld.SetGravity(Vector3(0, -9.81f, 0));
```

##### `Vector3 GetGravity() const`
获取重力。

**返回值**: 重力加速度向量

##### `const PhysicsConfig& GetConfig() const`
获取配置。

**返回值**: 物理配置的常量引用

#### 使用示例

```cpp
// 创建物理世界
auto world = std::make_shared<ECS::World>();
world->Initialize();

PhysicsConfig config = PhysicsConfig::Default();
config.gravity = Vector3(0, -9.81f, 0);
config.broadPhaseType = BroadPhaseType::SpatialHash;

PhysicsWorld physicsWorld(world.get(), config);

// 在主循环中
float deltaTime = 0.016f;
physicsWorld.Step(deltaTime);
```

---

### PhysicsConfig

物理引擎配置。

#### 头文件
```cpp
#include "render/physics/physics_config.h"
```

#### 成员变量

##### 重力设置

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `Vector3` | `gravity` | 全局重力加速度 (m/s²) | `Vector3(0, -9.81f, 0)` |

##### 时间步长

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `float` | `fixedDeltaTime` | 固定物理时间步长（秒） | `1.0f / 60.0f` |
| `int` | `maxSubSteps` | 最大子步数 | `5` |

##### 求解器设置

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `int` | `solverIterations` | 速度约束求解迭代次数 | `10` |
| `int` | `positionIterations` | 位置约束求解迭代次数 | `4` |

##### 粗检测设置

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `BroadPhaseType` | `broadPhaseType` | 粗检测算法类型 | `SpatialHash` |
| `float` | `spatialHashCellSize` | 空间哈希格子大小（米） | `5.0f` |

##### 高级特性开关

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `bool` | `enableCCD` | 是否启用连续碰撞检测 | `false` |
| `bool` | `enableSleeping` | 是否启用休眠系统 | `true` |
| `float` | `sleepThreshold` | 休眠能量阈值 | `0.01f` |
| `float` | `sleepTime` | 休眠时间（秒） | `0.5f` |

##### 性能设置

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `bool` | `enableMultithreading` | 是否启用多线程优化 | `true` |
| `int` | `workerThreadCount` | 工作线程数（0 = 自动检测） | `0` |

##### 调试设置

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `bool` | `enableDebugDraw` | 是否启用物理调试渲染 | `false` |
| `bool` | `showColliders` | 是否显示碰撞体 | `true` |
| `bool` | `showAABB` | 是否显示 AABB | `false` |
| `bool` | `showContacts` | 是否显示接触点 | `true` |
| `bool` | `showVelocity` | 是否显示速度向量 | `false` |

#### 枚举类型

```cpp
enum class BroadPhaseType {
    SpatialHash,  // 空间哈希（适合动态场景）
    Octree,       // 八叉树（适合静态场景）
    BVH           // 层次包围盒（平衡性能）
};
```

#### 静态方法

##### `static PhysicsConfig Default()`
获取默认配置。

**返回值**: 默认配置对象

##### `static PhysicsConfig HighPrecision()`
获取高精度配置（更多迭代次数，更高精度，但性能较低）。

**返回值**: 高精度配置对象

##### `static PhysicsConfig HighPerformance()`
获取高性能配置（较少迭代次数，牺牲精度换取性能）。

**返回值**: 高性能配置对象

#### 使用示例

```cpp
// 使用默认配置
PhysicsConfig config = PhysicsConfig::Default();

// 使用高精度配置
PhysicsConfig highPrecisionConfig = PhysicsConfig::HighPrecision();

// 自定义配置
PhysicsConfig customConfig;
customConfig.gravity = Vector3(0, -20.0f, 0);  // 更强的重力
customConfig.fixedDeltaTime = 1.0f / 120.0f;   // 120 Hz 物理更新
customConfig.broadPhaseType = BroadPhaseType::Octree;
customConfig.spatialHashCellSize = 10.0f;
customConfig.enableSleeping = true;
customConfig.solverIterations = 15;
```

---

## 动力学系统

### ForceAccumulator

力累加器，用于聚合作用在刚体上的力/扭矩/冲量。

- `AddForce(force)`：质心力
- `AddForceAtPoint(force, point, centerOfMass)`：产生扭矩
- `AddTorque(torque)`：直接施加扭矩
- `AddImpulse(impulse, inverseMass)` / `AddAngularImpulse(angularImpulse, invInertia)`：冲量支持
- `Clear()` / `ClearImpulses()`：重置累积量

### SymplecticEulerIntegrator

半隐式欧拉积分器，负责更新刚体速度与位置。

- `IntegrateVelocity(body, transform*, dt)`：考虑阻尼、速度上限、轴向锁定，使用世界惯性张量
- `IntegratePosition(body, transform, dt)`：更新位置与旋转，维护 `previousPosition/previousRotation` 供插值
- 适合实时场景，稳定性优于显式欧拉

### PhysicsUpdateSystem

刚体动力学系统（系统优先级 200，运行在碰撞检测之后）。

主要流程（固定时间步，最多 5 个子步）：
1) `ApplyForces`：全局重力、力场、刚体累积力/扭矩，应用冲量增量  
2) `IntegrateVelocity`、`IntegratePosition`：使用半隐式欧拉  
3) `ResolveCollisions` / `SolveConstraints`：占位，后续接入碰撞解算与约束  
4) `UpdateSleepingState`：外力/碰撞唤醒，岛屿传播，低动能休眠  
5) `UpdateAABBs`：刷新包围盒  
6) `InterpolateTransforms`：渲染插值，alpha=0 时保持上一帧，余量为 0 时直接使用最新解算结果

配置接口：
- `SetGravity(const Vector3&)` / `GetGravity()`
- `SetFixedDeltaTime(float)` / `GetFixedDeltaTime()`

施力接口：
- `ApplyForce(entity, force)`
- `ApplyForceAtPoint(entity, force, point)`
- `ApplyTorque(entity, torque)`
- `ApplyImpulse(entity, impulse)`
- `ApplyImpulseAtPoint(entity, impulse, point)`
- `ApplyAngularImpulse(entity, angularImpulse)`

#### 使用示例
```cpp
// 注册后设置重力与时间步
auto* physicsSystem = world->RegisterSystem<PhysicsUpdateSystem>();
physicsSystem->SetGravity(Vector3(0, -9.81f, 0));
physicsSystem->SetFixedDeltaTime(1.0f / 60.0f);

// 应用一次冲量（比如开局推球）
physicsSystem->ApplyImpulse(ballEntity, Vector3(0, 0, 5.0f));
```

---

## 碰撞检测系统

### CollisionDetectionSystem

碰撞检测系统，负责检测场景中所有物理物体的碰撞。

#### 头文件
```cpp
#include "render/physics/physics_systems.h"
```

#### 继承关系

```cpp
class CollisionDetectionSystem : public ECS::System
```

#### 构造函数

```cpp
CollisionDetectionSystem()
```

#### 成员函数

##### `void Update(float deltaTime) override`
更新碰撞检测系统（由 ECS 系统自动调用）。

**参数**:
- `deltaTime`: 帧时间（秒）

##### `int GetPriority() const override`
获取系统优先级（返回 `100`，在物理更新之前执行）。

##### `const std::vector<CollisionPair>& GetCollisionPairs() const`
获取当前帧的碰撞对。

**返回值**: 碰撞对列表的常量引用

##### `void SetBroadPhase(std::unique_ptr<BroadPhase> broadPhase)`
设置粗检测算法。

**参数**:
- `broadPhase`: 粗检测算法对象（移动语义）

**示例**:
```cpp
auto broadPhase = std::make_unique<SpatialHashBroadPhase>(5.0f);
collisionSystem->SetBroadPhase(std::move(broadPhase));
```

##### `void SetEventBus(Application::EventBus* eventBus)`
设置事件总线（用于发送碰撞事件）。

**参数**:
- `eventBus`: 事件总线指针

##### `const Stats& GetStats() const`
获取统计信息。

**返回值**: 统计信息结构体的常量引用

#### 统计信息结构体

```cpp
struct Stats {
    size_t totalColliders = 0;      // 总碰撞体数
    size_t broadPhasePairs = 0;      // 粗检测候选对数
    size_t narrowPhaseTests = 0;     // 细检测测试数
    size_t actualCollisions = 0;     // 实际碰撞数
    float broadPhaseTime = 0.0f;     // 粗检测耗时（秒）
    float narrowPhaseTime = 0.0f;   // 细检测耗时（秒）
};
```

#### 使用示例

```cpp
// 注册碰撞检测系统
auto* collisionSystem = world->RegisterSystem<CollisionDetectionSystem>();

// 设置粗检测算法
auto broadPhase = std::make_unique<SpatialHashBroadPhase>(5.0f);
collisionSystem->SetBroadPhase(std::move(broadPhase));

// 设置事件总线
Application::EventBus* eventBus = /* ... */;
collisionSystem->SetEventBus(eventBus);

// 在主循环中
world->Update(deltaTime);

// 获取碰撞对
const auto& pairs = collisionSystem->GetCollisionPairs();
for (const auto& pair : pairs) {
    std::cout << "碰撞: " << pair.entityA.index 
              << " vs " << pair.entityB.index << std::endl;
}

// 获取统计信息
const auto& stats = collisionSystem->GetStats();
std::cout << "碰撞体数: " << stats.totalColliders << std::endl;
std::cout << "碰撞对数: " << stats.actualCollisions << std::endl;
```

---

### BroadPhase

粗检测阶段抽象基类。

#### 头文件
```cpp
#include "render/physics/collision/broad_phase.h"
```

#### 成员函数

##### `virtual void Update(const std::vector<std::pair<ECS::EntityID, AABB>>& entities) = 0`
更新空间分区结构。

**参数**:
- `entities`: 所有需要检测的实体及其 AABB 信息

##### `virtual std::vector<EntityPair> DetectPairs() = 0`
检测可能碰撞的实体对。

**返回值**: 实体对列表

##### `virtual void Clear() = 0`
清空数据结构。

##### `virtual size_t GetCellCount() const`
获取格子数量（用于统计）。

**返回值**: 格子数量

##### `virtual size_t GetObjectCount() const`
获取物体数量（用于统计）。

**返回值**: 物体数量

---

### SpatialHashBroadPhase

空间哈希粗检测实现。

#### 头文件
```cpp
#include "render/physics/collision/broad_phase.h"
```

#### 构造函数

```cpp
explicit SpatialHashBroadPhase(float cellSize = 5.0f)
```

**参数**:
- `cellSize`: 格子大小（世界单位）

#### 特点

- **优点**: 实现简单、更新快速、适合动态场景
- **缺点**: 物体大小差异大时效率降低、格子大小需要调优

#### 使用示例

```cpp
auto broadPhase = std::make_unique<SpatialHashBroadPhase>(5.0f);
collisionSystem->SetBroadPhase(std::move(broadPhase));
```

---

### OctreeBroadPhase

八叉树粗检测实现。

#### 头文件
```cpp
#include "render/physics/collision/broad_phase.h"
```

#### 构造函数

```cpp
OctreeBroadPhase(const AABB& bounds, int maxDepth = 8, int maxObjectsPerNode = 10)
```

**参数**:
- `bounds`: 八叉树边界
- `maxDepth`: 最大深度
- `maxObjectsPerNode`: 每个节点的最大物体数

#### 特点

- **优点**: 适合静态场景、空间利用率高
- **缺点**: 动态物体更新开销较大

#### 使用示例

```cpp
AABB bounds(Vector3(-100, -100, -100), Vector3(100, 100, 100));
auto broadPhase = std::make_unique<OctreeBroadPhase>(bounds, 8, 10);
collisionSystem->SetBroadPhase(std::move(broadPhase));
```

---

### CollisionDetector

碰撞检测器，提供各种形状之间的细检测算法。

#### 头文件
```cpp
#include "render/physics/collision/collision_detection.h"
```

#### 静态方法

##### `static bool SphereVsSphere(...)`
球体 vs 球体碰撞检测。

##### `static bool SphereVsBox(...)`
球体 vs 盒体碰撞检测。

##### `static bool SphereVsCapsule(...)`
球体 vs 胶囊体碰撞检测。

##### `static bool BoxVsBox(...)`
盒体 vs 盒体碰撞检测（SAT 算法）。

##### `static bool CapsuleVsCapsule(...)`
胶囊体 vs 胶囊体碰撞检测。

##### `static bool CapsuleVsBox(...)`
胶囊体 vs 盒体碰撞检测。

#### 辅助函数

##### `static Vector3 ClosestPointOnSegment(...)`
计算点到线段的最近点。

##### `static void ClosestPointsBetweenSegments(...)`
计算两条线段之间的最近点。

##### `static Vector3 ClosestPointOnOBB(...)`
计算点到 OBB 的最近点。

---

### CollisionDispatcher

碰撞检测分发器，根据形状类型自动分发到对应的碰撞检测函数。

#### 头文件
```cpp
#include "render/physics/collision/collision_detection.h"
```

#### 静态方法

##### `static bool Detect(...)`
检测两个碰撞体是否碰撞。

**参数**:
- `shapeA`: 形状 A
- `transformA`: 变换 A（位置、旋转、缩放）
- `shapeB`: 形状 B
- `transformB`: 变换 B
- `manifold`: 输出：接触流形

**返回值**: 是否发生碰撞

**示例**:
```cpp
ContactManifold manifold;
bool colliding = CollisionDispatcher::Detect(
    shapeA, posA, rotA, scaleA,
    shapeB, posB, rotB, scaleB,
    manifold
);
```

---

## 碰撞形状

### CollisionShape

碰撞形状抽象基类。

#### 头文件
```cpp
#include "render/physics/collision/collision_shapes.h"
```

#### 纯虚函数

##### `virtual ShapeType GetType() const = 0`
获取形状类型。

##### `virtual AABB ComputeAABB(...) const = 0`
计算形状的轴对齐包围盒。

##### `virtual float ComputeVolume() const = 0`
计算形状的体积。

##### `virtual Matrix3 ComputeInertiaTensor(float mass) const = 0`
计算形状的惯性张量。

##### `virtual Vector3 GetSupportPoint(const Vector3& direction) const = 0`
获取形状的支撑点（用于 GJK 算法）。

---

### SphereShape

球体碰撞形状。

#### 头文件
```cpp
#include "render/physics/collision/collision_shapes.h"
```

#### 构造函数

```cpp
explicit SphereShape(float radius = 0.5f)
```

**参数**:
- `radius`: 半径 (m)

#### 成员函数

##### `float GetRadius() const`
获取半径。

##### `void SetRadius(float radius)`
设置半径。

#### 使用示例

```cpp
auto shape = std::make_shared<SphereShape>(1.0f);
```

---

### BoxShape

盒体碰撞形状。

#### 头文件
```cpp
#include "render/physics/collision/collision_shapes.h"
```

#### 构造函数

```cpp
explicit BoxShape(const Vector3& halfExtents = Vector3(0.5f, 0.5f, 0.5f))
```

**参数**:
- `halfExtents`: 半尺寸 (m)

#### 成员函数

##### `const Vector3& GetHalfExtents() const`
获取半尺寸。

##### `void SetHalfExtents(const Vector3& halfExtents)`
设置半尺寸。

#### 使用示例

```cpp
auto shape = std::make_shared<BoxShape>(Vector3(1.0f, 1.0f, 1.0f));
```

---

### CapsuleShape

胶囊体碰撞形状。

#### 头文件
```cpp
#include "render/physics/collision/collision_shapes.h"
```

#### 构造函数

```cpp
CapsuleShape(float radius = 0.5f, float height = 2.0f)
```

**参数**:
- `radius`: 半径 (m)
- `height`: 高度（中心线长度）(m)

#### 成员函数

##### `float GetRadius() const`
获取半径。

##### `void SetRadius(float radius)`
设置半径。

##### `float GetHeight() const`
获取高度。

##### `void SetHeight(float height)`
设置高度。

#### 使用示例

```cpp
auto shape = std::make_shared<CapsuleShape>(0.5f, 2.0f);
```

---

## 接触流形

### ContactManifold

接触流形，表示两个物体之间的接触信息。

#### 头文件
```cpp
#include "render/physics/collision/contact_manifold.h"
```

#### 成员变量

| 类型 | 名称 | 说明 |
|------|------|------|
| `Vector3` | `normal` | 碰撞法线（从 A 指向 B） |
| `float` | `penetration` | 最大穿透深度 |
| `int` | `contactCount` | 接触点数量 |
| `std::array<ContactPoint, 4>` | `contacts` | 接触点数组（最多 4 个） |

#### 成员函数

##### `bool IsValid() const`
是否有效（有接触点）。

**返回值**: `true` 如果有接触点且穿透深度 > 0

##### `void AddContact(const Vector3& position, float pen)`
添加接触点（简化版本）。

**参数**:
- `position`: 接触点位置（世界空间）
- `pen`: 穿透深度

##### `void AddContact(const Vector3& position, const Vector3& localA, const Vector3& localB, float pen)`
添加接触点（完整信息）。

**参数**:
- `position`: 接触点位置（世界空间）
- `localA`: 在物体 A 的局部空间坐标
- `localB`: 在物体 B 的局部空间坐标
- `pen`: 穿透深度

##### `void Clear()`
清空流形。

##### `void SetNormal(const Vector3& n)`
设置法线。

**参数**:
- `n`: 法线向量（会自动归一化）

---

### ContactPoint

接触点，表示两个物体之间的一个接触点。

#### 头文件
```cpp
#include "render/physics/collision/contact_manifold.h"
```

#### 成员变量

| 类型 | 名称 | 说明 |
|------|------|------|
| `Vector3` | `position` | 接触点位置（世界空间） |
| `Vector3` | `localPointA` | 在物体 A 的局部空间坐标 |
| `Vector3` | `localPointB` | 在物体 B 的局部空间坐标 |
| `float` | `penetration` | 穿透深度 |

---

## 物理工具

### PhysicsUtils

物理工具函数类。

#### 头文件
```cpp
#include "render/physics/physics_utils.h"
```

#### 静态方法

##### 惯性张量计算

- `static Matrix3 ComputeSphereInertiaTensor(float mass, float radius)`
- `static Matrix3 ComputeBoxInertiaTensor(float mass, const Vector3& halfExtents)`
- `static Matrix3 ComputeCapsuleInertiaTensor(float mass, float radius, float height)`

##### 质量计算

- `static float ComputeSphereMass(float density, float radius)`
- `static float ComputeBoxMass(float density, const Vector3& halfExtents)`
- `static float ComputeCapsuleMass(float density, float radius, float height)`

##### 刚体初始化

##### `static void InitializeRigidBody(RigidBodyComponent& rigidBody, const ColliderComponent& collider, float density = 0.0f)`
根据碰撞体自动初始化刚体的质量和惯性张量。

**参数**:
- `rigidBody`: 刚体组件（输出）
- `collider`: 碰撞体组件
- `density`: 密度 (kg/m³)，默认使用材质密度

**示例**:
```cpp
PhysicsUtils::InitializeRigidBody(rigidBody, collider);
```

##### AABB 计算

- `static AABB ComputeWorldAABB(const ColliderComponent& collider, const Matrix4& worldMatrix)`
- `static AABB ComputeWorldAABB(const ColliderComponent& collider, const Transform& transform)`

##### 空间变换

- `static Vector3 WorldToLocal(const Vector3& worldPoint, const Transform& transform)`
- `static Vector3 LocalToWorld(const Vector3& localPoint, const Transform& transform)`
- `static Vector3 WorldToLocalDirection(const Vector3& worldDirection, const Transform& transform)`
- `static Vector3 LocalToWorldDirection(const Vector3& localDirection, const Transform& transform)`

##### 数学工具

- `static float Distance(const Vector3& a, const Vector3& b)`
- `static float DistanceSquared(const Vector3& a, const Vector3& b)`
- `static Vector3 Project(const Vector3& vector, const Vector3& onNormal)`
- `static Vector3 Reflect(const Vector3& vector, const Vector3& normal)`

---

## 物理事件

### 碰撞事件

#### CollisionEnterEvent

碰撞开始事件，当两个物体开始碰撞时触发。

**头文件**: `render/physics/physics_events.h`

**成员变量**:
- `ECS::EntityID entityA`: 实体 A
- `ECS::EntityID entityB`: 实体 B
- `ContactManifold manifold`: 接触流形

#### CollisionStayEvent

碰撞持续事件，当两个物体持续碰撞时每帧触发。

**头文件**: `render/physics/physics_events.h`

**成员变量**:
- `ECS::EntityID entityA`: 实体 A
- `ECS::EntityID entityB`: 实体 B
- `ContactManifold manifold`: 接触流形

#### CollisionExitEvent

碰撞结束事件，当两个物体分离时触发。

**头文件**: `render/physics/physics_events.h`

**成员变量**:
- `ECS::EntityID entityA`: 实体 A
- `ECS::EntityID entityB`: 实体 B

### 触发器事件

#### TriggerEnterEvent

触发器进入事件，当物体进入触发器区域时触发。

**头文件**: `render/physics/physics_events.h`

**成员变量**:
- `ECS::EntityID trigger`: 触发器实体
- `ECS::EntityID other`: 进入的物体实体
- `ContactManifold manifold`: 接触流形

#### TriggerStayEvent

触发器停留事件，当物体在触发器区域内时每帧触发。

**头文件**: `render/physics/physics_events.h`

**成员变量**:
- `ECS::EntityID trigger`: 触发器实体
- `ECS::EntityID other`: 停留的物体实体

#### TriggerExitEvent

触发器退出事件，当物体离开触发器区域时触发。

**头文件**: `render/physics/physics_events.h`

**成员变量**:
- `ECS::EntityID trigger`: 触发器实体
- `ECS::EntityID other`: 离开的物体实体

### 使用示例

```cpp
// 订阅碰撞事件
eventBus->Subscribe<CollisionEnterEvent>([](const CollisionEnterEvent& event) {
    std::cout << "碰撞开始: " << event.entityA.index 
              << " vs " << event.entityB.index << std::endl;
});

eventBus->Subscribe<CollisionExitEvent>([](const CollisionExitEvent& event) {
    std::cout << "碰撞结束: " << event.entityA.index 
              << " vs " << event.entityB.index << std::endl;
});

// 订阅触发器事件
eventBus->Subscribe<TriggerEnterEvent>([](const TriggerEnterEvent& event) {
    std::cout << "进入触发器: " << event.trigger.index 
              << " <- " << event.other.index << std::endl;
});
```

---

## 使用示例

### 完整示例：创建物理场景

```cpp
#include "render/physics/physics_components.h"
#include "render/physics/physics_systems.h"
#include "render/physics/physics_world.h"
#include "render/physics/physics_config.h"
#include "render/physics/physics_utils.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/application/event_bus.h"
#include <iostream>

using namespace Render;
using namespace Render::Physics;
using namespace Render::ECS;

int main() {
    // 1. 创建 ECS 世界
    auto world = std::make_shared<World>();
    world->Initialize();
    
    // 2. 创建事件总线
    Application::EventBus eventBus;
    
    // 3. 创建物理配置
    PhysicsConfig config = PhysicsConfig::Default();
    config.gravity = Vector3(0, -9.81f, 0);
    config.broadPhaseType = BroadPhaseType::SpatialHash;
    config.spatialHashCellSize = 5.0f;
    
    // 4. 创建物理世界
    PhysicsWorld physicsWorld(world.get(), config);
    
    // 5. 注册碰撞检测系统
    auto* collisionSystem = world->RegisterSystem<CollisionDetectionSystem>();
    auto broadPhase = std::make_unique<SpatialHashBroadPhase>(5.0f);
    collisionSystem->SetBroadPhase(std::move(broadPhase));
    collisionSystem->SetEventBus(&eventBus);
    
    // 6. 注册刚体动力学系统
    auto* physicsSystem = world->RegisterSystem<PhysicsUpdateSystem>();
    physicsSystem->SetGravity(config.gravity);
    physicsSystem->SetFixedDeltaTime(config.fixedDeltaTime);
    
    // 7. 订阅碰撞事件
    eventBus.Subscribe<CollisionEnterEvent>([](const CollisionEnterEvent& event) {
        std::cout << "碰撞开始: " << event.entityA.index 
                  << " vs " << event.entityB.index << std::endl;
    });
    
    // 8. 创建地面（静态）
    auto ground = world->CreateEntity();
    auto& groundTransform = world->AddComponent<TransformComponent>(ground);
    groundTransform.position = Vector3(0, 0, 0);
    
    auto& groundCollider = world->AddComponent<ColliderComponent>(ground);
    groundCollider = ColliderComponent::CreateBox(Vector3(10, 0.5f, 10));
    groundCollider.material = std::make_shared<PhysicsMaterial>(PhysicsMaterial::Default());
    
    auto& groundBody = world->AddComponent<RigidBodyComponent>(ground);
    groundBody.type = RigidBodyComponent::BodyType::Static;
    
    // 9. 创建动态球体
    auto ball = world->CreateEntity();
    auto& ballTransform = world->AddComponent<TransformComponent>(ball);
    ballTransform.position = Vector3(0, 10, 0);
    
    auto& ballCollider = world->AddComponent<ColliderComponent>(ball);
    ballCollider = ColliderComponent::CreateSphere(1.0f);
    ballCollider.material = std::make_shared<PhysicsMaterial>(PhysicsMaterial::Rubber());
    
    auto& ballBody = world->AddComponent<RigidBodyComponent>(ball);
    ballBody.type = RigidBodyComponent::BodyType::Dynamic;
    PhysicsUtils::InitializeRigidBody(ballBody, ballCollider);
    
    // 10. 主循环
    float deltaTime = 0.016f;  // 60 FPS
    for (int i = 0; i < 100; ++i) {
        // 更新物理系统
        world->Update(deltaTime);
        
        // 获取碰撞对
        const auto& pairs = collisionSystem->GetCollisionPairs();
        if (!pairs.empty()) {
            std::cout << "帧 " << i << ": " << pairs.size() << " 个碰撞对" << std::endl;
        }
        
        // 获取统计信息
        const auto& stats = collisionSystem->GetStats();
        if (i % 10 == 0) {
            std::cout << "统计: 碰撞体=" << stats.totalColliders 
                      << ", 碰撞对=" << stats.actualCollisions << std::endl;
        }
    }
    
    world->Shutdown();
    return 0;
}
```

---

## 注意事项

1. **内存对齐**: `RigidBodyComponent` 和 `ColliderComponent` 包含 Eigen 类型，需要内存对齐。使用 ECS 系统时，会自动处理对齐问题。

2. **碰撞层和掩码**: 使用碰撞层和掩码可以优化性能，避免不必要的碰撞检测。

3. **触发器**: 触发器不产生物理响应，仅触发事件。适合用于检测区域、拾取物品等场景。

4. **材质共享**: `PhysicsMaterial` 使用 `shared_ptr`，可以多个碰撞体共享同一个材质对象，节省内存。

5. **AABB 缓存**: `ColliderComponent` 的 AABB 会被缓存，只有在物体移动或缩放时才需要更新。

---

## 测试

物理引擎提供了完整的单元测试，位于 `tests/` 目录：

- `test_physics_math.cpp` - 数学库测试（18/18 通过）
- `test_physics_components.cpp` - 组件测试（16/16 通过）
- `test_collision_shapes.cpp` - 碰撞形状测试（14/14 通过）
- `test_broad_phase.cpp` - 粗检测测试（12/12 通过）
- `test_collision_detection.cpp` - 细检测测试（23/23 通过）
- `test_gjk.cpp` - GJK/EPA 算法测试（12/12 通过）
- `test_collision_system.cpp` - 碰撞系统集成测试（8/8 通过）
- `test_force_and_impulse_system.cpp` - 力与冲量测试（29/29 通过）
- `test_physics_update_system_interpolation.cpp` - 物理更新系统测试（12/12 通过）
- `test_physics_sleeping.cpp` - 休眠系统测试（6/6 通过）

运行测试：
```bash
./build/tests/test_physics_math
./build/tests/test_physics_components
# ... 等等
```

---

## 后续开发计划

根据开发计划，后续阶段将实现：

- **阶段 4**: 约束求解（接触约束、关节约束）
- **阶段 5**: 物理世界管理（查询系统、性能优化）
- **阶段 6-9**: 性能优化、调试可视化、测试与文档

---

## 相关文档

- [物理引擎开发 Todolists](../todolists/物理引擎开发Todolists.md) - 详细的开发任务列表
- [ECS 系统文档](ECS.md) - ECS 架构说明
- [数学库文档](Types.md) - 数学类型和工具函数

---

**最后更新**: 2025-12-06  
**文档版本**: v1.7.0
