# Text API 参考

[返回 API 首页](README.md)

---

## 概述

`Text` 封装文本内容、颜色以及换行宽度，并在需要时将 UTF-8 字符串通过 `Font` 光栅化为纹理。  
对象内部维护脏标记（dirty flag），在读取纹理或尺寸时自动更新，适合在 UI 或世界空间中复用。

- **命名空间**：`Render`  
- **头文件**：`<render/text/text.h>`  
- **依赖**：`Font`（必需）、`Texture`

---

## 核心接口

```cpp
class Text : public std::enable_shared_from_this<Text> {
public:
    Text();
    explicit Text(const FontPtr& font);

    void SetFont(const FontPtr& font);
    FontPtr GetFont() const;

    void SetString(const std::string& text);
    const std::string& GetString() const;

    void SetColor(const Color& color);
    Color GetColor() const;

    void SetWrapWidth(int wrapWidth);
    int GetWrapWidth() const;

    bool EnsureUpdated() const;
    Ref<Texture> GetTexture() const;
    Vector2 GetSize() const;
};
```

所有访问方法为线程安全（内部 `shared_mutex`），但渲染相关调用仍需在主线程执行。

---

## 功能说明

- **字体管理**：`SetFont` 绑定 `Font` 实例，切换字体会触发重新光栅化。  
- **文本内容**：`SetString` 接受 UTF-8 字符串。  
- **颜色**：`SetColor` 仅影响最终着色器输出；纹理由 `Font` 始终以白色生成。  
- **自动换行**：`SetWrapWidth` 指定像素宽度，内部使用 SDL_ttf 包装 API 实现软换行。  
- **延迟更新**：在调用 `GetTexture`、`GetSize` 或 `EnsureUpdated` 时自动刷新纹理，避免重复开销。  
- **对齐控制**：可通过 `SetAlignment(TextAlignment)` 设置水平方向的对齐方式（Left、Center、Right），供 `TextRenderer` 布局使用。

---

## 使用范例

```cpp
FontPtr font = CreateRef<Font>();
font->LoadFromFile("assets/fonts/SourceHanSansSC-Regular.otf", 24.0f);

auto text = CreateRef<Text>(font);
text->SetString("欢迎使用 RenderEngine!\n下一行自动换行：");
text->SetWrapWidth(360);
text->SetColor(Color(1.0f, 0.9f, 0.2f, 1.0f));
text->SetAlignment(TextAlignment::Center);

// 获取纹理与尺寸（若需要手工绘制）
text->EnsureUpdated();
auto texture = text->GetTexture();
Vector2 size = text->GetSize();
```

---

## 注意事项

- 对齐方式当前仅影响水平方向：
  - `Left`：`TextRenderer::Draw` 的 `position.x` 视为文本左边界。
  - `Center`：`position.x` 视为文本水平中心。
  - `Right`：`position.x` 视为文本右边界。
- 纹理缓存以文本整体为粒度，频繁创建大量临时字符串仍会带来 GPU 纹理分配开销。  
- 若 `SetString("")` 设置为空文本，`Text` 将返回空纹理但保留行高信息，以便布局计算。  
- 颜色修改不会强制重新生成纹理，可在每帧动态改变。

---

## 相关文档

- [Font](Font.md) — 字体加载与光栅化。  
- [TextRenderable](TextRenderable.md) — 渲染对象。  
- [TextRenderer](TextRenderer.md) — 即时模式渲染器。  
- [Renderer](Renderer.md) — 渲染器与状态管理。

---

[上一页](TextRenderer.md) | [下一页](Font.md)


