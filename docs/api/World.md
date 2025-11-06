# World API 参考

[返回 API 目录](README.md) | [返回 ECS 概览](ECS.md)

---

## 📋 概述

World 是 ECS 的顶层容器，管理所有实体、组件和系统。它提供统一的接口来创建实体、添加组件、注册系统，并负责每帧更新所有系统。

**命名空间**：`Render::ECS`

**头文件**：`<render/ecs/world.h>`

---

## 🌍 类定义

```cpp
class World : public std::enable_shared_from_this<World> {
public:
    World();
    ~World();
    
    // 初始化/清理
    void Initialize();
    void PostInitialize();
    void Shutdown();
    bool IsInitialized() const;
    
    // 实体管理
    EntityID CreateEntity(const EntityDescriptor& desc = {});
    void DestroyEntity(EntityID entity);
    bool IsValidEntity(EntityID entity) const;
    
    // 组件管理
    template<typename T>
    void RegisterComponent();
    
    template<typename T>
    void AddComponent(EntityID entity, const T& component);
    
    template<typename T>
    void AddComponent(EntityID entity, T&& component);
    
    template<typename T>
    void RemoveComponent(EntityID entity);
    
    template<typename T>
    T& GetComponent(EntityID entity);
    
    template<typename T>
    const T& GetComponent(EntityID entity) const;
    
    template<typename T>
    bool HasComponent(EntityID entity) const;
    
    // 系统管理
    template<typename T, typename... Args>
    T* RegisterSystem(Args&&... args);
    
    template<typename T>
    T* GetSystem();
    
    template<typename T>
    void RemoveSystem();
    
    // 查询
    template<typename... Components>
    std::vector<EntityID> Query() const;
    
    std::vector<EntityID> QueryByTag(const std::string& tag) const;
    
    // 更新
    void Update(float deltaTime);
    
    // 辅助接口
    EntityManager& GetEntityManager();
    ComponentRegistry& GetComponentRegistry();
    
    // 统计信息
    struct Statistics {
        size_t entityCount = 0;
        size_t activeEntityCount = 0;
        size_t systemCount = 0;
        float lastUpdateTime = 0.0f;  // 毫秒
    };
    
    const Statistics& GetStatistics() const;
    void PrintStatistics() const;
};
```

---

## 🔧 成员函数详解

### 初始化/清理

#### `Initialize()`

初始化 World。

```cpp
void Initialize();
```

**说明**：
- 必须在使用 World 之前调用
- 初始化内部数据结构

**示例**：
```cpp
auto world = std::make_shared<World>();
world->Initialize();
```

#### `PostInitialize()`

后初始化，在所有系统注册完成后调用。

```cpp
void PostInitialize();
```

**说明**：
- 允许系统在此阶段获取对其他系统的引用
- 避免在 `OnCreate()` 中访问其他系统导致的死锁

**示例**：
```cpp
// 注册所有系统
world->RegisterSystem<CameraSystem>();
world->RegisterSystem<MeshRenderSystem>(renderer);

// 后初始化（允许系统间相互引用）
world->PostInitialize();
```

#### `Shutdown()`

关闭 World。

```cpp
void Shutdown();
```

**说明**：
- 销毁所有实体
- 销毁所有系统
- 清理所有资源

**示例**：
```cpp
world->Shutdown();
```

#### `IsInitialized()`

检查是否已初始化。

```cpp
bool IsInitialized() const;
```

**返回值**：如果已初始化返回 `true`。

---

### 实体管理

#### `CreateEntity()`

创建实体。

```cpp
EntityID CreateEntity(const EntityDescriptor& desc = {});
```

**参数**：
- `desc` - 实体描述符（可选）

**返回值**：新创建的实体 ID。

**示例**：
```cpp
// 简单创建
EntityID entity = world->CreateEntity();

// 使用描述符
EntityID camera = world->CreateEntity({
    .name = "MainCamera",
    .active = true,
    .tags = {"camera", "main"}
});
```

#### `DestroyEntity()`

销毁实体。

```cpp
void DestroyEntity(EntityID entity);
```

**参数**：
- `entity` - 要销毁的实体 ID

**说明**：
- 会自动移除实体的所有组件
- 实体索引会被加入空闲队列

**示例**：
```cpp
world->DestroyEntity(entity);
```

#### `IsValidEntity()`

检查实体是否有效。

```cpp
bool IsValidEntity(EntityID entity) const;
```

**参数**：
- `entity` - 实体 ID

**返回值**：如果实体有效返回 `true`。

**示例**：
```cpp
if (world->IsValidEntity(entity)) {
    // 实体有效
}
```

---

### 组件管理

#### `RegisterComponent()`

注册组件类型。

```cpp
template<typename T>
void RegisterComponent();
```

**说明**：
- 必须在使用组件之前注册
- 重复注册同一类型是安全的（会被忽略）

**示例**：
```cpp
world->RegisterComponent<TransformComponent>();
world->RegisterComponent<MeshRenderComponent>();
world->RegisterComponent<CameraComponent>();
```

#### `AddComponent()`

添加组件到实体。

```cpp
template<typename T>
void AddComponent(EntityID entity, const T& component);

template<typename T>
void AddComponent(EntityID entity, T&& component);
```

**参数**：
- `entity` - 实体 ID
- `component` - 组件数据

**示例**：
```cpp
// 拷贝语义
TransformComponent transform;
transform.SetPosition(Vector3(0, 1, 0));
world->AddComponent(entity, transform);

// 移动语义
world->AddComponent(entity, TransformComponent());
```

#### `RemoveComponent()`

从实体移除组件。

```cpp
template<typename T>
void RemoveComponent(EntityID entity);
```

**示例**：
```cpp
world->RemoveComponent<MeshRenderComponent>(entity);
```

#### `GetComponent()`

获取实体的组件。

```cpp
template<typename T>
T& GetComponent(EntityID entity);

template<typename T>
const T& GetComponent(EntityID entity) const;
```

**参数**：
- `entity` - 实体 ID

**返回值**：组件引用。

**异常**：如果实体没有该组件，抛出 `std::out_of_range`。

**示例**：
```cpp
// 可修改
auto& transform = world->GetComponent<TransformComponent>(entity);
transform.SetPosition(Vector3(0, 2, 0));

// 只读
const auto& transform = world->GetComponent<TransformComponent>(entity);
Vector3 pos = transform.GetPosition();
```

#### `HasComponent()`

检查实体是否有指定组件。

```cpp
template<typename T>
bool HasComponent(EntityID entity) const;
```

**返回值**：如果有该组件返回 `true`。

**示例**：
```cpp
if (world->HasComponent<MeshRenderComponent>(entity)) {
    auto& mesh = world->GetComponent<MeshRenderComponent>(entity);
    // ...
}
```

---

### 系统管理

#### `RegisterSystem()`

注册系统。

```cpp
template<typename T, typename... Args>
T* RegisterSystem(Args&&... args);
```

**参数**：
- `args` - 系统构造函数参数

**返回值**：系统指针。

**说明**：
- 系统会自动按优先级排序
- 会调用系统的 `OnCreate()` 方法

**示例**：
```cpp
// 无参数系统
world->RegisterSystem<CameraSystem>();

// 有参数系统
world->RegisterSystem<MeshRenderSystem>(renderer);
world->RegisterSystem<ResourceLoadingSystem>(&asyncLoader);
```

#### `GetSystem()`

获取系统。

```cpp
template<typename T>
T* GetSystem();
```

**返回值**：系统指针，如果未找到返回 `nullptr`。

**示例**：
```cpp
auto* cameraSystem = world->GetSystem<CameraSystem>();
if (cameraSystem) {
    EntityID mainCamera = cameraSystem->GetMainCamera();
}
```

#### `RemoveSystem()`

移除系统。

```cpp
template<typename T>
void RemoveSystem();
```

**说明**：
- 会调用系统的 `OnDestroy()` 方法
- 系统会被销毁

**示例**：
```cpp
world->RemoveSystem<MySystem>();
```

---

### 查询

#### `Query()`

查询具有特定组件的实体。

```cpp
template<typename... Components>
std::vector<EntityID> Query() const;
```

**返回值**：具有所有指定组件的实体列表。

**示例**：
```cpp
// 单个组件
auto entities = world->Query<TransformComponent>();

// 多个组件
auto entities = world->Query<TransformComponent, MeshRenderComponent>();

// 三个组件
auto entities = world->Query<TransformComponent, VelocityComponent, ColliderComponent>();

// 遍历结果
for (auto entity : entities) {
    auto& transform = world->GetComponent<TransformComponent>(entity);
    auto& mesh = world->GetComponent<MeshRenderComponent>(entity);
    // ...
}
```

#### `QueryByTag()`

按标签查询实体。

```cpp
std::vector<EntityID> QueryByTag(const std::string& tag) const;
```

**参数**：
- `tag` - 标签名称

**返回值**：具有该标签的实体列表。

**示例**：
```cpp
auto enemies = world->QueryByTag("enemy");
auto players = world->QueryByTag("player");
```

---

### 更新

#### `Update()`

更新 World（调用所有系统的 `Update()`）。

```cpp
void Update(float deltaTime);
```

**参数**：
- `deltaTime` - 帧间隔时间（秒）

**说明**：
- 按优先级顺序调用所有启用的系统
- 更新统计信息

**示例**：
```cpp
float deltaTime = 0.016f;  // 60 FPS
world->Update(deltaTime);
```

---

### 辅助接口

#### `GetEntityManager()`

获取 EntityManager。

```cpp
EntityManager& GetEntityManager();
```

**返回值**：EntityManager 引用。

**示例**：
```cpp
auto& entityManager = world->GetEntityManager();
size_t count = entityManager.GetEntityCount();
```

#### `GetComponentRegistry()`

获取 ComponentRegistry。

```cpp
ComponentRegistry& GetComponentRegistry();
```

**返回值**：ComponentRegistry 引用。

**示例**：
```cpp
auto& registry = world->GetComponentRegistry();
auto* array = registry.GetComponentArray<TransformComponent>();
```

---

### 统计信息

#### `GetStatistics()`

获取统计信息。

```cpp
const Statistics& GetStatistics() const;
```

**返回值**：统计信息结构体。

**Statistics 结构体**：
```cpp
struct Statistics {
    size_t entityCount = 0;          // 实体总数
    size_t activeEntityCount = 0;    // 激活实体数量
    size_t systemCount = 0;          // 系统数量
    float lastUpdateTime = 0.0f;     // 上次更新耗时（毫秒）
};
```

**示例**：
```cpp
const auto& stats = world->GetStatistics();
std::cout << "Entities: " << stats.entityCount << std::endl;
std::cout << "Active: " << stats.activeEntityCount << std::endl;
std::cout << "Systems: " << stats.systemCount << std::endl;
std::cout << "Update time: " << stats.lastUpdateTime << " ms" << std::endl;
```

#### `PrintStatistics()`

打印统计信息到日志。

```cpp
void PrintStatistics() const;
```

**示例**：
```cpp
world->PrintStatistics();
// 输出：
// [World] Statistics:
//   Entities: 100 (Active: 95)
//   Systems: 5
//   Last Update: 2.5 ms
```

---

## 🎯 完整示例

### 创建完整场景

```cpp
#include <render/ecs/world.h>
#include <render/ecs/systems.h>
#include <render/ecs/components.h>

using namespace Render;
using namespace Render::ECS;

int main() {
    // 1. 创建渲染器
    auto renderer = std::make_unique<Renderer>();
    renderer->Initialize("ECS Demo", 1280, 720);
    
    // 2. 创建异步加载器
    auto& asyncLoader = AsyncResourceLoader::GetInstance();
    asyncLoader.Initialize(4);
    
    // 3. 创建 World
    auto world = std::make_shared<World>();
    world->Initialize();
    
    // 4. 注册组件
    world->RegisterComponent<TransformComponent>();
    world->RegisterComponent<MeshRenderComponent>();
    world->RegisterComponent<CameraComponent>();
    world->RegisterComponent<LightComponent>();
    
    // 5. 注册系统
    world->RegisterSystem<CameraSystem>();
    world->RegisterSystem<TransformSystem>();
    world->RegisterSystem<ResourceLoadingSystem>(&asyncLoader);
    world->RegisterSystem<LightSystem>(renderer.get());
    world->RegisterSystem<MeshRenderSystem>(renderer.get());
    
    // 6. 后初始化
    world->PostInitialize();
    
    // 7. 创建相机
    EntityID camera = world->CreateEntity({.name = "MainCamera"});
    
    TransformComponent cameraTransform;
    cameraTransform.SetPosition(Vector3(0, 2, 5));
    cameraTransform.LookAt(Vector3(0, 0, 0));
    world->AddComponent(camera, cameraTransform);
    
    CameraComponent cameraComp;
    cameraComp.camera = std::make_shared<Camera>();
    cameraComp.camera->SetPerspective(60.0f, 16.0f/9.0f, 0.1f, 1000.0f);
    world->AddComponent(camera, cameraComp);
    
    // 8. 创建光源
    EntityID light = world->CreateEntity({.name = "Sun"});
    
    TransformComponent lightTransform;
    lightTransform.SetRotation(MathUtils::FromEulerDegrees(Vector3(30, 45, 0)));
    world->AddComponent(light, lightTransform);
    
    LightComponent lightComp;
    lightComp.type = LightType::Directional;
    lightComp.color = Color(1.0f, 1.0f, 0.9f);
    lightComp.intensity = 1.0f;
    world->AddComponent(light, lightComp);
    
    // 9. 创建多个立方体
    for (int i = 0; i < 10; i++) {
        EntityID cube = world->CreateEntity({.name = "Cube_" + std::to_string(i)});
        
        TransformComponent transform;
        float angle = (float)i * (360.0f / 10.0f);
        float radius = 5.0f;
        float x = radius * std::cos(angle * 3.14159f / 180.0f);
        float z = radius * std::sin(angle * 3.14159f / 180.0f);
        transform.SetPosition(Vector3(x, 0, z));
        world->AddComponent(cube, transform);
        
        MeshRenderComponent mesh;
        mesh.meshName = "models/cube.obj";  // 异步加载
        mesh.materialName = "default";
        world->AddComponent(cube, mesh);
    }
    
    // 10. 主循环
    bool running = true;
    while (running) {
        // 事件处理
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }
        
        float deltaTime = 0.016f;
        
        // 更新 World
        world->Update(deltaTime);
        
        // 渲染
        renderer->BeginFrame();
        renderer->Clear();
        renderer->FlushRenderQueue();
        renderer->EndFrame();
        renderer->Present();
    }
    
    // 11. 清理
    world->PrintStatistics();
    world->Shutdown();
    renderer->Shutdown();
    asyncLoader.Shutdown();
    
    return 0;
}
```

---

## 💡 使用建议

### 1. 使用 shared_ptr 管理 World

```cpp
// ✅ 好：使用 shared_ptr
auto world = std::make_shared<World>();

// ❌ 差：栈上分配
World world;  // 不支持异步回调的生命周期管理
```

### 2. 先注册组件，再注册系统

```cpp
// ✅ 好：先注册组件
world->RegisterComponent<TransformComponent>();
world->RegisterSystem<TransformSystem>();

// ❌ 差：顺序错误
world->RegisterSystem<TransformSystem>();  // 系统可能需要访问组件！
world->RegisterComponent<TransformComponent>();
```

### 3. 后初始化

```cpp
// ✅ 好：注册完所有系统后调用 PostInitialize
world->RegisterSystem<CameraSystem>();
world->RegisterSystem<MeshRenderSystem>(renderer);
world->PostInitialize();  // 允许系统间相互引用

// ❌ 差：不调用 PostInitialize
// 某些系统可能无法获取其他系统的引用
```

### 4. 批量查询

```cpp
// ✅ 好：一次查询，多次使用
auto entities = world->Query<TransformComponent, VelocityComponent>();
for (auto entity : entities) {
    // ...
}

// ❌ 差：每次都查询
for (int i = 0; i < 100; i++) {
    auto entities = world->Query<TransformComponent>();  // 重复查询！
}
```

---

## 🔒 线程安全

World 使用 `std::shared_mutex` 保护内部数据结构：

- `CreateEntity()`, `DestroyEntity()` - 独占锁
- `Query()`, `IsValidEntity()` - 共享锁
- `Update()` - 顺序执行，无并发

**注意**：虽然 World 本身是线程安全的，但不建议在多线程中同时修改实体和组件。

---

## 📊 性能指标

| 操作 | 复杂度 | 说明 |
|------|--------|------|
| `CreateEntity()` | O(1) | 索引复用 |
| `DestroyEntity()` | O(k) | k = 组件类型数量 |
| `AddComponent()` | O(1) | 哈希表插入 |
| `GetComponent()` | O(1) | 哈希表查找 |
| `Query<T>()` | O(n) | n = 实体数量 |
| `Update()` | O(s) | s = 系统数量 |

---

## 📖 相关文档

- [ECS 概览](ECS.md)
- [Entity API](Entity.md)
- [Component API](Component.md)
- [System API](System.md)

---

[返回 API 目录](README.md) | [返回 ECS 概览](ECS.md)

