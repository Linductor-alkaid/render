# MaterialSortKey 扩展优化详解

[返回文档首页](README.md) | [返回优化进度](RENDERER_OPTIMIZATION_PROGRESS.md)

---

## 优化概述

本文档详细说明了 `MaterialSortKey` 扩展优化的实现细节、优化效果和使用场景。

**优化版本**: 1.0  
**完成日期**: 2025-12-02  
**相关文档**: [RENDERER_OPTIMIZATION_PLAN.md](RENDERER_OPTIMIZATION_PLAN.md)

---

## 优化背景

### 问题分析

在优化前，`MaterialSortKey` 结构包含以下字段：
- `materialID` - 材质 ID
- `shaderID` - 着色器 ID
- `blendMode` - 混合模式
- `cullFace` - 面剔除模式
- `depthTest` - 深度测试开关
- `depthWrite` - 深度写入开关
- `overrideHash` - 覆盖哈希
- `pipelineFlags` - 管线标志

**存在的问题**：
1. 缺少 `depthFunc`（深度测试函数）字段
2. 相同材质但不同深度函数的对象被错误地合并
3. 导致不必要的深度函数状态切换
4. 无法从 RenderLayer 状态覆写中获取深度函数信息

### 优化目标

1. 扩展 `MaterialSortKey` 包含 `depthFunc` 字段
2. 从 RenderLayer 状态覆写中获取 `depthFunc` 值
3. 提高状态合并的准确性
4. 减少深度函数状态切换

---

## 实现细节

### 1. 扩展 MaterialSortKey 结构

**文件**: `include/render/material_sort_key.h`

```cpp
struct MaterialSortKey {
    uint32_t materialID = 0;
    uint32_t shaderID = 0;
    BlendMode blendMode = BlendMode::None;
    CullFace cullFace = CullFace::Back;
    bool depthTest = true;
    bool depthWrite = true;
    DepthFunc depthFunc = DepthFunc::Less;  // ✅ 新增字段
    uint32_t overrideHash = 0;
    uint32_t pipelineFlags = 0;

    bool operator==(const MaterialSortKey& other) const noexcept;
    bool operator!=(const MaterialSortKey& other) const noexcept;
};
```

**关键点**：
- 添加 `depthFunc` 字段，默认值为 `DepthFunc::Less`
- 保持与其他状态字段一致的结构

### 2. 更新比较和哈希函数

**文件**: `src/rendering/material_sort_key.cpp`

#### 2.1 相等性比较

```cpp
bool MaterialSortKey::operator==(const MaterialSortKey& other) const noexcept {
    return materialID == other.materialID &&
           shaderID == other.shaderID &&
           blendMode == other.blendMode &&
           cullFace == other.cullFace &&
           depthTest == other.depthTest &&
           depthWrite == other.depthWrite &&
           depthFunc == other.depthFunc &&  // ✅ 新增比较
           overrideHash == other.overrideHash &&
           pipelineFlags == other.pipelineFlags;
}
```

#### 2.2 哈希函数

```cpp
std::size_t MaterialSortKeyHasher::operator()(const MaterialSortKey& key) const noexcept {
    const uint64_t parts[] = {
        (static_cast<uint64_t>(key.materialID) << 32) | key.shaderID,
        (static_cast<uint64_t>(key.overrideHash) << 32) | key.pipelineFlags,
        static_cast<uint64_t>(static_cast<uint32_t>(key.blendMode)) << 32 |
        static_cast<uint32_t>(key.cullFace),
        (static_cast<uint64_t>(key.depthTest) << 2) |
        (static_cast<uint64_t>(key.depthWrite) << 1) |
        static_cast<uint64_t>(static_cast<uint32_t>(key.depthFunc))  // ✅ 新增哈希
    };
    // ... FNV-1a 哈希计算
}
```

**关键点**：
- 将 `depthFunc` 与 `depthTest` 和 `depthWrite` 打包到同一个 64 位整数
- 使用位移操作避免冲突
- 保持 FNV-1a 哈希算法

#### 2.3 排序函数

```cpp
bool MaterialSortKeyLess::operator()(const MaterialSortKey& lhs, const MaterialSortKey& rhs) const noexcept {
    return std::tie(lhs.materialID,
                    lhs.shaderID,
                    lhs.blendMode,
                    lhs.cullFace,
                    lhs.depthTest,
                    lhs.depthWrite,
                    lhs.depthFunc,  // ✅ 新增排序键
                    lhs.overrideHash,
                    lhs.pipelineFlags) <
           std::tie(rhs.materialID,
                    rhs.shaderID,
                    rhs.blendMode,
                    rhs.cullFace,
                    rhs.depthTest,
                    rhs.depthWrite,
                    rhs.depthFunc,
                    rhs.overrideHash,
                    rhs.pipelineFlags);
}
```

### 3. 扩展 BuildMaterialSortKey 函数

**文件**: `src/rendering/material_sort_key.cpp`

```cpp
MaterialSortKey BuildMaterialSortKey(const Material* material,
                                     uint32_t overrideHash,
                                     uint32_t pipelineFlags,
                                     std::optional<DepthFunc> depthFuncOverride) noexcept {
    MaterialSortKey key{};

    if (!material) {
        key.overrideHash = overrideHash;
        key.pipelineFlags = pipelineFlags;
        key.depthFunc = depthFuncOverride.value_or(DepthFunc::Less);  // ✅ 使用覆盖值
        return key;
    }

    // ... 其他字段设置

    // 优先使用层级覆盖的 depthFunc，否则使用默认值
    key.depthFunc = depthFuncOverride.value_or(DepthFunc::Less);  // ✅ 使用覆盖值
    
    return key;
}
```

**关键点**：
- 添加 `depthFuncOverride` 参数（`std::optional<DepthFunc>`）
- 优先使用覆盖值，如果没有则使用默认值 `DepthFunc::Less`
- 与现有的材质属性获取逻辑保持一致

### 4. 更新渲染对象排序键构建函数

**文件**: `src/core/renderer.cpp`

所有 `Build*RenderableSortKey` 函数都添加了 `layerDepthFunc` 参数：

```cpp
MaterialSortKey BuildMeshRenderableSortKey(
    MeshRenderable* meshRenderable, 
    std::optional<DepthFunc> layerDepthFunc = std::nullopt) {
    // ...
    key = BuildMaterialSortKey(material.get(), overrideHash, pipelineFlags, layerDepthFunc);
    return key;
}

MaterialSortKey BuildModelRenderableSortKey(
    ModelRenderable* modelRenderable, 
    std::optional<DepthFunc> layerDepthFunc = std::nullopt);

MaterialSortKey BuildSpriteRenderableSortKey(
    SpriteRenderable* spriteRenderable, 
    std::optional<DepthFunc> layerDepthFunc = std::nullopt);

MaterialSortKey BuildTextRenderableSortKey(
    TextRenderable* textRenderable, 
    std::optional<DepthFunc> layerDepthFunc = std::nullopt);
```

### 5. 从 RenderLayer 获取 depthFunc

**文件**: `src/core/renderer.cpp` - `Renderer::SubmitRenderable`

```cpp
void Renderer::SubmitRenderable(Renderable* renderable) {
    // ... 获取层级描述符和状态

    // ✅ 获取层级状态中的 depthFunc 覆盖值（如果有）
    std::optional<DepthFunc> layerDepthFunc;
    if (stateOpt.has_value() && stateOpt->overrides.depthFunc.has_value()) {
        // 优先使用运行时状态覆盖
        layerDepthFunc = stateOpt->overrides.depthFunc;
    } else if (descriptor.defaultState.depthFunc.has_value()) {
        // 其次使用层级默认状态
        layerDepthFunc = descriptor.defaultState.depthFunc;
    }
    
    // ✅ 使用层级的 depthFunc 确保材质排序键
    EnsureMaterialSortKey(renderable, layerDepthFunc);

    // ... 提交到层级桶
}
```

**关键流程**：
1. 获取渲染对象所属的 RenderLayer
2. 从层级状态覆写中查找 `depthFunc`
3. 如果状态覆写没有，则从层级默认状态中获取
4. 传递给 `EnsureMaterialSortKey` 函数
5. 最终传递到 `BuildMaterialSortKey` 函数

---

## 优化效果

### 状态合并改进

**优化前**：
```
渲染对象 A: Material=Mat1, DepthFunc=Less
渲染对象 B: Material=Mat1, DepthFunc=LessEqual
```
这两个对象会被错误地合并，因为 `MaterialSortKey` 不包含 `depthFunc`。

**优化后**：
```
渲染对象 A: MaterialSortKey{..., depthFunc=Less}
渲染对象 B: MaterialSortKey{..., depthFunc=LessEqual}
```
这两个对象会被正确分组，避免状态切换。

### 性能提升

**理论收益**：
- ✅ 减少深度函数状态切换次数
- ✅ 提高状态合并准确性
- ✅ 降低 GPU 状态切换开销

**预期指标**：
- 状态切换减少：5-10%（取决于场景复杂度）
- Draw Call 减少：2-5%（仅针对使用不同深度函数的场景）

---

## 使用场景

### 1. 多层渲染

不同层级可能需要不同的深度测试函数：

```cpp
// World 层使用 Less
worldLayer.defaultState.depthFunc = DepthFunc::Less;

// UI 层使用 Always（总是绘制）
uiLayer.defaultState.depthFunc = DepthFunc::Always;

// Overlay 层使用 LessEqual（允许深度相等）
overlayLayer.defaultState.depthFunc = DepthFunc::LessEqual;
```

### 2. 特殊渲染效果

某些特殊效果需要不同的深度函数：

```cpp
// 透明物体使用 LessEqual
transparentLayer.defaultState.depthFunc = DepthFunc::LessEqual;

// 轮廓渲染使用 Greater
outlineLayer.defaultState.depthFunc = DepthFunc::Greater;

// 调试渲染使用 Always
debugLayer.defaultState.depthFunc = DepthFunc::Always;
```

### 3. 动态切换

运行时可以动态修改层级的深度函数：

```cpp
// 切换到特殊渲染模式
RenderStateOverrides overrides;
overrides.depthFunc = DepthFunc::Equal;
renderer->GetLayerRegistry().SetOverrides(layerId, overrides);
```

---

## 后续优化方向

### 1. 扩展支持更多状态

可以继续扩展 `MaterialSortKey` 支持更多状态：

```cpp
struct MaterialSortKey {
    // 现有字段...
    
    // 待扩展字段
    std::optional<uint32_t> viewportHash;    // 视口哈希
    std::optional<uint32_t> scissorHash;     // 裁剪区域哈希
    bool stencilTest = false;                // 模板测试
    uint32_t stencilRef = 0;                 // 模板参考值
    // ...
};
```

### 2. 视口和裁剪区域哈希

如果层级使用了不同的视口或裁剪区域，可以添加哈希支持：

```cpp
uint32_t ComputeViewportHash(const RenderLayerViewport& viewport) {
    uint32_t hash = 0;
    hash = HashCombine(hash, viewport.x);
    hash = HashCombine(hash, viewport.y);
    hash = HashCombine(hash, viewport.width);
    hash = HashCombine(hash, viewport.height);
    return hash;
}
```

### 3. 模板测试支持

添加模板测试状态：

```cpp
struct MaterialSortKey {
    // ...
    bool stencilTest = false;
    uint32_t stencilFunc = 0;    // GL_ALWAYS, GL_EQUAL, etc.
    uint32_t stencilRef = 0;
    uint32_t stencilMask = 0xFF;
};
```

---

## 测试验证

### 单元测试

建议添加以下测试：

```cpp
TEST(MaterialSortKeyTest, DepthFuncComparison) {
    MaterialSortKey key1, key2;
    key1.depthFunc = DepthFunc::Less;
    key2.depthFunc = DepthFunc::LessEqual;
    
    EXPECT_NE(key1, key2);  // 应该不相等
    EXPECT_NE(MaterialSortKeyHasher{}(key1), MaterialSortKeyHasher{}(key2));  // 哈希应该不同
}

TEST(MaterialSortKeyTest, LayerDepthFuncOverride) {
    // 测试从层级状态获取 depthFunc
    // ...
}
```

### 集成测试

在实际渲染场景中验证：

1. 创建两个使用相同材质但不同深度函数的对象
2. 提交到不同层级
3. 验证它们被正确分组
4. 确认状态切换次数减少

---

## 相关文档

- [RENDERER_OPTIMIZATION_PLAN.md](RENDERER_OPTIMIZATION_PLAN.md) - 优化方案详细设计
- [RENDERER_OPTIMIZATION_PROGRESS.md](RENDERER_OPTIMIZATION_PROGRESS.md) - 优化进度跟踪
- [MATERIAL_SORTING_ARCHITECTURE.md](MATERIAL_SORTING_ARCHITECTURE.md) - 材质排序架构
- [RENDERING_LAYERS.md](RENDERING_LAYERS.md) - 渲染层级系统

---

[返回文档首页](README.md) | [返回优化进度](RENDERER_OPTIMIZATION_PROGRESS.md)

