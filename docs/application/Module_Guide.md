[返回文档首页](../README.md)

---

# 模块开发指南

**更新时间**: 2025-11-19  
**参考文档**: [SCENE_MODULE_FRAMEWORK.md](../SCENE_MODULE_FRAMEWORK.md), [EventBus_Guide.md](EventBus_Guide.md)

---

## 1. 概述

应用层模块系统提供了一种标准化的方式来组织和扩展应用功能。模块是应用层的基本构建单元，负责：

- **功能封装**：将相关功能组织成独立的模块
- **生命周期管理**：提供统一的注册、初始化、更新、清理流程
- **依赖管理**：自动处理模块间的依赖关系
- **优先级控制**：控制模块在不同阶段的执行顺序
- **ECS集成**：方便地注册ECS组件和系统

## 2. 模块系统架构

### 2.1 核心组件

```
┌─────────────────────────────────────────┐
│         ApplicationHost                 │
│  ┌──────────────────────────────────┐  │
│  │      ModuleRegistry              │  │
│  │  ┌──────────┐  ┌──────────┐    │  │
│  │  │ Module 1 │  │ Module 2 │    │  │
│  │  └──────────┘  └──────────┘    │  │
│  └──────────────────────────────────┘  │
│  ┌──────────────────────────────────┐  │
│  │         AppContext               │  │
│  │  - Renderer                      │  │
│  │  - ResourceManager               │  │
│  │  - EventBus                      │  │
│  │  - World                         │  │
│  └──────────────────────────────────┘  │
└─────────────────────────────────────────┘
```

### 2.2 模块生命周期

```
注册阶段 (Register)
    ↓
OnRegister() - 初始化模块，注册ECS组件/系统
    ↓
[每帧循环]
    ↓
PreFrame阶段
    ↓
OnPreFrame() - 帧开始处理（输入、更新等）
    ↓
ECS World.Update() - ECS系统更新
    ↓
PostFrame阶段
    ↓
OnPostFrame() - 帧结束处理（渲染、统计等）
    ↓
[循环继续...]
    ↓
卸载阶段 (Unregister)
    ↓
OnUnregister() - 清理资源，移除ECS系统
```

## 3. 创建自定义模块

### 3.1 基本模块结构

创建一个自定义模块需要继承 `AppModule` 基类：

```cpp
#include "render/application/app_module.h"
#include "render/application/app_context.h"
#include "render/ecs/world.h"
#include "render/logger.h"

namespace Render::Application {

class MyCustomModule final : public AppModule {
public:
    MyCustomModule() = default;
    ~MyCustomModule() override = default;

    // 必须实现：模块名称
    std::string_view Name() const override { 
        return "MyCustomModule"; 
    }

    // 可选：声明依赖
    ModuleDependencies Dependencies() const override { 
        return {"CoreRenderModule"};  // 依赖CoreRenderModule
    }

    // 可选：设置优先级
    int Priority(ModulePhase phase) const override {
        switch (phase) {
            case ModulePhase::PreFrame:
                return 50;  // PreFrame阶段优先级50
            case ModulePhase::PostFrame:
                return 0;  // PostFrame阶段优先级0（默认）
            default:
                return 0;
        }
    }

    // 必须实现：模块注册
    void OnRegister(ECS::World& world, AppContext& ctx) override {
        Logger::GetInstance().Info("[MyCustomModule] Registered");
        // 初始化模块
        // 注册ECS组件/系统
    }

    // 可选：模块卸载
    void OnUnregister(ECS::World& world, AppContext& ctx) override {
        Logger::GetInstance().Info("[MyCustomModule] Unregistered");
        // 清理资源
    }

    // 可选：PreFrame钩子
    void OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx) override {
        // 每帧开始时执行
    }

    // 可选：PostFrame钩子
    void OnPostFrame(const FrameUpdateArgs& frame, AppContext& ctx) override {
        // 每帧结束时执行
    }

private:
    bool m_registered = false;
};

} // namespace Render::Application
```

### 3.2 模块注册

在应用初始化时注册模块：

```cpp
#include "render/application/application_host.h"
#include "render/application/modules/my_custom_module.h"

ApplicationHost host;
// ... 初始化host ...

auto& moduleRegistry = host.GetModuleRegistry();

// 注册模块
moduleRegistry.RegisterModule(std::make_unique<MyCustomModule>());
```

## 4. 模块生命周期详解

### 4.1 OnRegister

`OnRegister` 在模块注册时调用一次，用于：

- 初始化模块状态
- 注册ECS组件
- 注册ECS系统
- 订阅事件
- 设置资源

```cpp
void MyModule::OnRegister(ECS::World& world, AppContext& ctx) override {
    // 1. 注册ECS组件
    world.RegisterComponent<MyComponent>();
    
    // 2. 注册ECS系统
    world.RegisterSystem<MySystem>(ctx.renderer);
    
    // 3. 订阅事件
    if (ctx.globalEventBus) {
        m_listenerId = ctx.globalEventBus->Subscribe<FrameBeginEvent>(
            [this](const FrameBeginEvent& e) {
                OnFrameBegin(e);
            }
        );
    }
    
    // 4. 初始化模块状态
    m_registered = true;
}
```

### 4.2 OnUnregister

`OnUnregister` 在模块卸载时调用，用于：

- 清理资源
- 取消事件订阅
- 移除ECS系统（可选，通常由World管理）

```cpp
void MyModule::OnUnregister(ECS::World& world, AppContext& ctx) override {
    // 1. 取消事件订阅
    if (ctx.globalEventBus && m_listenerId != 0) {
        ctx.globalEventBus->Unsubscribe(m_listenerId);
        m_listenerId = 0;
    }
    
    // 2. 清理资源
    m_resources.clear();
    
    // 3. 重置状态
    m_registered = false;
}
```

### 4.3 OnPreFrame

`OnPreFrame` 在每帧开始时调用，在ECS系统更新之前执行。适合：

- 处理输入
- 更新模块状态
- 发布事件

```cpp
void MyModule::OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx) override {
    // 处理输入
    if (ctx.globalEventBus) {
        // 发布自定义事件
        MyCustomEvent event;
        event.frame = frame;
        ctx.globalEventBus->Publish(event);
    }
    
    // 更新模块状态
    UpdateModuleState(frame.deltaTime);
}
```

### 4.4 OnPostFrame

`OnPostFrame` 在每帧结束时调用，在ECS系统更新之后执行。适合：

- 渲染UI/HUD
- 收集统计信息
- 执行后处理

```cpp
void MyModule::OnPostFrame(const FrameUpdateArgs& frame, AppContext& ctx) override {
    // 渲染调试信息
    if (ctx.renderer) {
        RenderDebugInfo(ctx);
    }
    
    // 收集统计信息
    CollectStats(frame);
}
```

## 5. 依赖管理

### 5.1 声明依赖

模块可以声明对其他模块的依赖：

```cpp
ModuleDependencies Dependencies() const override {
    return {"CoreRenderModule", "InputModule"};
}
```

### 5.2 依赖解析

`ModuleRegistry` 会自动：

- 检查依赖是否存在
- 按依赖顺序注册模块
- 确保依赖模块先于当前模块初始化

```cpp
// 模块注册顺序会自动调整为：
// 1. CoreRenderModule（无依赖）
// 2. InputModule（依赖CoreRenderModule）
// 3. MyModule（依赖CoreRenderModule和InputModule）
```

### 5.3 依赖检查

如果依赖未满足，模块注册会失败：

```cpp
// 如果InputModule未注册，MyModule注册会失败
auto& moduleRegistry = host.GetModuleRegistry();

// 先注册依赖
moduleRegistry.RegisterModule(std::make_unique<CoreRenderModule>());
moduleRegistry.RegisterModule(std::make_unique<InputModule>());

// 然后注册依赖它们的模块
moduleRegistry.RegisterModule(std::make_unique<MyModule>());
```

## 6. 优先级系统

### 6.1 优先级概念

优先级控制模块在不同阶段的执行顺序：

- **数字越大，优先级越高**
- 高优先级模块先执行
- 相同优先级按注册顺序执行

### 6.2 设置优先级

```cpp
int Priority(ModulePhase phase) const override {
    switch (phase) {
        case ModulePhase::PreFrame:
            return 200;  // PreFrame阶段高优先级
        case ModulePhase::PostFrame:
            return -50;  // PostFrame阶段低优先级（负值表示最后执行）
        default:
            return 0;
    }
}
```

### 6.3 常见优先级值

参考现有模块的优先级设置：

| 模块 | PreFrame | PostFrame | 说明 |
|------|----------|-----------|------|
| CoreRenderModule | 100 | 0 | 核心渲染模块 |
| InputModule | 200 | - | 输入处理（需要最早执行） |
| DebugHUDModule | - | -50 | 调试HUD（最后渲染） |

### 6.4 优先级建议

- **PreFrame阶段**：
  - 输入处理：200+
  - 核心更新：100-199
  - 一般更新：1-99
  - 默认：0

- **PostFrame阶段**：
  - 统计收集：0
  - UI渲染：-50
  - 调试信息：-100

## 7. 与ECS系统集成

### 7.1 注册ECS组件

```cpp
void MyModule::OnRegister(ECS::World& world, AppContext& ctx) override {
    // 注册自定义组件
    world.RegisterComponent<MyCustomComponent>();
    
    Logger::GetInstance().Info("[MyModule] Components registered");
}
```

### 7.2 注册ECS系统

```cpp
void MyModule::OnRegister(ECS::World& world, AppContext& ctx) override {
    // 注册自定义系统
    if (ctx.renderer) {
        world.RegisterSystem<MyCustomSystem>(ctx.renderer);
    }
    
    Logger::GetInstance().Info("[MyModule] Systems registered");
}
```

### 7.3 系统优先级

ECS系统也有优先级，在注册时设置：

```cpp
// 系统优先级在System类中定义
// 模块注册系统时，系统会按优先级自动排序
world.RegisterSystem<MySystem>(ctx.renderer);  // 使用系统默认优先级
```

## 8. 事件系统集成

### 8.1 订阅事件

```cpp
void MyModule::OnRegister(ECS::World& world, AppContext& ctx) override {
    if (ctx.globalEventBus) {
        // 订阅帧事件
        m_frameListenerId = ctx.globalEventBus->Subscribe<FrameBeginEvent>(
            [this](const FrameBeginEvent& e) {
                OnFrameBegin(e);
            },
            50  // 优先级
        );
        
        // 订阅输入事件（带过滤器）
        auto inputFilter = std::make_shared<TagEventFilter>("input");
        m_inputListenerId = ctx.globalEventBus->Subscribe<KeyEvent>(
            [this](const KeyEvent& e) {
                OnKeyEvent(e);
            },
            0,
            inputFilter
        );
    }
}
```

### 8.2 发布事件

```cpp
void MyModule::OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx) override {
    if (ctx.globalEventBus) {
        // 发布自定义事件
        MyCustomEvent event;
        event.frame = frame;
        event.data = m_moduleData;
        event.AddTag("my-module");
        ctx.globalEventBus->Publish(event);
    }
}
```

### 8.3 取消订阅

```cpp
void MyModule::OnUnregister(ECS::World& world, AppContext& ctx) override {
    if (ctx.globalEventBus) {
        if (m_frameListenerId != 0) {
            ctx.globalEventBus->Unsubscribe(m_frameListenerId);
        }
        if (m_inputListenerId != 0) {
            ctx.globalEventBus->Unsubscribe(m_inputListenerId);
        }
    }
}
```

## 9. 访问AppContext

### 9.1 可用服务

`AppContext` 提供以下服务：

```cpp
struct AppContext {
    Renderer* renderer;              // 渲染器
    UniformManager* uniformManager;  // Uniform管理器
    ResourceManager* resourceManager; // 资源管理器
    AsyncResourceLoader* asyncLoader;  // 异步加载器
    EventBus* globalEventBus;         // 事件总线
    ECS::World* world;                // ECS世界
    UI::UIInputRouter* uiInputRouter; // UI输入路由
    FrameUpdateArgs lastFrame;        // 上一帧信息
};
```

### 9.2 使用示例

```cpp
void MyModule::OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx) override {
    // 访问渲染器
    if (ctx.renderer) {
        auto stats = ctx.renderer->GetStats();
        // 使用统计信息
    }
    
    // 访问资源管理器
    if (ctx.resourceManager) {
        auto texture = ctx.resourceManager->GetTexture("my_texture");
        // 使用纹理
    }
    
    // 访问事件总线
    if (ctx.globalEventBus) {
        // 发布/订阅事件
    }
}
```

## 10. 完整示例

### 10.1 简单模块示例

```cpp
#include "render/application/app_module.h"
#include "render/application/app_context.h"
#include "render/application/event_bus.h"
#include "render/application/events/frame_events.h"
#include "render/ecs/world.h"
#include "render/logger.h"

namespace Render::Application {

class SimpleStatsModule final : public AppModule {
public:
    SimpleStatsModule() = default;
    ~SimpleStatsModule() override = default;

    std::string_view Name() const override { 
        return "SimpleStatsModule"; 
    }

    ModuleDependencies Dependencies() const override { 
        return {"CoreRenderModule"}; 
    }

    int Priority(ModulePhase phase) const override {
        switch (phase) {
            case ModulePhase::PostFrame:
                return -10;  // 在HUD之前显示
            default:
                return 0;
        }
    }

    void OnRegister(ECS::World& world, AppContext& ctx) override {
        m_registered = true;
        m_frameCount = 0;
        Logger::GetInstance().Info("[SimpleStatsModule] Registered");
    }

    void OnUnregister(ECS::World& world, AppContext& ctx) override {
        m_registered = false;
        Logger::GetInstance().Info("[SimpleStatsModule] Unregistered");
    }

    void OnPostFrame(const FrameUpdateArgs& frame, AppContext& ctx) override {
        if (!m_registered) {
            return;
        }

        m_frameCount++;
        
        // 每60帧输出一次统计
        if (m_frameCount % 60 == 0 && ctx.renderer) {
            auto stats = ctx.renderer->GetStats();
            Logger::GetInstance().InfoFormat(
                "[SimpleStatsModule] FPS: %.1f, DrawCalls: %u",
                stats.fps,
                stats.drawCalls
            );
        }
    }

private:
    bool m_registered = false;
    uint64_t m_frameCount = 0;
};

} // namespace Render::Application
```

### 10.2 复杂模块示例

```cpp
#include "render/application/app_module.h"
#include "render/application/app_context.h"
#include "render/application/event_bus.h"
#include "render/application/events/input_events.h"
#include "render/ecs/world.h"
#include "render/ecs/systems.h"
#include "render/ecs/components.h"
#include "render/logger.h"

namespace Render::Application {

class AdvancedModule final : public AppModule {
public:
    AdvancedModule() = default;
    ~AdvancedModule() override = default;

    std::string_view Name() const override { 
        return "AdvancedModule"; 
    }

    ModuleDependencies Dependencies() const override { 
        return {"CoreRenderModule", "InputModule"}; 
    }

    int Priority(ModulePhase phase) const override {
        switch (phase) {
            case ModulePhase::PreFrame:
                return 150;  // 在InputModule之后
            case ModulePhase::PostFrame:
                return -20;
            default:
                return 0;
        }
    }

    void OnRegister(ECS::World& world, AppContext& ctx) override {
        // 注册自定义组件
        world.RegisterComponent<MyCustomComponent>();
        
        // 注册自定义系统
        if (ctx.renderer) {
            world.RegisterSystem<MyCustomSystem>(ctx.renderer);
        }
        
        // 订阅事件
        if (ctx.globalEventBus) {
            m_keyListenerId = ctx.globalEventBus->Subscribe<Events::KeyEvent>(
                [this](const Events::KeyEvent& e) {
                    if (e.state == Events::KeyState::Pressed) {
                        OnKeyPressed(e.scancode);
                    }
                },
                0
            );
        }
        
        m_registered = true;
        Logger::GetInstance().Info("[AdvancedModule] Registered");
    }

    void OnUnregister(ECS::World& world, AppContext& ctx) override {
        // 取消事件订阅
        if (ctx.globalEventBus && m_keyListenerId != 0) {
            ctx.globalEventBus->Unsubscribe(m_keyListenerId);
            m_keyListenerId = 0;
        }
        
        m_registered = false;
        Logger::GetInstance().Info("[AdvancedModule] Unregistered");
    }

    void OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx) override {
        if (!m_registered) {
            return;
        }
        
        // 更新模块逻辑
        UpdateLogic(frame.deltaTime);
        
        // 发布自定义事件
        if (ctx.globalEventBus) {
            MyCustomEvent event;
            event.frame = frame;
            event.data = m_moduleData;
            ctx.globalEventBus->Publish(event);
        }
    }

    void OnPostFrame(const FrameUpdateArgs& frame, AppContext& ctx) override {
        if (!m_registered) {
            return;
        }
        
        // 收集统计信息
        if (ctx.renderer) {
            CollectStats(ctx);
        }
    }

private:
    void UpdateLogic(float deltaTime) {
        // 模块逻辑更新
    }

    void OnKeyPressed(int scancode) {
        // 处理按键
    }

    void CollectStats(AppContext& ctx) {
        // 收集统计信息
    }

    bool m_registered = false;
    EventBus::ListenerId m_keyListenerId = 0;
    MyModuleData m_moduleData;
};

} // namespace Render::Application
```

## 11. 最佳实践

### 11.1 模块设计原则

1. **单一职责**：每个模块只负责一个功能领域
2. **低耦合**：模块之间通过事件系统通信，避免直接依赖
3. **高内聚**：相关功能组织在同一个模块中
4. **可测试**：模块应该易于单独测试

### 11.2 资源管理

```cpp
void MyModule::OnRegister(ECS::World& world, AppContext& ctx) override {
    // ✅ 好：使用智能指针管理资源
    m_resource = std::make_shared<MyResource>();
    
    // ❌ 不好：直接使用裸指针
    // m_resource = new MyResource();
}

void MyModule::OnUnregister(ECS::World& world, AppContext& ctx) override {
    // ✅ 好：智能指针自动清理
    m_resource.reset();
    
    // ❌ 不好：需要手动delete
    // delete m_resource;
}
```

### 11.3 线程安全

```cpp
// 模块方法在主线程调用，但要注意：
// 1. AppContext中的服务可能是线程安全的
// 2. 事件系统是线程安全的
// 3. 但OpenGL调用必须在主线程

void MyModule::OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx) {
    // ✅ 安全：在主线程中访问
    if (ctx.renderer) {
        auto stats = ctx.renderer->GetStats();
    }
    
    // ⚠️ 注意：如果需要在其他线程访问，需要同步
}
```

### 11.4 错误处理

```cpp
void MyModule::OnRegister(ECS::World& world, AppContext& ctx) override {
    // ✅ 好：检查必要的服务
    if (!ctx.renderer) {
        Logger::GetInstance().Error("[MyModule] Renderer is required");
        return;
    }
    
    // ✅ 好：优雅降级
    if (!ctx.asyncLoader) {
        Logger::GetInstance().Warning("[MyModule] AsyncLoader not available, using sync loading");
        // 使用同步加载
    }
}
```

### 11.5 性能考虑

```cpp
void MyModule::OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx) override {
    // ✅ 好：避免在每帧分配内存
    // 使用预分配的缓冲区
    
    // ❌ 不好：每帧分配
    // std::vector<Data> data(frame.deltaTime * 1000);
    
    // ✅ 好：缓存计算结果
    if (m_cacheDirty) {
        RecalculateCache();
        m_cacheDirty = false;
    }
}
```

## 12. 调试与测试

### 12.1 日志记录

```cpp
void MyModule::OnRegister(ECS::World& world, AppContext& ctx) override {
    Logger::GetInstance().Info("[MyModule] Registering...");
    
    // 记录详细信息
    Logger::GetInstance().DebugFormat(
        "[MyModule] Dependencies: %zu",
        Dependencies().size()
    );
}
```

### 12.2 状态检查

```cpp
void MyModule::OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx) override {
    // 检查模块状态
    if (!m_registered) {
        Logger::GetInstance().Warning("[MyModule] Called OnPreFrame but not registered");
        return;
    }
    
    // 验证AppContext
    if (!ctx.IsValid()) {
        Logger::GetInstance().Error("[MyModule] Invalid AppContext");
        return;
    }
}
```

### 12.3 单元测试

```cpp
// 测试模块注册
TEST(MyModuleTest, Registration) {
    ApplicationHost host;
    // ... 初始化host ...
    
    auto& moduleRegistry = host.GetModuleRegistry();
    moduleRegistry.RegisterModule(std::make_unique<MyModule>());
    
    auto* module = moduleRegistry.GetModule("MyModule");
    ASSERT_NE(module, nullptr);
    EXPECT_EQ(module->Name(), "MyModule");
}
```

## 13. 常见问题

### Q: 模块注册失败怎么办？

A: 检查：
1. 依赖模块是否已注册
2. 模块名称是否唯一
3. AppContext是否有效

### Q: 如何控制模块执行顺序？

A: 使用优先级系统。在 `Priority()` 方法中返回不同的优先级值。

### Q: 模块可以动态启用/禁用吗？

A: 可以。使用 `ModuleRegistry::ActivateModule()` 和 `DeactivateModule()`。

### Q: 模块可以访问其他模块吗？

A: 可以，但不推荐直接访问。推荐通过事件系统通信。

### Q: 模块可以注册多个ECS系统吗？

A: 可以。在 `OnRegister` 中注册多个系统。

## 14. 参考

- [SCENE_MODULE_FRAMEWORK.md](../SCENE_MODULE_FRAMEWORK.md) - 应用层框架设计
- [EventBus_Guide.md](EventBus_Guide.md) - 事件总线使用指南
- [APPLICATION_LAYER_PROGRESS_REPORT.md](APPLICATION_LAYER_PROGRESS_REPORT.md) - 应用层开发进度
- `include/render/application/app_module.h` - AppModule API
- `include/render/application/module_registry.h` - ModuleRegistry API
- `examples/54_module_hud_test.cpp` - 模块使用示例

---

**上一章**: [EventBus_Guide.md](EventBus_Guide.md)  
**下一章**: [SceneGraph_Node_Guide.md](SceneGraph_Node_Guide.md)
