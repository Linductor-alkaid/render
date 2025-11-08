# SpriteRenderLayer API 参考

[返回 API 目录](README.md)

---

## 📋 概述

`SpriteRenderLayer` 提供基于名称的 UI/世界层级映射，以统一管理 `layerID` 与默认排序偏移，让 UI 代码能以语义化方式设置精灵层次。  

- **命名空间**：`Render`  
- **头文件**：`<render/sprite/sprite_layer.h>`  
- **线程安全**：是（内部使用互斥锁保护静态表）。

---

## 🏗️ 核心接口

```cpp
class SpriteRenderLayer {
public:
    struct LayerInfo {
        uint32_t layerID;
        int32_t sortBias;
    };

    static void RegisterLayer(const std::string& name, uint32_t layerID, int32_t sortBias = 0);
    static void RegisterLayers(const std::vector<std::pair<std::string, LayerInfo>>& layers);
    static std::optional<LayerInfo> GetLayer(const std::string& name);
    static bool HasLayer(const std::string& name);
    static bool ApplyLayer(const std::string& name, ECS::SpriteRenderComponent& component, int32_t localOrder = 0);
    static std::vector<std::pair<std::string, LayerInfo>> ListLayers();
    static void ResetToDefaults();
};
```

---

## 🔁 默认层列表

| 名称 | layerID | sortBias | 用途 |
| ---- | ------- | -------- | ---- |
| `world.background` | 680 | -100 | 世界背景元素 |
| `world.midground` | 700 | 0 | 世界主体 |
| `world.foreground` | 720 | +100 | 世界前景特效 |
| `ui.background` | 780 | -200 | 全屏背景板 |
| `ui.panel` | 790 | -50 | UI 面板 |
| `ui.default` | 800 | 0 | 一般 UI 控件 |
| `ui.foreground` | 810 | +50 | 高亮按钮 / 前景特效 |
| `ui.overlay` | 900 | 0 | 叠加层（弹窗、HUD） |
| `ui.tooltip` | 910 | +50 | 提示/光标 |
| `debug.overlay` | 999 | 0 | 调试覆盖 |

层名称不区分大小写，可根据项目需求覆盖或新增。

---

## 🧩 示例

```cpp
using namespace Render;
using namespace Render::ECS;

SpriteRenderComponent spriteComp;
SpriteRenderLayer::ApplyLayer("ui.panel", spriteComp);

// 叠加额外排序偏移，确保按钮排列顺序
SpriteRenderLayer::ApplyLayer("ui.default", spriteComp, localOrder /*=按钮索引*/);

// 注册自定义层
SpriteRenderLayer::RegisterLayer("ui.minimap", 850, 10);
```

---

## ⚙️ 与 ECS 的结合

- `ApplyLayer()` 直接写入 `SpriteRenderComponent::layerID` 与 `sortOrder`，可在组件构建或运行时调整。  
- 当与 `SpriteRenderSystem` 搭配使用时，layerID 会决定渲染队列的层顺序，sortOrder 在同层内部细分优先级。
- 引擎默认在 `SpriteRenderSystem` 中为 `screenSpace && layerID==800 && sortOrder==0` 的精灵应用 `ui.default` 层，为世界空间且保持默认设置的精灵应用 `world.midground` 层。

---

## 📦 管理与查询

- `RegisterLayer`：覆盖同名层；支持运行时配置 UI 层级。  
- `RegisterLayers`：便捷的批量注册接口。  
- `ListLayers`：返回排序后的层名与信息，可用于调试界面。  
- `ResetToDefaults`：清除自定义层，回到内置默认配置。

---

## 📚 相关文档

- [SpriteRenderComponent](System.md#spriterendersystem) — 层 ID 与排序字段。  
- [SpriteBatcher](SpriteBatcher.md) — 依据 layerID 聚合批次。  
- [SpriteRenderer](SpriteRenderer.md) — 即时模式渲染时也可调用 `ApplyLayer`。

---

[上一页](SpriteBatcher.md) | [下一页](System.md)

