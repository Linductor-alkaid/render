# Font API 参考

[返回 API 首页](README.md)

---

## 概述

`Font` 封装 SDL_ttf 字体对象，提供 TrueType/OTF 文件的加载、字号控制与文本光栅化，并内置简单的文本纹理缓存以避免重复光栅化。  
它会在内部管理 SDL_ttf 生命周期（引用计数），并将光栅化结果转换为引擎 `Texture` 对象。

- **命名空间**：`Render`  
- **头文件**：`<render/text/font.h>`  
- **依赖**：SDL3_ttf、`Texture`  
- **线程安全**：所有方法使用互斥锁保护

---

## 核心接口

```cpp
class Font : public std::enable_shared_from_this<Font> {
public:
    Font();
    ~Font();

    bool LoadFromFile(const std::string& filepath, float pointSize);
    void Close();
    bool IsValid() const;

    const std::string& GetFilePath() const;
    float GetPointSize() const;
    int GetAscent() const;
    int GetDescent() const;
    int GetHeight() const;
    int GetLineSkip() const;

    RasterizedText RenderText(const std::string& text, int wrapWidth = 0) const;
};
```

`RasterizedText` 为辅助结构：

```cpp
struct RasterizedText {
    Ref<Texture> texture;
    Vector2 size;
};
```

---

## 文本光栅化

- `RenderText(text, wrapWidth)`  
  - `text`：UTF-8 字符串。  
  - `wrapWidth`：像素宽度，>0 时启用自动换行。  
  - 返回的 `texture` 使用 RGBA32 格式，默认设置为 `ClampToEdge` + `Linear` 过滤。  
  - 若文本为空，返回空纹理但保留行高信息 `size.y`。  
  - 内部维护最近 64 条渲染结果的缓存，重复请求会直接复用纹理。

内部实现依赖 SDL_ttf 的 `TTF_RenderText_Blended`/`_Wrapped`，并在必要时转换到 `SDL_PIXELFORMAT_RGBA32`。

---

## 生命周期管理

- 多个 `Font` 实例共享 SDL_ttf，全局引用计数为 0 时自动调用 `TTF_Init/TTF_Quit`。  
- `LoadFromFile` 支持重复加载，会在成功前释放上一份字体。  
- `Close()` 可显式释放字体资源；析构函数会自动调用。

---

## 使用示例

```cpp
FontPtr uiFont = CreateRef<Font>();
if (!uiFont->LoadFromFile("assets/fonts/Roboto-Regular.ttf", 32.0f)) {
    LOG_ERROR("Failed to load font");
}

RasterizedText raster = uiFont->RenderText("UI Title");
if (raster.texture) {
    // 将纹理注册到 ResourceManager 或直接交给 Text 对象
}
```

通常情况下不直接使用 `RenderText`，而是由 `Text` 对象负责文本缓存与脏标记。

---

## 相关文档

- [Text](Text.md) — 文本数据对象，封装字符串与颜色。  
- [TextRenderable](TextRenderable.md) — 文本渲染对象。  
- [TextRenderer](TextRenderer.md) — 即时模式文本渲染器。  
- [ResourceManager](ResourceManager.md) — 字体注册与管理。

---

[上一页](Text.md) | [下一页](SpriteAtlas.md)


