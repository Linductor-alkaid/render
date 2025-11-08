# SpriteAnimation API 参考

[返回 API 目录](README.md)

---

## 📋 概述

`SpriteAnimationComponent` 搭配 `SpriteAnimationSystem` 提供基于 ECS 的精灵动画播放、状态机驱动与事件脚本化能力。组件存储动画剪辑、播放状态、状态机配置与参数，系统在 `Update()` 中推进时间、更新 `SpriteRenderComponent::sourceRect`，并触发 `ClipStarted`、`FrameChanged`、`ClipCompleted` 等事件及脚本。

- **命名空间**：`Render::ECS`
- **头文件**：
  - `components.h` — 组件与状态机数据结构
  - `sprite_animation_script_registry.h` — 脚本注册中心
- **系统实现**：`SpriteAnimationSystem`（`systems.cpp`）

---

## 🧱 数据结构

### 动画剪辑

```cpp
struct SpriteAnimationClip {
    std::vector<Rect> frames;
    float frameDuration = 0.1f;
    bool loop = true;
    SpritePlaybackMode playbackMode = SpritePlaybackMode::Loop;
};
```

### 动画事件

```cpp
struct SpriteAnimationEvent {
    enum class Type { ClipStarted, ClipCompleted, FrameChanged };
    Type type = Type::FrameChanged;
    std::string clip;
    int frameIndex = 0;
};
```

### 状态机描述

```cpp
struct SpriteAnimationTransitionCondition {
    enum class Type {
        Always,
        StateTimeGreater,
        Trigger,
        BoolEquals,
        FloatGreater,
        FloatLess,
        OnEvent
    };

    Type type = Type::Always;
    std::string parameter;
    float threshold = 0.0f;
    bool boolValue = true;
    SpriteAnimationEvent::Type eventType = SpriteAnimationEvent::Type::FrameChanged;
    std::string eventClip;
    int eventFrame = -1;
};

struct SpriteAnimationStateTransition {
    std::string fromState;
    std::string toState;
    std::vector<SpriteAnimationTransitionCondition> conditions;
    bool once = false;
    bool consumed = false;
};

struct SpriteAnimationState {
    std::string name;
    std::string clip;
    float playbackSpeed = 1.0f;
    std::optional<SpritePlaybackMode> playbackMode;
    bool resetOnEnter = true;
    std::vector<std::string> onEnterScripts;
    std::vector<std::string> onExitScripts;
};

struct SpriteAnimationScriptBinding {
    SpriteAnimationEvent::Type eventType = SpriteAnimationEvent::Type::FrameChanged;
    std::string clip;
    int frameIndex = -1;
    std::string scriptName;
};
```

### `SpriteAnimationComponent`

```cpp
struct SpriteAnimationComponent {
    // 基础播放状态
    std::unordered_map<std::string, SpriteAnimationClip> clips;
    std::string currentClip;
    int currentFrame = 0;
    float timeInFrame = 0.0f;
    float playbackSpeed = 1.0f;
    bool playing = false;
    bool dirty = false;
    int playbackDirection = 1;
    bool clipJustChanged = false;
    std::vector<SpriteAnimationEvent> events;

    using EventListener = std::function<void(EntityID, const SpriteAnimationEvent&)>;
    std::vector<EventListener> eventListeners;

    // 状态机
    std::unordered_map<std::string, SpriteAnimationState> states;
    std::vector<SpriteAnimationStateTransition> transitions;
    std::vector<SpriteAnimationScriptBinding> scriptBindings;
    std::string defaultState;
    std::string currentState;
    float stateTime = 0.0f;

    // 参数
    std::unordered_map<std::string, bool> boolParameters;
    std::unordered_map<std::string, float> floatParameters;
    std::unordered_set<std::string> triggers;

    // 基础接口
    void Play(const std::string& clipName, bool restart = true);
    void Stop(bool resetFrame = false);
    void SetPlaybackSpeed(float speed);
    bool HasClip(const std::string& clipName) const;
    void ClearEvents();
    void AddEventListener(const EventListener& listener);
    void ClearEventListeners();

    // 参数与触发器
    void SetBoolParameter(const std::string& name, bool value);
    bool GetBoolParameter(const std::string& name, bool defaultValue = false) const;
    void SetFloatParameter(const std::string& name, float value);
    float GetFloatParameter(const std::string& name, float defaultValue = 0.0f) const;
    void SetTrigger(const std::string& name);
    bool ConsumeTrigger(const std::string& name);
    void ResetTrigger(const std::string& name);
    void ClearTriggers();

    // 状态机配置
    void AddState(const SpriteAnimationState& state);
    void AddTransition(const SpriteAnimationStateTransition& transition);
    void AddScriptBinding(const SpriteAnimationScriptBinding& binding);
    void SetDefaultState(const std::string& stateName);
    bool HasState(const std::string& stateName) const;
};
```

---

## 🔄 系统行为

### 更新流程

1. 清空上一帧事件。
2. 若配置状态机且当前状态为空，切换至 `defaultState` 或第一个注册的状态。
3. 推进 `stateTime`（除非刚进入新状态）。
4. 推进动画帧并更新 `SpriteRenderComponent::sourceRect`。
5. 将 `ClipStarted` / `FrameChanged` / `ClipCompleted` 写入 `events`。
6. 依次触发 `eventListeners` 与脚本绑定。
7. 评估状态迁移条件：
   - `Always`：无条件迁移。
   - `StateTimeGreater`：`stateTime >= threshold`。
   - `Trigger`：`SetTrigger` 后等待迁移，迁移成功即自清除。
   - `BoolEquals` / `FloatGreater` / `FloatLess`：读取参数。
   - `OnEvent`：匹配当前帧产生的事件（可过滤 clip / frameIndex）。
8. 状态切换时执行：
   - Exit 脚本（旧状态 `onExitScripts`）。
   - 调整剪辑播放模式与速度。
   - 调用 `Play()`，重置为首帧并置 `dirty`。
   - Enter 脚本（新状态 `onEnterScripts`）。

### 参数接口

```cpp
animComp.SetBoolParameter("isGrounded", true);
animComp.SetFloatParameter("speed", velocity.length());
animComp.SetTrigger("attack");
```

- `Trigger` 一经使用自动清除；可调用 `ResetTrigger()` 手动取消。
- 布尔与浮点参数可参与多条件组合。

### 状态与过渡配置

```cpp
SpriteAnimationState idle{
    .name = "idle",
    .clip = "idle_clip",
    .playbackSpeed = 1.0f,
    .playbackMode = SpritePlaybackMode::Loop,
    .onEnterScripts = {"Anim.OnIdleEnter"},
};
animComp.AddState(idle);

SpriteAnimationStateTransition toRun{
    .fromState = "idle",
    .toState = "run",
    .conditions = {
        {.type = SpriteAnimationTransitionCondition::Type::FloatGreater,
         .parameter = "speed",
         .threshold = 0.2f}}
};
animComp.AddTransition(toRun);
```

- `fromState` 为空表示任意状态可触发。
- `once` / `consumed` 用于一次性过渡（播放完后不再重复）。
- 状态切换会自动同步 `SpriteRenderComponent::sourceRect` 为新剪辑第一帧。

---

## 🧩 事件脚本化

### 注册脚本

```cpp
using ScriptRegistry = SpriteAnimationScriptRegistry;

ScriptRegistry::Register("Anim.PlayFootstep",
    [](EntityID entity, const SpriteAnimationEvent& evt, SpriteAnimationComponent& comp) {
        AudioSystem::Get().Play3D("footstep", entity);
    });
```

- **头文件**：`<render/ecs/sprite_animation_script_registry.h>`
- `Invoke` 若找不到脚本名称，将记录警告但不中断流程。

### 绑定事件

```cpp
SpriteAnimationScriptBinding binding{
    .eventType = SpriteAnimationEvent::Type::FrameChanged,
    .clip = "run_clip",
    .frameIndex = 1,
    .scriptName = "Anim.PlayFootstep"
};
animComp.AddScriptBinding(binding);
```

- `clip` 为空则匹配任意剪辑。
- `frameIndex < 0` 表示忽略帧索引。
- 事件脚本在 `eventListeners` 之后执行，可在脚本内部访问并修改组件参数。

---

## 🧪 综合示例

```cpp
SpriteAnimationComponent anim{};
anim.clips["idle_clip"] = idleClip;
anim.clips["run_clip"]  = runClip;
anim.clips["attack_clip"] = attackClip;

anim.AddState({
    .name = "idle",
    .clip = "idle_clip",
    .playbackSpeed = 1.0f,
    .playbackMode = SpritePlaybackMode::Loop,
    .onEnterScripts = {"Anim.OnIdleEnter"}
});
anim.AddState({
    .name = "run",
    .clip = "run_clip",
    .playbackSpeed = 1.0f,
    .playbackMode = SpritePlaybackMode::Loop
});
anim.AddState({
    .name = "attack",
    .clip = "attack_clip",
    .playbackSpeed = 1.2f,
    .playbackMode = SpritePlaybackMode::Once,
    .onExitScripts = {"Anim.OnAttackFinished"}
});
anim.SetDefaultState("idle");

anim.AddTransition({
    .fromState = "idle",
    .toState = "run",
    .conditions = {
        {.type = SpriteAnimationTransitionCondition::Type::FloatGreater,
         .parameter = "speed",
         .threshold = 0.2f}}
});
anim.AddTransition({
    .fromState = "run",
    .toState = "idle",
    .conditions = {
        {.type = SpriteAnimationTransitionCondition::Type::FloatLess,
         .parameter = "speed",
         .threshold = 0.15f}}
});
anim.AddTransition({
    .fromState = "",
    .toState = "attack",
    .conditions = {
        {.type = SpriteAnimationTransitionCondition::Type::Trigger,
         .parameter = "attackTrigger"}},
    .once = true
});

anim.AddScriptBinding({
    .eventType = SpriteAnimationEvent::Type::FrameChanged,
    .clip = "run_clip",
    .frameIndex = 1,
    .scriptName = "Anim.PlayFootstep"
});
```

在游戏逻辑中只需设置参数即可驱动状态机：

```cpp
animComp.SetFloatParameter("speed", characterVelocity.Length());
if (input.AttackPressed()) {
    animComp.SetTrigger("attackTrigger");
}
```

---

## ⚠️ 注意事项

- 触发器默认在成功迁移后自动清除，可使用 `ClearTriggers()` 清空所有触发器。
- `ClipStarted` / `ClipCompleted` 也会驱动状态脚本（Enter/Exit）。
- 状态切换在同帧完成，但渲染器将在下一帧统一处理 `dirty` 标记；为避免闪烁，系统会即时写入新帧首帧 UV。
- 请确保所有脚本先通过 `SpriteAnimationScriptRegistry::Register` 注册，建议在模块初始化阶段执行。

---

## 📚 相关文档

- [SpriteAnimator](SpriteAnimator.md) — 即时模式动画器（非 ECS）。
- [SpriteRenderer](SpriteRenderer.md) — 即时模式渲染路径。
- [SpriteRenderSystem](System.md#spriterendersystem) — ECS 渲染系统实现。
- [SpriteAtlas](SpriteAtlas.md) — 图集与剪辑导入工具。
- [SpriteRenderLayer](SpriteRenderLayer.md) — 层级与排序工具。
- [SpriteAnimationDebugger](SpriteAnimationDebugger.md) — Debug 构建下的状态机快照与调试面板。

---

[上一页](SpriteAnimator.md) | [下一页](SpriteRenderer.md)

