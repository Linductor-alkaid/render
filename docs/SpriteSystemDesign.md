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
- ✅ 默认层映射：自动将默认 UI 精灵映射到 `ui.default`，世界精灵映射到 `world.midground`
- 资源工具：`SpriteAtlasImporter`、动画配置解析
- ✅ 文档：`docs/api/Sprite*.md` 系列（Sprite / SpriteSheet / SpriteAnimator / SpriteRenderer / SpriteAtlas / SpriteAtlasImporter / SpriteBatcher）
- ✅ 已完成：`screenSpace`/`sortOrder` 字段 & per-instance 视图投影覆盖
- ✅ 已完成：`SpriteAnimationSystem` 基础逻辑 & 示例验证
- ✅ 已完成：`SpriteRenderSystem` 屏幕/世界空间切换、排序 + 往返位移动画示例
- ✅ 已完成：`SpriteAtlasImporter` JSON 导入（帧/动画配置 + ResourceManager 注册）
- ⏳ 待完成：动画状态机与过渡配置

### Phase C：批处理与 UI 功能 ⏳ 规划中
- 引入 `SpriteBatch` / `BatchManager` 扩展以减少 Draw Call
- 支持九宫格、镜像翻转、子像素对齐等 UI 需求
- ✅ 设计 `SpriteRenderLayer` 管理 UI 层级与排序

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
5. **文档同步**：撰写 `docs/api/Sprite.md`、`SpriteSheet.md`、`SpriteRenderer.md`，并更新 Phase1 todo

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
| `docs/api/SpriteBatch.md` | 规划中 | 批处理策略（Phase C） |
| `docs/guides/2D_UI_Guide.md` | 规划中 | UI / 文本与 Sprite 整合 |

---

## 🔚 附录

- 所有新增类均遵循现有项目线程安全约束。
- 统一通过 `ResourceManager` 访问纹理与图集。
- 保持渲染管线的 `UniformManager` 使用规范。

[返回文档首页](./README.md)


