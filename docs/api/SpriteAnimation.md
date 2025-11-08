# SpriteAnimation API 参考

[返回 API 目录](README.md)

---

## 📋 概述

`SpriteAnimationComponent` 搭配 `SpriteAnimationSystem` 提供基于 ECS 的精灵动画播放、事件分发与资源映射能力。组件保存动画剪辑、播放状态、事件监听器，系统在 `Update()` 中推进时间、写回 `SpriteRenderComponent::sourceRect`，并触发 `ClipStarted`、`FrameChanged`、`ClipCompleted` 等事件。

- **命名空间**：`Render::ECS`
- **头文件**：`<render/ecs/components.h>`
- **系统实现**：`<render/ecs/systems.h>` / `SpriteAnimationSystem`

---

## 🧱 数据结构

### `SpriteAnimationClip`

```cpp
struct SpriteAnimationClip {
    std::vector<Rect> frames;
    float frameDuration = 0.1f;
    bool loop = true;
    SpritePlaybackMode playbackMode = SpritePlaybackMode::Loop;
};
```

- `frames`：UV 或像素坐标矩形，驱动 `SpriteRenderComponent::sourceRect`。
- `frameDuration`：每帧持续时间（秒）。
- `loop`：兼容旧数据的循环标记；若为 `false` 将强制播放模式降级为 `Once`。
- `playbackMode`：`Loop` / `Once` / `PingPong`。

### `SpriteAnimationEvent`

```cpp
struct SpriteAnimationEvent {
    enum class Type { ClipStarted, ClipCompleted, FrameChanged };
    Type type = Type::FrameChanged;
    std::string clip;
    int frameIndex = 0;
};
```

- `ClipStarted`：调用 `Play()` 后，系统在第一帧触发。
- `FrameChanged`：帧索引变化时触发；系统会保证至少触发一次（即便没有跨帧）。
- `ClipCompleted`：`Once` / `PingPong` 结束时触发。

### `SpriteAnimationComponent`

```cpp
struct SpriteAnimationComponent {
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

    void Play(const std::string& clipName, bool restart = true);
    void Stop(bool resetFrame = false);
    void SetPlaybackSpeed(float speed);
    bool HasClip(const std::string& clipName) const;
    void ClearEvents();
    void AddEventListener(const EventListener& listener);
    void ClearEventListeners();
};
```

- `events`：本帧生成的事件集合，系统在下一帧回合首自动清空。
- `eventListeners`：即时事件回调，`SpriteAnimationSystem` 在生成事件后逐一触发。
- `dirty`：设置为 `true` 时，无论时间推进与否都会刷新帧。

---

## 🔄 系统行为

### 注册与执行顺序

```cpp
world->RegisterSystem<TransformSystem>();
world->RegisterSystem<CameraSystem>();
world->RegisterSystem<SpriteAnimationSystem>();
world->RegisterSystem<SpriteRenderSystem>(renderer);
```

`SpriteAnimationSystem` 默认优先级为 150（在 `TransformSystem` 之后，`SpriteRenderSystem` 之前）。系统逻辑：

1. 清空组件上的 `events`。
2. 推进 `timeInFrame`，根据 `playbackMode` 调整 `currentFrame`、`playing`、`playbackDirection`。
3. 写回 `SpriteRenderComponent::sourceRect` 与 `dirty` 状态。
4. 产生事件并调用所有 `eventListeners`。

### 事件分发

```cpp
animComp.AddEventListener(
    [](EntityID entity, const SpriteAnimationEvent& evt) {
        switch (evt.type) {
        case SpriteAnimationEvent::Type::ClipStarted:
            Logger::GetInstance().InfoFormat("Entity %u clip '%s' started", entity.index, evt.clip.c_str());
            break;
        case SpriteAnimationEvent::Type::ClipCompleted:
            Logger::GetInstance().InfoFormat("Entity %u clip '%s' completed", entity.index, evt.clip.c_str());
            break;
        case SpriteAnimationEvent::Type::FrameChanged:
            // 使用 evt.frameIndex 触发帧标记或音效
            break;
        }
    });
```

- 回调在系统 `Update()` 期间同步触发，请确保逻辑轻量或自行排队。
- `events` 向量仍然保留，可用于 UI 层轮询或调试。

---

## 🧪 使用示例

```cpp
auto entity = world->CreateEntity({});
SpriteRenderComponent sprite{};
sprite.texture = myTexture;
sprite.resourcesLoaded = true;
world->AddComponent(entity, sprite);

SpriteAnimationComponent anim{};
anim.clips["run"] = SpriteAnimationClip{
    .frames = {Rect(0,0,64,64), Rect(64,0,64,64), Rect(128,0,64,64)},
    .frameDuration = 0.12f,
    .loop = true,
    .playbackMode = SpritePlaybackMode::Loop
};
anim.Play("run");
anim.AddEventListener([](EntityID id, const SpriteAnimationEvent& evt) {
    if (evt.type == SpriteAnimationEvent::Type::FrameChanged && evt.frameIndex == 0) {
        Logger::GetInstance().DebugFormat("Entity %u looped frame 0", id.index);
    }
});
world->AddComponent(entity, anim);
```

---

## ⚠️ 注意事项

- `frames` 支持像素或归一化坐标；系统会在渲染阶段自动转换。
- `playbackSpeed` 支持负值，用于倒放；`PingPong` 模式下会根据方向自动反向。
- 若 `clips` 中缺少 `currentClip`，系统不会更新 `SpriteRenderComponent`。
- `eventListeners` 保存在组件上，如需重置可调用 `ClearEventListeners()`。

---

## 📚 相关文档

- [SpriteAnimator](SpriteAnimator.md) — 即时模式动画器。
- [SpriteRenderer](SpriteRenderer.md) — 即时模式渲染路径。
- [SpriteRenderSystem](System.md#spriterendersystem) — ECS 渲染系统。
- [SpriteAtlas](SpriteAtlas.md) — 图集与剪辑导入。
- [SpriteRenderLayer](SpriteRenderLayer.md) — 层级与排序工具。

---

[上一页](SpriteAnimator.md) | [下一页](SpriteRenderer.md)

