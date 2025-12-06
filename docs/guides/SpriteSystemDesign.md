# Sprite 系统开发文档

[返回文档首页](./README.md)

---

## 📌 背景

在 Phase 1 中我们已经完成了 `SpriteRenderable` 与 `SpriteRenderSystem`，能够在 ECS 下渲染 2D 精灵。本阶段目标是搭建一个面向项目实际需求的完整 Sprite 系统，提供更易用的数据对象、动画支持和批处理能力，同时保持与现有渲染框架兼容。

---

## 🎯 目标拆分

| 阶段 | 目标 | 关键产物 |
| ---- | ---- | -------- |
| A | 建立 Sprite API | `Sprite`、`SpriteSheet`、`SpriteAnimator`、`SpriteRenderer` |
| B | ECS 集成增强 | 扩展 `SpriteRenderComponent`、`SpriteAnimationSystem` |
| C | 批处理与 UI 功能 | `SpriteBatch`、图集优化、九宫格、排序控制 |
| D | 测试与示例 | 单元测试 + Demo（基础/动画/ECS） |

---

## 🧱 架构分层

### Core（现有，引擎层）
- `SpriteRenderable`：负责底层 Draw 调用。
- `SpriteRenderSystem`：遍历 ECS 组件，提交渲染。
- `TextureLoader` / `ResourceManager`：负责纹理资源。

### Sprite API（新增）
- `Sprite`
  - 数据对象，包含纹理引用、UV、颜色、尺寸、旋转/翻转标记等。
  - 提供 `SetFrame(const SpriteFrame&)` 等便捷方法。

- `SpriteSheet`
  - 描述单个纹理中的多帧布局。
  - 支持名称索引、帧标签、动画信息。

- `SpriteAnimator`
  - 控制动画播放（速度、循环、PingPong）。
  - 维护当前帧指针，触发帧事件。

- `SpriteRenderer`
  - 非 ECS 场景下的即时渲染器。
  - 维护 `std::vector<SpriteInstance>`，在 `Flush()` 时批量提交给 `SpriteRenderable`。

- `SpriteBatch`（Phase C）
  - 针对大量静态 UI 元素的批处理，将多个 Sprite 合并一次 Draw Call。

### ECS 扩展
- `SpriteRenderComponent`
  - 新增动画状态（当前动画、帧索引、播放速度）。
  - 增加屏幕空间标记（UI 层 vs 世界层）、排序键。

- `SpriteAnimationSystem`
  - 独立于渲染系统，负责更新动画时间与帧。
  - 支持事件回调（如播放完成、帧切换）。

- `SpriteRenderSystem`（现有基础上升级）
  - 根据屏幕空间标记选择正交矩阵。
  - 处理排序键，保证 UI 元素顺序。
  - 后续接入批处理。

### 资源与工具
- `SpriteAtlasImporter`
  - 读取 TexturePacker、Spine、Unity SpriteAtlas 等格式。
  - 输出 `SpriteSheet`/`SpriteAtlas` 数据。

- `SpriteAtlas`
  - 管理帧信息、九宫格参数。
  - 提供查找：`GetFrameByName`、`GetNineSlice` 等。

- `SpriteFont` / `SpriteText`
  - 文本渲染将沿用 Sprite 体系（后续阶段）。

---

## 🛠️ 阶段工作流

### Phase A：Sprite API 初版（当前进行）
1. **实现核心类**
   - `Sprite`
   - `SpriteSheet`
   - `SpriteAnimator`
   - `SpriteRenderer`（即时模式）

2. **基础功能**
   - 从纹理或图集构建 Sprite。
   - 支持静态与动画播放。
   - 即时渲染：`SpriteRenderer::Draw(const Sprite&, const Transform&)`。

3. **示例与测试**
   - 新增 `examples/39_sprite_api_test.cpp`（后续创建）。
   - 单元测试验证动画帧推进。

### Phase B：ECS 集成
- 扩展 `SpriteRenderComponent`，加入动画数据结构。
- 新增 `SpriteAnimationSystem`。
- `SpriteRenderSystem` 读取动画播放结果、处理 UI 层级。

### Phase C：优化
- `SpriteBatch` + `BatchManager` 扩展。
- 处理九宫格、镜像、子像素对齐。
- 引入 `SpriteRenderLayer` 管理 UI 深度。

### Phase D：测试 & Demo
- 单元测试：UV 精度、动画循环、批处理稳定性。
- Demo：基础、动画、ECS 集成、UI 渲染。

---

## 📦 CMake / 目录调整

- 新增源文件：
  - `include/render/sprite/sprite.h`
  - `include/render/sprite/sprite_sheet.h`
  - `include/render/sprite/sprite_animator.h`
  - `include/render/sprite/sprite_renderer.h`
  - 对应 `src/sprite/*.cpp`

- `CMakeLists.txt`
  - 将新文件加入 `RENDER_SOURCES` / `RENDER_HEADERS`。
  - 示例添加到 `examples/CMakeLists.txt`。

---

## ✅ 阶段进度

### Phase A：Sprite API 初版 ✅ 已完成
- 目录结构：`include/render/sprite/`、`src/sprite/`
- 核心类：`Sprite`、`SpriteSheet`、`SpriteAnimator`、`SpriteRenderer`
- 即时渲染：统一 Quad UV，修复倒置问题；在正交投影下复用 `SpriteRenderable`
- 示例程序：`38_sprite_render_test`（ECS 流程）、`39_sprite_api_test`（即时模式）
- 构建支持：`CMakeLists.txt` 已纳入新模块源码

### Phase B：ECS 集成增强 🚧 进行中
- 扩展 `SpriteRenderComponent`：动画状态、屏幕空间标志、排序键
- 新增 `SpriteAnimationSystem`：逐帧更新动画，提供事件回调
- 升级 `SpriteRenderSystem`：读取动画结果，按屏幕空间/世界空间选择视图投影
- ✅ 动画事件回调：支持 `ClipStarted` / `FrameChanged` / `ClipCompleted` 监听
- ✅ 动画状态机：支持状态/过渡/参数/Trigger，默认状态自动切换
- ✅ 事件脚本化：通过 `SpriteAnimationScriptRegistry` 触发脚本
- ✅ 默认层映射：自动将默认 UI 精灵映射到 `ui.default`，世界精灵映射到 `world.midground`
- ✅ 示例 `42_sprite_state_machine_test`：演示状态机参数驱动、触发器、脚本回调与颜色/位移反馈
- 资源工具：`SpriteAtlasImporter`、动画配置解析
- ✅ 文档：`docs/api/Sprite*.md` 系列（Sprite / SpriteSheet / SpriteAnimator / SpriteRenderer / SpriteAtlas / SpriteAtlasImporter / SpriteBatcher）
- ✅ 已完成：`screenSpace`/`sortOrder` 字段 & per-instance 视图投影覆盖
- ✅ 已完成：`SpriteAnimationSystem` 基础逻辑 & 示例验证
- ✅ 已完成：`SpriteRenderSystem` 屏幕/世界空间切换、排序 + 往返位移动画示例
- ✅ 已完成：`SpriteAtlasImporter` JSON 导入（帧/动画配置 + ResourceManager 注册）
- ✅ 状态机可视化调试工具（`SpriteAnimationDebugger` + `SpriteAnimationDebugPanel` 初版）

### Phase C：批处理与 UI 功能 🚧 进行中
- ✅ 引入 `SpriteBatch` / `BatchManager` 扩展以减少 Draw Call（含 `SpriteBatcher` 管线与 `43_sprite_batch_validation_test` 验证）
- ✅ 设计 `SpriteRenderLayer` 管理 UI 层级与排序
- ✅ 九宫格 / 翻转 / 子像素对齐等高级 UI 配置已落地（组件 + 系统 + 批处理兼容）
- ⏳ UI 专用示例与文档补充（进行中）

#### Phase C.1 高级 UI 能力设计（进行中）

> 目标：在保持批处理效率的前提下，扩展 UI 精灵的展示能力，涵盖九宫格（Nine-slice）、镜像翻转、子像素对齐等常用需求。

1. **组件与数据结构**
   - `SpriteRenderComponent` 已扩展：
     - `SpriteUI::NineSliceSettings nineSlice`
     - `SpriteUI::SpriteFlipFlags flipFlags`（水平 / 垂直）
     - `bool snapToPixel`、`Vector2 subPixelOffset`
   - 默认关闭九宫格 / 翻转；状态机切换沿用显示属性，避免突兀跳变。

2. **九宫格渲染流程**
   - 采用 CPU 预分割：`SpriteRenderSystem` 在提交前按 3×3 切片迭代调用 `SpriteBatcher::AddSprite()`，保持同一批次键。
   - 当前 fillMode=Stretch；预留 `NineSliceSettings::fillMode` 拓展位，后续可扩展平铺等模式。

3. **镜像翻转**
   - `SpriteUI::SpriteFlipFlags` 通过 `flipFlags` 设置，`SpriteRenderSystem` 在模型矩阵上应用镜像缩放并调整九宫格索引。
   - 同一纹理 + 层仍共享批次，翻转不会破坏批处理。

4. **子像素对齐**
   - `snapToPixel`：渲染前将屏幕空间坐标对齐到整数像素，减少模糊。
   - `subPixelOffset`：允许在对齐基础上施加细微偏移（例如 0.5 像素），适配不同 DPI。
   - 仅对 `screenSpace` 精灵默认启用；世界空间可配置是否强制对齐。

5. **批处理兼容性**
   - 批次键仍基于纹理 / 视图 / 投影 / 屏幕空间标记；翻转与子像素通过模型矩阵体现，无需扩展键。
   - 九宫格拆分只增加实例数量；`SpriteRenderSystem::GetLastSubmittedSpriteCount()` 用于监测批量提交规模。

6. **开发与验证计划**
   - **实现顺序**：九宫格 → 翻转 → 子像素对齐（已完成）
   - **测试**：`43_sprite_batch_validation_test` 新增 `NineSliceSingleSprite`、`MirroredPanelsSharedBatch` 场景（批次=1，实例=9 / 18），并通过 `GetLastSubmittedSpriteCount()` 校验提交实例数
   - **示例**：计划补充 UI 专用演示，整合按钮/面板/翻转与像素对齐案例
   - **文档同步**：待更新 `SpriteBatch.md`、`2D_UI_Guide.md`，补充 API 指南与实践细节

### Phase D：测试与 Demo ⏳ 规划中
- 单元测试：动画播放、UV 精度、批处理排序
- Demo：基础/动画/ECS/UI 综合示例
- 文档：`docs/guides/2D_UI_Guide.md` 等

---

## 🎯 下一步（Phase B）

1. **组件与数据结构**：扩展 `SpriteRenderComponent`，必要时新增 `SpriteAnimationComponent`
2. **系统实现**：完成 `SpriteAnimationSystem`，调整 `SpriteRenderSystem` 以支持动画与屏幕空间矩阵
3. **资源与配置**：实现 `SpriteAtlasImporter`，制定动画配置格式（JSON/自定义） ✅
4. **示例与验证**：新增 `40_sprite_animation_test`，补充帧推进单元测试
   - ✅ 示例 `40_sprite_animation_test` 展示屏幕空间 + 世界空间动画
   - ✅ 示例 `42_sprite_state_machine_test` 验证状态机、Trigger、脚本事件与视觉反馈
5. **文档同步**：撰写 `docs/api/Sprite.md`、`SpriteSheet.md`、`SpriteRenderer.md`，并更新 Phase1 todo

---

## 🧭 状态机可视化调试工具设计与现状

> 参考 API 文档：`docs/api/SpriteAnimation.md`、`docs/api/SpriteAnimationScriptRegistry.md`、`docs/api/SpriteBatch.md`

### ✅ 当前能力
- **运行时快照**：`SpriteAnimationDebugger`（Debug 构建可用）实时缓存每个实体的状态机、剪辑、帧序号、参数、Trigger、最近事件。
- **事件追踪**：在 `SpriteAnimationSystem` 内部记录状态机事件与脚本回调触发顺序，并保留序号与注解。
- **调试面板**：`SpriteAnimationDebugPanel` 以文本面板形式输出精简视图，示例 `42_sprite_state_machine_test` 中每 2 秒打印一次快照，便于快速核对状态与参数。
- **远程指令**：调试器支持 `SetBool/SetFloat/Trigger/ForceState/QueueEvent` 等命令，后续 UI 可借此实现交互式调试。

### 🎯 后续目标
- **交互调试 UI**：基于现有调试面板接口继续构建可编辑界面（ImGui 或自研 HUD），支持参数输入、状态切换按钮、事件注入。
- **批次信息联动**：在面板中显示 `SpriteRenderLayer` 名称、最近批次数，以及 `SpriteBatcher` 分组信息，帮助排查渲染排序与批处理。
- **可视化拓扑**：绘制状态节点与过渡图（DAG），突出当前状态、可触发条件、脚本绑定等信息。

### 🧱 组件架构
1. **Debug 面板（UI 层）**
   - 使用现有调试 UI 框架（ImGui / 自研调试 HUD），新增 `SpriteAnimInspector` 面板。
   - 面板结构：
     - 实体选择器（支持名称模糊搜索/ID 列表）
     - 状态机概览（状态节点 + 过渡边，使用 DAG 或列表展示）
     - 当前状态详情（状态名、累计时间、关联剪辑、播放模式 `SpritePlaybackMode`）
     - 过渡条件实时评估（枚举 `SpriteAnimationTransitionCondition::Type`，展示阈值与当前值）
     - 参数编辑器：`boolParameters`、`floatParameters`、`triggers`
     - 事件日志：显示最近触发的 `SpriteAnimationEvent` 与脚本名
     - 批处理信息：`layerID`、`sortOrder`、`SpriteRenderLayer` 名称、`SpriteRenderSystem::GetLastBatchCount()` 引用

2. **数据采集接口**
   - **状态数据**：直接读取 `SpriteAnimationComponent`，引用结构请参考 `include/render/ecs/components.h`。
   - **脚本事件**：在 `SpriteAnimationSystem::Update` 中，向调试工具发送事件（可使用 `Logger` 或注册 `eventListeners` 内的调试钩子）。
   - **批次信息**：暴露 `SpriteRenderSystem::GetEntityBatchInfo(EntityID)`（新增小型 API，返回 `SpriteBatchKey` 和批次索引），或在调试工具内部通过 `SpriteBatcher::DebugGetSpriteRecord(entity)` 查询。
   - **资源信息**：`ResourceManager` 提供纹理/剪辑名称映射，便于面板显示。

3. **交互操作**
   - `SetBoolParameter` / `SetFloatParameter` / `SetTrigger`：调用组件提供的接口（见 `SpriteAnimationComponent`）。
   - 状态切换：调用 `SpriteAnimationComponent::ForceState("stateName")`（需新增 API，内部重置 `currentState` 与 `stateTime`）。
   - 事件模拟：通过 `SpriteAnimationComponent::QueueEvent(event)` 注入自定义事件（便于脚本测试）。
   - 批次重建：提供按钮触发 `SpriteRenderSystem::DebugRebuildBatches()`，观察批次数变化。

4. **可视化实现**
   - **结构图**：利用 ImGui（或项目现有绘图工具）绘制状态节点（圆角矩形）与箭头，展示 `SpriteAnimationTransition` 列表。
     - 选中边显示条件详细信息（参数名、阈值、触发类型）。
   - **时间轴**：小型条形图展示 `stateTime` 相对 `clip.duration`。
   - **事件序列**：滚动列表记录 `ClipStarted`、`ClipCompleted`、`FrameEvent`、脚本调用。

5. **持久化与回放**
   - 支持导出/导入调试会话（JSON），记录参数修改与事件序列，便于复现问题。
   - 与 `SpriteBatchValidationTest` 打通，在测试失败时保存当帧的调试快照。

### 🔄 工作流程
1. 调试面板初始化时，调用 `SpriteRenderLayer::ListLayers()` 获取图层名称，为批次信息提供友好描述。
2. 每帧刷新：
   - 拉取实体列表 + 组件状态（可缓存本帧数据减少锁竞争）。
   - 监听 `SpriteAnimationSystem` 内部事件回调，追加到事件日志。
3. 用户在面板中修改参数：
   - 即时调用组件 API 更新，触发状态机重评估。
4. 用户强制切换状态 / 注入事件：
   - 调试工具调用新增的调试 API，系统在下一帧应用变化并记录日志。
5. 若需要观察批次情况，调用 `SpriteRenderSystem::GetLastBatchCount()` & 调试 API，显示当前实体是否与其他实体同批次（纹理、layerID、screenSpace 等字段对齐）。

### 🧩 额外 API/功能需求
- `SpriteAnimationComponent`：
  - `ForceState(const std::string& stateName, bool resetTime)`
  - `QueueEvent(const SpriteAnimationEvent& evt)`（仅调试用途）
  - `CollectStateMachine(DebugStateMachine&)`（导出结构便于图形化）
- `SpriteRenderSystem`：
  - `DebugGetBatchInfo(EntityID)` 返回 `struct { std::string layerName; uint32_t batchIndex; uint32_t batchSize; }`
  - `DebugRebuildBatches()` 強制重建批次，便于验证参数变动对批次的影响
- `SpriteAnimationScriptRegistry`：
  - `DebugListRegisteredScripts()` 用于面板展示可调用脚本列表

### ✅ 交付进度
1. **Step 1**：调试 API 已完成，实现 `SpriteAnimationDebugger`（Debug 构建可用）并封装至 `SpriteAnimationComponent`。
2. **Step 2**：第一版调试面板（文本/日志输出）已上线，可通过 `SpriteAnimationDebugPanel::RenderToLogger()` 输出快照。
3. **Step 3**：示例 `42_sprite_state_machine_test` 集成调试面板，用于回归状态机与脚本事件。
4. **Step 4**：补充 `docs/api/SpriteAnimationDebugger.md`、`SpriteSystemDesign.md`、`2D_UI_Guide.md` 等文档，记录当前能力与后续规划。

---

## 📄 文档规划

| 文档 | 状态 | 内容 |
| ---- | ---- | ---- |
| `docs/api/Sprite.md` | ✅ 已完成 | Sprite 数据模型与 API |
| `docs/api/SpriteSheet.md` | ✅ 已完成 | 图集结构、导入流程 |
| `docs/api/SpriteRenderer.md` | ✅ 已完成 | 即时渲染接口、示例 |
| `docs/api/SpriteAnimator.md` | ✅ 已完成 | 动画播放控制 |
| `docs/api/SpriteAtlas.md` | ✅ 已完成 | 图集帧与动画描述 |
| `docs/api/SpriteAtlasImporter.md` | ✅ 已完成 | JSON 导入流程 |
| `docs/api/SpriteBatcher.md` | ✅ 已完成 | 轻量批处理实现 |
| `docs/api/SpriteAnimation.md` | ✅ 已完成 | ECS 动画组件、事件与系统 |
| `docs/api/SpriteBatch.md` | ✅ 已完成 | 批处理框架与模式说明 |
| `docs/guides/2D_UI_Guide.md` | ✅ 初稿完成 | UI / 文本与 Sprite 整合 |

---

## 🔚 附录

- 所有新增类均遵循现有项目线程安全约束。
- 统一通过 `ResourceManager` 访问纹理与图集。
- 保持渲染管线的 `UniformManager` 使用规范。

[返回文档首页](./README.md)


