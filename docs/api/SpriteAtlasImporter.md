# SpriteAtlasImporter API 参考

[返回 API 目录](README.md)

---

## 📋 概述

`SpriteAtlasImporter` 负责解析外部 JSON 图集（例如 TexturePacker 输出），构建 `SpriteAtlas`、`SpriteSheet` 以及 `SpriteAnimationComponent` 默认配置，可与 `ResourceManager` 自动集成。  

- **命名空间**：`Render`  
- **头文件**：`<render/sprite/sprite_atlas_importer.h>`  
- **线程安全**：否；通常在资源加载阶段执行。

---

## 🏗️ 接口概览

```cpp
struct SpriteAtlasImportResult {
    SpriteAtlasPtr atlas;
    SpriteSheet spriteSheet;
    ECS::SpriteAnimationComponent animationComponent;
    std::string defaultAnimation;
    bool autoPlay = false;
};

class SpriteAtlasImporter {
public:
    static std::optional<SpriteAtlasImportResult> LoadFromFile(
        const std::string& filePath,
        const std::string& atlasName,
        std::string& error);

    static bool LoadAndRegister(
        const std::string& filePath,
        const std::string& atlasName,
        std::string& error);
};
```

---

## 🔁 导入流程

1. **读取文件**：`FileUtils::ReadFile()` 读取 JSON 内容。  
2. **解析 meta**：获取纹理路径、图集尺寸、默认动画、自动播放标志。  
3. **加载纹理**：通过 `TextureLoader` 加载并绑定到 `SpriteAtlas`。  
4. **解析帧**：转换像素坐标为 UV，填充 `SpriteAtlasFrame` 与 `SpriteSheet`。  
5. **解析动画**：根据 `frames` 数组创建 `SpriteAtlasAnimation`，处理播放模式与倍速。  
6. **落地组件**：调用 `PopulateAnimationComponent()` 生成 ECS 动画剪辑。  
7. **可选注册**：`LoadAndRegister()` 进一步将纹理、图集注册到 `ResourceManager`。

---

## 🧩 示例

```cpp
std::string error;
auto resultOpt = SpriteAtlasImporter::LoadFromFile(
    "assets/atlases/test_sprite_atlas.json",
    "demo_sprite_atlas",
    error);

if (!resultOpt) {
    Logger::GetInstance().Error("[Atlas] Failed: " + error);
    return;
}

auto result = std::move(resultOpt.value());

// 注册到资源管理器（可选）
ResourceManager::GetInstance().RegisterSpriteAtlas(result.atlas->GetName(), result.atlas);

// 应用到 ECS 组件
auto& world = ...;
ECS::SpriteRenderComponent spriteComp;
spriteComp.texture = result.atlas->GetTexture();
spriteComp.textureName = result.atlas->GetTextureName();
spriteComp.sourceRect = result.spriteSheet.GetFrame("tile_0").uv;
spriteComp.size = result.spriteSheet.GetFrame("tile_0").size;

ECS::SpriteAnimationComponent animationComp = result.animationComponent;
world.AddComponent(entity, spriteComp);
world.AddComponent(entity, animationComp);
```

---

## ⚠️ 错误处理

- 读取/解析失败会返回 `std::nullopt` 并写入 `error` 字符串。  
- 缺失帧或动画引用错误会记录 `ResourceNotFound` 警告，但继续导入其他数据。  
- 若纹理加载失败，导入将终止并返回失败。

---

## 📦 JSON 支持字段

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| `meta.image` | `string` | 纹理相对路径。 |
| `meta.size` | `object` | 图集宽高（可选）。 |
| `meta.defaultAnimation` | `string` | 默认播放的剪辑（可选）。 |
| `meta.autoPlay` | `bool` | 是否自动播放默认剪辑（可选）。 |
| `meta.defaultFrameDuration` | `float` | 动画默认帧时长（可选）。 |
| `frames` | `object` | 帧字典，键为帧名。 |
| `animations` | `object` | 动画字典，包含 `frames` 数组、`frameDuration`、`playbackSpeed`、`playback`。 |

---

## 📚 相关文档

- [SpriteAtlas](SpriteAtlas.md) — 图集对象。  
- [SpriteSheet](SpriteSheet.md) — 帧容器。  
- [SpriteAnimationComponent](System.md#spriteanimationsystem) — ECS 动画组件。  
- [ResourceManager](ResourceManager.md) — 图集注册与生命周期管理。  
- [SpriteRenderer](SpriteRenderer.md) — 导入结果的即时渲染。

---

[上一页](SpriteAtlas.md) | [下一页](SpriteBatcher.md)

