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

#include "render/robot/joint_tf_system.h"
#include "render/robot/robot_components.h"
#include "render/robot/robot_model.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/math_utils.h"
#include "render/logger.h"

namespace Render {
namespace ECS {
namespace Robot {

void JointTFSystem::Update(float deltaTime) {
    (void)deltaTime;
    
    if (!m_world) return;
    
    // 更新所有机器人的TF
    auto robots = m_world->Query<RobotComponent>();
    for (const auto& robotEntity : robots) {
        UpdateAllTFs(robotEntity);
    }
}

void JointTFSystem::SetJointPosition(EntityID robotEntity, const std::string& jointName, float position) {
    m_jointPositions[robotEntity][jointName] = position;
    m_tfCacheValid[robotEntity] = false;  // 标记缓存失效
}

void JointTFSystem::SetJointPositions(EntityID robotEntity, const std::unordered_map<std::string, float>& positions) {
    for (const auto& [jointName, position] : positions) {
        m_jointPositions[robotEntity][jointName] = position;
    }
    m_tfCacheValid[robotEntity] = false;  // 标记缓存失效
}

const std::unordered_map<std::string, JointTF>& JointTFSystem::GetAllTFs(EntityID robotEntity) const {
    // 如果缓存无效，需要更新
    if (!m_tfCacheValid[robotEntity]) {
        const_cast<JointTFSystem*>(this)->UpdateAllTFs(robotEntity);
    }
    
    auto it = m_tfCache.find(robotEntity);
    if (it != m_tfCache.end()) {
        return it->second;
    }
    
    // 返回静态的空映射（避免返回临时对象）
    static const std::unordered_map<std::string, JointTF> emptyMap;
    return emptyMap;
}

bool JointTFSystem::GetJointTF(EntityID robotEntity, const std::string& jointName, JointTF& outTF) const {
    const auto& tfs = GetAllTFs(robotEntity);
    auto it = tfs.find(jointName);
    if (it != tfs.end()) {
        // 手动复制（因为Transform不可拷贝）
        outTF.jointName = it->second.jointName;
        outTF.linkName = it->second.linkName;
        outTF.transform.SetPosition(it->second.transform.GetWorldPosition());
        outTF.transform.SetRotation(it->second.transform.GetWorldRotation());
        outTF.transform.SetScale(it->second.transform.GetWorldScale());
        outTF.transformMatrix = it->second.transformMatrix;
        return true;
    }
    
    return false;
}

float JointTFSystem::GetJointPosition(EntityID robotEntity, const std::string& jointName) const {
    auto it = m_jointPositions.find(robotEntity);
    if (it != m_jointPositions.end()) {
        auto posIt = it->second.find(jointName);
        if (posIt != it->second.end()) {
            return posIt->second;
        }
    }
    return 0.0f;
}

void JointTFSystem::UpdateAllTFs(EntityID robotEntity) {
    if (!m_world) return;
    
    if (!m_world->HasComponent<RobotComponent>(robotEntity)) {
        return;
    }
    
    const auto& robotComp = m_world->GetComponent<RobotComponent>(robotEntity);
    if (!robotComp.model) {
        return;
    }
    
    const auto& model = *robotComp.model;
    
    // 获取基座link的Transform
    Transform baseTF;  // 默认构造函数创建单位变换
    if (robotComp.baseLinkEntity.IsValid() && 
        m_world->HasComponent<TransformComponent>(robotComp.baseLinkEntity)) {
        const auto& baseTransformComp = m_world->GetComponent<TransformComponent>(robotComp.baseLinkEntity);
        if (baseTransformComp.transform) {
            // Transform不可赋值，需要手动设置
            baseTF.SetPosition(baseTransformComp.transform->GetWorldPosition());
            baseTF.SetRotation(baseTransformComp.transform->GetWorldRotation());
            baseTF.SetScale(baseTransformComp.transform->GetWorldScale());
        }
    }
    
    // 获取关节位置
    auto& jointPositions = m_jointPositions[robotEntity];
    
    // 清空TF缓存
    m_tfCache[robotEntity].clear();
    
    // 从基座link开始递归计算
    ComputeLinkAndJointTFs(model, model.baseLink, jointPositions, baseTF, m_tfCache[robotEntity]);
    
    // 标记缓存有效
    m_tfCacheValid[robotEntity] = true;
}

void JointTFSystem::ComputeLinkAndJointTFs(
    const Render::Robot::RobotModel& model,
    const std::string& linkName,
    const std::unordered_map<std::string, float>& jointPositions,
    const Transform& parentTF,
    std::unordered_map<std::string, JointTF>& tfs) const
{
    // 获取该link的所有子关节
    const auto& childJoints = model.GetChildJoints(linkName);
    
    for (const auto& jointName : childJoints) {
        auto jointIt = model.joints.find(jointName);
        if (jointIt == model.joints.end()) {
            continue;
        }
        
        const auto& joint = jointIt->second;
        
        // 获取关节位置（默认为0）
        float jointPosition = 0.0f;
        auto posIt = jointPositions.find(jointName);
        if (posIt != jointPositions.end()) {
            jointPosition = posIt->second;
        }
        
        // 计算关节的局部变换
        Transform jointLocalTF = ComputeJointTransform(joint, jointPosition);
        
        // 关节的origin变换
        Transform originTF(joint.origin, joint.originRotation, Vector3::Ones());
        
        // 关节在基座坐标系下的TF = 父TF * origin * jointTransform
        // 计算组合变换：先计算origin变换后的位置
        Vector3 originWorldPos = parentTF.TransformPoint(originTF.GetPosition());
        Quaternion originWorldRot = parentTF.GetWorldRotation() * originTF.GetRotation();
        
        // 再应用joint的局部变换
        Vector3 jointLocalPos = jointLocalTF.GetPosition();
        Quaternion jointLocalRot = jointLocalTF.GetRotation();
        
        // 组合：joint的世界位置 = origin世界位置 + joint局部位置（在origin旋转后的坐标系中）
        Vector3 jointPos = originWorldPos + originWorldRot * jointLocalPos;
        Quaternion jointRot = originWorldRot * jointLocalRot;
        Vector3 jointScale = parentTF.GetWorldScale();
        
        // 创建joint的Transform（用于递归调用）
        Transform jointTF(jointPos, jointRot, jointScale);
        
        // 存储关节TF
        JointTF& jointTFData = tfs[jointName];
        jointTFData.jointName = jointName;
        jointTFData.linkName = joint.childLink;
        // Transform不可拷贝，需要手动设置
        jointTFData.transform.SetPosition(jointTF.GetWorldPosition());
        jointTFData.transform.SetRotation(jointTF.GetWorldRotation());
        jointTFData.transform.SetScale(jointTF.GetWorldScale());
        jointTFData.transformMatrix = jointTF.GetWorldMatrix();
        
        // 递归计算子link的TF
        if (model.HasLink(joint.childLink)) {
            // 子link的TF = 关节TF（因为关节连接父link和子link）
            ComputeLinkAndJointTFs(model, joint.childLink, jointPositions, jointTF, tfs);
        }
    }
}

Transform JointTFSystem::ComputeJointTransform(const Render::Robot::URDFJoint& joint, float jointPosition) const {
    switch (joint.type) {
        case Render::Robot::JointType::Revolute:
        case Render::Robot::JointType::Continuous: {
            // 旋转关节：绕轴旋转
            Quaternion rotation = Quaternion(Eigen::AngleAxisf(jointPosition, joint.axis));
            return Transform(Vector3::Zero(), rotation, Vector3::Ones());
        }
        
        case Render::Robot::JointType::Prismatic: {
            // 平移关节：沿轴平移
            Vector3 translation = joint.axis * jointPosition;
            return Transform(translation, Quaternion::Identity(), Vector3::Ones());
        }
        
        case Render::Robot::JointType::Fixed: {
            // 固定关节：无变换
            return Transform();  // 默认构造函数创建单位变换
        }
        
        default: {
            // 未知类型：无变换
            return Transform();  // 默认构造函数创建单位变换
        }
    }
}

} // namespace Robot
} // namespace ECS
} // namespace Render
