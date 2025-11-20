[返回文档首页](../README.md)

---

# 事件总线使用指南

**更新时间**: 2025-11-19  
**参考文档**: [SCENE_MODULE_FRAMEWORK.md](../SCENE_MODULE_FRAMEWORK.md)

---

## 1. 概述

`EventBus` 是应用层的统一事件系统，提供类型安全的事件订阅和发布机制。它支持：

- **类型安全**：编译时类型检查，避免事件类型错误
- **优先级支持**：高优先级监听者先执行
- **事件过滤**：支持按标签、场景等条件过滤事件
- **线程安全**：使用互斥锁保护，支持多线程环境

## 2. 基本使用

### 2.1 获取 EventBus 实例

EventBus 通常通过 `AppContext` 访问：

```cpp
#include "render/application/app_context.h"
#include "render/application/event_bus.h"

void MyModule::OnRegister(ECS::World& world, AppContext& ctx) {
    // 通过 AppContext 获取 EventBus
    EventBus* eventBus = ctx.globalEventBus;
    
    // 订阅事件
    m_listenerId = eventBus->Subscribe<FrameBeginEvent>(
        [this](const FrameBeginEvent& event) {
            OnFrameBegin(event);
        }
    );
}
```

### 2.2 订阅事件

使用 `Subscribe` 方法订阅事件：

```cpp
// 基本订阅
auto listenerId = eventBus->Subscribe<FrameBeginEvent>(
    [](const FrameBeginEvent& event) {
        std::cout << "Frame begin: " << event.frame.deltaTime << std::endl;
    }
);

// 带优先级的订阅（数字越大优先级越高）
auto highPriorityId = eventBus->Subscribe<FrameBeginEvent>(
    [](const FrameBeginEvent& event) {
        // 这个回调会先执行
    },
    100  // 高优先级
);
```

### 2.3 发布事件

使用 `Publish` 方法发布事件：

```cpp
// 创建并发布事件
FrameBeginEvent event;
event.frame.deltaTime = 0.016f;
event.frame.frameIndex = 12345;
eventBus->Publish(event, "MyScene");  // 可选：指定当前场景ID
```

### 2.4 取消订阅

使用 `Unsubscribe` 方法取消订阅：

```cpp
eventBus->Unsubscribe(listenerId);
```

## 3. 事件类型

### 3.1 帧事件

帧事件在每帧的特定阶段发布：

```cpp
#include "render/application/events/frame_events.h"

// FrameBeginEvent - 帧开始
eventBus->Subscribe<FrameBeginEvent>([](const FrameBeginEvent& e) {
    // 在帧开始时执行
});

// FrameTickEvent - 帧更新
eventBus->Subscribe<FrameTickEvent>([](const FrameTickEvent& e) {
    // 在帧更新时执行
});

// FrameEndEvent - 帧结束
eventBus->Subscribe<FrameEndEvent>([](const FrameEndEvent& e) {
    // 在帧结束时执行
});
```

### 3.2 场景事件

场景事件在场景生命周期变化时发布：

```cpp
#include "render/application/events/scene_events.h"

using namespace Render::Application::Events;

// 场景切换事件
eventBus->Subscribe<SceneTransitionEvent>([](const SceneTransitionEvent& e) {
    if (e.type == SceneTransitionEvent::Type::Push) {
        std::cout << "Pushing scene: " << e.sceneId << std::endl;
    }
});

// 场景生命周期事件
eventBus->Subscribe<SceneLifecycleEvent>([](const SceneLifecycleEvent& e) {
    switch (e.stage) {
        case SceneLifecycleEvent::Stage::Entered:
            std::cout << "Scene entered: " << e.sceneId << std::endl;
            break;
        case SceneLifecycleEvent::Stage::Exiting:
            std::cout << "Scene exiting: " << e.sceneId << std::endl;
            break;
        // ...
    }
});

// 资源预加载进度事件
eventBus->Subscribe<ScenePreloadProgressEvent>([](const ScenePreloadProgressEvent& e) {
    float progress = static_cast<float>(e.requiredLoaded) / e.requiredTotal;
    std::cout << "Preload progress: " << progress * 100.0f << "%" << std::endl;
});
```

### 3.3 输入事件

输入事件由 `InputModule` 发布：

```cpp
#include "render/application/events/input_events.h"

using namespace Render::Application::Events;

// 键盘事件
eventBus->Subscribe<KeyEvent>([](const KeyEvent& e) {
    if (e.state == KeyState::Pressed && !e.repeat) {
        std::cout << "Key pressed: " << e.scancode << std::endl;
    }
});

// 鼠标按钮事件
eventBus->Subscribe<MouseButtonEvent>([](const MouseButtonEvent& e) {
    if (e.state == MouseButtonState::Pressed) {
        std::cout << "Mouse button " << static_cast<int>(e.button) 
                  << " pressed at (" << e.x << ", " << e.y << ")" << std::endl;
    }
});

// 鼠标移动事件
eventBus->Subscribe<MouseMotionEvent>([](const MouseMotionEvent& e) {
    // 处理鼠标移动
});

// Blender风格操作事件
eventBus->Subscribe<OperationEvent>([](const OperationEvent& e) {
    switch (e.type) {
        case OperationType::Move:
            std::cout << "Move operation at (" << e.x << ", " << e.y << ")" << std::endl;
            break;
        case OperationType::Rotate:
            std::cout << "Rotate operation" << std::endl;
            break;
        // ...
    }
});

// 手势事件
eventBus->Subscribe<GestureEvent>([](const GestureEvent& e) {
    if (e.type == GestureType::Drag && e.isActive) {
        std::cout << "Dragging from (" << e.startX << ", " << e.startY 
                  << ") to (" << e.currentX << ", " << e.currentY << ")" << std::endl;
    }
});
```

## 4. 事件过滤

EventBus 支持多种事件过滤器，用于精确控制哪些事件被接收。

### 4.1 标签过滤器

只接收包含指定标签的事件：

```cpp
#include "render/application/event_bus.h"

// 创建标签过滤器
auto filter = std::make_shared<TagEventFilter>("debug");

// 订阅时使用过滤器
eventBus->Subscribe<FrameBeginEvent>(
    [](const FrameBeginEvent& e) {
        // 只接收带有 "debug" 标签的事件
    },
    0,  // 优先级
    filter
);

// 发布时添加标签
FrameBeginEvent event;
event.AddTag("debug");
eventBus->Publish(event);
```

### 4.2 场景过滤器

只接收目标场景的事件：

```cpp
// 创建场景过滤器
auto sceneFilter = std::make_shared<SceneEventFilter>("MyScene");

eventBus->Subscribe<SceneLifecycleEvent>(
    [](const SceneLifecycleEvent& e) {
        // 只接收目标场景为 "MyScene" 的事件
    },
    0,
    sceneFilter
);
```

### 4.3 组合过滤器

组合多个过滤器（AND逻辑）：

```cpp
auto compositeFilter = std::make_shared<CompositeEventFilter>();
compositeFilter->AddFilter(std::make_shared<TagEventFilter>("debug"));
compositeFilter->AddFilter(std::make_shared<SceneEventFilter>("MyScene"));

eventBus->Subscribe<FrameBeginEvent>(
    [](const FrameBeginEvent& e) {
        // 只接收同时满足两个条件的事件
    },
    0,
    compositeFilter
);
```

## 5. 事件标签和目标场景

### 5.1 添加标签

事件可以添加标签用于分类和过滤：

```cpp
FrameBeginEvent event;
event.AddTag("debug");
event.AddTag("performance");
eventBus->Publish(event);
```

### 5.2 设置目标场景

事件可以指定目标场景：

```cpp
SceneLifecycleEvent event;
event.sceneId = "MyScene";
event.stage = SceneLifecycleEvent::Stage::Entered;
event.targetSceneId = "MyScene";  // 只有订阅了该场景的监听者会收到
eventBus->Publish(event);
```

## 6. 优先级

监听者可以设置优先级，数字越大优先级越高，高优先级的监听者会先执行：

```cpp
// 低优先级（后执行）
eventBus->Subscribe<FrameBeginEvent>(
    [](const FrameBeginEvent& e) {
        // 后执行
    },
    0  // 低优先级
);

// 高优先级（先执行）
eventBus->Subscribe<FrameBeginEvent>(
    [](const FrameBeginEvent& e) {
        // 先执行
    },
    100  // 高优先级
);
```

## 7. 完整示例

### 7.1 在模块中订阅事件

```cpp
#include "render/application/app_module.h"
#include "render/application/event_bus.h"
#include "render/application/events/frame_events.h"
#include "render/application/events/input_events.h"

class MyModule : public AppModule {
public:
    void OnRegister(ECS::World& world, AppContext& ctx) override {
        m_eventBus = ctx.globalEventBus;
        
        // 订阅帧事件
        m_frameListenerId = m_eventBus->Subscribe<FrameBeginEvent>(
            [this](const FrameBeginEvent& e) {
                OnFrameBegin(e);
            },
            50  // 中等优先级
        );
        
        // 订阅输入事件（只接收调试标签）
        auto debugFilter = std::make_shared<TagEventFilter>("debug");
        m_inputListenerId = m_eventBus->Subscribe<KeyEvent>(
            [this](const KeyEvent& e) {
                OnKeyEvent(e);
            },
            0,
            debugFilter
        );
    }
    
    void OnUnregister(ECS::World& world, AppContext& ctx) override {
        // 取消订阅
        if (m_eventBus) {
            m_eventBus->Unsubscribe(m_frameListenerId);
            m_eventBus->Unsubscribe(m_inputListenerId);
        }
    }
    
private:
    EventBus* m_eventBus = nullptr;
    EventBus::ListenerId m_frameListenerId = 0;
    EventBus::ListenerId m_inputListenerId = 0;
    
    void OnFrameBegin(const FrameBeginEvent& e) {
        // 处理帧开始事件
    }
    
    void OnKeyEvent(const KeyEvent& e) {
        // 处理键盘事件
    }
};
```

### 7.2 在场景中订阅事件

```cpp
#include "render/application/scene.h"
#include "render/application/event_bus.h"
#include "render/application/events/scene_events.h"

class MyScene : public Scene {
public:
    void OnAttach(AppContext& ctx, ModuleRegistry& modules) override {
        m_eventBus = ctx.globalEventBus;
        
        // 订阅场景生命周期事件
        m_lifecycleListenerId = m_eventBus->Subscribe<SceneLifecycleEvent>(
            [this](const SceneLifecycleEvent& e) {
                if (e.sceneId == Name() && e.stage == SceneLifecycleEvent::Stage::Entered) {
                    OnSceneEntered();
                }
            }
        );
    }
    
    void OnDetach() override {
        if (m_eventBus) {
            m_eventBus->Unsubscribe(m_lifecycleListenerId);
        }
    }
    
private:
    EventBus* m_eventBus = nullptr;
    EventBus::ListenerId m_lifecycleListenerId = 0;
    
    void OnSceneEntered() {
        // 场景进入时的处理
    }
};
```

## 8. 最佳实践

### 8.1 及时取消订阅

避免内存泄漏，在模块或场景销毁时取消订阅：

```cpp
void OnUnregister(ECS::World& world, AppContext& ctx) override {
    if (m_eventBus) {
        for (auto id : m_listenerIds) {
            m_eventBus->Unsubscribe(id);
        }
        m_listenerIds.clear();
    }
}
```

### 8.2 使用智能指针管理过滤器

```cpp
class MyModule {
private:
    std::shared_ptr<EventFilter> m_filter;
    
public:
    void OnRegister(ECS::World& world, AppContext& ctx) override {
        // 创建过滤器并保存引用
        m_filter = std::make_shared<TagEventFilter>("my-tag");
        
        m_listenerId = ctx.globalEventBus->Subscribe<FrameBeginEvent>(
            [](const FrameBeginEvent& e) { /* ... */ },
            0,
            m_filter
        );
    }
};
```

### 8.3 避免在事件处理中执行耗时操作

事件处理是同步的，耗时操作会阻塞事件派发：

```cpp
// ❌ 不好：耗时操作
eventBus->Subscribe<FrameBeginEvent>([](const FrameBeginEvent& e) {
    LoadLargeFile();  // 阻塞其他监听者
});

// ✅ 好：异步处理
eventBus->Subscribe<FrameBeginEvent>([](const FrameBeginEvent& e) {
    std::thread([]() {
        LoadLargeFile();  // 在后台线程执行
    }).detach();
});
```

### 8.4 使用事件标签进行分类

```cpp
// 发布调试事件
FrameBeginEvent debugEvent;
debugEvent.AddTag("debug");
debugEvent.AddTag("performance");
eventBus->Publish(debugEvent);

// 只订阅调试事件
auto debugFilter = std::make_shared<TagEventFilter>("debug");
eventBus->Subscribe<FrameBeginEvent>(
    [](const FrameBeginEvent& e) { /* ... */ },
    0,
    debugFilter
);
```

## 9. 常见问题

### Q: 如何确保事件处理顺序？

A: 使用优先级。高优先级的监听者会先执行。如果需要严格的顺序，可以使用不同的优先级值。

### Q: 事件处理是同步还是异步？

A: 同步。`Publish` 方法会同步调用所有匹配的监听者。如果需要异步处理，请在监听者内部启动异步任务。

### Q: 可以在事件处理中发布新事件吗？

A: 可以，但要注意避免无限递归。建议使用事件队列或延迟发布。

### Q: 如何调试事件系统？

A: 可以添加日志记录所有事件的发布和接收：

```cpp
eventBus->Subscribe<FrameBeginEvent>(
    [](const FrameBeginEvent& e) {
        Logger::GetInstance().Debug("FrameBeginEvent received");
    }
);
```

## 10. 参考

- [SCENE_MODULE_FRAMEWORK.md](../SCENE_MODULE_FRAMEWORK.md) - 应用层框架设计
- [APPLICATION_LAYER_PROGRESS_REPORT.md](APPLICATION_LAYER_PROGRESS_REPORT.md) - 应用层开发进度
- `include/render/application/event_bus.h` - EventBus API
- `include/render/application/events/` - 事件类型定义

---

**上一章**: [APPLICATION_LAYER_PROGRESS_REPORT.md](APPLICATION_LAYER_PROGRESS_REPORT.md)  
**下一章**: [SceneGraph_Node_Guide.md](SceneGraph_Node_Guide.md)
