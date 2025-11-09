# Mesh/Assimp 高级导入方案

## 背景

现有 `MeshLoader` 已经覆盖基本几何体生成与 Assimp 几何数据导入，并通过 `LoadFromFileWithMaterials()` 提供材质加载。然而针对复杂模型（多 UV、多颜色通道、骨骼蒙皮、动画等）仍存在信息缺口，导致：
- 需要手动补充蒙皮数据，无法直接驱动骨骼动画。
- 无法访问多套 UV/顶点颜色，限制了高级材质流程。
- 无统一入口配置 Assimp 后处理选项及格式差异。

本方案旨在补齐上述缺口，并建立面向后续骨骼动画、MMD 特性和自定义渲染流程的统一导入能力。

## 现状评估

- **自动处理能力**：已启用 `Triangulate`、`GenSmoothNormals`、`CalcTangentSpace` 等关键标志，满足大多数静态网格场景。
- **材质支持**：`LoadFromFileWithMaterials()` 可以解析纹理与基础物理参数，但仍以第一套 UV/颜色为主。
- **数据缺失**：`Mesh` 顶点只包含单套 UV/颜色，未存储骨骼权重、绑定姿势矩阵；`MeshLoader` 没有暴露 Assimp 解析到的蒙皮、层级、节点变换等信息。

## 新增能力（已落地）

- **统一区域结构**  
  - `MeshImportOptions`：集中控制翻转 UV、自动上传、Assimp 后处理标志、蒙皮信息采集、权重归一化等。
  - `MeshImportResult`：返回 `Ref<Mesh>`、可选 `Ref<Material>` 与 `MeshExtraData`，其中 `MeshExtraData` 承载局部/世界变换、多套 UV、颜色通道以及 `MeshSkinningData`。
  - `MeshSkinningData`：包含骨骼列表（名称、父级、Offset 矩阵）、每个骨骼影响的顶点权重、顶点到骨骼的权重映射。
  - `ModelPart` 直接持有 `MeshExtraData`，并提供 `HasSkinning()`/`GetSkinningData()` 与 `Model::HasSkinning()`，为后续渲染与动画系统暴露统一查询入口。

- **高级加载接口**  
  - 新增 `MeshLoader::LoadDetailedFromFile()`：一次性加载几何、材质（可选）、UV/颜色通道、蒙皮信息，并保留节点层级矩阵。
  - 支持限制/归一化骨骼权重、自动收集多 UV 与多颜色通道，满足 PBR、光照贴图、MMD Toon 等后续需求。

- **辅助工具链**  
  - 抽取出 `ResolveBasePath()`、`GeneratePostProcessFlags()`、`ProcessAssimpNodeDetailed()` 等复用函数，便于未来扩展更多导入流程。
  - 新增 `FindNodeByName()`、`ConvertMatrix()`，确保骨骼父子层级与绑定矩阵正确解析。

## 待补充功能

| 功能领域 | 当前状态 | 后续工作 |
| --- | --- | --- |
| 骨骼动画 | 仅导出骨骼与权重数据；未上传到 GPU | 设计 `SkinnedMesh`/`SkinningBuffer`，在渲染管线中上传骨骼姿势矩阵；对接 Animator |
| 多 UV/颜色 | 数据已采集，尚未进入 Material/Uniform | 扩展 `Material`/`UniformManager`，允许绑定额外贴图集与颜色层 |
| 动画 Clip | 未解析 | 引入 `MeshAnimationData`（骨骼关键帧、Morph Targets），并提供动画解析器 |
| MMD 特性 | 基础几何 + 材质 | 增补 Toon/Sphere Map、Morph、物理骨骼等特殊处理 |

## 分阶段路线

1. **阶段 1（当前）**  
   - 建立高级数据结构与导入管线 ✅  
   - 通过 `LoadDetailedFromFile()` 验证多格式、多信息载入流程 ✅

2. **阶段 2（近期）**  
   - 为资源管理器增添注册接口，使 `MeshExtraData` 与 `MeshSkinningData` 可被 ECS/渲染系统消费。  
   - 设计 SkinnedMesh GPU Buffer，实现基础骨骼动画渲染路径。  
   - 将多 UV/颜色与材质系统打通（PBR、遮罩、AO）。  

3. **阶段 3（扩展）**  
   - 增加动画 Clip/Morphs 解析与播放。  
   - 引入 Assimp 特定格式配置（FBX Pivots、GLTF Draco、PMX Toon）。  
   - 构建测试样例与工具脚本，验证跨格式一致性。

## 下一步

- 将 `MeshImportResult` 接入 `ResourceManager`，提供可查询的蒙皮与层级数据。
- 设计 GPU 侧骨骼缓冲结构以及 `SkinnedMeshRenderable`，打通渲染链路。
- 扩充单元测试/示例（含 PMX/FBX/GLTF），确保多格式一致工作。
- 更新文档（API / Guides）说明新接口与数据结构的使用方式。

