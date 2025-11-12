[返回文档首页](../README.md)

# EventBus Guide

---

## 1. 概述

应用层 EventBus 为跨模块、跨场景通信提供统一通道，所有事件需继承 `EventBase`。本指南汇总内置事件族、推荐实践及订阅示例。

---

## 2. 事件分类总览

- **帧循环事件**（`events/frame_events.h`）
  - `FrameBeginEvent`：`CoreRenderModule::OnPreFrame` 触发，表示帧逻辑开始。
  - `FrameTickEvent`：同帧阶段触发，兼容旧接口。
  - `FrameEndEvent`：`CoreRenderModule::OnPostFrame` 触发，用于帧尾统计与同步。
- **输入事件**（`events/input_events.h`，由 `InputModule` 发布）
  - `KeyEvent`、`MouseButtonEvent`、`MouseMotionEvent`、`MouseWheelEvent`、`TextInputEvent`。
- **场景事件**（`events/scene_events.h`，由 `SceneManager` 发布）
  - `SceneTransitionEvent`：Push/Replace/Pop 请求。
  - `SceneLifecycleEvent`：Attach/Enter/Exit/Detach 等生命周期钩子。
  - `SceneManifestEvent`：场景构建资源清单时触发，便于预加载监控。
  - `ScenePreloadProgressEvent`：输出 required/optional 已加载数量、缺失资源与完成状态，可驱动 HUD 加载条。

---

## 3. 订阅与注销

```cpp
auto& eventBus = host.GetEventBus();
auto frameListener = eventBus.Subscribe<FrameBeginEvent>(
    [](const FrameBeginEvent& evt) {
        Logger::GetInstance().Info("Frame {}", evt.frame.frameIndex);
    });

// 结束时记得解绑
eventBus.Unsubscribe(frameListener);
```

- `EventBus::Subscribe` 返回 `ListenerId`，需在退出时调用 `Unsubscribe`；
- `EventBus::Clear()` 会在 `ApplicationHost::Shutdown` 执行，无需额外处理但谨防悬垂指针；
- 若需要优先级，可传入第三个参数（数值越大越先处理）。

---

## 4. 发布自定义事件

```cpp
struct GameplayEvent final : public EventBase {
    int scoreDelta = 0;
};

GameplayEvent evt{.scoreDelta = 10};
eventBus.Publish(evt);
```

建议将自定义事件放入独立头文件并置于 `Render::Application::Events` 命名空间，以保持一致。

---

## 5. 场景与模块范式

- **模块**：在 `OnRegister` 或 `OnPreFrame` 订阅事件，在 `OnUnregister` 注销。
- **场景/节点**：可在 `OnEnter` 订阅，`OnExit` 解除，避免跨场景泄漏。
- **调试工具**：`DebugHUDModule` 可监听 `FrameEndEvent` 收集统计；资源监控模块可监听 `SceneManifestEvent`。

---

## 6. 示例参考

- 示例程序：`examples/52_application_boot_demo.cpp` 展示了帧事件与场景生命周期的订阅。
- 自动化测试：`tests/scene_graph_demo_test.cpp` 可扩展监听事件验证节点行为。

---

通过统一的事件命名和传播流程，应用层可快速扩展输入、资源、UI 等跨系统交互逻辑，并方便未来将事件接入调试/监控工具链。*** End Patch

