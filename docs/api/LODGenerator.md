# LODGenerator API 参考

[返回 API 首页](README.md)

---

## 概述

`LODGenerator` 类提供自动生成不同 LOD（Level of Detail）级别网格的功能，使用 `meshoptimizer` 库进行网格简化。支持单个网格、整个模型以及批量处理，并提供文件保存和加载功能。

**头文件**: `render/lod_generator.h`  
**命名空间**: `Render`

### ✨ 功能特性

- ✅ **自动网格简化**：使用 `meshoptimizer` 库自动生成 LOD1、LOD2、LOD3 级别的简化网格
- ✅ **多种简化模式**：支持目标三角形数量或目标误差两种简化模式
- ✅ **属性保留**：支持保留法线、纹理坐标、颜色等顶点属性
- ✅ **模型支持**：支持为整个 Model 及其所有部分生成 LOD 级别
- ✅ **批量处理**：支持批量生成多个网格的 LOD 级别
- ✅ **文件保存/加载**：支持将 LOD 网格保存为 OBJ 文件，并支持从文件加载
- ✅ **自动配置**：提供自动配置 LODConfig 的便捷方法
- ✅ **推荐配置**：根据源网格自动计算推荐的简化参数

---

## 类定义

```cpp
class LODGenerator {
public:
    // 所有方法都是静态方法
    static std::vector<Ref<Mesh>> GenerateLODLevels(...);
    static Ref<Mesh> GenerateLODLevel(...);
    // ... 其他方法
};
```

**说明**:
- `LODGenerator` 是一个工具类，所有方法都是静态方法
- 不需要创建实例，直接通过类名调用方法
- 所有方法都是线程安全的（每个方法独立处理数据）

---

## 数据结构

### SimplifyOptions

简化配置结构，定义网格简化的各种参数。

```cpp
struct SimplifyOptions {
    // 简化模式
    enum class Mode {
        TargetTriangleCount,  ///< 目标三角形数量（推荐）
        TargetError          ///< 目标误差（相对网格范围）
    };
    
    Mode mode = Mode::TargetTriangleCount;
    
    // 目标三角形数量（用于 TargetTriangleCount 模式）
    struct TriangleCounts {
        size_t lod1 = 0;  ///< LOD1 目标三角形数量（0 = 自动，默认 50%）
        size_t lod2 = 0;  ///< LOD2 目标三角形数量（0 = 自动，默认 25%）
        size_t lod3 = 0;  ///< LOD3 目标三角形数量（0 = 自动，默认 10%）
    } triangleCounts;
    
    // 目标误差（用于 TargetError 模式，范围 [0..1]）
    struct TargetErrors {
        float lod1 = 0.01f;  ///< LOD1 目标误差（1%）
        float lod2 = 0.03f;  ///< LOD2 目标误差（3%）
        float lod3 = 0.05f;  ///< LOD3 目标误差（5%）
    } targetErrors;
    
    // 简化选项标志
    enum SimplifyFlags {
        LockBorder = 1 << 0,      ///< 锁定边界顶点（不移动）
        Sparse = 1 << 1,          ///< 稀疏简化（更快但质量稍低）
        Regularize = 1 << 2,      ///< 正则化（更平滑）
        Permissive = 1 << 3       ///< 允许跨属性不连续边折叠
    };
    
    unsigned int flags = 0;  ///< 标志位组合
    
    // 属性权重（用于保留顶点属性）
    struct AttributeWeights {
        float normal = 1.0f;      ///< 法线权重
        float texCoord = 1.0f;    ///< 纹理坐标权重
        float color = 0.5f;       ///< 颜色权重（通常较低）
    } attributeWeights;
    
    bool recalculateNormals = true;    ///< 是否重新计算法线（简化后）
    bool recalculateTangents = false;  ///< 是否重新计算切线（简化后）
};
```

**字段说明**:

- **mode**: 简化模式
  - `TargetTriangleCount`: 指定目标三角形数量（推荐，更直观）
  - `TargetError`: 指定目标误差百分比（更精确，但需要调参）

- **triangleCounts**: 目标三角形数量
  - 如果某个级别为 `0`，表示自动计算（使用默认比例）
  - LOD1: 默认保留 50% 的三角形
  - LOD2: 默认保留 25% 的三角形
  - LOD3: 默认保留 10% 的三角形

- **targetErrors**: 目标误差（仅用于 `TargetError` 模式）
  - 范围 `[0..1]`，例如 `0.01` 表示 1% 的变形误差
  - 误差越小，保留的三角形越多

- **flags**: 简化选项标志
  - `LockBorder`: 锁定边界顶点，防止边界变形（适用于需要保持边界的模型）
  - `Sparse`: 稀疏简化，速度更快但质量稍低
  - `Regularize`: 正则化，使网格更平滑
  - `Permissive`: 允许跨属性不连续边折叠（可能产生更好的简化效果）

- **attributeWeights**: 属性权重
  - 控制简化时如何保留顶点属性
  - 权重越高，该属性在简化时越重要
  - 法线和纹理坐标通常权重较高，颜色可以较低

- **recalculateNormals**: 是否重新计算法线
  - 简化后网格的法线可能不准确，建议启用

- **recalculateTangents**: 是否重新计算切线
  - 如果使用法线贴图，建议启用

**使用示例**:

```cpp
LODGenerator::SimplifyOptions options;
options.mode = LODGenerator::SimplifyOptions::Mode::TargetTriangleCount;
options.triangleCounts.lod1 = 1000;  // LOD1 保留 1000 个三角形
options.triangleCounts.lod2 = 500;   // LOD2 保留 500 个三角形
options.triangleCounts.lod3 = 200;   // LOD3 保留 200 个三角形
options.flags = LODGenerator::SimplifyOptions::LockBorder;
options.attributeWeights.normal = 1.5f;  // 提高法线权重
```

---

## 核心方法

### GenerateLODLevels

生成单个网格的所有 LOD 级别。

```cpp
static std::vector<Ref<Mesh>> GenerateLODLevels(
    Ref<Mesh> sourceMesh,
    const SimplifyOptions& options = SimplifyOptions{}
);
```

**参数**:
- `sourceMesh`: 源网格（LOD0），不能为 `nullptr`
- `options`: 简化选项，使用默认值时会自动计算推荐参数

**返回值**:
- `std::vector<Ref<Mesh>>`: LOD 网格数组 `[LOD1, LOD2, LOD3]`
  - 如果某个级别简化失败，对应位置为 `nullptr`
  - 返回的网格已经调用 `Upload()`，可以直接使用

**说明**:
- 从源网格（LOD0）生成 LOD1、LOD2、LOD3 三个级别的简化网格
- 如果 `options.triangleCounts` 中某个级别为 `0`，会自动计算（使用默认比例）
- 简化过程会保留顶点属性（法线、纹理坐标、颜色等）

**使用示例**:

```cpp
Ref<Mesh> sourceMesh = LoadMesh("tree.obj");
auto lodMeshes = LODGenerator::GenerateLODLevels(sourceMesh);

if (lodMeshes[0] && lodMeshes[1] && lodMeshes[2]) {
    std::cout << "LOD1: " << lodMeshes[0]->GetTriangleCount() << " 三角形" << std::endl;
    std::cout << "LOD2: " << lodMeshes[1]->GetTriangleCount() << " 三角形" << std::endl;
    std::cout << "LOD3: " << lodMeshes[2]->GetTriangleCount() << " 三角形" << std::endl;
    
    // 可以直接使用这些网格
    meshComp.mesh = lodMeshes[0];  // 使用 LOD1
}
```

---

### GenerateLODLevel

生成单个 LOD 级别。

```cpp
static Ref<Mesh> GenerateLODLevel(
    Ref<Mesh> sourceMesh,
    int lodLevel,
    const SimplifyOptions& options = SimplifyOptions{}
);
```

**参数**:
- `sourceMesh`: 源网格，不能为 `nullptr`
- `lodLevel`: 目标 LOD 级别（1, 2, 或 3）
- `options`: 简化选项

**返回值**:
- `Ref<Mesh>`: 简化后的网格，失败返回 `nullptr`
- 返回的网格已经调用 `Upload()`，可以直接使用

**说明**:
- 只生成指定级别的 LOD 网格，比 `GenerateLODLevels` 更灵活
- 适用于只需要某个特定 LOD 级别的场景

**使用示例**:

```cpp
Ref<Mesh> sourceMesh = LoadMesh("tree.obj");
Ref<Mesh> lod1Mesh = LODGenerator::GenerateLODLevel(sourceMesh, 1);
if (lod1Mesh) {
    std::cout << "LOD1 生成成功: " << lod1Mesh->GetTriangleCount() << " 三角形" << std::endl;
}
```

---

### GenerateModelLODLevels

为整个 Model 生成 LOD 级别。

```cpp
static std::vector<Ref<Model>> GenerateModelLODLevels(
    Ref<Model> sourceModel,
    const SimplifyOptions& options = SimplifyOptions{}
);
```

**参数**:
- `sourceModel`: 源模型，不能为 `nullptr`
- `options`: 简化选项

**返回值**:
- `std::vector<Ref<Model>>`: LOD 模型数组 `[LOD0, LOD1, LOD2, LOD3]`
  - `LOD0` 是原始模型（不进行简化）
  - 如果某个部分的简化失败，该部分在对应 LOD 级别中会使用原始网格

**说明**:
- 为 Model 中的每个部分（ModelPart）生成对应的 LOD 级别网格
- 返回的每个 Model 包含所有部分，但每个部分都使用相同的 LOD 级别
- 适用于需要为整个模型生成 LOD 的场景

**使用示例**:

```cpp
Ref<Model> sourceModel = ModelLoader::LoadFromFile("miku.pmx", "miku").model;
auto lodModels = LODGenerator::GenerateModelLODLevels(sourceModel);

// lodModels[0] 是原始模型
// lodModels[1] 是所有部分都简化为 LOD1 的模型
// lodModels[2] 是所有部分都简化为 LOD2 的模型
// lodModels[3] 是所有部分都简化为 LOD3 的模型

// 使用 LOD1 模型
if (lodModels[1]) {
    modelComp.model = lodModels[1];
}
```

---

### GenerateModelPartLODLevels

为 Model 的单个部分生成 LOD 级别。

```cpp
static std::vector<Ref<Mesh>> GenerateModelPartLODLevels(
    Ref<Model> sourceModel,
    size_t partIndex,
    const SimplifyOptions& options = SimplifyOptions{}
);
```

**参数**:
- `sourceModel`: 源模型，不能为 `nullptr`
- `partIndex`: 部分索引（从 0 开始）
- `options`: 简化选项

**返回值**:
- `std::vector<Ref<Mesh>>`: LOD 网格数组 `[LOD0, LOD1, LOD2, LOD3]`
  - `LOD0` 是原始网格
  - 如果简化失败，对应位置为 `nullptr`

**说明**:
- 只生成指定部分的 LOD 级别
- 适用于需要为不同部分使用不同 LOD 级别的场景

**使用示例**:

```cpp
Ref<Model> sourceModel = ModelLoader::LoadFromFile("miku.pmx", "miku").model;
auto partLODs = LODGenerator::GenerateModelPartLODLevels(sourceModel, 0);

// partLODs[0] 是第0个部分的原始网格
// partLODs[1] 是第0个部分的 LOD1 网格
// partLODs[2] 是第0个部分的 LOD2 网格
// partLODs[3] 是第0个部分的 LOD3 网格
```

---

### BatchGenerateLODLevels

批量生成多个网格的 LOD 级别。

```cpp
static std::vector<std::vector<Ref<Mesh>>> BatchGenerateLODLevels(
    const std::vector<Ref<Mesh>>& sourceMeshes,
    const SimplifyOptions& options = SimplifyOptions{}
);
```

**参数**:
- `sourceMeshes`: 源网格数组
- `options`: 简化选项（所有网格使用相同的选项）

**返回值**:
- `std::vector<std::vector<Ref<Mesh>>>`: 二维数组
  - `result[i][j]` = 第 `i` 个网格的第 `j` 个 LOD 级别（`j=0=LOD1, j=1=LOD2, j=2=LOD3`）
  - 返回的网格已经调用 `Upload()`，可以直接使用

**说明**:
- 批量处理多个网格，适用于需要为多个对象生成 LOD 的场景
- 所有网格使用相同的简化选项

**使用示例**:

```cpp
std::vector<Ref<Mesh>> sourceMeshes = {
    LoadMesh("tree1.obj"),
    LoadMesh("tree2.obj"),
    LoadMesh("tree3.obj")
};

auto allLODs = LODGenerator::BatchGenerateLODLevels(sourceMeshes);

// allLODs[0][0] 是 tree1 的 LOD1 网格
// allLODs[1][1] 是 tree2 的 LOD2 网格
// allLODs[2][2] 是 tree3 的 LOD3 网格
```

---

### AutoConfigureLOD

自动配置 LODConfig。

```cpp
static bool AutoConfigureLOD(
    Ref<Mesh> sourceMesh,
    LODConfig& config,
    const SimplifyOptions& options = SimplifyOptions{}
);
```

**参数**:
- `sourceMesh`: 源网格，不能为 `nullptr`
- `config`: 要配置的 LODConfig（会被修改）
- `options`: 简化选项

**返回值**:
- `bool`: 是否成功

**说明**:
- 从源网格生成所有 LOD 级别并自动配置到 `LODConfig`
- 设置 `config.lodMeshes` 为生成的 LOD 网格
- 适用于快速集成 LOD 系统

**使用示例**:

```cpp
Ref<Mesh> sourceMesh = LoadMesh("tree.obj");
LODConfig config;
config.enabled = true;
config.distanceThresholds = {50.0f, 150.0f, 500.0f, 1000.0f};

if (LODGenerator::AutoConfigureLOD(sourceMesh, config)) {
    // config.lodMeshes 已经包含了 LOD1-LOD3 的网格
    LODComponent lodComp;
    lodComp.config = config;
    world->AddComponent<LODComponent>(entity, lodComp);
}
```

---

### GetRecommendedOptions

获取推荐的简化配置。

```cpp
static SimplifyOptions GetRecommendedOptions(Ref<Mesh> sourceMesh);
```

**参数**:
- `sourceMesh`: 源网格，不能为 `nullptr`

**返回值**:
- `SimplifyOptions`: 推荐的配置

**说明**:
- 根据源网格的三角形数量自动计算推荐的简化参数
- 返回的配置可以直接使用，也可以根据需要进行调整

**使用示例**:

```cpp
Ref<Mesh> sourceMesh = LoadMesh("tree.obj");
auto options = LODGenerator::GetRecommendedOptions(sourceMesh);

// 可以根据需要调整
options.flags |= LODGenerator::SimplifyOptions::LockBorder;
options.attributeWeights.normal = 1.5f;

auto lodMeshes = LODGenerator::GenerateLODLevels(sourceMesh, options);
```

---

### ValidateSimplifiedMesh

验证简化结果。

```cpp
static bool ValidateSimplifiedMesh(Ref<Mesh> simplifiedMesh, Ref<Mesh> sourceMesh);
```

**参数**:
- `simplifiedMesh`: 简化后的网格
- `sourceMesh`: 源网格（用于对比）

**返回值**:
- `bool`: 是否有效

**说明**:
- 检查简化后的网格是否有效（顶点数、索引数、拓扑等）
- 验证索引是否在有效范围内
- 适用于调试和验证简化结果

**使用示例**:

```cpp
Ref<Mesh> sourceMesh = LoadMesh("tree.obj");
auto lodMeshes = LODGenerator::GenerateLODLevels(sourceMesh);

for (size_t i = 0; i < lodMeshes.size(); ++i) {
    if (lodMeshes[i]) {
        if (LODGenerator::ValidateSimplifiedMesh(lodMeshes[i], sourceMesh)) {
            std::cout << "LOD" << (i+1) << " 验证通过" << std::endl;
        } else {
            std::cout << "LOD" << (i+1) << " 验证失败" << std::endl;
        }
    }
}
```

---

## 文件操作

### SaveMeshToOBJ

保存网格到 OBJ 文件。

```cpp
static bool SaveMeshToOBJ(Ref<Mesh> mesh, const std::string& filepath);
```

**参数**:
- `mesh`: 要保存的网格，不能为 `nullptr`
- `filepath`: 输出文件路径

**返回值**:
- `bool`: 是否成功

**说明**:
- 将网格数据导出为 OBJ 格式文件
- 包含顶点位置、纹理坐标、法线
- 文件使用 UTF-8 编码（带 BOM）
- 自动创建目录（如果不存在）

**使用示例**:

```cpp
Ref<Mesh> lodMesh = LODGenerator::GenerateLODLevel(sourceMesh, 1);
if (lodMesh) {
    LODGenerator::SaveMeshToOBJ(lodMesh, "model_lod1.obj");
}
```

---

### SaveLODMeshesToFiles

批量保存 LOD 网格到文件。

```cpp
static bool SaveLODMeshesToFiles(
    Ref<Mesh> sourceMesh,
    const std::vector<Ref<Mesh>>& lodMeshes,
    const std::string& baseFilepath
);
```

**参数**:
- `sourceMesh`: 源网格（可选，用于保存 LOD0）
- `lodMeshes`: LOD 网格数组 `[LOD1, LOD2, LOD3]`
- `baseFilepath`: 基础文件路径（不含扩展名），例如 `"models/miku"`

**返回值**:
- `bool`: 是否全部成功

**说明**:
- 将生成的 LOD 级别网格保存为文件，文件名会自动添加 LOD 后缀
- 文件命名规则：`baseFilepath_lod0.obj`, `baseFilepath_lod1.obj`, ...
- 自动清理文件名中的非 ASCII 字符（避免编码问题）

**使用示例**:

```cpp
Ref<Mesh> sourceMesh = LoadMesh("miku.obj");
auto lodMeshes = LODGenerator::GenerateLODLevels(sourceMesh);

// 保存所有 LOD 级别
LODGenerator::SaveLODMeshesToFiles(sourceMesh, lodMeshes, "output/miku");
// 会生成: miku_lod0.obj, miku_lod1.obj, miku_lod2.obj, miku_lod3.obj
```

---

### SaveModelLODToFiles

保存 Model 的所有 LOD 级别到文件。

```cpp
static bool SaveModelLODToFiles(
    Ref<Model> sourceModel,
    const std::string& baseFilepath,
    const SimplifyOptions& options = SimplifyOptions{}
);
```

**参数**:
- `sourceModel`: 源模型，不能为 `nullptr`
- `baseFilepath`: 基础文件路径（不含扩展名），例如 `"output/miku"`
- `options`: 简化选项

**返回值**:
- `bool`: 是否全部成功

**文件命名规则**:
- 单部分模型: `miku_lod0.obj`, `miku_lod1.obj`, ...
- 多部分模型: `miku_part0_lod0.obj`, `miku_part0_lod1.obj`, `miku_part1_lod0.obj`, ...

**说明**:
- 为 Model 的每个部分生成 LOD 级别并保存为独立的 OBJ 文件
- 自动清理文件名中的非 ASCII 字符（避免编码问题）
- 使用部分索引确保文件名唯一性（即使部分名称清理后相同也不会冲突）

**使用示例**:

```cpp
Ref<Model> model = ModelLoader::LoadFromFile("miku.pmx", "miku").model;

// 保存所有部分的所有 LOD 级别
LODGenerator::SaveModelLODToFiles(model, "output/miku");
// 会生成: miku_part0_lod0.obj, miku_part0_lod1.obj, ...
```

---

### LoadPartLODMesh

加载指定部分的 LOD 网格。

```cpp
static Ref<Mesh> LoadPartLODMesh(
    const std::string& baseFilepath,
    size_t partIndex,
    int lodLevel,
    size_t totalParts
);
```

**参数**:
- `baseFilepath`: 基础文件路径（与 `SaveModelLODToFiles` 使用的路径相同）
- `partIndex`: 部分索引（从 0 开始）
- `lodLevel`: LOD 级别（0-3）
- `totalParts`: 模型总部分数（用于判断是单部分还是多部分模型）

**返回值**:
- `Ref<Mesh>`: 加载的网格，失败返回 `nullptr`

**说明**:
- 根据部分索引和 LOD 级别加载对应的网格文件
- 确保与原模型部分一一对应（使用部分索引而非名称）
- 自动处理单部分/多部分模型的路径差异

**使用示例**:

```cpp
// 加载第0个部分的LOD1网格
Ref<Mesh> lod1Mesh = LODGenerator::LoadPartLODMesh("output/miku", 0, 1, model->GetPartCount());
if (lod1Mesh) {
    meshComp.mesh = lod1Mesh;
}
```

---

### LoadModelLODMeshes

加载模型的所有 LOD 级别。

```cpp
static std::vector<std::vector<Ref<Mesh>>> LoadModelLODMeshes(
    Ref<Model> sourceModel,
    const std::string& baseFilepath
);
```

**参数**:
- `sourceModel`: 原始模型（用于获取部分信息）
- `baseFilepath`: 基础文件路径（与 `SaveModelLODToFiles` 使用的路径相同）

**返回值**:
- `std::vector<std::vector<Ref<Mesh>>>`: 二维数组 `[partIndex][lodLevel]`
  - `result[0][1]` 是第 0 个部分的 LOD1 网格
  - `result[1][2]` 是第 1 个部分的 LOD2 网格
  - 失败的部分为 `nullptr`

**说明**:
- 为模型的每个部分加载所有 LOD 级别的网格
- 自动处理文件路径和命名规则
- 适用于从文件加载预生成的 LOD 网格

**使用示例**:

```cpp
Ref<Model> model = ModelLoader::LoadFromFile("miku.pmx", "miku").model;

// 加载所有部分的所有 LOD 级别
auto lodMeshes = LODGenerator::LoadModelLODMeshes(model, "output/miku");

// lodMeshes[0][1] 是第0个部分的LOD1网格
// lodMeshes[1][2] 是第1个部分的LOD2网格

if (lodMeshes[0][1]) {
    // 使用第0个部分的LOD1网格
    meshComp.mesh = lodMeshes[0][1];
}
```

---

## 完整使用示例

### 示例 1: 基本使用

```cpp
#include <render/lod_generator.h>
#include <render/mesh_loader.h>

// 加载源网格
Ref<Mesh> sourceMesh = MeshLoader::LoadFromFile("tree.obj")[0];

// 生成 LOD 级别（使用默认配置）
auto lodMeshes = LODGenerator::GenerateLODLevels(sourceMesh);

// 使用 LOD 网格
if (lodMeshes[0]) {
    meshComp.mesh = lodMeshes[0];  // 使用 LOD1
}
```

### 示例 2: 自定义配置

```cpp
// 创建自定义配置
LODGenerator::SimplifyOptions options;
options.mode = LODGenerator::SimplifyOptions::Mode::TargetTriangleCount;
options.triangleCounts.lod1 = 1000;
options.triangleCounts.lod2 = 500;
options.triangleCounts.lod3 = 200;
options.flags = LODGenerator::SimplifyOptions::LockBorder;
options.attributeWeights.normal = 1.5f;

// 生成 LOD
auto lodMeshes = LODGenerator::GenerateLODLevels(sourceMesh, options);
```

### 示例 3: 为 Model 生成 LOD

```cpp
#include <render/model_loader.h>

// 加载模型
Ref<Model> model = ModelLoader::LoadFromFile("miku.pmx", "miku").model;

// 生成所有 LOD 级别
auto lodModels = LODGenerator::GenerateModelLODLevels(model);

// 使用 LOD1 模型
if (lodModels[1]) {
    modelComp.model = lodModels[1];
}
```

### 示例 4: 保存和加载

```cpp
// 生成并保存 LOD
Ref<Model> model = ModelLoader::LoadFromFile("miku.pmx", "miku").model;
LODGenerator::SaveModelLODToFiles(model, "output/miku");

// 稍后加载
auto lodMeshes = LODGenerator::LoadModelLODMeshes(model, "output/miku");
if (lodMeshes[0][1]) {
    meshComp.mesh = lodMeshes[0][1];  // 使用第0个部分的LOD1网格
}
```

### 示例 5: 集成到 ECS

```cpp
#include <render/lod_system.h>
#include <render/ecs/components.h>

// 生成 LOD 并自动配置
Ref<Mesh> sourceMesh = LoadMesh("tree.obj");
LODConfig config;
config.enabled = true;
config.distanceThresholds = {50.0f, 150.0f, 500.0f, 1000.0f};

if (LODGenerator::AutoConfigureLOD(sourceMesh, config)) {
    // 添加到实体
    LODComponent lodComp;
    lodComp.config = config;
    world->AddComponent<LODComponent>(entity, lodComp);
}
```

---

## 注意事项

### 性能考虑

1. **简化耗时**: 网格简化是 CPU 密集型操作，对于大型网格可能需要较长时间
   - 建议在加载时或后台线程中生成 LOD
   - 可以预先生成并保存到文件，运行时直接加载

2. **内存使用**: 生成的 LOD 网格会占用内存
   - 考虑在不需要时释放 LOD 网格
   - 对于大量对象，考虑使用文件加载而非内存缓存

3. **批量处理**: 使用 `BatchGenerateLODLevels` 可以批量处理多个网格
   - 适用于需要为多个相同类型的对象生成 LOD 的场景

### 质量考虑

1. **简化比例**: 建议的简化比例
   - LOD1: 保留 50-70% 的三角形
   - LOD2: 保留 20-40% 的三角形
   - LOD3: 保留 10-20% 的三角形
   - 过度简化可能导致视觉质量下降

2. **边界锁定**: 对于需要保持边界的模型（如地形、建筑），建议使用 `LockBorder` 标志
   - 防止边界顶点移动，保持模型轮廓

3. **属性权重**: 根据模型特点调整属性权重
   - 法线权重：影响光照效果，建议保持较高（1.0-1.5）
   - 纹理坐标权重：影响纹理映射，建议保持较高（1.0）
   - 颜色权重：通常可以较低（0.5），除非颜色信息很重要

### 文件命名

1. **编码问题**: 文件名中的非 ASCII 字符（如中文）会被自动清理
   - 使用部分索引确保文件名唯一性
   - 即使多个部分名称清理后相同也不会冲突

2. **文件路径**: 使用相对路径或绝对路径都可以
   - 目录不存在时会自动创建
   - 建议使用相对路径以便于部署

### 错误处理

1. **空指针检查**: 所有方法都要求输入参数不为 `nullptr`
   - 如果输入为 `nullptr`，方法会记录错误并返回失败

2. **简化失败**: 如果简化失败，对应位置为 `nullptr`
   - 建议检查返回值，确保所有 LOD 级别都生成成功
   - 可以使用 `ValidateSimplifiedMesh` 验证结果

3. **文件操作**: 文件保存/加载可能失败
   - 检查返回值，确保操作成功
   - 检查文件路径是否正确，权限是否足够

---

## 相关 API

- [LOD 系统 API](LOD.md) - LOD 系统的使用和配置
- [Mesh API](Mesh.md) - 网格数据结构和方法
- [Model API](Model.md) - 模型数据结构和方法
- [MeshLoader API](MeshLoader.md) - 网格加载器

---

## 版本历史

- **v1.0** (2024-11-28): 初始版本
  - 支持单个网格和整个模型的 LOD 生成
  - 支持文件保存和加载
  - 支持批量处理
  - 支持自动配置 LODConfig

