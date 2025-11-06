# ECS 渲染核心测试计划

[返回 PHASE1 文档](PHASE1_BASIC_RENDERING.md)

---

## 📋 测试目标

验证 ECS 系统与渲染器核心功能的集成情况，重点测试**已实现的功能**，识别并修复问题。

**测试范围**：
- ✅ MeshRenderable 和 MeshRenderSystem（已完整实现）
- ⚠️ 渲染队列管理（已实现，需验证）
- ⚠️ 视锥体裁剪（已禁用，需修复）
- ❌ SpriteRenderable（未实现，暂不测试）

---

## 🎯 测试用例

### 测试 1：基础 MeshRenderable 创建和渲染

**目的**：验证 MeshRenderSystem 能否正确创建 MeshRenderable 并提交到渲染队列

**测试步骤**：
1. 创建 World 和基础 ECS 系统
2. 创建实体并添加 TransformComponent + MeshRenderComponent
3. 同步加载网格和材质（避免异步加载的复杂性）
4. 调用 `world->Update()` 触发 MeshRenderSystem
5. 验证渲染队列中有对象
6. 验证渲染统计信息

**预期结果**：
- `GetRenderQueueSize()` > 0
- `MeshRenderSystem::GetStats().visibleMeshes` > 0
- 屏幕上能看到渲染的物体

**测试代码位置**：`examples/35_ecs_renderable_test.cpp`（新建）

---

### 测试 2：多实体渲染和对象池

**目的**：验证对象池机制和多实体渲染

**测试步骤**：
1. 创建 10 个实体，每个实体使用不同的位置
2. 所有实体共享同一个网格和材质
3. 验证对象池复用（m_renderables 向量）
4. 验证渲染队列中有 10 个对象

**预期结果**：
- 渲染队列大小 = 10
- 统计信息：visibleMeshes = 10
- 10 个物体都被正确渲染

---

### 测试 3：可见性控制

**目的**：验证 MeshRenderComponent.visible 是否生效

**测试步骤**：
1. 创建 5 个实体
2. 设置其中 3 个为 visible = true
3. 设置其中 2 个为 visible = false
4. 调用 Update 并检查渲染队列

**预期结果**：
- 渲染队列大小 = 3
- 统计信息：visibleMeshes = 3
- 只有 3 个物体被渲染

---

### 测试 4：资源加载状态检查

**目的**：验证未加载资源的实体不会被渲染

**测试步骤**：
1. 创建实体 A：mesh + material 都已加载
2. 创建实体 B：mesh 未加载
3. 创建实体 C：material 未加载
4. 创建实体 D：resourcesLoaded = false

**预期结果**：
- 只有实体 A 被提交到渲染队列
- 渲染队列大小 = 1
- 日志中有调试信息说明其他实体被跳过

---

### 测试 5：层级和优先级

**目的**：验证渲染队列排序

**测试步骤**：
1. 创建实体 A：layerID = 300（WORLD_GEOMETRY）
2. 创建实体 B：layerID = 100（SKYBOX）
3. 创建实体 C：layerID = 400（WORLD_TRANSPARENT）
4. 触发渲染并检查渲染顺序

**预期结果**：
- 渲染顺序：B（100）→ A（300）→ C（400）
- 队列按层级正确排序

**验证方法**：
```cpp
renderer->FlushRenderQueue();
// 添加日志输出每个 Renderable 的 layerID
```

---

### 测试 6：阴影属性传递

**目的**：验证阴影属性能否正确传递

**测试步骤**：
1. 创建实体：castShadows = true, receiveShadows = false
2. 验证 MeshRenderable 的属性

**预期结果**：
```cpp
auto& renderable = m_renderables[0];
assert(renderable.GetCastShadows() == true);
assert(renderable.GetReceiveShadows() == false);
```

---

### 测试 7：Transform 复用

**目的**：验证 Transform 对象被正确复用

**测试步骤**：
1. 创建实体并设置 Transform
2. 在 MeshRenderSystem 中验证 transform 指针相同
3. 修改 Transform，验证 MeshRenderable 能感知变化

**预期结果**：
- TransformComponent.transform 和 MeshRenderable.GetTransform() 指向同一对象
- 修改 Transform 后，下一帧渲染使用新位置

---

### 测试 8：包围盒计算（已知问题测试）

**目的**：验证包围盒计算逻辑（用于未来的视锥体裁剪）

**测试步骤**：
1. 创建一个立方体（size = 2）
2. 设置 Transform: position = (0,0,0), scale = (2,2,2)
3. 计算包围盒半径
4. 验证半径计算正确

**预期结果**：
```cpp
// 立方体对角线 = sqrt(3) * 2 ≈ 3.46
// 半径 = 3.46 * 0.5 = 1.73
// 考虑缩放 * 2 = 3.46
float expectedRadius = sqrt(3.0f) * 2.0f;
assert(abs(radius - expectedRadius) < 0.01f);
```

---

### 测试 9：渲染统计准确性

**目的**：验证渲染统计信息的准确性

**测试步骤**：
1. 创建 10 个可见实体
2. 创建 5 个不可见实体
3. 验证统计信息

**预期结果**：
```cpp
auto stats = meshRenderSystem->GetStats();
assert(stats.visibleMeshes == 10);
assert(stats.drawCalls == 10);
// culledMeshes = 0（因为视锥体裁剪被禁用）
```

---

### 测试 10：对象池性能（压力测试）

**目的**：验证对象池在大量实体下的表现

**测试步骤**：
1. 创建 1000 个实体
2. 运行 100 帧
3. 测量每帧的 Update 时间

**预期结果**：
- 对象池避免频繁内存分配
- 每帧 Update 时间稳定
- 无内存泄漏

**性能目标**：
- 1000 实体：< 5ms per frame
- 对象池复用率：100%（向量不重新分配内存）

---

## 🔧 已知问题验证

### 问题 1：视锥体裁剪被禁用

**测试**：
1. 在 ShouldCull 中添加计数器
2. 验证 ShouldCull 返回 false
3. 验证 culledMeshes 始终为 0

**预期行为**：
- ShouldCull 被调用但始终返回 false
- 所有对象都被渲染（即使在视锥体外）

**修复方案**：
```cpp
// 在 MeshRenderSystem::OnCreate 中不能获取 CameraSystem
// 需要在 World::PostInitialize 中设置

void MeshRenderSystem::OnCreate(World* world) {
    System::OnCreate(world);
}

// 新增方法
void MeshRenderSystem::PostInitialize() {
    // 现在可以安全地获取其他系统
    m_cameraSystem = m_world->GetSystemNoLock<CameraSystem>();
}
```

---

### 问题 2：材质排序未实现

**测试**：
1. 创建使用 Material A 的实体
2. 创建使用 Material B 的实体
3. 交替创建 A-B-A-B
4. 验证渲染顺序

**预期行为**：
- 当前：按创建顺序渲染（A-B-A-B），频繁切换材质
- 期望：按材质分组（A-A-B-B），减少切换

**当前状态**：
```cpp
// Renderer::SortRenderQueue() 中
// 2. 按材质排序（减少状态切换）
// TODO: 实现材质排序
```

---

### 问题 3：光源未自动设置

**测试**：
1. 创建 LightComponent 实体
2. 验证 LightSystem 缓存了光源数据
3. 验证渲染时需要手动设置 uniform

**当前行为**：
```cpp
// 在应用层手动设置
shader->Use();
uniformMgr->SetVector3("uLightPos", lightPos);
uniformMgr->SetVector3("uViewPos", cameraPos);
```

**期望行为**：
- LightSystem 自动将光源数据设置到 Renderer
- Renderer 在渲染时自动设置光源 uniform

---

## 📝 测试实施计划

### 阶段 1：基础功能测试（高优先级）

**目标**：验证核心渲染流程

1. ✅ 测试 1：基础 MeshRenderable 创建和渲染
2. ✅ 测试 2：多实体渲染和对象池
3. ✅ 测试 3：可见性控制
4. ✅ 测试 4：资源加载状态检查

**预计时间**：2-3 小时

**测试程序**：`examples/34_ecs_renderable_test.cpp`

---

### 阶段 2：高级功能测试（中优先级）

**目标**：验证排序、属性传递等

5. ✅ 测试 5：层级和优先级
6. ✅ 测试 6：阴影属性传递
7. ✅ 测试 7：Transform 复用

**预计时间**：1-2 小时

**测试程序**：扩展 `examples/34_ecs_renderable_test.cpp`

---

### 阶段 3：性能和已知问题（低优先级）

**目标**：识别性能瓶颈和验证已知问题

8. ✅ 测试 8：包围盒计算
9. ✅ 测试 9：渲染统计准确性
10. ✅ 测试 10：对象池性能

**预计时间**：2-3 小时

**测试程序**：`examples/35_ecs_performance_test.cpp`

---

## 🐛 问题修复优先级

基于测试结果，按优先级修复问题：

### P0 - 高优先级（必须修复）
1. ✅ 验证 MeshRenderSystem 基础功能
2. ✅ 验证渲染队列提交
3. ✅ 验证对象池无内存泄漏

### P1 - 中优先级（重要改进）
4. ⚠️ 修复视锥体裁剪（性能优化）
5. ⚠️ 实现材质排序（性能优化）

### P2 - 低优先级（可选改进）
6. ⚠️ 集成光源系统（功能完善）
7. ⚠️ 实现 SpriteRenderable（2D 支持）

---

## ✅ 测试通过标准

### 基础功能
- [x] MeshRenderable 能正确创建
- [x] 所有属性正确传递（mesh、material、transform、阴影等）
- [x] 渲染队列正确提交
- [x] 可见性控制生效
- [x] 资源加载检查生效

### 性能要求
- [x] 1000 实体 < 5ms per frame
- [x] 对象池复用，无频繁内存分配
- [x] 无内存泄漏
- [x] 统计信息准确

### 已知问题
- [x] 识别并记录视锥体裁剪被禁用的问题
- [x] 识别并记录材质排序未实现的问题
- [x] 识别并记录光源未自动设置的问题

---

## 📊 测试报告模板

### 测试结果记录

| 测试用例 | 状态 | 结果 | 备注 |
|---------|------|------|------|
| 测试 1 | ⏳ | - | 基础 MeshRenderable 创建 |
| 测试 2 | ⏳ | - | 多实体渲染 |
| 测试 3 | ⏳ | - | 可见性控制 |
| 测试 4 | ⏳ | - | 资源加载检查 |
| 测试 5 | ⏳ | - | 层级排序 |
| 测试 6 | ⏳ | - | 阴影属性 |
| 测试 7 | ⏳ | - | Transform 复用 |
| 测试 8 | ⏳ | - | 包围盒计算 |
| 测试 9 | ⏳ | - | 渲染统计 |
| 测试 10 | ⏳ | - | 性能测试 |

### 问题记录

| 问题 ID | 优先级 | 描述 | 状态 | 修复方案 |
|---------|-------|------|------|---------|
| 1 | P1 | 视锥体裁剪被禁用 | 已识别 | PostInitialize 缓存 CameraSystem |
| 2 | P1 | 材质排序未实现 | 已识别 | 实现 SortRenderQueue 材质部分 |
| 3 | P2 | 光源未自动设置 | 已识别 | LightSystem 集成到 Renderer |

---

## 🎯 成功标准

当以下条件全部满足时，渲染核心 ECS 集成测试通过：

1. ✅ 所有 P0 测试用例通过
2. ✅ 性能要求达标（1000 实体 < 5ms）
3. ✅ 无崩溃、无内存泄漏
4. ✅ 日志输出正确（统计信息准确）
5. ✅ 已知问题已记录并有修复方案

**备注**：P1 和 P2 问题不影响测试通过，但需要记录并规划后续修复。

---

[返回 PHASE1 文档](PHASE1_BASIC_RENDERING.md) | [查看 ECS API 文档](../api/ECS.md)

