# EventBus API 文档

## 目录
[返回API文档首页](README.md)

---

## 概述

`EventBus` 是应用层的统一事件系统，提供类型安全的事件订阅和发布机制。它支持优先级、事件过滤和线程安全。

**命名空间**: `Render::Application`

**完整使用指南**: [事件总线使用指南](../application/EventBus_Guide.md)

---

## 类定义

```cpp
class EventBus {
public:
    using ListenerId = uint64_t;

    EventBus();

    template <typename EventT>
    ListenerId Subscribe(
        std::function<void(const EventT&)> handler,
        int priority = 0,
        std::shared_ptr<EventFilter> filter = nullptr
    );

    template <typename EventT>
    void Unsubscribe(ListenerId listenerId);

    template <typename EventT>
    void Publish(const EventT& event);

    void Clear();
};
```

---

## 核心功能

### 1. 订阅事件

#### `Subscribe<EventT>(handler, priority, filter)`

订阅指定类型的事件。

**模板参数**:
- `EventT`: 事件类型（必须继承自 `EventBase`）

**参数**:
- `handler`: 事件处理函数（`std::function<void(const EventT&)>`）
- `priority`: 优先级（默认0，数值越大优先级越高）
- `filter`: 事件过滤器（可选）

**返回值**: 监听者ID（`ListenerId`），用于取消订阅

**示例**:
```cpp
auto& eventBus = host.GetEventBus();

// 基本订阅
auto id1 = eventBus.Subscribe<FrameBeginEvent>(
    [](const FrameBeginEvent& event) {
        LOG_INFO("Frame begin");
    }
);

// 带优先级的订阅
auto id2 = eventBus.Subscribe<FrameBeginEvent>(
    [](const FrameBeginEvent& event) {
        LOG_INFO("High priority handler");
    },
    100  // 高优先级
);

// 带过滤器的订阅
auto filter = std::make_shared<TagEventFilter>("debug");
auto id3 = eventBus.Subscribe<FrameBeginEvent>(
    [](const FrameBeginEvent& event) {
        LOG_INFO("Debug handler");
    },
    0,
    filter
);
```

### 2. 取消订阅

#### `Unsubscribe<EventT>(listenerId)`

取消订阅指定类型的事件。

**参数**:
- `listenerId`: 监听者ID

**示例**:
```cpp
auto id = eventBus.Subscribe<FrameBeginEvent>(handler);
// ... 使用中 ...
eventBus.Unsubscribe<FrameBeginEvent>(id);
```

### 3. 发布事件

#### `Publish<EventT>(event)`

发布事件，通知所有订阅者。

**参数**:
- `event`: 事件对象（const引用）

**注意**: 事件会按优先级排序，高优先级的处理函数先执行。

**示例**:
```cpp
FrameBeginEvent event{};
event.deltaTime = 0.016f;
eventBus.Publish(event);
```

### 4. 清空所有订阅

#### `Clear()`

清空所有事件订阅。

**注意**: 通常在系统关闭时调用。

---

## 事件过滤器

### EventFilter 接口

```cpp
class EventFilter {
public:
    virtual ~EventFilter() = default;
    virtual bool ShouldReceive(const EventBase& event) const = 0;
};
```

### 内置过滤器

#### TagEventFilter

只接收包含指定标签的事件。

```cpp
auto filter = std::make_shared<TagEventFilter>("debug");
auto id = eventBus.Subscribe<MyEvent>(handler, 0, filter);
```

#### SceneEventFilter

只接收目标场景的事件。

```cpp
auto filter = std::make_shared<SceneEventFilter>("BootScene");
auto id = eventBus.Subscribe<MyEvent>(handler, 0, filter);
```

#### CompositeEventFilter

组合多个过滤器（AND逻辑）。

```cpp
auto composite = std::make_shared<CompositeEventFilter>();
composite->AddFilter(std::make_shared<TagEventFilter>("debug"));
composite->AddFilter(std::make_shared<SceneEventFilter>("BootScene"));
auto id = eventBus.Subscribe<MyEvent>(handler, 0, composite);
```

---

## 标准事件类型

### 帧事件（frame_events.h）

- `FrameBeginEvent`: 帧开始
- `FrameTickEvent`: 帧更新
- `FrameEndEvent`: 帧结束

### 场景事件（scene_events.h）

- `SceneTransitionEvent`: 场景切换
- `SceneLifecycleEvent`: 场景生命周期
- `SceneManifestEvent`: 场景清单
- `ScenePreloadProgressEvent`: 资源预加载进度

### 输入事件（input_events.h）

- `KeyEvent`: 键盘事件
- `MouseButtonEvent`: 鼠标按钮事件
- `MouseMotionEvent`: 鼠标移动事件
- `OperationEvent`: 操作事件
- `GestureEvent`: 手势事件

---

## 使用示例

### 在模块中订阅事件

```cpp
class MyModule : public AppModule {
public:
    void OnRegister(ECS::World& world, AppContext& ctx) override {
        auto& eventBus = *ctx.globalEventBus;
        
        // 订阅帧事件
        m_frameListenerId = eventBus.Subscribe<FrameBeginEvent>(
            [this](const FrameBeginEvent& event) {
                OnFrameBegin(event);
            },
            100  // 高优先级
        );
        
        // 订阅场景事件
        m_sceneListenerId = eventBus.Subscribe<SceneLifecycleEvent>(
            [this](const SceneLifecycleEvent& event) {
                OnSceneLifecycle(event);
            }
        );
    }
    
    void OnUnregister(ECS::World& world, AppContext& ctx) override {
        auto& eventBus = *ctx.globalEventBus;
        
        // 取消订阅
        eventBus.Unsubscribe<FrameBeginEvent>(m_frameListenerId);
        eventBus.Unsubscribe<SceneLifecycleEvent>(m_sceneListenerId);
    }
    
private:
    EventBus::ListenerId m_frameListenerId = 0;
    EventBus::ListenerId m_sceneListenerId = 0;
};
```

### 发布自定义事件

```cpp
// 定义事件
struct MyCustomEvent : public EventBase {
    std::string message;
    int value = 0;
};

// 发布事件
MyCustomEvent event{};
event.message = "Hello, World!";
event.value = 42;
event.AddTag("custom");
eventBus.Publish(event);
```

---

## 注意事项

1. **优先级**: 数值越大优先级越高，高优先级的处理函数先执行
2. **线程安全**: EventBus 使用互斥锁保护，支持多线程环境
3. **性能**: 频繁发布事件可能影响性能，建议谨慎使用
4. **生命周期**: 确保在对象销毁前取消订阅，避免悬空指针

---

## 相关文档

- [事件总线使用指南](../application/EventBus_Guide.md) - 详细的使用指南和示例
- [ApplicationHost API](ApplicationHost.md) - 应用宿主API
- [AppContext API](AppContext.md) - 应用上下文API

---

[返回API文档首页](README.md)

