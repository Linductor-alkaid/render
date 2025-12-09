# CCD 连续碰撞检测使用指南

## 概述

CCD（Continuous Collision Detection，连续碰撞检测）用于防止高速物体穿透其他物体。与传统的离散碰撞检测（DCD）不同，CCD 会计算物体在整个时间步长内的运动轨迹，找到精确的碰撞时间点。

## 代码变化

**好消息：使用 CCD 的代码与之前几乎完全相同！** 物理系统会自动处理 CCD 和 DCD 的切换。

### 之前（只有 DCD）的代码：

```cpp
// 创建物理系统
PhysicsUpdateSystem physicsSystem;
physicsSystem.OnCreate(&world);

// 更新物理（每帧调用）
physicsSystem.Update(deltaTime);
```

### 现在（支持 CCD）的代码：

```cpp
// 创建物理系统
PhysicsUpdateSystem physicsSystem;
physicsSystem.OnCreate(&world);

// 配置 CCD（可选，默认关闭）
PhysicsConfig config;
config.enableCCD = true;  // 启用 CCD
config.ccdVelocityThreshold = 10.0f;  // 速度阈值 10 m/s
physicsSystem.SetConfig(config);

// 更新物理（每帧调用，代码不变！）
physicsSystem.Update(deltaTime);
```

**关键点：** `Update()` 方法的调用方式完全不变，系统会根据配置自动选择使用 CCD 或 DCD。

## 启用 CCD

### 方法 1：全局启用 CCD（推荐）

通过 `PhysicsConfig` 全局启用 CCD：

```cpp
#include "render/physics/physics_config.h"
#include "render/physics/physics_systems.h"

// 创建物理系统
PhysicsUpdateSystem physicsSystem;
physicsSystem.OnCreate(&world);

// 配置 CCD
PhysicsConfig config;
config.enableCCD = true;                    // 启用 CCD
config.ccdVelocityThreshold = 10.0f;       // 速度阈值（m/s）
config.ccdDisplacementThreshold = 0.5f;    // 位移阈值（相对尺寸比例）
config.maxCCDObjects = 50;                  // 每帧最大 CCD 对象数
config.enableBroadPhaseCCD = true;          // 启用 Broad Phase 加速

// 应用配置
physicsSystem.SetConfig(config);
```

### 方法 2：单个物体强制启用 CCD

对特定物体强制启用 CCD（无论速度如何）：

```cpp
// 创建高速物体（如子弹）
EntityID bullet = world.CreateEntity();

// 设置刚体组件
RigidBodyComponent body;
body.type = RigidBodyComponent::BodyType::Dynamic;
body.linearVelocity = Vector3(100.0f, 0.0f, 0.0f);  // 高速
body.useCCD = true;  // 强制启用 CCD（重要！）
world.AddComponent(bullet, body);

// 设置碰撞体
ColliderComponent collider = ColliderComponent::CreateSphere(0.1f);
world.AddComponent(bullet, collider);
```

### 方法 3：自动检测（默认行为）

如果 `enableCCD = true`，系统会自动检测以下情况并启用 CCD：

1. **高速物体**：速度超过 `ccdVelocityThreshold`（默认 10 m/s）
2. **大位移物体**：位移超过形状尺寸的 `ccdDisplacementThreshold` 倍（默认 0.5）
3. **薄片物体**：地面、墙壁等容易穿透的物体（自动检测）
4. **强制启用**：`body.useCCD = true` 的物体

## 配置参数说明

### PhysicsConfig 中的 CCD 配置

```cpp
struct PhysicsConfig {
    // 是否启用 CCD（默认 false）
    bool enableCCD = false;
    
    // 速度阈值（m/s）
    // 物体速度超过此值时自动启用 CCD
    float ccdVelocityThreshold = 10.0f;
    
    // 位移阈值（相对尺寸比例）
    // 位移 = 速度 * dt，如果位移 > 形状尺寸 * 此值，启用 CCD
    float ccdDisplacementThreshold = 0.5f;
    
    // 最大 CCD 对象数（性能限制）
    // 每帧最多对多少个物体进行 CCD 检测
    int maxCCDObjects = 50;
    
    // 最大 CCD 子步数（防止性能爆炸）
    int maxCCDSubSteps = 5;
    
    // 是否启用 Broad Phase CCD（推荐开启）
    // 使用 AABB 扫描加速 CCD 检测
    bool enableBroadPhaseCCD = true;
};
```

### RigidBodyComponent 中的 CCD 配置

```cpp
struct RigidBodyComponent {
    // 是否强制启用 CCD
    // true: 无论速度如何，都使用 CCD
    // false: 根据速度阈值自动判断
    bool useCCD = false;
    
    // CCD 速度阈值（m/s）
    // 当 useCCD=false 时，速度超过此值自动启用 CCD
    float ccdVelocityThreshold = 10.0f;
    
    // CCD 位移阈值（相对尺寸比例）
    float ccdDisplacementThreshold = 0.5f;
    
    // CCD 碰撞信息（只读）
    struct CCDCollisionInfo {
        bool occurred = false;           // 是否发生 CCD 碰撞
        float toi = 0.0f;               // 碰撞时间 [0, 1]
        Vector3 collisionPoint;          // 碰撞点
        Vector3 collisionNormal;         // 碰撞法线
        ECS::EntityID otherEntity;        // 碰撞的另一个实体
    } ccdCollision;
};
```

## 使用示例

### 示例 1：高速子弹场景

```cpp
// 创建物理系统并启用 CCD
PhysicsUpdateSystem physicsSystem;
physicsSystem.OnCreate(&world);

PhysicsConfig config;
config.enableCCD = true;
config.ccdVelocityThreshold = 15.0f;  // 子弹速度通常 > 15 m/s
physicsSystem.SetConfig(config);

// 创建高速子弹
EntityID bullet = world.CreateEntity();
RigidBodyComponent body;
body.type = RigidBodyComponent::BodyType::Dynamic;
body.linearVelocity = Vector3(100.0f, 0.0f, 0.0f);  // 100 m/s
body.useCCD = true;  // 强制启用 CCD
world.AddComponent(bullet, body);

ColliderComponent collider = ColliderComponent::CreateSphere(0.05f);
world.AddComponent(bullet, collider);

// 创建薄墙（会自动启用 CCD）
EntityID wall = world.CreateEntity();
RigidBodyComponent wallBody;
wallBody.type = RigidBodyComponent::BodyType::Static;
world.AddComponent(wall, wallBody);

ColliderComponent wallCollider = ColliderComponent::CreateBox(
    Vector3(0.1f, 2.0f, 2.0f)  // 薄墙（0.1m 厚）
);
world.AddComponent(wall, wallCollider);
```

### 示例 2：检查 CCD 碰撞信息

```cpp
// 更新物理
physicsSystem.Update(deltaTime);

// 检查子弹是否发生 CCD 碰撞
auto& body = world.GetComponent<RigidBodyComponent>(bullet);
if (body.ccdCollision.occurred) {
    std::cout << "子弹在时间 " << body.ccdCollision.toi 
              << " 碰撞到实体 " << body.ccdCollision.otherEntity.index 
              << std::endl;
    std::cout << "碰撞点: " << body.ccdCollision.collisionPoint.transpose() 
              << std::endl;
    std::cout << "碰撞法线: " << body.ccdCollision.collisionNormal.transpose() 
              << std::endl;
}
```

### 示例 3：性能优化配置

```cpp
// 高性能配置（限制 CCD 对象数）
PhysicsConfig config;
config.enableCCD = true;
config.maxCCDObjects = 20;  // 限制为 20 个对象
config.enableBroadPhaseCCD = true;  // 启用 Broad Phase 加速
physicsSystem.SetConfig(config);
```

## 性能考虑

### CCD 的性能开销

- **CPU 开销**：CCD 比 DCD 计算量大，特别是复杂形状
- **内存开销**：需要缓存形状对象和扫描 AABB
- **推荐使用场景**：
  - 高速物体（子弹、抛射物）
  - 薄片物体（地面、墙壁）
  - 小物体（容易穿透）

### 性能优化建议

1. **使用 Broad Phase CCD**：启用 `enableBroadPhaseCCD` 可以大幅减少检测次数
2. **限制 CCD 对象数**：设置合理的 `maxCCDObjects`（默认 50）
3. **选择性启用**：只对需要 CCD 的物体设置 `useCCD = true`
4. **调整阈值**：根据场景调整 `ccdVelocityThreshold`

## 常见问题

### Q: CCD 会影响性能吗？

A: 是的，CCD 比 DCD 计算量大。但通过 Broad Phase 优化和选择性启用，性能影响可以控制在可接受范围内。

### Q: 什么时候应该使用 CCD？

A: 以下情况建议使用 CCD：
- 高速物体（速度 > 10 m/s）
- 薄片物体（地面、墙壁）
- 小物体（容易穿透）

### Q: 如何知道物体是否使用了 CCD？

A: 检查 `RigidBodyComponent::ccdCollision.occurred`，如果为 `true` 说明发生了 CCD 碰撞。

### Q: CCD 和 DCD 可以同时使用吗？

A: 可以。系统会自动选择：CCD 候选物体使用 CCD，其他物体使用 DCD。

## 总结

- **代码变化**：几乎无变化，只需配置 `enableCCD = true`
- **启用方式**：全局配置或单个物体设置 `useCCD = true`
- **性能**：通过 Broad Phase 和选择性启用可以控制性能开销
- **推荐**：对高速物体和薄片物体启用 CCD

