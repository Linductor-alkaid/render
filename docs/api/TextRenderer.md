# TextRenderer API 参考

[返回 API 首页](README.md)

---

## 概述

`TextRenderer` 提供即时模式（Immediate Mode）的文本绘制接口，与 `SpriteRenderer` 用法一致：  
在 `Begin()` 与 `End()` 之间调用多次 `Draw()` 收集文本实例，最终依次调用 `TextRenderable` 完成渲染。

- **命名空间**：`Render`  
- **头文件**：`<render/text/text_renderer.h>`  
- **线程安全**：否；请在渲染线程内使用  
- **底层依赖**：`TextRenderable`、`Text`

---

## 类定义

```cpp
class TextRenderer {
public:
    explicit TextRenderer(Renderer* renderer);

    void Begin();
    void Draw(const TextPtr& text,
              const Vector3& position,
              float rotation = 0.0f,
              const Vector2& scale = Vector2(1.0f, 1.0f));
    void End();
};
```

内部缓存 `TextInstance`（文本引用 + 变换数据），`End()` 时为每个实例配置共享的 `TextRenderable` 并调用 `Render()`。

---

## 使用流程

```cpp
TextRenderer textRenderer(renderer);

auto font = CreateRef<Font>();
font->LoadFromFile("assets/fonts/Roboto-Regular.ttf", 28.0f);

auto text = CreateRef<Text>(font);
text->SetString("FPS: 60");
text->SetColor(Color(0.2f, 0.9f, 0.3f, 1.0f));

textRenderer.Begin();
textRenderer.Draw(text, Vector3(32.0f, 48.0f, 0.0f));
textRenderer.End();
```

默认会根据 `Renderer` 的窗口大小设置屏幕空间正交矩阵，因此 `Draw()` 位置以像素为单位（原点位于左上角）。  
若需要自定义视图/投影，可在调用 `Begin()` 前手动设置 `TextRenderable::SetViewProjection()`。

---

## 接口说明

- `Begin()`  
  清空上一帧的实例缓存；请在每帧开始时调用。

- `Draw(text, position, rotation, scale)`  
  将文本加入待绘制列表。  
  - `position`：依据文本的对齐方式解释（默认左上角）。  
    - `TextAlignment::Left`：`position` 为左上角。  
    - `TextAlignment::Center`：`position.x` 为水平中心，`position.y` 仍表示顶部。  
    - `TextAlignment::Right`：`position.x` 为右上角。  
  - `rotation`：绕 Z 轴顺时针旋转角度（度）。  
  - `scale`：额外缩放因子（在文本尺寸基础上）。

- `End()`  
  - 将所有缓存的文本提交给渲染器，内部会按字体纹理、视图/投影矩阵等状态分组批处理，以减少状态切换。
  依次配置 `TextRenderable` 并调用 `Render()`。若未调用将不会输出任何文本。

---

## 注意事项

- `Draw()` 会复制 `TextPtr`（共享指针），确保在渲染阶段文本仍然有效。  
- `Text` 对象的内容或颜色变化会自动刷新纹理，无需额外手动同步。  
- `TextRenderer` 在 `Begin/End` 之间复用内部 `TextRenderable` 对象，并通过 `Renderer::SubmitRenderable()` 与批处理/排序管线协作：  
  - 缺失的材质排序键会自动补全，`materialSortKeyMissing` 保持为 0。  
  - 深度提示按照文本位置写入，透明排序保持 UI 层正确顺序。  
  - `MaterialStateCache` 缓存最近绑定的材质，跳过重复 `Material::Bind()` 调用。  
- 同一字体纹理、视图/投影组合的文本在批处理模式下会自动合并，建议启用 `renderer->SetBatchingMode(BatchingMode::CpuMerge)`；`GpuInstancing` 仍主要用于 Mesh。

---

## 相关文档

- [TextRenderable](TextRenderable.md) — 底层渲染对象。  
- [Text](Text.md) — 文本数据对象，负责生成纹理。  
- [Font](Font.md) — 字体加载与光栅化。  
- [Renderer](Renderer.md) — 渲染队列、排序与状态管理。

---

[上一页](TextRenderable.md) | [下一页](Text.md)


