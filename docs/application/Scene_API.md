[返回文档首页](../README.md)

# Scene API

---

## 背景与目标

Scene API 负责组织应用层的场景生命周期、资源需求与节点树。结合 Phase 2 的应用层规划，我们需要：

- 提供统一的场景切换接口；
- 支持场景内的节点化管理（SceneGraph）；
- 保证资源加载、注册、销毁的顺序可控；
- 让模块系统（ModuleRegistry）与场景生命周期协同。

---

## 核心角色

### `Render::Application::Scene`

```cpp
class Scene {
public:
    virtual ~Scene() = default;

    virtual std::string_view Name() const = 0;

    virtual void OnAttach(AppContext& ctx, ModuleRegistry& modules) = 0;
    virtual void OnDetach(AppContext& ctx) = 0;

    virtual SceneResourceManifest BuildManifest() const = 0;

    virtual void OnEnter(const SceneEnterArgs& args) = 0;
    virtual void OnUpdate(const FrameUpdateArgs& frame) = 0;
    virtual SceneSnapshot OnExit(const SceneExitArgs& args) = 0;
};
```

- `OnAttach/OnDetach`：场景被 `SceneManager` 挂载或移除时调用，常用于注册组件、系统或搭建：UI 面板。
- `BuildManifest`：返回必需与可选资源清单，为 `SceneManager` 预加载。
- `OnEnter/OnExit`：场景激活/退出，负责恢复或保存状态。
- `OnUpdate`：每帧回调，运行场景逻辑。

### `SceneResourceManifest`

```cpp
struct SceneResourceManifest {
    std::vector<ResourceRequest> required;
    std::vector<ResourceRequest> optional;

    void Merge(const SceneResourceManifest& other);
};

struct ResourceRequest {
    std::string identifier;
    std::string type;       // mesh / material / shader ...
    ResourceScope scope;    // Scene / Shared
    bool optional;
};
```

`SceneGraph` 内的每个节点可注册资源需求，最终通过 `SceneGraph::BuildManifest()` 聚合。

### `SceneEnterArgs / SceneExitArgs / SceneSnapshot`

- `SceneEnterArgs`：包含上一场景快照、参数与预加载进度。
- `SceneSnapshot`：自定义键值对，存档 / 切换时使用。
- `SceneExitArgs`：当前场景退出时的附加信息（如退出原因）。

---

## 场景图（SceneGraph）

### `SceneNode`

```cpp
class SceneNode : public std::enable_shared_from_this<SceneNode> {
public:
    using Ptr = std::shared_ptr<SceneNode>;

    explicit SceneNode(std::string_view name);
    void AddChild(const Ptr& child);
    void RemoveChild(const Ptr& child);
    std::vector<Ptr> GetChildren() const;

protected:
    virtual void OnAttach(Scene& scene, AppContext& context);
    virtual void OnDetach();
    virtual void OnEnter(const SceneEnterArgs& args);
    virtual void OnExit();
    virtual void OnUpdate(const FrameUpdateArgs& frameArgs);

    void RegisterRequiredResource(std::string identifier, std::string type, ResourceScope scope);
    // ...
};
```

- 每个节点表示场景的一部分（逻辑、角色、灯光、UI）。
- 节点可管理 ECS 实体、资源或自定义状态。
- `RegisterRequiredResource` / `RegisterOptionalResource` 支持资源声明。

### `SceneGraph`

```cpp
class SceneGraph {
public:
    explicit SceneGraph(SceneNode::Ptr root);
    void SetRoot(SceneNode::Ptr root);
    SceneResourceManifest BuildManifest() const;
    void Attach(Scene& scene, AppContext& context);
    void Enter(const SceneEnterArgs& args);
    void Update(const FrameUpdateArgs& frameArgs);
    void Exit();
    void Detach();
};
```

场景中可维护一个 `SceneGraph`，在 `OnAttach` 时 `SetRoot` 并 `Attach`；`OnEnter`, `OnUpdate`, `OnExit`, `OnDetach` 分别调用 `SceneGraph` 对应接口，统一管理节点生命周期。

---

## `SceneManager`

### 接口概览

```cpp
class SceneManager {
public:
    void Initialize(AppContext* appContext, ModuleRegistry* modules);
    void Shutdown();
    
    // 场景工厂注册
    void RegisterSceneFactory(std::string sceneId, SceneFactory factory);
    bool HasSceneFactory(std::string_view sceneId) const;
    
    // 场景切换
    bool PushScene(std::string_view sceneId, SceneEnterArgs args = {});
    bool ReplaceScene(std::string_view sceneId, SceneEnterArgs args = {});
    std::optional<SceneSnapshot> PopScene(SceneExitArgs args = {});
    
    // 场景更新
    void Update(const FrameUpdateArgs& frameArgs);
    
    // 场景查询
    Scene* GetActiveScene() noexcept;
    const Scene* GetActiveScene() const noexcept;
    bool IsTransitionInProgress() const noexcept;
    size_t SceneCount() const noexcept;
    
    // 场景序列化
    bool SaveActiveScene(const std::string& filePath);
    bool LoadSceneFromFile(const std::string& filePath, SceneEnterArgs args = {});
};
```

### 核心方法

#### 初始化和关闭

- `Initialize(AppContext*, ModuleRegistry*)`：初始化场景管理器，需要提供应用上下文和模块注册表。
- `Shutdown()`：关闭场景管理器，清理所有场景。

#### 场景工厂注册

- `RegisterSceneFactory(sceneId, factory)`：注册场景工厂函数，用于创建场景实例。
- `HasSceneFactory(sceneId)`：检查场景工厂是否已注册。

#### 场景切换

- `PushScene(sceneId, args)`：将新场景压入场景栈并激活。返回 `true` 表示成功。
- `ReplaceScene(sceneId, args)`：替换栈顶场景（保留快照传递）。返回 `true` 表示成功。
- `PopScene(args)`：退出当前场景并返回快照。如果场景栈为空，返回 `std::nullopt`。

#### 场景更新和查询

- `Update(frameArgs)`：调用当前活动场景的 `OnUpdate` 方法。
- `GetActiveScene()`：获取当前活动场景指针，如果没有活动场景返回 `nullptr`。
- `IsTransitionInProgress()`：检查是否有场景切换正在进行中。
- `SceneCount()`：返回场景栈中的场景数量。

#### 场景序列化

- `SaveActiveScene(filePath)`：将当前活动场景保存到JSON文件。返回 `true` 表示成功。
- `LoadSceneFromFile(filePath, args)`：从JSON文件加载场景并推入场景栈。返回 `true` 表示成功。

### 事件广播

- `SceneTransitionEvent`：`PushScene` / `ReplaceScene` / `PopScene` 调用时发布。
- `SceneLifecycleEvent`：`OnAttach`、`OnEnter`、`OnExit`、`OnDetach` 等阶段触发，便于调试 HUD 或工具链监听。
- `SceneManifestEvent`：执行 `BuildManifest` 后发布，可用于资源加载监控。
- `ScenePreloadProgressEvent`：`SceneManager` 轮询资源可用性时发布，包含 required/optional 已就绪数量、缺失资源列表、完成/失败标记。

### 热切换流程

1. `PushScene("Demo")` → 通过工厂创建实例 → `OnAttach` → `BuildManifest` → 轮询资源（必要资源就绪后触发 `OnEnter`）。
2. `ReplaceScene("Menu")` → `PopScene` 保存快照 → 新场景 `OnEnter` 可读取 `previousSnapshot`。
3. `PopScene` → 调用 `OnExit` 并 `OnDetach`，返回快照给调用方。

---

## 示例：BootScene 场景图节点

```cpp
auto root = std::make_shared<SceneNode>("BootScene.Root");
root->AddChild(std::make_shared<CubeNode>());
root->AddChild(std::make_shared<CameraNode>());
root->AddChild(std::make_shared<PointLightNode>());
m_sceneGraph.SetRoot(root);
m_sceneGraph.Attach(*this, ctx);
```

- `CubeNode`：注册 mesh/material/shader，创建立方体实体，在 `OnUpdate` 中为实体旋转。
- `CameraNode`：`OnEnter` 创建相机实体，`OnExit`销毁。
- `PointLightNode`：布置场景光源。
- `Scene::OnUpdate` 中调用 `m_sceneGraph.Update(frame)` 即可遍历所有节点。

---

## 与模块系统的协作

- `OnAttach` 中可通过 `ModuleRegistry` 激活所需 `AppModule`（如 `InputModule`、`DebugHUDModule`）。
- `AppContext` 持有 `Renderer`、`ResourceManager`、`AsyncResourceLoader`、`EventBus` 等核心单例，节点可通过 `GetContext()` 访问。
- `SceneManager` 在 `Initialize` 时接收 `ModuleRegistry`，场景切换过程保持模块状态。

---

## 最佳实践

1. **职责单一**：将场景拆分为多个 `SceneNode`，每个节点管理自身实体与资源。
2. **资源声明**：节点在 `OnAttach` 或构造函数里注册资源清单，避免散落的加载逻辑。
3. **紧耦系统注册**：场景层负责 `World::RegisterComponent` / `RegisterSystem`，节点只关心业务逻辑。
4. **快照与恢复**：`SceneSnapshot` 可记录关键状态（如当前选中对象、UI 布局），在 `OnEnter` 中恢复。
5. **模块依赖**：按需激活 `AppModule` 并使用 `ModuleRegistry` 提供的依赖检查，保持组件组合的可控性。

---

## `SceneSerializer`

### 概述

`SceneSerializer` 负责将场景序列化为JSON格式，以及从JSON反序列化场景。支持ECS实体、组件的完整序列化和反序列化。

### 接口概览

```cpp
class SceneSerializer {
public:
    SceneSerializer() = default;
    ~SceneSerializer() = default;

    // 场景序列化
    bool SaveScene(ECS::World& world, const std::string& sceneName, const std::string& filePath);
    
    // 场景反序列化
    std::optional<std::string> LoadScene(ECS::World& world, const std::string& filePath, AppContext& ctx);
    
    // 资源路径校验
    static bool ValidateResourcePath(const std::string& resourcePath, const std::string& resourceType, AppContext& ctx);
};
```

### 核心方法

#### 保存场景

```cpp
bool SaveScene(ECS::World& world, const std::string& sceneName, const std::string& filePath);
```

将ECS World中的所有实体和组件序列化为JSON格式并保存到文件。

**参数**:
- `world`: ECS World对象，包含要序列化的实体和组件
- `sceneName`: 场景名称，会保存到JSON中
- `filePath`: 保存路径

**返回值**: 成功返回 `true`，失败返回 `false`

**支持的组件类型**:
- `TransformComponent`: 位置、旋转、缩放
- `MeshRenderComponent`: 网格和材质引用
- `CameraComponent`: 相机参数（投影类型、FOV、近远平面等）
- `LightComponent`: 光源类型、颜色、强度、范围等
- `NameComponent`: 实体名称

#### 加载场景

```cpp
std::optional<std::string> LoadScene(ECS::World& world, const std::string& filePath, AppContext& ctx);
```

从JSON文件加载场景，创建实体和组件。

**参数**:
- `world`: ECS World对象，用于创建新实体
- `filePath`: JSON文件路径
- `ctx`: AppContext，用于资源路径校验和资源获取

**返回值**: 成功返回场景名称，失败返回 `std::nullopt`

**功能**:
- 自动创建实体和组件
- 从 `ResourceManager` 获取资源指针（mesh、material）
- 验证资源路径的有效性
- 设置 `resourcesLoaded` 标志

#### 资源路径校验

```cpp
static bool ValidateResourcePath(const std::string& resourcePath, const std::string& resourceType, AppContext& ctx);
```

验证资源路径是否有效。

**参数**:
- `resourcePath`: 资源路径或名称
- `resourceType`: 资源类型（"mesh"、"material"、"texture"、"shader"）
- `ctx`: AppContext，包含 ResourceManager

**返回值**: 如果资源存在返回 `true`

### JSON 格式

场景序列化的JSON格式如下：

```json
{
  "sceneName": "MyScene",
  "entities": [
    {
      "id": 0,
      "name": "Cube",
      "components": {
        "transform": {
          "position": [0.0, 0.0, 0.0],
          "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.0, 1.0, 1.0]
        },
        "meshRender": {
          "meshName": "cube.mesh",
          "materialName": "default.material",
          "layerID": 200,
          "diffuseColor": [0.3, 0.7, 1.0, 1.0]
        }
      }
    },
    {
      "id": 1,
      "name": "MainCamera",
      "components": {
        "transform": {
          "position": [0.0, 1.5, 4.0],
          "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.0, 1.0, 1.0]
        },
        "camera": {
          "projectionType": "perspective",
          "fov": 60.0,
          "aspect": 1.77778,
          "nearPlane": 0.1,
          "farPlane": 100.0,
          "depth": 0,
          "clearColor": [0.05, 0.08, 0.12, 1.0],
          "layerMask": 4294967295
        }
      }
    }
  ]
}
```

### 使用示例

#### 保存场景

```cpp
#include "render/application/scene_serializer.h"
#include "render/application/scene_manager.h"

// 通过 SceneManager 保存
SceneManager sceneManager;
// ... 初始化场景管理器并创建场景 ...
if (sceneManager.SaveActiveScene("my_scene.json")) {
    Logger::GetInstance().Info("Scene saved successfully");
} else {
    Logger::GetInstance().Error("Failed to save scene");
}

// 直接使用 SceneSerializer
SceneSerializer serializer;
ECS::World& world = host.GetWorld();
if (serializer.SaveScene(world, "MyScene", "my_scene.json")) {
    Logger::GetInstance().Info("Scene saved successfully");
}
```

#### 加载场景

```cpp
// 通过 SceneManager 加载
SceneManager sceneManager;
// ... 初始化场景管理器 ...
if (sceneManager.LoadSceneFromFile("my_scene.json")) {
    Logger::GetInstance().Info("Scene loaded successfully");
} else {
    Logger::GetInstance().Error("Failed to load scene");
}

// 直接使用 SceneSerializer
SceneSerializer serializer;
ECS::World& world = host.GetWorld();
auto& ctx = host.GetContext();
auto sceneName = serializer.LoadScene(world, "my_scene.json", ctx);
if (sceneName.has_value()) {
    Logger::GetInstance().InfoFormat("Scene '%s' loaded successfully", sceneName.value().c_str());
} else {
    Logger::GetInstance().Error("Failed to load scene");
}
```

#### 资源路径校验

```cpp
AppContext& ctx = host.GetContext();
if (SceneSerializer::ValidateResourcePath("cube.mesh", "mesh", ctx)) {
    Logger::GetInstance().Info("Mesh resource exists");
} else {
    Logger::GetInstance().Warning("Mesh resource not found");
}
```

### 注意事项

1. **资源预加载**: 加载场景前，确保所需的资源（mesh、material、shader）已经在 `ResourceManager` 中注册。
2. **实体清理**: 加载新场景前，建议清空当前场景的实体，避免实体ID冲突。
3. **资源指针**: 反序列化时，`SceneSerializer` 会自动从 `ResourceManager` 获取资源指针，并设置 `resourcesLoaded` 标志。
4. **错误处理**: 如果资源路径无效，会记录警告但不会阻止场景加载（可选资源）。
5. **组件支持**: 目前支持 `TransformComponent`、`MeshRenderComponent`、`CameraComponent`、`LightComponent`、`NameComponent`。如需支持其他组件，需要扩展 `SceneSerializer`。

---

## 参考

- `include/render/application/scene.h`
- `include/render/application/scene_manager.h`
- `include/render/application/scene_serializer.h`
- `include/render/application/scene_graph.h`
- `src/application/scenes/boot_scene.cpp`
- `src/application/scene_graph.cpp`
- `src/application/scene_serializer.cpp`
- `include/render/application/modules/*.h`
- `docs/SCENE_MODULE_FRAMEWORK.md`
- `examples/55_scene_serialization_test.cpp` - 场景序列化测试示例
- `examples/56_scene_manager_stress_test.cpp` - 场景管理器压力测试示例

---

如需进一步示例或工具链说明，可参阅 Phase 2 TODO 列表和 HUD / UI 相关文档。欢迎在 Issue 或 TODO 文档中记录后续需求。

