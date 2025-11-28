# LOD 系统 API 参考

[返回 API 首页](README.md)

---

## 概述

LOD（Level of Detail）系统提供基于距离的细节级别管理，通过自动选择不同细节级别的网格、模型和材质来优化渲染性能。系统支持与 ECS 无缝集成，提供批量 LOD 计算、平滑过渡和统计信息查询功能。

**头文件**: `render/lod_system.h`  
**命名空间**: `Render`、`Render::ECS`

### ✨ 功能特性

- ✅ **距离驱动的 LOD 选择**：基于相机距离自动选择合适的 LOD 级别
- ✅ **ECS 集成**：通过 `LODComponent` 与现有 ECS 系统无缝集成
- ✅ **批量计算优化**：批量处理多个实体，提升性能
- ✅ **平滑过渡**：避免频繁切换，提供平滑的 LOD 过渡
- ✅ **多资源支持**：支持网格、模型、材质和纹理的 LOD 配置
- ✅ **统计信息**：提供详细的 LOD 使用统计，便于调试和优化
- ✅ **向后兼容**：LOD 为可选功能，不影响现有代码

---

## 数据结构

### LODLevel

LOD 级别枚举，定义不同的细节级别。

```cpp
enum class LODLevel {
    LOD0 = 0,  ///< 最高细节（最近）
    LOD1 = 1,  ///< 中等细节
    LOD2 = 2,  ///< 低细节
    LOD3 = 3,  ///< 最低细节（最远）
    Culled = 4 ///< 剔除（超出范围）
};
```

**说明**:
- `LOD0` - 最高细节级别，用于近距离对象
- `LOD1` - 中等细节级别
- `LOD2` - 低细节级别
- `LOD3` - 最低细节级别，用于远距离对象
- `Culled` - 超出最大距离，被剔除

---

### TextureLODStrategy

纹理 LOD 策略枚举，定义如何为不同 LOD 级别选择纹理。

```cpp
enum class TextureLODStrategy {
    UseLODTextures,    ///< 使用 lodTextures 中指定的纹理
    UseMipmap,         ///< 使用原始纹理的 mipmap（自动）
    DisableTextures    ///< 远距离禁用纹理（LOD2+）
};
```

**说明**:
- `UseLODTextures` - 使用 `LODConfig::lodTextures` 中配置的纹理集合
- `UseMipmap` - 使用原始纹理的 mipmap（推荐，自动管理）
- `DisableTextures` - 在 LOD2+ 级别禁用纹理以节省采样

---

### LODTextureSet

LOD 纹理集合，为每个 LOD 级别定义可选的纹理集合。

```cpp
struct LODTextureSet {
    Ref<Texture> diffuseMap;      ///< 漫反射贴图
    Ref<Texture> normalMap;        ///< 法线贴图（LOD2+ 可省略）
    Ref<Texture> specularMap;      ///< 高光贴图（LOD2+ 可省略）
    Ref<Texture> emissiveMap;      ///< 自发光贴图（LOD2+ 可省略）
    
    LODTextureSet() = default;
};
```

**使用场景**:
- 为不同 LOD 级别使用不同分辨率的纹理（例如：LOD0 使用 4K，LOD1 使用 2K，LOD2 使用 1K）
- 在远距离禁用某些纹理以节省内存和带宽

---

### LODConfig

LOD 配置结构，定义每个 LOD 级别的距离阈值、网格、材质和纹理。

```cpp
struct LODConfig {
    // 距离阈值
    std::vector<float> distanceThresholds{50.0f, 150.0f, 500.0f, 1000.0f};
    
    // LOD 资源
    std::vector<Ref<Mesh>> lodMeshes;           ///< 每个 LOD 级别的网格
    std::vector<Ref<Model>> lodModels;          ///< 每个 LOD 级别的模型
    std::vector<Ref<Material>> lodMaterials;   ///< 每个 LOD 级别的材质
    std::vector<LODTextureSet> lodTextures;    ///< 每个 LOD 级别的纹理集合
    
    // 配置参数
    TextureLODStrategy textureStrategy = TextureLODStrategy::UseMipmap;
    float transitionDistance = 10.0f;          ///< 平滑过渡距离
    float boundingBoxScale = 1.0f;            ///< 包围盒缩放因子
    bool enabled = true;                      ///< 是否启用 LOD
    
    // 辅助方法
    LODLevel CalculateLOD(float distance) const;
    Ref<Mesh> GetLODMesh(LODLevel level, Ref<Mesh> defaultMesh) const;
    Ref<Model> GetLODModel(LODLevel level, Ref<Model> defaultModel) const;
    Ref<Material> GetLODMaterial(LODLevel level, Ref<Material> defaultMaterial) const;
    void ApplyLODTextures(LODLevel level, Ref<Material> material) const;
};
```

**成员说明**:

#### distanceThresholds
LOD 级别距离阈值（从近到远）。例如 `{50.0f, 150.0f, 500.0f, 1000.0f}` 表示：
- 距离 < 50: LOD0
- 50 ≤ 距离 < 150: LOD1
- 150 ≤ 距离 < 500: LOD2
- 500 ≤ 距离 < 1000: LOD3
- 距离 ≥ 1000: Culled

#### lodMeshes / lodModels / lodMaterials
每个 LOD 级别对应的资源。索引对应 LOD 级别（0=LOD0, 1=LOD1, 2=LOD2, 3=LOD3）。如果某个级别为 `nullptr` 或未配置，则使用原始资源。

#### lodTextures
每个 LOD 级别对应的纹理集合。仅在 `textureStrategy == UseLODTextures` 时使用。

#### transitionDistance
LOD 切换的平滑过渡距离（单位：世界单位）。只有当距离变化超过此阈值时才会切换 LOD，避免频繁切换。

#### boundingBoxScale
包围盒缩放因子，用于考虑对象大小的距离计算。较大的对象应该使用更大的距离阈值。

#### enabled
是否启用 LOD。如果为 `false`，始终使用 LOD0（最高细节）。

**方法说明**:

##### CalculateLOD
```cpp
LODLevel CalculateLOD(float distance) const;
```
根据距离计算 LOD 级别。

**参数**:
- `distance` - 到相机的距离（世界单位）

**返回**: LOD 级别

##### GetLODMesh / GetLODModel / GetLODMaterial
```cpp
Ref<Mesh> GetLODMesh(LODLevel level, Ref<Mesh> defaultMesh) const;
Ref<Model> GetLODModel(LODLevel level, Ref<Model> defaultModel) const;
Ref<Material> GetLODMaterial(LODLevel level, Ref<Material> defaultMaterial) const;
```
获取指定 LOD 级别的资源。如果未配置该级别的资源，返回默认资源。

**参数**:
- `level` - LOD 级别
- `defaultMesh/defaultModel/defaultMaterial` - 默认资源（LOD0 或未配置时使用）

**返回**: 资源指针

##### ApplyLODTextures
```cpp
void ApplyLODTextures(LODLevel level, Ref<Material> material) const;
```
应用 LOD 纹理到材质。仅在 `textureStrategy == UseLODTextures` 时生效。

**参数**:
- `level` - LOD 级别
- `material` - 材质（会被修改）

**注意**: 对于 LOD2+，会自动禁用某些纹理以节省采样。

---

### LODComponent

LOD 组件（ECS），附加到实体上，提供 LOD 配置和当前 LOD 级别。

```cpp
namespace ECS {
    struct LODComponent {
        LODConfig config;              ///< LOD 配置
        LODLevel currentLOD = LODLevel::LOD0;  ///< 当前 LOD 级别
        float lastDistance = 0.0f;     ///< 上次计算的距离
        uint64_t lastUpdateFrame = 0;  ///< 上次更新的帧 ID
        
        // 统计信息（调试用）
        uint32_t lodSwitchCount = 0;   ///< LOD 切换次数
        LODLevel lastLOD = LODLevel::LOD0;  ///< 上次的 LOD 级别
        
        // 构造函数
        LODComponent() = default;
        explicit LODComponent(const LODConfig& lodConfig);
        
        // 辅助方法
        bool IsEnabled() const;
        std::string GetLODLevelString() const;
    };
}
```

**成员说明**:

- `config` - LOD 配置
- `currentLOD` - 当前 LOD 级别（由 `LODSelector` 自动更新）
- `lastDistance` - 上次计算的距离（用于平滑过渡）
- `lastUpdateFrame` - 上次更新的帧 ID（避免每帧都更新）
- `lodSwitchCount` - LOD 切换次数（用于性能分析）
- `lastLOD` - 上次的 LOD 级别（用于检测 LOD 切换）

**方法说明**:

##### IsEnabled
```cpp
bool IsEnabled() const;
```
检查 LOD 是否启用。

**返回**: 如果启用返回 `true`

##### GetLODLevelString
```cpp
std::string GetLODLevelString() const;
```
获取当前 LOD 级别的字符串表示（用于调试）。

**返回**: LOD 级别字符串（"LOD0", "LOD1", "LOD2", "LOD3", "Culled"）

---

## 主要类

### LODSelector

LOD 选择器类，负责计算实体到相机的距离并选择 LOD 级别。

```cpp
class LODSelector {
public:
    // 距离计算
    static float CalculateDistance(
        const Vector3& entityPosition,
        const Vector3& cameraPosition
    );
    
    static float CalculateDistanceWithBounds(
        const Vector3& entityPosition,
        const AABB& entityBounds,
        const Vector3& cameraPosition,
        float boundingBoxScale = 1.0f
    );
    
    // 批量 LOD 计算
    static void BatchCalculateLOD(
        const std::vector<ECS::EntityID>& entities,
        ECS::World* world,
        const Vector3& cameraPosition,
        uint64_t frameId
    );
    
    static void BatchCalculateLODWithBounds(
        const std::vector<ECS::EntityID>& entities,
        ECS::World* world,
        const Vector3& cameraPosition,
        uint64_t frameId,
        std::function<AABB(ECS::EntityID)> getBounds = nullptr
    );
};
```

**方法说明**:

#### CalculateDistance
```cpp
static float CalculateDistance(
    const Vector3& entityPosition,
    const Vector3& cameraPosition
);
```
计算实体到相机的欧氏距离。

**参数**:
- `entityPosition` - 实体世界位置
- `cameraPosition` - 相机世界位置

**返回**: 距离（世界单位）

#### CalculateDistanceWithBounds
```cpp
static float CalculateDistanceWithBounds(
    const Vector3& entityPosition,
    const AABB& entityBounds,
    const Vector3& cameraPosition,
    float boundingBoxScale = 1.0f
);
```
计算实体到相机的距离（考虑包围盒）。使用包围盒中心到相机的距离，并减去包围盒最大轴的一半，以更准确地反映对象的视觉大小。

**参数**:
- `entityPosition` - 实体世界位置
- `entityBounds` - 实体包围盒（世界空间）
- `cameraPosition` - 相机世界位置
- `boundingBoxScale` - 包围盒缩放因子（默认 1.0）

**返回**: 调整后的距离（世界单位）

#### BatchCalculateLOD
```cpp
static void BatchCalculateLOD(
    const std::vector<ECS::EntityID>& entities,
    ECS::World* world,
    const Vector3& cameraPosition,
    uint64_t frameId
);
```
批量计算多个实体的 LOD 级别。使用帧 ID 避免每帧都更新，支持平滑过渡。

**参数**:
- `entities` - 实体列表（应该已经包含 `LODComponent` 和 `TransformComponent`）
- `world` - ECS World 对象指针
- `cameraPosition` - 相机世界位置
- `frameId` - 当前帧 ID（用于避免重复计算）

**注意**:
- 此方法会修改实体的 `LODComponent`，更新 `currentLOD` 和统计信息
- 如果实体没有 `LODComponent` 或 `TransformComponent`，会被跳过
- 使用 `transitionDistance` 实现平滑过渡，避免频繁切换

#### BatchCalculateLODWithBounds
```cpp
static void BatchCalculateLODWithBounds(
    const std::vector<ECS::EntityID>& entities,
    ECS::World* world,
    const Vector3& cameraPosition,
    uint64_t frameId,
    std::function<AABB(ECS::EntityID)> getBounds = nullptr
);
```
批量计算 LOD 级别（考虑包围盒版本）。与 `BatchCalculateLOD` 类似，但使用包围盒进行更准确的距离计算。

**参数**:
- `entities` - 实体列表
- `world` - ECS World 对象指针
- `cameraPosition` - 相机世界位置
- `frameId` - 当前帧 ID
- `getBounds` - 函数，用于获取实体的包围盒（可选，如果为 `nullptr` 则使用位置计算）

**注意**: `getBounds` 函数签名：`AABB(EntityID entity)`

---

### LODDebug

LOD 调试和查询工具命名空间，提供查询实体 LOD 状态的辅助函数。

```cpp
namespace LODDebug {
    std::string GetEntityLODStatus(ECS::World* world, ECS::EntityID entity);
    bool IsLODEnabled(ECS::World* world, ECS::EntityID entity);
    LODLevel GetEntityLODLevel(ECS::World* world, ECS::EntityID entity);
}
```

**函数说明**:

#### GetEntityLODStatus
```cpp
std::string GetEntityLODStatus(ECS::World* world, ECS::EntityID entity);
```
获取实体的 LOD 状态信息（用于调试）。

**参数**:
- `world` - ECS World 对象
- `entity` - 实体 ID

**返回**: LOD 状态字符串，如果实体没有 `LODComponent` 返回 "No LOD"

**示例输出**:
- `"LOD: LOD1 (distance: 125.5), switches: 3"`
- `"No LOD"`
- `"LOD disabled"`

#### IsLODEnabled
```cpp
bool IsLODEnabled(ECS::World* world, ECS::EntityID entity);
```
检查实体是否启用了 LOD。

**参数**:
- `world` - ECS World 对象
- `entity` - 实体 ID

**返回**: 如果启用 LOD 返回 `true`

#### GetEntityLODLevel
```cpp
LODLevel GetEntityLODLevel(ECS::World* world, ECS::EntityID entity);
```
获取实体的当前 LOD 级别。

**参数**:
- `world` - ECS World 对象
- `entity` - 实体 ID

**返回**: LOD 级别，如果实体没有 `LODComponent` 返回 `LODLevel::LOD0`

---

## LOD 资源获取方式

### 概述

LOD 不同级别的网格/模型资源可以通过以下方式获得：

1. **手动准备**（当前支持）：在建模工具中创建不同细节级别的模型文件
2. **自动生成**（计划中）：通过网格简化算法自动生成 LOD 级别
3. **运行时简化**（计划中）：使用第三方库（如 meshoptimizer）进行网格简化

### 当前实现方式（手动配置）

**阶段 1 实现**：LOD 系统目前支持手动配置 LOD 资源。你需要：

1. **在建模工具中准备不同 LOD 级别的模型**
   - LOD0：原始高精度模型（100% 顶点数）
   - LOD1：简化 30-50% 的模型
   - LOD2：简化 60-70% 的模型
   - LOD3：简化 80-90% 的模型

2. **手动加载并配置到 LODConfig**

```cpp
// 加载不同 LOD 级别的网格
Ref<Mesh> lod0Mesh = LoadMesh("tree_lod0.obj");  // 原始模型（10000 顶点）
Ref<Mesh> lod1Mesh = LoadMesh("tree_lod1.obj");  // 简化模型（7000 顶点，减少 30%）
Ref<Mesh> lod2Mesh = LoadMesh("tree_lod2.obj");  // 简化模型（4000 顶点，减少 60%）
Ref<Mesh> lod3Mesh = LoadMesh("tree_lod3.obj");  // 简化模型（2000 顶点，减少 80%）

// 配置 LOD
LODComponent lodComp;
lodComp.config.enabled = true;
lodComp.config.distanceThresholds = {50.0f, 150.0f, 500.0f, 1000.0f};

// 设置 LOD 网格（索引对应 LOD 级别）
lodComp.config.lodMeshes.push_back(lod0Mesh);  // 索引 0 = LOD0
lodComp.config.lodMeshes.push_back(lod1Mesh);  // 索引 1 = LOD1
lodComp.config.lodMeshes.push_back(lod2Mesh);  // 索引 2 = LOD2
lodComp.config.lodMeshes.push_back(lod3Mesh);  // 索引 3 = LOD3

world->AddComponent<LODComponent>(entity, lodComp);
```

### LOD 级别简化说明

不同 LOD 级别通常通过以下方式简化：

#### LOD0（最高细节）
- **顶点数**：100%（原始模型）
- **三角形数**：100%
- **纹理**：最高分辨率（4K 或 2K）
- **材质**：完整材质（包含所有贴图：漫反射、法线、高光等）
- **用途**：近距离对象，玩家可以看到细节

#### LOD1（中等细节）
- **顶点数**：约 50-70%（简化 30-50%）
- **三角形数**：约 50-70%
- **纹理**：中等分辨率（2K 或 1K）
- **材质**：简化材质（可能省略某些贴图）
- **简化方法**：
  - 减少顶点密度
  - 合并相邻顶点
  - 移除不必要的细节（如小装饰）
- **用途**：中等距离对象

#### LOD2（低细节）
- **顶点数**：约 20-40%（简化 60-80%）
- **三角形数**：约 20-40%
- **纹理**：低分辨率（1K 或 512）
- **材质**：简化材质（通常只保留漫反射贴图）
- **简化方法**：
  - 大幅减少顶点
  - 简化几何形状
  - 移除法线贴图和高光贴图
- **用途**：远距离对象

#### LOD3（最低细节）
- **顶点数**：约 10-20%（简化 80-90%）
- **三角形数**：约 10-20%
- **纹理**：最低分辨率（512 或 256）
- **材质**：极简材质（可能只使用颜色）
- **简化方法**：
  - 极简几何形状
  - 可能使用 Billboard（广告牌）技术
  - 禁用大部分贴图
- **用途**：极远距离对象

### 网格简化方法

#### 1. 建模工具简化（推荐）

在 Blender、Maya、3ds Max 等建模工具中：

1. **Decimate Modifier（Blender）**
   ```
   - 选择模型
   - 添加 Decimate Modifier
   - 设置 Ratio（比例）：
     * LOD1: 0.5-0.7（保留 50-70%）
     * LOD2: 0.2-0.4（保留 20-40%）
     * LOD3: 0.1-0.2（保留 10-20%）
   - 导出为不同文件
   ```

2. **Progressive Mesh（Maya）**
   - 使用 Maya 的网格简化工具
   - 逐步减少顶点数
   - 导出不同 LOD 级别

#### 2. 自动简化算法（计划中）

未来版本将支持自动网格简化：

```cpp
// 计划中的 API（阶段 3.1）
LODLoadOptions options;
options.autoGenerateLOD = true;
options.lod1Simplification = 0.3f;  // 简化 30%
options.lod2Simplification = 0.6f;  // 简化 60%
options.lod3Simplification = 0.8f;  // 简化 80%

LODConfig config = LODLoader::LoadLODConfig(baseMesh, options);
```

**简化算法**：
- 使用 **meshoptimizer** 库（推荐）
- 或使用 **Quadric Error Metrics (QEM)** 算法
- 或使用 **Edge Collapse** 算法

#### 3. 运行时简化（高级）

对于动态生成的几何体，可以在运行时简化：

```cpp
// 示例：运行时简化（需要实现简化算法）
Ref<Mesh> baseMesh = GenerateComplexMesh();
Ref<Mesh> lod1Mesh = SimplifyMesh(baseMesh, 0.3f);  // 简化 30%
Ref<Mesh> lod2Mesh = SimplifyMesh(baseMesh, 0.6f);  // 简化 60%

lodComp.config.lodMeshes.push_back(lod1Mesh);
lodComp.config.lodMeshes.push_back(lod2Mesh);
```

### 纹理 LOD 配置

纹理也可以为不同 LOD 级别使用不同分辨率：

```cpp
// 加载不同分辨率的纹理
Ref<Texture> lod0Diffuse = LoadTexture("tree_diffuse_4k.png");  // 4K 纹理
Ref<Texture> lod1Diffuse = LoadTexture("tree_diffuse_2k.png");  // 2K 纹理
Ref<Texture> lod2Diffuse = LoadTexture("tree_diffuse_1k.png");  // 1K 纹理
Ref<Texture> lod3Diffuse = LoadTexture("tree_diffuse_512.png"); // 512 纹理

// 配置 LOD 纹理
LODTextureSet lod0Textures;
lod0Textures.diffuseMap = lod0Diffuse;
lod0Textures.normalMap = LoadTexture("tree_normal_4k.png");
lod0Textures.specularMap = LoadTexture("tree_specular_4k.png");
lodComp.config.lodTextures.push_back(lod0Textures);

LODTextureSet lod1Textures;
lod1Textures.diffuseMap = lod1Diffuse;
lod1Textures.normalMap = LoadTexture("tree_normal_2k.png");
lodComp.config.lodTextures.push_back(lod1Textures);

LODTextureSet lod2Textures;
lod2Textures.diffuseMap = lod2Diffuse;
// LOD2 不使用方法线贴图
lodComp.config.lodTextures.push_back(lod2Textures);

LODTextureSet lod3Textures;
lod3Textures.diffuseMap = lod3Diffuse;
// LOD3 只使用漫反射贴图
lodComp.config.lodTextures.push_back(lod3Textures);

lodComp.config.textureStrategy = TextureLODStrategy::UseLODTextures;
```

### 使用 Mipmap（推荐）

如果不想手动准备不同分辨率的纹理，可以使用 mipmap（自动生成）：

```cpp
// 使用原始纹理的 mipmap（自动）
lodComp.config.textureStrategy = TextureLODStrategy::UseMipmap;

// 系统会自动使用纹理的 mipmap 级别：
// - LOD0: 使用最高 mipmap 级别
// - LOD1: 使用中等 mipmap 级别
// - LOD2: 使用较低 mipmap 级别
// - LOD3: 使用最低 mipmap 级别
```

### 实际示例：检查 LOD 网格的顶点数

```cpp
// 检查不同 LOD 级别的顶点数
if (!lodComp.config.lodMeshes.empty()) {
    for (size_t i = 0; i < lodComp.config.lodMeshes.size(); ++i) {
        if (lodComp.config.lodMeshes[i]) {
            size_t vertexCount = lodComp.config.lodMeshes[i]->GetVertexCount();
            size_t triangleCount = lodComp.config.lodMeshes[i]->GetTriangleCount();
            
            std::cout << "LOD" << i << ": " 
                      << vertexCount << " 顶点, " 
                      << triangleCount << " 三角形" << std::endl;
        }
    }
}

// 输出示例：
// LOD0: 10000 顶点, 5000 三角形
// LOD1: 7000 顶点, 3500 三角形  (减少 30%)
// LOD2: 4000 顶点, 2000 三角形  (减少 60%)
// LOD3: 2000 顶点, 1000 三角形  (减少 80%)
```

### 性能影响

不同 LOD 级别对性能的影响：

| LOD 级别 | 顶点数 | 三角形数 | 纹理采样 | 性能提升 |
|---------|--------|---------|---------|---------|
| LOD0    | 100%   | 100%    | 4K      | 基准    |
| LOD1    | 50-70% | 50-70%  | 2K      | 1.5-2x  |
| LOD2    | 20-40% | 20-40%  | 1K      | 3-5x    |
| LOD3    | 10-20% | 10-20%  | 512     | 5-10x   |

**注意**：实际性能提升取决于 GPU、场景复杂度等因素。

---

## 使用示例

### 基础使用

#### 1. 创建实体并配置 LOD（手动加载资源）

```cpp
#include "render/lod_system.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"

using namespace Render;
using namespace Render::ECS;

// 创建实体
EntityID entity = world->CreateEntity();

// 添加 Transform 和 MeshRender 组件
world->AddComponent<TransformComponent>(entity);
world->AddComponent<MeshRenderComponent>(entity);

// 配置 LOD
LODComponent lodComp;
lodComp.config.enabled = true;
lodComp.config.distanceThresholds = {50.0f, 150.0f, 500.0f, 1000.0f};
lodComp.config.transitionDistance = 10.0f;

// 加载 LOD 网格（可选）
// lodComp.config.lodMeshes.push_back(LoadMesh("tree_lod1.obj"));
// lodComp.config.lodMeshes.push_back(LoadMesh("tree_lod2.obj"));

world->AddComponent<LODComponent>(entity, lodComp);
```

#### 2. 创建 LOD 更新系统

```cpp
class LODUpdateSystem : public System {
public:
    void Update(float deltaTime) override {
        if (!m_world) return;
        
        // 获取主相机位置
        Vector3 cameraPosition = Vector3::Zero();
        auto camEntities = m_world->Query<CameraComponent, TransformComponent>();
        if (!camEntities.empty()) {
            auto& camTransform = m_world->GetComponent<TransformComponent>(camEntities[0]);
            if (camTransform.transform) {
                cameraPosition = camTransform.transform->GetWorldPosition();
            }
        }
        
        // 获取所有有 LODComponent 的实体
        auto entities = m_world->Query<LODComponent, TransformComponent>();
        if (entities.empty()) return;
        
        // 获取当前帧 ID
        static uint64_t frameId = 0;
        frameId++;
        
        // 批量计算 LOD
        LODSelector::BatchCalculateLOD(entities, m_world, cameraPosition, frameId);
    }
    
    int GetPriority() const override { return 95; }  // 在 MeshRenderSystem 之前运行
};

// 注册系统
world->RegisterSystem<LODUpdateSystem>();
```

#### 3. 查询 LOD 使用情况

```cpp
#include "render/lod_system.h"
using namespace Render::LODDebug;

// 方法 1: 通过系统统计信息
auto* meshSystem = world->GetSystem<MeshRenderSystem>();
if (meshSystem) {
    const auto& stats = meshSystem->GetStats();
    if (stats.lodEnabledEntities > 0) {
        std::cout << "LOD 启用实体数: " << stats.lodEnabledEntities << std::endl;
        std::cout << "LOD0: " << stats.lod0Count << std::endl;
        std::cout << "LOD1: " << stats.lod1Count << std::endl;
        std::cout << "LOD2: " << stats.lod2Count << std::endl;
        std::cout << "LOD3: " << stats.lod3Count << std::endl;
        std::cout << "LOD 剔除: " << stats.lodCulledCount << std::endl;
    }
}

// 方法 2: 查询单个实体的 LOD 状态
EntityID entity = ...;
if (IsLODEnabled(world, entity)) {
    std::string status = GetEntityLODStatus(world, entity);
    std::cout << "实体 LOD 状态: " << status << std::endl;
    
    LODLevel level = GetEntityLODLevel(world, entity);
    switch (level) {
        case LODLevel::LOD0: std::cout << "使用最高细节" << std::endl; break;
        case LODLevel::LOD1: std::cout << "使用中等细节" << std::endl; break;
        case LODLevel::LOD2: std::cout << "使用低细节" << std::endl; break;
        case LODLevel::LOD3: std::cout << "使用最低细节" << std::endl; break;
        case LODLevel::Culled: std::cout << "已被剔除" << std::endl; break;
    }
}
```

#### 4. 配置 LOD 纹理

```cpp
LODComponent lodComp;
lodComp.config.enabled = true;
lodComp.config.distanceThresholds = {50.0f, 150.0f, 500.0f, 1000.0f};
lodComp.config.textureStrategy = TextureLODStrategy::UseLODTextures;

// 为每个 LOD 级别配置纹理
LODTextureSet lod0Textures;
lod0Textures.diffuseMap = LoadTexture("tree_diffuse_4k.png");
lod0Textures.normalMap = LoadTexture("tree_normal_4k.png");
lodComp.config.lodTextures.push_back(lod0Textures);

LODTextureSet lod1Textures;
lod1Textures.diffuseMap = LoadTexture("tree_diffuse_2k.png");
lod1Textures.normalMap = LoadTexture("tree_normal_2k.png");
lodComp.config.lodTextures.push_back(lod1Textures);

LODTextureSet lod2Textures;
lod2Textures.diffuseMap = LoadTexture("tree_diffuse_1k.png");
// LOD2 不使用方法线贴图
lodComp.config.lodTextures.push_back(lod2Textures);

world->AddComponent<LODComponent>(entity, lodComp);
```

#### 5. 批量配置相同类型的实体

```cpp
void ConfigureLODForEntities(
    World* world,
    const std::vector<EntityID>& entities,
    const LODConfig& config
) {
    for (EntityID entity : entities) {
        if (!world->HasComponent<LODComponent>(entity)) {
            LODComponent lodComp;
            lodComp.config = config;
            world->AddComponent<LODComponent>(entity, lodComp);
        }
    }
}

// 使用示例
LODConfig sharedConfig;
sharedConfig.enabled = true;
sharedConfig.distanceThresholds = {50.0f, 150.0f, 500.0f, 1000.0f};

auto treeEntities = world->Query<MeshRenderComponent>();
std::vector<EntityID> treeEntityList(treeEntities.begin(), treeEntities.end());
ConfigureLODForEntities(world.get(), treeEntityList, sharedConfig);
```

---

## 性能优化建议

### 1. 距离阈值配置

根据场景大小和对象类型调整距离阈值：

```cpp
// 小场景（室内）
lodComp.config.distanceThresholds = {20.0f, 50.0f, 100.0f, 200.0f};

// 中等场景（城市）
lodComp.config.distanceThresholds = {50.0f, 150.0f, 500.0f, 1000.0f};

// 大场景（开放世界）
lodComp.config.distanceThresholds = {100.0f, 300.0f, 800.0f, 2000.0f};
```

### 2. 平滑过渡距离

较大的 `transitionDistance` 可以减少 LOD 切换频率，但可能导致视觉上的延迟：

```cpp
// 快速切换（适合动态场景）
lodComp.config.transitionDistance = 5.0f;

// 标准切换
lodComp.config.transitionDistance = 10.0f;

// 平滑切换（适合静态场景）
lodComp.config.transitionDistance = 20.0f;
```

### 3. 包围盒缩放

对于较大的对象，使用更大的 `boundingBoxScale`：

```cpp
// 小对象（角色、道具）
lodComp.config.boundingBoxScale = 1.0f;

// 中等对象（车辆、建筑）
lodComp.config.boundingBoxScale = 1.5f;

// 大对象（地形、大型建筑）
lodComp.config.boundingBoxScale = 2.0f;
```

### 4. 批量更新优化

`LODSelector::BatchCalculateLOD` 已经优化了批量处理。建议：

- 每帧更新一次（或每 N 帧更新一次，通过 `frameId` 控制）
- 在 `MeshRenderSystem` 之前运行 LOD 更新系统（优先级 < 100）
- 使用 `transitionDistance` 避免频繁切换

### 5. 纹理策略选择

- **UseMipmap**（推荐）：自动使用纹理的 mipmap，无需额外配置
- **UseLODTextures**：需要手动配置每个 LOD 级别的纹理，但可以更精确控制
- **DisableTextures**：在 LOD2+ 禁用纹理，节省内存和带宽

---

## 统计信息

### MeshRenderSystem / ModelRenderSystem 统计

渲染系统提供 LOD 统计信息：

```cpp
struct RenderStats {
    size_t visibleMeshes = 0;
    size_t culledMeshes = 0;
    size_t drawCalls = 0;
    
    // LOD 统计信息
    size_t lodEnabledEntities = 0;      ///< 启用 LOD 的实体数量
    size_t lod0Count = 0;               ///< 使用 LOD0 的实体数量
    size_t lod1Count = 0;               ///< 使用 LOD1 的实体数量
    size_t lod2Count = 0;               ///< 使用 LOD2 的实体数量
    size_t lod3Count = 0;               ///< 使用 LOD3 的实体数量
    size_t lodCulledCount = 0;          ///< 被 LOD 剔除的实体数量
};
```

**获取统计信息**:

```cpp
auto* meshSystem = world->GetSystem<MeshRenderSystem>();
if (meshSystem) {
    const auto& stats = meshSystem->GetStats();
    
    // 计算 LOD 使用率
    float lodUsagePercent = (stats.lodEnabledEntities * 100.0f) / 
                           (stats.visibleMeshes + stats.culledMeshes);
    std::cout << "LOD 使用率: " << lodUsagePercent << "%" << std::endl;
}
```

**日志输出**:

系统会自动在日志中输出 LOD 统计信息（每 60 帧一次）：

```
[MeshRenderSystem] Submitted 1052 renderables (total: 2500, culled: 1448) | 
LOD: enabled=1052, LOD0=200, LOD1=300, LOD2=250, LOD3=150, culled=152
```

---

## 常见问题

### Q: 如何知道 LOD 是否在工作？

A: 检查日志输出或统计信息：

```cpp
// 方法 1: 检查日志
// 如果看到 "LOD: enabled=..." 这一行，说明 LOD 正在工作

// 方法 2: 检查统计信息
auto* meshSystem = world->GetSystem<MeshRenderSystem>();
const auto& stats = meshSystem->GetStats();
if (stats.lodEnabledEntities > 0) {
    // LOD 正在工作
}

// 方法 3: 查询单个实体
if (LODDebug::IsLODEnabled(world, entity)) {
    std::string status = LODDebug::GetEntityLODStatus(world, entity);
    std::cout << status << std::endl;
}
```

### Q: LOD 切换太频繁怎么办？

A: 增加 `transitionDistance`：

```cpp
lodComp.config.transitionDistance = 20.0f;  // 从默认的 10.0f 增加到 20.0f
```

### Q: 如何为不同大小的对象设置不同的 LOD 距离？

A: 使用 `boundingBoxScale`：

```cpp
// 小对象
lodComp.config.boundingBoxScale = 1.0f;
lodComp.config.distanceThresholds = {50.0f, 150.0f, 500.0f, 1000.0f};

// 大对象
lodComp.config.boundingBoxScale = 2.0f;
lodComp.config.distanceThresholds = {100.0f, 300.0f, 1000.0f, 2000.0f};
```

### Q: 如何禁用某个实体的 LOD？

A: 设置 `enabled = false` 或移除 `LODComponent`：

```cpp
// 方法 1: 禁用 LOD
if (world->HasComponent<LODComponent>(entity)) {
    auto& lodComp = world->GetComponent<LODComponent>(entity);
    lodComp.config.enabled = false;
}

// 方法 2: 移除 LODComponent
world->RemoveComponent<LODComponent>(entity);
```

### Q: LOD 网格/模型/材质是必需的吗？

A: 不是。如果没有配置 LOD 资源，系统会使用原始资源。只有配置了 LOD 资源时才会使用。

### Q: LOD 不同级别的网格是如何得到的？中等级别是否降低了网格数量？

A: 是的，LOD 不同级别通过降低网格复杂度来实现：

**当前实现（阶段 1）**：
- 需要手动在建模工具中创建不同 LOD 级别的模型文件
- LOD1 通常减少 30-50% 的顶点和三角形
- LOD2 通常减少 60-80% 的顶点和三角形
- LOD3 通常减少 80-90% 的顶点和三角形

**简化方法**：
1. **建模工具简化**（推荐）：
   - Blender: 使用 Decimate Modifier
   - Maya: 使用 Progressive Mesh
   - 3ds Max: 使用 ProOptimizer

2. **自动生成**（计划中，阶段 3.1）：
   - 使用网格简化算法（如 meshoptimizer）
   - 自动从基础网格生成不同 LOD 级别

**示例**：
```cpp
// LOD0: 10000 顶点（原始模型）
// LOD1: 7000 顶点（减少 30%）
// LOD2: 4000 顶点（减少 60%）
// LOD3: 2000 顶点（减少 80%）
```

详见文档中的 "LOD 资源获取方式" 章节。

---

## 相关文档

- [Mesh API](Mesh.md) - 网格系统
- [Model API](Model.md) - 模型系统
- [Material API](Material.md) - 材质系统
- [ECS Integration](../ECS_INTEGRATION.md) - ECS 系统集成指南
- [LOD 优化方案](../todolists/LOD_Instanced_Rendering_Optimization.md) - LOD 系统设计文档

---

**文档版本**: v1.0  
**最后更新**: 2025-11-28  
**对应代码版本**: RenderEngine v1.0.0

