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
    void RegisterSceneFactory(std::string sceneId, SceneFactory factory);
    bool PushScene(std::string_view sceneId, SceneEnterArgs args = {});
    bool ReplaceScene(std::string_view sceneId, SceneEnterArgs args = {});
    std::optional<SceneSnapshot> PopScene(SceneExitArgs args = {});
    void Update(const FrameUpdateArgs& frameArgs);
    // ...
};
```

- 事件广播：
  - `SceneTransitionEvent`：`PushScene` / `ReplaceScene` / `PopScene` 调用时发布。
  - `SceneLifecycleEvent`：`OnAttach`、`OnEnter`、`OnExit`、`OnDetach` 等阶段触发，便于调试 HUD 或工具链监听。
  - `SceneManifestEvent`：执行 `BuildManifest` 后发布，可用于资源加载监控。
  - `ScenePreloadProgressEvent`：`SceneManager` 轮询资源可用性时发布，包含 required/optional 已就绪数量、缺失资源列表、完成/失败标记。

- 注册场景工厂 `sceneId → SceneFactory`。
- `PushScene`：压入场景栈并激活。
- `ReplaceScene`：替换栈顶场景（保留快照传递）。
- `PopScene`：退出当前场景并返回快照。
- `Update`：调用当前场景 `OnUpdate`。

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

## 参考

- `include/render/application/scene.h`
- `include/render/application/scene_graph.h`
- `src/application/scenes/boot_scene.cpp`
- `src/application/scene_graph.cpp`
- `include/render/application/modules/*.h`
- `docs/SCENE_MODULE_FRAMEWORK.md`

---

如需进一步示例或工具链说明，可参阅 Phase 2 TODO 列表和 HUD / UI 相关文档。欢迎在 Issue 或 TODO 文档中记录后续需求。

