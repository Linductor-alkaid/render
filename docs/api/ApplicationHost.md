# ApplicationHost API 文档

## 目录
[返回API文档首页](README.md)

---

## 概述

`ApplicationHost` 是应用层的统一入口，负责协调和管理整个应用的生命周期。它封装了 `SceneManager`、`ModuleRegistry`、`EventBus` 和 `ECS::World`，提供统一的初始化和帧循环接口。

**命名空间**: `Render::Application`

---

## 类定义

```cpp
class ApplicationHost {
public:
    struct Config {
        Renderer* renderer = nullptr;
        UniformManager* uniformManager = nullptr;
        ResourceManager* resourceManager = nullptr;
        AsyncResourceLoader* asyncLoader = nullptr;
        std::shared_ptr<ECS::World> world;
        bool createWorldIfMissing = true;
    };

    ApplicationHost();
    ~ApplicationHost();

    bool Initialize(const Config& config);
    void Shutdown();

    void RegisterSceneFactory(std::string sceneId, SceneFactory factory);
    bool PushScene(std::string_view sceneId, SceneEnterArgs args = {});
    bool ReplaceScene(std::string_view sceneId, SceneEnterArgs args = {});

    void UpdateFrame(const FrameUpdateArgs& args);
    void UpdateWorld(float deltaTime);

    bool IsInitialized() const noexcept;

    AppContext& GetContext() noexcept;
    const AppContext& GetContext() const noexcept;
    ModuleRegistry& GetModuleRegistry() noexcept;
    SceneManager& GetSceneManager() noexcept;
    EventBus& GetEventBus() noexcept;
    ECS::World& GetWorld() noexcept;
};
```

---

## 核心功能

### 1. 初始化与关闭

#### `Initialize(const Config& config)`

初始化应用宿主，配置核心服务。

**参数**:
- `config.renderer`: Renderer实例指针（必需）
- `config.uniformManager`: UniformManager实例指针（可选）
- `config.resourceManager`: ResourceManager实例指针（必需）
- `config.asyncLoader`: AsyncResourceLoader实例指针（可选）
- `config.world`: ECS World实例（可选，如果为空且createWorldIfMissing为true，则自动创建）
- `config.createWorldIfMissing`: 如果World为空，是否自动创建（默认true）

**返回值**: 成功返回 `true`，失败返回 `false`

**示例**:
```cpp
ApplicationHost host;
ApplicationHost::Config config{};
config.renderer = renderer;
config.resourceManager = &ResourceManager::GetInstance();
config.asyncLoader = &AsyncResourceLoader::GetInstance();
config.uniformManager = nullptr;

if (!host.Initialize(config)) {
    LOG_ERROR("Failed to initialize ApplicationHost");
    return -1;
}
```

#### `Shutdown()`

关闭应用宿主，清理所有资源。

**注意**: 会自动调用所有模块的 `OnUnregister`，清理场景栈，并关闭内部系统。

---

### 2. 场景管理

#### `RegisterSceneFactory(std::string sceneId, SceneFactory factory)`

注册场景工厂函数，用于创建场景实例。

**参数**:
- `sceneId`: 场景标识符
- `factory`: 场景工厂函数（`std::function<ScenePtr()>`）

**示例**:
```cpp
host.RegisterSceneFactory("BootScene", []() {
    return std::make_unique<BootScene>();
});
```

#### `PushScene(std::string_view sceneId, SceneEnterArgs args = {})`

将新场景压入场景栈并激活。

**参数**:
- `sceneId`: 场景标识符
- `args`: 场景进入参数（可选）

**返回值**: 成功返回 `true`，失败返回 `false`

**示例**:
```cpp
SceneEnterArgs args{};
args.deltaTime = 0.0f;
args.absoluteTime = 0.0;
if (!host.PushScene("BootScene", args)) {
    LOG_ERROR("Failed to push scene");
}
```

#### `ReplaceScene(std::string_view sceneId, SceneEnterArgs args = {})`

替换栈顶场景（保留快照传递）。

**参数**:
- `sceneId`: 场景标识符
- `args`: 场景进入参数（可选）

**返回值**: 成功返回 `true`，失败返回 `false`

**示例**:
```cpp
if (!host.ReplaceScene("MenuScene")) {
    LOG_ERROR("Failed to replace scene");
}
```

---

### 3. 帧循环

#### `UpdateFrame(const FrameUpdateArgs& args)`

更新应用帧状态，调用所有模块的PreFrame钩子和场景的OnUpdate。

**参数**:
- `args.deltaTime`: 帧时间间隔（秒）
- `args.absoluteTime`: 绝对时间（秒）
- `args.frameIndex`: 帧索引

**示例**:
```cpp
FrameUpdateArgs frameArgs{};
frameArgs.deltaTime = renderer->GetDeltaTime();
frameArgs.absoluteTime = absoluteTime;
frameArgs.frameIndex = frameCount++;

host.UpdateFrame(frameArgs);
```

#### `UpdateWorld(float deltaTime)`

更新ECS世界（调用所有系统的Update方法）。

**参数**:
- `deltaTime`: 帧时间间隔（秒）

**示例**:
```cpp
host.UpdateWorld(renderer->GetDeltaTime());
```

---

### 4. 访问器

#### `GetContext()`

获取应用上下文（AppContext），包含Renderer、ResourceManager等核心服务引用。

**返回值**: `AppContext&` 或 `const AppContext&`

**示例**:
```cpp
auto& ctx = host.GetContext();
if (ctx.resourceManager) {
    auto mesh = ctx.resourceManager->GetMesh("cube.mesh");
}
```

#### `GetModuleRegistry()`

获取模块注册表（ModuleRegistry），用于注册和查询模块。

**返回值**: `ModuleRegistry&`

**示例**:
```cpp
auto& moduleRegistry = host.GetModuleRegistry();
moduleRegistry.RegisterModule(std::make_unique<CoreRenderModule>());
auto* module = moduleRegistry.GetModule("CoreRenderModule");
```

#### `GetSceneManager()`

获取场景管理器（SceneManager），用于管理场景栈和场景切换。

**返回值**: `SceneManager&`

**示例**:
```cpp
auto& sceneManager = host.GetSceneManager();
auto* activeScene = sceneManager.GetActiveScene();
```

#### `GetEventBus()`

获取事件总线（EventBus），用于发布和订阅事件。

**返回值**: `EventBus&`

**示例**:
```cpp
auto& eventBus = host.GetEventBus();
eventBus.Subscribe<FrameBeginEvent>([](const FrameBeginEvent& event) {
    // 处理帧开始事件
});
```

#### `GetWorld()`

获取ECS世界（World），包含所有实体、组件和系统。

**返回值**: `ECS::World&`

**示例**:
```cpp
auto& world = host.GetWorld();
auto entity = world.CreateEntity();
world.AddComponent<TransformComponent>(entity, TransformComponent{});
```

---

## 使用示例

### 完整应用初始化示例

```cpp
#include "render/application/application_host.h"
#include "render/application/modules/core_render_module.h"
#include "render/application/modules/input_module.h"
#include "render/application/modules/debug_hud_module.h"
#include "render/application/scenes/boot_scene.h"

int main() {
    // 1. 创建和初始化Renderer
    Renderer* renderer = Renderer::Create();
    if (!renderer->Initialize("My Application", 1280, 720)) {
        return -1;
    }
    renderer->SetClearColor(0.1f, 0.12f, 0.16f, 1.0f);

    // 2. 创建ResourceManager和AsyncResourceLoader
    auto& resourceManager = ResourceManager::GetInstance();
    auto& asyncLoader = AsyncResourceLoader::GetInstance();
    asyncLoader.Initialize(1);

    // 3. 创建和初始化ApplicationHost
    ApplicationHost host;
    ApplicationHost::Config config{};
    config.renderer = renderer;
    config.resourceManager = &resourceManager;
    config.asyncLoader = &asyncLoader;
    config.uniformManager = nullptr;

    if (!host.Initialize(config)) {
        LOG_ERROR("Failed to initialize ApplicationHost");
        asyncLoader.Shutdown();
        Renderer::Destroy(renderer);
        return -1;
    }

    // 4. 注册模块
    auto& moduleRegistry = host.GetModuleRegistry();
    moduleRegistry.RegisterModule(std::make_unique<CoreRenderModule>());
    moduleRegistry.RegisterModule(std::make_unique<InputModule>());
    moduleRegistry.RegisterModule(std::make_unique<DebugHUDModule>());

    // 5. 注册场景
    auto& sceneManager = host.GetSceneManager();
    sceneManager.RegisterSceneFactory("BootScene", []() {
        return std::make_unique<BootScene>();
    });

    // 6. 加载初始场景
    if (!sceneManager.PushScene("BootScene")) {
        LOG_ERROR("Failed to push BootScene");
        host.Shutdown();
        asyncLoader.Shutdown();
        Renderer::Destroy(renderer);
        return -1;
    }

    // 7. 主循环
    bool running = true;
    double absoluteTime = 0.0;
    uint64_t frameCount = 0;

    while (running) {
        // 处理事件
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        // 更新帧
        const float deltaTime = renderer->GetDeltaTime();
        absoluteTime += static_cast<double>(deltaTime);

        FrameUpdateArgs frameArgs{};
        frameArgs.deltaTime = deltaTime;
        frameArgs.absoluteTime = absoluteTime;
        frameArgs.frameIndex = frameCount++;

        host.UpdateFrame(frameArgs);

        // 渲染
        renderer->BeginFrame();
        renderer->Clear();

        host.UpdateWorld(deltaTime);

        renderer->FlushRenderQueue();
        renderer->EndFrame();
        renderer->Present();
    }

    // 8. 清理
    host.Shutdown();
    asyncLoader.Shutdown();
    Renderer::Destroy(renderer);

    return 0;
}
```

---

## 注意事项

1. **初始化顺序**: 必须先初始化Renderer、ResourceManager和AsyncResourceLoader，然后才能初始化ApplicationHost
2. **World生命周期**: 如果提供外部World，ApplicationHost不会销毁它；如果自动创建，ApplicationHost会在Shutdown时销毁
3. **场景切换**: 场景切换是异步的，需要等待资源预加载完成才会进入场景
4. **模块依赖**: 注册模块时会自动解析依赖关系并按拓扑排序激活
5. **线程安全**: ApplicationHost本身不是线程安全的，所有调用应在主线程中进行

---

## 相关文档

- [ModuleRegistry API](ModuleRegistry.md) - 模块注册表API
- [SceneManager API](SceneManager.md) - 场景管理器API
- [EventBus API](EventBus.md) - 事件总线API
- [AppContext API](AppContext.md) - 应用上下文API
- [Scene API](../application/Scene_API.md) - 场景接口文档

---

[返回API文档首页](README.md)

