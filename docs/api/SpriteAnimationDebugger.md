[返回 API 目录](README.md)

---

# SpriteAnimationDebugger / SpriteAnimationDebugPanel

## 📋 概述

`SpriteAnimationDebugger` 与 `SpriteAnimationDebugPanel` 为 Debug 构建提供运行时调试能力：记录动画状态机快照、事件日志、脚本回调，并允许注入调试指令。面板默认以文本形式输出快照，可集成到 Logger 或自研调试 HUD。

- **命名空间**：`Render::ECS`  
- **头文件**：`<render/debug/sprite_animation_debugger.h>`、`<render/debug/sprite_animation_debug_panel.h>`  
- **实现文件**：`src/debug/sprite_animation_debugger.cpp`、`src/debug/sprite_animation_debug_panel.cpp`  
- **可用性**：仅在 `DEBUG/_DEBUG` 构建启用（Release 返回空操作）

---

## 🧱 核心类

```cpp
class SpriteAnimationDebugger {
public:
    static SpriteAnimationDebugger& GetInstance();

    void SetEnabled(bool enabled);
    bool IsEnabled() const;
    void WatchAllEntities(bool enable);
    bool IsWatchingAll() const;

    void WatchEntity(EntityID entity);
    void UnwatchEntity(EntityID entity);
    bool IsEntityWatched(EntityID entity) const;
    std::vector<EntityID> GetWatchedEntities() const;

    void QueueCommand(EntityID entity, const Command& command);
    void ApplyPendingCommands(EntityID entity, SpriteAnimationComponent& component);

    void CaptureSnapshot(EntityID entity,
                         const SpriteAnimationComponent& animComp,
                         const SpriteRenderComponent& spriteComp);
    void AppendEvents(EntityID entity,
                      const std::vector<SpriteAnimationEvent>& events,
                      EventOrigin origin = EventOrigin::StateMachine,
                      const std::string& note = {});
    void RecordScriptInvocation(EntityID entity,
                                const std::string& scriptName,
                                const SpriteAnimationEvent& event);

    bool GetSnapshot(EntityID entity,
                     SpriteAnimationStateMachineDebug& outState,
                     SpriteRenderComponent& outSprite,
                     std::vector<EventLogEntry>& outEvents) const;
};
```

```cpp
class SpriteAnimationDebugPanel {
public:
    void SetIncludeEvents(bool include);
    void SetIncludeParameters(bool include);
    void SetIncludeSpriteInfo(bool include);
    void SetMaxEventsPerEntity(size_t count);

    void WatchEntity(EntityID entity) const;
    void WatchAllEntities(bool enable) const;
    void UnwatchEntity(EntityID entity) const;

    std::vector<std::string> BuildPanelLines() const;
    std::string BuildPanelString() const;
    void RenderToLogger() const;
};
```

---

## 🔍 快照内容

`SpriteAnimationDebugger` 为每个被监听的实体保存以下数据：

- 状态机概览：当前状态、默认状态、当前剪辑、帧索引、播放速度、`stateTime`、`playing` 标记。
- 参数集合：布尔/浮点参数、活动 Trigger 列表。
- 事件日志：最近触发的 `SpriteAnimationEvent`（含序号、来源、可选注解）；脚本回调通过 `RecordScriptInvocation()` 自动插入。
- 精灵快照：`SpriteRenderComponent` 的 `layerID`、`sortOrder`、`screenSpace` 等字段，方便与批处理分析结合。

> 日志默认保留 128 条，最新事件追加至队尾，可通过 `SetMaxEventsPerEntity()` 控制面板输出的可视条数。

---

## 🎮 调试指令

`QueueCommand()` 支持以下操作（下一帧由系统调用 `ApplyPendingCommands()` 生效）：

| 命令 | 说明 |
| ---- | ---- |
| `SetBool` / `SetFloat` | 设置状态机参数，触发条件会在下一帧重新评估 |
| `Trigger` / `ResetTrigger` | 激活或清除 Trigger |
| `ForceState` | 强制切换到指定状态（自动调用 `Play()`，可选重置计时） |
| `QueueEvent` | 注入自定义事件（由 `SpriteAnimationComponent::debugEventQueue` 转入本帧事件列表） |
| `ClearEvents` | 清空调试事件队列与当前帧事件，便于逐条观察 |

---

## 🧪 示例用法

```cpp
using namespace Render::ECS;

SpriteAnimationDebugPanel panel;
panel.WatchAllEntities(true);
panel.SetMaxEventsPerEntity(5);
panel.SetIncludeSpriteInfo(true);

// 主循环中周期性输出快照
accumulator += deltaTime;
if (accumulator >= 2.0) {
    Logger::GetInstance().Info("[Debug] Sprite animation snapshot");
    panel.RenderToLogger();
    accumulator = 0.0;
}

// 向实体注入调试指令
SpriteAnimationDebugger::Command cmd{};
cmd.type = SpriteAnimationDebugger::CommandType::ForceState;
cmd.parameter = "attack";
SpriteAnimationDebugger::GetInstance().QueueCommand(entity, cmd);
```

`examples/42_sprite_state_machine_test.cpp` 已集成该面板：Debug 构建下会每 2 秒打印一次三位角色的状态、参数与最近事件。

---

## ⚙️ 集成说明

- **系统接入**：`SpriteAnimationSystem` 在 Debug 构建中自动调用 `ApplyPendingCommands()`、`CaptureSnapshot()` 与 `AppendEvents()`，无需额外接线。Release 构建宏展开为空操作。
- **脚本联动**：脚本注册表在脚本成功执行后会通过 `RecordScriptInvocation()` 写入日志，可对照事件顺序验证回调链。
- **批处理扩展**：快照中包含 `SpriteRenderComponent` 基础信息，后续可拓展至记录批次索引或 `SpriteRenderLayer` 名称。

---

## ⚠️ 注意事项

- 调试器仅在 Debug 构建启用（`DEBUG` 或 `_DEBUG` 定义），Release 中所有接口安全短路。
- 快照/命令操作均受互斥锁保护，但仍建议避免在渲染热路径中频繁创建大体量对象。
- 建议在场景切换时调用 `SetEnabled(false)` 或 `WatchAllEntities(false)` 清理数据，避免过期实体持有快照。
- 如需图形化 UI，可基于 `BuildPanelLines()` 返回的数据驱动自定义面板。

---

## 📚 相关文档

- [SpriteAnimation](SpriteAnimation.md) — 动画组件结构、状态机字段、事件类型。
- [SpriteAnimationScriptRegistry](SpriteAnimationScriptRegistry.md) — 脚本注册与回调机制。
- [SpriteBatch](SpriteBatch.md) — 批处理框架概览，可与调试器结合分析批次拆分。
- [SpriteSystemDesign](../SpriteSystemDesign.md) — 状态机调试工具设计与路线图。
- [42_sprite_state_machine_test.cpp](../../examples/42_sprite_state_machine_test.cpp) — 示例演示调试输出。

---

[上一页](SpriteAnimationScriptRegistry.md) | [下一页](SpriteBatch.md)


