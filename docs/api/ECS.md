# ECS 系统 API 参考

[返回 API 目录](README.md)

---

## 📋 概述

ECS（Entity Component System）是一种现代化的游戏对象架构模式，它将对象拆分为：
- **Entity（实体）**：轻量级的 ID，用于关联组件
- **Component（组件）**：纯数据结构，不包含逻辑
- **System（系统）**：处理具有特定组件的实体的逻辑

本项目的 ECS 系统特点：
- ✅ 数据导向设计（DOD）- 组件存储紧凑，缓存友好
- ✅ 线程安全 - 所有操作都有适当的锁保护
- ✅ 🆕 **安全的迭代接口** - v1.1 新增 ForEachComponent 等安全接口
- ✅ 🆕 **性能优化** - v1.1 优化递归锁问题，提升 5-10% 性能
- ✅ 资源复用 - Transform、Camera 等对象使用 shared_ptr 复用
- ✅ 异步资源加载集成 - 与 AsyncResourceLoader 深度集成
- ✅ 灵活查询 - 支持多组件查询和标签查询
- ✅ 资源管理统一 - 通过 ResourceManager 统一管理所有资源
- ✅ 自动 Uniform 管理 - UniformSystem 自动设置全局 shader uniform
- ✅ 材质属性覆盖 - 支持每个实体独立覆盖材质属性
- ✅ 视锥体裁剪 - 自动剔除相机不可见的对象
- ✅ 透明物体排序 - 自动按深度排序透明对象
- ✅ 资源自动清理 - 定期清理未使用的资源防止内存泄漏

---

## 🏗️ 架构概览

```
┌─────────────────────────────────────────────────────────────┐
│                        Application Layer                     │
│                    (Game Logic / Scene)                      │
└─────────────────────────────────────────────────────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                          ECS Layer                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   Entity     │  │  Component   │  │   System     │      │
│  │   Manager    │◄─┤   Registry   │─►│   Manager    │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                     Renderable Layer                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │  Renderable  │  │    Mesh      │  │   Sprite     │      │
│  │     Base     │  │  Renderable  │  │  Renderable  │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      Rendering Backend                       │
│   (Renderer, RenderState, Mesh, Material, Shader...)        │
└─────────────────────────────────────────────────────────────┘
```

---

## 📚 核心 API 文档

### 主要模块

- [**Entity**](Entity.md) - 实体和实体管理器
- [**Component**](Component.md) - 组件和组件注册表
- [**System**](System.md) - 系统基类和常用系统
- [**World**](World.md) - ECS 世界容器
- [**Renderable**](Renderable.md) - 渲染对象基类
- [**MeshRenderable**](MeshRenderable.md) - 3D 网格渲染对象
- [**SpriteRenderable**](SpriteRenderable.md) - 2D 精灵渲染对象

---

## 🚀 快速开始

### 1. 创建 World

```cpp
#include <render/ecs/world.h>

using namespace Render::ECS;

// 创建 World（使用 shared_ptr 管理生命周期）
auto world = std::make_shared<World>();
world->Initialize();
```

### 2. 注册组件

```cpp
// 注册需要使用的组件类型
world->RegisterComponent<TransformComponent>();
world->RegisterComponent<MeshRenderComponent>();
world->RegisterComponent<CameraComponent>();
world->RegisterComponent<LightComponent>();
```

### 3. 注册系统

```cpp
// 注册系统（按优先级自动排序）
world->RegisterSystem<WindowSystem>(renderer.get());        // 窗口管理
world->RegisterSystem<CameraSystem>();                       // 相机管理
world->RegisterSystem<TransformSystem>();                    // 变换更新
world->RegisterSystem<GeometrySystem>();                     // 几何生成
world->RegisterSystem<ResourceLoadingSystem>(&asyncLoader); // 资源加载
world->RegisterSystem<LightSystem>(renderer.get());         // 光照管理
world->RegisterSystem<UniformSystem>(renderer.get());        // Uniform 管理
world->RegisterSystem<MeshRenderSystem>(renderer.get());     // 网格渲染
world->RegisterSystem<ResourceCleanupSystem>();              // 资源清理（可选）

// 后初始化（允许系统间相互引用）
world->PostInitialize();
```

### 4. 创建实体

```cpp
// 创建相机实体
EntityID camera = world->CreateEntity({
    .name = "MainCamera",
    .active = true,
    .tags = {"camera", "main"}
});

// 添加 Transform 组件
TransformComponent transform;
transform.SetPosition(Vector3(0, 2, 5));
world->AddComponent(camera, transform);

// 添加 Camera 组件
CameraComponent cameraComp;
cameraComp.camera = std::make_shared<Camera>();
cameraComp.camera->SetPerspective(45.0f, 16.0f/9.0f, 0.1f, 1000.0f);
world->AddComponent(camera, cameraComp);
```

### 5. 更新和渲染

```cpp
// 主循环
while (running) {
    float deltaTime = 0.016f; // 60 FPS
    
    // 更新 World（自动调用所有系统）
    world->Update(deltaTime);
    
    // 渲染
    renderer->BeginFrame();
    renderer->Clear();
    renderer->FlushRenderQueue();
    renderer->EndFrame();
    renderer->Present();
}

// 清理
world->Shutdown();
```

---

## 🎯 常见用例

### 创建 3D 网格对象（异步加载）

```cpp
// 创建实体
EntityID entity = world->CreateEntity({.name = "Cube"});

// 添加 Transform
TransformComponent transform;
transform.SetPosition(Vector3(0, 1, 0));
world->AddComponent(entity, transform);

// 添加 MeshRenderComponent（自动异步加载）
MeshRenderComponent mesh;
mesh.meshName = "models/cube.obj";      // 设置资源名称
mesh.materialName = "default";
mesh.visible = true;
// ResourceLoadingSystem 会自动加载资源
world->AddComponent(entity, mesh);
```

### 创建光源

```cpp
EntityID light = world->CreateEntity({.name = "DirectionalLight"});

TransformComponent lightTransform;
lightTransform.SetRotation(MathUtils::FromEulerDegrees(Vector3(30, 45, 0)));
world->AddComponent(light, lightTransform);

LightComponent lightComp;
lightComp.type = LightType::Directional;
lightComp.color = Color(1.0f, 1.0f, 0.9f);
lightComp.intensity = 1.0f;
world->AddComponent(light, lightComp);
```

### 查询实体

```cpp
// 查询具有特定组件的实体
auto entities = world->Query<TransformComponent, MeshRenderComponent>();

for (EntityID entity : entities) {
    auto& transform = world->GetComponent<TransformComponent>(entity);
    auto& mesh = world->GetComponent<MeshRenderComponent>(entity);
    
    // 处理实体...
}

// 按标签查询
auto enemies = world->QueryByTag("enemy");
```

### 🆕 使用安全迭代接口（v1.1 推荐）

```cpp
// ✅ 推荐：使用 ForEachComponent 安全遍历
auto& registry = world->GetComponentRegistry();

registry.ForEachComponent<TransformComponent>(
    [](EntityID entity, TransformComponent& transform) {
        // 在锁保护下安全访问组件
        transform.SetPosition(Vector3::Zero());
    }
);

// ✅ 推荐：获取实体列表（更安全）
auto entities = registry.GetEntitiesWithComponent<MeshRenderComponent>();
Logger::Info("Found " + std::to_string(entities.size()) + " meshes");

for (const auto& entity : entities) {
    // 注意：使用前应该检查实体有效性
    if (!world->IsValidEntity(entity)) continue;
    
    auto& mesh = registry.GetComponent<MeshRenderComponent>(entity);
    // 处理组件...
}

// ✅ 推荐：获取组件数量
size_t transformCount = registry.GetComponentCount<TransformComponent>();
size_t meshCount = registry.GetComponentCount<MeshRenderComponent>();
Logger::Info("Transforms: " + std::to_string(transformCount) + 
             ", Meshes: " + std::to_string(meshCount));
```

**安全接口的优势**：
- ✅ 无需获取裸指针，更安全
- ✅ 在回调期间自动持有锁，线程安全
- ✅ 避免生命周期问题
- ✅ 代码更简洁

---

## ⚙️ 系统执行顺序

系统按优先级从小到大执行：

| 优先级 | 系统 | 职责 |
|--------|------|------|
| 3 | `WindowSystem` | 窗口大小变化处理、视口更新 |
| 5 | `CameraSystem` | 更新相机矩阵、主相机自动选择和验证（v1.1增强） |
| 10 | `TransformSystem` | 父子关系同步、批量更新变换层级（v1.2重构 - 方案B） |
| 15 | `GeometrySystem` | 生成基本几何形状网格 |
| 20 | `ResourceLoadingSystem` | 异步资源加载、多纹理支持 |
| 50 | `LightSystem` | 光照数据更新 |
| 90 | `UniformSystem` | 自动管理全局 shader uniform（相机、光照、时间） |
| 100 | `MeshRenderSystem` | 提交 3D 网格渲染（支持材质覆盖、视锥剔除、透明排序） |
| 200 | `SpriteRenderSystem` | 提交 2D 精灵渲染 |
| 1000 | `ResourceCleanupSystem` | 定期清理未使用的资源 |

---

## 🔧 性能优化建议

### 1. 使用对象池

```cpp
// ✅ 好：复用 Transform 对象
TransformComponent comp;
comp.transform = std::make_shared<Transform>();  // 创建一次
comp.SetPosition(pos);  // 修改已有对象

// ❌ 差：每次创建新对象
for (int i = 0; i < 1000; i++) {
    Transform temp;  // 栈上创建
    temp.SetPosition(pos);
    // 销毁
}
```

### 2. 批量查询

```cpp
// ✅ 好：一次查询
auto entities = world->Query<TransformComponent, MeshRenderComponent>();
for (auto entity : entities) {
    // 处理...
}

// ❌ 差：多次查询
for (int i = 0; i < 1000; i++) {
    auto entities = world->Query<TransformComponent>();  // 重复查询
}
```

### 3. 异步资源加载

```cpp
// ✅ 好：使用异步加载
MeshRenderComponent mesh;
mesh.meshName = "large_model.fbx";  // 自动后台加载
world->AddComponent(entity, mesh);

// ❌ 差：同步加载会阻塞主线程
auto mesh = MeshLoader::LoadFromFile("large_model.fbx");  // 阻塞！
```

### 4. Transform 父子关系管理（v1.2新增）

**推荐做法**（方案B - 使用实体ID）：

```cpp
// ✅ 推荐：使用实体ID管理父子关系
EntityID parent = world->CreateEntity();
EntityID child = world->CreateEntity();

world->AddComponent(parent, TransformComponent{});
world->AddComponent(child, TransformComponent{});

auto& childComp = world->GetComponent<TransformComponent>(child);
if (!childComp.SetParentEntity(&world, parent)) {
    Logger::Error("Failed to set parent (circular reference or invalid)");
}

// TransformSystem 自动同步和验证
world->Update(0.016f);
```

**不推荐做法**（直接操作Transform指针）：

```cpp
// ❌ 不推荐：直接设置Transform指针（生命周期不明确）
auto& childComp = world->GetComponent<TransformComponent>(child);
auto& parentComp = world->GetComponent<TransformComponent>(parent);
childComp.transform->SetParent(parentComp.transform.get());  // 风险！
```

**安全特性**：
- ✅ 自动检测父实体销毁并清除关系
- ✅ 循环引用检测和拒绝
- ✅ 层级深度限制（1000层）
- ✅ 批量更新优化（3-5倍性能提升）

---

## 📷 相机系统改进（v1.1）

### 新增功能

**CameraComponent 增强**：
- ✅ 显式初始化 `camera` 为 nullptr，避免未初始化问题
- ✅ 新增 `IsValid()` 方法 - 快速检查相机是否可用
- ✅ 新增 `Validate()` 方法 - 严格验证组件状态
- ✅ 新增 `DebugString()` 方法 - 输出调试信息

**CameraSystem 增强**：
- ✅ 自动验证主相机有效性，失效时自动切换
- ✅ 按 `depth` 值自动选择主相机（depth 越小优先级越高）
- ✅ 新增 `GetMainCameraSharedPtr()` - 返回安全的智能指针
- ✅ 新增 `SetMainCamera()` - 手动设置主相机
- ✅ 新增 `ClearMainCamera()` - 清除主相机
- ✅ 新增 `SelectMainCameraByDepth()` - 立即按 depth 选择主相机

### 主相机管理

**自动选择规则**：
```cpp
// 1. 首次选择：选择所有激活相机中 depth 最小的
// 2. 每帧验证：自动检查主相机是否仍然有效
// 3. 自动切换：主相机被禁用/删除时，自动选择下一个有效相机
// 4. 深度优先：depth 值越小，优先级越高

// 示例：创建多个相机
EntityID camera1 = world->CreateEntity({.name = "MainCamera"});
CameraComponent cam1;
cam1.camera = std::make_shared<Camera>();
cam1.depth = 0;  // 最高优先级
cam1.active = true;
world->AddComponent(camera1, cam1);

EntityID camera2 = world->CreateEntity({.name = "SecondaryCamera"});
CameraComponent cam2;
cam2.camera = std::make_shared<Camera>();
cam2.depth = 10;  // 较低优先级
cam2.active = true;
world->AddComponent(camera2, cam2);

// CameraSystem 会自动选择 camera1（depth=0）作为主相机
// 如果 camera1 被禁用，会自动切换到 camera2
```

### 使用推荐

```cpp
// ✅ 推荐：使用智能指针（更安全）
auto camera = cameraSystem->GetMainCameraSharedPtr();
if (camera) {
    Matrix4 viewMatrix = camera->GetViewMatrix();
}

// ⚠️ 不推荐：使用裸指针（可能失效）
Camera* camera = cameraSystem->GetMainCameraObject();  // 已废弃

// ✅ 推荐：验证相机组件
CameraComponent& cameraComp = world->GetComponent<CameraComponent>(entity);
if (cameraComp.IsValid()) {
    // 安全使用
}

// ✅ 推荐：使用调试信息
Logger::Debug(cameraComp.DebugString());
```

### 迁移指南

**从旧版本迁移**：

```cpp
// 旧代码（仍然可用，但不推荐）
Camera* camera = cameraSystem->GetMainCameraObject();
if (camera) {
    Matrix4 view = camera->GetViewMatrix();
}

// 新代码（推荐）
auto camera = cameraSystem->GetMainCameraSharedPtr();
if (camera) {
    Matrix4 view = camera->GetViewMatrix();
}
```

**向后兼容性**：
- ✅ 所有旧接口保留，不会破坏现有代码
- ⚠️ `GetMainCameraObject()` 标记为 deprecated，建议迁移
- ✅ 新方法是可选的，渐进式升级

---

## 📊 性能指标

| 指标 | 目标 | 备注 |
|------|------|------|
| 实体数量 | 10,000+ | 不包含组件 |
| 带组件实体 | 5,000+ | 每个实体 3-5 个组件 |
| 查询速度 | < 1ms | 查询 10,000 实体 |
| 系统更新 | < 5ms | 5 个活跃系统 |
| 内存占用 | < 100MB | 10,000 实体 + 组件 |

---

## 🛡️ 线程安全

所有 ECS 组件都是线程安全的：

- **EntityManager**：使用 `std::shared_mutex` 保护实体数据
- **ComponentRegistry**：每个 `ComponentArray` 独立锁
- **World**：锁保护系统列表和查询操作
- **AsyncResourceLoader**：工作线程与主线程分离

---

## 🆕 版本更新历史

### v1.1 (2025-11-06) - 安全性和性能改进

#### EntityManager 优化
- ✅ **修复递归锁问题**：添加 `IsValidNoLock()` 内部方法
- ✅ **性能提升 5-10%**：减少不必要的锁获取操作
- ✅ **13个方法已优化**：所有内部调用使用无锁版本
- ✅ **完全向后兼容**：公共 API 保持不变

**影响的方法**：
`SetName`, `GetName`, `SetActive`, `IsActive`, `AddTag`, `RemoveTag`, `HasTag`, `GetTags`, `GetAllEntities`, `GetEntitiesWithTag`, `GetActiveEntities`, `GetEntityCount`, `GetActiveEntityCount`

#### ComponentRegistry 安全接口
- ✅ **新增 ForEachComponent**：安全的组件遍历接口
- ✅ **新增 GetEntitiesWithComponent**：直接获取实体列表
- ✅ **新增 GetComponentCount**：直接获取组件数量
- ✅ **GetComponentArray 标记为废弃**：仍可用但不推荐

**迁移建议**：
```cpp
// 旧代码（会产生编译警告）
auto* array = registry.GetComponentArray<TransformComponent>();
array->ForEach([](EntityID e, TransformComponent& t) { /*...*/ });

// 新代码（推荐）
registry.ForEachComponent<TransformComponent>(
    [](EntityID e, TransformComponent& t) { /*...*/ }
);
```

#### 文档更新
- ✅ 更新所有 API 文档以反映新接口
- ✅ 添加迁移指南和示例
- ✅ 创建安全性改进报告

---

## 📖 相关文档

### API 参考
- [Entity API](Entity.md) - 实体详细 API（v1.1 已更新）
- [Component API](Component.md) - 组件详细 API（v1.1 已更新）
- [System API](System.md) - 系统详细 API
- [World API](World.md) - World 详细 API
- [AsyncResourceLoader API](AsyncResourceLoader.md) - 异步资源加载

### 分析报告
- [ECS 安全性分析](../ECS_SECURITY_ANALYSIS.md) - 全面的安全性审查报告
- [ECS 安全性改进](../ECS_SAFETY_IMPROVEMENTS.md) - v1.1 改进详细说明

---

## 🎓 示例代码

完整示例代码请参考：
- `examples/33_ecs_async_test.cpp` - ECS + 异步加载集成测试
- `docs/todolists/PHASE2_ECS_AND_RENDERABLE.md` - ECS 开发文档

---

[返回 API 目录](README.md)

