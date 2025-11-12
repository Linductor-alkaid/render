[返回文档首页](../README.md)

---

# 基础复用 UI 框架总体方案

## 1. 现状评估与问题

- **渲染能力**：当前 UI 依赖 `SpriteRenderSystem`、`SpriteBatcher`、`TextRenderer`，具备屏幕/世界空间渲染与批处理能力，但缺少统一的 UI 状态管理与布局层。
- **应用层接口**：`AppContext`、`SceneManager`、`ModuleRegistry` 已提供场景与模块框架；`EventBus` 支持优先级监听，但尚未与 UI 输入语义绑定。
- **资源与 Uniform**：`ResourceManager` 与 `UniformManager` 工作正常，HUD 方案已明确需要通过统一 Uniform 管理 UI/HUD 参数，目前尚无专用 UI uniform 套件。
- **Phase 2 需求**：TODO 中要求“UIRenderer 扩展、输入适配 Blender 规范、Widget 组件库、UI 自动化测试”，目前这些能力均未落地。

> 结论：基础渲染通路成熟，但缺乏一套覆盖数据模型、布局、皮肤、交互到工具链的 UI Runtime，需要构建可扩展框架保障后续 HUD、工具面板与应用层 UI 迭代。

## 2. 设计目标

1. **模块化 UI Runtime**：提供可扩展的 `UICanvas`、`UIWidget`、`UILayout`、`UITheme` 等核心组件，支撑复杂工具 UI。
2. **渲染桥接统一**：所有 UI shader uniform 必须经 `UniformManager`，共享 Sprite/Text 批处理，支持 LayerMask 与排序策略。
3. **Blender 风格交互**：输入事件经 `EventBus`→`UIInputRouter`，遵循 Blender 控制约定，支持快捷键上下文与操作回放。
4. **资源与配置中心化**：UI 纹理/字体/主题定义走 `ResourceManager`，新增资源自动纳入 CMake。
5. **可测试性**：内置 UI 状态快照、自动化脚本回放接口，支持截图与日志对比。
6. **渐进式落地**：分阶段交付（P0~P3），避免阻塞现有渲染与 HUD 开发。

## 2.1 最新进展（2025-11-10）

| 模块 | 状态 | 说明 |
| --- | --- | --- |
| `UIRuntimeModule` | ✅ 已接入 | 完成模块注册、帧生命周期钩子；示例场景已加载 UI。 |
| `UICanvas` | ✅ 已落地 | 维护窗口尺寸、缩放、焦点信息，并驱动 `UIRendererBridge`。 |
| `UIRendererBridge` | ✅ 已落地 | 建立 Sprite/Text 命令生成链路，默认加载 `assets/fonts/NotoSansSC-Regular.ttf`。 |
| `UIWidget`/`UIWidgetTree` | ✅ 初版 | 支持基本属性、树结构与脏标记。 |
| `UILayoutEngine` | ✅ 初版 | 提供纵向栈式布局、根节点自动居中与基本 spacing。 |
| `UIInputRouter` | ✅ 初版 | 解析鼠标/键盘事件，管理焦点/悬停，并与 SDL 文本输入联动。 |
| 控件库 | ✅ 基础 | 实现 `UIButton`、`UITextField`，示例中展示点击、输入交互。 |
| 调试能力 | ✅ 初版 | `UIDebugConfig` 支持输入日志与布局框绘制。 |

当前示例（`examples/60_ui_framework_showcase.cpp`）展示按钮与文本输入，可进行实时渲染与交互验证。输入框焦点状态联动 SDL 文本输入 API，按钮点击日志输出正常。

## 2.2 已知问题与待办

- 布局系统仅支持绝对/纵向堆叠，尚未实现 Flex/Grid/Dock。
- 渲染命令仍与 Widget 耦合，缺乏 `UIRenderNode` 抽象与特效（九宫格、阴影等）。
- 缺乏主题/样式配置，控件样式写死在代码中。
- 尚未建立 UI 自动化测试与控件 API 文档。
- 文本输入交互仍存在问题（缺少可靠的视觉反馈与文本同步），需进一步排查并修复。

## 3. 框架总览

```
┌──────────────────────────────────────────────────────────────┐
│                      Application Layer                        │
│  SceneManager ── ModuleRegistry ── EventBus ── InputSystem    │
│               │                 │                 │          │
└───────────────┴─────────────────┴─────────────────┴──────────┘
                │                 │                 │
┌───────────────▼─────────────────▼─────────────────▼──────────┐
│                         UI Runtime                           │
│  UICanvas  ── UIWidgetTree ── UILayoutEngine ── UIStyling     │
│      │               │                 │             │       │
│  UIRendererBridge    │                 │             │       │
└───────────────┬──────┴────────────┬────┴────────────┴────────┘
                │                   │
┌───────────────▼────────────┐ ┌────▼──────────────────────────┐
│ Sprite/Text Render Systems │ │ UniformManager / ResourceMgr   │
└────────────────────────────┘ └────────────────────────────────┘
```

核心组成：

- **UIRuntime 核心**：`UICanvas` 管理根节点与缩放策略，`UIWidgetTree` 维护 Widget 层级，`UILayoutEngine` 执行布局计算，`UIStyling` 负责主题/皮肤。
- **UIRendererBridge**：与 `SpriteRenderSystem`、`TextRenderer`、`UniformManager` 协作，负责批量生成渲染实例与 Uniform 更新。
- **UIInputRouter**：连接 `EventBus` 与 Blender 风格输入映射，提供命令级事件给 Widget。
- **UIAutomation**：录制/回放输入脚本，采集 UI 状态与截图，驱动自动化测试。

## 4. 核心子系统设计

### 4.1 UICanvas 与渲染桥接

- 新增 `Render::UI::UICanvas`，负责：
  - 保存参考分辨率、缩放策略（Fixed, ScaleToFit, MatchHeight 等）；
  - 维护 `UICanvasState`（窗口尺寸、DPI、焦点、光标位置）；
  - 暴露 `BeginFrame()`/`EndFrame()` 钩子，调用 `UIRendererBridge` 刷新渲染实例。
- `UIRendererBridge` 提供：
  - `CollectDrawCommands(const UICanvas&)`：遍历 Widget 树，生成 `SpriteRenderable`/`TextRenderable`；
  - `UploadPerFrameUniforms(UniformManager&)`：写入 `uUiViewport`、`uUiScale`、`uUiTime`、`uUiFocus` 等字段；
  - `ApplyLayer(SpriteRenderComponent&, layerId)`：统一使用 `SpriteRenderLayer::ApplyLayer("ui.default", ...)`，与 HUD 方案兼容。
- Hook 点：
  - 在 `ModuleRegistry::PreFrame` 注册 `UIRuntimeModule`，`PreFrame` 内刷新 Canvas 状态 → `UICanvas::BeginFrame`；
  - 在 `ModuleRegistry::PostFrame` 调用 `UIRendererBridge::Flush()`，并维持与 `Renderer::SetBatchingMode(BatchingMode::CpuMerge)` 的一致性。

### 4.2 Widget 树与声明式属性

- `UIWidget` 基类：
  - 属性：`id`、`layoutNode`、`styleHandle`、`visibility`、`interactionFlags`；
  - 生命周期：`OnAttach(UICanvas&)`、`OnDetach()`、`OnTick(FrameUpdateArgs)`、`OnInput(const UIInputEvent&)`；
  - 数据接口：`SetState(UIStateBlock)`，支持脏标记与 diff。
- `UIWidgetTree`：
  - 提供 `AddChild`, `RemoveChild`, `FindById`；
  - 引入 `UIWidgetSnapshot` 以支持回放与测试；
  - 支持局部无锁读：渲染线程获取快照，逻辑线程异步更新状态（参考 `HudState` 差分策略）。
- 属性系统：
  - 采用轻量 `UIPropertySet`（键值对 + 观察者）实现样式覆盖；
  - 支持 `Bind(PropertyKey, ValueProvider)`，结合 Scene/EventBus 实现数据绑定。

### 4.3 布局引擎

- 引入 `UILayoutNode` 描述布局属性（方向、对齐、伸缩、间距、padding）。
- 支持三类布局器：
  1. **FlexLayout**：水平/垂直流布局，支持 Blender 面板常用的上下栈、左右分栏。
  2. **GridLayout**：二维格子，兼容属性面板矩阵、快捷键布局。
  3. **DockLayout**：用于窗口/面板停靠，提供 `UIDockTree` 管理拖拽、拆分、浮动。
- 布局计算：
  - `UILayoutEngine::MeasurePass(UICanvas&, WidgetTree)` 计算最小/期望尺寸；
  - `UILayoutEngine::ArrangePass(...)` 输出每个 Widget 的屏幕矩形；
  - 结果写入 `WidgetRenderContext`，供渲染阶段使用。
- 参考 Blender 行为：
  - 面板标题区固定高度；
  - 交互区域可拖拽调整比例，记录于 `UISerializationState`。

### 4.4 样式与主题

- `UITheme` 数据：
  - 颜色表（命名空间 + 色阶）；
  - 字体族与字号；
  - 控件尺寸（padding、边框、圆角）；
  - 渐变、阴影、图标 atlas 映射。
- 资源加载：
  - 主题定义使用 JSON（或 TOML）文件，注册到 `ResourceManager`；
  - 主题切换通过 `UIThemeManager::LoadTheme(themeId)`，内部触发 `UIWidgetTree::ApplyStyleOverrides`。
- Skin 实现：
  - 支持多皮肤配色（Dark/Light/HighContrast）；
  - 利用 `SpriteAtlas` 管理控件图标，`UIRendererBridge` 负责根据主题配置选取材质/九宫格。

### 4.5 输入与 Blender 控制

- `UIInputRouter`：
  - 订阅 `EventBus` 的 `InputEvent`（键、鼠、手柄、触摸）；
  - 解析为 Blender 风格语义（Select、Confirm、Cancel、Grab、Scale、Axis Snap 等）；
  - 维护 `InputContextStack`（场景、UI 模式、文本编辑等）。
- 快捷键系统：
  - `UIShortcutBinding` 描述组合键、鼠标、修饰键；
  - 冲突检测：注册时检查上下文内唯一性，冲突写入日志；
  - 提供 `ShortcutPalette` UI 展示当前上下文映射。
- 事件分发：
  - 先路由至 `UIFocusable` 控件（如文本输入框）；
  - 未消费事件转发到场景层（与 Blender 行为一致）。
- 操作记录：
  - `UICommandRecorder` 记录用户操作（执行命令、参数、时间戳），支持回放与调试。

### 4.6 资源与 Uniform 流程

- 所有 UI shader uniform 由 `UIUniformBlock` 管理：
  - `PerFrame`：`uUiViewport`, `uUiDpiScale`, `uUiTime`, `uUiDeltaTime`, `uUiFocus`, `uUiCursor`.
  - `PerWidget`：`uUiWidgetRect`, `uUiCornerRadius`, `uUiThemeColorIndex`.
- Uniform 写入策略：
  - `UIRendererBridge::UploadPerFrame` 在 `PreFrame` 统一写入；
  - `UIRenderableBatch::BindWidgetUniforms` 按 Widget diff 设置，确保通过 `UniformManager`。
- 资源资产：
  - UI atlas（按钮、滑块、勾选框、分隔条等）统一打包至 `assets/atlases/ui_core.atlas.json`；
  - 字体集与图标字体通过 `ResourceManager` 注册；
  - 新增/修改资源需更新根 `CMakeLists.txt` 与对应子目录 CMake。

### 4.7 Widget 组件库

分层次实现：

1. **基础控件 (P1)**：`UIButton`, `UIToggle`, `UISlider`, `UICheckBox`, `UIColorPicker`, `UIIconButton`, `UILineEdit`.
2. **复合控件 (P2)**：`UIPropertyPanel`, `UITreeView`, `UITable`, `UIListView`, `UIBreadcrumb`.
3. **窗口管理 (P2/P3)**：`UIDockSpace`, `UIFloatingWindow`, `UIPopup`, `UITooltip`.
4. **Blender 专属 (P3)**：操作历史面板、快捷键显示、Transform 面板。

每个控件：

- 定义 `UIWidgetDescriptor`（属性 + 主题 key）；
- 提供 `UIWidgetRenderer` 描述如何生成 Sprite/Text；
- 在 `docs/api` 添加独立文档（落地后），便于工具与脚本引用。

### 4.8 调试与工具链

- `UIDebugOverlay`：显示布局框、对齐线、Widget id、输入命中测试结果。
- `UIProfilerPanel`：统计绘制批次数、Uniform 写入次数、布局耗时。
- `UIAutomationConsole`：回放脚本、输出断言状态、截图路径；与 Phase2 的 UI 自动化 TODO 对齐。
- 日志分类：新增 `UI` Channel，记录资源加载失败、快捷键冲突、Uniform 缺失等。

## 5. 与现有系统协作

- **SceneManager**：在场景 `OnAttach` 中注册 `UIRuntimeModule`，场景切换时保持 Canvas 状态可选持久化 (`SceneSnapshot.state["ui.canvas"]`)。
- **ModuleRegistry**：`UIRuntimeModule` 依赖 `SpriteRenderSystem`、`TextRendererModule`，优先级设为 `PreFrame=50`（早于 HUD），`PostFrame=60`。
- **HUD**：HUD 继续使用专用系统；当 HUD 面板依赖 UI 控件时，可创建 `HudUiBridge`，共享 Widget 库但挂载到 `hud.overlay` 层。
- **ResourceManager**：UI 主题/atlas/字体 manifest 集中在 `configs/ui_manifest.json`，Scene 加载时调用 `ResourceManager::LoadManifest(...)`。
- **UniformManager**：扩展 `UniformSystem`，新增 `UploadUiUniforms(Renderer&, UICanvasState&)`，确保与 HUD uniform 不冲突。

## 6. 测试与验证策略

- **单元测试**：
  - `UICanvasStateTest`：验证缩放计算、鼠标坐标转换、焦点切换。
  - `UILayoutEngineTest`：覆盖 Flex/Grid/Dock 布局边缘情况。
  - `UIShortcutTest`：快捷键冲突检测、上下文切换。
- **集成测试**：
  - 新增 `examples/60_ui_framework_showcase.cpp`：展示基础控件与 Blender 风格交互。
  - 扩展 `tests/renderer_layer_mask_test.cpp`：确认 UI 层与 HUD 层共存时遮罩正确。
  - UI 自动化：脚本回放 + 截图对比，记录在 `tests/ui_automation/`.
- **性能基线**：
  - 目标：常规面板（50+ 控件）批处理 ≤ 2 DrawCall（Sprite + Text）。
  - 压力测试：批量创建 500 控件，帧率保持 ≥ 120 FPS（1080p）。
- **回归流程**：
  - 将 UI 自动化与 HUD 回归加入 CI（后续 Phase 3）。
  - 每次主题/布局变更运行 `43_sprite_batch_validation_test` + 新增 UI 测试。

## 7. 实施阶段规划

| 阶段 | 内容 | 关键交付 |
| --- | --- | --- |
| **P0 基础设施** | 创建 `UICanvas`、`UIRuntimeModule`、`UIRendererBridge`、基本 uniform/资源管线 | 核心类头/源文件、CMake 更新、Canvas 单元测试、示例最小 UI |
| **P1 布局与基础控件** | 完成 Widget 树、FlexLayout、基础控件、主题系统初版 | UIButton/UISlider 等控件、默认主题、演示示例 |
| **P2 高级组件与输入** | Blender 输入映射、DockLayout、复合控件、自动化测试框架 | 快捷键映射表、Dock 面板、UIAutomation 原型 |
| **P3 打磨与集成** | 性能调优、文档补齐、CI 集成、Blender 专属面板 | UI 调试工具、性能报告、API 文档、CI 脚本 |

阶段间保持可运行状态，确保 HUD 与场景开发可渐进复用 Widget 库。

## 8. 文档与后续行动

- 新增/更新文档：
  - 本方案落地后，定期更新 `docs/api`（UI 相关 API）、`docs/guides/2D_UI_Guide.md`（加入 UIRuntime 用法）、`docs/HUD_DEVELOPMENT_PLAN.md`（引用 Widget 库）。
  - 为每个控件撰写 API 参考，遵循返回/前后导航规范。
- 待创建的源码目录与 CMake 项：
  - `include/render/ui/` 与 `src/ui/` 新增核心类；
  - 更新根 `CMakeLists.txt` 与对应子目录；
  - 若引入新 shader/atlas，更新 `shaders/` 与 `assets/atlases/`，并写明 UniformManager 接入。
- 近期行动建议（P0）：
  1. 在 `ModuleRegistry` 中注册 `UIRuntimeModule` 骨架，连通 `AppContext`。
  2. 建立 `UICanvas` 与 `UIRendererBridge`，实现最小按钮渲染。
  3. 定义 `UIUniformBlock`，扩展 `UniformSystem` 的 per-frame uniform 上传。
  4. 准备 `ui_core.atlas` 与默认主题 JSON，纳入 `ResourceManager`。
  5. 编写 `examples/60_ui_framework_showcase.cpp` 骨架，验证渲染链路。

## 9. 下一阶段计划（短期目标）

1. **布局系统升级**（UI-Layout Sprint）
   - 引入 Flex 布局属性（方向、对齐、伸缩因子），支持百分比与自适应高度。
   - 联动 `UIDebugConfig` 绘制布局调试 overlay（展示 padding、占位与对齐线）。
2. **渲染命令抽象**
   - 设计 `UIRenderNode`/`UIRenderStyle`，解耦 Widget 逻辑与绘制命令。
   - 扩展 `UIRendererBridge` 支持九宫格、阴影、渐变等视觉效果。
3. **资源与主题体系**
   - 建立默认主题配置（颜色、字体、间距），实现 `UIThemeManager`。
   - 规范 `ResourceManager` 对 UI 资源的加载流程（atlas/字体 manifest），补充 CMake。
4. **示例与测试**
   - 新增“控件操作面板”示例，覆盖按钮、文本框、列表等交互。
   - 编写 `UILayoutEngine`/`UIInputRouter` 单元测试，准备可视回归测试的脚本接口。

---

[上一页](2D_UI_Guide.md) | [下一页](../HUD_DEVELOPMENT_PLAN.md)

