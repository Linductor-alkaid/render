# ModelRenderer API 参考

[返回 API 首页](README.md)

---

## 概述

`ModelRenderer` 是 `Renderable` 家族中负责 3D 组合模型渲染的派生类。它聚合了一个 `ModelPtr`，在每帧渲染时迭代模型内的全部 `ModelPart`，完成材质绑定、Uniform 设置（依赖 `UniformManager` 满足项目统一要求）以及网格绘制。  
该组件由 `ModelRenderSystem` 管理，支持对象池复用、视锥体裁剪、材质排序键构建，并在透明/不透明通道中与现有 `RenderQueue` 协同工作。

**头文件**: `render/renderable.h`  
**命名空间**: `Render`

---

## 功能特性

- ✅ **Renderable 体系集成**：继承自 `Renderable`，复用图层、优先级、包围盒接口。  
- ✅ **多部件渲染**：遍历 `ModelPart`，对缺失材质/网格进行容错处理。  
- ✅ **材质排序键**：通过 `Renderer::BuildModelRenderableSortKey()` 生成稳定的 `MaterialSortKey`，减少状态切换。  
- ✅ **UniformManager 驱动**：统一设置 `uModel`、`uView`、`uProjection` 等矩阵以及 `hasDiffuseMap`/`hasNormalMap` 标志，满足用户统一 Uniform 管理要求。  
- ✅ **附加 UV 缩放开关**：仅当 `ModelPart::extraData->enableUvChannelScaling` 为 `true` 时才会推送 `uExtraUVSetScales`，避免默认对模型主贴图产生影响。
- ✅ **裁剪优化**：缓存模型整体包围盒，并在 `ModelRenderSystem` 中结合 `CameraSystem` 视锥体进行裁剪。  
- ✅ **线程安全**：公共方法使用互斥量保护，支持在 ECS 提交流程与渲染线程之间安全访问。

---

## 继承关系

```
Renderable (基类)
└── ModelRenderable
```

`ModelRenderable` 复用基类提供的：
- 线程安全开关（`SetVisible`/`IsVisible`、`SetLayerID` 等）
- 世界变换缓存（`SetTransform(const std::shared_ptr<Transform>&)`）
- 阴影属性与透明提示

---

## 主要成员

| 成员 | 类型 | 说明 |
| --- | --- | --- |
| `SetModel(ModelPtr)` | 方法 | 绑定模型，刷新缓存的部件数量、包围盒、蒙皮标记。 |
| `GetModel() const` | 方法 | 返回当前模型指针（共享锁保护）。 |
| `GetBoundingBox() const override` | 方法 | 返回模型整体包围盒，内部会缓存 `Model` 的 AABB 并结合世界变换。 |
| `Render(RenderState*) override` | 方法 | 在当前渲染阶段执行绘制；遍历所有部件进行材质/网格处理。 |
| `SubmitToRenderer(Renderer*) override` | 方法 | 将自身提交到 `Renderer::SubmitRenderable()`，由渲染队列统一调度。 |
| `HasSkinning() const` | 方法 | 查询缓存的蒙皮标志，便于阴影 pass 或动画系统判断。 |

内部还维护：
- `std::vector<ModelPart> m_modelPartsCache`：用于在提交阶段缓存部件快照，避免渲染时再次加锁。  
- `AABB m_modelBoundsCache` 与 `bool m_modelBoundsDirty`：配合 `Model` 的懒计算逻辑更新包围盒。  
- `bool m_hasSkinning`：由 `Model::HasSkinning()` 预取结果。

---

## 渲染流程

1. **模型缓存**：`SetModel()` 读取 `Model` 部件数据写入本地缓存，记录蒙皮标志与包围盒状态。  
2. **提交阶段**：`ModelRenderSystem` 调用 `SubmitToRenderer()`，在此之前会根据视锥体过滤不可见实体。  
3. **排序键生成**：`Renderer::EnsureMaterialSortKey()` 针对 `RenderableType::Model` 调用 `BuildModelRenderableSortKey()`，通常取首个有效材质/着色器作为排序依据。  
4. **渲染阶段**：`Render()` 对缓存的部件执行：  
   - 检查材质与网格有效性；  
   - 调用材质 `Bind()`，由 `UniformManager` 设置 `uModel` 等矩阵；  
   - 将材质状态（diffuse/normal map）同步到 `hasDiffuseMap`、`hasNormalMap` uniform；  
   - 调用 `mesh->Draw()` 完成绘制。

> 注意：`ModelRenderable` 本身不直接访问摄像机 Uniform，而是依赖上层 `UniformSystem` 遍历模型组件后，统一写入 `uView`/`uProjection`/`uViewPos` 等常用参数。

---

## 与 ECS 的协同

- `ModelComponent`：在 ECS 中记录模型名称、`ModelPtr`、资源注册结果及阴影/可见性标记。  
- `ModelRenderSystem`：  
  - 查询 `TransformComponent` + `ModelComponent`；  
  - 调用 `ModelRenderable::SetTransform()` 与 `SetModel()`；  
  - 判断 `ModelComponent::resourcesLoaded`、透明提示等状态；  
  - 维护对象池 `std::vector<ModelRenderable>`，避免频繁分配。  
- `ResourceLoadingSystem`：处理模型异步加载回调，在主线程更新组件并触发 `ModelRenderable` 刷新。

---

## 使用示例

```cpp
#include <render/renderer.h>
#include <render/ecs/world.h>
#include <render/ecs/components.h>

// 假设 modelComp 已加载完成
ModelRenderable renderable;
renderable.SetTransform(transformComponent.transform);
renderable.SetModel(modelComp.model);
renderable.SetLayerID(modelComp.layerID);
renderable.SetCastShadows(modelComp.castShadows);
renderable.SetReceiveShadows(modelComp.receiveShadows);

// 交给 Renderer 排队
renderer->SubmitRenderable(&renderable);
```

在实际项目中，该流程由 `ModelRenderSystem` 自动完成，上述代码可用于调试或手动提交模型实例。

---

## 常见问题（FAQ）

**Q1: 模型更新后需要手动刷新缓存吗？**  
A: 使用 `SetModel()` 会自动刷新缓存；若模型内容在运行时改变，可再次调用 `SetModel()` 或在 ECS 中重新装载 `ModelComponent`。

**Q2: 如何支持材质覆盖？**  
A: `ModelComponent` 暂存材质覆盖数据，`ModelRenderSystem` 会在提交阶段将覆盖内容写入材质的自定义 uniform；如需 per-part 覆盖，可扩展 `ModelComponent` 数据结构，并在渲染时根据部件名称匹配。

**Q3: 是否支持 GPU Instancing？**  
A: 目前 `ModelRenderable` 走的是逐实例提交流程；批量模型实例化可在后续阶段通过 `Renderer::CreateBatchableItem()` 扩展 instancing 支持。

---

[上一篇: Model](Model.md) | [下一篇: ModelLoader](ModelLoader.md) | [返回 API 首页](README.md)

