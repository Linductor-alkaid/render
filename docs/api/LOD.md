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
- ✅ **实例化渲染**：支持 LOD 实例化渲染，大幅减少 Draw Call（阶段2.2）
- ✅ **批处理集成**：与批处理系统无缝集成，支持优先级和回退机制（阶段2.3）
- ✅ **视锥体裁剪优化**：结合视锥体裁剪和 LOD 选择，进一步提升性能（阶段3.3）
- ✅ **双缓冲渲染**：使用双缓冲策略，消除清空和重建开销，提升30-50%性能（阶段3.1）
- ✅ **多线程数据准备**：后台线程准备实例数据，主线程专注GPU上传和渲染，提升20-40%性能（阶段3.2）
- ✅ **GPU剔除**：使用Compute Shader在GPU上进行视锥剔除和LOD选择，避免CPU-GPU往返，提升50-200%性能（阶段3.3）

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
LOD 切换的平滑过渡距离（单位：世界单位）。**注意**：当前版本中，LOD 级别会在距离跨过阈值时立即切换，此字段保留用于未来可能的平滑过渡功能。切换基于距离阈值，与远距离剔除保持一致。

#### boundingBoxScale
包围盒缩放因子，用于考虑对象大小的距离计算。较大的对象应该使用更大的距离阈值。

#### enabled
是否启用 LOD。如果为 `false`，始终使用 LOD0（最高细节）。

#### frustumOutBehavior（阶段3.3扩展）
视锥体外的行为模式，控制视锥体外的实体如何处理：
- `Cull`（默认）：完全剔除，不渲染（性能最好）
- `UseLowerLOD`：使用更低的LOD级别，降低细节但保持光影效果
- `UseMinimalLOD`：使用最低LOD级别（LOD3），极简渲染

**使用场景**：
- `Cull`：适合大多数情况，完全剔除视锥体外的物体
- `UseLowerLOD`：需要保持光影效果时（如阴影投射），降低细节但继续渲染
- `UseMinimalLOD`：需要极简渲染时，使用最低LOD级别

#### frustumOutLODReduction（阶段3.3扩展）
当 `frustumOutBehavior == UseLowerLOD` 时，降低多少级LOD。默认值：2（降低2级）

**示例**：
- 如果正常是LOD1，降级2级后使用LOD3
- 如果正常是LOD0，降级2级后使用LOD2

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
- `affectedByFrustumCulling` - 是否受视锥体裁剪影响（阶段3.3扩展）
  - 如果为 `false`，该实体不受视锥体裁剪影响，始终按距离计算LOD
  - 用于重要的物体（如UI、特效等）需要保证渲染
  - 默认值：`true`（受视锥体裁剪影响）

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
- LOD 级别会在距离跨过阈值时立即切换，与远距离剔除保持一致
- 每帧都会计算 LOD，确保实时响应相机位置变化

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

### LODFrustumCullingSystem（阶段3.3）

LOD 视锥体裁剪系统，结合视锥体裁剪和 LOD 选择，进一步提升性能。系统先进行视锥体裁剪，只对可见实体计算 LOD，减少不必要的计算。

```cpp
class LODFrustumCullingSystem {
public:
    /**
     * @brief 批量进行视锥体裁剪和 LOD 选择
     * @param entities 实体列表
     * @param world ECS World 对象指针
     * @param camera 相机对象
     * @param frameId 当前帧 ID
     * @return 可见实体列表（按 LOD 分组）
     */
    static std::map<LODLevel, std::vector<ECS::EntityID>> BatchCullAndSelectLOD(
        const std::vector<ECS::EntityID>& entities,
        ECS::World* world,
        const Camera* camera,
        uint64_t frameId
    );
    
    /**
     * @brief 批量进行视锥体裁剪和 LOD 选择（考虑包围盒）
     * @param entities 实体列表
     * @param world ECS World 对象指针
     * @param camera 相机对象
     * @param frameId 当前帧 ID
     * @param getBounds 函数，用于获取实体的包围盒
     * @return 可见实体列表（按 LOD 分组）
     */
    static std::map<LODLevel, std::vector<ECS::EntityID>> BatchCullAndSelectLODWithBounds(
        const std::vector<ECS::EntityID>& entities,
        ECS::World* world,
        const Camera* camera,
        uint64_t frameId,
        std::function<AABB(ECS::EntityID)> getBounds
    );
};
```

**方法说明**:

#### BatchCullAndSelectLOD
```cpp
static std::map<LODLevel, std::vector<ECS::EntityID>> BatchCullAndSelectLOD(
    const std::vector<ECS::EntityID>& entities,
    ECS::World* world,
    const Camera* camera,
    uint64_t frameId
);
```
批量进行视锥体裁剪和 LOD 选择。先进行视锥体裁剪，只对可见实体计算 LOD，提升性能。

**参数**:
- `entities` - 实体列表
- `world` - ECS World 对象指针
- `camera` - 相机对象（用于获取视锥体和位置）
- `frameId` - 当前帧 ID（用于避免重复计算）

**返回**: 按 LOD 级别分组的可见实体列表

**性能优化**:
- 先进行视锥体裁剪，减少需要计算 LOD 的实体数量
- 批量计算距离和 LOD 级别，提升缓存命中率
- 使用帧 ID 避免重复计算
- 近距离保护：相机 5 米内的物体不进行视锥体裁剪
- 扩大安全边距：包围球半径扩大 2.5 倍，避免边缘物体被过度剔除

**注意**:
- 如果实体没有 `LODComponent`，会被归类到 `LODLevel::LOD0`
- 如果实体不在视锥体内，会被跳过（不包含在结果中）
- 如果实体的 LOD 级别是 `Culled`，会被跳过（不包含在结果中）

#### BatchCullAndSelectLODWithBounds
```cpp
static std::map<LODLevel, std::vector<ECS::EntityID>> BatchCullAndSelectLODWithBounds(
    const std::vector<ECS::EntityID>& entities,
    ECS::World* world,
    const Camera* camera,
    uint64_t frameId,
    std::function<AABB(ECS::EntityID)> getBounds
);
```
批量进行视锥体裁剪和 LOD 选择（使用包围盒版本）。与 `BatchCullAndSelectLOD` 类似，但使用包围盒进行更准确的视锥体裁剪和距离计算。

**参数**:
- `entities` - 实体列表
- `world` - ECS World 对象指针
- `camera` - 相机对象
- `frameId` - 当前帧 ID
- `getBounds` - 函数，用于获取实体的包围盒（世界空间）

**返回**: 按 LOD 级别分组的可见实体列表

**注意**: `getBounds` 函数签名：`AABB(EntityID entity)`

---

## LOD 资源获取方式

### 概述

LOD 不同级别的网格/模型资源可以通过以下方式获得：

1. **手动准备**（当前支持）：在建模工具中创建不同细节级别的模型文件
2. **自动生成**（✅ 已实现）：使用 `LODGenerator` 通过网格简化算法自动生成 LOD 级别
3. **运行时简化**（✅ 已实现）：使用 `meshoptimizer` 库进行网格简化

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

#### 2. 自动简化算法（✅ 已实现）

使用 `LODGenerator` 类自动生成 LOD 级别：

```cpp
#include <render/lod_generator.h>

// 方法 1: 生成所有 LOD 级别
Ref<Mesh> sourceMesh = LoadMesh("tree.obj");
auto lodMeshes = LODGenerator::GenerateLODLevels(sourceMesh);

// lodMeshes[0] = LOD1, lodMeshes[1] = LOD2, lodMeshes[2] = LOD3
if (lodMeshes[0] && lodMeshes[1] && lodMeshes[2]) {
    // 配置 LOD
    LODComponent lodComp;
    lodComp.config.enabled = true;
    lodComp.config.distanceThresholds = {50.0f, 150.0f, 500.0f, 1000.0f};
    
    // 设置 LOD 网格
    lodComp.config.lodMeshes.push_back(sourceMesh);  // LOD0
    lodComp.config.lodMeshes.push_back(lodMeshes[0]); // LOD1
    lodComp.config.lodMeshes.push_back(lodMeshes[1]); // LOD2
    lodComp.config.lodMeshes.push_back(lodMeshes[2]); // LOD3
    
    world->AddComponent<LODComponent>(entity, lodComp);
}

// 方法 2: 自动配置 LODConfig（推荐）
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

// 方法 3: 自定义简化选项
LODGenerator::SimplifyOptions options;
options.mode = LODGenerator::SimplifyOptions::Mode::TargetTriangleCount;
options.triangleCounts.lod1 = 1000;  // LOD1 保留 1000 个三角形
options.triangleCounts.lod2 = 500;   // LOD2 保留 500 个三角形
options.triangleCounts.lod3 = 200;   // LOD3 保留 200 个三角形
options.flags = LODGenerator::SimplifyOptions::LockBorder;  // 锁定边界

auto lodMeshes = LODGenerator::GenerateLODLevels(sourceMesh, options);
```

**简化算法**：
- 使用 **meshoptimizer** 库（已集成）
- 支持目标三角形数量模式（推荐）
- 支持目标误差模式
- 支持属性保留（法线、纹理坐标、颜色等）

**详细文档**：参见 [LODGenerator API 文档](LODGenerator.md)

#### 3. 运行时简化（✅ 已实现）

对于动态生成的几何体，可以在运行时使用 `LODGenerator` 简化：

```cpp
#include <render/lod_generator.h>

// 运行时简化动态生成的网格
Ref<Mesh> baseMesh = GenerateComplexMesh();

// 生成 LOD 级别
auto lodMeshes = LODGenerator::GenerateLODLevels(baseMesh);

// 配置 LOD
LODComponent lodComp;
lodComp.config.enabled = true;
lodComp.config.distanceThresholds = {50.0f, 150.0f, 500.0f, 1000.0f};
lodComp.config.lodMeshes.push_back(baseMesh);  // LOD0
lodComp.config.lodMeshes.push_back(lodMeshes[0]); // LOD1
lodComp.config.lodMeshes.push_back(lodMeshes[1]); // LOD2
lodComp.config.lodMeshes.push_back(lodMeshes[2]); // LOD3

world->AddComponent<LODComponent>(entity, lodComp);
```

**注意**：
- 网格简化是 CPU 密集型操作，对于大型网格可能需要较长时间
- 建议在加载时或后台线程中生成 LOD
- 可以预先生成并保存到文件，运行时直接加载（参见 `LODGenerator::SaveLODMeshesToFiles`）

**详细文档**：参见 [LODGenerator API 文档](LODGenerator.md)

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

#### 方法 1: 手动配置（基础）

```cpp
// 使用原始纹理的 mipmap（自动）
lodComp.config.textureStrategy = TextureLODStrategy::UseMipmap;

// 系统会自动使用纹理的 mipmap 级别：
// - LOD0: 使用最高 mipmap 级别
// - LOD1: 使用中等 mipmap 级别
// - LOD2: 使用较低 mipmap 级别
// - LOD3: 使用最低 mipmap 级别
```

#### 方法 2: 使用 LODGenerator 自动配置（推荐）

使用 `LODGenerator` 可以自动为材质和模型配置纹理 mipmap：

```cpp
#include <render/lod_generator.h>

// 方式 A: 为单个材质配置纹理 LOD
Ref<Material> material = LoadMaterial("tree.mat");
if (LODGenerator::AutoConfigureTextureLOD(material)) {
    // 材质的所有纹理现在都支持 mipmap LOD
    lodComp.config.textureStrategy = TextureLODStrategy::UseMipmap;
}

// 方式 B: 为整个模型配置纹理 LOD
Ref<Model> model = ModelLoader::LoadFromFile("tree.pmx", "tree").model;
if (LODGenerator::ConfigureModelTextureLOD(model)) {
    // 模型的所有材质纹理都支持 mipmap LOD
    lodComp.config.textureStrategy = TextureLODStrategy::UseMipmap;
}

// 方式 C: 自动配置 LODConfig 的纹理策略（最简单）
LODConfig config;
config.enabled = true;
config.distanceThresholds = {50.0f, 150.0f, 500.0f, 1000.0f};

Ref<Material> material = LoadMaterial("tree.mat");
if (LODGenerator::AutoConfigureTextureLODStrategy(config, material)) {
    // config.textureStrategy 已设置为 UseMipmap
    // material 的所有纹理都已配置 mipmap
    lodComp.config = config;
}
```

**优势**：
- ✅ 自动为所有纹理生成 mipmap
- ✅ 自动设置正确的过滤模式（三线性过滤）
- ✅ 支持材质和模型的批量配置
- ✅ 无需手动准备多个纹理文件

**详细文档**：参见 [LODGenerator API 文档](LODGenerator.md) 中的"纹理 LOD（使用 Mipmap）"章节。

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

##### 方式 A: 使用 Mipmap（推荐，最简单）

```cpp
#include <render/lod_generator.h>

LODComponent lodComp;
lodComp.config.enabled = true;
lodComp.config.distanceThresholds = {50.0f, 150.0f, 500.0f, 1000.0f};

// 使用 LODGenerator 自动配置纹理 LOD
Ref<Material> material = LoadMaterial("tree.mat");
if (LODGenerator::AutoConfigureTextureLODStrategy(lodComp.config, material)) {
    // config.textureStrategy 已设置为 UseMipmap
    // material 的所有纹理都已配置 mipmap
    world->AddComponent<LODComponent>(entity, lodComp);
}
```

##### 方式 B: 手动配置不同分辨率的纹理

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

**推荐使用方式 A**，因为：
- 无需手动准备多个纹理文件
- 自动生成 mipmap，OpenGL 根据距离自动选择级别
- 性能优化，减少内存带宽
- 视觉质量好，平滑的纹理细节过渡

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

#### 6. 启用 LOD 视锥体裁剪优化（阶段3.3）

##### 方式 A: 使用 MeshRenderSystem 设置（推荐）

```cpp
#include "render/ecs/systems.h"

// 获取 MeshRenderSystem
auto* meshSystem = world->GetSystem<MeshRenderSystem>();
if (meshSystem) {
    // 启用 LOD 视锥体裁剪优化
    meshSystem->SetLODFrustumCullingEnabled(true);
    
    // 检查是否启用
    if (meshSystem->IsLODFrustumCullingEnabled()) {
        std::cout << "LOD 视锥体裁剪优化已启用" << std::endl;
    }
}
```

**优势**:
- 先进行视锥体裁剪，只对可见实体计算 LOD，减少不必要的计算
- 批量处理，提升缓存命中率
- 近距离保护：相机 5 米内的物体不进行视锥体裁剪
- 扩大安全边距：包围球半径扩大 2.5 倍，避免边缘物体被过度剔除

**注意**:
- 需要启用 LOD 实例化渲染才能使用此优化
- 如果 LOD 实例化渲染不可用，会自动回退到原始逻辑

##### 方式 B: 直接使用 LODFrustumCullingSystem

```cpp
#include "render/lod_system.h"

// 获取主相机
Camera* mainCamera = cameraSystem->GetMainCameraObject();
if (mainCamera) {
    // 获取所有需要测试的实体
    auto entities = world->Query<TransformComponent, MeshRenderComponent>();
    std::vector<EntityID> entityList(entities.begin(), entities.end());
    
    // 使用 LODFrustumCullingSystem 进行批量视锥体裁剪和 LOD 选择
    static uint64_t frameId = 0;
    frameId++;
    
    auto visibleEntitiesByLOD = LODFrustumCullingSystem::BatchCullAndSelectLOD(
        entityList,
        world.get(),
        mainCamera,
        frameId
    );
    
    // 处理可见实体（按 LOD 级别分组）
    for (const auto& [lodLevel, visibleEntities] : visibleEntitiesByLOD) {
        for (EntityID entity : visibleEntities) {
            // 处理可见实体...
        }
    }
}
```

#### 7. 启用 LOD 实例化渲染（阶段2.2 + 阶段2.3）

##### 方式 A: 使用 Renderer 级别设置（推荐，阶段2.3）

```cpp
#include "render/renderer.h"

// 在 Renderer 级别启用 LOD 实例化渲染
Renderer* renderer = Renderer::Create();
renderer->Initialize("MyApp", 1920, 1080);

// 启用 LOD 实例化渲染
renderer->SetLODInstancingEnabled(true);

// 检查是否可用
if (renderer->IsLODInstancingAvailable()) {
    std::cout << "LOD 实例化渲染可用" << std::endl;
}

// 获取统计信息
auto stats = renderer->GetLODInstancingStats();
std::cout << "LOD 组数: " << stats.lodGroupCount << std::endl;
std::cout << "总实例数: " << stats.totalInstances << std::endl;
std::cout << "Draw Calls: " << stats.drawCalls << std::endl;
```

##### 方式 B: 使用 MeshRenderSystem 设置（向后兼容）

```cpp
#include "render/ecs/systems.h"

// 获取 MeshRenderSystem
auto* meshSystem = world->GetSystem<MeshRenderSystem>();
if (meshSystem) {
    // 阶段2.3：此方法会同步到 Renderer 的设置
    meshSystem->SetLODInstancingEnabled(true);
    
    // 检查是否启用（从 Renderer 获取）
    if (meshSystem->IsLODInstancingEnabled()) {
        std::cout << "LOD 实例化渲染已启用" << std::endl;
    }
}
```

##### 与批处理模式集成（阶段2.3）

```cpp
// LOD 实例化渲染可以与任何批处理模式共存
renderer->SetBatchingMode(BatchingMode::GpuInstancing);  // 或 Disabled, CpuMerge
renderer->SetLODInstancingEnabled(true);

// LOD 实例化渲染优先级高于批处理系统
// 如果 LOD 实例化可用，将优先使用 LOD 实例化渲染
// 如果不可用，将回退到批处理模式

// 检查兼容性
if (renderer->IsLODInstancingAvailable()) {
    std::cout << "LOD 实例化渲染可用，将优先使用" << std::endl;
} else {
    std::cout << "LOD 实例化不可用，将使用批处理模式" << std::endl;
}
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

### 2. 距离阈值配置（平滑过渡）

通过调整距离阈值的间隔来控制 LOD 切换的平滑度。较大的阈值间隔可以减少切换频率：

```cpp
// 快速切换（较小的阈值间隔，适合动态场景）
lodComp.config.distanceThresholds = {30.0f, 80.0f, 200.0f, 500.0f};

// 标准切换
lodComp.config.distanceThresholds = {50.0f, 150.0f, 500.0f, 1000.0f};

// 平滑切换（较大的阈值间隔，适合静态场景）
lodComp.config.distanceThresholds = {100.0f, 300.0f, 800.0f, 2000.0f};
```

**注意**：当前版本的 LOD 系统会在距离跨过阈值时立即切换，与远距离剔除保持一致。`transitionDistance` 字段保留用于未来可能的平滑过渡功能。

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

- 每帧更新一次（与远距离剔除保持一致，确保实时响应）
- 在 `MeshRenderSystem` 之前运行 LOD 更新系统（优先级 < 100）
- 通过调整距离阈值间隔来控制切换平滑度（见"平滑过渡距离"章节）

### 5. 纹理策略选择

- **UseMipmap**（推荐）：自动使用纹理的 mipmap，无需额外配置
  - 使用 `LODGenerator::AutoConfigureTextureLOD` 或 `AutoConfigureTextureLODStrategy` 自动配置
  - OpenGL 根据距离自动选择合适的 mipmap 级别
  - 性能优化，减少内存带宽
  - 详细文档：参见 [LODGenerator API 文档](LODGenerator.md) 中的"纹理 LOD（使用 Mipmap）"章节
- **UseLODTextures**：需要手动配置每个 LOD 级别的纹理，但可以更精确控制
  - 需要手动准备不同分辨率的纹理文件
  - 可以精确控制每个 LOD 级别使用的纹理
- **DisableTextures**：在 LOD2+ 禁用纹理，节省内存和带宽
  - 适合极简渲染场景
  - 可以显著减少纹理采样开销

### 6. 双缓冲渲染策略（阶段3.1）

双缓冲渲染策略是系统内部自动启用的优化，无需用户配置。系统使用两个独立的组集合，一个用于渲染当前帧，另一个用于构建下一帧，完全消除清空和重建的开销。

**优势**:
- 零延迟切换：只需交换索引，无需数据拷贝
- 完全并行：CPU构建下一帧数据时，GPU渲染当前帧
- 内存效率：复用缓冲区，减少分配/释放

**性能提升**: 在极端场景（100K+ 实例）下，预期带来 **30-50%** 的性能提升。

**注意**: 双缓冲策略是内部实现，自动启用，无需用户配置。

### 7. 多线程数据准备（阶段3.2）

对于大量实例的场景（1000+），可以考虑启用多线程数据准备。系统会在后台线程准备实例数据，主线程仅负责GPU上传和渲染。

**使用建议**:
- 默认情况下，多线程功能是禁用的
- 对于大量实例的场景（1000+），可以考虑启用多线程
- 系统会自动检测实例数量，超过阈值（100个实例）时才会使用多线程处理
- 小场景可能不会带来明显提升，甚至可能因为线程同步开销而降低性能

**性能提升**: 在大量实例场景（1000+）下，预期带来 **20-40%** 的性能提升。

**注意**: 
- 多线程优化需要非常小心的同步设计
- 建议先实现前面的优化，确认性能瓶颈后再考虑
- 多线程主要用于处理大量实例的场景

### 8. LOD 视锥体裁剪优化（阶段3.3）

启用 LOD 视锥体裁剪优化可以进一步提升性能，特别是在大量实体的场景中：

```cpp
// 启用 LOD 视锥体裁剪优化
auto* meshSystem = world->GetSystem<MeshRenderSystem>();
if (meshSystem) {
    meshSystem->SetLODFrustumCullingEnabled(true);
}
```

**性能提升**:
- 先进行视锥体裁剪，只对可见实体计算 LOD，减少不必要的计算
- 批量处理，提升缓存命中率
- 在大量实体的场景中，可以显著减少 CPU 开销

**优化细节**:
- **近距离保护**：相机 5 米内的物体不进行视锥体裁剪，避免误剔除
- **扩大安全边距**：包围球半径扩大 2.5 倍，避免边缘物体（尤其是下边和左右两边）被过度剔除
- **包围球计算**：使用包围盒对角线的一半作为半径，考虑 Transform 的缩放

**使用建议**:
- 在大量实体的场景中（1000+ 实体）启用此优化
- 需要启用 LOD 实例化渲染才能使用此优化
- 如果发现物体被过度剔除，可以调整安全边距（修改代码中的 `2.5f` 系数）

**视锥体外的行为配置**（阶段3.3扩展）:
- **完全剔除**（`Cull`，默认）：性能最好，适合大多数情况
- **使用更低LOD**（`UseLowerLOD`）：降低细节但保持光影效果，适合需要阴影投射的场景
- **使用最低LOD**（`UseMinimalLOD`）：极简渲染，适合需要极简渲染的场景
- **不受视锥体裁剪影响**（`affectedByFrustumCulling = false`）：用于重要的物体需要保证渲染

### 9. GPU剔除（Compute Shader，阶段3.3）

GPU剔除使用Compute Shader在GPU上进行视锥剔除和LOD选择，完全避免CPU-GPU往返，在大量实例场景（10K+）下可以带来显著的性能提升。

**工作原理**:
1. 将所有实例的变换矩阵上传到GPU的SSBO（Shader Storage Buffer Object）
2. 在GPU上并行执行视锥剔除和LOD选择计算
3. 使用原子操作收集可见实例索引，按LOD级别分组
4. CPU读取结果，使用可见实例列表进行渲染

**优势**:
- ✅ **完全并行**：GPU的数千个核心同时处理，远超CPU性能
- ✅ **零CPU开销**：视锥剔除和LOD选择完全在GPU上执行
- ✅ **避免往返**：不需要CPU-GPU数据传输，减少延迟
- ✅ **可扩展性**：性能随实例数量线性扩展，适合超大规模场景

**性能提升**: 在大量实例场景（10K+）下，预期带来 **50-200%** 的性能提升。

**使用示例**:

```cpp
// 获取 MeshRenderSystem
auto* meshSystem = world->GetSystem<MeshRenderSystem>();
if (meshSystem) {
    // 检查GPU剔除是否可用（需要OpenGL 4.3+）
    if (meshSystem->IsLODGPUCullingAvailable()) {
        // 启用GPU剔除
        meshSystem->EnableLODGPUCulling(true);
        
        // 检查是否启用
        if (meshSystem->IsLODGPUCullingEnabled()) {
            std::cout << "GPU剔除已启用" << std::endl;
        }
    } else {
        std::cout << "GPU剔除不可用（需要OpenGL 4.3+）" << std::endl;
    }
}
```

**系统要求**:
- **OpenGL版本**：需要OpenGL 4.3+支持Compute Shader
- **实例数量**：建议在10K+实例的场景中使用，小场景可能不会带来明显提升
- **显存**：需要足够的显存存储所有实例的矩阵数据

**使用建议**:
- 默认情况下，GPU剔除功能是禁用的
- 对于大量实例的场景（10K+），可以考虑启用GPU剔除
- 系统会自动检测OpenGL版本，如果不支持会显示警告并禁用
- GPU剔除可以与CPU视锥体裁剪优化同时使用，但通常只需要使用一种

**注意**: 
- GPU剔除需要OpenGL 4.3+支持Compute Shader
- 如果系统不支持，会自动回退到CPU剔除
- GPU剔除主要用于处理大量实例的场景（10K+），小场景可能不会带来明显提升
- 所有实例的矩阵数据需要上传到GPU，需要足够的显存

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

### Renderer 级别的 LOD 实例化统计（阶段2.3）

`Renderer` 提供 LOD 实例化渲染的统计信息，用于性能分析和调试：

```cpp
struct LODInstancingStats {
    size_t lodGroupCount = 0;        ///< LOD 组数量（按网格、材质、LOD级别分组）
    size_t totalInstances = 0;       ///< 总实例数
    size_t drawCalls = 0;            ///< Draw Call 数量（每个组一次）
    size_t lod0Instances = 0;        ///< LOD0 实例数
    size_t lod1Instances = 0;        ///< LOD1 实例数
    size_t lod2Instances = 0;        ///< LOD2 实例数
    size_t lod3Instances = 0;        ///< LOD3 实例数
    size_t culledCount = 0;          ///< 剔除数量
    
    // ✅ 阶段3.1 + 3.2 新增性能指标
    size_t vboUploadCount = 0;        ///< VBO上传次数
    size_t bytesUploaded = 0;         ///< 总上传字节数
    float uploadTimeMs = 0.0f;       ///< 上传耗时(ms)
    size_t pendingCount = 0;          ///< 待处理实例数
    float sortTimeMs = 0.0f;         ///< 排序耗时(ms)
    float renderTimeMs = 0.0f;       ///< 渲染耗时(ms)
    size_t totalAllocatedMemory = 0;  ///< 总分配内存(bytes)
    size_t peakInstanceCount = 0;     ///< 峰值实例数
};
```

**获取统计信息**:

```cpp
#include "render/renderer.h"

Renderer* renderer = ...;

// 获取 LOD 实例化统计信息
auto stats = renderer->GetLODInstancingStats();

std::cout << "LOD 组数: " << stats.lodGroupCount << std::endl;
std::cout << "总实例数: " << stats.totalInstances << std::endl;
std::cout << "Draw Calls: " << stats.drawCalls << std::endl;
std::cout << "LOD0: " << stats.lod0Instances << std::endl;
std::cout << "LOD1: " << stats.lod1Instances << std::endl;
std::cout << "LOD2: " << stats.lod2Instances << std::endl;
std::cout << "LOD3: " << stats.lod3Instances << std::endl;

// 计算性能提升
if (stats.totalInstances > 0 && stats.drawCalls > 0) {
    float reduction = (1.0f - static_cast<float>(stats.drawCalls) / stats.totalInstances) * 100.0f;
    std::cout << "Draw Call 减少: " << reduction << "%" << std::endl;
}

// ✅ 阶段3.1 + 3.2：显示详细性能指标
std::cout << "上传次数: " << stats.vboUploadCount << std::endl;
std::cout << "上传字节数: " << stats.bytesUploaded / (1024.0f * 1024.0f) << " MB" << std::endl;
std::cout << "上传耗时: " << stats.uploadTimeMs << " ms" << std::endl;
std::cout << "排序耗时: " << stats.sortTimeMs << " ms" << std::endl;
std::cout << "渲染耗时: " << stats.renderTimeMs << " ms" << std::endl;
std::cout << "待处理实例数: " << stats.pendingCount << std::endl;
std::cout << "总分配内存: " << stats.totalAllocatedMemory / (1024.0f * 1024.0f) << " MB" << std::endl;
std::cout << "峰值实例数: " << stats.peakInstanceCount << std::endl;

```

**示例输出**:

```
LOD 组数: 2
总实例数: 100
Draw Calls: 2
LOD0: 47
LOD1: 53
Draw Call 减少: 98%
```

**说明**:
- `lodGroupCount`: 按网格、材质、LOD级别分组的组数，每个组对应一次 Draw Call
- `totalInstances`: 所有实例的总数
- `drawCalls`: 实际的 Draw Call 数量（通常远小于实例数）
- 性能提升：100个实例仅需2个 Draw Call，减少了98%的 Draw Call

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

A: 调整距离阈值，使阈值间隔更大：

```cpp
// 增大阈值间隔，使切换不那么频繁
lodComp.config.distanceThresholds = {50.0f, 150.0f, 500.0f, 1000.0f};  // 间隔较小
lodComp.config.distanceThresholds = {100.0f, 300.0f, 800.0f, 2000.0f};  // 间隔较大，切换更平滑
```

**说明**：当前版本的 LOD 系统会在距离跨过阈值时立即切换，与远距离剔除保持一致。如果需要更平滑的过渡，可以增大阈值之间的间隔。

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

### Q: 如何使用 LOD 实例化渲染？

A: 在 Renderer 级别启用 LOD 实例化渲染（阶段2.3）：

```cpp
// 启用 LOD 实例化渲染
renderer->SetLODInstancingEnabled(true);

// 检查是否可用
if (renderer->IsLODInstancingAvailable()) {
    // LOD 实例化渲染可用，将自动使用
}
```

**优势**:
- 大幅减少 Draw Call（100个实例可能只需2-10个 Draw Call）
- 自动按 LOD 级别分组，相同 LOD 级别的实例批量渲染
- 与批处理系统兼容，可以共存

**注意**:
- LOD 实例化渲染优先级高于批处理系统
- 如果 LOD 实例化不可用，会自动回退到批处理模式
- 需要实体配置 `LODComponent` 才能使用 LOD 实例化渲染

### Q: 如何使用 LOD 视锥体裁剪优化（阶段3.3）？

A: 在 MeshRenderSystem 中启用 LOD 视锥体裁剪优化：

```cpp
// 获取 MeshRenderSystem
auto* meshSystem = world->GetSystem<MeshRenderSystem>();
if (meshSystem) {
    // 启用 LOD 视锥体裁剪优化
    meshSystem->SetLODFrustumCullingEnabled(true);
    
    // 检查是否启用
    if (meshSystem->IsLODFrustumCullingEnabled()) {
        std::cout << "LOD 视锥体裁剪优化已启用" << std::endl;
    }
}
```

**优势**:
- 先进行视锥体裁剪，只对可见实体计算 LOD，减少不必要的计算
- 批量处理，提升缓存命中率
- 在大量实体的场景中，可以显著减少 CPU 开销

**注意**:
- 需要启用 LOD 实例化渲染才能使用此优化
- 如果 LOD 实例化渲染不可用，会自动回退到原始逻辑

### Q: 为什么部分物体在还能看到的时候就被剔除了？

A: 这是视锥体裁剪过度剔除的问题。系统已经实现了以下优化来避免此问题：

1. **近距离保护**：相机 5 米内的物体不进行视锥体裁剪
2. **扩大安全边距**：包围球半径扩大 2.5 倍，避免边缘物体被过度剔除
3. **准确的包围球计算**：使用包围盒对角线的一半作为半径，考虑 Transform 的缩放

如果仍然出现过度剔除，可以：
- 检查物体的包围盒是否正确
- 检查 Transform 的缩放是否正确
- 调整安全边距（修改代码中的 `2.5f` 系数，在 `LODFrustumCullingSystem::BatchCullAndSelectLOD` 中）

### Q: LOD 实例化渲染与批处理模式的关系？

A: LOD 实例化渲染是独立系统，可以与任何批处理模式共存（阶段2.3）：

```cpp
// 可以同时设置批处理模式和 LOD 实例化
renderer->SetBatchingMode(BatchingMode::GpuInstancing);
renderer->SetLODInstancingEnabled(true);

// 优先级：LOD 实例化渲染 > 批处理系统
// 如果 LOD 实例化可用，优先使用 LOD 实例化渲染
// 如果不可用，回退到批处理模式
```

**兼容性**:
- `BatchingMode::Disabled`: LOD 实例化仍可使用
- `BatchingMode::CpuMerge`: LOD 实例化仍可使用
- `BatchingMode::GpuInstancing`: LOD 实例化仍可使用，两者可以协同工作

### Q: LOD 不同级别的网格是如何得到的？中等级别是否降低了网格数量？

A: 是的，LOD 不同级别通过降低网格复杂度来实现：

**简化方法**：
1. **建模工具简化**：
   - Blender: 使用 Decimate Modifier
   - Maya: 使用 Progressive Mesh
   - 3ds Max: 使用 ProOptimizer

2. **自动生成**（✅ 已实现，推荐）：
   - 使用 `LODGenerator` 和 `meshoptimizer` 库自动生成 LOD 级别
   - 自动从基础网格生成不同 LOD 级别
   - 支持自定义简化选项和属性保留

**使用 LODGenerator 自动生成**：

```cpp
#include <render/lod_generator.h>

// 方法 1: 自动生成并配置（最简单）
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

// 方法 2: 手动生成并配置
auto lodMeshes = LODGenerator::GenerateLODLevels(sourceMesh);
// lodMeshes[0] = LOD1, lodMeshes[1] = LOD2, lodMeshes[2] = LOD3

LODComponent lodComp;
lodComp.config.enabled = true;
lodComp.config.distanceThresholds = {50.0f, 150.0f, 500.0f, 1000.0f};
lodComp.config.lodMeshes.push_back(sourceMesh);  // LOD0
lodComp.config.lodMeshes.push_back(lodMeshes[0]); // LOD1
lodComp.config.lodMeshes.push_back(lodMeshes[1]); // LOD2
lodComp.config.lodMeshes.push_back(lodMeshes[2]); // LOD3
world->AddComponent<LODComponent>(entity, lodComp);
```

**示例输出**：
```cpp
// LOD0: 10000 顶点（原始模型）
// LOD1: 5000 顶点（减少 50%，自动计算）
// LOD2: 2500 顶点（减少 75%，自动计算）
// LOD3: 1000 顶点（减少 90%，自动计算）
```

**详细文档**：参见 [LODGenerator API 文档](LODGenerator.md) 和文档中的 "LOD 资源获取方式" 章节。

### Q: 如何启用多线程数据准备（阶段3.2）？

A: 多线程数据准备功能通过 `LODInstancedRenderer` 类提供。由于 `LODInstancedRenderer` 是内部类，通常通过 `Renderer` 管理。如果需要手动控制多线程，可以通过以下方式：

**注意**: 当前版本中，`LODInstancedRenderer` 是内部实现类，多线程功能通常由系统自动管理。如果需要手动控制，可能需要通过 `Renderer` 的扩展接口（如果提供）或直接访问 `LODInstancedRenderer`。

**使用建议**:
- 默认情况下，多线程功能是禁用的
- 对于大量实例的场景（1000+），系统会自动检测并使用多线程处理
- 小场景（<100实例）会自动使用单线程，避免线程同步开销
- 如果发现性能瓶颈，可以考虑手动启用多线程

**性能考虑**:
- 多线程主要用于处理大量实例的场景（1000+）
- 小场景可能不会带来明显提升，甚至可能因为线程同步开销而降低性能
- 建议先实现前面的优化（双缓冲、脏标记等），确认性能瓶颈后再考虑多线程

### Q: 双缓冲渲染策略和多线程数据准备的区别？

A: 这两个优化针对不同的性能瓶颈：

**双缓冲渲染策略（阶段3.1）**:
- **目标**：消除清空和重建开销
- **方法**：使用两个缓冲区，一个渲染当前帧，一个构建下一帧
- **优势**：零延迟切换，完全并行
- **适用场景**：所有场景，自动启用
- **性能提升**：30-50%（极端场景）

**多线程数据准备（阶段3.2）**:
- **目标**：利用多核CPU并行处理实例数据
- **方法**：后台线程准备数据，主线程负责GPU上传和渲染
- **优势**：充分利用多核CPU，主线程释放
- **适用场景**：大量实例场景（1000+），可选启用
- **性能提升**：20-40%（大量实例场景）

**两者可以同时使用**，协同工作以最大化性能提升。

### Q: 如何启用GPU剔除（阶段3.3）？

A: GPU剔除功能通过 `MeshRenderSystem` 提供：

```cpp
// 获取 MeshRenderSystem
auto* meshSystem = world->GetSystem<MeshRenderSystem>();
if (meshSystem) {
    // 检查GPU剔除是否可用（需要OpenGL 4.3+）
    if (meshSystem->IsLODGPUCullingAvailable()) {
        // 启用GPU剔除
        meshSystem->EnableLODGPUCulling(true);
        
        std::cout << "GPU剔除已启用" << std::endl;
    } else {
        std::cout << "GPU剔除不可用（需要OpenGL 4.3+）" << std::endl;
    }
}
```

**系统要求**:
- 需要OpenGL 4.3+支持Compute Shader
- 建议在10K+实例的场景中使用
- 需要足够的显存存储所有实例的矩阵数据

**性能提升**: 在大量实例场景（10K+）下，预期带来 **50-200%** 的性能提升。

**使用建议**:
- 默认情况下，GPU剔除功能是禁用的
- 对于大量实例的场景（10K+），可以考虑启用GPU剔除
- 系统会自动检测OpenGL版本，如果不支持会显示警告并禁用
- GPU剔除可以与CPU视锥体裁剪优化同时使用，但通常只需要使用一种

**注意**: 
- GPU剔除需要OpenGL 4.3+支持Compute Shader
- 如果系统不支持，会自动回退到CPU剔除
- GPU剔除主要用于处理大量实例的场景（10K+），小场景可能不会带来明显提升

### Q: GPU剔除和CPU视锥体裁剪优化的区别？

A: 这两个优化针对不同的性能瓶颈和使用场景：

**CPU视锥体裁剪优化（阶段3.3）**:
- **目标**：减少CPU端的LOD计算开销
- **方法**：先进行视锥体裁剪，只对可见实体计算LOD
- **优势**：减少不必要的LOD计算，提升缓存命中率
- **适用场景**：中等规模场景（1K-10K实体），CPU瓶颈
- **性能提升**：20-50%（中等规模场景）
- **系统要求**：OpenGL 3.3+（几乎所有现代GPU都支持）

**GPU剔除（阶段3.3）**:
- **目标**：完全避免CPU-GPU往返，在GPU上并行处理
- **方法**：使用Compute Shader在GPU上执行视锥剔除和LOD选择
- **优势**：完全并行，零CPU开销，可扩展性强
- **适用场景**：大规模场景（10K+实例），GPU瓶颈
- **性能提升**：50-200%（大规模场景）
- **系统要求**：OpenGL 4.3+（需要Compute Shader支持）

**选择建议**:
- **小场景（<1K实例）**：不需要启用任何优化
- **中等场景（1K-10K实例）**：使用CPU视锥体裁剪优化
- **大规模场景（10K+实例）**：使用GPU剔除
- **超大规模场景（100K+实例）**：GPU剔除 + 多线程数据准备

**两者可以同时使用**，但通常只需要使用一种，根据场景规模选择最适合的优化方式。

---

## LOD 实例化渲染（阶段2.2 + 阶段2.3）

### 概述

LOD 实例化渲染将相同网格、相同材质、相同 LOD 级别的实例分组，使用 GPU 实例化渲染，大幅减少 Draw Call。

**性能提升**:
- 100个实例：从100个 Draw Call 减少到2-10个 Draw Call
- 1000个实例：从1000个 Draw Call 减少到10-50个 Draw Call
- 性能提升：**20-100倍** Draw Call 减少

### Renderer API（阶段2.3）

```cpp
class Renderer {
public:
    // 设置 LOD 实例化渲染模式
    void SetLODInstancingEnabled(bool enabled);
    
    // 获取是否启用
    bool IsLODInstancingEnabled() const;
    
    // 检查是否可用
    bool IsLODInstancingAvailable() const;
    
    // 获取统计信息
    struct LODInstancingStats {
        size_t lodGroupCount = 0;
        size_t totalInstances = 0;
        size_t drawCalls = 0;
        size_t lod0Instances = 0;
        size_t lod1Instances = 0;
        size_t lod2Instances = 0;
        size_t lod3Instances = 0;
        size_t culledCount = 0;
    };
    
    LODInstancingStats GetLODInstancingStats() const;
};
```

### 使用示例

```cpp
// 1. 启用 LOD 实例化渲染
renderer->SetLODInstancingEnabled(true);

// 2. 检查是否可用
if (renderer->IsLODInstancingAvailable()) {
    std::cout << "LOD 实例化渲染可用" << std::endl;
}

// 3. 获取统计信息
auto stats = renderer->GetLODInstancingStats();
std::cout << "LOD 组数: " << stats.lodGroupCount << std::endl;
std::cout << "总实例数: " << stats.totalInstances << std::endl;
std::cout << "Draw Calls: " << stats.drawCalls << std::endl;
```

### 与批处理系统集成（阶段2.3）

LOD 实例化渲染可以与任何批处理模式共存：

```cpp
// 设置批处理模式
renderer->SetBatchingMode(BatchingMode::GpuInstancing);

// 启用 LOD 实例化渲染
renderer->SetLODInstancingEnabled(true);

// 优先级：LOD 实例化渲染 > 批处理系统
// 如果 LOD 实例化可用，优先使用
// 如果不可用，回退到批处理模式
```

**兼容性**:
- ✅ `BatchingMode::Disabled`: LOD 实例化仍可使用
- ✅ `BatchingMode::CpuMerge`: LOD 实例化仍可使用
- ✅ `BatchingMode::GpuInstancing`: LOD 实例化仍可使用，两者可以协同工作

### 工作原理

1. **分组**：按网格、材质、LOD级别分组
2. **收集**：收集每个组的实例数据（变换矩阵、颜色等）
3. **上传**：上传实例数据到 GPU（VBO）
4. **渲染**：使用 `DrawInstanced` 批量渲染

**分组示例**:
- 组1：网格A + 材质M + LOD0 → 47个实例 → 1个 Draw Call
- 组2：网格A + 材质M + LOD1 → 53个实例 → 1个 Draw Call
- 总计：100个实例，2个 Draw Call（减少98%）

### 高级优化（阶段3.1 + 3.2）

#### 双缓冲渲染策略（阶段3.1）

系统内部使用双缓冲策略，一个缓冲区用于渲染当前帧，另一个用于构建下一帧，完全消除清空和重建的开销。

**优势**:
- ✅ **零延迟切换**：只需交换索引，无需数据拷贝
- ✅ **完全并行**：CPU构建下一帧数据时，GPU渲染当前帧
- ✅ **内存效率**：复用缓冲区，减少分配/释放

**性能提升**: 在极端场景（100K+ 实例）下，预期带来 **30-50%** 的性能提升。

**注意**: 双缓冲策略是内部实现，无需用户配置，自动启用。

#### 多线程数据准备（阶段3.2）

系统支持在后台线程准备实例数据（矩阵变换、视锥剔除等），主线程仅负责GPU上传和渲染。

**工作原理**:
1. 主线程将待处理的实例分批，创建任务并放入任务队列
2. 工作线程从任务队列获取任务，在后台处理实例数据
3. 主线程等待所有任务完成，然后进行GPU上传和渲染

**优势**:
- ✅ **并行处理**：多个线程同时处理实例数据，充分利用多核CPU
- ✅ **主线程释放**：主线程可以专注于GPU上传和渲染
- ✅ **可扩展性**：根据CPU核心数自动调整线程数（默认：`hardware_concurrency() - 1`）
- ✅ **智能切换**：小批量数据（<100实例）自动使用单线程，避免开销

**性能提升**: 在大量实例场景（1000+）下，预期带来 **20-40%** 的性能提升。

**使用建议**:
- 默认情况下，多线程功能是禁用的
- 对于大量实例的场景（1000+），可以考虑启用多线程
- 系统会自动检测实例数量，超过阈值（100个实例）时才会使用多线程处理
- 小场景可能不会带来明显提升，甚至可能因为线程同步开销而降低性能

**注意**: 
- 多线程优化需要非常小心的同步设计
- 建议先实现前面的优化，确认性能瓶颈后再考虑
- 多线程主要用于处理大量实例的场景（1000+）
- 所有对构建缓冲区的访问都通过互斥锁保护，确保线程安全

#### GPU剔除（Compute Shader，阶段3.3）

系统支持使用Compute Shader在GPU上进行视锥剔除和LOD选择，完全避免CPU-GPU往返。

**工作原理**:
1. 将所有实例的变换矩阵上传到GPU的SSBO（Shader Storage Buffer Object）
2. 在GPU上并行执行视锥剔除和LOD选择计算（使用Compute Shader）
3. 使用原子操作收集可见实例索引，按LOD级别分组
4. CPU读取结果，使用可见实例列表进行渲染

**优势**:
- ✅ **完全并行**：GPU的数千个核心同时处理，远超CPU性能
- ✅ **零CPU开销**：视锥剔除和LOD选择完全在GPU上执行
- ✅ **避免往返**：不需要CPU-GPU数据传输，减少延迟
- ✅ **可扩展性**：性能随实例数量线性扩展，适合超大规模场景

**性能提升**: 在大量实例场景（10K+）下，预期带来 **50-200%** 的性能提升。

**使用建议**:
- 默认情况下，GPU剔除功能是禁用的
- 对于大量实例的场景（10K+），可以考虑启用GPU剔除
- 系统会自动检测OpenGL版本，如果不支持会显示警告并禁用
- GPU剔除可以与CPU视锥体裁剪优化同时使用，但通常只需要使用一种

**系统要求**:
- **OpenGL版本**：需要OpenGL 4.3+支持Compute Shader
- **实例数量**：建议在10K+实例的场景中使用
- **显存**：需要足够的显存存储所有实例的矩阵数据

**注意**: 
- GPU剔除需要OpenGL 4.3+支持Compute Shader
- 如果系统不支持，会自动回退到CPU剔除
- GPU剔除主要用于处理大量实例的场景（10K+），小场景可能不会带来明显提升

---

## 相关 API 和文档

- **[LODGenerator API](LODGenerator.md)** - LOD 网格和纹理生成器
  - 使用 `meshoptimizer` 库进行网格简化
  - 支持单个网格、整个模型以及批量处理
  - 提供文件保存和加载功能
  - 支持自动配置 LODConfig
  - **纹理 LOD 支持**：使用 mipmap 自动配置纹理 LOD
    - `EnsureTextureMipmap` - 确保纹理有 mipmap
    - `ConfigureMaterialTextureLOD` - 配置材质纹理 LOD
    - `AutoConfigureTextureLOD` - 自动配置材质纹理 LOD
    - `ConfigureModelTextureLOD` - 配置模型纹理 LOD
    - `AutoConfigureTextureLODStrategy` - 自动配置纹理 LOD 策略
- [Mesh API](Mesh.md) - 网格系统
- [Model API](Model.md) - 模型系统
- [Material API](Material.md) - 材质系统
- [Renderer API](Renderer.md) - 渲染器 API（包含 LOD 实例化渲染支持）
- [ECS Integration](../ECS_INTEGRATION.md) - ECS 系统集成指南
- [LOD 优化方案](../todolists/LOD_Instanced_Rendering_Optimization.md) - LOD 系统设计文档

---

**文档版本**: v1.7  
**最后更新**: 2025-12-01  
**对应代码版本**: RenderEngine v1.0.0

**更新历史**:
- **v1.7** (2025-12-01): 添加阶段3.3 - GPU剔除（Compute Shader）
  - 添加GPU剔除说明，包括工作原理、优势、使用建议和性能提升
  - 更新功能特性列表，添加GPU剔除支持
  - 更新性能优化建议，添加GPU剔除章节（9. GPU剔除）
  - 更新"LOD 实例化渲染"章节，添加GPU剔除高级优化说明
  - 添加常见问题：如何启用GPU剔除、GPU剔除和CPU视锥体裁剪优化的区别
  - 说明GPU剔除需要OpenGL 4.3+支持Compute Shader
  - 说明GPU剔除在10K+实例场景下的性能提升（50-200%）
- **v1.6** (2025-12-01): 添加阶段3.1和3.2 - 双缓冲渲染策略和多线程数据准备
  - 添加双缓冲渲染策略说明，包括工作原理、优势和使用建议
  - 添加多线程数据准备说明，包括API、优势、使用建议和性能提升
  - 更新功能特性列表，添加双缓冲和多线程支持
  - 更新性能优化建议，添加双缓冲和多线程相关章节
  - 更新"LOD 实例化渲染"章节，添加高级优化说明
- **v1.5** (2025-11-29): 扩展阶段3.3 - LOD 视锥体裁剪优化配置选项
  - 添加 `LODConfig::FrustumOutBehavior` 枚举，支持视锥体外的行为配置（Cull、UseLowerLOD、UseMinimalLOD）
  - 添加 `LODConfig::frustumOutBehavior` 和 `frustumOutLODReduction` 配置选项
  - 添加 `LODComponent::affectedByFrustumCulling` 标志，支持标记实体不受视锥体裁剪影响
  - 扩展 `LODFrustumCullingSystem`，支持视锥体外的实体使用更低LOD级别
  - 添加"配置视锥体外的行为"使用示例（方式A-D）
  - 添加"常见问题"章节中的"如何配置视锥体外的物体使用更低LOD级别"和"如何标记某些实体不受视锥体裁剪影响"问题
  - 更新性能优化建议，添加视锥体外的行为配置说明
- **v1.4** (2025-11-29): 添加阶段3.3 - LOD 视锥体裁剪优化
  - 添加 `LODFrustumCullingSystem` 类文档，包含 `BatchCullAndSelectLOD` 和 `BatchCullAndSelectLODWithBounds` 方法说明
  - 添加 `MeshRenderSystem::SetLODFrustumCullingEnabled()` 和 `IsLODFrustumCullingEnabled()` 方法说明
  - 添加"LOD 视锥体裁剪优化"使用示例（方式A和方式B）
  - 添加"性能优化建议"章节中的"LOD 视锥体裁剪优化"部分
  - 添加"常见问题"章节中的"如何使用 LOD 视锥体裁剪优化"和"为什么部分物体在还能看到的时候就被剔除了"问题
  - 更新功能特性列表，添加视锥体裁剪优化支持
- **v1.3** (2025-11-29): 添加阶段2.3 - LOD 实例化渲染与批处理系统集成
  - 添加 Renderer 级别的 LOD 实例化渲染 API 文档
  - 添加 `SetLODInstancingEnabled()`、`IsLODInstancingEnabled()`、`IsLODInstancingAvailable()` 方法说明
  - 添加 `LODInstancingStats` 结构体和 `GetLODInstancingStats()` 方法说明
  - 添加"LOD 实例化渲染"章节，包含工作原理、使用示例和性能提升说明
  - 添加"Renderer 级别的 LOD 实例化统计"章节
  - 更新"使用示例"章节，添加启用 LOD 实例化渲染的示例（方式A和方式B）
  - 更新"常见问题"章节，添加"如何使用 LOD 实例化渲染"和"LOD 实例化渲染与批处理模式的关系"问题
  - 更新"相关 API 和文档"部分，添加 Renderer API 链接
- **v1.2** (2025-11-29): 更新 LOD 切换逻辑说明
  - 更新平滑过渡逻辑说明，明确当前版本会在距离跨过阈值时立即切换
  - 更新 `transitionDistance` 说明，说明当前保留但未用于切换控制
  - 更新"平滑过渡距离"章节，改为通过调整距离阈值间隔来控制切换平滑度
  - 更新常见问题中的"LOD 切换太频繁"解答，改为调整阈值间隔而非 `transitionDistance`
  - 明确说明 LOD 计算与远距离剔除保持一致，每帧都执行
- **v1.1** (2025-11-28): 添加纹理 LOD 使用 LODGenerator 的说明
  - 更新"使用 Mipmap"章节，添加使用 `LODGenerator` 自动配置的示例
  - 更新"配置 LOD 纹理"示例，推荐使用 `LODGenerator` 自动配置
  - 更新"纹理策略选择"说明，添加 `LODGenerator` 相关文档链接
  - 更新"相关 API 和文档"部分，添加纹理 LOD 功能说明
- **v1.0** (2025-11-28): 初始版本

