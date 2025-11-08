[返回文档首页](../README.md)

---

# 2D UI 集成指南

## 🎯 目标

本文汇总基于精灵管线构建 UI 的常见做法：如何在 ECS 中组织 UI 元素、控制屏幕/世界坐标渲染、配置渲染层级、与批处理系统协作，以及如何利用资源管理与脚本事件实现交互反馈。

- **适用范围**：`SpriteRenderComponent`、`SpriteAnimationComponent`、`SpriteRenderLayer`、`SpriteBatcher`
- **前置阅读**：[`SpriteSystemDesign.md`](../SpriteSystemDesign.md)、[`SpriteRenderer.md`](../api/SpriteRenderer.md)、[`SpriteBatcher.md`](../api/SpriteBatcher.md)

---

## 🧭 坐标空间与矩阵配置

1. **屏幕空间 UI**
   - `SpriteRenderComponent::screenSpace = true`
   - 使用正交投影矩阵（`SpriteRenderSystem` 默认提供 `Renderer::GetOrthoProjection()`）
   - `TransformComponent` 的位置以像素为单位，原点位于窗口左下角（可通过自定义偏移调整到左上角）
   - 适合 HUD、按钮、弹窗等固定在屏幕的元素

2. **世界空间 UI**
   - `screenSpace = false`
   - 共享场景相机的视图/投影矩阵，例如头顶血条、3D 看板
   - 通过 `SpriteRenderLayer` 与世界物件区分层级，避免被地形/模型遮挡

3. **即时模式 UI**
   - 使用 `SpriteRenderer` 或 `SpriteRenderable` 手动渲染
   - 适合启动菜单、加载进度条等独立界面
   - 别忘记显式调用 `SetViewProjectionOverride` 配置正交矩阵

> ⚠️ 注意：无论哪种路径，都应通过 `UniformManager` 统一设置矩阵与通用 uniform，保持渲染状态一致。

---

## 🧱 层级与排序

### SpriteRenderLayer 快速应用

```cpp
using namespace Render::ECS;

SpriteRenderComponent buttonSprite;
SpriteRenderLayer::ApplyLayer("ui.default", buttonSprite, /*localOrder=*/buttonIndex);
```

默认内置层有：

| 名称              | layerID | sortBias | 典型用途          |
| ----------------- | ------- | -------- | ----------------- |
| `world.background`| 700     | -100     | 世界远景 / 天空盒 |
| `world.midground` | 700     | 0        | 世界主体 / 角色   |
| `world.foreground`| 700     | +100     | 世界前景特效      |
| `ui.background`   | 800     | -200     | UI 背景板         |
| `ui.panel`        | 800     | -50      | 面板 / 窗口底板   |
| `ui.default`      | 800     | 0        | 一般 UI 控件      |
| `ui.foreground`   | 800     | +50      | 高亮、按钮前景    |
| `ui.overlay`      | 900     | 0        | 模态层 / HUD      |
| `ui.tooltip`      | 910     | +50      | 提示气泡 / 光标   |
| `debug.overlay`   | 999     | 0        | 调试覆盖          |

框架会在 `SpriteRenderSystem` 中为“默认配置”自动填充 `ui.default` / `world.midground`，但显式调用 `ApplyLayer` 可以精确控制排序。

### 层级最佳实践

- 按功能模块预留排序区间，例如主菜单 (0~99)、HUD (100~199)、弹窗 (200~299)
- 屏幕空间元素尽量保持独立层级，减少与世界空间对象的竞争
- 透明元素尽量聚合在同一层内，避免与深度写入对象相互覆盖

---

## 🧩 批处理与性能

- `SpriteRenderSystem` 默认使用 `SpriteBatcher` + GPU Instancing 合批，确保纹理、混合模式、视图矩阵一致即可享受批处理收益
- UI 资源通常共享同一 Atlas，可显著减少批次数
- 监控入口：
  - `SpriteRenderSystem::GetLastBatchCount()`：本帧 Sprite 批次数
  - `SpriteRenderSystem::GetLastSubmittedSpriteCount()`：当帧提交给批处理的实例总数（九宫格拆分时尤为有用）
  - 渲染日志落地的 `Batch flush` 行：查看 instanced draw 调用数量
  - `BatchManager::FlushResult`：总 DrawCall、实例数、fallback 情况
- 若某 UI 组件频繁更改纹理或混合模式，请考虑：
  - 复用共享材质或九宫格纹理
  - 在 `SpriteBatcher` 前自定义排序（例如按照 atlas 顺序添加）

> 🎯 建议和 UI 设计师共同规划 atlas，将同层控件的纹理合并在同一张贴图上，以最大化合批效果。

### 批处理验证场景参考

`examples/43_sprite_batch_validation_test.cpp` 覆盖多类场景，可作为 UI 迭代时的回归测试（括号内为预期批次数 / 实例数）：

- `SingleTextureScreenSpace`：同纹理屏幕 UI，应合并为 1 / 12
- `TwoTexturesSameLayer`：多纹理同层，预计 2 / 12
- `MixedScreenAndWorld`：屏幕/世界混合，预计 2 / 10
- `DifferentLayersSameTexture`：同纹理不同图层，预计 3 / 3
- `NineSliceSingleSprite`：单个九宫格面板拆出 3×3 子块，预计 1 / 9
- `MirroredPanelsSharedBatch`：两个翻转 + 子像素偏移的面板共享批次，预计 1 / 18

如有新增 UI 特性，可仿照上述结构扩展测试，确保批处理策略不过度分裂。

---

## 🛠️ ECS UI 构建流程示例
- `sprite.nineSlice.borderPixels` 指定九宫格切边，`SpriteRenderSystem` 会在 CPU 端拆分 3×3 片段并保持原批次键
- `sprite.flipFlags`（`SpriteUI::SpriteFlipFlags`）可在不增加 draw call 的情况下左右 / 上下镜像精灵
- `sprite.snapToPixel` 与 `sprite.subPixelOffset` 允许 UI 吸附像素网格并施加细微偏移，减少模糊感

```cpp
void CreateButton(World& world,
                  const Vector2& position,
                  const Vector2& size,
                  const std::string& textureName,
                  uint32_t localOrder) {
    EntityID entity = world.CreateEntity({"UI.Button"});

    TransformComponent transform;
    transform.SetPosition(Vector3(position.x(), position.y(), 0.0f));
    transform.SetScale(Vector3(size.x(), size.y(), 1.0f));
    world.AddComponent(entity, transform);

    SpriteRenderComponent sprite;
    sprite.textureName = textureName;
    sprite.screenSpace = true;
    sprite.resourcesLoaded = false; // 由异步加载填充
    SpriteRenderLayer::ApplyLayer("ui.default", sprite, localOrder);
    world.AddComponent(entity, sprite);

    SpriteAnimationComponent anim;
    anim.SetDefaultState("Idle");
    anim.Play("Idle");
    world.AddComponent(entity, anim);
}
```

- 资源加载：通过 `TextureLoader` 或 `SpriteAtlasImporter` 注册纹理/图集，再在组件激活时标记 `resourcesLoaded = true`
- 状态/事件：`SpriteAnimationScriptRegistry` 可在鼠标悬停、按下时触发颜色/音效回调
- 交互逻辑：结合输入系统或脚本，监听鼠标坐标 → 转换至 UI 坐标空间 → 触发对应实体的状态机

---

## 🔌 与动画系统配合

1. **状态驱动 UI**
   - `SpriteAnimationComponent` 可配置 Idle / Hover / Pressed 状态
   - 通过 `SetTrigger`、`SetBoolParameter` 响应输入事件
   - 使用 `frameScripts` 在特定帧触发音效或粒子效果

2. **脚本回调**
   - 在 `SpriteAnimationScriptRegistry` 注册 `UI.Button.OnPressed` 等脚本
   - `SpriteAnimationSystem` 在状态/过渡事件发生时调用，能直接修改其他 ECS 组件或调度游戏逻辑

3. **与世界空间交互**
   - 可在同一状态机内切换到 `screenSpace=false` 的状态，用于“弹出”至世界空间的特效（例如按钮按下后在场景中生成提示箭头）

---

## 🔍 调试与排错

- **显示异常 / 覆盖错误**
  - 检查 `layerID`、`sortOrder` 是否正确
  - 留意透明对象与深度写入组合，必要时禁用深度写入或调整 `BlendMode`
  - 在日志中留意 `SpriteRenderSystem` 输出的 `sourceRect` 与批次信息

- **批处理失效**
  - 观察 `Batch flush` 中 `fallbackDraw` 是否升高
  - 核对是否混用了不同纹理 / 混合模式 / 视图矩阵
  - 检查是否存在未加载完成的贴图（`resourcesLoaded == false`）

- **交互脚本无反应**
  - 确认脚本已注册并在状态机中绑定
  - 查看 `SpriteAnimationSystem` 日志，确保 `Event` 被正确派发
  - 调试时可临时启用 `SpriteAnimationComponent::eventListeners` 打印每个状态变化
- **状态调试辅助**
  - Debug 构建可使用 `SpriteAnimationDebugPanel` 定期打印状态机快照（详见 `SpriteAnimationDebugger` 文档）
  - 配合 `SpriteRenderLayer::ListLayers()` 检查层级命名与批次划分

---

## 📦 资源管理建议

- 将 UI 纹理与世界纹理统一托管在 `ResourceManager` 中；当 UI 不再使用时调用 `RemoveTexture` 释放
- 通过 `ResourceDependencyTracker` 可视化引用关系，确认 UI 资源不与场景主资源互相依赖
- 异步加载（`AsyncResourceLoader`）完成后记得同步更新 `SpriteRenderComponent::resourcesLoaded`

---

## ✅ 统一规范清单

- [ ] 所有 UI 精灵使用 `SpriteRenderLayer` 管理层级与排序
- [ ] `screenSpace` 与视图矩阵配置遵守 UI / 世界元素划分
- [ ] Atlas / 纹理规划支持批处理要求
- [ ] 通过 `UniformManager` 统一设置 UI 着色器所需 uniform（时间、窗口尺寸等）
- [ ] 动画脚本注册于 `SpriteAnimationScriptRegistry`，避免直接在系统中绑定 lambda
- [ ] 资源生命周期受 `ResourceManager` 监管，退出场景前清理引用
- [ ] 回归执行 `43_sprite_batch_validation_test`，确认批次数与预期一致

---

[上一页](../SpriteSystemDesign.md) | [下一页](../RENDERING_LAYERS.md)

- 使用 `TextRenderer` 绘制即时文本时，请在初始化后调用 `renderer->SetBatchingMode(BatchingMode::CpuMerge)` 以确保文本参与合批。
- `Text::SetAlignment(TextAlignment::Center/Right)` 可以在保持批处理的同时控制水平对齐，配合批次键不会额外增加 draw call。

