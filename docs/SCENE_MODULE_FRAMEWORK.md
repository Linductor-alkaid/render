[返回文档首页](README.md)

---

# 应用层场景与模块框架设计方案（Phase 2）

**最后更新**: 2025-11-22  
**整体进度**: 约 90% 完成  
**参考文档**: [APPLICATION_LAYER_PROGRESS_REPORT.md](application/APPLICATION_LAYER_PROGRESS_REPORT.md)

## 1. 背景与现状评估

- 引擎核心层已完成资源、渲染、ECS 系统（`World`、`SystemRegistry`、`ResourceManager`、`UniformManager` 等），具备稳定的渲染循环与组件化逻辑。
- 当前缺乏面向应用层的场景生命周期、模块化系统组织、统一事件流与配置入口，导致示例/工具链无法复用统一框架。
- Phase 2 TODO 强调：场景节点生命周期（OnEnter/OnExit/OnUpdate）、热切换/多场景管理、资源预加载、系统注册排序、帧循环钩子、事件总线、UI 工具联动。
- 项目偏好：所有着色器 Uniform 必须通过 `UniformManager` 设置；新增资源需纳入核心资源管理器及 CMake；操作与 UI 风格需对齐 Blender。

## 2. 设计目标

- **统一场景生命周期**：提供场景基类，标准化 OnEnter/OnExit/OnUpdate/OnEvent 等接口。
- **模块化系统管理**：通过应用层 `ModuleRegistry` 封装 ECS 系统与工具模块，管理依赖、排序和热插拔。
- **资源预加载策略**：使用 Resource Manifest + `AsyncResourceLoader` 实现场景切换时的资源准备/释放。
- **多场景与热切换**：支持独立 Scene 状态缓存、并行预加载、队列式切换与回退（栈式）。
- **统一事件总线**：整合输入、渲染、资源、UI、工具事件，提供优先级与过滤机制。
- **Blender 风格交互支撑**：输入映射、操作记录在应用层集中处理，方便 UI/工具复用。
- **调试与可视化友好**：暴露模块与场景状态给 Phase 2 工具链（HUD、材质/Shader 面板等）。

## 3. 总体架构

```
┌────────────────────────────────────────────────────────────────┐
│                         Application Layer                      │
│                                                                │
│  ┌────────────┐    ┌──────────────────┐    ┌────────────────┐  │
│  │ AppContext │◄─► │ ModuleRegistry   │ ◄─►│ EventBus       │  │
│  └────────────┘    └──────────────────┘    └────────────────┘  │
│          ▲                      ▲                    ▲         │
│          │                      │                    │         │
│  ┌─────────────┐        ┌───────────────┐    ┌────────────────┐│
│  │ SceneManager│ ─────► │ Scene (Base)  │ ◄─►│ SceneGraph Ext ││
│  └─────────────┘        └───────────────┘    └────────────────┘│
│          │                      │                              │
│          ▼                      ▼                              │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │                    ECS World / Systems                   │  │
│  └──────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────┘
```

- **AppContext**：封装 `Renderer`、`ResourceManager`、`UniformManager`、`AsyncResourceLoader`、平台输入接口等对应用层公开的核心句柄。
- **SceneManager**：负责场景栈、切换策略、资源预加载与热切换调度。
- **Scene 基类**：定义生命周期、资源清单、模块依赖、自定义状态保存/恢复。
- **ModuleRegistry**：应用层模块登记表，对接 ECS 的 `World::RegisterSystem`，扩展依赖排序、PreFrame/PostFrame 钩子与热插拔调试入口。
- **EventBus**：统一事件流，支持优先级、过滤器、同步/异步派发，与 UI/工具联动。
- **SceneGraph 扩展**：在 ECS `World` 基础上提供节点生命周期管理、局部资源守护、场景树状态持久化。

## 4. 核心组件设计

### 4.1 AppContext

- **职责**：集中持有跨场景共享的核心服务与配置，确保模块之间通过显式接口访问。
- **字段建议**：
  - `Renderer* renderer`
  - `Ref<UniformManager> uniformManager`
  - `ResourceManager* resourceManager`
  - `AsyncResourceLoader* asyncLoader`
  - 输入系统（键鼠、手柄、命令队列）、窗口信息、路径配置
  - `EventBus* globalEventBus`
  - 可选：性能统计、FrameClock、Logger 通道
- **生命周期**：由应用初始化阶段创建，传入 `SceneManager` 与各模块。
- **Uniform 约束**：所有模块通过 `AppContext::uniformManager` 写 Uniform，保证遵循用户偏好。

### 4.2 Scene 基类

```cpp
class Scene : public IEventSink {
public:
    virtual ~Scene() = default;
    virtual std::string_view Name() const = 0;

    // 生命周期
    virtual void OnAttach(AppContext& ctx, ModuleRegistry& modules) = 0; // 注册组件、系统
    virtual void OnEnter(SceneEnterArgs args) = 0;  // 场景激活，数据加载完成
    virtual void OnUpdate(const FrameUpdateArgs& frame) = 0; // 每帧
    virtual void OnExit(SceneExitReason reason) = 0; // 即将离开
    virtual void OnDetach() = 0; // 场景永久卸载

    // 资源清单
    virtual SceneResourceManifest BuildManifest() const = 0;

    // 状态保存（用于热切换/暂停）
    virtual SceneSnapshot Serialize() const;
    virtual void Deserialize(const SceneSnapshot& snapshot);
};
```

- **SceneEnterArgs**：包含前一场景快照、传入参数、预加载进度。
- **SceneResourceManifest**：列出必需与可选资源（模型、纹理、shader、材质、配置 JSON）；`SceneManager` 调用 `ResourceManager`/`AsyncResourceLoader` 执行加载。
- **OnAttach/OnDetach**：只在场景首次创建或最终销毁时调用，用于注册 ECS 组件/系统、创建 UI 面板等。
- **事件处理**：继承 `IEventSink`，使用 `EventBus` 的订阅/优先级机制。

### 4.3 SceneManager

- **功能点**：
  - 维护场景栈与状态机：`PushScene`、`PopScene`、`ReplaceScene`、`PauseScene`。
  - 异步预加载：在新场景 `SceneResourceManifest` 准备完成前保持旧场景活跃。
  - 场景热切换：在 `OnExit` 与 `OnEnter` 之间执行资源引用迁移、状态快照传递。
  - 多场景共存：支持 Overlay 场景（如调试 HUD）挂在栈顶，同时保持底层场景更新策略（可配置 Update/Render Flag）。
  - Frame 钩子：提供 `PreFrame()`、`PostFrame()`，供 `ModuleRegistry` 和工具链注册 lambda。
- **数据结构**：
  - `struct SceneStackEntry { std::unique_ptr<Scene> instance; SceneSnapshot snapshot; SceneFlags flags; }`
  - `struct TransitionTask { SceneID target; SceneTransitionType type; Promise<LoadResult>; }`
- **资源管理**：集成 `ResourceManager` 引用计数；场景退出时释放 Manifest 中的 Scoped 资源（除非标记为 `Shared`）。

### 4.4 ModuleRegistry 与 AppModule

- **AppModule 基类**：
```cpp
class AppModule {
public:
    virtual ~AppModule() = default;
    virtual std::string_view Name() const = 0;
    virtual ModuleDependencies Dependencies() const;
    virtual int Priority(ModulePhase phase) const; // Register, PreFrame, PostFrame

    virtual void OnRegister(World& world, AppContext& ctx) = 0;
    virtual void OnUnregister(World& world, AppContext& ctx);

    virtual void OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx);
    virtual void OnPostFrame(const FrameUpdateArgs& frame, AppContext& ctx);
};
```

- **ModuleRegistry 职责**：
  - 根据依赖图拓扑排序模块，调用 `World::RegisterSystem` 或场景自定义初始化。
  - 提供运行时启用/禁用模块接口，自动处理系统添加/移除（对应 `World::RemoveSystem`）。
  - 暴露调试接口给工具链：列出模块状态、依赖关系、帧开销。
  - 允许场景声明所需模块集合，场景激活时自动启用。
  - 管理 `PreFrame` / `PostFrame` 回调队列，支持帧循环扩展。

### 4.5 EventBus

- **特性**：
  - 标准事件族：`events/frame_events.h`（帧循环）、`events/input_events.h`（由 `InputModule` 发布）、`events/scene_events.h`（由 `SceneManager` 发布），可扩展自定义事件放入 `Render::Application::Events` 命名空间。
  - 优先级与过滤器：监听者可声明优先级；过滤器根据事件类型、标签、目标场景等条件提早筛选。
  - 同步/异步派发：关键路径（输入、生命周期）同步；统计/日志类异步（通过线程安全队列）。
  - 与 Blender 风格控制兼容：在 EventBus 中维护操作映射表、快捷键上下文、模式切换。
  - 针对 UI 工具：提供 `EventChannel` 子总线，将事件流向 HUD、材质/Shader 面板、LayerMask 编辑器等。

### 4.6 SceneGraph 扩展

- 在 `World` 的实体与组件基础上，为场景提供：
  - **生命周期钩子**：`SceneNode::OnEnter/OnExit/OnUpdate`，实现在 Scene 内部的子节点控制。
  - **节点类型**：逻辑节点（不渲染）、ECS 节点、UI 节点（映射到 Blender 风格控件）。
  - **热切换策略**：节点可声明 `PersistencePolicy`（例如跨场景保留、临时、按条件销毁）。
  - **资源守护**：节点持有的资源 handle 自动注册到 Scene 的 Manifest，Scene 退出时统一释放。
  - **调试接口**：输出节点树到调试面板，与 Phase 2 的 Renderer HUD/工具链联动。

## 5. 资源生命周期与预加载流程

1. **构建 Manifest**：场景在 `BuildManifest()` 返回必需 (`required`) 与可选 (`optional`) 资源表（模型、纹理、shader、材质、配置数据、动画）。
2. **预加载管线**：
   - `SceneManager` 在 `OnAttach` 后生成 Manifest，并跟踪每帧资源可用性。
   - 可结合 `AsyncResourceLoader` 提交缺失资源的异步任务；完成后由 `SceneManager` 重新检测并进入场景。
   - 事件：`SceneManifestEvent` 提供清单概览，`ScenePreloadProgressEvent` 推送 required/optional 已就绪数量、缺失资源、完成状态。
3. **资源激活**：当必需资源全部就绪时，`SceneManager` 发布 `SceneLifecycleEvent::Entering/Entered` 并调用 Scene::OnEnter；可选资源缺失不会阻塞，可在 HUD 中提示。
4. **释放策略**：场景退出时，根据 Manifest 标记 (`ReleaseOnExit`, `KeepSharedCache`) 调用 `ResourceManager::Release()`；`ResourceCleanupSystem` 继续维护全局未使用资源。
5. **Uniform 同步**：模块或场景在使用 shader 参数时，通过 `AppContext::uniformManager` 写入，禁止直接 GL 调用。

## 6. 帧循环与流程

```
Initialize AppContext
    ↓
SceneManager.PushScene("Boot")
    ↓
[每帧]
    ├─ ProcessPlatformEvents() → EventBus.Dispatch(Input/Window events)
    ├─ SceneManager.PreFrame(frameArgs)
    │     ├─ ModuleRegistry.PreFrame()  // 优先级排序执行
    │     └─ Scene::OnUpdate(frameArgs)
    ├─ ECS World.Update(deltaTime)
    ├─ Renderer.BeginFrame()
    ├─ ModuleRegistry.PostFrame()      // 用于提交统计、UI 同步
    ├─ Renderer.FlushRenderQueue()
    ├─ Renderer.EndFrame()
    └─ SceneManager.PostFrame(frameArgs) // 处理切换、清理
```

- 场景切换时，`SceneManager.PreFrame()` 会检测 TransitionTask，执行资源预加载与生命周期回调。
- `ModuleRegistry.PreFrame` 内部按模块优先级（默认遵循系统优先级）运行，为 HUD/工具提供帧开始状态同步。
- `ModuleRegistry.PostFrame` 用于推送统计数据到 UI、写 Uniform、派发帧结果事件。

## 7. 与现有系统的集成

- **ECS World**：默认每个场景持有独立 `World`；支持共享模块（例如调试 HUD）通过 `ModuleRegistry` 标记 `global = true` 复用。
- **Renderer & RenderQueue**：场景模块在 `OnAttach` 中注册所需渲染系统（Mesh、Sprite、Text、PostProcess）。热切换时维持层级与 LayerMask 配置。
- **UniformManager**：模块统一通过 `AppContext` 写入自定义 Uniform（如 SceneTime、LayerMask 调试开关），确保遵守用户偏好。
- **ResourceManager**：Manifest 结合缓存查询与 `AsyncResourceLoader`（可选）拉起加载，由 `SceneManager` 统一监视并通过进度事件告知工具链。
- **Input/操作约定**：事件总线的 Input Channel 按 Blender 规范解析手势，模块通过订阅接收高层语义事件。
- **工具链**：应用层向 Renderer HUD、材质/Shader 面板、LayerMask 编辑器提供实时数据源；模块状态暴露给调试面板。
  - ✅ ModuleRegistry 模块状态查询接口已实现（GetModuleState、GetAllModuleStates、IsModuleActive、IsModuleRegistered）
  - ✅ MaterialShaderPanelDataSource 已实现，提供材质和着色器信息查询接口
  - ✅ LayerMaskEditorDataSource 已实现，支持LayerMask编辑功能
  - ✅ SceneGraphVisualizerDataSource 已实现，支持场景图可视化
  - ✅ 工具链集成指南文档已完成（Toolchain_Integration_Guide.md）

## 8. 开发计划与里程碑

| 里程碑 | 状态 | 完成度 | 说明 |
|--------|------|--------|------|
| M1: 基础框架 | ✅ 已完成 | 100% | `AppContext`, `Scene`, `SceneManager` 原型完成，支持单场景和多场景栈式管理，生命周期已验证 |
| M2: 模块注册 | ✅ 已完成 | 100% | `ModuleRegistry` 完成，基础模块（CoreRenderModule、InputModule、DebugHUDModule）已实现，ECS 系统自动注册已实现 |
| M3: 资源管线 | ✅ 已完成 | 100% | `SceneResourceManifest` 完成，预加载/释放策略已实现，与 `AsyncResourceLoader` 整合完成，细粒度资源控制已实现 |
| M4: 事件总线 | ✅ 已完成 | 100% | `EventBus` 完成，输入映射、优先级、过滤器已实现，Blender 风格操作映射已实现 |
| M5: 多场景 | ✅ 已完成 | 95% | 场景栈完成，热切换完成，SceneGraph 节点生命周期已实现，Overlay 支持待测试 |
| M6: 工具联动 | ✅ 已完成 | 90% | 工具链集成接口已完成（ModuleRegistry状态查询、MaterialShaderPanelDataSource、LayerMaskEditorDataSource、SceneGraphVisualizerDataSource），UI界面实现待完善 |

**整体进度**: 约 90% 完成（2025-11-22）

**已完成功能**:
- ✅ 核心框架组件（AppContext、Scene、SceneManager、ModuleRegistry、EventBus、SceneGraph、ApplicationHost）
- ✅ 模块实现（CoreRenderModule、InputModule、DebugHUDModule）
- ✅ 资源管理（资源清单系统、预加载机制、资源释放策略）
- ✅ 事件系统（帧事件、场景事件、输入事件、操作事件、手势事件）
- ✅ 场景序列化（SceneSerializer，支持JSON格式）
- ✅ 工具链集成接口（模块状态查询、材质/Shader面板数据源、LayerMask编辑器、场景图可视化）

**测试状态**:
- ✅ 单元测试和压力测试已通过
- ✅ 事件系统测试已通过（53_event_system_test）
- ✅ 模块和HUD测试已通过（54_module_hud_test）
- ✅ 场景序列化测试已通过（55_scene_serialization_test）
- ✅ 多场景热切换压力测试已通过（56_scene_manager_stress_test）
- ✅ 工具链集成测试已通过（57_toolchain_integration_test）

## 9. 测试与验证策略

### 已完成的测试 ✅

- **单元测试**（✅ 已完成）：
  - SceneManager 状态机（Push/Pop/Replace）测试
  - Manifest 资源引用计数测试
  - 资源预加载检测机制测试
  - 资源释放（Scene/Shared 范围区分）测试
  - 必需/可选资源阻塞机制测试
  - 测试文件：`tests/scene_manager_resource_test.cpp`（5个测试用例，全部通过，2.60秒）

- **压力测试**（✅ 已完成）：
  - 批量资源加载性能测试（50/200/500个资源）
  - 快速场景切换性能测试（10次切换）
  - Shared资源复用测试
  - 并发加载压力测试（500个资源）
  - 测试文件：`tests/scene_manager_stress_test.cpp`（5个压力场景，全部通过，13.05秒）

- **集成测试**（✅ 已完成）：
  - 事件系统测试：事件过滤器、Blender风格映射、操作事件、手势事件（`examples/53_event_system_test.cpp`）
  - 模块和HUD测试：CoreRenderModule自动注册、DebugHUDModule统计信息显示（`examples/54_module_hud_test.cpp`）
  - 场景序列化测试：场景保存/加载、ECS组件序列化（`examples/55_scene_serialization_test.cpp`）
  - 多场景热切换压力测试：快速切换、场景栈压力、长时间运行（`examples/56_scene_manager_stress_test.cpp`）
  - 工具链集成测试：模块状态查询、材质/Shader面板、LayerMask编辑器、场景图可视化（`examples/57_toolchain_integration_test.cpp`）

- **性能测试**（✅ 部分完成）：
  - 多模块 PreFrame/PostFrame 开销已验证（通过DebugHUDModule显示统计信息）
  - 资源预加载批量任务限制已验证（通过配置maxTasksPerFrame）
  - 场景切换性能基准已建立（通过压力测试验证）

### 待补充的测试 🔄

- ModuleRegistry 拓扑排序单元测试
- 事件优先级冲突测试
- 帧循环开销详细分析
- Overlay 场景支持测试

## 10. 风险与应对

### 已解决的风险 ✅

- ✅ **资源阻塞**：已通过 Manifest 分批加载、配置 `maxTasksPerFrame` 解决，压力测试验证通过。
- ✅ **场景切换资源泄露**：已实现细粒度资源释放控制（ResourceScope区分），SceneManager 中已添加资源清理逻辑，测试验证通过。
- ✅ **输入冲突**：已统一接管输入事件，通过 EventBus 和 InputModule 实现 Blender 风格操作映射，底层系统改为订阅应用层语义事件。

### 潜在风险与缓解措施

- **模块依赖循环**：ModuleRegistry 拓扑排序失败 → 在构建期检测并输出诊断图；提供调试面板（ModuleRegistry状态查询接口已实现）。
- **调试开销**：EventBus 与 ModuleRegistry 的统计影响性能 → 提供可配置开关与 `Debug` 模式编译开关（DebugHUDModule 已实现统计信息显示，可通过配置控制）。
- **资源预加载阻塞帧循环**：批量资源加载可能导致帧时间波动 → 已配置 `maxTasksPerFrame` 限制，分批加载，压力测试验证性能可接受。
- **事件总线性能瓶颈**：大量事件订阅可能导致锁竞争 → 考虑无锁数据结构或事件通道分离（当前性能测试显示影响可接受）。

## 11. 后续工作与文档

### 已完成的文档 ✅

- ✅ `docs/application/Module_Guide.md`：模块开发指南（已完成，2025-11-19）
- ✅ `docs/application/EventBus_Guide.md`：事件总线使用指南（已完成，2025-11-19）
- ✅ `docs/application/Toolchain_Integration_Guide.md`：工具链集成指南（已完成，2025-11-21）
- ✅ `docs/application/SceneGraph_Node_Guide.md`：SceneGraph节点开发手册
- ✅ `docs/application/APPLICATION_LAYER_PROGRESS_REPORT.md`：应用层开发进度汇报（持续更新）

### 待完善的文档 🔄

- 🔄 `docs/application/Scene_API.md`：场景接口详细说明（部分内容已在进度报告中）
- 🔄 API 文档更新：更新核心API文档以反映应用层接口

### 已完成的构建配置 ✅

- ✅ `CMakeLists.txt`：已纳入所有 `src/application`、`include/render/application` 目录和文件
- ✅ 工具链集成接口已纳入构建系统

### 已完成的示例 ✅

- ✅ `BootScene`：完整示例场景，展示事件订阅、实体创建、状态快照、SceneGraph使用
- ✅ `examples/53_event_system_test.cpp`：事件系统完整示例
- ✅ `examples/54_module_hud_test.cpp`：模块和HUD完整示例
- ✅ `examples/55_scene_serialization_test.cpp`：场景序列化完整示例
- ✅ `examples/56_scene_manager_stress_test.cpp`：多场景热切换压力测试示例
- ✅ `examples/57_toolchain_integration_test.cpp`：工具链集成完整示例

### 待补充的示例 🔄

- 🔄 `SimpleSceneDemo`：多相机场景示例
- 🔄 `DebugOverlayScene`：HUD Overlay 场景示例（Overlay支持待测试）
- 🔄 `ResourceStressTestScene`：预加载压力测试场景（已有压力测试程序，可扩展为独立场景）

### 后续开发重点

1. **完善工具链集成**：实现UI界面，将数据源接口与可视化工具连接
2. **Overlay场景支持**：完善多场景共存机制，支持调试HUD等Overlay场景
3. **性能优化**：模块PreFrame/PostFrame开销分析，事件总线性能优化
4. **测试覆盖**：补充ModuleRegistry拓扑排序、事件优先级冲突等单元测试

## 12. 已落地代码结构快照（2025-11-21）

### 核心框架组件

- 头文件：`include/render/application/`
  - `app_context.h`：统一保存渲染器、UniformManager、资源管理器、异步加载器等句柄，并提供校验函数。
  - `scene_types.h`：收敛场景生命周期数据结构（Manifest、Snapshot、Enter/Exit 参数、Flags）。
  - `scene.h`：应用层场景抽象基类，定义 OnAttach/OnEnter/OnUpdate/OnExit 等接口。
  - `scene_manager.h`：维护场景工厂、栈式切换、快照传递和生命周期钩子，支持资源预加载和细粒度释放控制。
  - `scene_graph.h`：SceneNode / SceneGraph 实现，支持节点生命周期、资源声明、节点树更新。
  - `app_module.h`、`module_registry.h`：模块基类与注册表，支持依赖检查、PreFrame/PostFrame 调用、模块状态查询接口。
  - `event_bus.h`、`events/frame_events.h`、`events/scene_events.h`、`events/input_events.h`：轻量事件总线封装与统一事件定义，支持事件过滤器（EventFilter、TagEventFilter、SceneEventFilter、CompositeEventFilter）。
  - `application_host.h`：集中调度 World、SceneManager、ModuleRegistry、EventBus，提供初始化与帧循环入口。
  - `scene_serializer.h`：场景序列化器，支持JSON格式的场景保存和加载。

### 模块实现

- `modules/core_render_module.h/cpp`：核心渲染模块，自动注册ECS组件和渲染系统，支持异步加载任务限制配置。
- `modules/input_module.h/cpp`：输入模块，支持SDL事件处理、Blender风格操作映射（OperationMappingManager、ShortcutContext）、手势检测、操作事件发布。
- `modules/debug_hud_module.h/cpp`：调试HUD模块，支持FPS显示、完整统计信息（性能、渲染、资源、内存）、渲染层级可视化、Uniform/材质状态检查。

### 工具链集成接口

- `toolchain/material_shader_panel.h/cpp`：材质/Shader面板数据源，提供MaterialInfo、ShaderInfo查询和材质属性更新接口。
- `toolchain/layermask_editor.h/cpp`：LayerMask编辑器数据源，支持LayerMask与层级列表转换、LayerMask操作。
- `toolchain/scene_graph_visualizer.h/cpp`：场景图可视化工具数据源，支持节点信息查询、树形结构输出、统计信息收集。

### 示例场景

- `scenes/boot_scene.h/cpp`：示例场景，展示事件订阅、实体创建与状态快照，基于 SceneGraph 实现。

### 源文件结构

- 源文件：`src/application/`
  - 核心框架实现：`app_context.cpp`、`application_host.cpp`、`scene_types.cpp`、`scene_manager.cpp`、`scene_graph.cpp`、`app_module.cpp`、`module_registry.cpp`、`event_bus.cpp`
  - 模块实现：`modules/core_render_module.cpp`、`modules/input_module.cpp`、`modules/debug_hud_module.cpp`
  - 工具链集成：`toolchain/material_shader_panel.cpp`、`toolchain/layermask_editor.cpp`、`toolchain/scene_graph_visualizer.cpp`
  - 场景序列化：`scene_serializer.cpp`
  - 示例场景：`scenes/boot_scene.cpp`

### 测试程序

- `examples/53_event_system_test.cpp`：事件系统测试（事件过滤器、Blender风格映射、操作事件、手势事件）
- `examples/54_module_hud_test.cpp`：模块和HUD测试（CoreRenderModule自动注册、DebugHUDModule统计信息显示）
- `examples/55_scene_serialization_test.cpp`：场景序列化测试（场景保存/加载、ECS组件序列化）
- `examples/56_scene_manager_stress_test.cpp`：多场景热切换压力测试（快速切换、场景栈压力、长时间运行）
- `examples/57_toolchain_integration_test.cpp`：工具链集成测试（模块状态查询、材质/Shader面板、LayerMask编辑器、场景图可视化）

### 文档

- `docs/application/APPLICATION_LAYER_PROGRESS_REPORT.md`：应用层开发进度汇报
- `docs/application/EventBus_Guide.md`：事件总线使用指南
- `docs/application/Module_Guide.md`：模块开发指南
- `docs/application/Toolchain_Integration_Guide.md`：工具链集成指南
- `docs/application/SceneGraph_Node_Guide.md`：SceneGraph节点开发手册

### 构建脚本

- `CMakeLists.txt`：已将上述所有目录和文件纳入 `RENDER_SOURCES` 与 `RENDER_HEADERS`，确保编译。

---

**参考**：`docs/api/ECS.md`、`docs/api/System.md`、`docs/api/World.md`、`docs/HUD_DEVELOPMENT_PLAN.md` 提供的当前系统约束与 Phase2 工具链需求。

