# Bullet3 物理引擎集成设计文档

## 📋 目录

- [概述](#概述)
- [设计目标](#设计目标)
- [架构设计](#架构设计)
- [组件设计](#组件设计)
- [系统设计](#系统设计)
- [数据同步机制](#数据同步机制)
- [实现计划](#实现计划)
- [API设计](#api设计)
- [使用示例](#使用示例)
- [性能优化](#性能优化)
- [注意事项](#注意事项)

---

## 概述

本文档描述如何将 Bullet3 物理引擎集成到基于 ECS 架构的渲染引擎中。物理引擎将作为 ECS 系统的一部分，与现有的 Transform 系统和渲染系统无缝协作。

### 当前状态

- ✅ Bullet3 已在 `third_party/bullet3` 目录中
- ✅ CMakeLists.txt 已配置 Bullet3（目前仅用于 URDF）
- ✅ ECS 系统架构完整，支持组件和系统
- ✅ TransformComponent 使用 `Ref<Transform>` 管理变换
- ✅ TransformSystem 负责同步 Transform 层级关系

### 集成目标

- ✅ 将 Bullet3 集成到 ECS 架构中
- ✅ 提供物理组件（RigidBody、Collider 等）
- ✅ 实现物理系统（PhysicsSystem）
- ✅ 自动同步 Transform 和物理体状态
- ✅ 支持刚体、碰撞体、约束等物理特性
- ✅ 线程安全设计

---

## 设计目标

### 功能目标

1. **物理模拟**
   - 刚体动力学（RigidBody）
   - 碰撞检测（Collider）
   - 物理约束（Constraint）
   - 触发器（Trigger）

2. **ECS 集成**
   - 物理组件作为 ECS 组件
   - 物理系统作为 ECS 系统
   - 与 Transform 系统无缝协作

3. **数据同步**
   - Transform ↔ 物理体位置/旋转同步
   - 支持物理驱动和手动控制两种模式
   - 自动处理父子关系

4. **性能优化**
   - 批量更新物理体
   - 减少不必要的同步操作
   - 支持物理模拟暂停/恢复

### 非功能目标

- **线程安全**：所有物理操作线程安全
- **易用性**：简洁的 API，符合项目风格
- **可扩展性**：易于添加新的物理特性
- **性能**：不影响渲染性能

---

## 架构设计

### 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                         │
│                  (Game Logic / Scene)                        │
└─────────────────────────────────────────────────────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      ECS Layer                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │ Transform    │  │ Physics      │  │ Render       │       │
│  │ System       │  │ System       │  │ System       │       │
│  └──────────────┘  └──────────────┘  └──────────────┘       │
│         │                  │                    │           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │ Transform    │  │ RigidBody    │  │ MeshRender   │       │
│  │ Component    │  │ Component    │  │ Component    │       │
│  └──────────────┘  └──────────────┘  └──────────────┘       │
└─────────────────────────────────────────────────────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────┐
│              Physics Engine Wrapper Layer                   │
│  ┌──────────────────────────────────────────────────────┐   │
│  │         BulletPhysicsWorld (封装类)                   │  │
│  │  - btDiscreteDynamicsWorld                           │   │
│  │  - btBroadphaseInterface                             │   │
│  │  - btCollisionDispatcher                             │   │
│  │  - btConstraintSolver                                │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Bullet3 Library                          │
└─────────────────────────────────────────────────────────────┘
```

### 系统执行顺序

```
优先级顺序（从高到低）：
1. WindowSystem (3)          - 窗口管理
2. CameraSystem (5)          - 相机更新
3. TransformSystem (10)       - Transform 层级同步
4. PhysicsSystem (15)         - 物理模拟（新增）
5. ResourceLoadingSystem (20)  - 资源加载
6. LightSystem (50)           - 光照管理
7. UniformSystem (90)         - Uniform 管理
8. MeshRenderSystem (100)     - 网格渲染
9. SpriteRenderSystem (200)   - 精灵渲染
```

**说明**：
- PhysicsSystem 优先级设为 15，在 TransformSystem (10) 之后
- 这样确保 Transform 层级关系先更新，物理系统再同步到物理世界
- 物理模拟后，Transform 会被更新，然后渲染系统使用更新后的 Transform

---

## 组件设计

### 1. RigidBodyComponent（刚体组件）

```cpp
namespace Render {
namespace ECS {

/**
 * @brief 刚体类型
 */
enum class RigidBodyType {
    Static,      ///< 静态刚体（不受物理影响，但可以碰撞）
    Dynamic,     ///< 动态刚体（受物理影响）
    Kinematic    ///< 运动学刚体（不受物理影响，但可以移动）
};

/**
 * @brief 刚体组件
 * 
 * 为实体添加物理刚体特性
 * 需要配合 TransformComponent 使用
 */
struct RigidBodyComponent {
    // ==================== 基本属性 ====================
    RigidBodyType type = RigidBodyType::Dynamic;  ///< 刚体类型
    
    float mass = 1.0f;                             ///< 质量（kg）
    Vector3 linearVelocity{0, 0, 0};              ///< 线性速度
    Vector3 angularVelocity{0, 0, 0};             ///< 角速度
    
    // ==================== 物理属性 ====================
    float friction = 0.5f;                        ///< 摩擦系数
    float restitution = 0.0f;                    ///< 弹性系数（反弹）
    float linearDamping = 0.0f;                   ///< 线性阻尼
    float angularDamping = 0.0f;                  ///< 角阻尼
    
    // ==================== 控制标志 ====================
    bool enabled = true;                          ///< 是否启用物理模拟
    bool useGravity = true;                        ///< 是否受重力影响
    bool isKinematic = false;                     ///< 是否为运动学模式（手动控制位置）
    
    // ==================== 同步控制 ====================
    /**
     * @brief 同步模式
     * 
     * - PhysicsToTransform: 物理模拟结果同步到 Transform（默认）
     * - TransformToPhysics: Transform 变化同步到物理体（用于运动学）
     * - Manual: 手动控制，不自动同步
     */
    enum class SyncMode {
        PhysicsToTransform,   ///< 物理驱动 Transform
        TransformToPhysics,   ///< Transform 驱动物理
        Manual                ///< 手动控制
    };
    
    SyncMode syncMode = SyncMode::PhysicsToTransform;
    
    // ==================== 内部状态 ====================
    void* bulletRigidBody = nullptr;              ///< Bullet 刚体指针（内部使用）
    bool needsSync = true;                         ///< 是否需要同步（内部使用）
    
    RigidBodyComponent() = default;
    
    // ==================== 便捷方法 ====================
    
    /**
     * @brief 应用力
     * @param force 力向量
     */
    void ApplyForce(const Vector3& force);
    
    /**
     * @brief 应用冲量
     * @param impulse 冲量向量
     */
    void ApplyImpulse(const Vector3& impulse);
    
    /**
     * @brief 应用扭矩
     * @param torque 扭矩向量
     */
    void ApplyTorque(const Vector3& torque);
    
    /**
     * @brief 设置速度
     * @param linear 线性速度
     * @param angular 角速度
     */
    void SetVelocity(const Vector3& linear, const Vector3& angular);
    
    /**
     * @brief 清除所有力
     */
    void ClearForces();
};

} // namespace ECS
} // namespace Render
```

### 2. ColliderComponent（碰撞体组件）

```cpp
namespace Render {
namespace ECS {

/**
 * @brief 碰撞体形状类型
 */
enum class ColliderShape {
    Box,          ///< 盒子
    Sphere,       ///< 球体
    Capsule,      ///< 胶囊体
    Cylinder,     ///< 圆柱体
    Cone,         ///< 圆锥体
    Mesh,         ///< 网格（静态或凸包）
    Plane         ///< 平面
};

/**
 * @brief 碰撞体组件
 * 
 * 定义实体的碰撞形状
 * 需要配合 RigidBodyComponent 使用（或单独使用作为触发器）
 */
struct ColliderComponent {
    // ==================== 形状属性 ====================
    ColliderShape shape = ColliderShape::Box;     ///< 碰撞体形状
    
    // Box 参数
    Vector3 boxSize{1, 1, 1};                      ///< 盒子尺寸
    
    // Sphere 参数
    float sphereRadius = 0.5f;                    ///< 球体半径
    
    // Capsule 参数
    float capsuleRadius = 0.5f;                   ///< 胶囊体半径
    float capsuleHeight = 1.0f;                   ///< 胶囊体高度
    
    // Cylinder/Cone 参数
    Vector3 cylinderSize{1, 1, 1};                ///< 圆柱/圆锥尺寸
    
    // Mesh 参数
    std::string meshName;                         ///< 网格资源名称（用于网格碰撞体）
    bool useConvexHull = true;                    ///< 是否使用凸包（否则使用三角网格）
    
    // Plane 参数
    Vector3 planeNormal{0, 1, 0};                  ///< 平面法线
    float planeConstant = 0.0f;                    ///< 平面常数
    
    // ==================== 偏移和旋转 ====================
    Vector3 offset{0, 0, 0};                       ///< 相对于 Transform 的偏移
    Quaternion rotation{1, 0, 0, 0};              ///< 相对于 Transform 的旋转
    
    // ==================== 触发器 ====================
    bool isTrigger = false;                       ///< 是否为触发器（不产生物理响应）
    
    // ==================== 碰撞过滤 ====================
    uint16_t collisionGroup = 0x0001;             ///< 碰撞组（位掩码）
    uint16_t collisionMask = 0xFFFF;              ///< 碰撞遮罩（与哪些组碰撞）
    
    // ==================== 内部状态 ====================
    void* bulletCollisionShape = nullptr;         ///< Bullet 碰撞形状指针（内部使用）
    bool needsUpdate = true;                      ///< 是否需要更新（内部使用）
    
    ColliderComponent() = default;
    
    // ==================== 便捷方法 ====================
    
    /**
     * @brief 设置盒子碰撞体
     */
    void SetBox(const Vector3& size);
    
    /**
     * @brief 设置球体碰撞体
     */
    void SetSphere(float radius);
    
    /**
     * @brief 设置胶囊体碰撞体
     */
    void SetCapsule(float radius, float height);
    
    /**
     * @brief 设置网格碰撞体
     */
    void SetMesh(const std::string& meshName, bool convexHull = true);
};

} // namespace ECS
} // namespace Render
```

### 3. ConstraintComponent（约束组件）

```cpp
namespace Render {
namespace ECS {

/**
 * @brief 约束类型
 */
enum class ConstraintType {
    PointToPoint,     ///< 点对点约束
    Hinge,            ///< 铰链约束
    Slider,           ///< 滑动约束
    ConeTwist,        ///< 圆锥扭转约束
    Generic6Dof,      ///< 6自由度约束
    Generic6DofSpring ///< 6自由度弹簧约束
};

/**
 * @brief 约束组件
 * 
 * 连接两个刚体的约束
 */
struct ConstraintComponent {
    ConstraintType type = ConstraintType::PointToPoint;
    
    EntityID connectedEntity = EntityID::Invalid();  ///< 连接的实体ID
    
    // 约束点（本地空间）
    Vector3 pivotA{0, 0, 0};                          ///< 实体A的约束点
    Vector3 pivotB{0, 0, 0};                          ///< 实体B的约束点
    
    // 约束轴（铰链、滑动等）
    Vector3 axisA{1, 0, 0};                           ///< 实体A的轴
    Vector3 axisB{1, 0, 0};                           ///< 实体B的轴
    
    // 限制（角度/距离）
    float lowerLimit = 0.0f;                          ///< 下限
    float upperLimit = 0.0f;                          ///< 上限
    
    // 弹簧参数（Generic6DofSpring）
    bool enableSpring = false;                        ///< 是否启用弹簧
    float springStiffness = 0.0f;                    ///< 弹簧刚度
    float springDamping = 0.0f;                       ///< 弹簧阻尼
    
    bool enabled = true;                              ///< 是否启用
    
    void* bulletConstraint = nullptr;                 ///< Bullet 约束指针（内部使用）
    
    ConstraintComponent() = default;
};

} // namespace ECS
} // namespace Render
```

### 4. PhysicsWorldComponent（物理世界组件）

```cpp
namespace Render {
namespace ECS {

/**
 * @brief 物理世界组件
 * 
 * 标记实体为物理世界根节点
 * 一个 World 中应该只有一个实体拥有此组件
 */
struct PhysicsWorldComponent {
    Vector3 gravity{0, -9.81f, 0};      ///< 重力加速度
    float timeStep = 1.0f / 60.0f;      ///< 固定时间步长（秒）
    int maxSubSteps = 10;               ///< 最大子步数
    
    bool enabled = true;                ///< 是否启用物理模拟
    
    // 内部状态
    void* bulletWorld = nullptr;        ///< Bullet 世界指针（内部使用）
    
    PhysicsWorldComponent() = default;
};

} // namespace ECS
} // namespace Render
```

---

## 系统设计

### PhysicsSystem（物理系统）

```cpp
namespace Render {
namespace ECS {

/**
 * @brief 物理系统
 * 
 * 管理物理模拟，同步 Transform 和物理体状态
 * 优先级：15（在 TransformSystem 之后）
 */
class PhysicsSystem : public System {
public:
    PhysicsSystem();
    ~PhysicsSystem();
    
    void OnCreate(World* world) override;
    void OnDestroy() override;
    void Update(float deltaTime) override;
    [[nodiscard]] int GetPriority() const override { return 15; }
    
    // ==================== 物理世界管理 ====================
    
    /**
     * @brief 获取物理世界组件对应的实体
     * @return 物理世界实体ID，如果不存在返回 Invalid
     */
    [[nodiscard]] EntityID GetPhysicsWorldEntity() const;
    
    /**
     * @brief 设置物理世界实体
     * @param entity 物理世界实体ID
     * @return 成功返回 true
     */
    bool SetPhysicsWorldEntity(EntityID entity);
    
    /**
     * @brief 创建物理世界实体
     * @return 新创建的物理世界实体ID
     */
    EntityID CreatePhysicsWorld();
    
    // ==================== 物理模拟控制 ====================
    
    /**
     * @brief 启用/禁用物理模拟
     * @param enabled 是否启用
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    
    /**
     * @brief 获取是否启用
     */
    [[nodiscard]] bool IsEnabled() const { return m_enabled; }
    
    /**
     * @brief 设置重力
     * @param gravity 重力向量
     */
    void SetGravity(const Vector3& gravity);
    
    /**
     * @brief 获取重力
     */
    [[nodiscard]] Vector3 GetGravity() const;
    
    // ==================== 查询接口 ====================
    
    /**
     * @brief 射线检测
     * @param start 起点
     * @param end 终点
     * @return 碰撞结果列表
     */
    struct RaycastHit {
        EntityID entity;                ///< 碰撞的实体
        Vector3 point;                  ///< 碰撞点
        Vector3 normal;                 ///< 碰撞法线
        float distance;                 ///< 距离
    };
    
    std::vector<RaycastHit> Raycast(const Vector3& start, const Vector3& end) const;
    
    /**
     * @brief 球形检测
     * @param center 中心点
     * @param radius 半径
     * @return 碰撞的实体列表
     */
    std::vector<EntityID> SphereCast(const Vector3& center, float radius) const;
    
    // ==================== 统计信息 ====================
    
    struct PhysicsStats {
        size_t rigidBodyCount = 0;     ///< 刚体数量
        size_t colliderCount = 0;       ///< 碰撞体数量
        size_t constraintCount = 0;      ///< 约束数量
        float simulationTime = 0.0f;    ///< 模拟时间（ms）
        int stepCount = 0;               ///< 步数
    };
    
    [[nodiscard]] const PhysicsStats& GetStats() const { return m_stats; }
    
private:
    // ==================== 初始化/清理 ====================
    void InitializePhysicsWorld(EntityID entity);
    void ShutdownPhysicsWorld();
    
    // ==================== 组件同步 ====================
    void SyncTransformToPhysics(EntityID entity, RigidBodyComponent& rb, TransformComponent& transform);
    void SyncPhysicsToTransform(EntityID entity, RigidBodyComponent& rb, TransformComponent& transform);
    void UpdateCollider(EntityID entity, ColliderComponent& collider, TransformComponent& transform);
    void UpdateConstraints();
    
    // ==================== 批量处理 ====================
    void BatchSyncTransformsToPhysics();
    void BatchSyncPhysicsToTransforms();
    
    // ==================== 碰撞回调 ====================
    void OnCollisionEnter(EntityID entityA, EntityID entityB, const Vector3& point, const Vector3& normal);
    void OnCollisionExit(EntityID entityA, EntityID entityB);
    void OnTriggerEnter(EntityID entityA, EntityID entityB);
    void OnTriggerExit(EntityID entityA, EntityID entityB);
    
    // ==================== 成员变量 ====================
    EntityID m_physicsWorldEntity = EntityID::Invalid();  ///< 物理世界实体ID
    bool m_enabled = true;                                 ///< 是否启用
    PhysicsStats m_stats;                                  ///< 统计信息
    
    // Bullet 对象（通过 void* 存储，避免暴露 Bullet 头文件）
    void* m_bulletWorld = nullptr;                         ///< btDiscreteDynamicsWorld
    void* m_broadphase = nullptr;                          ///< btBroadphaseInterface
    void* m_dispatcher = nullptr;                          ///< btCollisionDispatcher
    void* m_solver = nullptr;                              ///< btConstraintSolver
    void* m_collisionConfig = nullptr;                     ///< btDefaultCollisionConfiguration
};

} // namespace ECS
} // namespace Render
```

---

## 数据同步机制

### 同步流程

```
每帧更新流程：

1. TransformSystem.Update() [优先级 10]
   └─> 同步 Transform 层级关系
   └─> 更新 Transform 世界矩阵

2. PhysicsSystem.Update() [优先级 15]
   ├─> BatchSyncTransformsToPhysics()
   │   └─> 遍历所有 RigidBodyComponent
   │       └─> 如果 syncMode == TransformToPhysics
   │           └─> 将 Transform 位置/旋转同步到物理体
   │
   ├─> 执行物理模拟
   │   └─> bulletWorld->stepSimulation(deltaTime)
   │
   └─> BatchSyncPhysicsToTransforms()
       └─> 遍历所有 RigidBodyComponent
           └─> 如果 syncMode == PhysicsToTransform
               └─> 将物理体位置/旋转同步到 Transform

3. MeshRenderSystem.Update() [优先级 100]
   └─> 使用更新后的 Transform 进行渲染
```

### 同步模式

#### 1. PhysicsToTransform（物理驱动，默认）

- **用途**：动态刚体，受物理模拟影响
- **同步方向**：物理体 → Transform
- **时机**：物理模拟后
- **示例**：掉落物体、受力的物体

```cpp
auto& rb = world.AddComponent<RigidBodyComponent>(entity);
rb.type = RigidBodyType::Dynamic;
rb.syncMode = RigidBodyComponent::SyncMode::PhysicsToTransform;
rb.mass = 10.0f;
```

#### 2. TransformToPhysics（Transform 驱动）

- **用途**：运动学刚体、玩家控制的对象
- **同步方向**：Transform → 物理体
- **时机**：物理模拟前
- **示例**：玩家角色、移动平台

```cpp
auto& rb = world.AddComponent<RigidBodyComponent>(entity);
rb.type = RigidBodyType::Kinematic;
rb.syncMode = RigidBodyComponent::SyncMode::TransformToPhysics;
rb.isKinematic = true;
```

#### 3. Manual（手动控制）

- **用途**：需要精确控制的场景
- **同步方向**：无自动同步
- **时机**：手动调用同步方法
- **示例**：特殊物理效果、调试

```cpp
auto& rb = world.AddComponent<RigidBodyComponent>(entity);
rb.syncMode = RigidBodyComponent::SyncMode::Manual;
// 手动同步
physicsSystem.SyncTransformToPhysics(entity);
```

---

## 实现计划

### 阶段 1：基础集成（核心功能）

**目标**：实现基本的物理模拟功能

1. **创建物理组件**
   - [x] `RigidBodyComponent`
   - [x] `ColliderComponent`（Box、Sphere）
   - [x] `PhysicsWorldComponent`

2. **实现 PhysicsSystem**
   - [x] 初始化 Bullet 物理世界
   - [x] 创建/销毁物理体
   - [x] 基本的 Transform 同步
   - [x] 物理模拟更新

3. **CMake 集成**
   - [x] 更新 CMakeLists.txt，链接 Bullet3 库
   - [x] 添加物理引擎头文件路径

4. **基础测试**
   - [x] 创建测试示例（掉落物体）
   - [x] 验证物理模拟正常工作

**预计时间**：2-3 天

### 阶段 2：完整功能（扩展功能）

**目标**：实现完整的物理特性

1. **扩展 ColliderComponent**
   - [x] 支持 Capsule、Cylinder、Cone
   - [x] 支持 Mesh 碰撞体
   - [x] 支持 Plane

2. **实现 ConstraintComponent**
   - [x] PointToPoint 约束
   - [x] Hinge 约束
   - [x] Generic6Dof 约束

3. **碰撞检测和查询**
   - [x] 射线检测（Raycast）
   - [x] 球形检测（SphereCast）
   - [x] 碰撞回调

4. **触发器支持**
   - [x] Trigger 检测
   - [x] Trigger 回调

**预计时间**：3-4 天

### 阶段 3：优化和增强（性能优化）

**目标**：性能优化和功能增强

1. **性能优化**
   - [ ] 批量同步优化
   - [ ] 减少不必要的同步操作
   - [ ] 物理体激活/休眠管理

2. **功能增强**
   - [ ] 物理材质系统
   - [ ] 碰撞过滤（Group/Mask）
   - [ ] 物理调试可视化

3. **序列化支持**
   - [ ] 物理组件序列化
   - [ ] 场景保存/加载物理状态

**预计时间**：2-3 天

### 阶段 4：文档和示例（完善）

**目标**：完善文档和示例

1. **API 文档**
   - [ ] 物理组件 API 文档
   - [ ] PhysicsSystem API 文档
   - [ ] 使用指南

2. **示例程序**
   - [ ] 基础物理示例
   - [ ] 约束示例
   - [ ] 触发器示例
   - [ ] 物理材质示例

**预计时间**：1-2 天

**总计预计时间**：8-12 天

---

## API设计

### 创建物理世界

```cpp
// 方式1：自动创建
auto physicsWorldEntity = physicsSystem.CreatePhysicsWorld();

// 方式2：手动创建
auto physicsWorldEntity = world.CreateEntity();
auto& physicsWorld = world.AddComponent<PhysicsWorldComponent>(physicsWorldEntity);
physicsWorld.gravity = Vector3(0, -9.81f, 0);
physicsSystem.SetPhysicsWorldEntity(physicsWorldEntity);
```

### 创建物理对象

```cpp
// 创建动态刚体
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

### 应用力

```cpp
auto& rb = world.GetComponent<RigidBodyComponent>(entity);
rb.ApplyForce(Vector3(0, 100, 0));  // 向上施加力
rb.ApplyImpulse(Vector3(10, 0, 0)); // 向右施加冲量
```

### 射线检测

```cpp
auto hits = physicsSystem.Raycast(
    Vector3(0, 10, 0),  // 起点
    Vector3(0, -10, 0)  // 终点
);

for (const auto& hit : hits) {
    std::cout << "Hit entity: " << hit.entity.index << std::endl;
    std::cout << "Hit point: " << hit.point << std::endl;
}
```

### 约束

```cpp
// 创建铰链约束
auto constraintEntity = world.CreateEntity();
auto& constraint = world.AddComponent<ConstraintComponent>(constraintEntity);
constraint.type = ConstraintType::Hinge;
constraint.connectedEntity = entityA;  // 连接到实体A
constraint.pivotA = Vector3(0, 0, 0);
constraint.axisA = Vector3(0, 1, 0);
constraint.lowerLimit = -45.0f;  // 角度限制
constraint.upperLimit = 45.0f;
```

---

## 使用示例

### 示例 1：掉落物体

```cpp
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/ecs/systems.h"

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
    
    // 刚体
    auto& rigidBody = world.AddComponent<RigidBodyComponent>(entity);
    rigidBody.type = RigidBodyType::Dynamic;
    rigidBody.mass = 10.0f;
    rigidBody.friction = 0.5f;
    rigidBody.restitution = 0.3f;  // 有弹性
}
```

### 示例 2：地面

```cpp
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
```

### 示例 3：触发器

```cpp
void CreateTriggerZone(World& world) {
    auto trigger = world.CreateEntity();
    
    // Transform
    auto& transform = world.AddComponent<TransformComponent>(trigger);
    transform.SetPosition(Vector3(0, 5, 0));
    
    // 触发器碰撞体
    auto& collider = world.AddComponent<ColliderComponent>(trigger);
    collider.SetSphere(2.0f);
    collider.isTrigger = true;  // 设置为触发器
    
    // 不需要 RigidBodyComponent（触发器不需要物理响应）
}
```

---

## 性能优化

### 1. 批量同步优化

- **问题**：逐个同步 Transform 和物理体效率低
- **方案**：批量收集需要同步的实体，一次性处理
- **实现**：
  ```cpp
  void PhysicsSystem::BatchSyncTransformsToPhysics() {
      std::vector<EntityID> entitiesToSync;
      
      // 收集需要同步的实体
      world->ForEachComponent<RigidBodyComponent>([&](EntityID e, RigidBodyComponent& rb) {
          if (rb.syncMode == RigidBodyComponent::SyncMode::TransformToPhysics) {
              entitiesToSync.push_back(e);
          }
      });
      
      // 批量同步
      for (auto entity : entitiesToSync) {
          SyncTransformToPhysics(entity, ...);
      }
  }
  ```

### 2. 减少不必要的同步

- **问题**：每帧都同步所有物理体，即使没有变化
- **方案**：使用 `needsSync` 标志，只同步变化的物理体
- **实现**：
  ```cpp
  struct RigidBodyComponent {
      bool needsSync = true;  // 初始需要同步
      // ...
  };
  
  // 只在 Transform 变化时标记
  void TransformComponent::SetPosition(const Vector3& pos) {
      if (transform) {
          transform->SetPosition(pos);
          // 标记关联的 RigidBody 需要同步
          MarkRigidBodyDirty();
      }
  }
  ```

### 3. 物理体激活/休眠

- **问题**：静止的物理体仍在参与模拟，浪费性能
- **方案**：使用 Bullet 的激活/休眠机制
- **实现**：
  ```cpp
  // 自动休眠静止的物理体
  rigidBody->setActivationState(DISABLE_DEACTIVATION);  // 禁用自动休眠（需要时）
  rigidBody->setActivationState(ACTIVE_TAG);            // 激活
  rigidBody->setActivationState(ISLAND_SLEEPING);       // 休眠
  ```

### 4. 碰撞过滤优化

- **问题**：所有物体都参与碰撞检测，计算量大
- **方案**：使用碰撞组和遮罩，减少不必要的碰撞检测
- **实现**：
  ```cpp
  collider.collisionGroup = 0x0001;  // 组1
  collider.collisionMask = 0x0002;   // 只与组2碰撞
  ```

---

## 注意事项

### 1. 线程安全

- **问题**：Bullet3 不是线程安全的，但 ECS 系统需要线程安全
- **方案**：
  - PhysicsSystem 的所有操作都在主线程执行
  - 使用互斥锁保护 Bullet 世界操作
  - 避免在物理回调中修改 ECS 组件（使用队列延迟处理）

### 2. 单位系统

- **问题**：Bullet3 使用米-千克-秒（MKS）单位，需要与渲染单位一致
- **方案**：
  - 统一使用 MKS 单位系统
  - Transform 的位置单位是米
  - 质量单位是千克

### 3. 内存管理

- **问题**：Bullet 对象需要手动管理内存
- **方案**：
  - 在组件销毁时自动清理 Bullet 对象
  - 使用 RAII 原则管理 Bullet 对象生命周期
  - 避免内存泄漏

### 4. 父子关系

- **问题**：物理体的父子关系与 Transform 的父子关系可能不一致
- **方案**：
  - 物理体不直接支持父子关系
  - 使用约束（Constraint）连接父子物理体
  - 或者只同步根节点的物理体，子节点通过 Transform 层级关系跟随

### 5. 性能考虑

- **问题**：物理模拟可能影响帧率
- **方案**：
  - 使用固定时间步长（fixed timestep）
  - 限制每帧最大子步数
  - 对于不需要精确物理的对象，使用更简单的碰撞体（Box/Sphere）

### 6. 调试

- **问题**：物理状态难以可视化调试
- **方案**：
  - 实现物理调试渲染（绘制碰撞体线框）
  - 提供物理统计信息（PhysicsStats）
  - 支持暂停/恢复物理模拟

---

## 总结

本文档描述了将 Bullet3 物理引擎集成到 ECS 架构渲染引擎的完整设计方案。通过组件化设计、系统化管理和自动同步机制，实现了物理引擎与渲染引擎的无缝集成。

**关键特性**：
- ✅ 完整的 ECS 组件支持（RigidBody、Collider、Constraint）
- ✅ 自动 Transform 同步机制
- ✅ 灵活的同步模式（物理驱动/Transform驱动/手动）
- ✅ 线程安全设计
- ✅ 性能优化策略

**下一步**：
1. 按照实现计划逐步实现各个阶段
2. 创建测试示例验证功能
3. 完善 API 文档和使用指南

---

**文档版本**：1.0  
**最后更新**：2026-01-10  
**作者**：Linductor
