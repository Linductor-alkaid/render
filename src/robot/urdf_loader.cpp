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

#include "render/robot/urdf_loader.h"
#include "render/file_utils.h"
#include "render/math_utils.h"
#include <algorithm>
#include <regex>
#include <cmath>
#include <set>

namespace Render {
namespace Robot {

Ref<RobotModel> URDFLoader::LoadFromFile(
    const std::string& urdfPath,
    const std::string& meshBasePath)
{
    std::string content = ReadFile(urdfPath);
    if (content.empty()) {
        Logger::GetInstance().ErrorFormat("[URDFLoader] Failed to read URDF file: %s", urdfPath.c_str());
        return nullptr;
    }
    
    // 如果meshBasePath为空，使用URDF文件所在目录
    std::string actualMeshBasePath = meshBasePath;
    if (actualMeshBasePath.empty()) {
        size_t lastSlash = urdfPath.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            actualMeshBasePath = urdfPath.substr(0, lastSlash + 1);
        }
    }
    
    return LoadFromString(content, actualMeshBasePath);
}

Ref<RobotModel> URDFLoader::LoadFromString(
    const std::string& urdfContent,
    const std::string& meshBasePath)
{
    auto model = std::make_shared<RobotModel>();
    
    try {
        ParseRobot(urdfContent, *model, meshBasePath);
        
        // 构建关节树结构
        for (const auto& [jointName, joint] : model->joints) {
            model->linkChildJoints[joint.parentLink].push_back(jointName);
        }
        
        Logger::GetInstance().InfoFormat(
            "[URDFLoader] Loaded robot '%s' with %zu links and %zu joints",
            model->name.c_str(), model->links.size(), model->joints.size()
        );
        
        return model;
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("[URDFLoader] Error parsing URDF: %s", e.what());
        return nullptr;
    }
}

std::string URDFLoader::ReadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string URDFLoader::GetAttribute(const std::string& tag, const std::string& attrName) {
    std::string patternStr = attrName + "=\"([^\"]*)\"";
    std::regex pattern(patternStr);
    std::smatch match;
    if (std::regex_search(tag, match, pattern)) {
        return match[1].str();
    }
    return "";
}

std::string URDFLoader::GetTagContent(const std::string& xml, const std::string& tagName) {
    std::string openTag = "<" + tagName;
    std::string closeTag = "</" + tagName + ">";
    
    size_t start = xml.find(openTag);
    if (start == std::string::npos) return "";
    
    // 找到标签结束位置
    size_t tagEnd = xml.find(">", start);
    if (tagEnd == std::string::npos) return "";
    
    // 检查是否是自闭合标签
    if (xml[tagEnd - 1] == '/') return "";
    
    // 找到内容开始位置
    size_t contentStart = tagEnd + 1;
    
    // 找到闭合标签
    size_t end = xml.find(closeTag, contentStart);
    if (end == std::string::npos) return "";
    
    return xml.substr(contentStart, end - contentStart);
}

std::vector<std::string> URDFLoader::GetAllTags(const std::string& xml, const std::string& tagName) {
    std::vector<std::string> result;
    std::string openTag = "<" + tagName;
    std::string closeTag = "</" + tagName + ">";
    
    size_t pos = 0;
    while ((pos = xml.find(openTag, pos)) != std::string::npos) {
        size_t tagEnd = xml.find(">", pos);
        if (tagEnd == std::string::npos) break;
        
        // 检查是否是自闭合标签
        if (xml[tagEnd - 1] == '/') {
            result.push_back(xml.substr(pos, tagEnd - pos + 1));
            pos = tagEnd + 1;
            continue;
        }
        
        // 找到闭合标签
        size_t end = xml.find(closeTag, tagEnd);
        if (end == std::string::npos) break;
        
        result.push_back(xml.substr(pos, end - pos + closeTag.length()));
        pos = end + closeTag.length();
    }
    
    return result;
}

void URDFLoader::ParseRobot(const std::string& xml, RobotModel& model, const std::string& meshBasePath) {
    // 获取robot名称
    size_t robotStart = xml.find("<robot");
    if (robotStart == std::string::npos) {
        throw std::runtime_error("No <robot> tag found");
    }
    
    size_t nameStart = xml.find("name=\"", robotStart);
    if (nameStart != std::string::npos) {
        nameStart += 6;
        size_t nameEnd = xml.find("\"", nameStart);
        if (nameEnd != std::string::npos) {
            model.name = xml.substr(nameStart, nameEnd - nameStart);
        }
    }
    
    // 解析所有links
    auto linkTags = GetAllTags(xml, "link");
    for (const auto& linkTag : linkTags) {
        URDFLink link;
        ParseLink(linkTag, link, meshBasePath);
        if (!link.name.empty()) {
            model.links[link.name] = link;
        }
    }
    
    // 解析所有joints
    std::set<std::string> childLinks;  // 所有作为child的link
    auto jointTags = GetAllTags(xml, "joint");
    for (const auto& jointTag : jointTags) {
        URDFJoint joint;
        ParseJoint(jointTag, joint);
        if (!joint.name.empty()) {
            model.joints[joint.name] = joint;
            // 保存关节定义顺序（按照URDF文件中的定义顺序）
            model.jointOrder.push_back(joint.name);
            
            // 记录所有child links
            if (!joint.childLink.empty()) {
                childLinks.insert(joint.childLink);
            }
            
            // 如果parent link是world或没有parent，则child link是base link
            if (joint.parentLink == "world" || joint.parentLink.empty()) {
                model.baseLink = joint.childLink;
            }
        }
    }
    
    // 如果没有明确指定base link，找到所有没有作为child的link（即base link）
    if (model.baseLink.empty()) {
        for (const auto& [linkName, link] : model.links) {
            if (childLinks.find(linkName) == childLinks.end()) {
                model.baseLink = linkName;
                break;
            }
        }
    }
    
    // 如果仍然没有找到base link，使用第一个link
    if (model.baseLink.empty() && !model.links.empty()) {
        model.baseLink = model.links.begin()->first;
        Logger::GetInstance().WarningFormat("[URDFLoader] No base link found, using first link: '%s'", 
            model.baseLink.c_str());
    }
    
}

void URDFLoader::ParseLink(const std::string& linkXml, URDFLink& link, const std::string& meshBasePath) {
    // 获取link名称
    link.name = GetAttribute(linkXml, "name");
    if (link.name.empty()) {
        return;
    }
    
    // 解析inertial
    std::string inertialXml = GetTagContent(linkXml, "inertial");
    if (!inertialXml.empty()) {
        ParseInertial(inertialXml, link);
    }
    
    // 解析visuals
    auto visualTags = GetAllTags(linkXml, "visual");
    for (const auto& visualTag : visualTags) {
        URDFVisual visual;
        ParseVisual(visualTag, visual, meshBasePath);
        link.visuals.push_back(visual);
    }
    
    // 解析collisions
    auto collisionTags = GetAllTags(linkXml, "collision");
    for (const auto& collisionTag : collisionTags) {
        URDFCollision collision;
        ParseCollision(collisionTag, collision, meshBasePath);
        link.collisions.push_back(collision);
    }
}

void URDFLoader::ParseJoint(const std::string& jointXml, URDFJoint& joint) {
    joint.name = GetAttribute(jointXml, "name");
    if (joint.name.empty()) {
        return;
    }
    
    // 解析joint类型
    std::string typeStr = GetAttribute(jointXml, "type");
    joint.type = ParseJointType(typeStr);
    
    // 解析parent和child link
    // 注意：<parent> 和 <child> 可能是自闭合标签，需要特殊处理
    std::string parentOpenTag = "<parent";
    size_t parentStart = jointXml.find(parentOpenTag);
    if (parentStart != std::string::npos) {
        size_t parentEnd = jointXml.find(">", parentStart);
        if (parentEnd != std::string::npos) {
            std::string parentTag = jointXml.substr(parentStart, parentEnd - parentStart + 1);
            joint.parentLink = GetAttribute(parentTag, "link");
        }
    }
    // 如果上面没找到（可能是非自闭合标签），尝试GetTagContent
    if (joint.parentLink.empty()) {
        std::string parentXml = GetTagContent(jointXml, "parent");
        if (!parentXml.empty()) {
            joint.parentLink = GetAttribute(parentXml, "link");
        }
    }
    
    std::string childOpenTag = "<child";
    size_t childStart = jointXml.find(childOpenTag);
    if (childStart != std::string::npos) {
        size_t childEnd = jointXml.find(">", childStart);
        if (childEnd != std::string::npos) {
            std::string childTag = jointXml.substr(childStart, childEnd - childStart + 1);
            joint.childLink = GetAttribute(childTag, "link");
        }
    }
    // 如果上面没找到（可能是非自闭合标签），尝试GetTagContent
    if (joint.childLink.empty()) {
        std::string childXml = GetTagContent(jointXml, "child");
        if (!childXml.empty()) {
            joint.childLink = GetAttribute(childXml, "link");
        }
    }
    
    // 解析origin（可能是自闭合标签，如 <origin xyz="0.1745 0.062 0"/>）
    std::string originOpenTag = "<origin";
    size_t originStart = jointXml.find(originOpenTag);
    if (originStart != std::string::npos) {
        size_t originEnd = jointXml.find(">", originStart);
        if (originEnd != std::string::npos) {
            // 检查是否是自闭合标签（检查 originEnd 前一个字符是否是 /）
            if (originEnd > originStart && jointXml[originEnd - 1] == '/') {
                // 自闭合标签，提取整个标签（包括 />）
                std::string originTag = jointXml.substr(originStart, originEnd - originStart + 1);
                ParseOrigin(originTag, joint.origin, joint.originRotation);
            } else {
                // 非自闭合标签，使用GetTagContent
                std::string originXml = GetTagContent(jointXml, "origin");
                if (!originXml.empty()) {
                    ParseOrigin(originXml, joint.origin, joint.originRotation);
                }
            }
        }
    } else {
        // 如果没找到，尝试GetTagContent（非自闭合标签）
        std::string originXml = GetTagContent(jointXml, "origin");
        if (!originXml.empty()) {
            ParseOrigin(originXml, joint.origin, joint.originRotation);
        }
    }
    
    // 解析axis（可能是自闭合标签，如 <axis xyz="-1 0 0"/>）
    std::string axisOpenTag = "<axis";
    size_t axisStart = jointXml.find(axisOpenTag);
    if (axisStart != std::string::npos) {
        size_t axisEnd = jointXml.find(">", axisStart);
        if (axisEnd != std::string::npos) {
            // 检查是否是自闭合标签（检查 axisEnd 前一个字符是否是 /）
            if (axisEnd > axisStart && jointXml[axisEnd - 1] == '/') {
                // 自闭合标签，提取整个标签（包括 />）
                std::string axisTag = jointXml.substr(axisStart, axisEnd - axisStart + 1);
                joint.axis = ParseXYZ(GetAttribute(axisTag, "xyz"));
                if (joint.axis.norm() < 0.001f) {
                    joint.axis = Vector3::UnitZ();
                } else {
                    joint.axis.normalize();
                }
            } else {
                // 非自闭合标签，使用GetTagContent
                std::string axisXml = GetTagContent(jointXml, "axis");
                if (!axisXml.empty()) {
                    joint.axis = ParseXYZ(GetAttribute(axisXml, "xyz"));
                    if (joint.axis.norm() < 0.001f) {
                        joint.axis = Vector3::UnitZ();
                    } else {
                        joint.axis.normalize();
                    }
                }
            }
        }
    } else {
        // 如果没找到，尝试GetTagContent（非自闭合标签）
        std::string axisXml = GetTagContent(jointXml, "axis");
        if (!axisXml.empty()) {
            joint.axis = ParseXYZ(GetAttribute(axisXml, "xyz"));
            if (joint.axis.norm() < 0.001f) {
                joint.axis = Vector3::UnitZ();
            } else {
                joint.axis.normalize();
            }
        }
    }
    
    // 解析limits（可能是自闭合标签，如 <limit lower="-0.611" upper="0.436" effort="76.4" velocity="22.4"/>）
    std::string limitOpenTag = "<limit";
    size_t limitStart = jointXml.find(limitOpenTag);
    if (limitStart != std::string::npos) {
        size_t limitEnd = jointXml.find(">", limitStart);
        if (limitEnd != std::string::npos) {
            // 检查是否是自闭合标签（检查 limitEnd 前一个字符是否是 /）
            if (limitEnd > limitStart && jointXml[limitEnd - 1] == '/') {
                // 自闭合标签，提取整个标签（包括 />）
                std::string limitTag = jointXml.substr(limitStart, limitEnd - limitStart + 1);
                ParseLimits(limitTag, joint.limits);
            } else {
                // 非自闭合标签，使用GetTagContent
                std::string limitsXml = GetTagContent(jointXml, "limit");
                if (!limitsXml.empty()) {
                    ParseLimits(limitsXml, joint.limits);
                }
            }
        }
    } else {
        // 如果没找到，尝试GetTagContent（非自闭合标签）
        std::string limitsXml = GetTagContent(jointXml, "limit");
        if (!limitsXml.empty()) {
            ParseLimits(limitsXml, joint.limits);
        }
    }
}

void URDFLoader::ParseInertial(const std::string& inertialXml, URDFLink& link) {
    // 解析origin
    std::string originXml = GetTagContent(inertialXml, "origin");
    if (!originXml.empty()) {
        Quaternion defaultRot = Quaternion::Identity();
        ParseOrigin(originXml, link.inertialOrigin, defaultRot);
    }
    
    // 解析mass
    std::string massXml = GetTagContent(inertialXml, "mass");
    if (!massXml.empty()) {
        std::string massStr = GetAttribute(massXml, "value");
        if (!massStr.empty()) {
            link.mass = std::stof(massStr);
        }
    }
    
    // 解析inertia
    std::string inertiaXml = GetTagContent(inertialXml, "inertia");
    if (!inertiaXml.empty()) {
        link.inertia = Matrix3::Identity();
        
        auto getInertiaValue = [&](const std::string& attr) -> float {
            std::string val = GetAttribute(inertiaXml, attr);
            return val.empty() ? 0.0f : std::stof(val);
        };
        
        link.inertia(0, 0) = getInertiaValue("ixx");
        link.inertia(0, 1) = getInertiaValue("ixy");
        link.inertia(0, 2) = getInertiaValue("ixz");
        link.inertia(1, 0) = getInertiaValue("ixy");
        link.inertia(1, 1) = getInertiaValue("iyy");
        link.inertia(1, 2) = getInertiaValue("iyz");
        link.inertia(2, 0) = getInertiaValue("ixz");
        link.inertia(2, 1) = getInertiaValue("iyz");
        link.inertia(2, 2) = getInertiaValue("izz");
    }
}

void URDFLoader::ParseVisual(const std::string& visualXml, URDFVisual& visual, const std::string& meshBasePath) {
    visual.name = GetAttribute(visualXml, "name");
    
    // 解析origin
    std::string originXml = GetTagContent(visualXml, "origin");
    if (!originXml.empty()) {
        ParseOrigin(originXml, visual.origin, visual.originRotation);
    }
    
    // 解析geometry
    std::string geometryXml = GetTagContent(visualXml, "geometry");
    if (!geometryXml.empty()) {
        ParseGeometry(geometryXml, visual.geometryType, visual.meshFilename, 
                     visual.size, visual.radius, visual.length);
        
        // 解析mesh文件的scale（如果有，可能是自闭合标签）
        if (visual.geometryType == "mesh") {
            std::string meshOpenTag = "<mesh";
            size_t meshStart = geometryXml.find(meshOpenTag);
            if (meshStart != std::string::npos) {
                size_t meshEnd = geometryXml.find(">", meshStart);
                if (meshEnd != std::string::npos) {
                    std::string meshTag = geometryXml.substr(meshStart, meshEnd - meshStart + 1);
                    std::string scaleStr = GetAttribute(meshTag, "scale");
                    if (!scaleStr.empty()) {
                        visual.scale = ParseXYZ(scaleStr);
                    }
                }
            }
            // 如果不是自闭合标签，尝试GetTagContent
            if (visual.scale == Vector3::Ones()) {
                std::string meshTag = GetTagContent(geometryXml, "mesh");
                if (!meshTag.empty()) {
                    std::string scaleStr = GetAttribute(meshTag, "scale");
                    if (!scaleStr.empty()) {
                        visual.scale = ParseXYZ(scaleStr);
                    }
                }
            }
        }
        
        // 解析material（可选）
        std::string materialXml = GetTagContent(visualXml, "material");
        // 可以在这里解析材质信息
    }
}

void URDFLoader::ParseCollision(const std::string& collisionXml, URDFCollision& collision, const std::string& meshBasePath) {
    collision.name = GetAttribute(collisionXml, "name");
    
    // 解析origin
    std::string originXml = GetTagContent(collisionXml, "origin");
    if (!originXml.empty()) {
        ParseOrigin(originXml, collision.origin, collision.originRotation);
    }
    
    // 解析geometry
    std::string geometryXml = GetTagContent(collisionXml, "geometry");
    if (!geometryXml.empty()) {
        ParseGeometry(geometryXml, collision.geometryType, collision.meshFilename,
                     collision.size, collision.radius, collision.length);
    }
}

void URDFLoader::ParseOrigin(const std::string& originXml, Vector3& position, Quaternion& rotation) {
    std::string xyzStr = GetAttribute(originXml, "xyz");
    if (!xyzStr.empty()) {
        position = ParseXYZ(xyzStr);
    }
    
    std::string rpyStr = GetAttribute(originXml, "rpy");
    if (!rpyStr.empty()) {
        Vector3 rpy = ParseRPY(rpyStr);
        rotation = RPYToQuaternion(rpy);
    } else {
        rotation = Quaternion::Identity();
    }
}

void URDFLoader::ParseGeometry(const std::string& geometryXml, std::string& geometryType,
                               std::string& meshFilename, Vector3& size, float& radius, float& length) {
    // 检查mesh（可能是自闭合标签）
    std::string meshOpenTag = "<mesh";
    size_t meshStart = geometryXml.find(meshOpenTag);
    if (meshStart != std::string::npos) {
        size_t meshEnd = geometryXml.find(">", meshStart);
        if (meshEnd != std::string::npos) {
            std::string meshTag = geometryXml.substr(meshStart, meshEnd - meshStart + 1);
            geometryType = "mesh";
            meshFilename = GetAttribute(meshTag, "filename");
            if (!meshFilename.empty()) {
                return;
            }
        }
    }
    
    // 如果上面的方法失败，尝试GetTagContent（非自闭合标签）
    std::string meshXml = GetTagContent(geometryXml, "mesh");
    if (!meshXml.empty()) {
        geometryType = "mesh";
        meshFilename = GetAttribute(meshXml, "filename");
        if (!meshFilename.empty()) {
            return;
        }
    }
    
    // 检查box（可能是自闭合标签）
    std::string boxOpenTag = "<box";
    size_t boxStart = geometryXml.find(boxOpenTag);
    if (boxStart != std::string::npos) {
        size_t boxEnd = geometryXml.find(">", boxStart);
        if (boxEnd != std::string::npos) {
            std::string boxTag = geometryXml.substr(boxStart, boxEnd - boxStart + 1);
            geometryType = "box";
            std::string sizeStr = GetAttribute(boxTag, "size");
            if (!sizeStr.empty()) {
                size = ParseXYZ(sizeStr);
                return;
            }
        }
    }
    std::string boxXml = GetTagContent(geometryXml, "box");
    if (!boxXml.empty()) {
        geometryType = "box";
        std::string sizeStr = GetAttribute(boxXml, "size");
        if (!sizeStr.empty()) {
            size = ParseXYZ(sizeStr);
            return;
        }
    }
    
    // 检查cylinder（可能是自闭合标签）
    std::string cylinderOpenTag = "<cylinder";
    size_t cylinderStart = geometryXml.find(cylinderOpenTag);
    if (cylinderStart != std::string::npos) {
        size_t cylinderEnd = geometryXml.find(">", cylinderStart);
        if (cylinderEnd != std::string::npos) {
            std::string cylinderTag = geometryXml.substr(cylinderStart, cylinderEnd - cylinderStart + 1);
            geometryType = "cylinder";
            std::string radiusStr = GetAttribute(cylinderTag, "radius");
            std::string lengthStr = GetAttribute(cylinderTag, "length");
            if (!radiusStr.empty()) {
                radius = std::stof(radiusStr);
            }
            if (!lengthStr.empty()) {
                length = std::stof(lengthStr);
            }
            if (!radiusStr.empty() || !lengthStr.empty()) {
                return;
            }
        }
    }
    std::string cylinderXml = GetTagContent(geometryXml, "cylinder");
    if (!cylinderXml.empty()) {
        geometryType = "cylinder";
        std::string radiusStr = GetAttribute(cylinderXml, "radius");
        std::string lengthStr = GetAttribute(cylinderXml, "length");
        if (!radiusStr.empty()) {
            radius = std::stof(radiusStr);
        }
        if (!lengthStr.empty()) {
            length = std::stof(lengthStr);
        }
        if (!radiusStr.empty() || !lengthStr.empty()) {
            return;
        }
    }
    
    // 检查sphere（可能是自闭合标签）
    std::string sphereOpenTag = "<sphere";
    size_t sphereStart = geometryXml.find(sphereOpenTag);
    if (sphereStart != std::string::npos) {
        size_t sphereEnd = geometryXml.find(">", sphereStart);
        if (sphereEnd != std::string::npos) {
            std::string sphereTag = geometryXml.substr(sphereStart, sphereEnd - sphereStart + 1);
            geometryType = "sphere";
            std::string radiusStr = GetAttribute(sphereTag, "radius");
            if (!radiusStr.empty()) {
                radius = std::stof(radiusStr);
                return;
            }
        }
    }
    std::string sphereXml = GetTagContent(geometryXml, "sphere");
    if (!sphereXml.empty()) {
        geometryType = "sphere";
        std::string radiusStr = GetAttribute(sphereXml, "radius");
        if (!radiusStr.empty()) {
            radius = std::stof(radiusStr);
            return;
        }
    }
}

void URDFLoader::ParseLimits(const std::string& limitsXml, JointLimits& limits) {
    std::string lowerStr = GetAttribute(limitsXml, "lower");
    std::string upperStr = GetAttribute(limitsXml, "upper");
    std::string effortStr = GetAttribute(limitsXml, "effort");
    std::string velocityStr = GetAttribute(limitsXml, "velocity");
    
    if (!lowerStr.empty()) {
        limits.lower = std::stof(lowerStr);
    }
    if (!upperStr.empty()) {
        limits.upper = std::stof(upperStr);
    }
    if (!effortStr.empty()) {
        limits.effort = std::stof(effortStr);
    }
    if (!velocityStr.empty()) {
        limits.velocity = std::stof(velocityStr);
    }
}

JointType URDFLoader::ParseJointType(const std::string& typeStr) {
    if (typeStr == "revolute") return JointType::Revolute;
    if (typeStr == "prismatic") return JointType::Prismatic;
    if (typeStr == "fixed") return JointType::Fixed;
    if (typeStr == "continuous") return JointType::Continuous;
    if (typeStr == "planar") return JointType::Planar;
    if (typeStr == "floating") return JointType::Floating;
    return JointType::Unknown;
}

Vector3 URDFLoader::ParseXYZ(const std::string& xyzStr) {
    Vector3 result = Vector3::Zero();
    std::istringstream iss(xyzStr);
    std::string token;
    int index = 0;
    
    while (std::getline(iss, token, ' ') && index < 3) {
        if (!token.empty()) {
            result[index++] = std::stof(token);
        }
    }
    
    return result;
}

Vector3 URDFLoader::ParseRPY(const std::string& rpyStr) {
    return ParseXYZ(rpyStr);  // RPY格式与XYZ相同
}

Quaternion URDFLoader::RPYToQuaternion(const Vector3& rpy) {
    // RPY: Roll (绕X), Pitch (绕Y), Yaw (绕Z)
    // 注意：URDF使用Z-up，需要转换到Y-up（OpenGL）
    // 这里先简单实现，后续可能需要坐标系转换
    
    Quaternion qx = Quaternion(Eigen::AngleAxisf(rpy.x(), Vector3::UnitX()));
    Quaternion qy = Quaternion(Eigen::AngleAxisf(rpy.y(), Vector3::UnitY()));
    Quaternion qz = Quaternion(Eigen::AngleAxisf(rpy.z(), Vector3::UnitZ()));
    
    // ZYX顺序（Yaw-Pitch-Roll）
    return qz * qy * qx;
}

std::string URDFLoader::ResolveMeshPath(const std::string& filename, const std::string& urdfPath, const std::string& meshBasePath) {
    // 如果filename是绝对路径，直接返回
    if (filename.length() > 0 && (filename[0] == '/' || filename[1] == ':')) {
        return filename;
    }
    
    // 如果filename以../开头，相对于URDF文件解析
    if (filename.find("../") == 0) {
        size_t lastSlash = urdfPath.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            std::string urdfDir = urdfPath.substr(0, lastSlash + 1);
            return urdfDir + filename;
        }
    }
    
    // 否则使用meshBasePath
    return meshBasePath + filename;
}

} // namespace Robot
} // namespace Render
