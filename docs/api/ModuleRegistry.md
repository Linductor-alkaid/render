# ModuleRegistry API 文档

## 目录
[返回API文档首页](README.md)

---

## 概述

`ModuleRegistry` 是应用层的模块注册表，负责管理所有应用模块（AppModule）的生命周期、依赖关系和执行顺序。模块可以注册ECS组件和系统，并在PreFrame和PostFrame阶段执行自定义逻辑。

**命名空间**: `Render::Application`

---

## 类定义

```cpp
class ModuleRegistry {
public:
    ModuleRegistry();
    ~ModuleRegistry();

    void Initialize(ECS::World* world, AppContext* ctx);
    void Shutdown();

    bool RegisterModule(std::unique_ptr<AppModule> module, bool activateImmediately = true);
    void UnregisterModule(std::string_view name);

    bool ActivateModule(std::string_view name);
    void DeactivateModule(std::string_view name);

    void ForEachModule(const std::function<void(const AppModule&)>& visitor) const;
    AppModule* GetModule(std::string_view name);
    const AppModule* GetModule(std::string_view name) const;

    void InvokePhase(ModulePhase phase, const FrameUpdateArgs& frameArgs);

    // 工具链集成接口
    struct ModuleState {
        std::string name;
        bool active = false;
        bool registered = false;
        ModuleDependencies dependencies;
        int preFramePriority = 0;
        int postFramePriority = 0;
    };

    std::optional<ModuleState> GetModuleState(std::string_view name) const;
    std::vector<ModuleState> GetAllModuleStates() const;
    bool IsModuleActive(std::string_view name) const;
    bool IsModuleRegistered(std::string_view name) const;
};
```

---

## 核心功能

### 1. 初始化与关闭

#### `Initialize(ECS::World* world, AppContext* ctx)`

初始化模块注册表。

**参数**:
- `world`: ECS World实例指针
- `ctx`: 应用上下文指针

**注意**: 通常在ApplicationHost初始化时自动调用，无需手动调用。

#### `Shutdown()`

关闭模块注册表，清理所有模块。

**注意**: 会自动调用所有模块的 `OnUnregister` 方法。

---

### 2. 模块注册

#### `RegisterModule(std::unique_ptr<AppModule> module, bool activateImmediately = true)`

注册一个模块到注册表中。

**参数**:
- `module`: 模块的unique_ptr（所有权转移）
- `activateImmediately`: 是否立即激活模块（默认true）

**返回值**: 成功返回 `true`，失败返回 `false`（例如依赖未满足）

**依赖解析**: 注册时会自动解析模块依赖关系，确保依赖模块先于当前模块激活。

**示例**:
```cpp
auto& moduleRegistry = host.GetModuleRegistry();

// 注册核心渲染模块
moduleRegistry.RegisterModule(std::make_unique<CoreRenderModule>());

// 注册输入模块（依赖CoreRenderModule）
moduleRegistry.RegisterModule(std::make_unique<InputModule>());

// 注册调试HUD模块（依赖CoreRenderModule）
moduleRegistry.RegisterModule(std::make_unique<DebugHUDModule>());
```

#### `UnregisterModule(std::string_view name)`

从注册表中移除模块。

**参数**:
- `name`: 模块名称

**注意**: 会自动调用模块的 `OnUnregister` 方法，并停用模块。

---

### 3. 模块激活控制

#### `ActivateModule(std::string_view name)`

激活已注册的模块。

**参数**:
- `name`: 模块名称

**返回值**: 成功返回 `true`，失败返回 `false`

**注意**: 如果模块的依赖未激活，会自动激活依赖模块。

**示例**:
```cpp
if (moduleRegistry.ActivateModule("InputModule")) {
    LOG_INFO("InputModule activated");
} else {
    LOG_ERROR("Failed to activate InputModule");
}
```

#### `DeactivateModule(std::string_view name)`

停用已激活的模块。

**参数**:
- `name`: 模块名称

**注意**: 会自动调用模块的 `OnUnregister` 方法，但不会移除模块。

---

### 4. 模块查询

#### `GetModule(std::string_view name)`

获取模块指针。

**参数**:
- `name`: 模块名称

**返回值**: 模块指针，如果不存在返回 `nullptr`

**示例**:
```cpp
auto* inputModule = moduleRegistry.GetModule("InputModule");
if (inputModule) {
    // 使用模块
}
```

#### `ForEachModule(const std::function<void(const AppModule&)>& visitor) const`

遍历所有已注册的模块。

**参数**:
- `visitor`: 访问函数

**示例**:
```cpp
moduleRegistry.ForEachModule([](const AppModule& module) {
    LOG_INFO("Module: %s, Active: %s", 
             module.Name().data(), 
             module.IsActive() ? "Yes" : "No");
});
```

---

### 5. 模块阶段调用

#### `InvokePhase(ModulePhase phase, const FrameUpdateArgs& frameArgs)`

调用指定阶段的所有模块钩子。

**参数**:
- `phase`: 模块阶段（`ModulePhase::PreFrame` 或 `ModulePhase::PostFrame`）
- `frameArgs`: 帧更新参数

**注意**: 通常由ApplicationHost自动调用，无需手动调用。模块会按优先级排序执行。

---

### 6. 工具链集成接口

#### `GetModuleState(std::string_view name) const`

获取模块状态信息。

**参数**:
- `name`: 模块名称

**返回值**: 模块状态，如果模块不存在返回 `std::nullopt`

**示例**:
```cpp
auto state = moduleRegistry.GetModuleState("CoreRenderModule");
if (state.has_value()) {
    LOG_INFO("Module: %s, Active: %s, Registered: %s",
             state->name.c_str(),
             state->active ? "Yes" : "No",
             state->registered ? "Yes" : "No");
}
```

#### `GetAllModuleStates() const`

获取所有模块的状态列表。

**返回值**: 模块状态列表（按名称排序）

**示例**:
```cpp
auto states = moduleRegistry.GetAllModuleStates();
for (const auto& state : states) {
    std::cout << "Module: " << state.name 
              << ", Active: " << (state.active ? "Yes" : "No")
              << ", PreFrame Priority: " << state.preFramePriority << std::endl;
}
```

#### `IsModuleActive(std::string_view name) const`

检查模块是否激活。

**返回值**: 如果模块存在且激活返回 `true`

#### `IsModuleRegistered(std::string_view name) const`

检查模块是否已注册。

**返回值**: 如果模块已注册返回 `true`

---

## 模块定义（AppModule）

模块必须继承自 `AppModule` 基类：

```cpp
class MyModule : public AppModule {
public:
    std::string_view Name() const override { return "MyModule"; }
    
    ModuleDependencies Dependencies() const override {
        return {"CoreRenderModule"};  // 声明依赖
    }
    
    int Priority(ModulePhase phase) const override {
        if (phase == ModulePhase::PreFrame) {
            return 100;  // PreFrame阶段优先级
        }
        return 0;  // PostFrame阶段优先级
    }
    
    void OnRegister(ECS::World& world, AppContext& ctx) override {
        // 注册ECS组件和系统
        world.RegisterComponent<MyComponent>();
        world.RegisterSystem<MySystem>();
    }
    
    void OnUnregister(ECS::World& world, AppContext& ctx) override {
        // 清理资源
    }
    
    void OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx) override {
        // PreFrame阶段逻辑
    }
    
    void OnPostFrame(const FrameUpdateArgs& frame, AppContext& ctx) override {
        // PostFrame阶段逻辑
    }
};
```

---

## 使用示例

### 创建自定义模块

```cpp
#include "render/application/app_module.h"
#include "render/ecs/world.h"

class CustomModule : public AppModule {
public:
    std::string_view Name() const override { 
        return "CustomModule"; 
    }
    
    ModuleDependencies Dependencies() const override {
        return {"CoreRenderModule", "InputModule"};
    }
    
    int Priority(ModulePhase phase) const override {
        return phase == ModulePhase::PreFrame ? 150 : -50;
    }
    
    void OnRegister(ECS::World& world, AppContext& ctx) override {
        // 注册自定义组件
        world.RegisterComponent<CustomComponent>();
        
        // 注册自定义系统
        auto system = std::make_unique<CustomSystem>();
        world.RegisterSystem(std::move(system));
        
        LOG_INFO("CustomModule registered");
    }
    
    void OnUnregister(ECS::World& world, AppContext& ctx) override {
        // 清理资源
        LOG_INFO("CustomModule unregistered");
    }
    
    void OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx) override {
        // 每帧开始时的逻辑
    }
    
    void OnPostFrame(const FrameUpdateArgs& frame, AppContext& ctx) override {
        // 每帧结束时的逻辑
    }
};

// 注册模块
auto& moduleRegistry = host.GetModuleRegistry();
moduleRegistry.RegisterModule(std::make_unique<CustomModule>());
```

### 查询模块状态（用于工具链）

```cpp
// 获取所有模块状态
auto states = moduleRegistry.GetAllModuleStates();
for (const auto& state : states) {
    std::cout << "Module: " << state.name << std::endl;
    std::cout << "  Active: " << (state.active ? "Yes" : "No") << std::endl;
    std::cout << "  Registered: " << (state.registered ? "Yes" : "No") << std::endl;
    std::cout << "  PreFrame Priority: " << state.preFramePriority << std::endl;
    std::cout << "  PostFrame Priority: " << state.postFramePriority << std::endl;
    std::cout << "  Dependencies: ";
    for (const auto& dep : state.dependencies) {
        std::cout << dep << " ";
    }
    std::cout << std::endl;
}
```

---

## 注意事项

1. **依赖顺序**: 模块注册时会自动解析依赖关系，确保依赖模块先激活
2. **优先级**: PreFrame阶段按优先级从高到低执行，PostFrame阶段按优先级从低到高执行
3. **生命周期**: 模块的OnRegister在激活时调用，OnUnregister在停用或卸载时调用
4. **线程安全**: ModuleRegistry不是线程安全的，所有调用应在主线程中进行

---

## 相关文档

- [AppModule API](AppModule.md) - 模块基类API
- [ApplicationHost API](ApplicationHost.md) - 应用宿主API
- [模块开发指南](../application/Module_Guide.md) - 模块开发详细指南

---

[返回API文档首页](README.md)

