[返回 API 首页](README.md)

---

# Lighting 光照系统

## 概述

`Lighting` 模块提供多光源统一管理与上传流程，涵盖光源组件、运行时管理器以及与着色器的 `uLighting.*` Uniform 协议。通过它可以在 ECS 世界中注册方向光、点光源、聚光灯和环境光，并在渲染阶段一次性将结构化光照数据推送到 GPU。

核心目标：
- 统一 `LightComponent` → `LightSystem` → `LightManager` → `UniformSystem` 的数据链路。
- 支持最多 4 组方向光、8 个点光、4 个聚光灯与 4 个环境光（默认，可配置）。
- 与旧版 `uLightPos` / `uLightColor` 单光源接口兼容，避免历史着色器断档。

## 关键类型

| 模块 | 说明 |
| --- | --- |
| `LightComponent` | ECS 组件，定义光源类型、颜色、强度、范围、衰减及阴影标记。 |
| `LightSystem` | 每帧收集所有启用光源，注册/更新到 `LightManager`，并维护主光源缓存（供旧 uniform 使用）。 |
| `Lighting::LightManager` | 线程安全的光源管理器，生成 `LightingFrameSnapshot`（按优先级与距离裁剪）。 |
| `UniformSystem` | 构建并写入结构化 `uLighting` uniform，遍历所有使用光照的着色器。 |
| `Lighting::LightLimits` | 定义最大光源数量，可在运行时通过 `LightManager::SetLimits()` 调整。 |

## Uniform 协议

`UniformSystem::SetLightUniforms()` 会根据快照写入以下结构（GLSL 中定义为 `uniform LightingData uLighting;`）：

```
struct LightingData {
    int directionalCount;
    int pointCount;
    int spotCount;
    int ambientCount;
    int hasLights;            // 是否启用多光源路径
    vec3 cameraPosition;      // 最近一次相机位置

    vec4 directionalDirections[MAX_DIRECTIONAL]; // xyz: 方向, w 未使用
    vec4 directionalColors[MAX_DIRECTIONAL];     // rgb: 颜色, a: 强度

    vec4 pointPositions[MAX_POINT];   // xyz: 位置, w: 作用范围
    vec4 pointColors[MAX_POINT];      // rgb: 颜色, a: 强度
    vec3 pointAttenuation[MAX_POINT]; // (常数, 线性, 二次)

    vec4 spotPositions[MAX_SPOT];     // xyz: 位置, w: 作用范围
    vec4 spotColors[MAX_SPOT];        // rgb: 颜色, a: 强度
    vec4 spotDirections[MAX_SPOT];    // xyz: 朝向, w: 外角余弦
    vec3 spotAttenuation[MAX_SPOT];   // (常数, 线性, 二次)
    float spotInnerCos[MAX_SPOT];     // 内角余弦

    vec4 ambientColors[MAX_AMBIENT];  // rgb: 颜色, a: 强度倍率

    int culledDirectional;
    int culledPoint;
    int culledSpot;
    int culledAmbient;
};
```

> ⭐ 提示：如果 `hasLights == 0`，着色器应回退到传统 `uLightPos`/`uLightColor` 逻辑，保证旧资源兼容。

## 使用流程

1. **注册组件与系统**
   ```cpp
   world.RegisterComponent<LightComponent>();
   world.RegisterSystem<LightSystem>(renderer);
   world.RegisterSystem<UniformSystem>(renderer);
   world.RegisterSystem<MeshRenderSystem>(renderer);
   ```

2. **创建光源实体**
   ```cpp
   EntityID sun = world.CreateEntity({ .name = "Sun" });
   TransformComponent transform;
   transform.SetPosition({-5.0f, 10.0f, 4.0f});
   transform.transform->LookAt({0.0f, 0.0f, 0.0f});
   world.AddComponent(sun, transform);

   LightComponent light;
   light.type = LightType::Directional;
   light.color = Color(1.0f, 0.97f, 0.9f, 1.0f);
   light.intensity = 1.2f;
   world.AddComponent(sun, light);
   ```

3. **运行时更新**
   - `LightSystem` 会在 `world.Update()` 中同步组件状态到 `LightManager`。
   - `UniformSystem` 在同一帧构建快照，写入所有启用着色器的 `uLighting` uniform。

4. **着色器读取**
   ```glsl
   if (uLighting.hasLights != 0) {
       for (int i = 0; i < uLighting.directionalCount; ++i) {
           vec3 dir = normalize(-uLighting.directionalDirections[i].xyz);
           vec3 color = uLighting.directionalColors[i].rgb * uLighting.directionalColors[i].a;
           // ... 计算漫反射与高光 ...
       }
       // 同理遍历 point / spot / ambient
   } else {
       // 旧版单光源代码
   }
   ```

5. **调试与统计**
   - `Renderer` 日志会打印 `culled*` 与 `materialSortKeyMissing` 指标。
   - `LightManager::BuildFrameSnapshot()` 保证按优先级与距离排序，首个可用光源用作 Legacy fallback。

## 示例程序

- **`examples/45_lighting_test.cpp`**：方向光 + 动态点光 + 聚光灯演示，核对着色器 uniform 与 `LightSystem` 行为。
- **`examples/20_camera_test.cpp`**：更新后的 `camera_test.frag` 展示多光源兼容路径。

## 与其他模块的关系

- `LightComponent` 属于 ECS 组件，依赖 `TransformComponent` 提供世界空间位置/方向。
- `LightManager` 位于渲染层，通过 `Renderer::GetLightManager()` 共享给系统和示例。
- `UniformManager` 新增 `SetVector4Array` 等接口以支持批量 uniform 写入。

## 常见问题

- **为何日志中只有 fallback draw？** 当前示例未启用批处理，所有 Renderable 直接走原始绘制路径，属预期行为。
- **如何调整最大光源数？** 使用 `LightManager::SetLimits()` 设置 `LightLimits`，着色器需同步调整 `MAX_*` 常量。
- **为何旧材质仍能工作？** 当场景无新式光源或 shader 未声明 `uLighting` 时，系统自动回退到单光源 uniform。

---

[上一篇: RenderBatching](RenderBatching.md) | [下一篇: GLThreadChecker](GLThreadChecker.md)
