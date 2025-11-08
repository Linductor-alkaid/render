# Sprite API 参考

[返回 API 目录](README.md)

---

## 📋 概述

`Sprite` 是 2D 精灵的轻量级数据对象，封装纹理引用、帧信息、颜色以及翻转状态，供即时模式与 ECS 流程共同复用。  

- **命名空间**：`Render`  
- **头文件**：`<render/sprite/sprite.h>`  
- **线程安全**：否；`Sprite` 设计用于单线程构建与提交，请在多线程环境中自行同步。

---

## 🏗️ 结构定义

```cpp
struct SpriteFrame {
    Rect uv;
    Vector2 size;
    Vector2 pivot;
};

class Sprite {
public:
    Sprite();

    void SetTexture(const Ref<Texture>& texture);
    Ref<Texture> GetTexture() const;

    void SetFrame(const SpriteFrame& frame);
    const SpriteFrame& GetFrame() const;

    void SetTint(const Color& color);
    Color GetTint() const;

    void SetFlip(bool flipX, bool flipY);
    bool IsFlipX() const;
    bool IsFlipY() const;

    void SetUserData(int userData);
    int GetUserData() const;
};
```

---

## 🔑 关键接口

- `SetTexture / GetTexture`：绑定渲染所需的纹理对象（`Ref<Texture>`）。  
- `SetFrame`：写入一帧的 UV、尺寸和枢轴。典型来源为 `SpriteSheet::GetFrame()`。  
- `SetTint`：设定顶点颜色（RGBA），用于闪烁、渐隐等效果。  
- `SetFlip`：标记 X/Y 镜像翻转，具体由上层渲染器解释。  
- `SetUserData`：保留的整型字段，可在动画事件回调或 UI 系统中传递额外信息。

---

## 🧩 典型用法

```cpp
using namespace Render;

Sprite sprite;
sprite.SetTexture(TextureLoader::GetInstance().LoadTexture("ui_icon", "textures/ui.png", true));

SpriteFrame frame;
frame.uv = Rect(0.0f, 0.0f, 0.25f, 0.25f);      // UV 范围
frame.size = Vector2(64.0f, 64.0f);             // 显示尺寸（像素）
frame.pivot = Vector2(0.5f, 0.5f);              // 以中心为枢轴
sprite.SetFrame(frame);

sprite.SetTint(Color(1.0f, 0.9f, 0.9f, 1.0f));  // 微红的 UI 高亮
sprite.SetFlip(false, true);                    // 上下翻转
sprite.SetUserData(42);                         // 与 UI 状态机联动
```

---

## 🔁 与动画/渲染的协作

- **SpriteAnimator**：在 `Update()` 中迭代帧名称，并调用 `Sprite::SetFrame()` 将当前帧写回。  
- **SpriteRenderer**：将 `Sprite` 序列化为 `SpriteRenderable` 的参数（纹理、UV、颜色等）。  
- **SpriteRenderSystem**：ECS 流程中，`SpriteAnimationComponent` 会将剪辑帧同步到 `SpriteRenderComponent::sourceRect`，其语义等同 `SpriteFrame::uv`。

---

## ⚠️ 注意事项

- `SpriteFrame::uv` 支持像素或归一化坐标；若数值 > 1.0，`SpriteRenderable` 会自动归一化。  
- `Sprite` 本身不保存世界矩阵或排序信息，这些由 `SpriteRenderer` 或 ECS 组件负责。  
- 若需要对同一纹理重复引用，推荐通过 `SpriteSheet` 统一注册帧，避免重复硬编码。

---

## 📚 相关文档

- [SpriteSheet](SpriteSheet.md) — 管理命名帧与图集。  
- [SpriteAnimator](SpriteAnimator.md) — 基于帧列表的播放控制。  
- [SpriteRenderer](SpriteRenderer.md) — 即时模式渲染器。  
- [SpriteRenderable](SpriteRenderable.md) — 底层渲染对象。  
- [SpriteAtlasImporter](SpriteAtlasImporter.md) — 解析外部 JSON 图集。

---

[上一页](SpriteRenderable.md) | [下一页](SpriteSheet.md)

