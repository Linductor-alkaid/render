[返回文档首页](README.md)

---

# 渲染层级系统开发手册

## 1. 当前状态评估

- 渲染器目前仅维护一个扁平的 `m_renderQueue`，排序逻辑为 `layerID → MaterialSortKey → RenderPriority`，缺少层级配置、启用状态以及渲染状态覆写功能，也无法在提交阶段按层过滤。
- 透明段的排序沿用相同的 `layerID` 数值并依赖深度提示，屏幕空间/UI 物体无法定义独立的排序策略或固定视图/投影。
- `Renderable` 与 ECS 组件通过裸的 `uint32_t layerID`/`int32_t sortOrder` 保存层信息，缺少集中校验、枚举值和默认元数据。
- `SpriteRenderLayer` 仅提供命名映射与排序偏移，没有与渲染器共享的层级定义，也无法驱动批处理策略或渲染状态参数。
- 现有文档与示例提到的 `RenderLayer` 类、层级启用/禁用、视口剪裁等能力尚未落地，需要重新规划。

## 2. 设计目标

- 统一管理层级元数据，支持命名、优先级、类型（世界/屏幕空间）与默认渲染状态。
- 层级可启用/禁用、设置视口/剪裁/深度/混合/Cull 等状态覆写，并在每帧渲染前自动套用。
- 提供稳定的层级 ID 注册与查询流程，避免魔法数字，同时与 `SpriteRenderLayer`、ECS 组件、示例保持一致。
- 在 Renderer 中引入分层 render pass，将同层对象聚合后再执行排序与批处理，保留现有材质排序优化。
- 支持多相机扩展（预留 LayerMask/Filter），并为透明对象提供可配置的排序策略。
- 提供调试与统计接口，协助追踪每个层的提交数量、批次数与耗时。

## 3. 架构蓝图

### 3.1 核心类型

| 类型 | 作用 | 关键字段 |
| --- | --- | --- |
| `RenderLayerId` | 层级 ID 包装（强类型） | `uint32_t value`、比较/哈希 |
| `RenderLayerDescriptor` | 静态定义 | `name`、`priority`、`type`、`defaultSortMode`、`defaultState`、`maskIndex` |
| `RenderLayerState` | 运行时开关/覆写 | `enabled`、`overrideState`、`viewport`、`scissor` |
| `RenderLayerRegistry` | 单例/Renderer 成员 | 注册、查询、遍历层定义与状态 |
| `LayeredRenderQueue` | 帧内数据结构 | `std::vector<LayerBucket>`，每个 bucket 持有渲染对象列表与层缓存 |
| `LayerBucket` | 渲染阶段容器 | `RenderLayerId id`、`std::vector<Renderable*>`、排序配置 |
| `LayerSortPolicy` | 排序策略枚举 | `OpaqueMaterialFirst`、`DepthBackToFront`、`StableSubmission` 等 |

### 3.2 帧流程

1. ECS / 即时模式提交 renderable 时，查询 `RenderLayerRegistry` 获取层信息、验证层启用状态，并写入 `layerID`、`priority`、`sortHint`。
2. Renderer 的 `SubmitRenderable` 将对象放入对应 `LayerBucket`，记录提交顺序，用于需要稳定排序的层。
3. `FlushRenderQueue` 遍历启用的层，按 `priority` 进行渲染 pass：
   - 应用层级覆写的渲染状态（清除/设置深度、混合、视口等）。
   - 执行层内排序策略，再交给 `BatchManager` 生成批次。
   - 收集 per-layer 统计并写入 `RenderStats::layers`。
4. 完成一层后恢复全局渲染状态（或使用 `RenderState` 记忆栈）。
5. 清理 `LayerBucket`，等待下一帧。

## 4. API 设计

### 4.1 RenderLayer 注册

```cpp
struct RenderLayerDescriptor {
    RenderLayerId id;
    std::string name;
    uint32_t priority;            // 越小越早渲染
    RenderLayerType type;         // World / Screen / Overlay
    LayerSortPolicy sortPolicy;
    RenderStateOverrides defaultState;
    bool enableByDefault = true;
    int32_t defaultSortBias = 0;
    uint8_t maskIndex = 0;        // 对应 CameraComponent::layerMask 的比特索引（0-31）
};

class RenderLayerRegistry {
public:
    void RegisterLayer(const RenderLayerDescriptor& desc);
    void RegisterLayers(std::span<const RenderLayerDescriptor> descs);
    bool HasLayer(RenderLayerId id) const;
    const RenderLayerDescriptor& GetDescriptor(RenderLayerId id) const;
    RenderLayerState& GetMutableState(RenderLayerId id);
    void SetEnabled(RenderLayerId id, bool enabled);
    std::vector<RenderLayerDescriptor> ListLayers() const;
    void ResetToDefaults();
};
```

- `RenderStateOverrides` 复用现有 `RenderState` 能力，允许选择性设置 `depthTest`, `depthWrite`, `blendMode`, `cullFace`, `stencil` 等。
- 注册流程在引擎初始化时集中完成，并提供 JSON/TOML 扩展点。
- `maskIndex` 用于与 `CameraComponent::layerMask` 对齐（0-31），`FlushRenderQueue()` 会按相机遮罩过滤对应层。

### 4.2 Renderer 扩展

- `Renderer` 初始化阶段创建 `RenderLayerRegistry`，注册默认层（背景、世界、透明、UI、调试等），支持从配置文件覆盖。
- `SubmitRenderable` 根据 `RenderLayerId` 推送至 `LayeredRenderQueue`，若层被禁用则跳过。
- `FlushRenderQueue` 改为遍历 `LayerBucket`：
  1. 在进入层时应用 `RenderLayerState` 指定的覆写，更新 `RenderState`。
  2. 对 `LayerBucket` 内对象执行 `SortOpaque` 或 `SortTransparent`，透明层可选择稳定提交顺序。
  3. 将结果写入 `BatchManager`，保留现有批处理逻辑。
  4. 统计 `drawCalls`、`batchCount`、`culledSubmit` 等 per-layer 数据。
- `RenderStats` 增加 `struct LayerStats { RenderLayerId id; uint32_t submitted; uint32_t visible; uint32_t drawCalls; uint32_t batches; bool enabled; }` 列表，便于调试 HUD。

### 4.3 ECS 集成

- `MeshRenderComponent`, `ModelComponent`, `SpriteRenderComponent` 替换为 `RenderLayerId layer` 字段，并提供校验辅助（构造默认值时查询 registry）。
- 在系统提交前，使用 `RenderLayerRegistry` 验证层是否存在；若不存在，记录警告并回退到默认层。
- 提供便捷 API：`Render::Layers::World::Geometry()`, `Render::Layers::UI::Default()` 等，以 constexpr 形式避免硬编码。

### 4.4 SpriteRenderLayer 升级

- 将 `SpriteRenderLayer` 默认表迁移到 `RenderLayerRegistry` 上层，`ApplyLayer(name, component, localOrder)` 在命名查找后调用 `registry.SetEnabled` 或查询 `sortBias`。
- `SpriteBatcher` 读取层排序策略：世界空间层继续按深度排序，UI 层固定按照提交顺序 + sortBias。
- 提供 `ListLayers()` 输出 `RenderLayerDescriptor` 视图，使 UI 调试界面展示完整信息。

### 4.5 透明排序策略

- 为层级提供 `LayerSortPolicy`：
  - `OpaqueDefault`: 现有 `materialKey → priority` 逻辑。
  - `TransparentDepth`: 先按层默认深度提示再按材质。
  - `ScreenSpaceStable`: 保持提交顺序，仅使用 `sortOrder` 进行细分。
- 每个层可指定默认策略，`SpriteRenderLayer` 可额外提供 `localOrder`。

### 4.6 扩展入口

- 预留摄像机层级过滤：`CameraComponent` 增加 `LayerMask`, `Renderer::FlushRenderQueue` 依据当前 camera 过滤 bucket。
- 支持运行时调试面板：暴露 `RenderLayerRegistry::ForEachLayer(std::function<void(const RenderLayerDescriptor&, const RenderLayerState&, const LayerStats&)>)`。

## 5. 开发阶段计划

| 阶段 | 目标 | 关键产物 |
| --- | --- | --- |
| A. 基础设施 | 引入 `RenderLayerId`、`RenderLayerRegistry`、默认层定义；`Renderer` 挂载 registry，保持旧排序逻辑。 | 注册接口、单元测试、默认层配置 |
| B. Renderer 改造 | 改写 `SubmitRenderable`/`FlushRenderQueue` 使用 `LayeredRenderQueue`，支持启用开关与排序策略；更新 `RenderStats`。 | 新队列结构、per-layer 统计、性能基准 |
| C. ECS & Sprite 联动 | 组件字段迁移、`SpriteRenderLayer` 对接 registry、示例/测试刷新。 | ECS 更新、`SpriteBatcher` 排序策略、UI 调试 |
| D. 扩展与工具 | 可选：Camera LayerMask、运行时调试界面、配置驱动（JSON/TOML）。 | Debug UI、配置文件 |

每个阶段结束需运行 `43_sprite_batch_validation_test`、`37_batching_benchmark`、`45_lighting_test` 等回归，确保 `materialSortKeyMissing == 0`。

## 6. 测试与验证

- **单元测试**：`RenderLayerRegistry` 注册与查找；层启用切换；排序策略函数。
- **集成测试**：扩充 `examples/50_geometry_catalog_test` 展示不同层；新增 `examples/51_layer_debug_view.cpp` 演示 UI/世界叠加。
- **性能回归**：记录改造前后 `drawCalls`、`batchCount`、`workerWaitTimeMs`，确保无倒退。
- **调试工具**：在调试 HUD 输出 per-layer 统计，验证启用/禁用与排序策略生效。

## 7. 文档与维护

- 更新 `docs/api/Renderer.md`、`SpriteRenderLayer.md`，补充新的层级接口。
- 在 `guides/2D_UI_Guide.md` 链接本手册章节，说明 UI 层级注册方式。
- 于 `CMakeLists.txt` 注册新增源码文件，保持与资源管理约定一致。
- 提供示例配置文件模板（可放置于 `configs/render_layers.json`），便于团队覆盖默认层。

## 8. 当前落地进展（2025-11-09）

- ✅ `RenderLayerRegistry` 与默认层描述注册完毕，提供线程安全的注册/查询与状态覆写接口。
- ✅ Renderer 已切换至按层收集的 `LayeredRenderQueue`，`SubmitRenderable` 会在层未注册时回退到 `world.midground` 并跳过禁用层。
- ✅ 每层排序策略初步生效：
  - `OpaqueMaterialFirst`：延续旧有材质优先排序，并在同层内分离透明对象。
  - `TransparentDepth`：依据深度提示降序排列，辅以材质键与渲染优先级兜底。
  - `ScreenSpaceStable`：以 `defaultSortBias + RenderPriority` 为主序，保持提交顺序稳定。
- ✅ ECS Mesh/Model/Sprite 组件默认层 ID 已与命名层同步，SpriteAnimationSystem / SpriteRenderSystem 自动层映射时会查询 `RenderLayerRegistry` 并在缺失时应用默认层。
- ✅ `CameraComponent::layerMask` 现由 `UniformSystem` 驱动 `Renderer::SetActiveLayerMask()`，`FlushRenderQueue()` 会基于 `maskIndex` 过滤层级，支持按相机分层渲染。
- ✅ 渲染统计扩展为记录排序前/后的材质切换次数，方便后续性能分析。
- ✅ 层级遮罩与渲染状态覆写已在批处理阶段重放：`Renderer::FlushRenderQueue()` 会在每个 `Renderable` 进入 `BatchManager` 前重新应用所属层的覆写，`SpriteBatcher` 也会在提交 UI 批次后恢复深度/混合等状态，避免跨层污染。
- ✅ `examples/51_layer_mask_demo` 展示世界层与 UI 层遮罩切换，支持按键 `1/2/3` 切换可见层级、`U` 切换 UI 可见性，并在日志中输出 `[LayerMaskDebug]` 状态，便于排查 LayerMask 行为。

---

[返回文档首页](README.md) | [上一篇: MATERIAL_SYSTEM.md](MATERIAL_SYSTEM.md) | [下一篇: RESOURCE_OWNERSHIP_GUIDE.md](RESOURCE_OWNERSHIP_GUIDE.md)

