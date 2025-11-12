[返回文档首页](../README.md)

# Module Guide

---

## 1. 模块系统概览

应用层模块（`AppModule`）用于封装跨场景、跨系统的功能，如输入处理、调试 HUD、UI Runtime 等，主要目标：

- 将核心逻辑封装为独立单元，可按需启停；
- 明确依赖关系，避免模块调度顺序混乱；
- 贯穿帧循环（PreFrame/PostFrame），方便接入输入、统计等逻辑；
- 支持运行时状态查询与调试。

关键类：

| 类/接口 | 说明 |
|---------|------|
| `AppModule` | 模块基类，定义生命周期回调与依赖、优先级接口 |
| `ModuleRegistry` | 模块注册表，管理模块实例、依赖检查、排序与执行 |
| `CoreRenderModule` | 示例：驱动帧事件广播（FrameTickEvent） |
| `InputModule` | 示例：处理 SDL 输入事件并广播至 EventBus |
| `DebugHUDModule` | 示例：输出 FPS / draw call 等统计信息 |

---

## 2. `AppModule` 结构

```cpp
class AppModule {
public:
    virtual ~AppModule() = default;

    virtual std::string_view Name() const = 0;
    virtual ModuleDependencies Dependencies() const { return {}; }
    virtual int Priority(ModulePhase phase) const { return 0; }

    virtual void OnRegister(ECS::World& world, AppContext& ctx) = 0;
    virtual void OnUnregister(ECS::World& world, AppContext& ctx);

    virtual void OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx);
    virtual void OnPostFrame(const FrameUpdateArgs& frame, AppContext& ctx);
};
```

### 生命周期说明

| 回调 | 调用时机 | 用途 |
|------|----------|------|
| `OnRegister` | 模块激活时，传入 `World` 与 `AppContext` | 初始化资源、注册系统或事件 |
| `OnUnregister` | 模块停用时 | 清理资源、注销事件或系统 |
| `OnPreFrame` | 每帧渲染前，按优先级排序 | 处理输入、更新状态、帧前准备 |
| `OnPostFrame` | 每帧渲染后 | 提交统计、刷新 HUD、写回日志等 |

### 依赖与优先级

- `Dependencies()`：返回模块名称列表，`ModuleRegistry` 会递归检查是否存在缺失依赖；
- `Priority(ModulePhase)`：为不同阶段提供排序，数值越大越靠前执行；
- `ModulePhase` 包括 `Register`（暂未使用）、`PreFrame`、`PostFrame`。

---

## 3. `ModuleRegistry` 功能

### 主要接口

```cpp
void Initialize(ECS::World* world, AppContext* ctx);
bool RegisterModule(std::unique_ptr<AppModule> module, bool activateImmediately = true);
void UnregisterModule(std::string_view name);
bool ActivateModule(std::string_view name);
void DeactivateModule(std::string_view name);
void InvokePhase(ModulePhase phase, const FrameUpdateArgs& frameArgs);
```

### 注册流程

1. `RegisterModule`：注册模块但不开启（`activateImmediately=false` 时），会检查依赖存在性；
2. `ActivateModule`：
   - 调用 `OnRegister`；
   - 标记 `active = true`；
   - 重新排序 PreFrame/PostFrame 队列；
3. `DeactivateModule`：
   - 调用 `OnUnregister`；
   - 从活跃列表移除。

### 依赖解析

`ResolveDependencies` 会递归遍历依赖链并记录缺失模块；如果存在遗漏，`CanRegister` 会记录警告并阻止注册。日志形如下：

```
[W] ModuleRegistry::CanRegister – unresolved dependencies for module 'UIRuntimeModule': CoreRenderModule, InputModule
```

---

## 4. 示例模块解析

### 4.1 CoreRenderModule

职责：保证 `World` 初始化、广播帧事件（Begin/Tick/End）。

```cpp
FrameBeginEvent begin{.frame = frameArgs};
ctx.globalEventBus->Publish(begin);

FrameTickEvent tick{.frame = frameArgs};
ctx.globalEventBus->Publish(tick);
```

- `OnPostFrame` 额外广播 `FrameEndEvent`，便于 HUD、统计模块订阅帧尾信息；
- `Priority(ModulePhase::PreFrame) = 100`：确保在输入模块之前执行；
- 对缺失 UniformManager/Renderer 给出警告，便于开发者补齐。

### 4.2 InputModule

职责：处理 SDL 输入事件。主要逻辑：

- 监听 `SDL_EVENT_KEY_*` / `SDL_EVENT_MOUSE_*`；
- 维护 `m_keysDown`、`m_keysPressed`、`m_keysReleased`；
- 通过 EventBus 发布 `KeyEvent`、`MouseButtonEvent` 等结构。

`Priority(ModulePhase::PreFrame) = 200`，确保在核心模块之后立即处理输入。

### 4.3 DebugHUDModule

职责：输出 FPS / draw call 等统计信息。

- `Priority(ModulePhase::PostFrame) = -50`，在 PostFrame 阶段靠后执行；
- 内部维护平滑 FPS（带指数平滑因子 0.1）；
- 目前通过日志输出，可与 UI Runtime 或 HUD 系统对接以渲染到屏幕。

---

## 5. 模块与场景协同

- 场景可在 `OnAttach` 中调用 `ModuleRegistry::ActivateModule` 或 `DeactivateModule`；
- 模块依赖的系统（例如 `InputModule` 依赖 `CoreRenderModule`）将在场景激活顺序前确保就绪；
- `ApplicationHost` 初始化时默认不会激活所有模块，开发者可根据场景需求决定组合。

示例（BootScene）：

```cpp
void BootScene::OnAttach(AppContext& ctx, ModuleRegistry& modules) {
    modules.ActivateModule("CoreRenderModule");
    modules.ActivateModule("InputModule");
    modules.ActivateModule("DebugHUDModule");
    // ...
}
```

> 建议：在 OnDetach 中对应停用模块或恢复默认状态，避免跨场景残留。

---

## 6. 监视与调试

当前 `ModuleRegistry` 提供：

- `ForEachModule(std::function<void(const AppModule&)>)`：遍历所有模块；
- `GetModule(name)`：查询模块实例；
- 日志信息：
  - 注册/激活/卸载状态；
  - 依赖缺失告警；
  - 排序之后的执行顺序。

后续可根据需求扩展调试接口，例如导出模块状态 JSON、集成到 HUD 面板。

---

## 7. 常见问题与建议

1. **模块依赖循环**：若两个模块互相依赖，`ResolveDependencies` 将递归调用导致无限循环，需要开发时手动避免。推荐结构：
   - 核心模块提供事件或服务；
   - 其他模块订阅 / 使用，但不要双向依赖。

2. **资源清理**：在 `OnUnregister` 中务必撤销事件订阅、关闭线程或释放资源，防止模块停用时残留状态。

3. **场景与模块分层**：场景只组织业务逻辑，模块处理跨场景功能；组件（component/system）尽量放在 `ECS` 层，避免模块中对具体场景逻辑的强耦合。

4. **EventBus 多线程**：当前示例在主线程中发布事件，若后续有线程池需求，需要在模块内部保证线程安全。

---

## 8. 参考与示例

- 源码：
  - `include/render/application/modules/core_render_module.h`
  - `include/render/application/modules/input_module.h`
  - `include/render/application/modules/debug_hud_module.h`
  - `src/application/modules/*.cpp`
  - `src/application/module_registry.cpp`
- 文档：
  - `docs/application/Scene_API.md`
  - `docs/SCENE_MODULE_FRAMEWORK.md`
  - `docs/application/EventBus_Guide.md`

---

如需添加新模块，可参考输入/HUD 模块骨架，在 `CMakeLists.txt` 中注册源文件，并在场景或引导逻辑中按需激活。欢迎根据业务拆分更多模块，如 `ToolingModule`、`HotReloadModule` 等，保持应用层结构清晰可扩展。*** End Patch

