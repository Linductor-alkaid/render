# SpriteAnimator API 参考

[返回 API 目录](README.md)

---

## 📋 概述

`SpriteAnimator` 基于 `SpriteSheet` 的帧名称驱动动画播放，负责时间累积、循环模式与速度控制，可被即时模式或 ECS 流程重用。  

- **命名空间**：`Render`  
- **头文件**：`<render/sprite/sprite_animator.h>`  
- **线程安全**：否；请在更新线程中独占访问。

---

## 🏗️ 核心结构

```cpp
struct SpriteAnimationClip {
    std::string name;
    std::vector<std::string> frames;
    float frameRate = 12.0f;
    SpritePlaybackMode playbackMode = SpritePlaybackMode::Loop;
};

class SpriteAnimator {
public:
    explicit SpriteAnimator(Sprite* sprite = nullptr);

    void SetSprite(Sprite* sprite);
    void AddClip(const SpriteAnimationClip& clip);

    void Play(const std::string& clipName, bool restart = false);
    void Stop();

    void Update(float deltaTime);

    const std::string& GetCurrentClip() const;
    int GetCurrentFrameIndex() const;

    void SetPlaybackSpeed(float speed);
    float GetPlaybackSpeed() const;
};
```

---

## 🔁 播放模式

`SpritePlaybackMode` 在 `render/ecs/components.h` 中定义，与 ECS 动画组件共用：

- `Loop`：循环播放。  
- `Once`：播放一次后停止在最后一帧。  
- `PingPong`：到尾帧后反向播放。  

`SpriteAnimator` 通过内部 `m_direction` 控制正向/反向切换。

---

## 🔧 使用流程

1. **绑定 Sprite**：调用 `SetSprite()` 或在构造时传入。  
2. **注册剪辑**：使用 `AddClip()` 写入帧序列。  
3. **播放**：`Play("idle")` 将从第一帧开始，按 `frameRate` 推进。  
4. **更新**：每帧调用 `Update(deltaTime)`，内部自动切换帧并写入 `Sprite::SetFrame()`。

---

## 🧩 示例

```cpp
using namespace Render;

SpriteSheet sheet;
// ... 省略纹理与帧的填充

Sprite sprite;
sprite.SetTexture(sheet.GetTexture());

SpriteAnimator animator(&sprite);
animator.AddClip({
    .name = "run",
    .frames = {"run_0", "run_1", "run_2", "run_3"},
    .frameRate = 14.0f,
    .playbackMode = SpritePlaybackMode::Loop
});

animator.Play("run");

while (running) {
    animator.Update(deltaTime);
    // 将 sprite 传给 SpriteRenderer 或 SpriteRenderable
}
```

---

## ⚙️ 与 ECS 的关系

- `SpriteAnimationComponent` 封装了更完整的动画状态（播放速度、事件回调、Dirty 标记等），并由 `SpriteAnimationSystem` 驱动。  
- `SpriteAnimator` 适合即时模式或轻量流程；若需要事件回调、资源依赖跟踪，建议使用 ECS 侧组件。  
- `SpriteAtlas::PopulateAnimationComponent()` 会将 `SpriteAtlasAnimation` 转换为 `SpriteAnimationComponent`，其逻辑与 `SpriteAnimator` 相互兼容。

---

## ⚠️ 注意事项

- `AddClip()` 不会自动去重；在重复导入时请先清理或检测。  
- 若剪辑引用的帧名称不存在，将抛出警告并跳过该帧。  
- `Update()` 内部直接修改绑定的 `Sprite`，请确保在渲染前执行。

---

## 📚 相关文档

- [Sprite](Sprite.md) — 动画写入目标。  
- [SpriteSheet](SpriteSheet.md) — 帧查找。  
- [SpriteRenderer](SpriteRenderer.md) — 即时模式渲染。  
- [SpriteAnimationSystem](System.md#spriteanimationsystem) — ECS 动画系统。  
- [SpriteAtlas](SpriteAtlas.md) — 外部图集与剪辑定义。

---

[上一页](SpriteSheet.md) | [下一页](SpriteAnimation.md)

