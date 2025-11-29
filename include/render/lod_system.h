#pragma once

#include "render/types.h"
#include "render/mesh.h"
#include "render/model.h"
#include "render/material.h"
#include "render/texture.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/ecs/entity.h"
#include <vector>
#include <optional>
#include <string>
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace Render {

// 前向声明
class Mesh;
class Model;
class Material;
class Texture;

namespace ECS {
// 前向声明
class World;
}

/**
 * @brief LOD 级别枚举
 * 
 * 定义不同的细节级别，从最高细节（LOD0）到最低细节（LOD3），以及剔除状态
 */
enum class LODLevel {
    LOD0 = 0,  ///< 最高细节（最近）
    LOD1 = 1,  ///< 中等细节
    LOD2 = 2,  ///< 低细节
    LOD3 = 3,  ///< 最低细节（最远）
    Culled = 4 ///< 剔除（超出范围）
};

/**
 * @brief LOD 纹理集合
 * 
 * 为每个 LOD 级别定义可选的纹理集合
 * 远距离可以使用更低分辨率的纹理以节省内存和带宽
 */
struct LODTextureSet {
    Ref<Texture> diffuseMap;      ///< 漫反射贴图
    Ref<Texture> normalMap;        ///< 法线贴图（LOD2+ 可省略）
    Ref<Texture> specularMap;      ///< 高光贴图（LOD2+ 可省略）
    Ref<Texture> emissiveMap;      ///< 自发光贴图（LOD2+ 可省略）
    
    LODTextureSet() = default;
};

/**
 * @brief 纹理 LOD 策略
 * 
 * 定义如何为不同 LOD 级别选择纹理
 */
enum class TextureLODStrategy {
    UseLODTextures,    ///< 使用 lodTextures 中指定的纹理
    UseMipmap,         ///< 使用原始纹理的 mipmap（自动）
    DisableTextures    ///< 远距离禁用纹理（LOD2+）
};

/**
 * @brief LOD 配置
 * 
 * 定义每个 LOD 级别的距离阈值、网格、材质和纹理
 * 支持 Mesh 和 Model 两种类型的 LOD
 */
struct LODConfig {
    // ==================== 距离阈值 ====================
    
    /**
     * @brief LOD 级别距离阈值（从近到远）
     * 
     * 例如：{50.0f, 150.0f, 500.0f, 1000.0f}
     * - 距离 < 50: LOD0
     * - 50 <= 距离 < 150: LOD1
     * - 150 <= 距离 < 500: LOD2
     * - 500 <= 距离 < 1000: LOD3
     * - 距离 >= 1000: Culled
     */
    std::vector<float> distanceThresholds{50.0f, 150.0f, 500.0f, 1000.0f};
    
    // ==================== LOD 网格/模型 ====================
    
    /**
     * @brief 每个 LOD 级别对应的网格（可选）
     * 
     * 如果为空或某个级别为 nullptr，则使用原始网格
     * 索引对应 LOD 级别：0=LOD0, 1=LOD1, 2=LOD2, 3=LOD3
     */
    std::vector<Ref<Mesh>> lodMeshes;
    
    /**
     * @brief 每个 LOD 级别对应的模型（可选，用于 ModelComponent）
     * 
     * 如果为空或某个级别为 nullptr，则使用原始模型
     * 索引对应 LOD 级别：0=LOD0, 1=LOD1, 2=LOD2, 3=LOD3
     */
    std::vector<Ref<Model>> lodModels;
    
    // ==================== LOD 材质 ====================
    
    /**
     * @brief 每个 LOD 级别对应的材质（可选）
     * 
     * 远距离可以使用更简单的材质（减少纹理、禁用某些效果）
     * 如果为空或某个级别为 nullptr，则使用原始材质
     * 索引对应 LOD 级别：0=LOD0, 1=LOD1, 2=LOD2, 3=LOD3
     */
    std::vector<Ref<Material>> lodMaterials;
    
    // ==================== LOD 纹理 ====================
    
    /**
     * @brief 每个 LOD 级别对应的纹理集合（可选）
     * 
     * 例如：LOD0 使用 4K 纹理，LOD1 使用 2K，LOD2 使用 1K
     * 索引对应 LOD 级别：0=LOD0, 1=LOD1, 2=LOD2, 3=LOD3
     */
    std::vector<LODTextureSet> lodTextures;
    
    /**
     * @brief 纹理 LOD 策略
     * 
     * 控制如何为不同 LOD 级别选择纹理
     */
    TextureLODStrategy textureStrategy = TextureLODStrategy::UseMipmap;
    
    // ==================== 切换参数 ====================
    
    /**
     * @brief LOD 切换的平滑过渡距离（避免频繁切换）
     * 
     * 单位：世界单位
     * 例如：transitionDistance = 10.0f 表示距离变化超过 10 单位才切换 LOD
     */
    float transitionDistance = 10.0f;
    
    /**
     * @brief 包围盒缩放因子（用于距离计算，考虑对象大小）
     * 
     * 较大的对象应该使用更大的距离阈值
     * 默认值 1.0 表示不缩放
     */
    float boundingBoxScale = 1.0f;
    
    /**
     * @brief 是否启用 LOD
     * 
     * 如果为 false，始终使用 LOD0（最高细节）
     */
    bool enabled = true;
    
    // ==================== 辅助方法 ====================
    
    /**
     * @brief 根据距离计算 LOD 级别
     * @param distance 到相机的距离（世界单位）
     * @return LOD 级别
     */
    [[nodiscard]] LODLevel CalculateLOD(float distance) const {
        if (!enabled) {
            return LODLevel::LOD0;
        }
        
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
    [[nodiscard]] Ref<Mesh> GetLODMesh(LODLevel level, Ref<Mesh> defaultMesh) const {
        if (lodMeshes.empty()) {
            return defaultMesh;
        }
        
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
    [[nodiscard]] Ref<Model> GetLODModel(LODLevel level, Ref<Model> defaultModel) const {
        if (lodModels.empty()) {
            return defaultModel;
        }
        
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
    [[nodiscard]] Ref<Material> GetLODMaterial(LODLevel level, Ref<Material> defaultMaterial) const {
        if (lodMaterials.empty()) {
            return defaultMaterial;
        }
        
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
     * 
     * @note 只有在 textureStrategy == UseLODTextures 时才会应用
     * @note 对于 LOD2+，会自动禁用某些纹理以节省采样
     */
    void ApplyLODTextures(LODLevel level, Ref<Material> material) const {
        if (lodTextures.empty() || textureStrategy != TextureLODStrategy::UseLODTextures) {
            return;
        }
        
        if (!material) {
            return;
        }
        
        size_t index = static_cast<size_t>(level);
        if (index >= lodTextures.size()) {
            return;
        }
        
        const LODTextureSet& textureSet = lodTextures[index];
        
        // 应用纹理到材质
        if (textureSet.diffuseMap) {
            material->SetTexture("diffuseMap", textureSet.diffuseMap);
        }
        if (textureSet.normalMap) {
            material->SetTexture("normalMap", textureSet.normalMap);
        }
        if (textureSet.specularMap) {
            material->SetTexture("specularMap", textureSet.specularMap);
        }
        if (textureSet.emissiveMap) {
            material->SetTexture("emissiveMap", textureSet.emissiveMap);
        }
        
        // 根据 LOD 级别禁用某些纹理（节省采样）
        if (level >= LODLevel::LOD2) {
            // LOD2+ 可以禁用法线贴图和高光贴图
            if (!textureSet.normalMap) {
                material->SetInt("uUseNormalMap", 0);
            }
            if (!textureSet.specularMap) {
                material->SetInt("uUseSpecularMap", 0);
            }
        }
    }
};

namespace ECS {

/**
 * @brief LOD 组件（ECS）
 * 
 * 附加到实体上，提供 LOD 配置和当前 LOD 级别
 * 与 MeshRenderComponent 或 ModelComponent 配合使用
 * 
 * **使用示例**：
 * @code
 * EntityID entity = world.CreateEntity();
 * 
 * // 添加 Transform 和 MeshRender 组件
 * world.AddComponent<TransformComponent>(entity);
 * world.AddComponent<MeshRenderComponent>(entity);
 * 
 * // 配置 LOD
 * LODComponent lodComp;
 * lodComp.config.enabled = true;
 * lodComp.config.distanceThresholds = {50.0f, 150.0f, 500.0f, 1000.0f};
 * 
 * // 加载 LOD 网格（可选）
 * lodComp.config.lodMeshes.push_back(LoadMesh("tree_lod1.obj"));
 * lodComp.config.lodMeshes.push_back(LoadMesh("tree_lod2.obj"));
 * 
 * world.AddComponent<LODComponent>(entity, lodComp);
 * @endcode
 */
struct LODComponent {
    /**
     * @brief LOD 配置
     */
    LODConfig config;
    
    /**
     * @brief 当前 LOD 级别
     * 
     * 由 LODSelector 系统自动更新
     */
    LODLevel currentLOD = LODLevel::LOD0;
    
    /**
     * @brief 上次计算的距离
     * 
     * 用于平滑过渡，避免频繁切换
     */
    float lastDistance = 0.0f;
    
    /**
     * @brief 上次更新的帧 ID
     * 
     * 避免每帧都更新，可以每 N 帧更新一次
     */
    uint64_t lastUpdateFrame = 0;
    
    // ==================== 统计信息（调试用）====================
    
    /**
     * @brief LOD 切换次数
     * 
     * 用于性能分析和调试
     */
    uint32_t lodSwitchCount = 0;
    
    /**
     * @brief 上次的 LOD 级别
     * 
     * 用于检测 LOD 切换
     */
    LODLevel lastLOD = LODLevel::LOD0;
    
    /**
     * @brief 默认构造函数
     */
    LODComponent() = default;
    
    /**
     * @brief 使用配置构造
     * @param lodConfig LOD 配置
     */
    explicit LODComponent(const LODConfig& lodConfig) : config(lodConfig) {}
    
    /**
     * @brief 检查 LOD 是否启用
     * @return 如果启用返回 true
     */
    [[nodiscard]] bool IsEnabled() const {
        return config.enabled;
    }
    
    /**
     * @brief 获取当前 LOD 级别的字符串表示（用于调试）
     * @return LOD 级别字符串
     */
    [[nodiscard]] std::string GetLODLevelString() const {
        switch (currentLOD) {
            case LODLevel::LOD0: return "LOD0";
            case LODLevel::LOD1: return "LOD1";
            case LODLevel::LOD2: return "LOD2";
            case LODLevel::LOD3: return "LOD3";
            case LODLevel::Culled: return "Culled";
            default: return "Unknown";
        }
    }
};

} // namespace ECS

/**
 * @brief LOD 选择器
 * 
 * 负责计算实体到相机的距离并选择 LOD 级别
 * 提供批量计算功能以优化性能
 * 
 * **使用示例**：
 * @code
 * // 获取主相机位置
 * Vector3 cameraPos = GetMainCameraPosition();
 * uint64_t frameId = GetCurrentFrameId();
 * 
 * // 批量计算 LOD
 * std::vector<EntityID> entities = world.Query<LODComponent, TransformComponent>();
 * LODSelector::BatchCalculateLOD(entities, &world, cameraPos, frameId);
 * @endcode
 */
class LODSelector {
public:
    /**
     * @brief 计算实体到相机的距离
     * @param entityPosition 实体世界位置
     * @param cameraPosition 相机世界位置
     * @return 距离（单位：世界单位）
     */
    [[nodiscard]] static float CalculateDistance(
        const Vector3& entityPosition,
        const Vector3& cameraPosition
    ) {
        return (entityPosition - cameraPosition).norm();
    }
    
    /**
     * @brief 计算实体到相机的距离（考虑包围盒）
     * 
     * 考虑对象大小，使用包围盒中心到相机的距离，并减去包围盒的最大轴的一半
     * 这样可以更准确地反映对象的视觉大小
     * 
     * @param entityPosition 实体世界位置
     * @param entityBounds 实体包围盒（世界空间）
     * @param cameraPosition 相机世界位置
     * @param boundingBoxScale 包围盒缩放因子（默认 1.0）
     * @return 调整后的距离（单位：世界单位）
     */
    [[nodiscard]] static float CalculateDistanceWithBounds(
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
     * 
     * 对多个实体批量计算 LOD 级别，提升性能
     * 使用帧 ID 避免每帧都更新，支持平滑过渡
     * 
     * @param entities 实体列表（应该已经包含 LODComponent 和 TransformComponent）
     * @param world ECS World 对象指针
     * @param cameraPosition 相机世界位置
     * @param frameId 当前帧 ID（用于避免重复计算）
     * 
     * @note 此方法会修改实体的 LODComponent，更新 currentLOD 和统计信息
     * @note 如果实体没有 LODComponent 或 TransformComponent，会被跳过
     * @note 使用 transitionDistance 实现平滑过渡，避免频繁切换
     */
    static void BatchCalculateLOD(
        const std::vector<ECS::EntityID>& entities,
        ECS::World* world,
        const Vector3& cameraPosition,
        uint64_t frameId
    ) {
        if (!world) {
            return;
        }
        
        for (ECS::EntityID entity : entities) {
            // 检查是否有 LOD 组件
            if (!world->HasComponent<ECS::LODComponent>(entity)) {
                continue;
            }
            
            auto& lodComp = world->GetComponent<ECS::LODComponent>(entity);
            
            // ✅ 与远距离剔除保持一致：每帧都计算LOD
            // 移除帧ID检查，确保每帧都能更新LOD级别（远距离剔除也是每帧都执行）
            
            // 获取 Transform 组件
            if (!world->HasComponent<ECS::TransformComponent>(entity)) {
                continue;
            }
            
            auto& transformComp = world->GetComponent<ECS::TransformComponent>(entity);
            if (!transformComp.transform) {
                continue;
            }
            
            // ✅ 计算距离：使用与远距离剔除相同的位置获取方式
            // 使用 GetPosition() 获取位置（与远距离剔除的 ShouldCull 保持一致）
            // 这样可以确保距离计算与远距离剔除使用相同的坐标系统
            Vector3 entityPos = transformComp.GetPosition();
            float distance = CalculateDistance(entityPos, cameraPosition);
            
            // 计算 LOD 级别
            LODLevel newLOD = lodComp.config.CalculateLOD(distance);
            
            // ✅ 平滑过渡：避免频繁切换（与远距离剔除的实时性保持一致）
            // 第一次计算时（lastDistance为0），直接更新LOD级别
            bool isFirstUpdate = (lodComp.lastDistance == 0.0f);
            
            // ✅ 强制更新逻辑：第一次更新时总是更新，后续更新直接切换
            // 如果newLOD与currentLOD不同，说明距离已经跨过了阈值，应该立即切换
            if (isFirstUpdate || newLOD != lodComp.currentLOD) {
                lodComp.currentLOD = newLOD;
                lodComp.lodSwitchCount++;
                lodComp.lastLOD = lodComp.currentLOD;
            }
            
            lodComp.lastDistance = distance;
            lodComp.lastUpdateFrame = frameId;
        }
    }
    
    /**
     * @brief 批量计算 LOD 级别（考虑包围盒版本）
     * 
     * 与 BatchCalculateLOD 类似，但使用包围盒进行更准确的距离计算
     * 
     * @param entities 实体列表
     * @param world ECS World 对象指针
     * @param cameraPosition 相机世界位置
     * @param frameId 当前帧 ID
     * @param getBounds 函数，用于获取实体的包围盒（可选，如果为 nullptr 则使用位置计算）
     * 
     * @note getBounds 函数签名：AABB(EntityID entity)
     * @note 如果 getBounds 为 nullptr，则回退到普通距离计算
     */
    static void BatchCalculateLODWithBounds(
        const std::vector<ECS::EntityID>& entities,
        ECS::World* world,
        const Vector3& cameraPosition,
        uint64_t frameId,
        std::function<AABB(ECS::EntityID)> getBounds = nullptr
    ) {
        if (!world) {
            return;
        }
        
        for (ECS::EntityID entity : entities) {
            // 检查是否有 LOD 组件
            if (!world->HasComponent<ECS::LODComponent>(entity)) {
                continue;
            }
            
            auto& lodComp = world->GetComponent<ECS::LODComponent>(entity);
            
            // ✅ 与远距离剔除保持一致：每帧都计算LOD
            // 移除帧ID检查，确保每帧都能更新LOD级别（远距离剔除也是每帧都执行）
            
            // 获取 Transform 组件
            if (!world->HasComponent<ECS::TransformComponent>(entity)) {
                continue;
            }
            
            auto& transformComp = world->GetComponent<ECS::TransformComponent>(entity);
            if (!transformComp.transform) {
                continue;
            }
            
            // ✅ 计算距离：使用与远距离剔除相同的位置获取方式
            // 使用 GetPosition() 获取位置（与远距离剔除的 ShouldCull 保持一致）
            Vector3 entityPos = transformComp.GetPosition();
            float distance;
            
            if (getBounds) {
                AABB bounds = getBounds(entity);
                distance = CalculateDistanceWithBounds(
                    entityPos,
                    bounds,
                    cameraPosition,
                    lodComp.config.boundingBoxScale
                );
            } else {
                distance = CalculateDistance(entityPos, cameraPosition);
            }
            
            // 计算 LOD 级别
            LODLevel newLOD = lodComp.config.CalculateLOD(distance);
            
            // ✅ 平滑过渡：避免频繁切换（与远距离剔除的实时性保持一致）
            // 第一次计算时（lastDistance为0），直接更新LOD级别
            bool isFirstUpdate = (lodComp.lastDistance == 0.0f);
            
            // ✅ 强制更新逻辑：第一次更新时总是更新，后续更新根据距离变化决定
            if (isFirstUpdate) {
                // 第一次更新：强制更新LOD级别
                lodComp.currentLOD = newLOD;
                lodComp.lodSwitchCount++;
                lodComp.lastLOD = lodComp.currentLOD;
            } else if (newLOD != lodComp.currentLOD) {
                // ✅ 后续更新：当新LOD与当前LOD不同时，直接切换
                // 平滑过渡通过transitionDistance控制，但不应阻止LOD级别的切换
                // 如果计算出的newLOD与currentLOD不同，说明距离已经跨过了阈值，应该切换
                lodComp.currentLOD = newLOD;
                lodComp.lodSwitchCount++;
                lodComp.lastLOD = lodComp.currentLOD;
            }
            
            lodComp.lastDistance = distance;
            lodComp.lastUpdateFrame = frameId;
        }
    }
};

/**
 * @brief LOD 调试和查询工具
 * 
 * 提供查询实体 LOD 状态的辅助函数
 */
namespace LODDebug {
    /**
     * @brief 获取实体的 LOD 状态信息（用于调试）
     * @param world ECS World 对象
     * @param entity 实体 ID
     * @return LOD 状态字符串，如果实体没有 LODComponent 返回 "No LOD"
     */
    inline std::string GetEntityLODStatus(ECS::World* world, ECS::EntityID entity) {
        if (!world || !entity.IsValid()) {
            return "Invalid entity";
        }
        
        if (!world->HasComponent<ECS::LODComponent>(entity)) {
            return "No LOD";
        }
        
        auto& lodComp = world->GetComponent<ECS::LODComponent>(entity);
        if (!lodComp.config.enabled) {
            return "LOD disabled";
        }
        
        std::string status = "LOD: " + lodComp.GetLODLevelString();
        status += " (distance: " + std::to_string(lodComp.lastDistance) + ")";
        status += ", switches: " + std::to_string(lodComp.lodSwitchCount);
        
        return status;
    }
    
    /**
     * @brief 检查实体是否启用了 LOD
     * @param world ECS World 对象
     * @param entity 实体 ID
     * @return 如果启用 LOD 返回 true
     */
    inline bool IsLODEnabled(ECS::World* world, ECS::EntityID entity) {
        if (!world || !entity.IsValid()) {
            return false;
        }
        
        if (!world->HasComponent<ECS::LODComponent>(entity)) {
            return false;
        }
        
        auto& lodComp = world->GetComponent<ECS::LODComponent>(entity);
        return lodComp.config.enabled;
    }
    
    /**
     * @brief 获取实体的当前 LOD 级别
     * @param world ECS World 对象
     * @param entity 实体 ID
     * @return LOD 级别，如果实体没有 LODComponent 返回 LODLevel::LOD0
     */
    inline LODLevel GetEntityLODLevel(ECS::World* world, ECS::EntityID entity) {
        if (!world || !entity.IsValid()) {
            return LODLevel::LOD0;
        }
        
        if (!world->HasComponent<ECS::LODComponent>(entity)) {
            return LODLevel::LOD0;
        }
        
        auto& lodComp = world->GetComponent<ECS::LODComponent>(entity);
        return lodComp.currentLOD;
    }
}

} // namespace Render

