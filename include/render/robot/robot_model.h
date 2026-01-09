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

#include "render/types.h"
#include "render/transform.h"
#include "render/mesh.h"
#include "render/ecs/entity.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace Render {

// 前向声明
class Mesh;

namespace Robot {

/**
 * @brief 关节类型
 */
enum class JointType {
    Revolute,    // 旋转关节
    Prismatic,   // 平移关节
    Fixed,       // 固定关节
    Continuous,  // 连续旋转关节
    Planar,      // 平面关节
    Floating,    // 浮动关节
    Unknown      // 未知类型
};

/**
 * @brief 关节限制
 */
struct JointLimits {
    float lower = 0.0f;      // 下限
    float upper = 0.0f;      // 上限
    float effort = 0.0f;     // 最大力矩/力
    float velocity = 0.0f;   // 最大速度
    
    JointLimits() = default;
    JointLimits(float l, float u, float e, float v)
        : lower(l), upper(u), effort(e), velocity(v) {}
};

/**
 * @brief URDF视觉几何
 */
struct URDFVisual {
    std::string name;
    Vector3 origin;          // 相对于link的变换位置
    Quaternion originRotation; // 相对于link的变换旋转
    std::string geometryType; // "mesh", "box", "cylinder", "sphere"
    std::string meshFilename; // mesh文件路径（如果geometryType为"mesh"）
    Vector3 size;            // 尺寸（用于box/cylinder/sphere）
    float radius = 0.0f;     // 半径（用于cylinder/sphere）
    float length = 0.0f;     // 长度（用于cylinder）
    Vector3 scale;           // mesh的缩放（用于镜像等，默认1,1,1）
    
    URDFVisual() : origin(Vector3::Zero()), originRotation(Quaternion::Identity()), scale(Vector3::Ones()) {}
};

/**
 * @brief URDF碰撞几何
 */
struct URDFCollision {
    std::string name;
    Vector3 origin;
    Quaternion originRotation;
    std::string geometryType;
    std::string meshFilename;
    Vector3 size;
    float radius = 0.0f;
    float length = 0.0f;
    
    URDFCollision() : origin(Vector3::Zero()), originRotation(Quaternion::Identity()) {}
};

/**
 * @brief URDF Link（连杆）
 */
struct URDFLink {
    std::string name;
    
    // 惯性参数
    Vector3 inertialOrigin;
    float mass = 0.0f;
    Matrix3 inertia;  // 惯性张量
    
    // 视觉几何
    std::vector<URDFVisual> visuals;
    
    // 碰撞几何
    std::vector<URDFCollision> collisions;
    
    URDFLink() : inertia(Matrix3::Identity()) {}
};

/**
 * @brief URDF Joint（关节）
 */
struct URDFJoint {
    std::string name;
    std::string parentLink;
    std::string childLink;
    JointType type = JointType::Unknown;
    
    // 关节原点（相对于父link的变换）
    Vector3 origin;
    Quaternion originRotation;
    
    // 关节轴（旋转轴或平移轴）
    Vector3 axis;
    
    // 关节限制
    JointLimits limits;
    
    URDFJoint() 
        : origin(Vector3::Zero())
        , originRotation(Quaternion::Identity())
        , axis(Vector3::UnitZ()) {}
};

/**
 * @brief 机器人模型
 * 
 * 包含URDF解析后的所有信息
 */
struct RobotModel {
    std::string name;
    std::string baseLink;  // 基座link名称
    
    // Link和Joint映射
    std::unordered_map<std::string, URDFLink> links;
    std::unordered_map<std::string, URDFJoint> joints;
    
    // Link到ECS实体的映射（加载后填充）
    std::unordered_map<std::string, ECS::EntityID> linkEntities;
    
    // 关节树结构（用于快速遍历）
    std::unordered_map<std::string, std::vector<std::string>> linkChildJoints;  // link名称 -> 子关节列表
    
    RobotModel() = default;
    
    /**
     * @brief 获取基座link
     */
    const URDFLink* GetBaseLink() const {
        auto it = links.find(baseLink);
        return it != links.end() ? &it->second : nullptr;
    }
    
    /**
     * @brief 获取link的子关节列表
     */
    const std::vector<std::string>& GetChildJoints(const std::string& linkName) const {
        static const std::vector<std::string> empty;
        auto it = linkChildJoints.find(linkName);
        return it != linkChildJoints.end() ? it->second : empty;
    }
    
    /**
     * @brief 检查link是否存在
     */
    bool HasLink(const std::string& linkName) const {
        return links.find(linkName) != links.end();
    }
    
    /**
     * @brief 检查joint是否存在
     */
    bool HasJoint(const std::string& jointName) const {
        return joints.find(jointName) != joints.end();
    }
};

} // namespace Robot
} // namespace Render
