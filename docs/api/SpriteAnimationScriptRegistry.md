[返回 API 目录](README.md) | [返回 Sprite 系统设计](../SpriteSystemDesign.md)

---

# SpriteAnimationScriptRegistry API 参考

## 📋 概述

`SpriteAnimationScriptRegistry` 是一个线程安全的单例，用于集中管理精灵动画状态机的脚本回调。系统会在状态切换、过渡、帧事件等时机调用注册的脚本，开发者可在脚本中触发音效、粒子、逻辑事件等游戏行为。

- **命名空间**：`Render::ECS`  
- **头文件**：`<render/ecs/sprite_animation_script_registry.h>`  
- **实现文件**：`src/ecs/sprite_animation_script_registry.cpp`  
- **线程安全**：是（内部使用互斥锁保护注册表）

---

## 🧱 核心类型

```cpp
class SpriteAnimationScriptRegistry final {
public:
    using ScriptCallback = std::function<void(EntityID, const SpriteAnimationEvent&)>;

    static SpriteAnimationScriptRegistry& GetInstance();

    bool RegisterScript(const std::string& name, ScriptCallback callback);
    bool UnregisterScript(const std::string& name);
    bool ExecuteScript(const std::string& name, EntityID entity, const SpriteAnimationEvent& event);
    bool HasScript(const std::string& name) const;
    void Clear();
};
```

> **提示**：脚本名称不区分大小写建议保持统一命名规范，例如 `"Player.Attack.Start"`。

---

## 🔄 生命周期

1. **注册脚本**：游戏初始化时通过 `RegisterScript()` 注册需要的回调。  
2. **状态机触发**：`SpriteAnimationSystem` 在状态进入、退出、过渡、帧事件等时机调用 `ExecuteScript()`。  
3. **脚本执行**：回调接收到 `EntityID` 与 `SpriteAnimationEvent`，可访问事件类型、当前剪辑、帧索引、状态名称等上下文数据。  
4. **卸载脚本**：在场景切换或模块卸载时调用 `UnregisterScript()` 或 `Clear()`，避免悬挂引用。

---

## 🚦 常见事件类型

`SpriteAnimationEvent::Type` 支持以下枚举：

| 事件 | 触发时机 |
| ---- | -------- |
| `ClipStarted` | `Play()` 或状态切换到新剪辑时执行第一帧前触发 |
| `FrameChanged` | 当前帧索引发生变化 |
| `ClipCompleted` | `Once` 或 `PingPong` 播放模式达到尾部 |
| `StateEntered` / `StateExited` | 状态机切换状态时 |
| `TransitionStarted` / `TransitionCompleted` | 状态过渡开始/结束 |
| `CustomEvent` | 由脚本或外部逻辑显式触发的自定义事件 |

---

## 🧪 使用示例

```cpp
using namespace Render::ECS;

auto& registry = SpriteAnimationScriptRegistry::GetInstance();

registry.RegisterScript("Enemy.Attack.OnEnter",
    [](EntityID entity, const SpriteAnimationEvent& evt) {
        Logger::GetInstance().InfoFormat(
            "[Script] Enemy %u attack enter (clip=%s)", entity.index, evt.clip.c_str());
        AudioSystem::GetInstance().Play("enemy_attack_start");
    });

registry.RegisterScript("Enemy.Attack.OnExit",
    [](EntityID entity, const SpriteAnimationEvent&) {
        Spawner::GetInstance().SpawnHitbox(entity, /*lifetime=*/0.3f);
    });

// 在动画状态机配置中绑定脚本名称：
SpriteAnimationState attackState;
attackState.name = "Attack";
attackState.clipName = "enemy_attack";
attackState.onEnterScript = "Enemy.Attack.OnEnter";
attackState.onExitScript = "Enemy.Attack.OnExit";
```

---

## ⚙️ 系统集成

- `SpriteAnimationSystem` 在以下节点调用脚本注册表：  
  - 状态进入、退出 → 执行 `onEnterScript` / `onExitScript`。  
  - 状态过渡开始、结束 → 执行 `onTransitionScript`。  
  - 帧事件 (`frameScripts`) → 按帧索引调用。  
  - 通用事件监听器 (`SpriteAnimationComponent::eventListeners`) → 可在回调中根据 `SpriteAnimationEvent` 再次调用脚本注册表，实现二次分发。  
- 脚本执行失败（未注册或回调为空）会在日志中输出警告，便于调试。

---

## 🧹 资源管理

- **重复注册**：相同名称重复注册会覆盖旧回调，并输出警告。  
- **卸载**：`UnregisterScript()` 找不到脚本时同样会输出警告，提醒可能出现的拼写或生命周期问题。  
- **清空**：`Clear()` 在场景切换时调用，确保无残留脚本引用。该操作线程安全。

---

## ⚠️ 注意事项

- 回调在 `SpriteAnimationSystem::Update()` 中同步执行，应保持逻辑轻量。如需耗时操作，建议投递到任务队列。  
- 若脚本需要访问其他组件，请通过 `World` 安全地获取（注意锁定顺序）。  
- 建议脚本名称遵循模块化命名，如 `"Player.Run.OnEnter"`、`"UI.Button.Highlight"`，便于组织与批量卸载。  
- 在单元测试或示例中请确保注册/清理对称，避免跨测试污染。

---

## 📚 相关文档

- [SpriteAnimation](SpriteAnimation.md) — 动画组件字段、状态机结构与事件枚举。  
- [SpriteRenderSystem](System.md#spriterendersystem) — 如何在渲染阶段消费动画结果。  
- [SpriteSystemDesign](../SpriteSystemDesign.md) — 项目整体设计与分阶段目标。  
- [42_sprite_state_machine_test.cpp](../../examples/42_sprite_state_machine_test.cpp) — 状态机与脚本综合示例。

---

[上一页](SpriteAnimation.md) | [下一页](SpriteAnimationDebugger.md)

