[返回文档首页](../README.md)

# SceneGraph 节点开发手册

---

## 1. 综述

SceneGraph 将场景逻辑拆分为树状节点，每个节点可独立管理实体、资源与状态。目标：

- 避免场景基类臃肿，按功能模块（渲染物体、相机、灯光、UI 等）拆分；
- 统一管理资源声明、生命周期与父子关系；
- 支持在场景切换时安全地 Attach/Enter/Update/Exit/Detach。

典型节点结构：

```cpp
class MyNode : public SceneNode {
public:
    MyNode() : SceneNode("MyNode") {}
protected:
    void OnAttach(Scene& scene, AppContext& ctx) override;
    void OnEnter(const SceneEnterArgs& args) override;
    void OnUpdate(const FrameUpdateArgs& frameArgs) override;
    void OnExit() override;
};
```

---

## 2. 架构与生命周期

### 2.1 SceneGraph 调用流程

| 阶段 | 调用函数 | 描述 |
|------|----------|------|
| 场景初始化 (`OnAttach`) | `SceneGraph::Attach` → 节点 `OnAttach` | 注册资源、准备环境 |
| 场景激活 (`OnEnter`) | `SceneGraph::Enter` → 节点 `OnEnter` | 创建实体、读取快照 |
| 每帧 (`OnUpdate`) | `SceneGraph::Update` → 节点 `OnUpdate` | 更新逻辑 |
| 场景退出 (`OnExit`) | `SceneGraph::Exit` → 节点 `OnExit` | 保存状态、销毁实体 |
| 场景卸载 (`OnDetach`) | `SceneGraph::Detach` → 节点 `OnDetach` | 恢复资源、清理指针 |

### 2.2 生命周期回调

- `OnAttach(Scene& scene, AppContext& ctx)`：可注册资源、缓存 `World`、`ResourceManager` 等句柄。
- `OnEnter(const SceneEnterArgs& args)`：创建 ECS 实体、读取快照、初始化节点状态。
- `OnUpdate(const FrameUpdateArgs& frameArgs)`：每帧逻辑，如动画、事件响应。
- `OnExit()`：销毁 ECS 实体或保存状态。
- `OnDetach()`：释放持有的资源引用或取消注册。

### 2.3 资源声明

- `RegisterRequiredResource(identifier, type, scope)`：必需资源，场景预加载失败会阻塞进入。
- `RegisterOptionalResource(identifier, type, scope)`：可选资源，缺失时可降级处理。
- `SceneGraph::BuildManifest()` 汇总所有节点需求供 `SceneManager` 预加载。

---

## 3. 节点与 World / Resource 的交互

`SceneNode` 提供以下便捷接口：

```cpp
Scene& GetScene() const;
AppContext& GetContext() const;
ECS::World& GetWorld() const;
ResourceManager& GetResourceManager() const;
```

- 使用 `GetWorld()` 访问 ECS 组件与系统；
- 使用 `GetResourceManager()` 读取/注册共享资源；
- `GetContext()` 提供 Renderer、AsyncResourceLoader、EventBus 等。

### 3.1 实体管理

- 推荐在 `OnEnter` 创建实体，在 `OnExit` 中销毁；
- 节点可持有 `std::optional<EntityID>`，在 `OnExit` 时检查有效性；
- 若节点需要跨场景保留状态，可在 `OnExit` 中写入 `SceneSnapshot`（由场景负责收集）。

---

## 4. 示例节点拆解

### 4.1 CubeNode（渲染立方体）

```cpp
class CubeNode : public SceneNode {
public:
    CubeNode() : SceneNode("CubeNode") {
        RegisterRequiredResource("boot.demo.mesh", "mesh");
        RegisterRequiredResource("boot.demo.material", "material");
        RegisterRequiredResource("boot.demo.shader", "shader");
    }

protected:
    void OnAttach(Scene&, AppContext& ctx) override {
        auto& rm = *ctx.resourceManager;
        if (!rm.HasMesh("boot.demo.mesh")) {
            rm.RegisterMesh("boot.demo.mesh", MeshLoader::CreateCube(1.f, 1.f, 1.f, Color::White()));
        }
        // 创建 shader 与材质，略…
    }

    void OnEnter(const SceneEnterArgs&) override {
        auto& world = GetWorld();
        m_entity = world.CreateEntity({.name = "DemoCube"});
        // 添加 Transform 与 MeshRenderComponent …
    }

    void OnUpdate(const FrameUpdateArgs& frame) override {
        // 更新 Transform，提高旋转效果 …
    }

    void OnExit() override {
        if (m_entity && GetWorld().IsValidEntity(*m_entity)) {
            GetWorld().DestroyEntity(*m_entity);
        }
        m_entity.reset();
    }

private:
    std::optional<ECS::EntityID> m_entity;
};
```

### 4.2 CameraNode（场景相机）

- `OnEnter` 创建相机实体，设置投影矩阵；
- `GetContext().renderer` 可读取窗口尺寸；
- `OnExit` 及时销毁相机实体。

### 4.3 PointLightNode（点光源）

- `OnEnter` 创建点光源实体并配置 `LightComponent`；
- 在 `OnExit` 销毁实体，防止下次进入重复创建；
- 适配 `LightSystem`，自动进入渲染管线。

---

## 5. 节点模式与最佳实践

| 模式 | 说明 | 建议 |
|------|------|------|
| **渲染节点** | 管理某种渲染对象（模型、特效） | 在 `OnAttach` 注册资源，`OnEnter` 创建实体 |
| **控制节点** | 单纯运行逻辑，不管理实体 | 在 `OnUpdate` 调用 World 查询或事件处理 |
| **管理节点** | 组合子节点，协调状态（如 `SceneRoot`） | 仅组织结构，不直接持有实体 |
| **资源守护节点** | 负责加载/缓存一类资源 | 预加载后常驻，可在 `SceneGraph` 模式中重用 |

### 注意事项

1. **节点扁平 vs 深层**：节点可多级嵌套，但建议保持逻辑清晰。若节点之间强耦合，可考虑合并。
2. **资源 scope**：`ResourceScope::Scene` 表示场景退出时释放，`ResourceScope::Shared` 表示跨场景共享。
3. **SceneGraph 重复 Attach**：`SceneGraph::SetRoot` 会在新根被设置前对旧根执行 `Detach`，避免重复 attach。
4. **场景快照**：节点可自行保存局部状态，但由 `Scene` 统一输出 `SceneSnapshot`。推荐将节点状态写入 `SceneSnapshot::state`（如 JSON 或 key/value）。

---

## 6. 扩展案例

### 6.1 UI 节点

```cpp
class UINode : public SceneNode {
protected:
    void OnAttach(Scene&, AppContext& ctx) override {
        auto* moduleRegistry = ctx.moduleRegistry; // 若暴露接口，可激活 UIRuntimeModule
    }
    void OnEnter(const SceneEnterArgs&) override {
        // 构建 UI Widget 树，或使用 UIRuntimeModule 提供的接口
    }
};
```

### 6.2 动画节点

- `OnEnter` 加载动画曲线，绑定目标实体；
- `OnUpdate` 计算动画状态，更新 Transform 或自定义 Component；
- `OnExit` 复位或保存当前动画进度（可写入 Snapshot）。

### 6.3 资源加载节点

- `OnAttach` 注册需要加载的资源；
- `OnEnter` 发起异步加载（通过 `AsyncResourceLoader`），并在 `OnUpdate` 中检查是否完成；
- `OnExit` 清理加载任务或缓存。

---

## 7. 与 Module 协同

场景节点可在 `OnAttach` 或 `OnEnter` 中启停模块。例如：

```cpp
if (auto* modules = GetContext().modules) {
    modules->ActivateModule("InputModule");
}
```

建议将模块启停统一放在场景层（非节点层），保持节点只关注业务逻辑，以免节点对模块管理造成混乱。

---

## 8. 调试与工具

### 日志

- `SceneNode` 默认提供 `Logger` 使用，可针对 OnAttach/OnEnter 等节点生命周期记录信息；
- `SceneGraph::Update` 可加入调试开关，打印节点树结构或更新顺序。

### 可视化

- 后续可在 DebugHUD 或独立工具中展示 SceneGraph 结构（树形视图、节点状态、资源清单）。

---

## 9. 文件与参考

- 代码：
  - `include/render/application/scene_graph.h`
  - `src/application/scene_graph.cpp`
  - `src/application/scenes/boot_scene.cpp`（Cube/Camera/Light Node 示例）
- 文档：
  - `docs/application/Scene_API.md`
  - `docs/application/Module_Guide.md`
  - `docs/SCENE_MODULE_FRAMEWORK.md`

---

有了 SceneGraph 模式后，每个场景可根据需求拼装节点树，使逻辑更模块化、易扩展，同时可在文档与工具链中更直观地呈现场景结构。欢迎在实践中根据业务总结更多节点模式并补充到手册。
