# SpriteRenderer API 参考

[返回 API 目录](README.md)

---

## 📋 概述

`SpriteRenderer` 提供即时模式的 2D 精灵渲染接口，封装批量收集与一次性提交逻辑，内部复用 `SpriteRenderable` 以保证与 ECS 渲染管线一致。  

- **命名空间**：`Render`  
- **头文件**：`<render/sprite/sprite_renderer.h>`  
- **线程安全**：否；请在渲染线程内使用。

---

## 🏗️ 类定义

```cpp
class SpriteRenderer {
public:
    explicit SpriteRenderer(Renderer* renderer);

    void Begin();
    void Draw(const Sprite& sprite,
              const Vector3& position,
              float rotation = 0.0f,
              const Vector2& scale = Vector2(1.0f, 1.0f));
    void End();
};
```

内部维护 `std::vector<SpriteInstance>`：缓存请求直至 `End()` 调用，再依次配置 `SpriteRenderable` 并提交到 `Renderer`。

---

## 🔄 渲染流程

1. `Begin()`：清空实例缓存。  
2. `Draw()`：记录 `Sprite` 数据与变换信息。  
3. `End()`：逐实例设置 `SpriteRenderable` 的纹理、UV、大小、颜色、变换，并调用 `SubmitToRenderer()`。

若需要与其他渲染对象共存，应在渲染循环内调用：

```cpp
renderer.BeginFrame();

spriteRenderer.Begin();
spriteRenderer.Draw(sprite, Vector3(100, 120, 0));
spriteRenderer.Draw(sprite, Vector3(200, 120, 0));
spriteRenderer.End();

renderer.FlushRenderQueue();
renderer.EndFrame();
renderer.Present();
```

---

## ⚙️ 参数说明

- **position**：世界坐标或屏幕坐标（取决于是否设置视图/投影矩阵）。  
- **rotation**：围绕 Z 轴的角度（弧度）。  
- **scale**：局部缩放；会与 `SpriteFrame::size` 联合决定最终大小。  
- **Sprite**：提供纹理、UV、颜色、翻转信息。

---

## 🧩 与批处理的关系

`SpriteRenderer` 同样依赖 `SpriteRenderable` 的共享 quad 网格与 sprite shader。当启用 `Renderer::SetBatchingMode(BatchingMode::GpuInstancing)` 后，`SpriteRenderable` 将交由 `SpriteBatcher` 合并实例，从而减少 Draw Call。

---

## 📣 常见用法

### UI 面板

```cpp
SpriteRenderer spriteRenderer(renderer.get());

spriteRenderer.Begin();
spriteRenderer.Draw(panelSprite, Vector3(0.0f, 0.0f, 0.0f));
spriteRenderer.Draw(iconSprite, Vector3(32.0f, 32.0f, 0.0f));
spriteRenderer.Draw(textSprite, Vector3(64.0f, 32.0f, 0.0f));
spriteRenderer.End();
```

### 带旋转的指针

```cpp
float angleRad = MathUtils::DegToRad(pointerDegrees);
spriteRenderer.Begin();
spriteRenderer.Draw(pointerSprite, hudCenter, angleRad, Vector2(1.0f, 1.0f));
spriteRenderer.End();
```

---

## ⚠️ 注意事项

- `SpriteRenderer` 不自动设置视图/投影矩阵。请确保在进入渲染循环前调用 `SpriteRenderable::SetViewProjection()` 指定屏幕空间矩阵。  
- 若调用 `Draw()` 后忘记 `End()`，实例将不会提交。  
- 所有 `Draw()` 调用都会复制 `Sprite`，请合理规划生命周期以避免不必要的纹理引用。

---

## 📚 相关文档

- [Sprite](Sprite.md) — 渲染源数据。  
- [SpriteSheet](SpriteSheet.md) — 帧管理。  
- [SpriteAnimator](SpriteAnimator.md) — 动画驱动。  
- [SpriteRenderable](SpriteRenderable.md) — 底层渲染对象。  
- [Renderer](Renderer.md) — 渲染器生命周期。  
- [SpriteBatcher](SpriteBatcher.md) — 批处理实现。

---

[上一页](SpriteAnimation.md) | [下一页](SpriteAtlas.md)

