# 35测试 LOD 渲染问题调试指南

## 问题现状
- **51测试**：修复着色器location后恢复正常
- **35测试**：修复后仍看不到渲染对象
- **日志**：`LOD0=1250, LOD1=1250, Culled=0`，距离正常，无剔除

## 可能的原因分析

### 1. 着色器加载失败

35测试的着色器加载逻辑：
```cpp
// 优先加载 material_phong.vert
auto phongShader = shaderCache.LoadShader("material_phong", 
    "shaders/material_phong.vert", "shaders/material_phong.frag");

if (!phongShader) {
    // 回退到 basic.vert
    phongShader = shaderCache.LoadShader("basic", 
        "shaders/basic.vert", "shaders/basic.frag");
}
```

**检查点**：
- 查看日志，确认实际加载的是哪个着色器
- 如果是`material_phong.vert`（location 6-9） → 应该正常
- 如果是`basic.vert`（location 4-7） → 需要修复才能正常

### 2. LODComponent 未配置 lodMeshes

**现象**：
- 35测试添加了`LODComponent`，但`lodMeshes`为空
- `GetLODMesh(lodLevel, defaultMesh)`会返回`defaultMesh`（原始网格）
- 理论上应该能正常渲染，只是所有LOD级别使用同一个网格

**可能的问题**：
如果`meshComp.mesh`本身是`nullptr`或无效：
```cpp
// src/ecs/systems.cpp:1689
renderMesh = lodComp.config.GetLODMesh(lodLevel, meshComp.mesh);

// 如果 meshComp.mesh 是 nullptr，GetLODMesh 返回 nullptr
// 然后第1701行检查会跳过：
if (!renderMesh || !renderMaterial) {
    continue;  // 跳过渲染
}
```

**验证方法**：
添加日志检查`meshComp.mesh`是否有效：
```cpp
if (!meshComp.mesh) {
    Logger::GetInstance().WarningFormat(
        "[MeshRenderSystem] Entity %u has null mesh!", entity.index);
}
```

### 3. 材质问题

检查材质是否有效：
```cpp
// src/ecs/systems.cpp:1692
renderMaterial = lodComp.config.GetLODMaterial(lodLevel, meshComp.material);

if (!renderMesh || !renderMaterial) {
    continue;
}
```

如果`meshComp.material`是`nullptr`，会跳过渲染。

### 4. 实例化VAO创建失败

即使着色器location修复了，VAO创建仍可能失败：
- 着色器查询location失败（返回-1）
- 回退到硬编码location 6-9
- 但如果着色器是`basic.vert`（期望4-7），仍会不匹配

**检查点**：
在`GetOrCreateInstancedVAO`中添加日志：
```cpp
Logger::GetInstance().InfoFormat(
    "[LODInstancedRenderer] Queried locations: [%d, %d, %d, %d]",
    instanceMatrixLocations[0], instanceMatrixLocations[1], 
    instanceMatrixLocations[2], instanceMatrixLocations[3]);
```

### 5. 可见性和渲染状态

检查其他可能影响渲染的因素：
```cpp
// 可见性
meshComp.visible = true;

// 着色器uniform
uniformMgr->SetBool("uHasInstanceData", true);

// 渲染状态
renderState->SetDepthTest(true);
renderState->SetCullFace(CullFace::Back);
```

## 调试步骤

### 步骤1：确认着色器加载

在35测试中添加日志：
```cpp
if (phongShader) {
    Logger::GetInstance().InfoFormat("✓ 着色器加载成功: %s", 
        phongShader->GetName().c_str());
}
```

### 步骤2：检查mesh和material有效性

在`MeshRenderSystem::Update`的LOD实例化路径中添加：
```cpp
if (!meshComp.mesh) {
    Logger::GetInstance().WarningFormat(
        "[Entity %u] Mesh is null!", entity.index);
    continue;
}
if (!meshComp.material) {
    Logger::GetInstance().WarningFormat(
        "[Entity %u] Material is null!", entity.index);
    continue;
}
```

### 步骤3：检查LOD网格获取

```cpp
renderMesh = lodComp.config.GetLODMesh(lodLevel, meshComp.mesh);
if (!renderMesh) {
    Logger::GetInstance().WarningFormat(
        "[Entity %u] GetLODMesh returned nullptr! LOD=%d", 
        entity.index, static_cast<int>(lodLevel));
}
```

### 步骤4：检查实例化VAO创建

在`GetOrCreateInstancedVAO`开始处添加：
```cpp
Logger::GetInstance().InfoFormat(
    "[LODInstancedRenderer] Creating VAO for mesh=%p, material=%p",
    mesh.get(), material.get());

// 查询location后：
Logger::GetInstance().InfoFormat(
    "[LODInstancedRenderer] Instance matrix locations: [%d, %d, %d, %d]",
    instanceMatrixLocations[0], instanceMatrixLocations[1], 
    instanceMatrixLocations[2], instanceMatrixLocations[3]);
```

### 步骤5：检查渲染统计

查看日志中的渲染统计：
```
[MeshRenderSystem] LOD Instanced: X groups, Y instances, Z draw calls
```
- 如果`groups=0`：没有组被创建，可能是mesh/material问题
- 如果`instances=0`：没有实例被添加，可能是裁剪或跳过问题
- 如果`draw calls=0`：组被创建但没有渲染，可能是VAO或着色器问题

## 与58测试的对比

| 特性 | 35测试 | 58测试 | 51测试 |
|------|--------|--------|--------|
| 组件 | MeshRenderComponent | ModelComponent | MeshRenderComponent |
| 系统 | MeshRenderSystem | ModelRenderSystem | MeshRenderSystem |
| 着色器 | material_phong或basic | material_phong | basic |
| LODComponent | ✅ 有（但未配置lodMeshes） | ❌ 无 | ✅ 有（但未配置lodMeshes） |
| 修复后状态 | ❌ 仍不可见 | ✅ 正常 | ✅ 恢复正常 |

**关键差异**：
- 51测试修复后正常 → 说明`basic.vert`的location查询工作正常
- 35测试修复后仍不可见 → 可能不是location问题，而是其他原因

## 推荐的修复方案

### 方案1：临时禁用LOD实例化（快速验证）

保持第241行：
```cpp
renderer->SetLODInstancingEnabled(false);
```

如果禁用后能看到对象 → 确认是LOD实例化路径的问题

### 方案2：配置lodMeshes（如果需要真正的LOD）

使用`LODGenerator`生成LOD网格：
```cpp
// 为每个miku部分生成LOD网格
auto lodMeshes = LODGenerator::GenerateLODLevels(
    meshComp.mesh, 
    LODGenerator::GetRecommendedOptions(meshComp.mesh)
);

// 配置到LODComponent
lodComp.config.lodMeshes.resize(4);
lodComp.config.lodMeshes[0] = lodMeshes[0];  // LOD0
lodComp.config.lodMeshes[1] = lodMeshes[1];  // LOD1
lodComp.config.lodMeshes[2] = lodMeshes[2];  // LOD2
lodComp.config.lodMeshes[3] = lodMeshes[3];  // LOD3
```

### 方案3：移除LODComponent（简化测试）

如果不需要LOD功能，不添加`LODComponent`：
```cpp
// 注释掉第670-678行
// LODComponent lodComp;
// ...
// world->AddComponent(mikuPart, lodComp);
```

## 下一步行动

1. **添加调试日志**：按照上述步骤添加日志，找出具体失败点
2. **对比51和35**：两者都使用MeshRenderSystem，但51正常35不正常
3. **检查着色器**：确认35使用的是哪个着色器
4. **验证mesh有效性**：确认加载的mesh不是nullptr

