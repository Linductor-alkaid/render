#pragma once

#include "render/types.h"
#include "render/mesh.h"
#include "render/model.h"
#include "render/material.h"
#include "render/texture.h"
#include "render/camera.h"
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
#include <map>

namespace Render {

// 前向声明
class Mesh;
class Model;
class Material;
class Texture;
class Camera;

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
    
    // ==================== 视锥体裁剪配置（阶段3.3扩展）====================
    
    /**
     * @brief 视锥体外的行为模式
     * 
     * 控制视锥体外的实体如何处理：
     * - Cull: 完全剔除（不渲染）
     * - UseLowerLOD: 使用更低的LOD级别（降低细节但保持光影效果）
     * - UseMinimalLOD: 使用最低LOD级别（LOD3，极简渲染）
     */
    enum class FrustumOutBehavior {
        Cull,              ///< 完全剔除（默认）
        UseLowerLOD,      ///< 使用更低的LOD级别（降低1-2级）
        UseMinimalLOD     ///< 使用最低LOD级别（LOD3）
    };
    
    /**
     * @brief 视锥体外的行为
     * 
     * 当实体不在视锥体内时，如何处理：
     * - Cull: 完全剔除，不渲染（默认，性能最好）
     * - UseLowerLOD: 使用更低的LOD级别，降低细节但保持光影效果
     * - UseMinimalLOD: 使用最低LOD级别（LOD3），极简渲染
     */
    FrustumOutBehavior frustumOutBehavior = FrustumOutBehavior::Cull;
    
    /**
     * @brief 视锥体外LOD降级级别数
     * 
     * 当 frustumOutBehavior == UseLowerLOD 时，降低多少级LOD
     * 例如：如果正常是LOD1，降级2级后使用LOD3
     * 默认值：2（降低2级）
     */
    int frustumOutLODReduction = 2;
    
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
     * @brief 是否受视锥体裁剪影响
     * 
     * 如果为 false，该实体不受视锥体裁剪影响，始终按距离计算LOD
     * 用于重要的物体（如UI、特效等）需要保证渲染
     * 
     * 默认值：true（受视锥体裁剪影响）
     */
    bool affectedByFrustumCulling = true;
    
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
        const Vector3& /* entityPosition */,
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

/**
 * @brief LOD 视锥体裁剪系统
 * 
 * 结合视锥体裁剪和 LOD 选择，进一步提升性能
 * 在视锥体裁剪的同时计算 LOD 级别，返回按 LOD 分组的可见实体列表
 * 
 * **使用示例**：
 * @code
 * // 获取主相机
 * Camera* mainCamera = GetMainCamera();
 * 
 * // 批量进行视锥体裁剪和 LOD 选择
 * auto visibleEntitiesByLOD = LODFrustumCullingSystem::BatchCullAndSelectLOD(
 *     entities,
 *     &world,
 *     mainCamera,
 *     frameId
 * );
 * 
 * // 按 LOD 级别渲染
 * for (const auto& [lodLevel, entities] : visibleEntitiesByLOD) {
 *     RenderLODGroup(lodLevel, entities);
 * }
 * @endcode
 */
class LODFrustumCullingSystem {
public:
    /**
     * @brief 批量进行视锥体裁剪和 LOD 选择
     * 
     * 对实体列表同时进行视锥体裁剪和 LOD 级别计算，返回按 LOD 分组的可见实体列表
     * 
     * @param entities 实体列表（应该包含 TransformComponent，可选 LODComponent）
     * @param world ECS World 对象指针
     * @param camera 相机对象（用于获取视锥体和位置）
     * @param frameId 当前帧 ID（用于避免重复计算）
     * @return 按 LOD 级别分组的可见实体列表
     * 
     * @note 如果实体没有 LODComponent，会被归类到 LODLevel::LOD0
     * @note 如果实体不在视锥体内，会被跳过（不包含在结果中）
     * @note 如果实体的 LOD 级别是 Culled，会被跳过（不包含在结果中）
     * 
     * **性能优化**：
     * - 先进行视锥体裁剪，减少需要计算 LOD 的实体数量
     * - 批量计算距离和 LOD 级别，提升缓存命中率
     * - 使用帧 ID 避免重复计算
     */
    static std::map<LODLevel, std::vector<ECS::EntityID>> BatchCullAndSelectLOD(
        const std::vector<ECS::EntityID>& entities,
        ECS::World* world,
        const Camera* camera,
        uint64_t frameId
    ) {
        std::map<LODLevel, std::vector<ECS::EntityID>> result;
        
        if (!world || !camera || entities.empty()) {
            return result;
        }
        
        // 获取视锥体和相机位置
        const Frustum& frustum = camera->GetFrustum();
        Vector3 cameraPos = camera->GetPosition();
        
        // 遍历所有实体
        for (ECS::EntityID entity : entities) {
            // 检查是否有 Transform 组件
            if (!world->HasComponent<ECS::TransformComponent>(entity)) {
                continue;
            }
            
            auto& transformComp = world->GetComponent<ECS::TransformComponent>(entity);
            if (!transformComp.transform) {
                continue;
            }
            
            // 获取实体位置和包围球半径
            Vector3 entityPos = transformComp.GetPosition();
            
            // 尝试获取包围球半径（从 MeshRenderComponent 或使用默认值）
            float radius = 1.0f;  // 默认半径
            if (world->HasComponent<ECS::MeshRenderComponent>(entity)) {
                auto& meshComp = world->GetComponent<ECS::MeshRenderComponent>(entity);
                if (meshComp.mesh) {
                    // 尝试从网格获取包围盒
                    AABB bounds = meshComp.mesh->CalculateBounds();
                    // 检查包围盒是否有效（min <= max）
                    if (bounds.min.x() <= bounds.max.x() && 
                        bounds.min.y() <= bounds.max.y() && 
                        bounds.min.z() <= bounds.max.z()) {
                        Vector3 boundsSize = bounds.GetSize();
                        // 使用包围盒对角线的一半作为半径（与 MeshRenderSystem 保持一致）
                        radius = boundsSize.norm() * 0.5f;
                        
                        // 考虑Transform的缩放（与 MeshRenderSystem 保持一致）
                        if (transformComp.transform) {
                            Vector3 scale = transformComp.transform->GetScale();
                            float maxScale = std::max(std::max(scale.x(), scale.y()), scale.z());
                            radius *= maxScale;
                        }
                    }
                }
            }
            
            // ==================== 近距离保护：相机附近的物体永不剔除 ====================
            // 与 MeshRenderSystem::ShouldCull 保持一致
            Vector3 cameraPos = camera->GetPosition();
            float distanceToCamera = (entityPos - cameraPos).norm();
            const float noCullRadius = 5.0f;  // 5米内的物体不剔除
            
            bool skipFrustumCull = false;
            if (distanceToCamera < noCullRadius + radius) {
                // 物体在相机附近，跳过视锥体裁剪，直接处理LOD选择
                skipFrustumCull = true;
            }
            
            // ==================== 视锥体裁剪 ====================
            bool isInFrustum = true;
            bool shouldCull = false;
            
            // 检查是否受视锥体裁剪影响
            bool affectedByCulling = true;
            LODConfig::FrustumOutBehavior frustumOutBehavior = LODConfig::FrustumOutBehavior::Cull;
            int frustumOutLODReduction = 2;
            
            if (world->HasComponent<ECS::LODComponent>(entity)) {
                auto& lodComp = world->GetComponent<ECS::LODComponent>(entity);
                affectedByCulling = lodComp.affectedByFrustumCulling;
                frustumOutBehavior = lodComp.config.frustumOutBehavior;
                frustumOutLODReduction = lodComp.config.frustumOutLODReduction;
            }
            
            if (!skipFrustumCull && affectedByCulling) {
                // 扩大包围球半径以避免过度剔除（与 MeshRenderSystem 保持一致）
                // 增加更大的安全边距，避免边缘物体被过度剔除
                // 注意：从1.5倍增加到2.5倍，提供更大的安全边距，避免下边和左右两边的物体被过度剔除
                float expandedRadius = radius * 2.5f;  // 增加到2.5倍，提供更大的安全边距
                
                isInFrustum = frustum.IntersectsSphere(entityPos, expandedRadius);
                
                if (!isInFrustum) {
                    // 实体不在视锥体内
                    // 根据配置决定如何处理
                    if (frustumOutBehavior == LODConfig::FrustumOutBehavior::Cull) {
                        // 完全剔除，跳过
                        shouldCull = true;
                    }
                    // 否则继续处理，使用降低的LOD级别
                }
            }
            
            if (shouldCull) {
                continue;
            }
            
            // ==================== LOD 选择 ====================
            LODLevel lodLevel = LODLevel::LOD0;  // 默认 LOD0
            bool useReducedLOD = false;  // 是否使用降低的LOD级别
            
            if (world->HasComponent<ECS::LODComponent>(entity)) {
                auto& lodComp = world->GetComponent<ECS::LODComponent>(entity);
                
                if (lodComp.config.enabled) {
                    // 计算距离
                    float distance = LODSelector::CalculateDistance(entityPos, cameraPos);
                    
                    // 计算 LOD 级别
                    lodLevel = lodComp.config.CalculateLOD(distance);
                    
                    // 如果不在视锥体内，根据配置降低LOD级别
                    if (!isInFrustum && affectedByCulling) {
                        if (frustumOutBehavior == LODConfig::FrustumOutBehavior::UseLowerLOD) {
                            // 降低指定的LOD级别数
                            int currentLODIndex = static_cast<int>(lodLevel);
                            int reducedLODIndex = currentLODIndex + frustumOutLODReduction;
                            // 限制在有效范围内
                            reducedLODIndex = std::min(reducedLODIndex, static_cast<int>(LODLevel::LOD3));
                            lodLevel = static_cast<LODLevel>(reducedLODIndex);
                            useReducedLOD = true;
                        } else if (frustumOutBehavior == LODConfig::FrustumOutBehavior::UseMinimalLOD) {
                            // 使用最低LOD级别
                            lodLevel = LODLevel::LOD3;
                            useReducedLOD = true;
                        }
                    }
                    
                    // 如果 LOD 级别是 Culled，跳过
                    if (lodLevel == LODLevel::Culled) {
                        continue;
                    }
                    
                    // 更新 LOD 组件（与 LODSelector::BatchCalculateLOD 保持一致）
                    // 第一次计算时（lastDistance为0），直接更新LOD级别
                    bool isFirstUpdate = (lodComp.lastDistance == 0.0f);
                    
                    if (isFirstUpdate || lodLevel != lodComp.currentLOD) {
                        lodComp.currentLOD = lodLevel;
                        lodComp.lodSwitchCount++;
                        lodComp.lastLOD = lodComp.currentLOD;
                    }
                    
                    lodComp.lastDistance = distance;
                    lodComp.lastUpdateFrame = frameId;
                }
            } else if (!isInFrustum && affectedByCulling) {
                // 没有LODComponent但不在视锥体内，根据默认行为处理
                if (frustumOutBehavior == LODConfig::FrustumOutBehavior::Cull) {
                    continue;  // 默认完全剔除
                } else if (frustumOutBehavior == LODConfig::FrustumOutBehavior::UseMinimalLOD) {
                    lodLevel = LODLevel::LOD3;  // 使用最低LOD
                }
            }
            
            // 将实体添加到对应 LOD 级别的列表中
            result[lodLevel].push_back(entity);
        }
        
        return result;
    }
    
    /**
     * @brief 批量进行视锥体裁剪和 LOD 选择（使用包围盒版本）
     * 
     * 与 BatchCullAndSelectLOD 类似，但使用包围盒进行更准确的视锥体裁剪和距离计算
     * 
     * @param entities 实体列表
     * @param world ECS World 对象指针
     * @param camera 相机对象
     * @param frameId 当前帧 ID
     * @param getBounds 函数，用于获取实体的包围盒（可选，如果为 nullptr 则使用默认包围球）
     * @return 按 LOD 级别分组的可见实体列表
     * 
     * @note getBounds 函数签名：AABB(EntityID entity)
     * @note 如果 getBounds 为 nullptr，则使用默认的包围球进行视锥体裁剪
     */
    static std::map<LODLevel, std::vector<ECS::EntityID>> BatchCullAndSelectLODWithBounds(
        const std::vector<ECS::EntityID>& entities,
        ECS::World* world,
        const Camera* camera,
        uint64_t frameId,
        std::function<AABB(ECS::EntityID)> getBounds = nullptr
    ) {
        std::map<LODLevel, std::vector<ECS::EntityID>> result;
        
        if (!world || !camera || entities.empty()) {
            return result;
        }
        
        // 获取视锥体和相机位置
        const Frustum& frustum = camera->GetFrustum();
        Vector3 cameraPos = camera->GetPosition();
        
        // 遍历所有实体
        for (ECS::EntityID entity : entities) {
            // 检查是否有 Transform 组件
            if (!world->HasComponent<ECS::TransformComponent>(entity)) {
                continue;
            }
            
            auto& transformComp = world->GetComponent<ECS::TransformComponent>(entity);
            if (!transformComp.transform) {
                continue;
            }
            
            // 获取实体位置
            Vector3 entityPos = transformComp.GetPosition();
            
            // ==================== 视锥体裁剪（使用包围盒）====================
            bool isVisible = false;
            float radius = 1.0f;  // 默认半径（用于距离计算）
            Vector3 cameraPos = camera->GetPosition();
            float distanceToCamera = (entityPos - cameraPos).norm();
            const float noCullRadius = 5.0f;  // 近距离保护半径
            
            if (getBounds) {
                // 使用提供的包围盒函数
                AABB bounds = getBounds(entity);
                // 检查包围盒是否有效（min <= max）
                if (bounds.min.x() <= bounds.max.x() && 
                    bounds.min.y() <= bounds.max.y() && 
                    bounds.min.z() <= bounds.max.z()) {
                    // 计算包围盒大小（用于距离计算）
                    Vector3 boundsSize = bounds.GetSize();
                    radius = boundsSize.norm() * 0.5f;  // 使用对角线的一半
                    
                    // 考虑Transform的缩放
                    if (transformComp.transform) {
                        Vector3 scale = transformComp.transform->GetScale();
                        float maxScale = std::max(std::max(scale.x(), scale.y()), scale.z());
                        radius *= maxScale;
                    }
                    
                    // ==================== 近距离保护 ====================
                    if (distanceToCamera < noCullRadius + radius) {
                        // 物体在相机附近，不剔除
                        isVisible = true;
                    } else {
                        // 扩大包围盒进行视锥体裁剪，避免过度剔除
                        // 使用包围盒的扩展版本（增加安全边距）
                        AABB expandedBounds;
                        Vector3 extents = bounds.GetExtents();
                        Vector3 center = bounds.GetCenter();
                        // 扩大包围盒（增加25%的安全边距）
                        Vector3 expandedExtents = extents * 1.25f;
                        expandedBounds.min = center - expandedExtents;
                        expandedBounds.max = center + expandedExtents;
                        
                        isVisible = frustum.IntersectsAABB(expandedBounds);
                    }
                } else {
                    // 包围盒无效，使用默认包围球
                    radius = 1.0f;
                    
                    // ==================== 近距离保护 ====================
                    if (distanceToCamera < noCullRadius + radius) {
                        isVisible = true;
                    } else {
                        float expandedRadius = radius * 2.5f;  // 增加到2.5倍
                        isVisible = frustum.IntersectsSphere(entityPos, expandedRadius);
                    }
                }
            } else {
                // 没有提供包围盒函数，使用默认包围球
                radius = 1.0f;
                
                // ==================== 近距离保护 ====================
                if (distanceToCamera < noCullRadius + radius) {
                    // 物体在相机附近，不剔除
                    isVisible = true;
                } else {
                    // 扩大包围球半径以避免过度剔除
                    float expandedRadius = radius * 2.5f;  // 增加到2.5倍
                    isVisible = frustum.IntersectsSphere(entityPos, expandedRadius);
                }
            }
            
            // ==================== 视锥体裁剪处理 ====================
            bool shouldCull = false;
            bool affectedByCulling = true;
            LODConfig::FrustumOutBehavior frustumOutBehavior = LODConfig::FrustumOutBehavior::Cull;
            int frustumOutLODReduction = 2;
            
            if (world->HasComponent<ECS::LODComponent>(entity)) {
                auto& lodComp = world->GetComponent<ECS::LODComponent>(entity);
                affectedByCulling = lodComp.affectedByFrustumCulling;
                frustumOutBehavior = lodComp.config.frustumOutBehavior;
                frustumOutLODReduction = lodComp.config.frustumOutLODReduction;
            }
            
            if (!isVisible && affectedByCulling) {
                // 实体不在视锥体内
                // 根据配置决定如何处理
                if (frustumOutBehavior == LODConfig::FrustumOutBehavior::Cull) {
                    // 完全剔除，跳过
                    shouldCull = true;
                }
                // 否则继续处理，使用降低的LOD级别
            }
            
            if (shouldCull) {
                continue;
            }
            
            // ==================== LOD 选择 ====================
            LODLevel lodLevel = LODLevel::LOD0;  // 默认 LOD0
            
            if (world->HasComponent<ECS::LODComponent>(entity)) {
                auto& lodComp = world->GetComponent<ECS::LODComponent>(entity);
                
                if (lodComp.config.enabled) {
                    // 计算距离（使用包围盒版本）
                    float distance;
                    
                    if (getBounds) {
                        AABB bounds = getBounds(entity);
                        // 检查包围盒是否有效（min <= max）
                        if (bounds.min.x() <= bounds.max.x() && 
                            bounds.min.y() <= bounds.max.y() && 
                            bounds.min.z() <= bounds.max.z()) {
                            distance = LODSelector::CalculateDistanceWithBounds(
                                entityPos,
                                bounds,
                                cameraPos,
                                lodComp.config.boundingBoxScale
                            );
                        } else {
                            distance = LODSelector::CalculateDistance(entityPos, cameraPos);
                        }
                    } else {
                        distance = LODSelector::CalculateDistance(entityPos, cameraPos);
                    }
                    
                    // 计算 LOD 级别
                    lodLevel = lodComp.config.CalculateLOD(distance);
                    
                    // 如果不在视锥体内，根据配置降低LOD级别
                    if (!isVisible && affectedByCulling) {
                        if (frustumOutBehavior == LODConfig::FrustumOutBehavior::UseLowerLOD) {
                            // 降低指定的LOD级别数
                            int currentLODIndex = static_cast<int>(lodLevel);
                            int reducedLODIndex = currentLODIndex + frustumOutLODReduction;
                            // 限制在有效范围内
                            reducedLODIndex = std::min(reducedLODIndex, static_cast<int>(LODLevel::LOD3));
                            lodLevel = static_cast<LODLevel>(reducedLODIndex);
                        } else if (frustumOutBehavior == LODConfig::FrustumOutBehavior::UseMinimalLOD) {
                            // 使用最低LOD级别
                            lodLevel = LODLevel::LOD3;
                        }
                    }
                    
                    // 如果 LOD 级别是 Culled，跳过
                    if (lodLevel == LODLevel::Culled) {
                        continue;
                    }
                    
                    // 更新 LOD 组件（与 LODSelector::BatchCalculateLODWithBounds 保持一致）
                    bool isFirstUpdate = (lodComp.lastDistance == 0.0f);
                    
                    if (isFirstUpdate) {
                        lodComp.currentLOD = lodLevel;
                        lodComp.lodSwitchCount++;
                        lodComp.lastLOD = lodComp.currentLOD;
                    } else if (lodLevel != lodComp.currentLOD) {
                        lodComp.currentLOD = lodLevel;
                        lodComp.lodSwitchCount++;
                        lodComp.lastLOD = lodComp.currentLOD;
                    }
                    
                    lodComp.lastDistance = distance;
                    lodComp.lastUpdateFrame = frameId;
                }
            } else if (!isVisible && affectedByCulling) {
                // 没有LODComponent但不在视锥体内，根据默认行为处理
                if (frustumOutBehavior == LODConfig::FrustumOutBehavior::Cull) {
                    continue;  // 默认完全剔除
                } else if (frustumOutBehavior == LODConfig::FrustumOutBehavior::UseMinimalLOD) {
                    lodLevel = LODLevel::LOD3;  // 使用最低LOD
                }
            }
            
            // 将实体添加到对应 LOD 级别的列表中
            result[lodLevel].push_back(entity);
        }
        
        return result;
    }
};

} // namespace Render

