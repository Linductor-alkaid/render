# SpriteSheet API 参考

[返回 API 目录](README.md)

---

## 📋 概述

`SpriteSheet` 负责管理同一纹理中的多帧数据，提供名称索引、批量迭代等能力，是 `SpriteAnimator` 与 `SpriteAtlas` 的基础容器。  

- **命名空间**：`Render`  
- **头文件**：`<render/sprite/sprite_sheet.h>`  
- **线程安全**：否；请在构建阶段或主线程中维护。

---

## 🏗️ 类定义

```cpp
class SpriteSheet {
public:
    SpriteSheet();

    void SetTexture(const Ref<Texture>& texture);
    Ref<Texture> GetTexture() const;

    void AddFrame(const std::string& name, const SpriteFrame& frame);
    bool HasFrame(const std::string& name) const;
    const SpriteFrame& GetFrame(const std::string& name) const;

    const std::unordered_map<std::string, SpriteFrame>& GetAllFrames() const;
};
```

---

## 🔑 关键接口

| 接口 | 说明 |
| ---- | ---- |
| `SetTexture` | 绑定整张图集的纹理引用。 |
| `AddFrame` | 插入命名帧；若名称已存在，将被覆盖。 |
| `GetFrame` | 以常量引用返回帧信息，若名称缺失会触发断言。 |
| `GetAllFrames` | 返回内部字典，用于自定义遍历或调试。 |

---

## 🧱 数据模型

- `SpriteFrame::uv`：帧的 UV 矩形，支持像素或归一化坐标。  
- `SpriteFrame::size`：推荐设置为帧的像素宽高，便于渲染时直接复用。  
- `SpriteFrame::pivot`：默认 `(0.5, 0.5)`，即以中心为枢轴，可用于控制旋转/对齐。

---

## 🧩 示例

```cpp
using namespace Render;

SpriteSheet sheet;
sheet.SetTexture(TextureLoader::GetInstance().LoadTexture("atlas_ui", "assets/ui_atlas.png", true));

sheet.AddFrame("button_normal", SpriteFrame{
    .uv = Rect(0.0f, 0.0f, 128.0f, 64.0f),
    .size = Vector2(128.0f, 64.0f),
    .pivot = Vector2(0.5f, 0.5f)
});

sheet.AddFrame("button_hover", SpriteFrame{
    .uv = Rect(128.0f, 0.0f, 128.0f, 64.0f),
    .size = Vector2(128.0f, 64.0f),
    .pivot = Vector2(0.5f, 0.5f)
});

Sprite sprite;
sprite.SetTexture(sheet.GetTexture());
sprite.SetFrame(sheet.GetFrame("button_normal"));
```

---

## 🔁 与其他模块的协作

- **SpriteAnimator**：使用帧名称驱动动画序列，依赖 `SpriteSheet::GetFrame()` 在播放过程中切换。  
- **SpriteAtlasImporter**：读取 JSON/TexturePacker 等格式后调用 `PopulateSpriteSheet()` 填充帧数据。  
- **ResourceManager**：通过 `RegisterSpriteAtlas()` 统一管理纹理与帧，避免重复加载。  
- **ECS 流程**：`SpriteAtlas` 转写 `SpriteAnimationComponent` 时会将帧转为 `Rect`，与 `SpriteSheet` 数据保持一致。

---

## ⚠️ 注意事项

- `SpriteSheet` 不做线程同步；建议在资源初始化阶段填充，运行时仅查询。  
- 若帧名称查找失败将抛出 `ResourceNotFound` 错误，请在添加帧后调用 `HasFrame()` 做防御性检查。  
- 对于需要九宫格或偏移量的帧，请使用 `SpriteAtlas`，其包含更完整的元信息。

---

## 📚 相关文档

- [Sprite](Sprite.md) — 精灵数据对象。  
- [SpriteAnimator](SpriteAnimator.md) — 播放控制器。  
- [SpriteAtlas](SpriteAtlas.md) — 带元数据的图集封装。  
- [SpriteAtlasImporter](SpriteAtlasImporter.md) — 图集导入工具。

---

[上一页](Sprite.md) | [下一页](SpriteAnimator.md)

