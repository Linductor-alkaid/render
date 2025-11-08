# TextRenderable API 参考

[返回 API 首页](README.md)

---

## 概述

`TextRenderable` 是 2D 文本渲染对象，继承自 `Renderable`，与 `SpriteRenderable` 保持一致的接口设计，用于在渲染管线中提交文本。  
它会读取 `Text` 对象生成的纹理，支持屏幕空间渲染、透视变换以及自定义视图/投影矩阵覆盖。

- **命名空间**：`Render`  
- **头文件**：`<render/renderable.h>`（类定义）  
- **依赖对象**：`Text`、`Font`  
- **线程安全**：内部操作受 `shared_mutex` 保护，但需在渲染线程使用

---

## 类定义

```cpp
class TextRenderable : public Renderable {
public:
    TextRenderable();

    void Render(RenderState* renderState = nullptr) override;
    void SubmitToRenderer(Renderer* renderer) override;

    void SetText(const Ref<Text>& text);
    Ref<Text> GetText() const;

    void SetViewProjectionOverride(const Matrix4& view, const Matrix4& projection);
    void ClearViewProjectionOverride();

    static void SetViewProjection(const Matrix4& view, const Matrix4& projection);
    AABB GetBoundingBox() const override;
};
```

默认层级为 UI 层（`layerID = 800`），并自动设置透明渲染提示以便排序阶段处理。

---

## 主要成员方法

- `SetText(const Ref<Text>& text)`  
  设置文本数据源；接受共享指针以便与资源管理器协同工作。

- `Render(RenderState* state)`  
  读取 `Text` 对象当前的纹理与颜色，将其绘制在共享的四边形网格上。  
  自动启用 Alpha 混合、禁用深度测试和面剔除。

- `SetViewProjection` / `SetViewProjectionOverride`  
  - `SetViewProjection`：设置全局屏幕空间矩阵（常用于 UI）。  
  - `SetViewProjectionOverride`：为单个实例指定专用视图/投影矩阵。

- `GetBoundingBox()`  
  基于最新纹理尺寸与 `Transform` 计算 AABB，用于视锥体裁剪。

---

## 渲染流程简述

1. `Text` 对象在内容或样式变化后自动重新生成纹理。  
2. `Render()` 调用时读取纹理尺寸并将其转换为模型缩放矩阵。  
3. 通过共享的四边形网格与专用着色器（`shaders/text.vert/frag`）进行绘制。  
4. 着色器中使用 `uTextColor` 与纹理 Alpha 通道合成最终颜色。  

---

## 使用示例

```cpp
auto font = CreateRef<Font>();
font->LoadFromFile("assets/fonts/Roboto-Regular.ttf", 32.0f);

auto text = CreateRef<Text>(font);
text->SetString("Hello RenderEngine!");
text->SetColor(Color(0.95f, 0.95f, 0.2f, 1.0f));

TextRenderable textRenderable;
textRenderable.SetText(text);
textRenderable.SetTransform(myTransform);

// 渲染阶段
auto renderState = renderer->GetRenderState();
textRenderable.Render(renderState.get());
```

若在 UI 系统中：在帧开始时调用 `TextRenderable::SetViewProjection(Matrix4::Identity(), orthoMatrix)` 即可使用屏幕坐标。

---

## 注意事项

- 文本纹理由 `Text` 对象管理；`TextRenderable` 不会主动触发刷新。  
- 当 `Text` 为空字符串时不会执行绘制，但仍提供正确的包围盒高度。  
- 若需要世界空间文本（例如 3D 标签），请在 `Transform` 上设置合适的位置与旋转，并根据需要覆盖视图/投影。

---

## 相关文档

- [Text](Text.md) — 文本数据对象，负责生成纹理。
- [TextRenderer](TextRenderer.md) — 即时模式文本渲染器。
- [Font](Font.md) — 字体加载与光栅化。
- [Renderer](Renderer.md) — 渲染队列与排序流程。

---

[上一页](SpriteRenderer.md) | [下一页](TextRenderer.md)


