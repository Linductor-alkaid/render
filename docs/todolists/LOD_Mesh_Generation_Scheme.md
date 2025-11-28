# LOD 网格自动生成方案

## 📋 文档信息

| 项目 | 内容 |
|------|------|
| **文档版本** | v1.0 |
| **创建日期** | 2025-11-28 |
| **方案目标** | 设计并实现基于 meshoptimizer 的自动 LOD 网格生成系统 |
| **优先级** | P1 (功能增强) |
| **依赖** | LOD 系统、Mesh 系统、meshoptimizer 库 |

---

## 🎯 方案概述

### 核心目标

1. **自动生成 LOD 网格**：从原始高精度网格自动生成 LOD1、LOD2、LOD3 级别的简化网格
2. **无缝集成**：与现有 LOD 系统无缝集成，支持自动配置 LODConfig
3. **性能优化**：使用 meshoptimizer 库进行高效的网格简化
4. **易用性**：提供简洁的 API，支持一键生成所有 LOD 级别

### 设计原则

- **渐进式简化**：从 LOD0 逐步简化到 LOD3，保证视觉连续性
- **质量可控**：支持基于目标三角形数量或目标误差的简化
- **属性保留**：尽可能保留顶点属性（法线、纹理坐标、颜色等）
- **线程安全**：与现有 Mesh 类的线程安全机制兼容

---

## 📊 当前状态分析

### 已有功能

#### LOD 系统
- ✅ **LODConfig 结构**：支持 `lodMeshes` 数组存储不同级别的网格
- ✅ **LOD 级别枚举**：LOD0-LOD3 四个级别
- ✅ **距离驱动选择**：基于相机距离自动选择 LOD 级别

#### Mesh 系统
- ✅ **Mesh 类**：支持顶点和索引数据管理
- ✅ **Vertex 结构**：包含 position、texCoord、normal、color、tangent、bitangent
- ✅ **数据访问**：提供 `AccessVertices()` 和 `AccessIndices()` 方法

#### meshoptimizer 库
- ✅ **位置**：已放置在 `third_party/meshoptimizer/` 目录
- ✅ **简化 API**：`meshopt_simplify()` 和 `meshopt_simplifyWithAttributes()`
- ✅ **功能**：支持基于目标索引数量或目标误差的简化

### 缺失功能

- ❌ **LOD 生成工具**：没有自动生成 LOD 网格的工具类
- ❌ **简化配置**：没有统一的简化参数配置
- ❌ **属性处理**：没有处理顶点属性（法线、UV等）的简化流程
- ❌ **批量生成**：没有批量生成多个网格的 LOD 级别

---

## 🔧 技术方案

### 1. 核心 API 设计

#### 1.1 LODGenerator 类

```cpp
// include/render/lod_generator.h

namespace Render {

/**
 * @brief LOD 网格生成器
 * 
 * 使用 meshoptimizer 库自动生成不同 LOD 级别的网格
 */
class LODGenerator {
public:
    /**
     * @brief LOD 简化配置
     */
    struct SimplifyOptions {
        // 简化模式
        enum class Mode {
            TargetTriangleCount,  // 目标三角形数量（推荐）
            TargetError          // 目标误差（相对网格范围）
        };
        
        Mode mode = Mode::TargetTriangleCount;
        
        // 目标三角形数量（用于 TargetTriangleCount 模式）
        // LOD1: 通常保留 50-70% 的三角形
        // LOD2: 通常保留 20-40% 的三角形
        // LOD3: 通常保留 10-20% 的三角形
        struct TriangleCounts {
            size_t lod1 = 0;  // 0 表示自动计算（原始数量的 50%）
            size_t lod2 = 0;  // 0 表示自动计算（原始数量的 25%）
            size_t lod3 = 0;  // 0 表示自动计算（原始数量的 10%）
        } triangleCounts;
        
        // 目标误差（用于 TargetError 模式，范围 [0..1]）
        // 例如：0.01 = 1% 变形误差
        struct TargetErrors {
            float lod1 = 0.01f;  // 1% 误差
            float lod2 = 0.03f;  // 3% 误差
            float lod3 = 0.05f;  // 5% 误差
        } targetErrors;
        
        // 简化选项标志
        enum SimplifyFlags {
            LockBorder = 1 << 0,      // 锁定边界顶点（不移动）
            Sparse = 1 << 1,          // 稀疏简化（更快但质量稍低）
            Regularize = 1 << 2,      // 正则化（更平滑）
            Permissive = 1 << 3       // 允许跨属性不连续边折叠
        };
        unsigned int flags = 0;
        
        // 属性权重（用于保留顶点属性）
        struct AttributeWeights {
            float normal = 1.0f;      // 法线权重
            float texCoord = 1.0f;    // 纹理坐标权重
            float color = 0.5f;       // 颜色权重（通常较低）
        } attributeWeights;
        
        // 是否重新计算法线（简化后）
        bool recalculateNormals = true;
        
        // 是否重新计算切线（简化后）
        bool recalculateTangents = false;
    };
    
    /**
     * @brief 生成单个网格的 LOD 级别
     * 
     * @param sourceMesh 源网格（LOD0）
     * @param options 简化选项
     * @return std::vector<Ref<Mesh>> LOD 网格数组 [LOD1, LOD2, LOD3]
     * 
     * @note 如果某个级别简化失败，对应位置为 nullptr
     */
    static std::vector<Ref<Mesh>> GenerateLODLevels(
        Ref<Mesh> sourceMesh,
        const SimplifyOptions& options = SimplifyOptions{}
    );
    
    /**
     * @brief 生成单个 LOD 级别
     * 
     * @param sourceMesh 源网格
     * @param lodLevel 目标 LOD 级别（1, 2, 或 3）
     * @param options 简化选项
     * @return Ref<Mesh> 简化后的网格，失败返回 nullptr
     */
    static Ref<Mesh> GenerateLODLevel(
        Ref<Mesh> sourceMesh,
        int lodLevel,
        const SimplifyOptions& options = SimplifyOptions{}
    );
    
    /**
     * @brief 自动配置 LODConfig
     * 
     * 从源网格生成所有 LOD 级别并自动配置到 LODConfig
     * 
     * @param sourceMesh 源网格
     * @param config 要配置的 LODConfig（会被修改）
     * @param options 简化选项
     * @return bool 是否成功
     */
    static bool AutoConfigureLOD(
        Ref<Mesh> sourceMesh,
        LODConfig& config,
        const SimplifyOptions& options = SimplifyOptions{}
    );
    
    /**
     * @brief 批量生成多个网格的 LOD 级别
     * 
     * @param sourceMeshes 源网格数组
     * @param options 简化选项
     * @return std::vector<std::vector<Ref<Mesh>>> 每个网格的 LOD 级别数组
     */
    static std::vector<std::vector<Ref<Mesh>>> BatchGenerateLODLevels(
        const std::vector<Ref<Mesh>>& sourceMeshes,
        const SimplifyOptions& options = SimplifyOptions{}
    );
    
    /**
     * @brief 获取推荐的简化配置
     * 
     * 根据源网格的三角形数量自动计算推荐的简化参数
     * 
     * @param sourceMesh 源网格
     * @return SimplifyOptions 推荐的配置
     */
    static SimplifyOptions GetRecommendedOptions(Ref<Mesh> sourceMesh);
    
    /**
     * @brief 验证简化结果
     * 
     * 检查简化后的网格是否有效（顶点数、索引数、拓扑等）
     * 
     * @param simplifiedMesh 简化后的网格
     * @param sourceMesh 源网格（用于对比）
     * @return bool 是否有效
     */
    static bool ValidateSimplifiedMesh(Ref<Mesh> simplifiedMesh, Ref<Mesh> sourceMesh);
};

} // namespace Render
```

#### 1.2 使用示例

```cpp
#include "render/lod_generator.h"
#include "render/lod_system.h"

using namespace Render;

// 示例 1: 基本使用 - 自动生成所有 LOD 级别
void Example1_BasicUsage() {
    // 加载原始网格
    Ref<Mesh> sourceMesh = LoadMesh("tree.obj");
    
    // 使用默认配置生成 LOD
    auto lodMeshes = LODGenerator::GenerateLODLevels(sourceMesh);
    
    // lodMeshes[0] = LOD1, lodMeshes[1] = LOD2, lodMeshes[2] = LOD3
    if (lodMeshes[0] && lodMeshes[1] && lodMeshes[2]) {
        std::cout << "LOD 生成成功！" << std::endl;
        std::cout << "LOD0: " << sourceMesh->GetTriangleCount() << " 三角形" << std::endl;
        std::cout << "LOD1: " << lodMeshes[0]->GetTriangleCount() << " 三角形" << std::endl;
        std::cout << "LOD2: " << lodMeshes[1]->GetTriangleCount() << " 三角形" << std::endl;
        std::cout << "LOD3: " << lodMeshes[2]->GetTriangleCount() << " 三角形" << std::endl;
    }
}

// 示例 2: 自定义配置
void Example2_CustomOptions() {
    Ref<Mesh> sourceMesh = LoadMesh("tree.obj");
    
    LODGenerator::SimplifyOptions options;
    options.mode = LODGenerator::SimplifyOptions::Mode::TargetTriangleCount;
    
    // 手动指定三角形数量
    size_t originalTriangles = sourceMesh->GetTriangleCount();
    options.triangleCounts.lod1 = originalTriangles * 0.5;  // 50%
    options.triangleCounts.lod2 = originalTriangles * 0.25;  // 25%
    options.triangleCounts.lod3 = originalTriangles * 0.1;  // 10%
    
    // 锁定边界顶点（保持网格边界不变）
    options.flags |= LODGenerator::SimplifyOptions::LockBorder;
    
    // 重新计算法线
    options.recalculateNormals = true;
    
    auto lodMeshes = LODGenerator::GenerateLODLevels(sourceMesh, options);
}

// 示例 3: 使用目标误差模式
void Example3_TargetError() {
    Ref<Mesh> sourceMesh = LoadMesh("tree.obj");
    
    LODGenerator::SimplifyOptions options;
    options.mode = LODGenerator::SimplifyOptions::Mode::TargetError;
    
    // 设置目标误差（相对网格范围）
    options.targetErrors.lod1 = 0.01f;  // 1% 误差
    options.targetErrors.lod2 = 0.03f;  // 3% 误差
    options.targetErrors.lod3 = 0.05f;  // 5% 误差
    
    auto lodMeshes = LODGenerator::GenerateLODLevels(sourceMesh, options);
}

// 示例 4: 自动配置 LODConfig
void Example4_AutoConfigure() {
    Ref<Mesh> sourceMesh = LoadMesh("tree.obj");
    
    // 创建 LOD 组件
    ECS::LODComponent lodComp;
    lodComp.config.enabled = true;
    lodComp.config.distanceThresholds = {50.0f, 150.0f, 500.0f, 1000.0f};
    
    // 自动生成并配置 LOD 网格
    if (LODGenerator::AutoConfigureLOD(sourceMesh, lodComp.config)) {
        std::cout << "LOD 自动配置成功！" << std::endl;
        
        // 现在 lodComp.config.lodMeshes 已经包含了 LOD1-LOD3 的网格
        // 可以直接使用
        world->AddComponent<ECS::LODComponent>(entity, lodComp);
    }
}

// 示例 5: 批量生成
void Example5_BatchGenerate() {
    std::vector<Ref<Mesh>> sourceMeshes = {
        LoadMesh("tree1.obj"),
        LoadMesh("tree2.obj"),
        LoadMesh("tree3.obj")
    };
    
    // 批量生成所有网格的 LOD 级别
    auto allLODs = LODGenerator::BatchGenerateLODLevels(sourceMeshes);
    
    // allLODs[i][j] = 第 i 个网格的第 j 个 LOD 级别（j=0=LOD1, j=1=LOD2, j=2=LOD3）
    for (size_t i = 0; i < allLODs.size(); ++i) {
        std::cout << "网格 " << i << " LOD 生成完成" << std::endl;
    }
}

// 示例 6: 使用推荐配置
void Example6_RecommendedOptions() {
    Ref<Mesh> sourceMesh = LoadMesh("tree.obj");
    
    // 获取推荐的配置
    auto options = LODGenerator::GetRecommendedOptions(sourceMesh);
    
    // 可以根据需要调整
    options.flags |= LODGenerator::SimplifyOptions::LockBorder;
    
    // 生成 LOD
    auto lodMeshes = LODGenerator::GenerateLODLevels(sourceMesh, options);
}
```

---

## 🔨 实现细节

### 2.1 简化流程

#### 步骤 1: 提取网格数据

```cpp
// 从 Mesh 对象提取顶点和索引数据
std::vector<Vertex> vertices;
std::vector<uint32_t> indices;

sourceMesh->AccessVertices([&](const std::vector<Vertex>& vs) {
    vertices = vs;
});

sourceMesh->AccessIndices([&](const std::vector<uint32_t>& is) {
    indices = is;
});
```

#### 步骤 2: 准备 meshoptimizer 输入

```cpp
// 提取顶点位置（meshoptimizer 需要）
std::vector<float> positions;
positions.reserve(vertices.size() * 3);
for (const auto& v : vertices) {
    positions.push_back(v.position.x());
    positions.push_back(v.position.y());
    positions.push_back(v.position.z());
}

// 提取顶点属性（法线、UV、颜色等）
std::vector<float> attributes;
attributes.reserve(vertices.size() * 8);  // normal(3) + texCoord(2) + color(3)
for (const auto& v : vertices) {
    // 法线
    attributes.push_back(v.normal.x());
    attributes.push_back(v.normal.y());
    attributes.push_back(v.normal.z());
    // 纹理坐标
    attributes.push_back(v.texCoord.x());
    attributes.push_back(v.texCoord.y());
    // 颜色
    attributes.push_back(v.color.r);
    attributes.push_back(v.color.g);
    attributes.push_back(v.color.b);
}
```

#### 步骤 3: 执行简化

```cpp
// 计算目标索引数量
size_t targetIndexCount = CalculateTargetIndexCount(
    indices.size(),
    lodLevel,
    options
);

// 计算误差缩放因子
float errorScale = meshopt_simplifyScale(
    positions.data(),
    vertices.size(),
    sizeof(float) * 3
);

// 计算目标误差（如果使用 TargetError 模式）
float targetError = options.targetErrors.lod1 / errorScale;

// 准备属性权重
std::vector<float> attributeWeights = {
    options.attributeWeights.normal,      // 法线权重
    options.attributeWeights.normal,
    options.attributeWeights.normal,
    options.attributeWeights.texCoord,   // UV 权重
    options.attributeWeights.texCoord,
    options.attributeWeights.color,      // 颜色权重
    options.attributeWeights.color,
    options.attributeWeights.color
};

// 执行简化（使用属性感知简化）
std::vector<unsigned int> simplifiedIndices(indices.size());
float resultError = 0.0f;

size_t newIndexCount = meshopt_simplifyWithAttributes(
    simplifiedIndices.data(),
    indices.data(),
    indices.size(),
    positions.data(),
    vertices.size(),
    sizeof(float) * 3,
    attributes.data(),
    sizeof(float) * 8,
    attributeWeights.data(),
    8,  // 属性数量
    nullptr,  // vertex_lock
    targetIndexCount,
    targetError,
    options.flags,
    &resultError
);

simplifiedIndices.resize(newIndexCount);
```

#### 步骤 4: 重建顶点数据

```cpp
// 使用简化后的索引重建顶点数据
// 需要重新映射顶点，移除未使用的顶点
std::vector<Vertex> simplifiedVertices;
std::unordered_set<uint32_t> usedIndices;

// 收集使用的顶点索引
for (uint32_t idx : simplifiedIndices) {
    usedIndices.insert(idx);
}

// 创建顶点重映射表
std::vector<uint32_t> remap(vertices.size(), UINT32_MAX);
uint32_t newVertexIndex = 0;
for (uint32_t i = 0; i < vertices.size(); ++i) {
    if (usedIndices.count(i) > 0) {
        remap[i] = newVertexIndex++;
        simplifiedVertices.push_back(vertices[i]);
    }
}

// 重映射索引
for (uint32_t& idx : simplifiedIndices) {
    idx = remap[idx];
}
```

#### 步骤 5: 重新计算法线和切线

```cpp
// 创建新的 Mesh 对象
Ref<Mesh> simplifiedMesh = std::make_shared<Mesh>(
    simplifiedVertices,
    simplifiedIndices
);

// 重新计算法线
if (options.recalculateNormals) {
    simplifiedMesh->RecalculateNormals();
}

// 重新计算切线
if (options.recalculateTangents) {
    simplifiedMesh->RecalculateTangents();
}

// 上传到 GPU
simplifiedMesh->Upload();
```

### 2.2 目标三角形数量计算

```cpp
size_t CalculateTargetIndexCount(
    size_t originalIndexCount,
    int lodLevel,
    const SimplifyOptions& options
) {
    size_t originalTriangleCount = originalIndexCount / 3;
    size_t targetTriangleCount = 0;
    
    if (options.mode == SimplifyOptions::Mode::TargetTriangleCount) {
        // 使用指定的三角形数量
        switch (lodLevel) {
            case 1:
                targetTriangleCount = options.triangleCounts.lod1;
                if (targetTriangleCount == 0) {
                    targetTriangleCount = originalTriangleCount * 0.5;  // 默认 50%
                }
                break;
            case 2:
                targetTriangleCount = options.triangleCounts.lod2;
                if (targetTriangleCount == 0) {
                    targetTriangleCount = originalTriangleCount * 0.25;  // 默认 25%
                }
                break;
            case 3:
                targetTriangleCount = options.triangleCounts.lod3;
                if (targetTriangleCount == 0) {
                    targetTriangleCount = originalTriangleCount * 0.1;  // 默认 10%
                }
                break;
        }
    } else {
        // TargetError 模式：使用误差，让 meshoptimizer 决定三角形数量
        // 返回一个较大的值，让简化算法根据误差自动决定
        targetTriangleCount = 1;  // 最小值，让算法根据误差决定
    }
    
    // 确保至少保留一些三角形
    targetTriangleCount = std::max(targetTriangleCount, size_t(1));
    
    // 不能超过原始数量
    targetTriangleCount = std::min(targetTriangleCount, originalTriangleCount);
    
    return targetTriangleCount * 3;  // 返回索引数量
}
```

### 2.3 推荐配置计算

```cpp
SimplifyOptions GetRecommendedOptions(Ref<Mesh> sourceMesh) {
    SimplifyOptions options;
    
    size_t triangleCount = sourceMesh->GetTriangleCount();
    
    // 根据三角形数量选择模式
    if (triangleCount > 10000) {
        // 高多边形：使用目标三角形数量
        options.mode = SimplifyOptions::Mode::TargetTriangleCount;
        options.triangleCounts.lod1 = triangleCount * 0.5;
        options.triangleCounts.lod2 = triangleCount * 0.25;
        options.triangleCounts.lod3 = triangleCount * 0.1;
    } else {
        // 低多边形：使用目标误差
        options.mode = SimplifyOptions::Mode::TargetError;
        options.targetErrors.lod1 = 0.01f;
        options.targetErrors.lod2 = 0.03f;
        options.targetErrors.lod3 = 0.05f;
    }
    
    // 根据网格复杂度设置属性权重
    if (triangleCount > 50000) {
        // 高复杂度：降低属性权重以加快简化速度
        options.attributeWeights.normal = 0.8f;
        options.attributeWeights.texCoord = 0.8f;
        options.attributeWeights.color = 0.3f;
    }
    
    // 默认重新计算法线
    options.recalculateNormals = true;
    
    return options;
}
```

---

## 📦 集成方案

### 3.1 文件结构

```
include/render/
  ├── lod_generator.h          # LOD 生成器头文件
  └── lod_system.h             # 现有 LOD 系统（不变）

src/render/
  ├── lod_generator.cpp        # LOD 生成器实现
  └── lod_system.cpp           # 现有 LOD 系统实现（不变）

third_party/meshoptimizer/    # meshoptimizer 库（已存在）
  └── src/
      └── meshoptimizer.h
```

### 3.2 CMake 集成

```cmake
# CMakeLists.txt

# 添加 meshoptimizer 库
add_subdirectory(third_party/meshoptimizer)

# 添加 LOD 生成器源文件
set(LOD_GENERATOR_SOURCES
    src/render/lod_generator.cpp
)

# 链接 meshoptimizer
target_link_libraries(render_lib
    PRIVATE
    meshoptimizer
)
```

### 3.3 依赖关系

```
LODGenerator
  ├── Mesh (已有)
  ├── LODConfig (已有)
  └── meshoptimizer (第三方库)
      └── meshopt_simplify()
      └── meshopt_simplifyWithAttributes()
      └── meshopt_simplifyScale()
```

---

## ⚡ 性能考虑

### 4.1 简化性能

- **时间复杂度**：O(n log n)，其中 n 是三角形数量
- **内存占用**：临时需要约 2-3 倍原始网格内存
- **典型性能**：
  - 10K 三角形：~10-50ms
  - 100K 三角形：~100-500ms
  - 1M 三角形：~1-5s

### 4.2 优化建议

1. **异步生成**：对于大型网格，可以在后台线程生成 LOD
2. **缓存结果**：生成后保存到文件，避免重复生成
3. **批量处理**：使用 `BatchGenerateLODLevels()` 批量处理多个网格
4. **渐进式生成**：先生成 LOD1，需要时再生成 LOD2/LOD3

### 4.3 内存管理

- 简化过程中会创建临时顶点和索引缓冲区
- 简化完成后，临时缓冲区会被释放
- 最终网格会占用与简化后网格大小相当的内存

---

## 🧪 测试方案

### 5.1 单元测试

```cpp
// tests/lod_generator_test.cpp

TEST(LODGenerator, BasicGeneration) {
    // 创建测试网格（立方体）
    auto cube = CreateCubeMesh();
    
    // 生成 LOD
    auto lodMeshes = LODGenerator::GenerateLODLevels(cube);
    
    // 验证结果
    ASSERT_NE(lodMeshes[0], nullptr);  // LOD1
    ASSERT_NE(lodMeshes[1], nullptr);  // LOD2
    ASSERT_NE(lodMeshes[2], nullptr);  // LOD3
    
    // 验证三角形数量递减
    size_t lod0Triangles = cube->GetTriangleCount();
    size_t lod1Triangles = lodMeshes[0]->GetTriangleCount();
    size_t lod2Triangles = lodMeshes[1]->GetTriangleCount();
    size_t lod3Triangles = lodMeshes[2]->GetTriangleCount();
    
    ASSERT_LT(lod1Triangles, lod0Triangles);
    ASSERT_LT(lod2Triangles, lod1Triangles);
    ASSERT_LT(lod3Triangles, lod2Triangles);
}

TEST(LODGenerator, TargetTriangleCount) {
    auto mesh = LoadTestMesh();
    size_t originalTriangles = mesh->GetTriangleCount();
    
    LODGenerator::SimplifyOptions options;
    options.mode = LODGenerator::SimplifyOptions::Mode::TargetTriangleCount;
    options.triangleCounts.lod1 = originalTriangles / 2;
    
    auto lod1 = LODGenerator::GenerateLODLevel(mesh, 1, options);
    
    // 验证三角形数量接近目标
    size_t actualTriangles = lod1->GetTriangleCount();
    size_t targetTriangles = options.triangleCounts.lod1;
    
    // 允许 10% 误差（meshoptimizer 可能无法精确达到目标）
    float error = std::abs((float)actualTriangles - (float)targetTriangles) / targetTriangles;
    ASSERT_LT(error, 0.1f);
}

TEST(LODGenerator, AutoConfigure) {
    auto mesh = LoadTestMesh();
    LODConfig config;
    
    bool success = LODGenerator::AutoConfigureLOD(mesh, config);
    
    ASSERT_TRUE(success);
    ASSERT_EQ(config.lodMeshes.size(), 3);  // LOD1, LOD2, LOD3
    ASSERT_NE(config.lodMeshes[0], nullptr);
    ASSERT_NE(config.lodMeshes[1], nullptr);
    ASSERT_NE(config.lodMeshes[2], nullptr);
}
```

### 5.2 性能测试

```cpp
TEST(LODGenerator, Performance) {
    auto largeMesh = LoadLargeMesh(100000);  // 10万三角形
    
    auto start = std::chrono::high_resolution_clock::now();
    auto lodMeshes = LODGenerator::GenerateLODLevels(largeMesh);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // 应该在合理时间内完成（例如 < 1秒）
    ASSERT_LT(duration.count(), 1000);
}
```

---

## 📝 使用指南

### 6.1 快速开始

```cpp
#include "render/lod_generator.h"
#include "render/lod_system.h"

// 1. 加载原始网格
Ref<Mesh> sourceMesh = LoadMesh("my_model.obj");

// 2. 自动生成 LOD 级别
auto lodMeshes = LODGenerator::GenerateLODLevels(sourceMesh);

// 3. 配置 LOD
ECS::LODComponent lodComp;
lodComp.config.enabled = true;
lodComp.config.distanceThresholds = {50.0f, 150.0f, 500.0f, 1000.0f};
lodComp.config.lodMeshes = lodMeshes;  // [LOD1, LOD2, LOD3]

// 4. 添加到实体
world->AddComponent<ECS::LODComponent>(entity, lodComp);
```

### 6.2 最佳实践

1. **预生成 LOD**：在资源加载时生成 LOD，而不是运行时
2. **保存到文件**：生成后保存到文件，避免重复生成
3. **使用推荐配置**：对于大多数情况，使用 `GetRecommendedOptions()` 即可
4. **验证结果**：生成后使用 `ValidateSimplifiedMesh()` 验证结果
5. **渐进式生成**：对于大型网格，可以先生成 LOD1，需要时再生成其他级别

### 6.3 常见问题

**Q: 简化后的网格看起来不对？**
A: 尝试调整 `attributeWeights`，增加法线和 UV 的权重。

**Q: 简化太慢？**
A: 对于大型网格，考虑使用 `Sparse` 标志或降低属性权重。

**Q: 简化后三角形数量不对？**
A: meshoptimizer 可能无法精确达到目标数量，这是正常的。使用 `TargetError` 模式可以获得更可预测的结果。

---

## 🚀 实施计划

### 阶段 1: 基础实现 (1-2 周)

- [ ] 实现 `LODGenerator` 类
- [ ] 实现基本的简化流程
- [ ] 集成 meshoptimizer
- [ ] 单元测试

### 阶段 2: 功能完善 (1 周)

- [ ] 实现属性感知简化
- [ ] 实现推荐配置
- [ ] 实现批量生成
- [ ] 性能优化

### 阶段 3: 集成和文档 (1 周)

- [ ] 集成到现有 LOD 系统
- [ ] 更新 API 文档
- [ ] 编写使用指南
- [ ] 性能测试

---

## 📚 参考资料

- [meshoptimizer 文档](https://github.com/zeux/meshoptimizer)
- [LOD 系统 API 文档](../api/LOD.md)
- [Mesh API 文档](../api/Mesh.md)
- [LOD 优化方案](./LOD_Instanced_Rendering_Optimization.md)

---

**文档版本**: v1.0  
**最后更新**: 2025-11-28 
**维护者**: Linductor

