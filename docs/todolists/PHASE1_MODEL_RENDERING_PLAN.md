# Phase 1: 模型渲染与基础几何形状开发方案

[返回文档首页](../README.md)

---

## 🎯 阶段目标
- 交付 `Model`/`ModelRenderer` 体系，完成 `PHASE1_BASIC_RENDERING.md` 中“模型渲染”相关未完项（`Model` 类、`ModelRenderer`、模型加载流程）。
- 梳理与扩展基础几何网格生成，覆盖 TODO 列表中“预定义网格生成器”子任务，并与资源管理、批处理策略保持一致。
- 保障与既有系统（`ResourceManager`、`UniformManager`、ECS、渲染排序/批处理框架）无缝协作，延续线程安全与统一资源接入规范。

---

## 🧭 现状分析
- 渲染核心：`Mesh`、`MeshRenderable`、`Renderer`、`RenderQueue`、`MaterialStateCache` 等已完备，支持批处理和材质键缓存。
- 资源路径：`MeshLoader` 已支持文件加载、材质打包与基础几何函数（`CreateCube` 等），但缺乏面向“模型（多子网格+材质）”的一等抽象及缓存复用策略。
- ECS：`MeshRenderSystem` 已上线，`Renderable` 层具备对象池。尚无面向模型的组件/系统与多子网格提交流程。
- 统一管理：Uniform 需通过 `UniformManager` 设置（[[memory:7889023]]），资源需经 `ResourceManager` 注册（[[memory:7392268]]）；现阶段模型/几何体尚未纳入统一策略。

---

## 📌 功能需求拆解

### Model 核心能力
- 表达多子网格模型：维护 `ModelPart`（网格 + 材质 + 本地变换/包围盒）列表，可嵌套节点层级以支持复杂模型。
- 数据管理：提供加载状态、包围盒（整体/分部）、统计信息（网格数、材质数）与线程安全访问接口。
- 资源集成：支持与 `ResourceManager`、`AsyncResourceLoader` 协同，提供命名注册/引用计数、延迟上传、批量上传钩子。
- Uniform 设置：在渲染提交前，通过 `UniformManager` 统一上传 `uModel`、`material.*`、光照相关字段，遵循项目规范。

### ModelRenderer / Model 系统
- 渲染流程：`ModelRenderer` 继承 `Renderable`，按模型子网格生成批次，复用材质键排序；支持阴影、透明排序、统计接口。
- 提交流程：将每个 `ModelPart` 映射为内部 `MeshRenderable`/DrawCall，避免重复创建，可复用对象池。
- ECS 扩展：设计 `ModelComponent` 与 `ModelRenderSystem`（或扩展 `MeshRenderSystem`）以处理模型实体，多相机/层级兼容。
- 调试与诊断：提供可视化统计、日志标签，保障与现有 `Renderer::FlushRenderQueue` 报告格式一致。

### 基础几何形状体系
- 统一入口：基于 `MeshLoader` 现有实现，补充参数校验、LOD 版本（简化/高模）、UV/法线一致性验证，形成 `GeometryLibrary` / `GeometryPreset` 目录。
- 资源缓存：支持 `ResourceManager` 中“命名几何体”注册，避免重复生成；新增批量上传/预热接口。
- 扩展形状：补齐 TODO 中列出的圆锥、圆柱、圆环、胶囊等生成函数文档、单元测试与示例覆盖。
- 切线空间：所有 procedural mesh 默认生成切线/副切线，支持法线贴图、各向异性材质。
- 测试矩阵：针对每种几何体校验包围盒、顶点数量、法线/切线正确性与渲染效果（线框/实体），确保与现有批处理兼容。

---

## 🧱 架构与模块设计

### 1. 数据结构层
- `Model`：管理模型元数据、`ModelNode`（层级）、`ModelPart`（网格/材质绑定），线程安全读写。
- `ModelPart`：包含 `Ref<Mesh>`、`Ref<Material>`、局部矩阵、可选动画占位（后续扩展）；提供快速访问与渲染标记。
- `ModelBounds`：聚合所有部件包围盒，支持裁剪优化（与 `MeshRenderable::GetBoundingBox` 风格一致）。

### 2. 资源加载与缓存
- `ModelLoader`（新）：封装 `MeshLoader::LoadFromFileWithMaterials`，产出 `Model`；处理路径、材质、纹理注册；支持延迟上传。
- `ResourceManager` 扩展：新增模型注册查询接口（`RegisterModel`/`GetModel`），与现有引用计数一致；批量预热 `ModelPart` 网格。
- `AsyncResourceLoader`：添加模型异步任务，复用现有线程池、进度回调、延迟上传策略。

### 3. 渲染与 ECS 集成
- `ModelRenderer`：内部持有 `Model` 引用，在 `Render()` 中遍历 `ModelPart`，使用材质绑定 + `UniformManager` 设置 + 网格绘制；可与 `MaterialStateCache` 协同减少重复 Bind。
- 对象池策略：沿用 `MeshRenderSystem` 池化思路，创建 `ModelRenderablePool`，避免频繁分配。
- `ModelRenderSystem`：在 ECS 中查找 `ModelComponent`，确保资源加载完成后提交 `ModelRenderer`；集成视锥体裁剪、多层级排序。
- 透明管线：与既有透明排序逻辑对齐（`Renderer::SortRenderQueue`），新增模型透明部件处理策略。

### 4. 基础几何体整理
- API 重构：在 `MeshLoader` 维度统一命名/参数顺序，补充文档，必要时拆分到 `geometry/` 子目录。
- 生成策略：引入细分级别枚举、共享顶点缓存（可选），确保线程安全。
- 资源入口：提供 `GeometryPreset::RegisterDefaults(ResourceManager&)`，一次性注册常用形状（Cube/Sphere/Plane 等）。

---

## 🚧 实施计划

### Phase A - 准备与设计验证
- 梳理现有 `MeshLoader`、`ResourceManager`、`AsyncResourceLoader`、`MeshRenderSystem` 实现，记录接口差异。
- 产出 `Model`/`ModelPart`/`ModelRenderer` 头文件草稿与 UML（放置于 `docs/guides` 或 API 目录）。
- 梳理 Mesh 切线空间需求，确认顶点布局调整范围与既有着色器/资源的影响面。
- 完成本方案文档（当前任务），同步参与者共识与评审。

### Phase B - 模型数据与资源管线
- [x] 实现 `Model` 基础类、线程安全封装、包围盒计算。
- [x] 开发 `ModelLoader`，整合 `MeshLoader::LoadFromFileWithMaterials`，增加资源注册、延迟上传策略。
- [x] 扩展 `ResourceManager`、`AsyncResourceLoader` 支持模型资产。
- [x] `MeshLoader` 导入流程补全切线/副切线缺失路径，落地法线贴图最小闭环示例。
- [x] 单元测试：模型加载、引用计数、延迟上传回归。

### Phase C - 渲染路径与 ECS
- [x] 编写 `ModelRenderer`，对接 `Renderer::SubmitRenderable`、`RenderQueue`。
- [x] 新增 `ModelComponent` 与 `ModelRenderSystem`，实现对象池、视锥体裁剪、材质键生成。
- [x] 更新渲染排序/统计，保证与透明通道、阴影 pass 集成。
- [x] 集成测试：新增 `48_model_render_test`（展示多子网格、材质混合）。

### Phase D - 几何库整理与验证
- [x] 整理 `MeshLoader` 预定义几何 API，补充缺失形状/参数校验。
- [x] 建立 `GeometryPreset` 注册流程，更新 `ResourceManager` 初始化逻辑。
- [x] 编写 `50_geometry_catalog_test`（多种形状 + Wireframe 校验 + 法线贴图演示），增强 `43_sprite_batch_validation_test` 旁路检查。

### Phase E - 文档 & 最终验收
- 更新 `docs/api`：新增 `Model.md`、`ModelRenderer.md`、`ModelLoader.md`、`GeometryPreset.md`（如需）。
- 刷新 `PHASE1_BASIC_RENDERING.md` 状态，勾选完成项。
- 输出性能基准（DrawCall、材质键命中）对比，纳入 `FINAL_SUMMARY`。
- 维护 `docs/tests/PHASE1_Rendering_Verification.md`，记录法线贴图与骨骼测试结果，规划自动化脚本。

---

## ✅ 验收与测试矩阵
- **单元测试**：模型加载、资源注册、包围盒计算、延迟上传、基础几何网格属性验证。
- **集成测试**：新增示例程序验证渲染输出，多光源阴影开关、透明排序、批处理统计。
- **性能/回归**：比较 `mesh_test`、`batching_benchmark` 在启用模型后指标变化；监控 `materialSortKeyMissing`、`drawCallCount`。
- **工具链**：CI/脚本更新（`tests/CMakeLists.txt`、`examples`），确保本地 `build` 通过。

---

## ⚠️ 风险与缓解
- **资源膨胀**：模型包含大量材质/纹理，风险在于加载/内存峰值 → 采用延迟上传与批处理，提供进度回调与取消机制。
- **渲染排序冲突**：多部件模型与透明排序交织 → 在 `ModelRenderer` 中拆分透明/不透明队列，复用现有排序键生成策略。
- **线程安全**：模型遍历时的锁冲突 → 实施细粒度读锁 + 预处理结果缓存（包围盒、部件指针表）。
- **接口兼容性**：ECS/Renderer 与模型新接口耦合 → 在实现中保留旧 API，新增扩展接口，逐步迁移。

---

## 📚 文档与示例交付
- 新增/更新文档：`docs/api/Model*.md`、`docs/guides/ModelPipeline.md`（待定）、`docs/examples` 说明。
- 更新 TODO：`docs/todolists/PHASE1_BASIC_RENDERING.md` 相关条目、`FEATURE_LIST.md` 状态。
- 示例程序：`examples/48_model_render_test.cpp`、`examples/47_geometry_catalog_test.cpp`（命名暂定），附运行说明。

---

## 🔁 关联 TODO
- 覆盖 `PHASE1_BASIC_RENDERING.md` 中“模型渲染”“基本几何形状”未完成项（L289-L305）。
- 为后续“Simple Scene”“2D Demo”综合示例提供 3D 基础资源，支撑后续阶段目标。

---

[返回文档首页](../README.md)


