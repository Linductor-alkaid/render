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

#include "render/robot/robot_model.h"
#include "render/ecs/system.h"
#include "render/ecs/entity.h"
#include "render/transform.h"
#include <string>
#include <unordered_map>

namespace Render {
namespace ECS {

// 前向声明
class World;

namespace Robot {

/**
 * @brief 关节TF（变换）数据
 */
struct JointTF {
    std::string jointName;
    std::string linkName;
    Transform transform;  // 基座坐标系下的变换
    Matrix4 transformMatrix;  // 变换矩阵（用于渲染）
    
    JointTF() : transformMatrix(Matrix4::Identity()) {}
};

/**
 * @brief 关节TF计算系统
 * 
 * 根据关节位置计算所有关节在基座坐标系下的TF
 */
class JointTFSystem : public System {
public:
    JointTFSystem() = default;
    ~JointTFSystem() override = default;
    
    void OnCreate(World* world) override {
        System::OnCreate(world);
    }
    
    void Update(float deltaTime) override;
    
    [[nodiscard]] int GetPriority() const override { return 15; }  // 在TransformSystem之后
    
    /**
     * @brief 设置关节位置
     * @param robotEntity 机器人实体ID
     * @param jointName 关节名称
     * @param position 关节位置（角度或距离）
     */
    void SetJointPosition(EntityID robotEntity, const std::string& jointName, float position);
    
    /**
     * @brief 批量设置关节位置
     * @param robotEntity 机器人实体ID
     * @param positions 关节名称到位置的映射
     */
    void SetJointPositions(EntityID robotEntity, const std::unordered_map<std::string, float>& positions);
    
    /**
     * @brief 获取所有关节的TF（返回常量引用，因为JointTF包含不可拷贝的Transform）
     * @param robotEntity 机器人实体ID
     * @return 关节名称到TF的映射的常量引用
     */
    const std::unordered_map<std::string, JointTF>& GetAllTFs(EntityID robotEntity) const;
    
    /**
     * @brief 获取指定关节的TF
     * @param robotEntity 机器人实体ID
     * @param jointName 关节名称
     * @param outTF 输出的关节TF（如果不存在则保持原值）
     * @return 是否找到该关节
     */
    bool GetJointTF(EntityID robotEntity, const std::string& jointName, JointTF& outTF) const;
    
    /**
     * @brief 获取关节位置
     * @param robotEntity 机器人实体ID
     * @param jointName 关节名称
     * @return 关节位置
     */
    float GetJointPosition(EntityID robotEntity, const std::string& jointName) const;
    
    /**
     * @brief 更新所有TF（内部使用）
     * @param robotEntity 机器人实体ID
     */
    void UpdateAllTFs(EntityID robotEntity);

private:
    /**
     * @brief 递归计算关节TF
     * @param model 机器人模型
     * @param linkName 当前link名称
     * @param jointPositions 关节位置映射
     * @param parentTF 父link的TF（基座坐标系）
     * @param tfs 输出的TF映射
     */
    void ComputeLinkAndJointTFs(
        const Render::Robot::RobotModel& model,
        const std::string& linkName,
        const std::unordered_map<std::string, float>& jointPositions,
        const Transform& parentTF,
        std::unordered_map<std::string, JointTF>& tfs
    ) const;
    
    /**
     * @brief 计算关节变换
     * @param joint 关节信息
     * @param jointPosition 关节位置
     * @return 关节的局部变换
     */
    Transform ComputeJointTransform(const Render::Robot::URDFJoint& joint, float jointPosition) const;
    
    // 存储每个机器人的关节位置
    std::unordered_map<EntityID, std::unordered_map<std::string, float>, EntityID::Hash> m_jointPositions;
    
    // 存储每个机器人的TF缓存
    mutable std::unordered_map<EntityID, std::unordered_map<std::string, JointTF>, EntityID::Hash> m_tfCache;
    
    // TF缓存是否有效
    mutable std::unordered_map<EntityID, bool, EntityID::Hash> m_tfCacheValid;
};

} // namespace Robot
} // namespace ECS
} // namespace Render
