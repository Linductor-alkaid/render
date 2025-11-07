# 渲染批处理（Render Batching）

[返回 API 首页](README.md)

---

## 概述

渲染批处理负责将同类 `Renderable` 聚合为更少的 GPU 提交，降低 Draw Call 数量并减少 OpenGL 状态切换。当前实现覆盖以下能力：

- **批处理模式**：`Disabled`、`CpuMerge`、`GpuInstancing`
- **批次调度器**：`BatchManager` 负责批次组装、命令缓冲和后台线程处理
- **统计与调试**：`RenderStats` 与日志输出记录批次数量、实例化数据、后台线程指标
- **示例程序**：`examples/37_batching_benchmark.cpp` 用于对比三种模式的性能

实现完全兼容现有渲染流程，ECS 系统仍通过 `Renderer::SubmitRenderable()` 提交对象。

---

## 核心类型

| 类型 | 说明 |
| --- | --- |
| `BatchingMode` | 批处理模式枚举，决定执行路径（禁用/CPU 合批/GPU 实例化） |
| `BatchableItem` | 从 `Renderable` 提取的批处理条目，包含网格、材质、模型矩阵等数据 |
| `RenderBatchKey` | 批次键，聚合材质、着色器、渲染状态、网格句柄等信息 |
| `RenderBatch` | 承载批次数据并提交 Draw Call，支持 CPU 合批与 GPU 实例化 |
| `BatchCommandBuffer` | 记录批次指令的轻量命令缓冲，配合双缓冲与后台线程 |
| `BatchManager` | 批处理调度器，维护批次存储、命令缓冲和工作线程 |

`BatchManager` 同时维护 `BatchStorage`（批次对象/查找表）与工作队列，通过双缓冲确保主线程与后台线程安全交接。

---

## 使用指南

1. **启用模式**：在运行时调用 `Renderer::SetBatchingMode(BatchingMode mode)`。默认禁用，可在每帧切换。
2. **提交对象**：保持原有 `Renderable::SubmitToRenderer(renderer)` 流程，批处理自动生效。
3. **材质与 Uniform**：所有 uniform 仍通过 `UniformManager` 设置；GPU 实例化时 `uModel` 固定为单位矩阵，实例化数据通过顶点属性 4~7 传入。
4. **统计查看**：通过 `Renderer::GetStats()` 读取 `RenderStats`，新增字段包括 `batchCount`、`batchedDrawCalls`、`instancedDrawCalls`、`instancedInstances`、`workerProcessed` 等。
5. **调试日志**：调试级日志在检测到回退或达到固定帧间隔时输出批处理统计（可用于观察后台线程负载）。

---

## 执行流程

### CPU 合批（CpuMerge）

1. 根据 `RenderBatchKey` 聚合 `BatchableItem`
2. 复制并合并顶点/索引数据，生成批次专用网格
3. 通过 `ResourceManager` 管理临时 GPU 资源
4. 调用 `glDrawElements` 提交合并后的网格

### GPU 实例化（GpuInstancing）

1. 同批次对象共享源网格，上传实例矩阵到实例化缓冲 (VBO)
2. 顶点着色器通过 `uHasInstanceData` 判断是否读取实例矩阵
3. 调用 `glDrawElementsInstanced` 提交批次

若某条目不满足批处理条件（透明、材质覆盖、自定义类型等），会自动回退到原始渲染路径，统计值记录为 `fallbackDrawCalls`/`fallbackBatches`。

---

## 多线程与命令缓冲

- **后台线程**：`BatchManager` 内部工作线程从 `WorkItem` 队列取任务，组装批次数据与命令
- **双缓冲**：`BatchCommandBuffer` 与 `BatchStorage` 成对双缓冲；主线程在 `SwapBuffers()` 后执行完整命令序列
- **同步机制**：`DrainWorker()` 确保 `Flush()` 前所有工作项已处理；统计信息记录处理数量、队列高水位与等待时长

---

## 调试与测试

- **示例程序**：`37_batching_benchmark` 提供三种模式的对比测试，日志记录 FPS、Draw Call、批次数等
- **日志节流**：批处理调试日志默认每 120 次刷新打印一次，若检测到回退则立即输出，避免淹没控制台
- **常见注意事项**：
  - 所有材质 uniform 必须通过 `UniformManager` 设定，保持与批处理协同
  - 批处理资源由 `ResourceManager` 托管，新增网格需在 CMake 中注册
  - 后台线程依赖 `GLThreadChecker`，确保所有 OpenGL 调用仍在主线程执行

---

[← RenderBatching 方案](RenderBatchingPlan.md) | [Renderer →](Renderer.md)

