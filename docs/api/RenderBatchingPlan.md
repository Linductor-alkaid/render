# RenderBatching 方案

[返回 API 首页](README.md)

---

## 概述

本方案旨在为 RenderEngine 引入可扩展的渲染批处理（Render Batching）能力，降低每帧的 Draw Call 数量、减少 OpenGL 状态切换，并兼容现有的 `Renderer` 渲染队列与 ECS 渲染系统。目标是在不破坏已实现渲染管线的前提下，逐步引入按材质/着色器/渲染状态聚合的批处理流程，并为未来的实例化渲染、命令缓冲和多线程渲染奠定基础。

## 设计原则

- **非侵入性集成**：沿用 `Renderer::SubmitRenderable()` → `FlushRenderQueue()` 的工作流，通过新增批处理管理器接管排序后的渲染对象执行逻辑，而不修改调用侧接口。
- **状态一致性**：所有材质相关的 uniform 必须继续通过 `UniformManager` 设置；批处理逻辑负责在批次内复用 uniform 绑定并在切换批次时刷新。
- **资源集中管理**：批处理使用的临时 VBO/IBO、实例缓冲由核心 `ResourceManager` 创建与回收，遵循项目中“资产统一由资源管理器管理”的约定。
- **渐进式落地**：先实现 CPU 侧合批与 Draw Call 合并，再引入 GPU 侧实例化、命令缓冲与多线程准备，确保每个阶段都可独立验证与回滚。
- **可观测性**：在 `RenderStats` 扩展批处理相关统计（批次数、合并前后 Draw Call 对比、实例化对象数），便于评估收益。

## 关键术语与范围

| 名称 | 说明 |
| --- | --- |
| RenderBatchKey | 用于区分批次的键，包含材质 ID、着色器 ID、混合模式、深度/模板状态、阴影标记等。 |
| RenderBatch | 同一 `RenderBatchKey` 下的渲染数据容器，负责收集顶点/索引缓冲或实例数据，并提交 Draw Call。 |
| BatchManager | 本方案新增的批处理调度器，负责在帧内维护所有 `RenderBatch`，并在 `Flush` 时统一提交。 |
| BatchCommandBuffer | （阶段二引入）记录 GPU 指令的轻量命令序列，兼容未来的多线程命令生成。 |
| InstancePayload | （阶段二引入）实例化渲染的 per-instance 数据结构，如模型矩阵、材质覆盖参数。 |

本方案优先覆盖 `MeshRenderable` 和 `SpriteRenderable`，其余 Renderable 类型在后续阶段扩展。ECS 侧的 `MeshRenderSystem`、`SpriteRenderSystem` 继续负责创建/更新 Renderable，仅需在渲染阶段配合批处理查询。

## 与现有渲染队列的衔接

当前 `Renderer` 在 `FlushRenderQueue()` 中会对 `Renderable` 进行排序后逐个调用 `Render()` 完成绘制：

```195:236:src/core/renderer.cpp
    for (auto* renderable : m_renderQueue) {
        if (renderable && renderable->IsVisible()) {
            renderable->Render(m_renderState.get());
        }
    }
```

计划中的改动：

1. `SortRenderQueue()` 维持现有排序（层级 → 渲染优先级 → 类型），为批处理聚合提供输入顺序保障。
2. 在 `FlushRenderQueue()` 中新增 `BatchManager`：
   - 遍历排序后的 Renderable，将 `MeshRenderable`、`SpriteRenderable` 转换为 `BatchableItem`（包含网格、材质、变换、覆盖参数、阴影标记等）。
   - 调用 `BatchManager::AddItem(BatchableItem)`，根据 `RenderBatchKey` 聚合。
3. 在遍历结束后调用 `BatchManager::Flush(m_renderState.get())`，统一提交 Draw Call。其余无法批处理的 Renderable（如自定义类型）直接 fallback 到原始 `Render()`。
4. `BatchManager::Flush` 内部负责：
   - 确保 `UniformManager` 在批次切换时重新绑定材质/着色器 uniform。
   - 写入/更新批次使用的 VBO/IBO 或实例缓冲。
   - 调用 `glDrawElements` 或 `glDrawElementsInstanced`。
5. `RenderStats` 在 Flush 结束后记录统计信息。

## 批处理数据结构设计

### 1. BatchKey（不可变）

```text
struct RenderBatchKey {
    uint32_t materialID;
    uint32_t shaderID;
    BlendMode blendMode;
    DepthState depthState;
    StencilState stencilState;
    bool castShadow;
    bool receiveShadow;
    RenderLayer layer;
    uint64_t renderStateHash; // 包含开启/关闭的渲染状态
};
```

- 来源：`MeshRenderable`/`SpriteRenderable` 的材质、渲染状态。
- 哈希函数需稳定、可在帧内重复使用，以支持 `std::unordered_map`。

### 2. RenderBatch（可变）

| 字段 | 用途 |
| --- | --- |
| `RenderBatchKey key` | 当前批次的状态键 |
| `std::vector<Vertex>` | CPU 端顶点缓存（阶段一） |
| `std::vector<uint32_t>` | CPU 端索引缓存（阶段一） |
| `std::vector<InstancePayload>` | 实例化数据（阶段二） |
| `GpuBufferHandle vertexBuffer/indexBuffer` | GPU 缓冲句柄，由 `ResourceManager` 提供 |
| `uint32_t drawCallCount` | 该批次产生的 Draw Call 数（阶段一通常为 1） |
| `uint32_t instanceCount` | 实例数（阶段二） |
- 提供 `AddMesh(Renderable*)`、`AddSprite(Renderable*)`、`UploadBuffers()`、`Draw(RenderState*)` 等方法。

### 3. BatchManager（帧级别对象）

- 生命周期：`Renderer::BeginFrame()` 时 `Reset()`，`FlushRenderQueue()` 中使用，`EndFrame()` 自动释放。
- 主要接口：
  - `void AddItem(const BatchableItem& item);`
  - `void Flush(RenderState* renderState);`
  - `void Reset();`
- 内部持有 `std::unordered_map<RenderBatchKey, RenderBatch>`，并维护用于减少内存分配的对象池。
- 支持 `BatchingMode`（枚举）：`Disabled` / `CPU_Merge` / `GPU_Instancing`，以便在调试时快速切换。

## ECS 渲染系统配合

- `MeshRenderSystem` 与 `SpriteRenderSystem` 保持现有逻辑，继续在 `Update()` 末尾调用 `renderer->SubmitRenderable()`。
- 若未来需要在 ECS 层预聚合，可新增可选 `BatchComponent`（记录批处理提示，如实例化 ID、LOD 级别）。该组件由系统写入，批管理器读取。
- `TransformSystem` 已具备批量更新能力（`BatchUpdateTransforms()`），批处理方案需确保在渲染阶段读取的世界矩阵已更新完毕。
- 异步资源加载（`AsyncResourceLoader`）在资源就绪时需触发批次刷新，避免提交失效批次。

## UniformManager 协调

- 每个 `RenderBatch` 在 Draw 前调用 `Material->Apply(renderState)`，内部继续通过 `UniformManager` 设置材质 Uniform。
- 引入 `UniformBlockCache`（可选）：记录同批次内重复使用的 uniform 值，减少 CPU→GPU 绑定。实现思路：
  1. `RenderBatch` 在添加新 Renderable 时，收集所有会触发 uniform 写入的参数（材质覆盖、骨骼矩阵等）。
  2. 对于实例化渲染，将 per-instance uniform 写入 `InstancePayload`，在 GLSL 端以 UBO / TBO 读取。
- 确保所有新增 uniform 字段仍通过 `UniformManager` 注册、查询、设置，满足用户偏好（[[memory:7889023]]）。

## GPU 资源与内存管理

- 批次使用的 VBO/IBO/实例缓冲统一由 `ResourceManager` 创建，命名遵循项目资产管理规范（[[memory:7392268]]）。
- 采用 **缓冲回收池**：
  - 每种批次类型维护固定大小的 GPU 缓冲池，可根据 `maxVertexCount`、`maxInstanceCount` 预分配。
  - `BatchManager::Reset()` 时不销毁缓冲，仅标记为可复用，减少频繁创建销毁。 
- 当批次数据超过当前缓冲容量时，触发缓冲扩容（`glBufferStorage` 或 `glBufferData`）。同时记录统计信息用于调优。

## 阶段划分与交付里程碑

| 阶段 | 内容 | 交付物 | 验证方式 |
| --- | --- | --- | --- |
| Phase 0 | 引入 `BatchManager` 骨架，保持原有逐对象渲染 | `BatchManager`、`BatchableItem` 结构，`Renderer` 中挂钩但尚未合批 | 单元测试：渲染结果一致；性能指标不倒退 |
| Phase 1 | CPU 合批，合并相同 `RenderBatchKey` 的网格数据 | `RenderBatch::AddMesh()`、`AddSprite()`、`UploadBuffers()`、`Draw()` | Profiling：Draw Call 数量下降；功能测试：透明物体/阴影正确 |
| Phase 2 | 实例化渲染与命令缓冲 | `InstancePayload`、`BatchCommandBuffer`、`glDrawElementsInstanced` | 场景大规模复制模型性能对比；ECS 压力测试 |
| Phase 3 | 多线程批次准备 + GPU 指令提交优化 | 线程安全 `CommandBuffer`、后台准备线程 | 多线程压力测试；Frame Time 统计 |
| Phase 4 | 可调试性与回滚支持 | Batch 可视化调试、统计面板 | 调试界面截图、统计数据日志 |

每个阶段完成后需要更新 `RenderStats`、文档与示例，确保 API 使用者了解变更。

> **阶段进展（2025-11-07）**：已实现线程安全、双缓冲的 `BatchCommandBuffer` 与后台批次准备线程，GPU 实例化路径输出 `instancedDrawCalls`、`instancedInstances`，并新增后台任务统计（`workerProcessed`、`workerMaxQueueDepth`、`workerWaitTimeMs`）同步到 `RenderStats` 与调试日志；新增示例程序 `examples/37_batching_benchmark.cpp` 用于对比各批处理模式的性能表现。
>
> **阶段进展（2025-11-08）**：完善了实例化着色器管线与 `UniformManager` 协作，批处理日志支持节流输出，`37_batching_benchmark` 在三种模式下保持稳定阵列结果；新增文档 **RenderBatching** 记录落地实现、使用步骤与调试建议。

## 依赖与风险管控

- **依赖**：
  - OpenGL 上下文与缓冲工具封装（`RenderState`、`OpenGLContext`）
  - Uniform/材质系统（`Material`、`UniformManager`）
  - ECS 系统（`TransformSystem`、`MeshRenderSystem`、`SpriteRenderSystem`）
- **潜在风险**：
  - 材质状态不一致导致渲染伪像：需在 `RenderBatchKey` 中覆盖所有会影响渲染结果的状态。
  - 透明物体排序问题：透明渲染需在批次内按深度排序或延迟渲染；方案为第一阶段保留透明物体逐对象渲染，后续再做专门处理。
  - 队列与批次的线程安全：所有批处理操作限定在渲染线程；未来多线程扩展要引入锁或双缓冲。

## 开发与测试建议

- 单元测试：
  - `RenderBatchKey` 哈希/比较一致性。
  - `BatchManager` 添加/重置流程。
- 集成测试：
  - 在现有示例（`examples/32_ecs_renderer_test.cpp`）中对比启用/关闭批处理的渲染统计。
  - 使用 `MeshLoader` 加载大模型，验证批处理后的帧率提升与资源占用。
- 调试工具：
  - 提供运行时开关（`Renderer::SetBatchingMode`）。
  - 在日志中输出每帧批次数、合并前后 Draw Call 对比。
- 文档：
  - 完成每阶段后更新 `docs/api` 与 `docs/todolists`，同步记录最佳实践。

---

[上一篇: RenderState](RenderState.md) | [下一篇: GLThreadChecker](GLThreadChecker.md)


