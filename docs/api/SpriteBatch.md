[返回 API 目录](README.md)

---

# SpriteBatch 框架概览

## 📋 概述

`SpriteBatch` 并不是单一类，而是一组围绕渲染器批处理管线的结构与工具，负责把多个 `Renderable` 汇聚成少量 DrawCall。它同样承担了 2D 精灵批处理与 3D 网格合批的统一调度，核心代码位于 `<render/render_batching.h>`。

- **命名空间**：`Render`
- **核心组件**：`BatchManager`、`RenderBatch`、`RenderBatchKey`、`BatchableItem`
- **批处理模式**：`BatchingMode::Disabled / CpuMerge / GpuInstancing`
- **线程模型**：前端录制、后台工作线程异步整理、主线程最终 Flush

---

## 🧱 关键数据结构

### `BatchingMode`

```cpp
enum class BatchingMode {
    Disabled,    // 逐对象提交
    CpuMerge,    // CPU 合并网格后一次提交
    GpuInstancing// GPU 实例化（Sprite 默认使用）
};
```

渲染器通过 `Renderer::SetBatchingMode()` 选择模式；Sprite 批处理当前主要依赖 `GpuInstancing`。

### `RenderBatchKey`

批次分组的“指纹”，包含材质、着色器、网格 / 纹理、混合模式、视图/投影哈希、屏幕空间标记、层级等字段。`BatchManager` 使用哈希表根据 key 聚类兼容的渲染项。

### `BatchableItem`

渲染器在遍历 `Renderable` 时生成的通用条目，标记该对象是否可批处理以及采用的具体数据：

```cpp
struct BatchableItem {
    Renderable* renderable = nullptr;
    BatchItemType type = BatchItemType::Unsupported; // Mesh / Sprite
    RenderBatchKey key{};
    MeshBatchData meshData{};
    SpriteBatchData spriteData{};
    bool batchable = false;
    bool isTransparent = false;
    bool instanceEligible = false;
};
```

- `SpriteBatchData` 与 `SpriteBatcher` 绑定，持有批次索引与纹理、混合信息。
- `MeshBatchData` 用于 CPU 合批或实例化网格。

### `RenderBatch`

对应一个物理批次，内部存储:

- 当前批次 key 与所有条目 (`BatchableItem`)
- 合批后的顶点 / 索引缓存（CPU Merge 时使用）
- GPU 实例化数据 (`InstancePayload`)
- 上传状态与统计信息（三角形 / 实例数量）

`RenderBatch::Draw()` 根据模式执行：

- `CpuMerge`：确保合并网格已上传到 `Mesh`
- `GpuInstancing`：绑定实例缓冲，通过 `DrawInstanced()` 发起绘制

### `BatchManager`

批处理调度器，负责：

- 接收 `BatchableItem`
- 在后台线程整理批次
- 提供 `Flush()` 接口返回 `FlushResult`（绘制统计）

内部采用“双缓冲”存储结构与 `BatchCommandBuffer`，保证录制与执行互不阻塞。`Flush()` 时会：

1. 等待工作线程完成当前队列
2. 交换录制 / 执行缓冲
3. 遍历批次，执行 Draw 或直接调用 `Renderable::Render`
4. 汇总统计数据（DrawCall、实例数、Fallback 数等）

---

## 🔄 精灵批处理与 `SpriteBatcher`

当渲染器检测到 `RenderableType::Sprite`（例如 `SpriteBatchRenderable`）时，会在 `BatchableItem::spriteData` 中记录：

- 对应的 `SpriteBatcher` 指针
- 批次索引与实例数量
- 混合模式、屏幕空间标记、纹理引用

随后 `BatchManager` 在 `GpuInstancing` 模式下调用 `RenderBatch::Draw()`，最终转而执行：

```cpp
spriteBatchData.batcher->DrawBatch(spriteBatchData.batchIndex, renderState);
```

该函数会：

1. 绑定共享的 Quad Mesh 与 Sprite Shader
2. 上传实例缓冲（模型矩阵 + UVRect + Tint）
3. 调整视图/投影矩阵以及正交/透视配置
4. 依据实例数量一次性绘制所有精灵

因此，在 ECS 流程中：

1. `SpriteRenderSystem` 调用 `SpriteBatcher::AddSprite()` 收集所有精灵
2. `BuildBatches()` 生成纹理 + 视图/投影 + Layer 分组
3. 对每个批次创建 `SpriteBatchRenderable` 并提交给渲染器
4. 渲染器转化为 `BatchableItem`，交给 `BatchManager`
5. Flush 时调用 `SpriteBatcher::DrawBatch()` 完成绘制

---

## 🧮 批次划分规则

`SpriteBatcher::BuildBatches()` 会根据以下关键字段拆分批次：

| 字段 | 说明 | 常见拆分原因 |
| ---- | ---- | ------------ |
| `textureHandle` | GPU 纹理对象 | UI 使用不同图集或独立贴图 |
| `layerID` / `sortOrder` | 层级与排序偏移 | UI 层切换、模态弹窗置顶 |
| `screenSpace` | 是否屏幕空间 | 屏幕 UI 与世界物件分离 |
| `blendMode` | 混合模式 | 叠加 / 正常 / AlphaClip 等混合差异 |
| `viewProjectionHash` | 视图/投影矩阵哈希 | 绑定不同相机或手动覆盖矩阵 |

> 💡 建议通过 `SpriteRenderLayer::ApplyLayer()` 管理层级，而非直接硬编码 `layerID`，以便跨场景保持一致性。

每个批次都会记录实例范围与纹理、混合状态等信息，在 `DrawBatch()` 时一次性绑定并绘制。

---

## 🧱 实例化数据布局

Sprite 实例缓冲结构（位于 `SpriteBatcher`）如下：

```cpp
struct SpriteInstancePayload {
    Matrix4 model;      // 模型矩阵
    Vector4 uvRect;     // UV 起点与尺寸
    Vector4 tintColor;  // RGBA 颜色
    Vector2 pivot;      // 旋转 / 缩放支点
    Vector2 padding;    // 对齐占位
};
```

- Shader Uniform 由 `UniformManager` 提供，包括投影矩阵、时间、屏幕尺寸等。
- 如需扩展属性（描边、剪裁矩阵），请同步更新 payload、`sprite.vert` layout 以及 `SpriteBatcher::BuildBatches()`。

---

## 🧪 批处理验证（示例 43）

`examples/43_sprite_batch_validation_test.cpp` 用于回归验证批处理策略。运行：

```powershell
cmake --build build --config Release --target 43_sprite_batch_validation_test
.\build\bin\Release\43_sprite_batch_validation_test.exe
```

当前涵盖场景：

| 场景 | 预期批次 | 预期实例（提交数） | 说明 |
| ---- | -------- | ------------------ | ---- |
| `SingleTextureScreenSpace` | 1 | 12 | 同 atlas 屏幕 UI，全部合并 |
| `TwoTexturesSameLayer` | 2 | 12 | 纹理不同导致拆分 |
| `MixedScreenAndWorld` | 2 | 10 | 屏幕/世界矩阵不同 |
| `DifferentLayersSameTexture` | 3 | 3 | 同纹理但层级不同 |
| `NineSliceSingleSprite` | 1 | 9 | 单个九宫格面板被拆出 9 个实例但仍保留单批次 |
| `MirroredPanelsSharedBatch` | 1 | 18 | 两个翻转的九宫格面板共享批次，验证翻转与子像素偏移 |

执行失败时会输出 `[ERROR]` 并退出，可在 CI 或本地迭代中快速检测回归。若新增批处理规则，请扩展此示例并更新预期值。

---

## 📊 FlushResult 指标

调用 `BatchManager::Flush()` 会返回 `FlushResult` 结构，常用字段：

| 字段 | 说明 |
| --- | --- |
| `drawCalls` | 总 DrawCall 数 |
| `batchCount` | 生成的批次数 |
| `instancedDrawCalls` / `instancedInstances` | 使用 GPU 实例化的统计 |
| `fallbackDrawCalls` | 无法批处理的即时绘制次数 |
| `workerProcessed` / `workerMaxQueueDepth` | 后台线程处理量与最高队列深度 |

在 Sprite 场景中，可以结合 `SpriteRenderSystem::GetLastBatchCount()`、`SpriteRenderSystem::GetLastSubmittedSpriteCount()` 或日志输出观察批处理效率与拆分实例数量。

---

## 🧪 使用示例（渲染器侧片段）

```cpp
BatchableItem item = renderer.CreateBatchableItem(renderable);

if (item.batchable) {
    batchManager.AddItem(item);
} else {
    // 立即绘制 fallback（保持渲染正确性）
    item.renderable->Render(renderState);
}

// 所有对象提交完毕后
auto stats = batchManager.Flush(renderState);
Logger::InfoFormat("Batches=%u Instanced=%u DrawCalls=%u",
                   stats.batchCount,
                   stats.instancedDrawCalls,
                   stats.drawCalls);
```

---

## ⚠️ 注意事项

- **层级与排序**：`RenderBatchKey::layerID`、`sortOrder` 决定批次顺序，与 `SpriteRenderLayer` 配合可保持 UI / 世界元素稳定排序。
- **混合模式**：不同 `BlendMode` 无法共批；透明物体仍需正确排序。
- **视图/投影哈希**：屏幕空间和世界空间使用不同矩阵，批次会拆分。请确保渲染时提供一致的矩阵哈希（`SpriteRenderSystem` 已内建处理）。
- **资源生命周期**：`BatchManager` 会缓存 `Mesh` / `Texture` 句柄；切换关卡或场景时应调用 `Reset()`。
- **后台线程**：虽然存在异步整理，但最终 Draw 仍在渲染线程完成。录制阶段请避免持有短生命周期对象的裸指针。

---

## 📚 延伸阅读

- [SpriteBatcher](SpriteBatcher.md) — 精灵实例化批处理器实现细节
- [SpriteRenderLayer](SpriteRenderLayer.md) — 层级与排序工具
- [RenderBatching](RenderBatching.md) — 批处理框架总体设计与调试指南

---

[上一页](SpriteAnimationDebugger.md) | [下一页](SpriteBatcher.md)

