# Bullet3 物理引擎集成 API 参考手册

> **版本**: v1.0.0  
> **开发阶段**: 阶段 1-4 已完成（基础架构 + 碰撞检测 + 刚体动力学 + 约束 + 物理-渲染同步）  
> **最后更新**: 2026-01-11

## 目录

- [概述](#概述)
- [快速开始](#快速开始)
- [核心组件](#核心组件)
  - [RigidBodyComponent](#rigidbodycomponent)
  - [ColliderComponent](#collidercomponent)
  - [ConstraintComponent](#constraintcomponent)
  - [PhysicsWorldComponent](#physicsworldcomponent)
- [物理系统](#物理系统)
  - [PhysicsSystem](#physicssystem)
- [材质系统](#材质系统)
  - [PhysicsMaterial](#physicsmaterial)
  - [PhysicsMaterialManager](#physicsmaterialmanager)
- [调试工具](#调试工具)
  - [PhysicsDebugRenderer](#physicsdebugrenderer)
- [碰撞组](#碰撞组)
- [使用示例](#使用示例)
- [注意事项](#注意事项)

---

## 概述

RenderEngine 的 Bullet3 物理引擎集成是一个基于 ECS 架构的 3D 物理模拟系统。通过封装 Bullet3 物理引擎，提供了简洁的组件化 API，支持：

- ✅ 完整的 ECS 组件支持（RigidBody、Collider、Constraint）
- ✅ 多种碰撞形状（Box、Sphere、Capsule、Cylinder、Cone、Mesh、Plane）
- ✅ 刚体动力学（力、冲量、扭矩、速度控制）
- ✅ 物理约束（点对点、铰链、滑动、圆锥扭转、6自由度、6自由度弹簧）
- ✅ 触发器系统（Trigger）
- ✅ 碰撞过滤（Group/Mask）
- ✅ 物理材质系统
- ✅ 物理-渲染自动同步（Transform ↔ Physics）
- ✅ 调试可视化（碰撞体线框、AABB、接触点）

**注意**: 物理引擎使用 Bullet3 库，所有物理操作在主线程执行，确保线程安全。

---

## 快速开始

### 1. 包含头文件

```cpp
#include "render/ecs/physics/physics_components.h"
#include "render/ecs/physics/physics_system.h"
#include "render/ecs/physics/physics_material.h"
#include "render/ecs/components.h"
#include "render/ecs/world.h"
#include "render/ecs/systems.h"
```

### 2. 注册组件和系统

```cpp
using namespace Render;
using namespace Render::ECS;

// 创建 ECS 世界
World world;
world.Initialize();

// 注册物理组件（通常在模块注册时完成）
world.RegisterComponent<RigidBodyComponent>();
world.RegisterComponent<ColliderComponent>();
world.RegisterComponent<ConstraintComponent>();
world.RegisterComponent<PhysicsWorldComponent>();

// 注册物理系统
auto* physicsSystem = world.RegisterSystem<PhysicsSystem>();
```

### 3. 创建物理世界

```cpp
// 方式1：自动创建（推荐）
auto physicsWorldEntity = physicsSystem->CreatePhysicsWorld();

// 方式2：手动创建
auto physicsWorldEntity = world.CreateEntity();
auto& physicsWorld = world.AddComponent<PhysicsWorldComponent>(physicsWorldEntity);
physicsWorld.gravity = Vector3(0, -9.81f, 0);
physicsWorld.timeStep = 1.0f / 60.0f;
physicsSystem->SetPhysicsWorldEntity(physicsWorldEntity);
```

### 4. 创建物理对象

```cpp
// 创建实体
auto entity = world.CreateEntity();

// 添加 Transform
auto& transform = world.AddComponent<TransformComponent>(entity);
transform.SetPosition(Vector3(0, 10, 0));

// 添加碰撞体
auto& collider = world.AddComponent<ColliderComponent>(entity);
collider.SetBox(Vector3(1, 1, 1));

// 添加刚体
auto& rigidBody = world.AddComponent<RigidBodyComponent>(entity);
rigidBody.type = RigidBodyType::Dynamic;
rigidBody.mass = 10.0f;
rigidBody.friction = 0.5f;
rigidBody.restitution = 0.3f;
```

### 5. 更新物理系统

```cpp
// 在主循环中
float deltaTime = 0.016f;  // 60 FPS
world.Update(deltaTime);   // 自动调用 PhysicsSystem::Update()
```

---

## 核心组件

### RigidBodyComponent

刚体组件，为实体添加物理动力学行为。

#### 头文件

```cpp
#include "render/ecs/physics/physics_components.h"
```

#### 成员变量

##### 基本属性

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `RigidBodyType` | `type` | 刚体类型（Static/Dynamic/Kinematic） | `Dynamic` |
| `float` | `mass` | 质量 (kg) | `1.0f` |
| `Vector3` | `linearVelocity` | 线性速度 (m/s) | `{0, 0, 0}` |
| `Vector3` | `angularVelocity` | 角速度 (rad/s) | `{0, 0, 0}` |

##### 物理属性

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `float` | `friction` | 摩擦系数 [0, 1]（会被材质覆盖） | `0.5f` |
| `float` | `restitution` | 弹性系数（反弹系数）[0, 1]（会被材质覆盖） | `0.0f` |
| `float` | `linearDamping` | 线性阻尼 [0, 1] | `0.0f` |
| `float` | `angularDamping` | 角阻尼 [0, 1] | `0.0f` |

##### 材质

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `std::string` | `materialName` | 材质名称（可选，如果指定则使用材质的属性） | `""` |

##### 控制标志

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `bool` | `enabled` | 是否启用物理模拟 | `true` |
| `bool` | `useGravity` | 是否受重力影响 | `true` |
| `bool` | `isKinematic` | 是否为运动学模式（手动控制位置） | `false` |

##### 同步控制

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `SyncMode` | `syncMode` | 同步模式（见下方枚举） | `PhysicsToTransform` |

#### 枚举类型

##### RigidBodyType

```cpp
enum class RigidBodyType {
    Static,      ///< 静态刚体（不受物理影响，但可以碰撞）
    Dynamic,     ///< 动态刚体（受物理影响）
    Kinematic    ///< 运动学刚体（不受物理影响，但可以移动）
};
```

##### SyncMode

```cpp
enum class SyncMode {
    PhysicsToTransform,   ///< 物理驱动 Transform（默认，用于动态刚体）
    TransformToPhysics,   ///< Transform 驱动物理（用于运动学刚体）
    Manual                ///< 手动控制，不自动同步
};
```

#### 成员函数

##### `void ApplyForce(const Vector3& force)`

应用力到刚体中心。

**参数**:
- `force`: 力向量 (N)

**注意**: 需要在 `PhysicsSystem` 更新时处理，建议在 `PhysicsSystem::Update()` 之后立即调用。

**示例**:
```cpp
auto& rb = world.GetComponent<RigidBodyComponent>(entity);
rb.ApplyForce(Vector3(0, 100, 0));  // 向上施加力
```

##### `void ApplyImpulse(const Vector3& impulse)`

应用冲量到刚体中心。

**参数**:
- `impulse`: 冲量向量 (N·s)

**注意**: 冲量会立即改变速度，适合瞬间效果（如爆炸、碰撞）。

**示例**:
```cpp
auto& rb = world.GetComponent<RigidBodyComponent>(entity);
rb.ApplyImpulse(Vector3(10, 0, 0));  // 向右施加冲量
```

##### `void ApplyTorque(const Vector3& torque)`

应用扭矩到刚体。

**参数**:
- `torque`: 扭矩向量 (N·m)

**示例**:
```cpp
auto& rb = world.GetComponent<RigidBodyComponent>(entity);
rb.ApplyTorque(Vector3(0, 0, 10));  // 绕Z轴旋转
```

##### `void SetVelocity(const Vector3& linear, const Vector3& angular)`

设置刚体的线性速度和角速度。

**参数**:
- `linear`: 线性速度 (m/s)
- `angular`: 角速度 (rad/s)

**示例**:
```cpp
auto& rb = world.GetComponent<RigidBodyComponent>(entity);
rb.SetVelocity(Vector3(5, 0, 0), Vector3(0, 0, 1));  // 向右移动并旋转
```

##### `void ClearForces()`

清除所有累积的力和扭矩。

**注意**: 通常在每一帧物理更新开始时自动调用。

#### 使用示例

```cpp
// 创建动态刚体
auto& rigidBody = world.AddComponent<RigidBodyComponent>(entity);
rigidBody.type = RigidBodyType::Dynamic;
rigidBody.mass = 5.0f;
rigidBody.useGravity = true;
rigidBody.linearDamping = 0.1f;
rigidBody.syncMode = RigidBodyComponent::SyncMode::PhysicsToTransform;

// 创建静态刚体（地面）
auto& groundBody = world.AddComponent<RigidBodyComponent>(groundEntity);
groundBody.type = RigidBodyType::Static;

// 创建运动学刚体（移动平台）
auto& platformBody = world.AddComponent<RigidBodyComponent>(platformEntity);
platformBody.type = RigidBodyType::Kinematic;
platformBody.syncMode = RigidBodyComponent::SyncMode::TransformToPhysics;
```

---

### ColliderComponent

碰撞体组件，定义物体的碰撞形状。

#### 头文件

```cpp
#include "render/ecs/physics/physics_components.h"
```

#### 成员变量

##### 形状属性

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `ColliderShape` | `shape` | 碰撞体形状类型 | `Box` |
| `Vector3` | `boxSize` | 盒子尺寸（半尺寸） | `{1, 1, 1}` |
| `float` | `sphereRadius` | 球体半径 | `0.5f` |
| `float` | `capsuleRadius` | 胶囊体半径 | `0.5f` |
| `float` | `capsuleHeight` | 胶囊体高度 | `1.0f` |
| `Vector3` | `cylinderSize` | 圆柱/圆锥尺寸 | `{1, 1, 1}` |
| `std::string` | `meshName` | 网格资源名称（用于网格碰撞体） | `""` |
| `bool` | `useConvexHull` | 是否使用凸包（否则使用三角网格） | `true` |
| `Vector3` | `planeNormal` | 平面法线 | `{0, 1, 0}` |
| `float` | `planeConstant` | 平面常数 | `0.0f` |

##### 偏移和旋转

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `Vector3` | `offset` | 相对于 Transform 的偏移 | `{0, 0, 0}` |
| `Quaternion` | `rotation` | 相对于 Transform 的旋转 | `{1, 0, 0, 0}` |

##### 触发器

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `bool` | `isTrigger` | 是否为触发器（不产生物理响应） | `false` |

##### 材质

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `std::string` | `materialName` | 材质名称（可选） | `""` |

##### 碰撞过滤

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `uint16_t` | `collisionGroup` | 碰撞组（位掩码） | `0x0001` |
| `uint16_t` | `collisionMask` | 碰撞遮罩（与哪些组碰撞） | `0xFFFF` |

#### 枚举类型

##### ColliderShape

```cpp
enum class ColliderShape {
    Box,          ///< 盒子
    Sphere,       ///< 球体
    Capsule,      ///< 胶囊体
    Cylinder,     ///< 圆柱体
    Cone,         ///< 圆锥体
    Mesh,         ///< 网格（凸包或三角网格）
    Plane         ///< 平面
};
```

#### 成员函数

##### `void SetBox(const Vector3& size)`

设置盒子碰撞体。

**参数**:
- `size`: 盒子尺寸（半尺寸）

**示例**:
```cpp
collider.SetBox(Vector3(1.0f, 1.0f, 1.0f));
```

##### `void SetSphere(float radius)`

设置球体碰撞体。

**参数**:
- `radius`: 半径 (m)

**示例**:
```cpp
collider.SetSphere(1.0f);
```

##### `void SetCapsule(float radius, float height)`

设置胶囊体碰撞体。

**参数**:
- `radius`: 半径 (m)
- `height`: 高度（中心线长度）(m)

**示例**:
```cpp
collider.SetCapsule(0.5f, 2.0f);
```

##### `void SetMesh(const std::string& name, bool convexHull = true)`

设置网格碰撞体。

**参数**:
- `name`: 网格资源名称
- `convexHull`: 是否使用凸包（`true`）或三角网格（`false`）

**注意**: 凸包碰撞体性能更好，但只能用于凸形状。三角网格可用于凹形状，但性能较低。

**示例**:
```cpp
collider.SetMesh("ground_mesh", false);  // 使用三角网格（凹形状）
```

#### 使用示例

```cpp
// 创建球体碰撞体
auto& collider = world.AddComponent<ColliderComponent>(entity);
collider.SetSphere(1.0f);
collider.materialName = "rubber";
collider.collisionGroup = CollisionGroups::DYNAMIC;
collider.collisionMask = CollisionGroups::ALL;

// 创建盒体碰撞体（触发器）
auto& trigger = world.AddComponent<ColliderComponent>(triggerEntity);
trigger.SetBox(Vector3(5.0f, 1.0f, 5.0f));
trigger.isTrigger = true;
trigger.collisionGroup = CollisionGroups::TRIGGER;
```

---

### ConstraintComponent

约束组件，连接两个刚体的约束。

#### 头文件

```cpp
#include "render/ecs/physics/physics_components.h"
```

#### 成员变量

##### 基础属性

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `ConstraintType` | `type` | 约束类型 | `PointToPoint` |
| `EntityID` | `connectedEntity` | 连接的实体ID | `Invalid()` |
| `bool` | `enabled` | 是否启用 | `true` |

##### 约束点（本地空间）

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `Vector3` | `pivotA` | 实体A的约束点 | `{0, 0, 0}` |
| `Vector3` | `pivotB` | 实体B的约束点 | `{0, 0, 0}` |

##### 约束轴（铰链、滑动等）

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `Vector3` | `axisA` | 实体A的轴 | `{1, 0, 0}` |
| `Vector3` | `axisB` | 实体B的轴 | `{1, 0, 0}` |

##### 限制（角度/距离）

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `float` | `lowerLimit` | 下限 | `0.0f` |
| `float` | `upperLimit` | 上限 | `0.0f` |

##### 弹簧参数（Generic6DofSpring）

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `bool` | `enableSpring` | 是否启用弹簧 | `false` |
| `float` | `springStiffness` | 弹簧刚度 | `0.0f` |
| `float` | `springDamping` | 弹簧阻尼 | `0.0f` |

#### 枚举类型

##### ConstraintType

```cpp
enum class ConstraintType {
    PointToPoint,     ///< 点对点约束
    Hinge,            ///< 铰链约束
    Slider,           ///< 滑动约束
    ConeTwist,        ///< 圆锥扭转约束
    Generic6Dof,      ///< 6自由度约束
    Generic6DofSpring ///< 6自由度弹簧约束
};
```

#### 使用示例

```cpp
// 创建铰链约束
auto constraintEntity = world.CreateEntity();
auto& constraint = world.AddComponent<ConstraintComponent>(constraintEntity);
constraint.type = ConstraintType::Hinge;
constraint.connectedEntity = doorEntity;
constraint.pivotA = Vector3(0, 0, 0);
constraint.axisA = Vector3(0, 1, 0);  // 绕Y轴旋转
constraint.lowerLimit = -MathUtils::PI / 2.0f;  // -90度
constraint.upperLimit = MathUtils::PI / 2.0f;   // +90度
```

---

### PhysicsWorldComponent

物理世界组件，标记实体为物理世界根节点。

#### 头文件

```cpp
#include "render/ecs/physics/physics_components.h"
```

#### 成员变量

| 类型 | 名称 | 说明 | 默认值 |
|------|------|------|--------|
| `Vector3` | `gravity` | 重力加速度 (m/s²) | `{0, -9.81f, 0}` |
| `float` | `timeStep` | 固定时间步长（秒） | `1.0f / 60.0f` |
| `int` | `maxSubSteps` | 最大子步数 | `10` |
| `bool` | `enabled` | 是否启用物理模拟 | `true` |

#### 使用示例

```cpp
auto physicsWorldEntity = world.CreateEntity();
auto& physicsWorld = world.AddComponent<PhysicsWorldComponent>(physicsWorldEntity);
physicsWorld.gravity = Vector3(0, -9.81f, 0);
physicsWorld.timeStep = 1.0f / 60.0f;  // 60 Hz
physicsWorld.maxSubSteps = 10;
```

---

## 物理系统

### PhysicsSystem

物理系统，管理物理模拟，同步 Transform 和物理体状态。

#### 头文件

```cpp
#include "render/ecs/physics/physics_system.h"
```

#### 系统优先级

优先级：`15`（在 `TransformSystem` 之后）

#### 构造函数

```cpp
PhysicsSystem()
```

#### 成员函数

##### 物理世界管理

##### `EntityID GetPhysicsWorldEntity() const`

获取物理世界组件对应的实体。

**返回值**: 物理世界实体ID，如果不存在返回 `Invalid()`

##### `bool SetPhysicsWorldEntity(EntityID entity)`

设置物理世界实体。

**参数**:
- `entity`: 物理世界实体ID

**返回值**: 成功返回 `true`

##### `EntityID CreatePhysicsWorld()`

创建物理世界实体。

**返回值**: 新创建的物理世界实体ID

**示例**:
```cpp
auto physicsWorldEntity = physicsSystem->CreatePhysicsWorld();
```

##### 物理模拟控制

##### `void SetEnabled(bool enabled)`

启用/禁用物理模拟。

**参数**:
- `enabled`: 是否启用

##### `bool IsEnabled() const`

获取是否启用。

**返回值**: 是否启用

##### `void SetGravity(const Vector3& gravity)`

设置重力。

**参数**:
- `gravity`: 重力向量 (m/s²)

**示例**:
```cpp
physicsSystem->SetGravity(Vector3(0, -9.81f, 0));
```

##### `Vector3 GetGravity() const`

获取重力。

**返回值**: 重力加速度向量

##### 材质管理

##### `PhysicsMaterialManager* GetMaterialManager() const`

获取材质管理器。

**返回值**: 材质管理器指针

**示例**:
```cpp
auto* materialManager = physicsSystem->GetMaterialManager();
auto* material = materialManager->GetMaterial("rubber");
```

##### `bool LoadMaterialsFromFile(const std::string& filePath)`

加载材质定义文件。

**参数**:
- `filePath`: JSON 文件路径

**返回值**: 成功返回 `true`

**示例**:
```cpp
physicsSystem->LoadMaterialsFromFile("assets/materials/physics_materials.json");
```

##### 调试渲染

##### `void SetDebugDrawEnabled(bool enabled)`

设置是否启用调试绘制。

**参数**:
- `enabled`: 是否启用

##### `bool IsDebugDrawEnabled() const`

获取是否启用调试绘制。

**返回值**: 是否启用

##### `PhysicsDebugRenderer* GetDebugRenderer() const`

获取调试渲染器。

**返回值**: 调试渲染器指针

**示例**:
```cpp
auto* debugRenderer = physicsSystem->GetDebugRenderer();
debugRenderer->SetShowWireframe(true);
debugRenderer->SetShowAABB(true);
physicsSystem->SetDebugDrawEnabled(true);
```

##### 查询接口

##### `std::vector<RaycastHit> Raycast(const Vector3& start, const Vector3& end) const`

射线检测。

**参数**:
- `start`: 起点
- `end`: 终点

**返回值**: 碰撞结果列表

**数据结构**:
```cpp
struct RaycastHit {
    EntityID entity;    ///< 碰撞的实体
    Vector3 point;      ///< 碰撞点
    Vector3 normal;     ///< 碰撞法线
    float distance;     ///< 距离
};
```

**示例**:
```cpp
auto hits = physicsSystem->Raycast(
    Vector3(0, 10, 0),  // 起点
    Vector3(0, -10, 0)  // 终点
);

for (const auto& hit : hits) {
    std::cout << "Hit entity: " << hit.entity.index << std::endl;
    std::cout << "Hit point: " << hit.point << std::endl;
    std::cout << "Distance: " << hit.distance << std::endl;
}
```

##### `std::vector<EntityID> SphereCast(const Vector3& center, float radius) const`

球形检测。

**参数**:
- `center`: 中心点
- `radius`: 半径

**返回值**: 碰撞的实体列表

**示例**:
```cpp
auto entities = physicsSystem->SphereCast(Vector3(0, 5, 0), 2.0f);
for (auto entity : entities) {
    std::cout << "Entity in sphere: " << entity.index << std::endl;
}
```

##### 统计信息

##### `const PhysicsStats& GetStats() const`

获取统计信息。

**返回值**: 统计信息结构体的常量引用

**数据结构**:
```cpp
struct PhysicsStats {
    size_t rigidBodyCount = 0;    ///< 刚体数量
    size_t colliderCount = 0;      ///< 碰撞体数量
    size_t constraintCount = 0;    ///< 约束数量
    float simulationTime = 0.0f;   ///< 模拟时间（ms）
    int stepCount = 0;             ///< 步数
};
```

**示例**:
```cpp
const auto& stats = physicsSystem->GetStats();
std::cout << "RigidBodies: " << stats.rigidBodyCount << std::endl;
std::cout << "Simulation Time: " << stats.simulationTime << " ms" << std::endl;
```

#### 使用示例

```cpp
// 注册物理系统
auto* physicsSystem = world.RegisterSystem<PhysicsSystem>();

// 创建物理世界
auto physicsWorldEntity = physicsSystem->CreatePhysicsWorld();

// 设置重力
physicsSystem->SetGravity(Vector3(0, -9.81f, 0));

// 启用调试绘制
auto* debugRenderer = physicsSystem->GetDebugRenderer();
debugRenderer->SetShowWireframe(true);
debugRenderer->SetShowContacts(true);
physicsSystem->SetDebugDrawEnabled(true);

// 在主循环中，系统会自动更新
world.Update(deltaTime);
```

---

## 材质系统

### PhysicsMaterial

物理材质，定义物体表面的物理属性。

#### 头文件

```cpp
#include "render/ecs/physics/physics_material.h"
```

#### 成员变量

| 类型 | 名称 | 说明 | 默认值 | 范围 |
|------|------|------|--------|------|
| `std::string` | `name` | 材质名称 | `""` | - |
| `float` | `friction` | 摩擦系数 | `0.5f` | [0, 1] |
| `float` | `restitution` | 弹性系数（恢复系数） | `0.0f` | [0, 1] |
| `float` | `density` | 密度 (kg/m³) | `1000.0f` | > 0 |

#### 构造函数

```cpp
PhysicsMaterial()
PhysicsMaterial(const std::string& n, float f, float r, float d)
```

**参数**:
- `n`: 材质名称
- `f`: 摩擦系数
- `r`: 弹性系数
- `d`: 密度

#### 使用示例

```cpp
// 创建材质
PhysicsMaterial rubber("rubber", 0.8f, 0.9f, 1000.0f);

// 使用材质
auto& collider = world.GetComponent<ColliderComponent>(entity);
collider.materialName = "rubber";
```

---

### PhysicsMaterialManager

物理材质管理器，管理物理材质的加载和查找。

#### 头文件

```cpp
#include "render/ecs/physics/physics_material.h"
```

#### 成员函数

##### `bool LoadMaterialsFromFile(const std::string& filePath)`

加载材质定义文件。

**参数**:
- `filePath`: JSON 文件路径

**返回值**: 成功返回 `true`

**JSON 格式示例**:
```json
{
  "materials": [
    {
      "name": "rubber",
      "friction": 0.8,
      "restitution": 0.9,
      "density": 1000.0
    },
    {
      "name": "metal",
      "friction": 0.5,
      "restitution": 0.1,
      "density": 7800.0
    }
  ]
}
```

##### `bool LoadMaterialsFromJson(const std::string& json)`

加载材质定义（从 JSON 字符串）。

**参数**:
- `json`: JSON 字符串

**返回值**: 成功返回 `true`

##### `bool RegisterMaterial(const PhysicsMaterial& material)`

注册材质。

**参数**:
- `material`: 材质对象

**返回值**: 成功返回 `true`，如果同名材质已存在返回 `false`

##### `const PhysicsMaterial* GetMaterial(const std::string& name) const`

获取材质。

**参数**:
- `name`: 材质名称

**返回值**: 材质指针，如果不存在返回 `nullptr`

##### `bool HasMaterial(const std::string& name) const`

检查材质是否存在。

**参数**:
- `name`: 材质名称

**返回值**: 存在返回 `true`

##### `void Clear()`

清除所有材质。

##### `std::vector<std::string> GetAllMaterialNames() const`

获取所有材质名称。

**返回值**: 材质名称列表

#### 使用示例

```cpp
auto* materialManager = physicsSystem->GetMaterialManager();

// 加载材质文件
materialManager->LoadMaterialsFromFile("assets/materials/physics_materials.json");

// 注册材质
PhysicsMaterial rubber("rubber", 0.8f, 0.9f, 1000.0f);
materialManager->RegisterMaterial(rubber);

// 获取材质
auto* material = materialManager->GetMaterial("rubber");
if (material) {
    std::cout << "Friction: " << material->friction << std::endl;
}

// 应用到碰撞体
auto& collider = world.GetComponent<ColliderComponent>(entity);
collider.materialName = "rubber";
```

---

## 调试工具

### PhysicsDebugRenderer

物理调试渲染器，用于可视化物理世界。

#### 头文件

```cpp
#include "render/ecs/physics/physics_debug_renderer.h"
```

#### 成员函数

##### 控制接口

##### `void SetEnabled(bool enabled)`

设置是否启用调试绘制。

**参数**:
- `enabled`: 是否启用

##### `bool IsEnabled() const`

获取是否启用。

**返回值**: 是否启用

##### `void Clear()`

清空调试绘制数据（每帧调用）。

##### 调试模式设置

##### `void SetShowWireframe(bool show)`

设置显示碰撞体线框。

**参数**:
- `show`: 是否显示

##### `void SetShowAABB(bool show)`

设置显示 AABB。

**参数**:
- `show`: 是否显示

##### `void SetShowContacts(bool show)`

设置显示接触点。

**参数**:
- `show`: 是否显示

##### `void SetShowConstraints(bool show)`

设置显示约束。

**参数**:
- `show`: 是否显示

##### 数据获取

##### `const std::vector<DebugLine>& GetDebugLines() const`

获取当前帧的调试线条数据。

**返回值**: 调试线条列表

**数据结构**:
```cpp
struct DebugLine {
    Vector3 from;
    Vector3 to;
    Color color;
};
```

##### `const std::vector<DebugContact>& GetDebugContacts() const`

获取当前帧的接触点数据。

**返回值**: 调试接触点列表

**数据结构**:
```cpp
struct DebugContact {
    Vector3 point;
    Vector3 normal;
    float distance;
    Color color;
};
```

#### 使用示例

```cpp
// 获取调试渲染器
auto* debugRenderer = physicsSystem->GetDebugRenderer();

// 启用调试绘制
debugRenderer->SetShowWireframe(true);
debugRenderer->SetShowAABB(true);
debugRenderer->SetShowContacts(true);
debugRenderer->SetEnabled(true);
physicsSystem->SetDebugDrawEnabled(true);

// 在渲染循环中
debugRenderer->Clear();  // 每帧清空
world.Update(deltaTime); // 物理更新会填充调试数据

// 获取调试数据并渲染
const auto& lines = debugRenderer->GetDebugLines();
for (const auto& line : lines) {
    // 渲染线条
    drawLine(line.from, line.to, line.color);
}

const auto& contacts = debugRenderer->GetDebugContacts();
for (const auto& contact : contacts) {
    // 渲染接触点
    drawPoint(contact.point, contact.normal, contact.color);
}
```

---

## 碰撞组

预定义的碰撞组常量，使用位掩码进行组合。

#### 头文件

```cpp
#include "render/ecs/physics/physics_components.h"
```

#### 常量定义

```cpp
namespace CollisionGroups {
    constexpr uint16_t DEFAULT = 0x0001;      ///< 默认组
    constexpr uint16_t STATIC = 0x0002;       ///< 静态物体组
    constexpr uint16_t KINEMATIC = 0x0004;    ///< 运动学物体组
    constexpr uint16_t DYNAMIC = 0x0008;      ///< 动态物体组
    constexpr uint16_t PLAYER = 0x0010;       ///< 玩家组
    constexpr uint16_t ENEMY = 0x0020;        ///< 敌人组
    constexpr uint16_t PROJECTILE = 0x0040;   ///< 投射物组
    constexpr uint16_t TRIGGER = 0x0080;      ///< 触发器组
    constexpr uint16_t SENSOR = 0x0100;       ///< 传感器组
    constexpr uint16_t ALL = 0xFFFF;          ///< 所有组
    constexpr uint16_t NONE = 0x0000;         ///< 无组
}
```

#### 使用示例

```cpp
// 设置碰撞组和遮罩
auto& collider = world.GetComponent<ColliderComponent>(entity);

// 玩家只与敌人和静态物体碰撞
collider.collisionGroup = CollisionGroups::PLAYER;
collider.collisionMask = CollisionGroups::ENEMY | CollisionGroups::STATIC;

// 投射物只与敌人碰撞
collider.collisionGroup = CollisionGroups::PROJECTILE;
collider.collisionMask = CollisionGroups::ENEMY;

// 触发器与所有动态物体碰撞
collider.collisionGroup = CollisionGroups::TRIGGER;
collider.collisionMask = CollisionGroups::DYNAMIC | CollisionGroups::PLAYER;
```

---

## 使用示例

### 完整示例：掉落物体

```cpp
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/ecs/systems.h"
#include "render/ecs/physics/physics_components.h"
#include "render/ecs/physics/physics_system.h"

using namespace Render;
using namespace Render::ECS;

void CreateFallingBox(World& world, PhysicsSystem& physicsSystem) {
    // 创建物理世界（如果还没有）
    auto physicsWorldEntity = physicsSystem.GetPhysicsWorldEntity();
    if (!physicsWorldEntity.IsValid()) {
        physicsWorldEntity = physicsSystem.CreatePhysicsWorld();
    }
    
    // 创建掉落物体
    auto entity = world.CreateEntity();
    
    // Transform
    auto& transform = world.AddComponent<TransformComponent>(entity);
    transform.SetPosition(Vector3(0, 10, 0));
    
    // 碰撞体
    auto& collider = world.AddComponent<ColliderComponent>(entity);
    collider.SetBox(Vector3(1, 1, 1));
    collider.materialName = "rubber";
    
    // 刚体
    auto& rigidBody = world.AddComponent<RigidBodyComponent>(entity);
    rigidBody.type = RigidBodyType::Dynamic;
    rigidBody.mass = 10.0f;
    rigidBody.friction = 0.5f;
    rigidBody.restitution = 0.3f;  // 有弹性
    rigidBody.syncMode = RigidBodyComponent::SyncMode::PhysicsToTransform;
}

void CreateGround(World& world) {
    auto ground = world.CreateEntity();
    
    // Transform
    auto& transform = world.AddComponent<TransformComponent>(ground);
    transform.SetPosition(Vector3(0, 0, 0));
    transform.SetScale(Vector3(10, 1, 10));
    
    // 碰撞体
    auto& collider = world.AddComponent<ColliderComponent>(ground);
    collider.SetBox(Vector3(10, 1, 10));
    
    // 静态刚体
    auto& rigidBody = world.AddComponent<RigidBodyComponent>(ground);
    rigidBody.type = RigidBodyType::Static;
    rigidBody.mass = 0.0f;  // 静态物体质量为0
}

void CreateTriggerZone(World& world) {
    auto trigger = world.CreateEntity();
    
    // Transform
    auto& transform = world.AddComponent<TransformComponent>(trigger);
    transform.SetPosition(Vector3(0, 5, 0));
    
    // 触发器碰撞体
    auto& collider = world.AddComponent<ColliderComponent>(trigger);
    collider.SetSphere(2.0f);
    collider.isTrigger = true;  // 设置为触发器
    collider.collisionGroup = CollisionGroups::TRIGGER;
    collider.collisionMask = CollisionGroups::DYNAMIC;
    
    // 不需要 RigidBodyComponent（触发器不需要物理响应）
}

int main() {
    // 创建 ECS 世界
    World world;
    world.Initialize();
    
    // 注册组件和系统
    world.RegisterComponent<RigidBodyComponent>();
    world.RegisterComponent<ColliderComponent>();
    world.RegisterComponent<ConstraintComponent>();
    world.RegisterComponent<PhysicsWorldComponent>();
    
    auto* physicsSystem = world.RegisterSystem<PhysicsSystem>();
    
    // 创建物理世界
    physicsSystem->CreatePhysicsWorld();
    physicsSystem->SetGravity(Vector3(0, -9.81f, 0));
    
    // 加载材质
    physicsSystem->LoadMaterialsFromFile("assets/materials/physics_materials.json");
    
    // 创建场景
    CreateGround(world);
    CreateFallingBox(world, *physicsSystem);
    CreateTriggerZone(world);
    
    // 主循环
    float deltaTime = 0.016f;  // 60 FPS
    for (int i = 0; i < 1000; ++i) {
        world.Update(deltaTime);
        
        // 获取统计信息
        const auto& stats = physicsSystem->GetStats();
        if (i % 60 == 0) {
            std::cout << "Frame " << i << ": "
                      << stats.rigidBodyCount << " rigid bodies, "
                      << stats.simulationTime << " ms" << std::endl;
        }
    }
    
    world.Shutdown();
    return 0;
}
```

### 示例：铰链约束（门）

```cpp
void CreateDoor(World& world, PhysicsSystem& physicsSystem) {
    // 创建门实体
    auto door = world.CreateEntity();
    
    auto& doorTransform = world.AddComponent<TransformComponent>(door);
    doorTransform.SetPosition(Vector3(5, 2, 0));
    
    auto& doorCollider = world.AddComponent<ColliderComponent>(door);
    doorCollider.SetBox(Vector3(1, 2, 0.1f));
    
    auto& doorBody = world.AddComponent<RigidBodyComponent>(door);
    doorBody.type = RigidBodyType::Dynamic;
    doorBody.mass = 10.0f;
    
    // 创建门框（静态）
    auto frame = world.CreateEntity();
    
    auto& frameTransform = world.AddComponent<TransformComponent>(frame);
    frameTransform.SetPosition(Vector3(5, 2, 0));
    
    auto& frameCollider = world.AddComponent<ColliderComponent>(frame);
    frameCollider.SetBox(Vector3(0.1f, 2, 0.1f));
    
    auto& frameBody = world.AddComponent<RigidBodyComponent>(frame);
    frameBody.type = RigidBodyType::Static;
    
    // 创建铰链约束
    auto constraintEntity = world.CreateEntity();
    auto& constraint = world.AddComponent<ConstraintComponent>(constraintEntity);
    constraint.type = ConstraintType::Hinge;
    constraint.connectedEntity = frame;  // 连接到门框
    constraint.pivotA = Vector3(0, 0, 0);  // 门的约束点
    constraint.pivotB = Vector3(1, 0, 0);  // 门框的约束点
    constraint.axisA = Vector3(0, 1, 0);   // 绕Y轴旋转
    constraint.axisB = Vector3(0, 1, 0);
    constraint.lowerLimit = -MathUtils::PI / 2.0f;  // -90度
    constraint.upperLimit = MathUtils::PI / 2.0f;   // +90度
}
```

### 示例：射线检测（拾取物体）

```cpp
void PickObject(PhysicsSystem& physicsSystem, const Vector3& cameraPos, 
                const Vector3& cameraForward, float maxDistance) {
    Vector3 start = cameraPos;
    Vector3 end = cameraPos + cameraForward * maxDistance;
    
    auto hits = physicsSystem.Raycast(start, end);
    
    if (!hits.empty()) {
        const auto& closestHit = hits[0];  // 第一个是最接近的
        std::cout << "Picked entity: " << closestHit.entity.index << std::endl;
        std::cout << "Hit point: " << closestHit.point << std::endl;
        
        // 可以对拾取的物体进行操作
        // ...
    }
}
```

---

## 注意事项

### 1. 线程安全

- **所有物理操作都在主线程执行**：`PhysicsSystem` 的所有操作必须在主线程中执行，确保与 Bullet3 的线程安全要求一致。
- **组件访问**：在 `PhysicsSystem::Update()` 期间，不要在其他线程修改物理组件。

### 2. 单位系统

- **MKS 单位系统**：统一使用米-千克-秒（MKS）单位系统。
- **Transform 位置**：单位是米 (m)
- **质量**：单位是千克 (kg)
- **速度**：单位是米/秒 (m/s)
- **力**：单位是牛顿 (N)

### 3. 内存管理

- **自动管理**：Bullet 对象由 `PhysicsSystem` 自动管理，在组件销毁时自动清理。
- **避免手动删除**：不要手动删除 `bulletRigidBody` 或 `bulletCollisionShape` 指针。

### 4. 父子关系

- **物理体不支持父子关系**：物理体之间不能直接建立父子关系。
- **Transform 层级**：子对象的物理状态会通过 Transform 层级自动跟随父对象。
- **建议**：对于复杂结构，使用约束（Constraint）连接物理体。

### 5. 性能优化

- **使用简单碰撞体**：对于不需要精确物理的对象，使用 Box、Sphere 等简单碰撞体。
- **碰撞过滤**：使用碰撞组和遮罩减少不必要的碰撞检测。
- **休眠**：静态或静止的物理体会自动休眠，节省性能。

### 6. 同步模式

- **PhysicsToTransform（默认）**：用于动态刚体，物理模拟结果同步到 Transform。
- **TransformToPhysics**：用于运动学刚体，Transform 变化同步到物理体。
- **Manual**：手动控制，需要手动同步。

### 7. 调试

- **调试可视化**：使用 `PhysicsDebugRenderer` 可视化碰撞体和约束。
- **统计信息**：使用 `PhysicsSystem::GetStats()` 获取物理模拟统计信息。

---

## 相关文档

- [物理引擎集成设计文档](../PHYSICS_ENGINE_INTEGRATION.md) - 详细的设计文档
- [ECS 系统文档](ECS.md) - ECS 架构说明
- [Transform 文档](Transform.md) - Transform 系统说明

---

**文档版本**：1.0.0  
**最后更新**：2026-01-11
**作者**：Linductor
