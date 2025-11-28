# LOD 实例化渲染优化方案

## 📋 文档信息

| 项目 | 内容 |
|------|------|
| **文档版本** | v1.0 |
| **创建日期** | 2025-11-28 |
| **优化目标** | 设计并实现基于 LOD（Level of Detail）的实例化渲染优化系统 |
| **优先级** | P1 (性能优化) |
| **依赖** | ECS 系统、批处理系统、Transform 优化、Model/Mesh 系统 |

---

## 🎯 优化目标

### 核心原则
1. **LOD 与实例化结合**：将相同网格的不同 LOD 级别进行实例化渲染，大幅减少 Draw Call
2. **距离驱动的 LOD 选择**：基于相机距离自动选择合适的 LOD 级别
3. **无缝集成**：与现有 ECS 系统、批处理系统、Transform 系统无缝集成
4. **性能优先**：在保证视觉质量的前提下，最大化性能提升
5. **向后兼容**：保持现有 API 不变，LOD 为可选功能

### 设计理念
- **渐进式实施**：先实现基础 LOD 系统，再优化实例化渲染
- **数据驱动**：LOD 配置通过数据文件或组件配置，便于调优
- **缓存友好**：优化数据访问模式，提升缓存命中率
- **线程安全**：充分利用现有线程安全机制

### 性能目标
- **Draw Call 减少**：在大量相同对象场景中，减少 60-80% 的 Draw Call
- **帧时间减少**：在 1000+ 实例场景中，帧时间减少 30-50%
- **内存使用**：LOD 系统额外内存开销 < 5%
- **LOD 切换开销**：LOD 级别切换开销 < 0.1ms

---

## 📊 当前状态分析

### 1. 已有功能

#### 渲染系统
- ✅ **批处理系统**：支持 CPU 合并（`CpuMerge`）和 GPU 实例化（`GpuInstancing`）
- ✅ **渲染队列**：`Renderer::SubmitRenderable()` 和 `FlushRenderQueue()` 已实现
- ✅ **材质排序**：`MaterialSortKey` 系统已实现，支持材质排序优化
- ✅ **实例化渲染**：`Mesh::DrawInstanced()` 已支持 GPU 实例化

#### ECS 系统
- ✅ **Transform 组件**：已优化，支持三层缓存（L1 ~5ns）
- ✅ **MeshRenderComponent**：支持网格渲染组件
- ✅ **ModelComponent**：支持组合模型渲染
- ✅ **系统架构**：`MeshRenderSystem` 和 `ModelRenderSystem` 已实现

#### 资源管理
- ✅ **Model/Mesh 系统**：支持多部件模型和网格管理
- ✅ **ResourceManager**：统一的资源加载和管理
- ✅ **对象池**：`MeshRenderSystem` 中已使用对象池

### 2. 缺失功能

#### LOD 系统
- ❌ **LOD 级别管理**：没有 LOD 级别定义和管理
- ❌ **距离计算**：没有基于相机距离的 LOD 选择
- ❌ **LOD 切换**：没有平滑的 LOD 切换机制
- ❌ **LOD 数据**：没有 LOD 网格数据加载和管理

#### 实例化优化
- ❌ **LOD 实例化分组**：没有按 LOD 级别分组的实例化渲染
- ❌ **批量 LOD 选择**：没有批量计算 LOD 级别的优化
- ❌ **LOD 批处理集成**：没有与批处理系统的深度集成

---

## 🔧 优化方案

### 阶段 1: LOD 基础系统 (P1)

#### 1.1 LOD 数据结构设计

**方案**：定义 LOD 级别和配置结构

```cpp
// include/render/lod_system.h

namespace Render {

/**
 * @brief LOD 级别枚举
 */
enum class LODLevel {
    LOD0 = 0,  // 最高细节（最近）
    LOD1 = 1,  // 中等细节
    LOD2 = 2,  // 低细节
    LOD3 = 3,  // 最低细节（最远）
    Culled = 4 // 剔除（超出范围）
};

/**
 * @brief LOD 配置
 * 定义每个 LOD 级别的距离阈值、网格、材质和纹理
 */
struct LODConfig {
    // LOD 级别距离阈值（从近到远）
    std::vector<float> distanceThresholds{50.0f, 150.0f, 500.0f, 1000.0f};
    
    // 每个 LOD 级别对应的网格（可选，如果为空则使用原始网格）
    std::vector<Ref<Mesh>> lodMeshes;
    
    // 每个 LOD 级别对应的模型（可选，用于 ModelComponent）
    std::vector<Ref<Model>> lodModels;
    
    // 每个 LOD 级别对应的材质（可选，如果为空则使用原始材质）
    // 远距离可以使用更简单的材质（减少纹理、禁用某些效果）
    std::vector<Ref<Material>> lodMaterials;
    
    // 每个 LOD 级别对应的纹理（可选，用于材质纹理替换）
    // 例如：LOD0 使用 4K 纹理，LOD1 使用 2K，LOD2 使用 1K
    struct LODTextureSet {
        Ref<Texture> diffuseMap;      // 漫反射贴图
        Ref<Texture> normalMap;        // 法线贴图（LOD2+ 可省略）
        Ref<Texture> specularMap;      // 高光贴图（LOD2+ 可省略）
        Ref<Texture> emissiveMap;      // 自发光贴图（LOD2+ 可省略）
    };
    std::vector<LODTextureSet> lodTextures;
    
    // LOD 切换的平滑过渡距离（避免频繁切换）
    float transitionDistance = 10.0f;
    
    // 是否启用 LOD
    bool enabled = true;
    
    // 包围盒缩放因子（用于距离计算，考虑对象大小）
    float boundingBoxScale = 1.0f;
    
    // 纹理 LOD 策略
    enum class TextureLODStrategy {
        UseLODTextures,    // 使用 lodTextures 中指定的纹理
        UseMipmap,         // 使用原始纹理的 mipmap（自动）
        DisableTextures    // 远距离禁用纹理（LOD2+）
    };
    TextureLODStrategy textureStrategy = TextureLODStrategy::UseMipmap;
    
    /**
     * @brief 根据距离计算 LOD 级别
     * @param distance 到相机的距离
     * @return LOD 级别
     */
    LODLevel CalculateLOD(float distance) const {
        if (!enabled) return LODLevel::LOD0;
        
        for (size_t i = 0; i < distanceThresholds.size(); ++i) {
            if (distance < distanceThresholds[i]) {
                return static_cast<LODLevel>(i);
            }
        }
        return LODLevel::Culled;
    }
    
    /**
     * @brief 获取指定 LOD 级别的网格
     * @param level LOD 级别
     * @param defaultMesh 默认网格（LOD0 或未配置时使用）
     * @return 网格指针
     */
    Ref<Mesh> GetLODMesh(LODLevel level, Ref<Mesh> defaultMesh) const {
        if (lodMeshes.empty()) return defaultMesh;
        
        size_t index = static_cast<size_t>(level);
        if (index < lodMeshes.size() && lodMeshes[index]) {
            return lodMeshes[index];
        }
        return defaultMesh;
    }
    
    /**
     * @brief 获取指定 LOD 级别的模型
     * @param level LOD 级别
     * @param defaultModel 默认模型（LOD0 或未配置时使用）
     * @return 模型指针
     */
    Ref<Model> GetLODModel(LODLevel level, Ref<Model> defaultModel) const {
        if (lodModels.empty()) return defaultModel;
        
        size_t index = static_cast<size_t>(level);
        if (index < lodModels.size() && lodModels[index]) {
            return lodModels[index];
        }
        return defaultModel;
    }
    
    /**
     * @brief 获取指定 LOD 级别的材质
     * @param level LOD 级别
     * @param defaultMaterial 默认材质（LOD0 或未配置时使用）
     * @return 材质指针
     */
    Ref<Material> GetLODMaterial(LODLevel level, Ref<Material> defaultMaterial) const {
        if (lodMaterials.empty()) return defaultMaterial;
        
        size_t index = static_cast<size_t>(level);
        if (index < lodMaterials.size() && lodMaterials[index]) {
            return lodMaterials[index];
        }
        return defaultMaterial;
    }
    
    /**
     * @brief 应用 LOD 纹理到材质
     * @param level LOD 级别
     * @param material 材质（会被修改）
     */
    void ApplyLODTextures(LODLevel level, Ref<Material> material) const {
        if (lodTextures.empty() || textureStrategy != TextureLODStrategy::UseLODTextures) {
            return;
        }
        
        size_t index = static_cast<size_t>(level);
        if (index >= lodTextures.size()) {
            return;
        }
        
        const LODTextureSet& textureSet = lodTextures[index];
        
        // 应用纹理到材质
        if (textureSet.diffuseMap) {
            material->SetTexture("diffuse", textureSet.diffuseMap);
        }
        if (textureSet.normalMap) {
            material->SetTexture("normal", textureSet.normalMap);
        }
        if (textureSet.specularMap) {
            material->SetTexture("specular", textureSet.specularMap);
        }
        if (textureSet.emissiveMap) {
            material->SetTexture("emissive", textureSet.emissiveMap);
        }
        
        // 根据 LOD 级别禁用某些纹理（节省采样）
        if (level >= LODLevel::LOD2) {
            // LOD2+ 可以禁用法线贴图和高光贴图
            if (!textureSet.normalMap) {
                material->SetBool("uUseNormalMap", false);
            }
            if (!textureSet.specularMap) {
                material->SetBool("uUseSpecularMap", false);
            }
        }
    }
};

/**
 * @brief LOD 组件（ECS）
 * 附加到实体上，提供 LOD 配置和当前 LOD 级别
 */
struct LODComponent {
    LODConfig config;
    LODLevel currentLOD = LODLevel::LOD0;
    float lastDistance = 0.0f;
    uint64_t lastUpdateFrame = 0;  // 避免每帧都更新
    
    // 统计信息（调试用）
    uint32_t lodSwitchCount = 0;
    LODLevel lastLOD = LODLevel::LOD0;
};

} // namespace Render
```

**优化要点**：
1. **距离阈值配置**：可配置的距离阈值，支持不同场景需求
2. **网格/模型分离**：支持 Mesh 和 Model 两种类型的 LOD
3. **平滑过渡**：通过 `transitionDistance` 避免频繁切换
4. **包围盒缩放**：考虑对象大小，使用包围盒缩放因子

---

#### 1.2 LOD 选择系统

**方案**：实现基于距离的 LOD 选择逻辑

```cpp
// include/render/lod_system.h

namespace Render {

/**
 * @brief LOD 选择器
 * 负责计算实体到相机的距离并选择 LOD 级别
 */
class LODSelector {
public:
    /**
     * @brief 计算实体到相机的距离
     * @param entityPosition 实体世界位置
     * @param cameraPosition 相机世界位置
     * @return 距离（单位：世界单位）
     */
    static float CalculateDistance(
        const Vector3& entityPosition,
        const Vector3& cameraPosition
    ) {
        return (entityPosition - cameraPosition).norm();
    }
    
    /**
     * @brief 计算实体到相机的距离（考虑包围盒）
     * @param entityPosition 实体世界位置
     * @param entityBounds 实体包围盒（世界空间）
     * @param cameraPosition 相机世界位置
     * @return 距离（单位：世界单位）
     */
    static float CalculateDistanceWithBounds(
        const Vector3& entityPosition,
        const AABB& entityBounds,
        const Vector3& cameraPosition,
        float boundingBoxScale = 1.0f
    ) {
        // 计算包围盒中心到相机的距离
        Vector3 boundsCenter = entityBounds.GetCenter();
        float centerDistance = (boundsCenter - cameraPosition).norm();
        
        // 考虑包围盒大小（使用最大轴）
        Vector3 boundsSize = entityBounds.GetSize();
        float maxAxis = std::max({boundsSize.x(), boundsSize.y(), boundsSize.z()});
        float adjustedDistance = centerDistance - (maxAxis * boundingBoxScale * 0.5f);
        
        return std::max(0.0f, adjustedDistance);
    }
    
    /**
     * @brief 批量计算 LOD 级别
     * @param entities 实体列表
     * @param world ECS World
     * @param cameraPosition 相机位置
     * @param frameId 当前帧 ID（用于避免重复计算）
     */
    static void BatchCalculateLOD(
        const std::vector<EntityID>& entities,
        World* world,
        const Vector3& cameraPosition,
        uint64_t frameId
    ) {
        for (EntityID entity : entities) {
            // 检查是否有 LOD 组件
            if (!world->HasComponent<LODComponent>(entity)) {
                continue;
            }
            
            auto& lodComp = world->GetComponent<LODComponent>(entity);
            
            // 避免每帧都更新（可以每 N 帧更新一次）
            if (lodComp.lastUpdateFrame == frameId) {
                continue;
            }
            
            // 获取 Transform 组件
            if (!world->HasComponent<TransformComponent>(entity)) {
                continue;
            }
            
            auto& transformComp = world->GetComponent<TransformComponent>(entity);
            if (!transformComp.transform) {
                continue;
            }
            
            // 计算距离
            Vector3 entityPos = transformComp.transform->GetWorldPosition();
            float distance = CalculateDistance(entityPos, cameraPosition);
            
            // 计算 LOD 级别
            LODLevel newLOD = lodComp.config.CalculateLOD(distance);
            
            // 平滑过渡：避免频繁切换
            if (newLOD != lodComp.currentLOD) {
                float transitionThreshold = lodComp.config.transitionDistance;
                float distanceDiff = std::abs(distance - lodComp.lastDistance);
                
                // 只有在距离变化足够大时才切换
                if (distanceDiff > transitionThreshold) {
                    lodComp.currentLOD = newLOD;
                    lodComp.lodSwitchCount++;
                    lodComp.lastLOD = lodComp.currentLOD;
                }
            }
            
            lodComp.lastDistance = distance;
            lodComp.lastUpdateFrame = frameId;
        }
    }
};

} // namespace Render
```

**优化要点**：
1. **批量计算**：批量处理多个实体，提升性能
2. **帧级缓存**：使用 `lastUpdateFrame` 避免重复计算
3. **平滑过渡**：通过 `transitionDistance` 避免频繁切换
4. **包围盒考虑**：可选的支持包围盒的距离计算

---

#### 1.3 LOD 组件集成

**方案**：在 ECS 组件中添加 LOD 支持

```cpp
// include/render/ecs/components.h (扩展)

namespace Render {
namespace ECS {

// 在现有组件中添加 LOD 支持

/**
 * @brief MeshRenderComponent 扩展（可选）
 * 如果实体有 LODComponent，则使用 LOD 网格
 */
struct MeshRenderComponent {
    // ... 现有字段 ...
    
    // LOD 相关（可选）
    // 如果实体有 LODComponent，系统会自动使用 LOD 网格
    // 这里不需要额外字段，通过 LODComponent 访问
    
    // Per-Instance 数据（用于实例化渲染）
    std::optional<Color> instanceColor;        // 实例颜色（可选）
    std::optional<Vector4> customParams;      // 自定义参数（可选）
    // 注意：worldMatrix 从 Transform 组件获取，不需要单独存储
};

/**
 * @brief ModelComponent 扩展（可选）
 * 如果实体有 LODComponent，则使用 LOD 模型
 */
struct ModelComponent {
    // ... 现有字段 ...
    
    // LOD 相关（可选）
    // 如果实体有 LODComponent，系统会自动使用 LOD 模型
};

} // namespace ECS
} // namespace Render
```

**实施步骤**：
1. 在 `components.h` 中添加 `LODComponent` 定义
2. 在 `MeshRenderSystem` 中检查 `LODComponent` 并使用 LOD 网格
3. 在 `ModelRenderSystem` 中检查 `LODComponent` 并使用 LOD 模型

---

### 阶段 2: LOD 实例化渲染 (P1)

#### 2.1 LOD 实例化分组

**方案**：按 LOD 级别分组，对相同 LOD 级别的实例进行批量渲染

```cpp
// include/render/lod_instanced_renderer.h

namespace Render {

/**
 * @brief 实例化渲染的 Per-Instance 数据
 * 每个实例的独立参数
 */
struct InstanceData {
    Matrix4 worldMatrix;         // 世界变换矩阵（必需）
    Vector3 worldPosition;        // 世界位置（从矩阵提取，用于调试/查询）
    Color instanceColor;           // 实例颜色（可选，用于颜色变化）
    Vector4 customParams;          // 自定义参数（可选，用于特殊效果）
    float scale = 1.0f;           // 实例缩放（可选，如果矩阵已包含则忽略）
    uint32_t instanceID = 0;      // 实例 ID（用于调试）
    
    // 扩展：可以添加更多 per-instance 数据
    // Vector3 instanceVelocity;   // 速度（用于运动模糊）
    // float instanceTime;         // 时间偏移（用于动画）
    // uint32_t instanceFlags;     // 标志位（用于特殊渲染）
};

/**
 * @brief LOD 实例化渲染组
 * 将相同网格、相同材质、相同 LOD 级别的实例分组
 */
struct LODInstancedGroup {
    Ref<Mesh> mesh;              // 网格（LOD 级别对应的网格）
    Ref<Material> material;      // 材质（LOD 级别对应的材质）
    LODLevel lodLevel;           // LOD 级别
    MaterialSortKey sortKey;     // 材质排序键
    
    // 实例数据
    std::vector<InstanceData> instances;  // 所有实例的数据
    std::vector<EntityID> entities;        // 对应的实体 ID（用于调试）
    
    // 统计信息
    size_t instanceCount() const { return instances.size(); }
};

/**
 * @brief LOD 实例化渲染器
 * 负责收集、分组和渲染 LOD 实例
 */
class LODInstancedRenderer {
public:
    /**
     * @brief 添加实例（简化版本，只使用矩阵）
     * @param entity 实体 ID
     * @param mesh 网格（已选择 LOD 级别）
     * @param material 材质
     * @param worldMatrix 世界变换矩阵
     * @param lodLevel LOD 级别
     */
    void AddInstance(
        EntityID entity,
        Ref<Mesh> mesh,
        Ref<Material> material,
        const Matrix4& worldMatrix,
        LODLevel lodLevel
    ) {
        InstanceData instanceData;
        instanceData.worldMatrix = worldMatrix;
        instanceData.worldPosition = worldMatrix.block<3, 1>(0, 3);  // 提取位置
        instanceData.instanceColor = Color::White();
        instanceData.instanceID = entity;
        
        AddInstance(entity, mesh, material, instanceData, lodLevel);
    }
    
    /**
     * @brief 添加实例（完整版本，支持所有 per-instance 数据）
     * @param entity 实体 ID
     * @param mesh 网格（已选择 LOD 级别）
     * @param material 材质
     * @param instanceData 实例数据（包含矩阵、颜色、自定义参数等）
     * @param lodLevel LOD 级别
     */
    void AddInstance(
        EntityID entity,
        Ref<Mesh> mesh,
        Ref<Material> material,
        const InstanceData& instanceData,
        LODLevel lodLevel
    ) {
        // 生成材质排序键
        MaterialSortKey sortKey = MaterialSortKey::Build(
            material.get(),
            mesh.get(),
            RenderableType::Mesh
        );
        
        // 查找或创建组
        LODInstancedGroupKey key{mesh, material, lodLevel, sortKey};
        auto& group = m_groups[key];
        
        if (group.instances.empty()) {
            group.mesh = mesh;
            group.material = material;
            group.lodLevel = lodLevel;
            group.sortKey = sortKey;
        }
        
        group.instances.push_back(instanceData);
        group.entities.push_back(entity);
    }
    
    /**
     * @brief 渲染所有实例组
     * @param renderer 渲染器
     * @param renderState 渲染状态
     */
    void RenderAll(Renderer* renderer, RenderState* renderState) {
        // 按材质排序键排序
        std::vector<LODInstancedGroup*> sortedGroups;
        for (auto& [key, group] : m_groups) {
            sortedGroups.push_back(&group);
        }
        
        std::sort(sortedGroups.begin(), sortedGroups.end(),
            [](const LODInstancedGroup* a, const LODInstancedGroup* b) {
                return a->sortKey < b->sortKey;
            });
        
        // 渲染每个组
        for (auto* group : sortedGroups) {
            RenderGroup(group, renderer, renderState);
        }
    }
    
    /**
     * @brief 清空所有组
     */
    void Clear() {
        m_groups.clear();
    }
    
    /**
     * @brief 获取统计信息
     */
    struct Stats {
        size_t groupCount = 0;
        size_t totalInstances = 0;
        size_t drawCalls = 0;
    };
    
    Stats GetStats() const {
        Stats stats;
        stats.groupCount = m_groups.size();
        for (const auto& [key, group] : m_groups) {
            stats.totalInstances += group.instanceCount();
            stats.drawCalls++;  // 每个组一次 Draw Call
        }
        return stats;
    }
    
private:
    /**
     * @brief 渲染单个组
     */
    void RenderGroup(
        LODInstancedGroup* group,
        Renderer* renderer,
        RenderState* renderState
    ) {
        if (!group->mesh || !group->material || group->instances.empty()) {
            return;
        }
        
        // 绑定材质
        group->material->Bind();
        
        // 应用 LOD 纹理（如果配置了）
        if (group->material && group->lodLevel != LODLevel::LOD0) {
            // 这里可以应用 LOD 级别的纹理替换
            // 例如：使用更低分辨率的纹理
        }
        
        // 上传实例数据到 GPU
        UploadInstanceData(group->instances);
        
        // 设置渲染状态
        if (renderState) {
            renderState->ApplyMaterialState(group->material.get());
        }
        
        // 实例化绘制
        group->mesh->DrawInstanced(
            static_cast<uint32_t>(group->instances.size())
        );
    }
    
    /**
     * @brief 上传实例数据到 GPU
     * 包括：变换矩阵、颜色、自定义参数等
     */
    void UploadInstanceData(const std::vector<InstanceData>& instances) {
        // 1. 提取矩阵数据（用于 location 4-7）
        std::vector<Matrix4> matrices;
        matrices.reserve(instances.size());
        for (const auto& instance : instances) {
            matrices.push_back(instance.worldMatrix);
        }
        
        // 2. 提取颜色数据（用于额外的实例化属性）
        std::vector<Vector4> colors;
        colors.reserve(instances.size());
        for (const auto& instance : instances) {
            colors.push_back(Vector4(
                instance.instanceColor.r,
                instance.instanceColor.g,
                instance.instanceColor.b,
                instance.instanceColor.a
            ));
        }
        
        // 3. 提取自定义参数（用于额外的实例化属性）
        std::vector<Vector4> customParams;
        customParams.reserve(instances.size());
        for (const auto& instance : instances) {
            customParams.push_back(instance.customParams);
        }
        
        // 4. 上传到 GPU 实例化缓冲
        // 使用实例化 VBO（Vertex Buffer Object）
        // Location 4-7: 实例矩阵（4x4，每行一个 vec4）
        UploadInstanceMatrices(matrices);
        
        // Location 8: 实例颜色（vec4）
        UploadInstanceColors(colors);
        
        // Location 9: 自定义参数（vec4）
        UploadInstanceCustomParams(customParams);
    }
    
    /**
     * @brief 上传实例矩阵到 GPU（location 4-7）
     */
    void UploadInstanceMatrices(const std::vector<Matrix4>& matrices) {
        // 使用现有的实例化缓冲系统
        // 矩阵数据按行存储：row0, row1, row2, row3
        // 每个矩阵占用 4 个 vec4（location 4-7）
        // 实现细节：使用 glBufferData 或 glMapBuffer 上传
    }
    
    /**
     * @brief 上传实例颜色到 GPU（location 8）
     */
    void UploadInstanceColors(const std::vector<Vector4>& colors) {
        // 上传颜色数据到额外的实例化属性
        // 着色器中使用：layout(location = 8) in vec4 aInstanceColor;
    }
    
    /**
     * @brief 上传自定义参数到 GPU（location 9）
     */
    void UploadInstanceCustomParams(const std::vector<Vector4>& customParams) {
        // 上传自定义参数到额外的实例化属性
        // 着色器中使用：layout(location = 9) in vec4 aInstanceCustomParams;
    }
    
    // 分组键
    struct LODInstancedGroupKey {
        Ref<Mesh> mesh;
        Ref<Material> material;
        LODLevel lodLevel;
        MaterialSortKey sortKey;
        
        bool operator<(const LODInstancedGroupKey& other) const {
            if (mesh != other.mesh) return mesh < other.mesh;
            if (material != other.material) return material < other.material;
            if (lodLevel != other.lodLevel) return lodLevel < other.lodLevel;
            return sortKey < other.sortKey;
        }
    };
    
    std::map<LODInstancedGroupKey, LODInstancedGroup> m_groups;
};

} // namespace Render
```

**优化要点**：
1. **按 LOD 分组**：相同 LOD 级别的实例分组渲染
2. **材质排序**：使用现有的 `MaterialSortKey` 系统
3. **实例化渲染**：使用 `Mesh::DrawInstanced()` 进行 GPU 实例化
4. **统计信息**：提供统计信息用于性能分析

---

#### 2.2 LOD 系统集成到 MeshRenderSystem

**方案**：在 `MeshRenderSystem` 中集成 LOD 和实例化渲染

```cpp
// src/ecs/systems.cpp (扩展)

namespace Render {
namespace ECS {

class MeshRenderSystem {
public:
    void Update(World* world, float dt) {
        // 1. 获取主相机位置
        Vector3 cameraPosition = GetMainCameraPosition(world);
        uint64_t frameId = GetCurrentFrameId();
        
        // 2. 查询需要渲染的实体
        auto entities = world->Query<TransformComponent, MeshRenderComponent>();
        
        // 3. 批量计算 LOD（如果有 LODComponent）
        std::vector<EntityID> lodEntities;
        for (EntityID entity : entities) {
            if (world->HasComponent<LODComponent>(entity)) {
                lodEntities.push_back(entity);
            }
        }
        
        if (!lodEntities.empty()) {
            LODSelector::BatchCalculateLOD(
                lodEntities,
                world,
                cameraPosition,
                frameId
            );
        }
        
        // 4. 收集实例数据（按 LOD 分组）
        m_lodRenderer.Clear();
        
        for (EntityID entity : entities) {
            auto& transformComp = world->GetComponent<TransformComponent>(entity);
            auto& meshComp = world->GetComponent<MeshRenderComponent>(entity);
            
            if (!transformComp.transform || !meshComp.mesh || !meshComp.material) {
                continue;
            }
            
            // 检查可见性
            if (!meshComp.visible) {
                continue;
            }
            
            // 获取 LOD 级别和网格
            LODLevel lodLevel = LODLevel::LOD0;
            Ref<Mesh> renderMesh = meshComp.mesh;
            
            if (world->HasComponent<LODComponent>(entity)) {
                auto& lodComp = world->GetComponent<LODComponent>(entity);
                lodLevel = lodComp.currentLOD;
                
                // 使用 LOD 网格（如果配置了）
                renderMesh = lodComp.config.GetLODMesh(lodLevel, meshComp.mesh);
            }
            
            // 如果 LOD 级别是 Culled，跳过
            if (lodLevel == LODLevel::Culled) {
                continue;
            }
            
            // 获取世界变换矩阵和位置
            Matrix4 worldMatrix = transformComp.transform->GetWorldMatrix();
            Vector3 worldPosition = transformComp.transform->GetWorldPosition();
            
            // 构建实例数据
            InstanceData instanceData;
            instanceData.worldMatrix = worldMatrix;
            instanceData.worldPosition = worldPosition;
            instanceData.instanceID = entity;
            
            // 从组件获取实例颜色（如果支持）
            if (meshComp.instanceColor.has_value()) {
                instanceData.instanceColor = *meshComp.instanceColor;
            } else {
                instanceData.instanceColor = Color::White();
            }
            
            // 从组件获取自定义参数（如果支持）
            if (meshComp.customParams.has_value()) {
                instanceData.customParams = *meshComp.customParams;
            }
            
            // 获取 LOD 材质（如果配置了）
            Ref<Material> renderMaterial = meshComp.material;
            if (world->HasComponent<LODComponent>(entity)) {
                auto& lodComp = world->GetComponent<LODComponent>(entity);
                renderMaterial = lodComp.config.GetLODMaterial(lodLevel, meshComp.material);
                
                // 应用 LOD 纹理（如果配置了）
                if (lodComp.config.textureStrategy == LODConfig::TextureLODStrategy::UseLODTextures) {
                    lodComp.config.ApplyLODTextures(lodLevel, renderMaterial);
                }
            }
            
            // 添加到实例化渲染器
            m_lodRenderer.AddInstance(
                entity,
                renderMesh,
                renderMaterial,
                instanceData,
                lodLevel
            );
        }
        
        // 5. 渲染所有实例组
        m_lodRenderer.RenderAll(m_renderer, m_renderer->GetRenderState());
    }
    
private:
    LODInstancedRenderer m_lodRenderer;
    Renderer* m_renderer;
    
    Vector3 GetMainCameraPosition(World* world) {
        // 获取主相机位置
        // 实现细节...
        return Vector3::Zero();
    }
    
    uint64_t GetCurrentFrameId() {
        // 获取当前帧 ID
        // 实现细节...
        return 0;
    }
};

} // namespace ECS
} // namespace Render
```

**优化要点**：
1. **批量 LOD 计算**：在渲染前批量计算所有实体的 LOD 级别
2. **实例收集**：收集相同 LOD 级别的实例
3. **实例化渲染**：使用 `LODInstancedRenderer` 进行批量渲染
4. **向后兼容**：没有 `LODComponent` 的实体使用原始网格

---

#### 2.3 与批处理系统集成

**方案**：将 LOD 实例化渲染集成到现有的批处理系统

```cpp
// include/render/renderer.h (扩展)

namespace Render {

class Renderer {
public:
    // ... 现有方法 ...
    
    /**
     * @brief 设置 LOD 实例化渲染模式
     * @param enabled 是否启用 LOD 实例化渲染
     */
    void SetLODInstancingEnabled(bool enabled) {
        m_lodInstancingEnabled = enabled;
    }
    
    /**
     * @brief 获取 LOD 实例化渲染统计信息
     */
    struct LODInstancingStats {
        size_t lodGroupCount = 0;
        size_t totalInstances = 0;
        size_t lod0Instances = 0;
        size_t lod1Instances = 0;
        size_t lod2Instances = 0;
        size_t lod3Instances = 0;
        size_t culledCount = 0;
    };
    
    LODInstancingStats GetLODInstancingStats() const {
        return m_lodInstancingStats;
    }
    
private:
    bool m_lodInstancingEnabled = true;
    LODInstancingStats m_lodInstancingStats;
};

} // namespace Render
```

**集成策略**：
1. **批处理模式兼容**：LOD 实例化渲染可以与现有的 `BatchingMode` 共存
2. **优先级**：如果启用 LOD 实例化，优先使用 LOD 实例化渲染
3. **回退机制**：如果 LOD 实例化不可用，回退到普通批处理

---

### 阶段 3: 高级优化 (P2)

#### 3.1 LOD 数据加载和管理

**方案**：实现 LOD 网格/模型的加载和管理

```cpp
// include/render/lod_loader.h

namespace Render {

/**
 * @brief LOD 加载选项
 */
struct LODLoadOptions {
    // LOD 文件命名约定
    // 例如：model_lod0.obj, model_lod1.obj, model_lod2.obj
    std::string basePath;
    std::string namingPattern = "{name}_lod{level}.{ext}";
    
    // 自动生成 LOD（通过网格简化）
    bool autoGenerateLOD = false;
    float lod0Simplification = 0.0f;  // 0 = 不简化
    float lod1Simplification = 0.3f;  // 简化 30%
    float lod2Simplification = 0.6f;  // 简化 60%
    float lod3Simplification = 0.8f;  // 简化 80%
    
    // 加载策略
    bool preloadAllLODs = false;  // 是否预加载所有 LOD 级别
    bool asyncLoad = true;         // 是否异步加载
};

/**
 * @brief LOD 加载器
 */
class LODLoader {
public:
    /**
     * @brief 加载 LOD 配置
     * @param baseMesh 基础网格（LOD0）
     * @param options 加载选项
     * @return LOD 配置
     */
    static LODConfig LoadLODConfig(
        Ref<Mesh> baseMesh,
        const LODLoadOptions& options
    ) {
        LODConfig config;
        config.enabled = true;
        
        if (options.autoGenerateLOD) {
            // 自动生成 LOD 网格（通过网格简化算法）
            config.lodMeshes = GenerateLODMeshes(baseMesh, options);
        } else {
            // 从文件加载 LOD 网格
            config.lodMeshes = LoadLODMeshesFromFiles(baseMesh, options);
        }
        
        return config;
    }
    
    /**
     * @brief 从文件加载 LOD 网格
     */
    static std::vector<Ref<Mesh>> LoadLODMeshesFromFiles(
        Ref<Mesh> baseMesh,
        const LODLoadOptions& options
    ) {
        std::vector<Ref<Mesh>> lodMeshes;
        
        // 实现文件加载逻辑
        // 根据 namingPattern 加载不同 LOD 级别的网格
        // ...
        
        return lodMeshes;
    }
    
    /**
     * @brief 自动生成 LOD 网格
     */
    static std::vector<Ref<Mesh>> GenerateLODMeshes(
        Ref<Mesh> baseMesh,
        const LODLoadOptions& options
    ) {
        std::vector<Ref<Mesh>> lodMeshes;
        
        // 实现网格简化算法
        // 可以使用第三方库（如 meshoptimizer）或自定义算法
        // ...
        
        return lodMeshes;
    }
};

} // namespace Render
```

**实施建议**：
1. **文件命名约定**：定义清晰的 LOD 文件命名约定
2. **自动生成**：可选的支持自动生成 LOD（通过网格简化）
3. **异步加载**：支持异步加载 LOD 数据，避免阻塞主线程

---

#### 3.2 LOD 切换平滑过渡

**方案**：实现 LOD 切换的平滑过渡（Morphing）

```cpp
// include/render/lod_morphing.h

namespace Render {

/**
 * @brief LOD Morphing 系统
 * 在 LOD 级别切换时进行平滑过渡
 */
class LODMorphingSystem {
public:
    /**
     * @brief 计算 Morphing 权重
     * @param distance 当前距离
     * @param lowerThreshold 较低 LOD 的距离阈值
     * @param upperThreshold 较高 LOD 的距离阈值
     * @return Morphing 权重 [0, 1]，0 表示使用较低 LOD，1 表示使用较高 LOD
     */
    static float CalculateMorphWeight(
        float distance,
        float lowerThreshold,
        float upperThreshold
    ) {
        if (distance <= lowerThreshold) return 0.0f;
        if (distance >= upperThreshold) return 1.0f;
        
        // 线性插值
        float t = (distance - lowerThreshold) / (upperThreshold - lowerThreshold);
        return t;
    }
    
    /**
     * @brief 渲染 Morphing 网格
     * 在着色器中进行顶点插值
     */
    static void RenderMorphingMesh(
        Ref<Mesh> lowerLODMesh,
        Ref<Mesh> upperLODMesh,
        float morphWeight,
        Renderer* renderer
    ) {
        // 实现 Morphing 渲染逻辑
        // 需要特殊的着色器支持顶点插值
        // ...
    }
};

} // namespace Render
```

**注意**：Morphing 会增加复杂度，建议作为可选的高级功能。

---

#### 3.3 LOD 视锥体裁剪优化

**方案**：结合视锥体裁剪和 LOD 选择

```cpp
// include/render/lod_frustum_culling.h

namespace Render {

/**
 * @brief LOD 视锥体裁剪系统
 * 结合视锥体裁剪和 LOD 选择，进一步提升性能
 */
class LODFrustumCullingSystem {
public:
    /**
     * @brief 批量进行视锥体裁剪和 LOD 选择
     * @param entities 实体列表
     * @param world ECS World
     * @param camera 相机
     * @param frameId 当前帧 ID
     * @return 可见实体列表（按 LOD 分组）
     */
    static std::map<LODLevel, std::vector<EntityID>> BatchCullAndSelectLOD(
        const std::vector<EntityID>& entities,
        World* world,
        const Camera& camera,
        uint64_t frameId
    ) {
        std::map<LODLevel, std::vector<EntityID>> result;
        
        // 获取视锥体
        Frustum frustum = camera.GetFrustum();
        Vector3 cameraPos = camera.GetPosition();
        
        for (EntityID entity : entities) {
            // 视锥体裁剪
            if (!IsEntityVisible(entity, world, frustum)) {
                continue;
            }
            
            // LOD 选择
            if (world->HasComponent<LODComponent>(entity)) {
                auto& lodComp = world->GetComponent<LODComponent>(entity);
                
                // 计算距离
                if (world->HasComponent<TransformComponent>(entity)) {
                    auto& transformComp = world->GetComponent<TransformComponent>(entity);
                    if (transformComp.transform) {
                        Vector3 entityPos = transformComp.transform->GetWorldPosition();
                        float distance = (entityPos - cameraPos).norm();
                        
                        LODLevel lod = lodComp.config.CalculateLOD(distance);
                        if (lod != LODLevel::Culled) {
                            result[lod].push_back(entity);
                        }
                    }
                }
            } else {
                // 没有 LOD 组件，使用 LOD0
                result[LODLevel::LOD0].push_back(entity);
            }
        }
        
        return result;
    }
    
private:
    static bool IsEntityVisible(EntityID entity, World* world, const Frustum& frustum) {
        // 实现视锥体裁剪逻辑
        // ...
        return true;
    }
};

} // namespace Render
```

---

## 📊 实施计划

### 时间表

| 阶段 | 任务 | 预计工时 | 优先级 |
|------|------|----------|--------|
| **阶段 1.1** | LOD 数据结构设计 | 8h | P1 |
| **阶段 1.2** | LOD 选择系统实现 | 12h | P1 |
| **阶段 1.3** | LOD 组件集成 | 8h | P1 |
| **测试 & 验证** | LOD 基础系统测试 | 8h | P1 |
| **阶段 2.1** | LOD 实例化分组 | 16h | P1 |
| **阶段 2.2** | MeshRenderSystem 集成 | 12h | P1 |
| **阶段 2.3** | 批处理系统集成 | 8h | P1 |
| **测试 & Benchmark** | LOD 实例化性能测试 | 12h | P1 |
| **阶段 3.1** | LOD 数据加载 | 16h | P2 |
| **阶段 3.2** | LOD Morphing（可选） | 20h | P2 |
| **阶段 3.3** | 视锥体裁剪优化 | 12h | P2 |
| **文档 & Review** | 代码审查和文档更新 | 8h | P2 |
| **总计** |  | **140h (17.5 工作日)** |  |

### 里程碑

- **M1 (Week 2)**: 阶段 1 完成，LOD 基础系统可用
- **M2 (Week 4)**: 阶段 2 完成，LOD 实例化渲染可用，性能提升 30%+
- **M3 (Week 6)**: 阶段 3 完成，完整 LOD 系统，性能提升 50%+

---

## 🧪 测试策略

### 性能基准测试

```cpp
// benchmark_lod_instancing.cpp

void BM_LODInstancing_1000Entities(benchmark::State& state) {
    World world;
    Renderer renderer;
    MeshRenderSystem system(&renderer);
    
    // 创建 1000 个实体，配置 LOD
    for (int i = 0; i < 1000; ++i) {
        EntityID entity = world.CreateEntity();
        world.AddComponent<TransformComponent>(entity);
        world.AddComponent<MeshRenderComponent>(entity);
        
        // 添加 LOD 组件
        LODComponent lodComp;
        lodComp.config.enabled = true;
        lodComp.config.distanceThresholds = {50.0f, 150.0f, 500.0f, 1000.0f};
        world.AddComponent<LODComponent>(entity, lodComp);
    }
    
    for (auto _ : state) {
        system.Update(&world, 0.016f);
        renderer.FlushRenderQueue();
    }
    
    state.SetComplexityN(1000);
}
BENCHMARK(BM_LODInstancing_1000Entities)->Complexity();

void BM_LODSelection_Batch(benchmark::State& state) {
    World world;
    Vector3 cameraPos(0, 0, 0);
    
    // 创建实体
    std::vector<EntityID> entities;
    for (int i = 0; i < state.range(0); ++i) {
        EntityID entity = world.CreateEntity();
        world.AddComponent<TransformComponent>(entity);
        world.AddComponent<LODComponent>(entity);
        entities.push_back(entity);
    }
    
    for (auto _ : state) {
        LODSelector::BatchCalculateLOD(entities, &world, cameraPos, 0);
    }
    
    state.SetComplexityN(state.range(0));
}
BENCHMARK(BM_LODSelection_Batch)->Range(100, 10000)->Complexity();
```

### 测试场景

1. **简单场景**：100 个相同网格实体，3 个 LOD 级别
2. **中等场景**：1000 个相同网格实体，4 个 LOD 级别
3. **复杂场景**：5000 个不同网格实体，混合 LOD 级别
4. **压力测试**：10000+ 实体，测试 LOD 选择性能

---

## 📈 预期效果

### 性能提升

| 场景 | 优化前 Draw Call | 优化后 Draw Call | 提升 |
|------|-----------------|-----------------|------|
| **1000 相同网格（无 LOD）** | 1000 | 1000 | 1x |
| **1000 相同网格（LOD 实例化）** | 1000 | 10-50 | **20-100x** |
| **5000 相同网格（LOD 实例化）** | 5000 | 20-100 | **50-250x** |

| 场景 | 优化前帧时间 | 优化后帧时间 | 提升 |
|------|------------|------------|------|
| **1000 实体（无 LOD）** | 16ms | 16ms | 1x |
| **1000 实体（LOD 实例化）** | 16ms | 8-10ms | **1.6-2x** |
| **5000 实体（LOD 实例化）** | 80ms | 20-30ms | **2.7-4x** |

### 内存使用

| 项目 | 优化前 | 优化后 | 变化 |
|------|--------|--------|------|
| **LOD 网格数据** | 0 | 基础网格的 50-80% | +50-80%（按 LOD 级别） |
| **LOD 组件** | 0 | 每实体 ~64 字节 | +64 字节/实体 |
| **实例化缓冲** | 0 | 每实例 64 字节（矩阵） | +64 字节/实例 |
| **总体影响** | 基准 | +5-10% | 可接受 |

---

## 🚀 实施建议

### 分支策略

```
main (production)
  ↑
  merge after full test
  ↑
feature/lod-instanced-rendering
  ├── phase1-lod-basic (P1)
  ├── phase2-lod-instancing (P1)
  └── phase3-lod-advanced (P2)
```

### 代码审查清单

- [ ] 所有公共接口行为保持不变
- [ ] 现有单元测试全部通过
- [ ] 新增性能测试达到目标
- [ ] 内存使用在可接受范围内
- [ ] 线程安全验证通过
- [ ] LOD 切换平滑，无视觉瑕疵
- [ ] 文档更新完整

### 回滚计划

每个阶段使用编译期开关，允许快速回滚：

```cpp
// config.h
#define ENABLE_LOD_SYSTEM 1              // 阶段 1
#define ENABLE_LOD_INSTANCING 1           // 阶段 2
#define ENABLE_LOD_MORPHING 0             // 阶段 3（可选）
#define ENABLE_LOD_FRUSTUM_CULLING 1      // 阶段 3
```

---

## 📝 使用示例

### 基础使用

```cpp
// 创建实体并配置 LOD
EntityID entity = world.CreateEntity();

// 添加 Transform 和 MeshRender 组件
world.AddComponent<TransformComponent>(entity);
world.AddComponent<MeshRenderComponent>(entity);

// 配置 LOD
LODComponent lodComp;
lodComp.config.enabled = true;
lodComp.config.distanceThresholds = {50.0f, 150.0f, 500.0f, 1000.0f};

// 加载 LOD 网格（可选）
LODLoadOptions options;
options.basePath = "models/tree";
options.namingPattern = "{name}_lod{level}.obj";
lodComp.config.lodMeshes = LODLoader::LoadLODConfig(
    baseMesh,
    options
).lodMeshes;

world.AddComponent<LODComponent>(entity, lodComp);
```

### 批量配置

```cpp
// 批量配置相同类型的实体
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
```

---

## 🔗 相关文档

- [RenderCore_ECS_Transform优化方案.md](RenderCore_ECS_Transform优化方案.md) - Transform 优化详情
- [RENDERER_OPTIMIZATION_PLAN.md](../RENDERER_OPTIMIZATION_PLAN.md) - 渲染器优化方案
- [RenderBatching.md](../api/RenderBatching.md) - 批处理系统文档
- [ECS_INTEGRATION.md](../ECS_INTEGRATION.md) - ECS 系统使用指南
- [Model.md](../api/Model.md) - Model API 文档
- [Mesh.md](../api/Mesh.md) - Mesh API 文档

---

## 📝 总结

### 关键优化

1. **LOD 基础系统** - 距离驱动的 LOD 选择，减少渲染复杂度
2. **LOD 实例化渲染** - 将相同 LOD 级别的实例批量渲染，大幅减少 Draw Call
3. **批处理集成** - 与现有批处理系统无缝集成
4. **性能优化** - 批量计算、帧级缓存、平滑过渡

### 零破坏承诺

- ✅ 所有公共 API 签名不变
- ✅ 所有现有行为保持一致
- ✅ 现有代码无需修改（LOD 为可选功能）
- ✅ 编译期向后兼容

### 下一步

1. 获得团队对方案的 approval
2. 创建 feature 分支
3. 按阶段实施，每阶段独立测试
4. 性能对比和文档更新
5. Code review 后合并到主分支

---

**文档版本**: v1.0  
**最后更新**: 2025-11-28  
**作者**: AI Assistant  
**审核状态**: 待审核

