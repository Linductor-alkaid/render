/*
 * Copyright (c) 2025 Li Chaoyu
 * 
 * This file is part of Render.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * For commercial licensing, please contact: 2052046346@qq.com
 */
#pragma once

#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "physics_components.h"
#include "render/math_utils.h"
#include <unordered_map>

namespace Render {
namespace Physics {

/**
 * @brief 物理-渲染变换同步类
 * 
 * 负责在物理模拟和渲染系统之间同步变换数据
 * 
 * **功能**：
 * - 物理 → 渲染：将动态物体的物理位置/旋转同步到 Transform
 * - 渲染 → 物理：将 Kinematic/Static 物体的 Transform 同步到物理状态
 * - 插值平滑：在固定时间步长和渲染帧率之间进行插值，提升视觉平滑度
 * 
 * **父子关系处理**：
 * - 物理仅影响根对象（无父实体的对象）
 * - 子对象的变换由父对象通过 Transform 层级自动计算
 */
class PhysicsTransformSync {
public:
    /**
     * @brief 构造函数
     */
    PhysicsTransformSync() = default;
    
    /**
     * @brief 析构函数
     */
    ~PhysicsTransformSync() = default;
    
    /**
     * @brief 物理 → 渲染同步
     * 
     * 将动态物体的物理位置和旋转同步到 TransformComponent
     * 只处理根对象（无父实体的对象）
     * 
     * @param world ECS 世界指针
     */
    void SyncPhysicsToTransform(ECS::World* world);
    
    /**
     * @brief 渲染 → 物理同步
     * 
     * 将 Kinematic/Static 物体的 Transform 同步到物理状态
     * 对于 Kinematic 物体，还会计算速度（基于位置变化）
     * 
     * @param world ECS 世界指针
     * @param deltaTime 时间步长（用于计算速度）
     */
    void SyncTransformToPhysics(ECS::World* world, float deltaTime);
    
    /**
     * @brief 插值变换（平滑渲染）
     * 
     * 在固定时间步长和渲染帧率之间进行插值
     * 使用上一帧和当前帧的物理状态进行线性/球面插值
     * 
     * @param world ECS 世界指针
     * @param alpha 插值因子 [0, 1]，0 表示上一帧，1 表示当前帧
     */
    void InterpolateTransforms(ECS::World* world, float alpha);
    
    /**
     * @brief 清除缓存的变换状态
     * 
     * 当实体被销毁时调用，清理相关缓存
     */
    void ClearCache();

private:
    /**
     * @brief 缓存的变换状态（用于插值）
     */
    struct CachedTransformState {
        Vector3 position{0.0f, 0.0f, 0.0f};
        Quaternion rotation{1.0f, 0.0f, 0.0f, 0.0f};
    };
    
    /**
     * @brief 检查实体是否为根对象（无父实体）
     */
    bool IsRootEntity(ECS::World* world, ECS::EntityID entity) const;
    
    /**
     * @brief 缓存的上一帧变换状态（用于插值）
     */
    std::unordered_map<ECS::EntityID, CachedTransformState, ECS::EntityID::Hash> m_previousTransforms;
    
    /**
     * @brief 缓存的当前帧变换状态（用于插值）
     */
    std::unordered_map<ECS::EntityID, CachedTransformState, ECS::EntityID::Hash> m_currentTransforms;
};

} // namespace Physics
} // namespace Render

