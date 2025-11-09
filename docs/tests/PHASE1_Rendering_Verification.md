# Phase 1 渲染功能验证清单

[返回文档首页](../README.md)

---

## 目标

确保 Phase 1 网格/材质/法线贴图相关改动具备可操作的验证手段，可用于手动回归与后续自动化脚本迁移。

---

## 当前验证项

- **法线贴图演示**：`examples/46_normal_map_test`
  - 启动命令：`46_normal_map_test`
  - 热键：
    - `N` 切换法线贴图
    - `D` 切换漫反射贴图
    - `L` 切换旋转光源
  - 使用 `textures/manfanshetest.*` 验证漫反射采样，确保不再复用缓存纹理。
  - 日志关注：`漫反射贴图加载成功` 与 `漫反射贴图启用` Debug 输出。

- **骨骼矩阵正确性**：`examples/47_skeleton_palette_test`
  - 启动命令：`47_skeleton_palette_test`
  - 预期输出：
    - 控制台列出骨骼结构
    - 子骨骼世界矩阵与 Skinning palette 矩阵最后一列应为 `(1, 0, 0, 1)`。

- **模型渲染渲染管线**：`examples/48_model_render_test`
  - 验证材质 `RegisterTextureUniform` 接入路径是否正常设置 `diffuseMap` / `normalMap`。
  - 日志检查：
    - `UniformManager::RegisterTextureUniform` 相关警告是否为 0。
    - `hasDiffuseMap` / `hasNormalMap` uniform 切换是否正确。
- **复杂模型加载与渲染**：`examples/49_miku_model_test`
  - 使用 `ModelLoader` 加载 `models/miku/v4c5.0short.pmx`，验证多子网格/多材质模型的注册与渲染。
  - 观察：模型应完整呈现并缓慢旋转，支持第一人称相机（WASD/QE 移动、Shift 加速、鼠标视角、Tab 捕获/释放），日志包含资源注册信息且无 “材质没有着色器” 等警告。
  - 可用于回归 `ModelRenderSystem`、`UniformSystem` 对模型材质的统一处理。
- **多 UV & 顶点颜色映射**：`examples/50_geometry_catalog_test`（新增 uniform）
  - 开启 `Tab` 线框/填充切换，留意控制台打印 `uExtraUVSetCount` / `uExtraColorSetCount`（>0 表示启用）。
  - Shader `material_phong` 会依次应用 `uExtraUVSetScales` 中的缩放系数到 `TexCoord`，并将 `uExtraColorSets` 乘入最终色值，可观察材质随缩放/调色组合变化。
  - 如需单独验证，可复制用例并将 `GeometryPreset::CreatePlane` 生成的 `MeshExtraData` 写入第二套 UV/颜色，观察漫反射贴图是否按比例缩放、颜色是否叠乘。
- **预设几何体目录**：`examples/50_geometry_catalog_test`
  - 通过 `GeometryPreset::GetMesh` 展示常用几何体（Cube/Sphere/Cylinder/Cone/Torus/Capsule/Quad/Triangle/Circle）。
  - 操作：`Tab` 切换线框、`Space` 暂停旋转、`ESC` 退出；确认不同材质颜色与动画更新。
  - Sphere/Torus 共享 `textures/faxiantest.*` 法线贴图，观察 `hasNormalMap` 日志并对比 `Tab` 线框开关，确保切线空间与材质 uniform 生效。
  - 可与 `43_sprite_batch_validation_test` 联动，先运行 50 号用例确认预设资源注册，再执行 43 号验证批渲染统计，观察法线贴图开启后渲染态是否一致。
- **模型加载回归**：`model_loader_regression_test`
  - 运行方式：`ctest -C Release -R model_loader_regression_test --output-on-failure`
  - 自动验证导入缺失切线时的补齐效果、延迟上传与资源引用计数。
- **法线切线单元测试**：`tests/mesh_tangent_test`
  - 运行方式：`ctest -R mesh_tangent_test` 或直接执行 `mesh_tangent_test`
  - 功能：构建平面三角网格，调用 `Mesh::RecalculateTangents()` 并验证切线/副切线的正交归一与右手性。
  - 该用例将纳入 CI，可在本地 `cmake --build . --target mesh_tangent_test && ctest -R mesh_tangent_test` 快速回归。

---

## 回归策略

1. **手动回归**（当前阶段）
   - 每次改动网格/材质系统后运行上述三个示例，确认输出与视觉效果符合预期。
   - 记录测试结果到 `docs/tests/PHASE1_Rendering_Verification.md`（本文件）。

2. **自动化计划**
   - 计划在 Phase 1 末期扩展 `tests/` 目录：
     - 基于 `Render::Mesh` 单元测试法线/切线重建函数。
     - 引入截图对比或缓冲区采样脚本，验证法线贴图影响。
   - 与 CI/CD 整合后，将上述示例编译后在本地运行并收集日志断言。

---

## 记录

| 日期 | 验证项 | 结果 | 说明 |
| ---- | ------ | ---- | ---- |
| 2025-11-09 | 46_normal_map_test | ✅ | 漫反射/法线贴图切换正常，日志输出纹理尺寸 |
| 2025-11-09 | model_loader_regression_test | ✅ | PMX 模型导入回归通过，遍历骨骼与材质输出正确 |
| 2025-11-09 | 50_geometry_catalog_test | ✅ | 预置几何体渲染正常，线框切换和旋转动画运行稳定 |
| 2025-11-09 | mesh_tangent_test | ✅ | Mesh::RecalculateTangents 正交性自检通过，便于自动化集成 |
