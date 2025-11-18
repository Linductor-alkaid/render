[返回文档首页](../README.md)

---

# 事件系统使用指南

**更新时间**: 2025-11-18  
**参考文档**: [APPLICATION_LAYER_PROGRESS_REPORT.md](APPLICATION_LAYER_PROGRESS_REPORT.md)

---

## 1. 概述

事件系统是应用层框架的核心通信机制，提供了类型安全的事件订阅/发布功能，支持事件过滤、Blender 风格操作映射和手势检测。

### 1.1 核心组件

- **EventBus**: 事件总线，负责事件的订阅和发布
- **EventBase**: 所有事件的基类，支持标签和目标场景过滤
- **EventFilter**: 事件过滤器接口，支持自定义过滤逻辑
- **InputModule**: 输入模块，处理 SDL 事件并发布高层语义事件
- **OperationMappingManager**: 操作映射管理器，实现 Blender 风格快捷键映射

### 1.2 事件类型

| 事件类型 | 说明 | 用途 |
|---------|------|------|
| **帧事件** | FrameBeginEvent、FrameTickEvent、FrameEndEvent | 帧循环生命周期 |
| **场景事件** | SceneTransitionEvent、SceneLifecycleEvent 等 | 场景切换和生命周期 |
| **输入事件** | KeyEvent、MouseButtonEvent、MouseMotionEvent | 原始输入事件 |
| **操作事件** | OperationEvent | Blender 风格操作（Move、Rotate、Scale 等） |
| **手势事件** | GestureEvent | 高级手势（Drag、Pan、Zoom、BoxSelect 等） |

---

## 2. EventBus 基础使用

### 2.1 订阅事件

使用 `EventBus::Subscribe` 方法订阅事件，支持优先级和过滤器：

```cpp
#include "render/application/event_bus.h"
#include "render/application/events/input_events.h"

using namespace Render::Application;
using namespace Render::Application::Events;

// 获取 EventBus 引用
EventBus& eventBus = applicationHost.GetEventBus();

// 基础订阅（无过滤器）
auto listenerId = eventBus.Subscribe<KeyEvent>(
    [](const KeyEvent& event) {
        std::cout << "Key pressed: " << event.scancode << std::endl;
    }
);

// 带优先级的订阅（高优先级先执行）
auto highPriorityId = eventBus.Subscribe<KeyEvent>(
    [](const KeyEvent& event) {
        // 高优先级处理逻辑
    },
    EventPriority::High  // 或 EventPriority::Normal、EventPriority::Low
);
```

### 2.2 发布事件

使用 `EventBus::Publish` 方法发布事件：

```cpp
// 创建并发布事件
KeyEvent keyEvent;
keyEvent.scancode = SDL_SCANCODE_SPACE;
keyEvent.state = KeyState::Pressed;
keyEvent.repeat = false;

eventBus.Publish(keyEvent);
```

### 2.3 取消订阅

使用 `EventBus::Unsubscribe` 方法取消订阅：

```cpp
eventBus.Unsubscribe<KeyEvent>(listenerId);
```

---

## 3. 事件过滤器

事件过滤器允许订阅者只接收符合特定条件的事件，减少不必要的回调执行。

### 3.1 内置过滤器

#### DefaultEventFilter（默认过滤器）

接收所有事件（默认行为）：

```cpp
auto filter = std::make_shared<DefaultEventFilter>();
auto listenerId = eventBus.Subscribe<KeyEvent>(
    [](const KeyEvent& event) { /* ... */ },
    EventPriority::Normal,
    filter
);
```

#### TagEventFilter（标签过滤器）

只接收包含指定标签的事件：

```cpp
// 创建标签过滤器
auto tagFilter = std::make_shared<TagEventFilter>("ui");

// 订阅时使用过滤器
auto listenerId = eventBus.Subscribe<KeyEvent>(
    [](const KeyEvent& event) {
        std::cout << "Received UI key event" << std::endl;
    },
    EventPriority::Normal,
    tagFilter
);

// 发布带标签的事件
KeyEvent keyEvent;
keyEvent.AddTag("ui");
keyEvent.scancode = SDL_SCANCODE_ESCAPE;
eventBus.Publish(keyEvent);  // 只有使用 tagFilter 的订阅者会收到
```

#### SceneEventFilter（场景过滤器）

只接收目标场景的事件：

```cpp
// 创建场景过滤器
auto sceneFilter = std::make_shared<SceneEventFilter>("MainScene");

// 订阅时使用过滤器
auto listenerId = eventBus.Subscribe<KeyEvent>(
    [](const KeyEvent& event) {
        std::cout << "Received MainScene key event" << std::endl;
    },
    EventPriority::Normal,
    sceneFilter
);

// 发布目标场景的事件
KeyEvent keyEvent;
keyEvent.targetSceneId = "MainScene";
keyEvent.scancode = SDL_SCANCODE_RETURN;
eventBus.Publish(keyEvent);  // 只有使用 sceneFilter 的订阅者会收到
```

#### CompositeEventFilter（组合过滤器）

组合多个过滤器（AND 逻辑）：

```cpp
// 创建组合过滤器
auto compositeFilter = std::make_shared<CompositeEventFilter>();
compositeFilter->AddFilter(std::make_shared<TagEventFilter>("ui"));
compositeFilter->AddFilter(std::make_shared<SceneEventFilter>("MainScene"));

// 订阅时使用组合过滤器
auto listenerId = eventBus.Subscribe<KeyEvent>(
    [](const KeyEvent& event) {
        // 只接收同时满足标签和场景条件的事件
    },
    EventPriority::Normal,
    compositeFilter
);
```

### 3.2 自定义过滤器

实现 `EventFilter` 接口创建自定义过滤器：

```cpp
class CustomEventFilter : public EventFilter {
public:
    explicit CustomEventFilter(int minScancode) : m_minScancode(minScancode) {}
    
    bool ShouldReceive(const EventBase& event) const override {
        // 类型检查
        if (auto* keyEvent = dynamic_cast<const KeyEvent*>(&event)) {
            return keyEvent->scancode >= m_minScancode;
        }
        return false;
    }
    
private:
    int m_minScancode;
};

// 使用自定义过滤器
auto customFilter = std::make_shared<CustomEventFilter>(SDL_SCANCODE_A);
auto listenerId = eventBus.Subscribe<KeyEvent>(
    [](const KeyEvent& event) { /* ... */ },
    EventPriority::Normal,
    customFilter
);
```

---

## 4. 输入事件系统

### 4.1 基础输入事件

#### KeyEvent（键盘事件）

```cpp
struct KeyEvent : public EventBase {
    int scancode;           // SDL 扫描码
    KeyState state;          // Pressed 或 Released
    bool repeat;             // 是否为重复按键
    bool ctrl;               // Ctrl 键状态
    bool shift;              // Shift 键状态
    bool alt;                // Alt 键状态
};

// 订阅键盘事件
eventBus.Subscribe<KeyEvent>([](const KeyEvent& event) {
    if (event.state == KeyState::Pressed && !event.repeat) {
        std::cout << "Key pressed: " << event.scancode << std::endl;
    }
});
```

#### MouseButtonEvent（鼠标按钮事件）

```cpp
struct MouseButtonEvent : public EventBase {
    MouseButton button;           // Left、Right、Middle 等
    MouseButtonState state;        // Pressed 或 Released
    float x, y;                    // 鼠标位置
    bool ctrl, shift, alt;         // 修饰键状态
};

// 订阅鼠标按钮事件
eventBus.Subscribe<MouseButtonEvent>([](const MouseButtonEvent& event) {
    if (event.state == MouseButtonState::Pressed) {
        std::cout << "Mouse clicked at (" << event.x << ", " << event.y << ")" << std::endl;
    }
});
```

#### MouseMotionEvent（鼠标移动事件）

```cpp
struct MouseMotionEvent : public EventBase {
    float x, y;                    // 当前鼠标位置
    float deltaX, deltaY;          // 相对移动量
    bool ctrl, shift, alt;         // 修饰键状态
};

// 订阅鼠标移动事件
eventBus.Subscribe<MouseMotionEvent>([](const MouseMotionEvent& event) {
    std::cout << "Mouse moved: (" << event.deltaX << ", " << event.deltaY << ")" << std::endl;
});
```

### 4.2 InputModule 集成

`InputModule` 自动处理 SDL 事件并发布高层事件：

```cpp
#include "render/application/modules/input_module.h"

// 注册 InputModule（通常在 ApplicationHost 初始化时完成）
ApplicationHost host;
host.GetModuleRegistry().RegisterModule(std::make_shared<InputModule>());

// InputModule 会自动发布 KeyEvent、MouseButtonEvent 等事件
// 只需订阅即可接收
```

---

## 5. 操作映射系统

操作映射系统实现了 Blender 风格的快捷键映射，将底层输入转换为高层语义操作。

### 5.1 OperationType（操作类型）

```cpp
enum class OperationType {
    Select,      // 选择
    Add,          // 添加
    Delete,       // 删除
    Move,         // 移动（G）
    Rotate,       // 旋转（R）
    Scale,        // 缩放（S）
    Duplicate,    // 复制（Shift+D）
    Cancel,       // 取消（ESC）
    Confirm       // 确认（Enter）
};
```

### 5.2 OperationEvent（操作事件）

```cpp
struct OperationEvent : public EventBase {
    OperationType type;     // 操作类型
    float x, y;             // 操作位置
    bool isStart;           // 是否为操作开始
    std::string context;    // 操作上下文（如 "ObjectMode"、"EditMode"）
};

// 订阅操作事件
eventBus.Subscribe<OperationEvent>([](const OperationEvent& event) {
    switch (event.type) {
        case OperationType::Move:
            std::cout << "Move operation " << (event.isStart ? "started" : "ended") << std::endl;
            break;
        case OperationType::Rotate:
            std::cout << "Rotate operation " << (event.isStart ? "started" : "ended") << std::endl;
            break;
        // ...
    }
});
```

### 5.3 ShortcutContext（快捷键上下文）

不同上下文可以有不同的快捷键映射：

```cpp
#include "render/application/operation_mapping.h"

// 创建上下文
auto objectModeContext = std::make_shared<ShortcutContext>("ObjectMode");
objectModeContext->AddMapping({SDL_SCANCODE_G, false, false, false}, OperationType::Move);
objectModeContext->AddMapping({SDL_SCANCODE_R, false, false, false}, OperationType::Rotate);
objectModeContext->AddMapping({SDL_SCANCODE_S, false, false, false}, OperationType::Scale);
objectModeContext->AddMapping({SDL_SCANCODE_X, false, false, false}, OperationType::Delete);
objectModeContext->AddMapping({SDL_SCANCODE_D, false, true, false}, OperationType::Duplicate);  // Shift+D

// 注册上下文
OperationMappingManager mappingManager;
mappingManager.RegisterContext(objectModeContext);
mappingManager.SetCurrentContext("ObjectMode");

// InputModule 会自动使用当前上下文进行映射
```

### 5.4 默认映射

`OperationMappingManager` 构造函数会自动创建默认的 "ObjectMode" 上下文，包含以下映射：

| 快捷键 | 操作 |
|--------|------|
| G | Move |
| R | Rotate |
| S | Scale |
| X | Delete |
| Shift+D | Duplicate |
| ESC | Cancel |
| Enter | Confirm |

---

## 6. 手势检测

手势检测将底层鼠标输入转换为高层手势事件。

### 6.1 GestureType（手势类型）

```cpp
enum class GestureType {
    Drag,          // 拖拽
    Click,         // 单击
    DoubleClick,   // 双击
    Pan,           // 平移（中键拖拽）
    Rotate,        // 旋转（Alt+左键拖拽）
    Zoom,          // 缩放（Ctrl+滚轮或中键拖拽）
    BoxSelect,     // 框选（Shift+左键拖拽）
    LassoSelect    // 套索选择（Ctrl+Shift+左键拖拽）
};
```

### 6.2 GestureEvent（手势事件）

```cpp
struct GestureEvent : public EventBase {
    GestureType type;       // 手势类型
    float startX, startY;   // 起始位置
    float currentX, currentY;  // 当前位置
    float deltaX, deltaY;   // 相对移动量
    bool isActive;           // 手势是否激活
    MouseButton button;     // 触发按钮
    bool ctrl, shift, alt;  // 修饰键状态
};

// 订阅手势事件
eventBus.Subscribe<GestureEvent>([](const GestureEvent& event) {
    switch (event.type) {
        case GestureType::Drag:
            std::cout << "Dragging from (" << event.startX << ", " << event.startY 
                      << ") to (" << event.currentX << ", " << event.currentY << ")" << std::endl;
            break;
        case GestureType::BoxSelect:
            std::cout << "Box selecting from (" << event.startX << ", " << event.startY 
                      << ") to (" << event.currentX << ", " << event.currentY << ")" << std::endl;
            break;
        // ...
    }
});
```

### 6.3 手势检测规则

`InputModule` 根据鼠标按钮和修饰键自动检测手势：

| 手势 | 触发条件 |
|------|---------|
| Drag | 左键拖拽（无修饰键） |
| Click | 左键单击 |
| DoubleClick | 左键双击 |
| Pan | 中键拖拽 |
| Rotate | Alt + 左键拖拽 |
| Zoom | Ctrl + 滚轮或中键拖拽 |
| BoxSelect | Shift + 左键拖拽 |
| LassoSelect | Ctrl + Shift + 左键拖拽 |

---

## 7. 完整示例

### 7.1 基础事件订阅示例

```cpp
#include "render/application/application_host.h"
#include "render/application/event_bus.h"
#include "render/application/events/input_events.h"
#include "render/application/modules/input_module.h"

using namespace Render::Application;
using namespace Render::Application::Events;

int main() {
    // 初始化 ApplicationHost
    ApplicationHost host;
    host.Initialize("Event Test", 1280, 720);
    
    // 注册 InputModule
    host.GetModuleRegistry().RegisterModule(std::make_shared<InputModule>());
    
    // 获取 EventBus
    EventBus& eventBus = host.GetEventBus();
    
    // 订阅键盘事件
    eventBus.Subscribe<KeyEvent>([](const KeyEvent& event) {
        if (event.state == KeyState::Pressed && !event.repeat) {
            std::cout << "Key pressed: " << event.scancode << std::endl;
        }
    });
    
    // 订阅操作事件
    eventBus.Subscribe<OperationEvent>([](const OperationEvent& event) {
        const char* opNames[] = {
            "Select", "Add", "Delete", "Move", "Rotate",
            "Scale", "Duplicate", "Cancel", "Confirm"
        };
        std::cout << "Operation: " << opNames[static_cast<int>(event.type)] << std::endl;
    });
    
    // 主循环
    while (host.IsRunning()) {
        host.UpdateFrame();
    }
    
    return 0;
}
```

### 7.2 带过滤器的事件订阅示例

```cpp
// 创建标签过滤器
auto uiFilter = std::make_shared<TagEventFilter>("ui");

// 订阅带标签过滤的键盘事件
eventBus.Subscribe<KeyEvent>(
    [](const KeyEvent& event) {
        std::cout << "UI key event: " << event.scancode << std::endl;
    },
    EventPriority::Normal,
    uiFilter
);

// 发布带标签的事件
KeyEvent keyEvent;
keyEvent.AddTag("ui");
keyEvent.scancode = SDL_SCANCODE_ESCAPE;
eventBus.Publish(keyEvent);  // 只有使用 uiFilter 的订阅者会收到
```

### 7.3 操作映射示例

```cpp
#include "render/application/operation_mapping.h"

// 创建自定义上下文
auto editModeContext = std::make_shared<ShortcutContext>("EditMode");
editModeContext->AddMapping({SDL_SCANCODE_G, false, false, false}, OperationType::Move);
editModeContext->AddMapping({SDL_SCANCODE_R, false, false, false}, OperationType::Rotate);
editModeContext->AddMapping({SDL_SCANCODE_X, false, false, false}, OperationType::Delete);

// 注册并切换上下文
OperationMappingManager mappingManager;
mappingManager.RegisterContext(editModeContext);
mappingManager.SetCurrentContext("EditMode");

// InputModule 会自动使用当前上下文
InputModule inputModule;
inputModule.SetShortcutContext("EditMode");
```

### 7.4 手势检测示例

```cpp
// 订阅手势事件
eventBus.Subscribe<GestureEvent>([](const GestureEvent& event) {
    switch (event.type) {
        case GestureType::Drag:
            // 处理拖拽
            if (event.isActive) {
                // 更新拖拽对象位置
                updateDragTarget(event.currentX, event.currentY);
            }
            break;
            
        case GestureType::BoxSelect:
            // 处理框选
            if (event.isActive) {
                // 更新选择框
                updateSelectionBox(
                    event.startX, event.startY,
                    event.currentX, event.currentY
                );
            } else {
                // 完成选择
                finalizeSelection(
                    event.startX, event.startY,
                    event.currentX, event.currentY
                );
            }
            break;
            
        case GestureType::Zoom:
            // 处理缩放
            if (event.isActive) {
                // 根据 deltaY 调整缩放比例
                float zoomFactor = 1.0f + event.deltaY * 0.01f;
                applyZoom(zoomFactor);
            }
            break;
            
        case GestureType::Pan:
            // 处理平移
            if (event.isActive) {
                // 更新相机或视图位置
                translateView(event.deltaX, event.deltaY);
            }
            break;
            
        default:
            break;
    }
});
```

---

## 8. 最佳实践

### 8.1 事件订阅管理

**保存 ListenerId**：订阅事件时保存返回的 `ListenerId`，以便后续取消订阅：

```cpp
class MyComponent {
public:
    void Initialize(EventBus& eventBus) {
        // 保存 listener ID
        m_keyListenerId = eventBus.Subscribe<KeyEvent>(
            [this](const KeyEvent& event) {
                this->OnKeyEvent(event);
            }
        );
    }
    
    void Shutdown(EventBus& eventBus) {
        // 取消订阅
        eventBus.Unsubscribe<KeyEvent>(m_keyListenerId);
    }
    
private:
    void OnKeyEvent(const KeyEvent& event) {
        // 处理事件
    }
    
    ListenerId m_keyListenerId;
};
```

### 8.2 使用过滤器减少回调

**优先使用过滤器**：使用过滤器可以减少不必要的回调执行，提高性能：

```cpp
// 不好的做法：在回调中检查条件
eventBus.Subscribe<KeyEvent>([](const KeyEvent& event) {
    if (event.scancode == SDL_SCANCODE_SPACE) {  // 每次都检查
        // 处理逻辑
    }
});

// 好的做法：使用自定义过滤器
class SpaceKeyFilter : public EventFilter {
public:
    bool ShouldReceive(const EventBase& event) const override {
        if (auto* keyEvent = dynamic_cast<const KeyEvent*>(&event)) {
            return keyEvent->scancode == SDL_SCANCODE_SPACE;
        }
        return false;
    }
};

auto filter = std::make_shared<SpaceKeyFilter>();
eventBus.Subscribe<KeyEvent>(
    [](const KeyEvent& event) {
        // 只接收 Space 键事件
    },
    EventPriority::Normal,
    filter
);
```

### 8.3 事件优先级使用

**合理设置优先级**：高优先级用于系统级处理（如输入拦截），低优先级用于业务逻辑：

```cpp
// 系统级处理（高优先级）
eventBus.Subscribe<KeyEvent>(
    [](const KeyEvent& event) {
        // 检查是否需要拦截（如模态对话框）
        if (shouldIntercept(event)) {
            event.SetHandled();  // 标记为已处理
        }
    },
    EventPriority::High
);

// 业务逻辑（正常优先级）
eventBus.Subscribe<KeyEvent>(
    [](const KeyEvent& event) {
        if (!event.IsHandled()) {  // 检查是否已被处理
            // 处理业务逻辑
        }
    },
    EventPriority::Normal
);
```

### 8.4 场景隔离

**使用场景过滤器**：不同场景的事件应该隔离，避免相互干扰：

```cpp
// 场景 A 的订阅
auto sceneAFilter = std::make_shared<SceneEventFilter>("SceneA");
eventBus.Subscribe<KeyEvent>(
    [](const KeyEvent& event) {
        // 只处理 SceneA 的事件
    },
    EventPriority::Normal,
    sceneAFilter
);

// 场景 B 的订阅
auto sceneBFilter = std::make_shared<SceneEventFilter>("SceneB");
eventBus.Subscribe<KeyEvent>(
    [](const KeyEvent& event) {
        // 只处理 SceneB 的事件
    },
    EventPriority::Normal,
    sceneBFilter
);
```

### 8.5 避免在回调中发布事件

**避免循环发布**：在事件回调中发布新事件可能导致无限循环，应该谨慎使用：

```cpp
// 危险：可能导致无限循环
eventBus.Subscribe<KeyEvent>([](const KeyEvent& event) {
    if (event.scancode == SDL_SCANCODE_SPACE) {
        KeyEvent newEvent;  // 创建新事件
        eventBus.Publish(newEvent);  // 可能导致循环
    }
});

// 安全：使用标志或延迟发布
bool shouldPublish = false;
eventBus.Subscribe<KeyEvent>([](const KeyEvent& event) {
    if (event.scancode == SDL_SCANCODE_SPACE) {
        shouldPublish = true;  // 设置标志
    }
});

// 在下一帧发布
if (shouldPublish) {
    KeyEvent newEvent;
    eventBus.Publish(newEvent);
    shouldPublish = false;
}
```

---

## 9. 常见问题

### 9.1 为什么我的事件订阅没有收到事件？

**可能原因**：
1. **过滤器不匹配**：检查事件是否包含所需的标签或目标场景
2. **事件未发布**：确认 `InputModule` 已注册并运行
3. **优先级问题**：高优先级订阅者可能标记事件为已处理
4. **订阅时机**：确保在事件发布之前完成订阅

**调试方法**：
```cpp
// 添加调试输出
eventBus.Subscribe<KeyEvent>([](const KeyEvent& event) {
    std::cout << "Received KeyEvent: " << event.scancode << std::endl;
});

// 检查事件标签
KeyEvent event;
event.AddTag("test");
std::cout << "Event tags: ";
for (const auto& tag : event.GetTags()) {
    std::cout << tag << " ";
}
std::cout << std::endl;
```

### 9.2 如何实现事件拦截？

**使用高优先级和 Handled 标志**：
```cpp
// 拦截器（高优先级）
eventBus.Subscribe<KeyEvent>(
    [](const KeyEvent& event) {
        if (shouldIntercept(event)) {
            event.SetHandled();  // 标记为已处理
        }
    },
    EventPriority::High
);

// 普通处理器（检查是否已处理）
eventBus.Subscribe<KeyEvent>(
    [](const KeyEvent& event) {
        if (!event.IsHandled()) {
            // 处理事件
        }
    },
    EventPriority::Normal
);
```

### 9.3 如何在不同场景间切换快捷键上下文？

**使用 InputModule 的 SetShortcutContext**：
```cpp
InputModule* inputModule = /* ... */;

// 切换到 ObjectMode
inputModule->SetShortcutContext("ObjectMode");

// 切换到 EditMode
inputModule->SetShortcutContext("EditMode");

// 获取当前上下文
const ShortcutContext* context = inputModule->GetOperationMapping().GetCurrentContext();
std::cout << "Current context: " << context->GetName() << std::endl;
```

### 9.4 如何自定义手势检测规则？

**扩展 InputModule**：目前手势检测规则在 `InputModule` 中硬编码，如需自定义，可以：
1. 继承 `InputModule` 并重写 `ProcessMouseGesture` 方法
2. 或直接修改 `InputModule` 的源码

---

## 10. 性能考虑

### 10.1 事件发布开销

**批量发布**：避免在循环中频繁发布事件，考虑批量处理：

```cpp
// 不好的做法
for (int i = 0; i < 1000; ++i) {
    KeyEvent event;
    eventBus.Publish(event);  // 每次发布都有开销
}

// 好的做法：批量处理
std::vector<KeyEvent> events;
events.reserve(1000);
// ... 收集事件 ...
for (const auto& event : events) {
    eventBus.Publish(event);
}
```

### 10.2 过滤器性能

**过滤器应该快速**：过滤器在每次事件发布时都会被调用，应该保持轻量：

```cpp
// 不好的做法：在过滤器中做复杂计算
class SlowFilter : public EventFilter {
    bool ShouldReceive(const EventBase& event) const override {
        // 复杂计算（不推荐）
        for (int i = 0; i < 1000000; ++i) {
            // ...
        }
        return true;
    }
};

// 好的做法：快速检查
class FastFilter : public EventFilter {
    bool ShouldReceive(const EventBase& event) const override {
        // 简单检查
        return event.HasTag("ui");
    }
};
```

### 10.3 回调函数性能

**避免在回调中做耗时操作**：事件回调应该快速执行，耗时操作应该异步处理：

```cpp
// 不好的做法
eventBus.Subscribe<KeyEvent>([](const KeyEvent& event) {
    // 耗时操作（不推荐）
    loadLargeFile("data.bin");
    processComplexData();
});

// 好的做法：异步处理
eventBus.Subscribe<KeyEvent>([](const KeyEvent& event) {
    // 快速处理
    if (event.scancode == SDL_SCANCODE_SPACE) {
        // 提交异步任务
        asyncTaskQueue.push([=]() {
            loadLargeFile("data.bin");
            processComplexData();
        });
    }
});
```

---

## 11. 与其他系统集成

### 11.1 与 SceneManager 集成

**场景切换时自动切换上下文**：
```cpp
// 在场景进入时切换快捷键上下文
class MyScene : public Scene {
    void OnEnter() override {
        auto* inputModule = GetAppContext().GetModuleRegistry()
            .GetModule<InputModule>("InputModule");
        if (inputModule) {
            inputModule->SetShortcutContext("ObjectMode");
        }
    }
};
```

### 11.2 与 ECS 系统集成

**在 ECS 组件中订阅事件**：
```cpp
class InputComponent : public Component {
public:
    void Initialize(EventBus& eventBus) {
        m_listenerId = eventBus.Subscribe<KeyEvent>(
            [this](const KeyEvent& event) {
                this->HandleKeyEvent(event);
            }
        );
    }
    
    void Destroy(EventBus& eventBus) {
        eventBus.Unsubscribe<KeyEvent>(m_listenerId);
    }
    
private:
    void HandleKeyEvent(const KeyEvent& event) {
        // 更新组件状态
    }
    
    ListenerId m_listenerId;
};
```

---

## 12. 参考资源

### 12.1 相关文档

- [APPLICATION_LAYER_PROGRESS_REPORT.md](APPLICATION_LAYER_PROGRESS_REPORT.md) - 应用层开发进度报告
- [SCENE_MODULE_FRAMEWORK.md](../SCENE_MODULE_FRAMEWORK.md) - 场景模块框架设计
- [Resource_Management_Guide.md](Resource_Management_Guide.md) - 资源管理指南

### 12.2 示例代码

- `examples/53_event_system_test.cpp` - 完整的事件系统测试示例
- `examples/52_application_boot_demo.cpp` - 应用层启动示例

### 12.3 API 参考

- `include/render/application/event_bus.h` - EventBus 接口定义
- `include/render/application/events/input_events.h` - 输入事件定义
- `include/render/application/operation_mapping.h` - 操作映射接口
- `include/render/application/modules/input_module.h` - InputModule 接口

---

## 13. 总结

事件系统是应用层框架的核心通信机制，提供了：

- ✅ **类型安全**：编译时类型检查，避免运行时错误
- ✅ **灵活过滤**：支持标签、场景、自定义过滤器
- ✅ **优先级控制**：支持事件处理优先级
- ✅ **Blender 风格**：符合用户偏好的操作映射
- ✅ **手势检测**：自动检测常见手势
- ✅ **线程安全**：支持多线程环境

通过合理使用事件系统，可以构建解耦、可维护的应用架构。

---

**上一章**: [APPLICATION_LAYER_PROGRESS_REPORT.md](APPLICATION_LAYER_PROGRESS_REPORT.md)  
**下一章**: [Resource_Management_Guide.md](Resource_Management_Guide.md)
       