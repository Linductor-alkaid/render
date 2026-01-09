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
#include "render/mesh_loader.h"
#include "render/logger.h"
#include <string>
#include <memory>
#include <fstream>
#include <sstream>

namespace Render {
namespace Robot {

/**
 * @brief URDF加载器
 * 
 * 从URDF文件加载机器人模型
 */
class URDFLoader {
public:
    /**
     * @brief 从文件加载URDF
     * @param urdfPath URDF文件路径
     * @param meshBasePath mesh文件的基准路径（相对于URDF文件）
     * @return 机器人模型指针，失败返回nullptr
     */
    static Ref<RobotModel> LoadFromFile(
        const std::string& urdfPath,
        const std::string& meshBasePath = ""
    );
    
    /**
     * @brief 从字符串加载URDF
     * @param urdfContent URDF XML内容
     * @param meshBasePath mesh文件的基准路径
     * @return 机器人模型指针，失败返回nullptr
     */
    static Ref<RobotModel> LoadFromString(
        const std::string& urdfContent,
        const std::string& meshBasePath = ""
    );

private:
    // 简单的XML解析辅助函数
    static std::string ReadFile(const std::string& path);
    static std::string GetAttribute(const std::string& tag, const std::string& attrName);
    static std::string GetTagContent(const std::string& xml, const std::string& tagName);
    static std::vector<std::string> GetAllTags(const std::string& xml, const std::string& tagName);
    
    // URDF解析函数
    static void ParseRobot(const std::string& xml, RobotModel& model, const std::string& meshBasePath);
    static void ParseLink(const std::string& linkXml, URDFLink& link, const std::string& meshBasePath);
    static void ParseJoint(const std::string& jointXml, URDFJoint& joint);
    static void ParseInertial(const std::string& inertialXml, URDFLink& link);
    static void ParseVisual(const std::string& visualXml, URDFVisual& visual, const std::string& meshBasePath);
    static void ParseCollision(const std::string& collisionXml, URDFCollision& collision, const std::string& meshBasePath);
    static void ParseOrigin(const std::string& originXml, Vector3& position, Quaternion& rotation);
    static void ParseGeometry(const std::string& geometryXml, std::string& geometryType, 
                             std::string& meshFilename, Vector3& size, float& radius, float& length);
    static void ParseLimits(const std::string& limitsXml, JointLimits& limits);
    
    // 辅助函数
    static JointType ParseJointType(const std::string& typeStr);
    static Vector3 ParseXYZ(const std::string& xyzStr);
    static Vector3 ParseRPY(const std::string& rpyStr);
    static Quaternion RPYToQuaternion(const Vector3& rpy);
    static std::string ResolveMeshPath(const std::string& filename, const std::string& urdfPath, const std::string& meshBasePath);
};

} // namespace Robot
} // namespace Render
