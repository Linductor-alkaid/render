# 材质排序架构设计

[返回文档首页](README.md)

---

## 背景
- `docs/todolists/PHASE1_BASIC_RENDERING.md` 指出了现有渲染队列缺少材质排序优化，导致状态切换成本高。
- `docs/api/Renderer.md`、`docs/api/RenderBatchingPlan.md` 描述了当前渲染管线：ECS 系统提交 `Renderable`，`Renderer::SortRenderQueue()` 仅按 `layerID → renderPriority → type` 排序，透明物体在 `MeshRenderSystem` 内自排序，Sprite 依赖 `SpriteBatcher` 的纹理排序。
- 资源、Uniform 与批处理的统一约束：所有资产受 `ResourceManager` 管理，材质相关 Uniform 必须通过 `UniformManager` 设置 [[memory:7392268]] [[memory:7889023]]。
- 当前 `BatchManager` 已支持 CPU/GPU 合批，但缺乏稳定的材质键，导致批次切分粒度细，阻碍 Phase 1 的性能目标。

## 目标
- 在不破坏现有 ECS 与批处理流程的前提下，引入稳定的材质排序与批次键。
- 减少材质、着色器、渲染状态切换，提升 CPU 执行效率与 GPU pipeline 利用率。
- 确保透明渲染的深度排序正确性，并兼容特殊材质覆盖（MaterialOverride）。
- 提供可观察性与可回滚能力，便于逐步上线和性能评估。

## 设计原则
- **非侵入式**：ECS 系统继续调用 `renderer->SubmitRenderable()`；材质排序逻辑集中在渲染阶段。
- **Uniform 一致性**：所有新增批次 Uniform 通过 `UniformManager` 注册与写入，避免直接 GL 调用。
- **资源集中**：排序键的元数据依赖 `ResourceManager` 提供的 ID/句柄，保持资产生命周期一致。
- **透明保守性**：透明对象保持距离排序优先，材质排序仅对不透明管线生效，透明对象在排序后再按材质聚合。
- **可配置性**：允许通过 `Renderer::SetBatchingMode` 和新引入的调试开关快速启停材质排序。

## 架构概览
- 在 `Renderer` 层新增 `MaterialSortPolicy`，负责生成排序键并驱动 `SortRenderQueue()` 的比较函数。
- 扩展 `BatchManager`，以 `MaterialSortKey` 作为 `RenderBatchKey` 的前缀，确保批次划分先按材质合并，再按实例化策略细分。
- `MeshRenderSystem` 与 `SpriteRenderSystem` 在构建 `Renderable` 时写入基础的材质 ID、覆盖信息与透明标记，为排序阶段提供输入。
- 透明渲染采用“双队列”策略：先根据距离排序，然后在保持深度顺序的前提下按材质分组批量提交。

## 关键组件
- **MaterialSortKey**：包含 `materialID`、`shaderID`、`blendMode`、`depthStateHash`、`stencilStateHash`、`pipelineFlags`。
- **MaterialSortPolicy**：生成排序键、提供比较与哈希函数；默认策略对不透明对象启用材质优先排序，对透明对象保持深度→材质的稳定排序。
- **RenderableMetadata**：扩展 `Renderable` 基类，缓存 `MaterialSortKey` 与透明标记；在材质或覆盖变更时自动刷新。
- **MaterialStateCache**：与 `UniformManager` 协作，缓存上一次绑定的材质 Uniform，减少批次内部重复写入。

## 流程整合
- `MeshRenderSystem`：在提交 `MeshRenderable` 前，调用 `UpdateMaterialSortInfo()`，根据材质与 `MaterialOverride` 生成 `MaterialSortKey`，并写入透明标记。
- `SpriteRenderSystem`：对世界空间 Sprite 引入 `SpriteMaterialTrait`，填充覆盖后的混合模式与排序键；屏幕空间 Sprite 维持纹理优先但附带材质键，保证 UI 元素材质可控。
- `Renderer::SortRenderQueue()`：扩展比较逻辑为 `layerID → materialSortKey → renderPriority → type`，其中透明对象先按透明队列处理再走材质键。
- `BatchManager::AddItem()`：读取 `RenderableMetadata`，使用 `MaterialSortKey` 创建或复用批次，同时保留现有实例化路径。

## UniformManager 协作
- `MaterialSortKey` 不直接存储 Uniform 值，只存储材质与渲染状态标识。
- `MaterialStateCache` 与 `UniformManager` 协作：缓存最近一次绑定的材质 ID 与覆盖哈希；当排序导致材质批量提交时，仅在键变化时触发 `Material::Bind()` 与覆盖 Uniform 写入。
- 对于 `MaterialOverride`，在生成排序键时纳入覆盖哈希，确保不同覆盖的 Renderable 不被错误合并。

## 数据结构与接口调整
- `render/renderable.h`：增加 `const MaterialSortKey& GetMaterialSortKey()` 与 `void RefreshMaterialSortKey(const MaterialSortContext&)`。
- `render/render_batching.h`：将 `RenderBatchKey` 的材质相关字段替换为 `MaterialSortKey`，并提供 `CombineWithRenderState(RenderStateSnapshot)` 接口。
- `Renderer` 新增 `SetMaterialSortPolicy(MaterialSortPolicy policy)`，默认使用 `MaterialSortPolicy::Default()`；调试模式允许禁用材质排序以便对比性能。
- `Material` 新增 `uint32_t GetStableID()`，由 `ResourceManager` 分配并缓存。

## 透明渲染策略
- 在 `MeshRenderSystem` 中维持 `opaqueIndices` 与 `transparentIndices`。
- 透明队列排序流程：距离（降序）→ 材质键 → Renderable 原序号，保证同材质的透明对象仍按正确深度绘制。
- 在 Renderer 阶段提供独立的透明批次处理路径，避免错误合批导致的深度伪像。

## 可观测性与调试
- `RenderStats` 新增字段：`materialSortEnabled`、`materialSwitchesBefore`、`materialSwitchesAfter`、`overrideBatches`。
- 日志在 `SortRenderQueue()` 后输出首帧统计，并在调试级别定期报告材质批次数。
- `examples/41_sprite_batch_test.cpp`、`examples/32_ecs_renderer_test.cpp` 增加 CLI 或快捷键开关以观察排序带来的差异。

## 实施阶段
- 阶段一：引入 `MaterialSortKey` 与 `Material::GetStableID()`，保持原有渲染逻辑，记录材质切换统计。
- 阶段二：更新 `Renderer::SortRenderQueue()` 与 `BatchManager`，实现不透明对象材质优先排序，透明对象保持原流程。
- 阶段三：引入透明队列的材质稳定排序与 `MaterialStateCache`，裁剪冗余 Uniform 绑定。
- 阶段四：为 Sprite 管线应用同一策略，统一 UI 与 3D 渲染的材质排序行为。
- 每个阶段完成后需更新 `docs/api` 对应手册、示例程序，并验证性能回归测试。

## 测试与验证
- **功能测试**：运行 `examples/12_material_test.cpp`、`examples/41_sprite_batch_test.cpp`，确认渲染结果与日志。
- **性能测试**：在 `examples/37_batching_benchmark.cpp` 中对比排序启用前后的 Draw Call、材质切换数。
- **回滚策略**：保留 `Renderer` 级别的排序开关；在 CI 中新增回归测试确保禁用排序时行为与旧版本一致。

## 风险与缓解
- 材质覆盖合并错误：通过覆盖哈希参与排序键并在调试模式下输出覆盖差异。
- 透明批次破坏深度：透明管线仅在距离排序后按材质分组，但保持稳定排序，必要时允许对透明排序禁用材质聚合。
- ResourceManager ID 不稳定：ID 分配使用有序递增并在材质释放时回收，确保跨帧稳定。
- UniformManager 写入遗漏：引入单元测试验证在材质切换时 `UniformManager` 的调用次数与批次一致。

## 后续工作
- 在 `docs/api/Renderer.md` 与 `docs/api/RenderBatching.md` 中补充材质排序相关接口说明。
- 更新 `docs/todolists/PHASE1_BASIC_RENDERING.md` 的材质排序条目状态，并规划 Phase 2 对透明渲染的进一步优化。
- 结合光照系统计划，评估材质排序对动态光源 uniform 上传的影响，必要时扩展 `MaterialStateCache` 支持光照参数缓存。

---

[上一篇: MATERIAL_SYSTEM](MATERIAL_SYSTEM.md) | [下一篇: RENDERABLE_ANALYSIS_REPORT](RENDERABLE_ANALYSIS_REPORT.md)

