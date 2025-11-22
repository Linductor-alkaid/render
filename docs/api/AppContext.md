# AppContext API 文档

## 目录
[返回API文档首页](README.md)

---

## 概述

`AppContext` 是应用层的上下文结构，包含对核心服务的引用。它提供了统一的方式访问 Renderer、ResourceManager、UniformManager、AsyncResourceLoader、EventBus 和 ECS::World 等核心服务。

**命名空间**: `Render::Application`

---

## 结构定义

```cpp
struct AppContext {
    Renderer* renderer = nullptr;
    UniformManager* uniformManager = nullptr;
    ResourceManager* resourceManager = nullptr;
    AsyncResourceLoader* asyncLoader = nullptr;
    EventBus* globalEventBus = nullptr;
    ECS::World* world = nullptr;
    UI::UIInputRouter* uiInputRouter = nullptr;

    FrameUpdateArgs lastFrame{};

    bool IsValid() const noexcept;
    void ValidateOrThrow(const std::string& source) const;
};

struct FrameUpdateArgs {
    float deltaTime = 0.0f;
    double absoluteTime = 0.0;
    uint64_t frameIndex = 0;
};
```

---

## 核心功能

### 字段说明

#### `renderer`
Renderer实例指针，用于渲染操作。

#### `uniformManager`
UniformManager实例指针，用于Uniform管理（可选，可能为nullptr）。

#### `resourceManager`
ResourceManager实例指针，用于资源管理。

#### `asyncLoader`
AsyncResourceLoader实例指针，用于异步资源加载（可选，可能为nullptr）。

#### `globalEventBus`
EventBus实例指针，用于事件发布和订阅。

#### `world`
ECS World实例指针，用于ECS操作。

#### `uiInputRouter`
UI输入路由器指针，用于UI输入处理（可选，可能为nullptr）。

#### `lastFrame`
上一帧的更新参数（`FrameUpdateArgs`）。

---

### 方法说明

#### `IsValid() const noexcept`

检查应用上下文是否有效（核心服务指针是否为非空）。

**返回值**: 如果所有必需的指针非空返回 `true`

**必需字段**: `renderer`、`resourceManager`、`world`

**示例**:
```cpp
AppContext& ctx = host.GetContext();
if (!ctx.IsValid()) {
    LOG_ERROR("AppContext is invalid");
    return;
}
```

#### `ValidateOrThrow(const std::string& source) const`

验证应用上下文是否有效，如果无效则抛出异常。

**参数**:
- `source`: 调用源标识（用于错误信息）

**异常**: 如果上下文无效，抛出 `std::runtime_error`

**示例**:
```cpp
AppContext& ctx = host.GetContext();
try {
    ctx.ValidateOrThrow("MyModule::OnRegister");
} catch (const std::exception& e) {
    LOG_ERROR("AppContext validation failed: %s", e.what());
    return;
}
```

---

## 使用示例

### 在模块中访问上下文

```cpp
class MyModule : public AppModule {
public:
    void OnRegister(ECS::World& world, AppContext& ctx) override {
        // 验证上下文
        ctx.ValidateOrThrow("MyModule::OnRegister");
        
        // 使用Renderer
        if (ctx.renderer) {
            ctx.renderer->SetClearColor(0.1f, 0.12f, 0.16f, 1.0f);
        }
        
        // 使用ResourceManager
        if (ctx.resourceManager) {
            auto mesh = ctx.resourceManager->GetMesh("cube.mesh");
        }
        
        // 使用UniformManager（如果可用）
        if (ctx.uniformManager) {
            ctx.uniformManager->SetFloat("time", 0.0f);
        }
        
        // 使用AsyncResourceLoader（如果可用）
        if (ctx.asyncLoader) {
            ctx.asyncLoader->LoadTextureAsync("texture.png", [](Texture* tex) {
                // 加载完成回调
            });
        }
        
        // 使用EventBus
        if (ctx.globalEventBus) {
            ctx.globalEventBus->Subscribe<FrameBeginEvent>(
                [](const FrameBeginEvent& event) {
                    // 处理事件
                }
            );
        }
        
        // 使用ECS World
        if (ctx.world) {
            auto entity = ctx.world->CreateEntity();
            ctx.world->AddComponent<TransformComponent>(entity, TransformComponent{});
        }
    }
};
```

### 在SceneNode中访问上下文

```cpp
class MyNode : public SceneNode {
protected:
    void OnAttach(Scene& scene, AppContext& ctx) override {
        // 验证上下文
        if (!ctx.IsValid()) {
            LOG_ERROR("AppContext is invalid");
            return;
        }
        
        // 使用资源管理器注册资源
        if (ctx.resourceManager) {
            RegisterRequiredResource("my_mesh.mesh", "mesh");
        }
        
        // 获取World
        auto& world = GetWorld();
        
        // 创建实体
        auto entity = world.CreateEntity();
        
        // 添加组件
        ECS::TransformComponent transform{};
        transform.SetPosition(Vector3::Zero());
        world.AddComponent(entity, transform);
    }
};
```

### 在Scene中访问上下文

```cpp
class MyScene : public Scene {
    void OnAttach(AppContext& ctx, ModuleRegistry& modules) override {
        // 验证上下文
        ctx.ValidateOrThrow("MyScene::OnAttach");
        
        // 使用Renderer创建层级
        if (ctx.renderer) {
            auto layer = ctx.renderer->CreateLayer(200);
            layer->SetEnabled(true);
        }
        
        // 使用ResourceManager预加载资源
        if (ctx.resourceManager) {
            // 预加载资源
        }
        
        // 订阅事件
        if (ctx.globalEventBus) {
            ctx.globalEventBus->Subscribe<SceneLifecycleEvent>(
                [](const SceneLifecycleEvent& event) {
                    // 处理场景生命周期事件
                }
            );
        }
    }
};
```

---

## FrameUpdateArgs 说明

`FrameUpdateArgs` 结构包含帧更新的相关信息：

```cpp
struct FrameUpdateArgs {
    float deltaTime = 0.0f;      // 帧时间间隔（秒）
    double absoluteTime = 0.0;   // 绝对时间（秒）
    uint64_t frameIndex = 0;     // 帧索引
};
```

**使用示例**:
```cpp
void OnUpdate(const FrameUpdateArgs& frame) override {
    // 使用帧时间
    float dt = frame.deltaTime;
    
    // 使用绝对时间
    double time = frame.absoluteTime;
    
    // 使用帧索引
    uint64_t frame = frame.frameIndex;
    
    // 更新逻辑
    UpdateLogic(dt);
}
```

---

## 注意事项

1. **指针有效性**: 某些字段（如 `uniformManager`、`asyncLoader`、`uiInputRouter`）可能为 `nullptr`，使用前需要检查
2. **必需字段**: `renderer`、`resourceManager`、`world` 通常应该非空，使用 `IsValid()` 或 `ValidateOrThrow()` 验证
3. **生命周期**: AppContext 的生命周期由 ApplicationHost 管理，无需手动创建或销毁
4. **线程安全**: AppContext 本身不提供线程安全保证，访问应在主线程中进行

---

## 相关文档

- [ApplicationHost API](ApplicationHost.md) - 应用宿主API
- [Renderer API](Renderer.md) - 渲染器API
- [ResourceManager API](ResourceManager.md) - 资源管理器API
- [EventBus API](EventBus.md) - 事件总线API
- [World API](World.md) - ECS世界API

---

[返回API文档首页](README.md)

