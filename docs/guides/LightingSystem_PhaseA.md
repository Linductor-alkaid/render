[返回文档首页](../README.md)

---

# 阶段A：光照系统设计方案

## 🎯 目标

- 在保持现有渲染/资源/批处理框架稳定的前提下，引入统一的实时光照支撑层，为后续阴影、后处理与 PBR 扩展夯实数据结构。
- 完成 `PHASE1_BASIC_RENDERING.md` 中“光照系统”条目的基础能力：光源类型、管理器、Uniform 上传与排序策略。
- 为 ECS、Renderer、UniformManager 提供清晰接口，确保线程安全与资源生命周期符合项目既有约束。

## 📌 基本要求

- **光源类型**：支持 `Directional`、`Point`、`Spot`、`Ambient`，属性覆盖颜色、强度、世界空间参数与衰减/夹角。
- **管理器**：集中注册/注销/更新，限制最大数量（默认 8，可配置），对外提供稳定句柄与只读快照。
- **Uniform**：通过 `UniformManager` 将结构化 `uLighting.*` uniform 写入着色器，提供方向光/点光/聚光/环境光数组以及相机位置；命名遵循 `uLighting.<Field>`。晚于最大容量时仍保留回退路径。
- **线程安全**：对外接口使用 `std::shared_mutex` 保护；OpenGL 调用仍在 Renderer 主线程执行。
- **资源一致性**：通过 `ResourceManager` 管理光照着色器与关联材质；新增 shader 需登记至 CMake。
- **回归验证**：提供示例/测试确保材质排序键仍稳定 (`materialSortKeyMissing == 0`)。

## 🧱 架构概览

```
Renderer::FlushRenderQueue()
 ├─ LightManager::SyncFrameData(cameraViewPos)
 │    ├─ 聚合 ECS 光源数据 (LightSystem)
 │    ├─ 生成 GPU Uniform 缓冲镜像
 │    └─ 统计活跃光源数量/排序键
 ├─ UniformManager::SetLightingData(frameBuffer)
 └─ RenderQueue 排序与 Draw Calls
```

- **LightManager**：负责光源池、静态配置、Uniform 缓冲镜像；内部维护 `std::vector<LightHandle>` 与 `LightFrameBuffer`。
- **Light** 基类：提供公共属性（`color`、`intensity`、`castsShadows`、`priority` 等）与派生专有数据（方向、位置、衰减、夹角）。
- **LightHandle**：64bit ID（高32位类型、低32位自增索引），避免裸指针；在 ECS/系统间传递。
- **Uniform 写入策略**：
  - `LightManager::BuildFrameSnapshot` 输出每帧光源快照；`UniformSystem::SetLightUniforms()` 将数据拆分为 `uLighting.*` uniform 数组。
  - 超出 `LightLimits` 定义的最大数量会被截断，同时记录 `culled*` 统计，保留回退排序。
- **排序策略**：按 `layerID`(`LightManager` 预设) → `priority` → `intensity`；与材质键结合以减少状态切换。

## 🔧 代码结构调整

```
include/render/lighting/
  light.h
  light_types.h
  light_manager.h
src/rendering/lighting/
  light.cpp
  light_manager.cpp
```

- **新枚举** `LightType`：`Directional`, `Point`, `Spot`, `Ambient`, `Unknown`。
- **数据结构**：
  - `struct LightCommonData { Color color; float intensity; bool castsShadows; bool enabled; ... }`
  - `struct DirectionalLightData { Vector3 direction; }` 等。
  - `struct LightingFrameSnapshot { std::vector<LightParameters> directionalLights; ... }` 用于传递 CPU 侧快照。
- **线程安全**：`LightManager` 使用 `mutable std::shared_mutex m_mutex;`；注册/更新写锁，查询读锁。
- **事件流**：
  1. ECS `LightSystem` 每帧查询 `LightComponent` → 调用 `LightManager::UpdateLight(handle, data)`.
  2. Renderer 在 `FlushRenderQueue` 前调用 `LightManager::SyncFrameData`，输出 `LightingUniformBlock`.
  3. `UniformManager` 负责缓存 UBO/SSBO bind point 与实际 `glBufferSubData`。

## 🧩 ECS 集成计划

| 类/组件 | 新增内容 | 说明 |
| --- | --- | --- |
| `LightComponent` | 光源公共属性 + 类型特定字段 | 使用 `std::variant` 或拆分子结构 |
| `LightSystem` | 维护对象池、与 `LightManager` 同步 | 仅在主线程运行 |
| `TransformComponent` | 提供位置/方向 | 点光/聚光读取世界矩阵 |
| `Material` 扩展 | 增加 `RequiresLighting()` 标记 | 无需光照的材质可跳过 uniform |

- 当实体移除 `LightComponent` 时自动注销光源句柄。
- 支持 `priority` 与 `influenceRadius` 控制排序与裁剪，远距离光源可被剔除。

## 🧪 Uniform 布局实现

```
struct LightingData {
    int directionalCount;
    int pointCount;
    int spotCount;
    int ambientCount;
    int hasLights;
    vec3 cameraPosition;
    vec4 directionalDirections[MAX_DIRECTIONAL];
    vec4 directionalColors[MAX_DIRECTIONAL];
    vec4 pointPositions[MAX_POINT];
    vec4 pointColors[MAX_POINT];
    vec3 pointAttenuation[MAX_POINT];
    vec4 spotPositions[MAX_SPOT];
    vec4 spotColors[MAX_SPOT];
    vec4 spotDirections[MAX_SPOT];
    vec3 spotAttenuation[MAX_SPOT];
    float spotInnerCos[MAX_SPOT];
    vec4 ambientColors[MAX_AMBIENT];
    int culledDirectional;
    int culledPoint;
    int culledSpot;
    int culledAmbient;
};
```

- GLSL 片段着色器通过 `uniform LightingData uLighting;` 获取全部数据，结合 `uLighting.hasLights` 判断是否走多光源路径。
- 方向光、点光、聚光分别使用 `.directional*` / `.point*` / `.spot*` 数组；颜色 `vec4` 的 `w` 分量存放强度。
- 衰减系数与内/外夹角以数组形式传输，便于着色器复用。

## 🎨 着色器整合

- `material_phong.frag`、`camera_test.frag`、`mesh_test.frag` 均已兼容 `uLighting` 数据结构，自动遍历多光源并保留 `uLightPos`/`uLightColor` 兼容路径。
- 方向光默认使用法线与光向量的点积，点光/聚光按距离与衰减系数计算；聚光额外根据内外角余弦插值。
- 如果场景未注册新式光源，着色器自动回退到旧的单光源逻辑，避免历史示例闪退。

## 📈 排序与裁剪

- `LightManager` 按照 `priority`、`distanceToCamera`（点/聚光）或 `enabled` 状态筛选。
- 提供 `CullLights(const Frustum&)` 接口以支持多相机（未来扩展）。
- Renderer 统计 `lightUniformBytes`、`culledLights` 写入 `RenderStats`。

## 🔍 测试与验证

- **单元测试** (`tests/lighting/light_manager_tests.cpp`)
  - 注册数量上限与回退策略。
  - 多线程读写（确保无死锁）。
  - Uniform 布局与结构体大小断言。
- **集成测试** (`examples/45_lighting_test.cpp`)
  - 展示方向光 + 动态点光 + 聚光灯动画，验证 uniform/排序/裁剪。
  - 启动日志对 `materialSortKeyMissing` 仍为 0 的统计进行观察。
  - 切换批处理模式验证兼容。
- **基准测试指标**：UBO 更新时间、光源剔除数量、Draw Call 保持不变。

## 🚀 开发顺序

1. ~~**接口搭建**：实现 `light.h/.cpp`、`light_manager.h/.cpp`，注册到 CMake，补充基础单元测试骨架。~~
2. ~~**Uniform 集成**：在 `UniformManager` 中新增 `SetLightingData` API，Renderer 在 `FlushRenderQueue` 前调用。~~
3. ~~**ECS 打通**：新增 `LightComponent`/`LightSystem`，提供示例注册。~~
4. ~~**示例与文档**：完成 `examples/45_lighting_test.cpp` 与 `docs/api/Lighting.md`，更新索引。~~
5. **回归**：执行现有批处理/材质测试，确保统计不回退；补充自动化脚本。

---

[上一页](2D_UI_Guide.md) | [下一页](../MATERIAL_SYSTEM.md)

