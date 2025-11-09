[返回文档首页](README.md)

---

# HUD 开发方案

## 1. 项目现状与 HUD 场景

- 渲染主干：`Renderer` 已采用分层 `LayeredRenderQueue`，支持 `RenderLayerRegistry`、`LayerStats`、`CameraComponent::layerMask` 与线程安全的渲染提交。
- UI 能力：`SpriteRenderSystem`、`SpriteBatcher` 与 `SpriteRenderLayer` 提供屏幕空间渲染及批处理；`TextRenderer`/`TextRenderable` 用于即时文本，默认配合正交矩阵。
- 资源体系：所有纹理、字体、着色器需通过 `ResourceManager`、`TextureLoader`、`ShaderCache`、`AsyncResourceLoader` 管理；UI 纹理合图已在 `SpriteSystemDesign` 中规划。
- Uniform 框架：`UniformManager` 是唯一的 uniform 写入口，`UniformSystem` 负责上传相机矩阵、时间、LayerMask 等公共数据。
- 调试能力：`RenderStats` 暴露 per-layer 统计，`renderer_layer_mask_test`、`43_sprite_batch_validation_test` 覆盖关键回归场景。

HUD（Heads-Up Display）需在保持现有渲染与资源约束的前提下，提供常驻的屏幕空间信息面板、交互提示、性能统计及系统状态展示。

## 2. 设计目标

- **统一渲染层级**：HUD 元素独立于普通 UI，默认位于 `ui.overlay` 或专用 `hud.overlay` 层，可通过 LayerMask 切换。
- **单一 Uniform 通道**：所有 HUD shader uniform 通过 `UniformManager` 注册与更新，确保与全局约束一致。
- **资源一致性**：HUD 使用的字体、贴图、材质全部由核心资源管理器加载并引用计数。
- **高性能与批处理**：依托 `SpriteBatcher` 与 `TextRenderer`，尽量保持 `drawCalls` 与批次数稳定。
- **可扩展交互**：遵守 Blender 风格的操作约定（快捷键、鼠标行为），为后续交互式控件预留 Hook。
- **流水线友好**：新增模块不破坏既有系统；重要阶段均附带测试计划与调试入口。

## 3. HUD 渲染架构

### 3.1 层级与相机策略

- 在引擎初始化的层注册阶段新增 `hud.overlay`：
  - `RenderLayerType::ScreenSpace` / `LayerSortPolicy::ScreenSpaceStable`；
  - `defaultSortBias` 建议设为 `+200`，位于 `ui.overlay` 与 `debug.overlay` 之间；
  - 通过 `maskIndex` 保证可使用 `CameraComponent::layerMask` 快速启停。
- ECS 端提供 `Layers::HUD::Overlay()` constexpr 访问器，`SpriteRenderLayer::ApplyLayer("hud.overlay", ...)` 统一赋值。
- HUD 默认复用主摄像机正交矩阵；若需独立摄像机，可在 `CameraSystem` 内注册只负责 HUD 的屏幕空间相机，与世界相机共享 `BeginFrame/EndFrame`。

### 3.2 Uniform 与 Shader 规范

- 新增 `HudPerFrameUniforms` 结构（窗口尺寸、DPI、帧时间、全局时间、焦点状态、鼠标坐标、LayerMask），由 `UniformSystem` 在 `Renderer::BeginFrame()` 后统一写入。
- `UniformManager` 扩展：
  - 注册 `uHudViewport`, `uHudTime`, `uHudDeltaTime`, `uHudFocus`, `uHudCursor` 等字段；
  - 对于图表/曲线类 HUD，可提供 `SetVector4Array`/`SetFloatArray` 方案传入缓存数据。
- HUD 专用 shader 应包含统一的 `struct HudMaterial`，所有字段由 `UniformManager::SetXXX()` 设置，禁止直接调用 GL API。

### 3.3 资源与批处理

- 字体：复用 `Text`/`Font` 管线，在引导阶段通过 `ResourceManager::AddFont()` 注册常用字体（常规、等宽、数字）。
- 贴图：HUD 图标打包进 Atlas，沿用 `SpriteAtlasImporter` 流程，纹理注册交给 `TextureLoader`；九宫格面板与进度条使用现有 `sprite.nineSlice`。
- 批处理：HUD 系统启动时调用 `renderer->SetBatchingMode(BatchingMode::CpuMerge)`，保持文本与精灵共享批次；若 HUD 需求 GPU instancing，可单独在层内切换。
- 统计数据展示可直接读取 `Renderer::GetStats()` 与 `RenderStats::layers`，经由 `UniformManager` 推送到 shader 或通过 `TextRenderer` 打印。

### 3.4 数据流总览

1. 游戏逻辑系统计算 HUD 数据（生命值、能量、FPS 等），写入 `HudState`（线程安全缓存结构）。
2. 渲染线程帧开始时，`HudSystem` 读取 `HudState` 快照，构建/更新对应的 `SpriteRenderComponent` 与 `Text` 对象。
3. `SpriteRenderSystem`/`TextRenderer` 提交至 `hud.overlay` Bucket，由 `LayeredRenderQueue` 按层渲染。
4. `UniformManager` 在渲染 pass 前注入必要的 per-frame 与 per-widget uniform。

## 4. ECS 集成与逻辑层

- **组件规划**
  - `HudCanvasComponent`：绑定到唯一 HUD 实体，存储布局基准（参考分辨率、缩放策略、对齐方式），提供 `ApplyResponsiveScale(windowSize)` 接口。
  - `HudWidgetComponent`：每个控件的视觉配置（Sprite/Text/九宫格引用）、排序偏移、动画曲线、可见性枚举；通过 `HudWidgetId` 索引 `HudState`。
  - `HudBindingComponent`（可选）：描述数据来源（观察的系统、脚本回调），用于双向绑定；支持 `OnValueChanged(HudWidgetId, const HudValue&)` 回调。
- **HudState 数据模型**
  - 线程安全快照结构：
    ```cpp
    struct HudState {
        std::atomic<uint64_t> version {0};
        HudPerfMetrics perf;
        HudPlayerStats player;
        HudPanelSnapshot panels[MAX_HUD_PANELS];
        uint32_t panelCount;
    };
    ```
    - `HudPerfMetrics`：`float frameTimeMs; float gpuTimeMs; uint32_t drawCalls; uint32_t batches;`。
    - `HudPlayerStats`：生命/耐力/资源条、状态效果标志、当前目标提示文本。
    - `HudPanelSnapshot`：`HudPanelId id; HudWidgetSnapshot widgets[MAX_WIDGETS]; uint32_t widgetCount; bool visible;`
    - `HudWidgetSnapshot`：用于渲染线程只读的数据（类型、数值、颜色、进度、文本缓冲索引等）。
  - 写线程使用 `HudStateWriter`（封装 `Acquire()`/`Commit()`，内部版本号自增 + memcpy），渲染线程使用 `HudStateReader::ReadIfChanged()` 获取最新快照，避免锁争用。
  - 高频曲线数据使用固定容量环形缓冲 `HudMetricRingBuffer<float, 128>`，写入使用 `std::atomic<uint32_t>` 与 acquire/release 语义。
- **HudSystem 生命周期**
  - `HudSystem` 继承 `System`，在 `OnCreate` 期间缓存 `Renderer`、`SpriteRenderSystem`、`TextRenderer`，并创建 `HudCanvasComponent` 实体。
  - `PreUpdate()`：从 `Renderer` 获取最新窗口尺寸，刷新 `HudCanvasComponent` 缩放；重置上一帧临时缓存。
  - `Update()`（渲染线程调用）：读取 `HudState` 快照 → 差分更新 `HudWidgetInstance`（SpriteRenderable/TextRenderable） → 通过 `SpriteRenderLayer::ApplyLayer("hud.overlay", ...)` 设置层与排序 → 调用 `Renderer::SubmitRenderable()`。
  - `PostUpdate()`：回收过期实例，统计 uniform 写入次数，向调试日志输出 HUD 同步延迟。
- **系统协作**
  - `HudSystem` 与 `SpriteRenderSystem`、`TextRenderer` 共用批处理通路；必要时在调试模式下插入 `debug.overlay` 面板显示差分结果。
  - 现有 `UniformSystem` 在上传相机与时间 uniform 时，调用 `UploadHudUniforms(UniformManager*)` 写入 `uHudViewport`、`uHudTime` 等公共字段。
  - 输入系统维护 Blender 风格快捷键映射（`F2` 切换 HUD、`Ctrl+H` 隐藏 HUD、`Shift+Space` 聚焦性能面板），将事件写入 `HudState` 或触发 `HudSystem::HandleInputEvent()`。
- **数据同步**
  - 游戏逻辑线程通过 `HudStateWriter` 批量写入：例如 `UpdatePerfMetrics(Renderer::GetStats())`、`UpdatePlayerStats(GameState)`。
  - 渲染线程在 `BeginFrame` 后调用 `HudStateReader::Consume()` 复制只读快照；若版本未变化则跳过重新生成控件。
  - 整体流程避免锁，确保在高帧率下 HUD 更新不会阻塞渲染。

## 5. HUD 元素类型与实现建议

- **基础文本面板**：使用 `TextRenderer`，支持自动换行、颜色渐变；对齐通过 `TextAlignment` 控制。
- **图标与条形图**：九宫格面板 + 动态缩放 sprite，进度值通过 `UniformManager::SetFloat("uHudProgress", value)` 传入 shader 控制截断。
- **状态指示器**：结合 `SpriteAnimationComponent`，在 `HudSystem` 内按事件切换 Idle/Hover/Warning 状态。
- **性能仪表**：读取 `RenderStats`、`BatchManager` 数据，放置在 HUD 专用 Debug 子面板；可通过 `ScreenSpaceStable` 排序保证文本顺序。
- **上下文提示**：使用 `ui.tooltip` 层作为暂存，触发时提升到 `hud.overlay` 并调整 `localOrder`，遵循 Blender 的提示样式（顶部/侧边）。

## 6. 性能、监控与调试

- 监控指标：`RenderStats::frameTimeMs`、`layers[hud.overlay].drawCalls/batches/visible`、`SpriteRenderSystem::GetLastBatchCount()`。
- 为 HUD 引入专用调试窗口（可放在 debug overlay），展示 layer 启用状态、排序策略、Uniform 更新次数。
- 日志：通过 `Logger` 设定 `HUD` 分类，关键事件（组件构建、资源加载失败、Uniform 缺失）均记录。
- 性能基线：目标 HUD 帧合批 ≤ 2 次 draw call（文本 + 精灵），GPU 时间低于 0.1 ms（720p 基准）。如超过阈值，输出 profiler 提示。

## 7. 实施阶段划分

| 阶段 | 焦点 | 关键产物 |
| --- | --- | --- |
| P0 准备 | 注册 `hud.overlay`、完善 LayerMask 配置；搭建 `HudState`、`HudSystem` 框架 | 层定义、系统骨架、文档（本文件） |
| P1 核心 HUD | 实现基础文本/图标控件、Uniform 写入、资源加载流程；构建性能面板 | HUD shader、Canvas & Widget 组件、示例场景 |
| P2 交互扩展 | 接入 Blender 风格操作、状态动画、脚本事件绑定；完善调试 UI | 输入映射、脚本回调、回归测试 |
| P3 质量打磨 | 压测与优化、完善自动化测试、补充文档与教程 | 基准测试报告、CI 测试、开发指南更新 |

各阶段完成后执行：`renderer_layer_mask_test`、`43_sprite_batch_validation_test`、`45_lighting_test`、`examples/51_layer_mask_demo`（新增 HUD 演示）确保稳定。

## 8. 测试与验证

- **单元测试**：`HudState` 数据同步；`HudSystem` 对缺失资源/Uniform 的降级处理。
- **集成测试**：新增 `examples/52_hud_showcase.cpp`（屏幕缩放、LayerMask 开关、交互快捷键）；扩展 `tests/renderer_layer_mask_test.cpp` 验证 HUD 层可见性。
- **压力测试**：批量生成 200+ HUD 小部件，确认批处理与帧率；与 `BatchManager` 日志比对。
- **可视化对比**：录制对齐 Blender 操作的交互视频，确保手势与用户偏好一致。

## 9. 文档与维护

- 更新 `docs/api/Renderer.md`、`docs/api/UniformManager.md`、`docs/guides/2D_UI_Guide.md` 以纳入 HUD 层与 uniform 规范。
- 在 `docs/api` 内新增 `HudSystem.md`（实现后），记录 API 与扩展点。
- `CMakeLists.txt` 按新增源码/资源同步更新，遵守项目内本地依赖引用策略（GLFW/Eigen 本地路径）。
- 提供 HUD 专用资产命名规范与打包脚本，确保与 ResourceManager 兼容。

---

[上一页](guides/2D_UI_Guide.md) | [下一页](RENDERING_LAYERS.md)

