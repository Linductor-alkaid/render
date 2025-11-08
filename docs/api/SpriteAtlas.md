# SpriteAtlas API 参考

[返回 API 目录](README.md)

---

## 📋 概述

`SpriteAtlas` 表示包含帧元数据与动画定义的图集对象，结合 `SpriteSheet` 与 `SpriteAnimationComponent`，实现从外部资源到运行时结构的桥梁。  

- **命名空间**：`Render`  
- **头文件**：`<render/sprite/sprite_atlas.h>`  
- **线程安全**：否；建议通过 `ResourceManager` 统一管理。

---

## 🏗️ 核心数据结构

```cpp
struct SpriteAtlasFrame {
    Rect uv;
    Vector2 size;
    Vector2 pivot;
    Vector2 originalSize;
    Vector2 offset;
    float duration = 0.0f;
};

struct SpriteAtlasAnimation {
    std::vector<std::string> frames;
    float frameDuration = 0.1f;
    float playbackSpeed = 1.0f;
    SpritePlaybackMode playbackMode = SpritePlaybackMode::Loop;
};

class SpriteAtlas {
public:
    void SetName(const std::string& name);
    const std::string& GetName() const;

    void SetTextureName(const std::string& textureName);
    const std::string& GetTextureName() const;

    void SetTexture(const Ref<Texture>& texture);
    Ref<Texture> GetTexture() const;

    void SetTextureSize(int width, int height);
    Vector2 GetTextureSize() const;

    void AddFrame(const std::string& name, const SpriteAtlasFrame& frame);
    bool HasFrame(const std::string& name) const;
    const SpriteAtlasFrame& GetFrame(const std::string& name) const;

    void AddAnimation(const std::string& name, const SpriteAtlasAnimation& animation);
    bool HasAnimation(const std::string& name) const;
    const SpriteAtlasAnimation& GetAnimation(const std::string& name) const;

    void PopulateSpriteSheet(SpriteSheet& sheet) const;
    void PopulateAnimationComponent(SpriteAnimationComponent& component,
                                    const std::string& defaultClip = std::string(),
                                    bool autoPlay = false) const;
};
```

---

## 🔑 功能概览

| 功能 | 说明 |
| ---- | ---- |
| 纹理管理 | 通过 `SetTexture` 与 `SetTextureName` 绑定图集纹理；`SpriteAtlasImporter` 会自动填充。 |
| 帧数据 | `SpriteAtlasFrame` 记录 UV、裁剪偏移、原始尺寸、单帧时长。 |
| 动画定义 | `SpriteAtlasAnimation` 保存帧序列、默认帧时长、播放模式、倍速。 |
| 数据落地 | `PopulateSpriteSheet()` 将帧写入 `SpriteSheet`；`PopulateAnimationComponent()` 将动画写入 ECS 组件。 |

---

## 🧩 与 ResourceManager 的集成

`ResourceManager::RegisterSpriteAtlas()` 会：
- 保存图集引用；
- 向 `ResourceDependencyTracker` 注册纹理依赖；
- 支持 `ForEachSpriteAtlas()`、列表与引用计数查询。

在示例 `40_sprite_animation_test.cpp` 中，导入器会检查 atlas 是否已注册，避免重复加载。

---

## 📦 使用示例

```cpp
SpriteAtlasPtr atlas = std::make_shared<SpriteAtlas>();
atlas->SetName("character");
atlas->SetTexture(TextureLoader::GetInstance().LoadTexture("character_tex", "textures/character.png", true));

SpriteAtlasFrame idle0{
    .uv = Rect(0, 0, 128, 128),
    .size = Vector2(128, 128),
    .pivot = Vector2(0.5f, 0.5f),
    .originalSize = Vector2(128, 128),
    .offset = Vector2(0, 0),
    .duration = 0.1f
};
atlas->AddFrame("idle_0", idle0);
// ... 继续添加帧

SpriteAtlasAnimation idleAnim{
    .frames = {"idle_0", "idle_1", "idle_2", "idle_3"},
    .frameDuration = 0.12f,
    .playbackSpeed = 1.0f,
    .playbackMode = SpritePlaybackMode::Loop
};
atlas->AddAnimation("idle", idleAnim);

SpriteSheet sheet;
atlas->PopulateSpriteSheet(sheet);

SpriteAnimationComponent animComp;
atlas->PopulateAnimationComponent(animComp, "idle", true);
```

---

## ⚠️ 注意事项

- `PopulateAnimationComponent()` 会清空目标组件的剪辑后再写入；请先备份或使用专用实例。  
- 若动画引用了缺失帧，将记录 `ResourceNotFound` 警告并跳过该帧。  
- `SpriteAtlasFrame::duration` 为可选覆盖值；若为 `0`，则使用动画默认帧时长。  
- 在 `SpriteBatcher` 与世界空间渲染中，`size` 与 `pivot` 信息可用于对齐 UI/精灵。

---

## 📚 相关文档

- [Sprite](Sprite.md) — 帧落地对象。  
- [SpriteSheet](SpriteSheet.md) — 帧容器。  
- [SpriteAnimator](SpriteAnimator.md) — 播放控制。  
- [SpriteAtlasImporter](SpriteAtlasImporter.md) — JSON 解析工具。  
- [SpriteAnimationComponent](System.md#spriteanimationsystem) — ECS 动画组件。

---

[上一页](SpriteRenderer.md) | [下一页](SpriteAtlasImporter.md)

