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

#include "render/robot/robot_systems.h"
#include "render/robot/robot_components.h"
#include "render/robot/robot_model.h"
#include "render/robot/joint_tf_system.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/ecs/physics/physics_components.h"
#include "render/mesh_loader.h"
#include "render/material.h"
#include "render/shader_cache.h"
#include "render/logger.h"
#include "render/file_utils.h"
#include "render/math_utils.h"
#include <Eigen/Eigenvalues>
#include <algorithm>
#include <cmath>

namespace Render {
namespace ECS {
namespace Robot {

EntityID URDFLoadSystem::LoadRobot(const std::string& urdfPath, const std::string& meshBasePath, 
                                   RigidBodyType baseLinkType) {
    if (!m_world) {
        Logger::GetInstance().Error("[URDFLoadSystem] World is null");
        return EntityID::Invalid();
    }
    
    // 从URDF文件路径中提取目录（用于解析相对路径）
    std::string urdfDir = FileUtils::GetDirectory(urdfPath);
    if (urdfDir.empty()) {
        urdfDir = ".";  // 如果无法获取目录，使用当前目录
    }
    // 确保目录路径以/或\结尾
    if (!urdfDir.empty() && urdfDir.back() != '/' && urdfDir.back() != '\\') {
        urdfDir += "/";
    }
    
    // 加载URDF文件（使用URDF文件所在目录作为meshBasePath）
    auto model = Render::Robot::URDFLoader::LoadFromFile(urdfPath, urdfDir);
    if (!model) {
        Logger::GetInstance().ErrorFormat("[URDFLoadSystem] Failed to load URDF: %s", urdfPath.c_str());
        return EntityID::Invalid();
    }
    
    // 创建机器人根实体
    EntityID robotEntity = m_world->CreateEntity();
    
    // 添加RobotComponent
    RobotComponent robotComp;
    robotComp.model = model;
    m_world->AddComponent(robotEntity, robotComp);
    
    // 添加TransformComponent
    TransformComponent transformComp;
    m_world->AddComponent(robotEntity, transformComp);
    
    // 创建所有link实体（使用URDF文件所在目录）
    // 注意：直接获取组件的引用，而不是使用局部变量
    auto& robotCompRef = m_world->GetComponent<RobotComponent>(robotEntity);
    for (const auto& pair : model->links) {
        const std::string& linkName = pair.first;
        const Render::Robot::URDFLink& link = pair.second;
        EntityID linkEntity = CreateLinkEntity(link, urdfDir);
        if (linkEntity.IsValid()) {
            robotCompRef.linkEntityMap[linkName] = linkEntity;
            model->linkEntities[linkName] = linkEntity;
            
            // 如果是基座link，设置父子关系
            bool isBaseLink = (linkName == model->baseLink);
            if (isBaseLink) {
                robotCompRef.baseLinkEntity = linkEntity;
                
                // 设置link为机器人的子实体
                // 基座link的位置就是机器人的位置（相对于机器人实体为(0,0,0)）
                if (m_world->HasComponent<TransformComponent>(linkEntity)) {
                    auto& linkTransformComp = m_world->GetComponent<TransformComponent>(linkEntity);
                    linkTransformComp.SetPosition(Vector3::Zero());
                    linkTransformComp.SetRotation(Quaternion::Identity());
                    linkTransformComp.parentEntity = robotEntity;
                }
            }
            
            // 为link创建物理组件（碰撞体和刚体）
            CreateLinkPhysicsComponents(linkEntity, link, isBaseLink, baseLinkType);
        }
    }
    
    // 创建所有joint实体并设置父子关系
    for (const auto& pair : model->joints) {
        const Render::Robot::URDFJoint& joint = pair.second;
        
        auto parentIt = robotCompRef.linkEntityMap.find(joint.parentLink);
        auto childIt = robotCompRef.linkEntityMap.find(joint.childLink);
        
        if (parentIt == robotComp.linkEntityMap.end()) {
            Logger::GetInstance().WarningFormat("[URDFLoadSystem] Joint '%s': parent link '%s' not found in linkEntityMap", 
                joint.name.c_str(), joint.parentLink.c_str());
            continue;
        }
        if (childIt == robotComp.linkEntityMap.end()) {
            Logger::GetInstance().WarningFormat("[URDFLoadSystem] Joint '%s': child link '%s' not found in linkEntityMap", 
                joint.name.c_str(), joint.childLink.c_str());
            continue;
        }
        
        EntityID jointEntity = CreateJointEntity(joint, parentIt->second, childIt->second);
        
        if (jointEntity.IsValid()) {
                // 设置joint的Transform（相对于父link）
                // joint的origin定义了从父link到子link的变换
                if (m_world->HasComponent<TransformComponent>(jointEntity)) {
                    auto& jointTransformComp = m_world->GetComponent<TransformComponent>(jointEntity);
                    jointTransformComp.SetPosition(joint.origin);
                    jointTransformComp.SetRotation(joint.originRotation);
                    jointTransformComp.parentEntity = parentIt->second;
                }
                
                // 设置子link的Transform（相对于joint）
                // 注意：在URDF中，joint的origin定义了从父link坐标系到子link坐标系的变换
                // 这意味着子link的坐标系原点在父link坐标系中的位置是joint.origin
                // 如果我们使用joint作为中间节点，那么：
                // - joint在父link坐标系中：位置=joint.origin，旋转=joint.originRotation
                // - 子link在joint坐标系中：位置=(0,0,0)，旋转=Identity
                if (m_world->HasComponent<TransformComponent>(childIt->second)) {
                    auto& childTransformComp = m_world->GetComponent<TransformComponent>(childIt->second);
                    // 子link相对于joint的位置为(0,0,0)，旋转为Identity
                    childTransformComp.SetPosition(Vector3::Zero());
                    childTransformComp.SetRotation(Quaternion::Identity());
                    childTransformComp.parentEntity = jointEntity;
                }
                
                // 为joint创建物理约束（如果需要的話）
                CreateJointConstraint(jointEntity, joint, parentIt->second, childIt->second);
        }
    }
    
    m_robots[robotEntity] = model;
    
    Logger::GetInstance().InfoFormat(
        "[URDFLoadSystem] Loaded robot '%s' as entity %u",
        model->name.c_str(), robotEntity.index
    );
    
    return robotEntity;
}

void URDFLoadSystem::UnloadRobot(EntityID robotEntity) {
    if (!m_world) return;
    
    auto it = m_robots.find(robotEntity);
    if (it == m_robots.end()) {
        return;
    }
    
    // 删除机器人实体（会自动删除所有子实体）
    m_world->DestroyEntity(robotEntity);
    
    m_robots.erase(it);
}

bool URDFLoadSystem::IsRobotLoaded(EntityID robotEntity) const {
    return m_robots.find(robotEntity) != m_robots.end();
}

void URDFLoadSystem::CreateLinkPhysicsComponents(EntityID linkEntity, const Render::Robot::URDFLink& urdfLink, 
                                                  bool isBaseLink, RigidBodyType baseLinkType) {
    if (!m_world || !linkEntity.IsValid()) {
        return;
    }
    
    // 检查link是否有collision几何
    if (urdfLink.collisions.empty()) {
        Logger::GetInstance().InfoFormat("[URDFLoadSystem] Link '%s' has no collision geometry, skipping physics components", 
            urdfLink.name.c_str());
        return;
    }
    
    // 创建碰撞体组件
    // 如果有多个collision，使用第一个（后续可以扩展为复合形状）
    // 注意：PhysicsSystem当前支持单个collider，复合形状需要特殊处理
    const auto& collision = urdfLink.collisions[0];
    ColliderComponent collider;
    
    // 根据collision几何类型设置碰撞体
    if (collision.geometryType == "box") {
        collider.SetBox(collision.size);
    } else if (collision.geometryType == "sphere") {
        float radius = collision.radius > 0.0f ? collision.radius : 0.1f;
        collider.SetSphere(radius);
    } else if (collision.geometryType == "cylinder") {
        float radius = collision.radius > 0.0f ? collision.radius : 0.1f;
        float length = collision.length > 0.0f ? collision.length : 0.2f;
        collider.shape = ColliderShape::Cylinder;
        collider.cylinderSize = Vector3(radius * 2.0f, length, radius * 2.0f);
        collider.needsUpdate = true;
    } else if (collision.geometryType == "mesh" && !collision.meshFilename.empty()) {
        // Mesh碰撞体：使用mesh文件名
        // 注意：mesh路径需要与visual mesh相同的路径解析逻辑
        collider.SetMesh(collision.meshFilename, true);  // 默认使用凸包
    } else {
        // 未知类型，使用默认box
        Logger::GetInstance().WarningFormat("[URDFLoadSystem] Unknown collision geometry type '%s' for link '%s', using default box", 
            collision.geometryType.c_str(), urdfLink.name.c_str());
        collider.SetBox(Vector3(0.1f, 0.1f, 0.1f));
    }
    
    // 设置collision的origin偏移和旋转
    collider.offset = collision.origin;
    collider.rotation = collision.originRotation;
    
    m_world->AddComponent(linkEntity, collider);
    
    // 创建刚体组件
    RigidBodyComponent rigidBody;
    
    // 设置刚体类型
    if (isBaseLink) {
        rigidBody.type = baseLinkType;
        // Base link通常不受重力影响（除非是Dynamic类型）
        if (baseLinkType == RigidBodyType::Kinematic) {
            rigidBody.useGravity = false;
            rigidBody.syncMode = RigidBodyComponent::SyncMode::TransformToPhysics;
        } else if (baseLinkType == RigidBodyType::Static) {
            rigidBody.useGravity = false;
            rigidBody.mass = 0.0f;  // 静态物体质量为0
        }
    } else {
        rigidBody.type = RigidBodyType::Dynamic;
        rigidBody.useGravity = true;
        rigidBody.syncMode = RigidBodyComponent::SyncMode::PhysicsToTransform;
    }
    
    // 设置质量（从URDF）
    if (urdfLink.mass > 0.0f) {
        rigidBody.mass = urdfLink.mass;
    } else {
        // 如果没有质量信息，使用默认值
        rigidBody.mass = isBaseLink ? 10.0f : 1.0f;
    }
    
    // 设置物理属性（默认值，可以从URDF扩展）
    rigidBody.friction = 0.7f;
    rigidBody.restitution = 0.1f;
    rigidBody.linearDamping = 0.1f;
    rigidBody.angularDamping = 0.2f;
    
    // 注意：惯性张量由PhysicsSystem根据碰撞形状自动计算
    // 如果需要使用URDF中定义的自定义惯性，需要扩展RigidBodyComponent支持自定义惯性
    // 当前实现让Bullet从形状自动计算惯性
    
    m_world->AddComponent(linkEntity, rigidBody);
    
    Logger::GetInstance().InfoFormat("[URDFLoadSystem] Created physics components for link '%s' (type: %s, mass: %.3f)", 
        urdfLink.name.c_str(), 
        isBaseLink ? (baseLinkType == RigidBodyType::Static ? "Static" : (baseLinkType == RigidBodyType::Kinematic ? "Kinematic" : "Dynamic")) : "Dynamic",
        rigidBody.mass);
}

void URDFLoadSystem::CreateJointConstraint(EntityID jointEntity, const Render::Robot::URDFJoint& urdfJoint,
                                           EntityID parentLinkEntity, EntityID childLinkEntity) {
    if (!m_world || !jointEntity.IsValid() || !parentLinkEntity.IsValid() || !childLinkEntity.IsValid()) {
        return;
    }
    
    // 检查连接的link是否都有物理组件
    if (!m_world->HasComponent<RigidBodyComponent>(parentLinkEntity) ||
        !m_world->HasComponent<RigidBodyComponent>(childLinkEntity)) {
        Logger::GetInstance().WarningFormat("[URDFLoadSystem] Joint '%s': parent or child link missing RigidBodyComponent, skipping constraint", 
            urdfJoint.name.c_str());
        return;
    }
    
    // Fixed joint不需要物理约束，使用Transform父子关系即可
    if (urdfJoint.type == Render::Robot::JointType::Fixed) {
        return;
    }
    
    // Floating joint不需要约束
    if (urdfJoint.type == Render::Robot::JointType::Floating) {
        return;
    }
    
    // 创建约束组件（添加到child link实体上）
    ConstraintComponent constraint;
    constraint.connectedEntity = parentLinkEntity;
    
    // 根据joint类型设置约束类型和参数
    if (urdfJoint.type == Render::Robot::JointType::Revolute || 
        urdfJoint.type == Render::Robot::JointType::Continuous) {
        // 旋转关节：使用Hinge约束
        constraint.type = ConstraintType::Hinge;
        
        // pivotA：在child link坐标系中，约束点的位置
        // 因为child link在joint坐标系中是(0,0,0)，所以约束点在child link坐标系中也是(0,0,0)
        constraint.pivotA = Vector3::Zero();
        // pivotB：在parent link坐标系中，joint.origin的位置（约束点位置）
        constraint.pivotB = urdfJoint.origin;
        
        // axis：joint轴（归一化）
        // axisA应该在child link坐标系中，axisB应该在parent link坐标系中
        // joint.axis是在parent link坐标系中定义的，需要转换到child link坐标系
        Vector3 axisInParent = urdfJoint.axis.normalized();
        constraint.axisB = axisInParent;  // parent link坐标系中的轴
        
        // 转换到child link坐标系：child坐标系相对于parent坐标系有joint.originRotation的旋转
        // 所以axisA = joint.originRotation的逆 * axisInParent
        constraint.axisA = urdfJoint.originRotation.inverse() * axisInParent;
        constraint.axisA.normalize();
        
        // 限制（角度，单位：弧度）
        if (urdfJoint.type == Render::Robot::JointType::Revolute) {
            // Revolute：使用limits
            constraint.lowerLimit = urdfJoint.limits.lower;
            constraint.upperLimit = urdfJoint.limits.upper;
        } else {
            // Continuous：无限制（设置为大范围）
            constraint.lowerLimit = -3.14159f;  // -π
            constraint.upperLimit = 3.14159f;   // +π
        }
        
        // 设置约束稳定性参数（防止约束在大力矩下失效）
        constraint.erp = 0.2f;                    // 误差修正参数，0.1-0.3之间比较稳定
        constraint.maxImpulse = 1e10f;           // 最大约束冲量，设置很大以防止约束失效
        constraint.damping = 0.1f;               // 约束阻尼，减少振荡
        constraint.breakingImpulseThreshold = 1e10f;  // 断裂阈值，设置很大以防止意外断裂
        
    } else if (urdfJoint.type == Render::Robot::JointType::Prismatic) {
        // 平移关节：使用Generic6Dof约束
        // 注意：PhysicsSystem目前不支持Slider约束，使用Generic6Dof
        // Generic6Dof可以限制平移轴（0-2轴）和旋转轴（3-5轴）
        constraint.type = ConstraintType::Generic6Dof;
        
        constraint.pivotA = Vector3::Zero();  // 在child link坐标系中
        constraint.pivotB = urdfJoint.origin;  // 在parent link坐标系中
        
        // 轴方向转换
        Vector3 axisInParent = urdfJoint.axis.normalized();
        constraint.axisB = axisInParent;  // parent link坐标系中的轴
        constraint.axisA = urdfJoint.originRotation.inverse() * axisInParent;
        constraint.axisA.normalize();
        
        // Prismatic的限制是距离（米）
        constraint.lowerLimit = urdfJoint.limits.lower;
        constraint.upperLimit = urdfJoint.limits.upper;
        
        // 设置约束稳定性参数
        constraint.erp = 0.2f;
        constraint.maxImpulse = 1e10f;
        constraint.damping = 0.1f;
        constraint.breakingImpulseThreshold = 1e10f;
        
        // 注意：Generic6Dof的限制设置比较复杂，需要在PhysicsSystem中特殊处理
        // 这里只设置基本限制值，实际限制由PhysicsSystem根据axis方向设置
        
    } else if (urdfJoint.type == Render::Robot::JointType::Planar) {
        // 平面关节：使用Generic6Dof约束
        constraint.type = ConstraintType::Generic6Dof;
        
        constraint.pivotA = Vector3::Zero();  // 在child link坐标系中
        constraint.pivotB = urdfJoint.origin;  // 在parent link坐标系中
        
        // 轴方向转换
        Vector3 axisInParent = urdfJoint.axis.normalized();
        constraint.axisB = axisInParent;  // parent link坐标系中的轴
        constraint.axisA = urdfJoint.originRotation.inverse() * axisInParent;
        constraint.axisA.normalize();
        
        // Planar关节的限制需要特殊处理（这里简化）
        constraint.lowerLimit = urdfJoint.limits.lower;
        constraint.upperLimit = urdfJoint.limits.upper;
        
    } else {
        // 未知类型，不创建约束
        Logger::GetInstance().WarningFormat("[URDFLoadSystem] Joint '%s' has unsupported type for constraint creation", 
            urdfJoint.name.c_str());
        return;
    }
    
    // 将约束组件添加到child link实体上
    // 注意：约束应该添加到有约束的实体上，这里添加到child link
    m_world->AddComponent(childLinkEntity, constraint);
    
    Logger::GetInstance().InfoFormat("[URDFLoadSystem] Created constraint for joint '%s' (type: %d, limits: [%.3f, %.3f])", 
        urdfJoint.name.c_str(), 
        static_cast<int>(constraint.type),
        constraint.lowerLimit, constraint.upperLimit);
}

EntityID URDFLoadSystem::CreateLinkEntity(const Render::Robot::URDFLink& link, const std::string& meshBasePath) {
    if (!m_world) {
        return EntityID::Invalid();
    }
    
    EntityID linkEntity = m_world->CreateEntity();
    
    // 添加LinkComponent
    LinkComponent linkComp;
    linkComp.linkName = link.name;
    linkComp.isBaseLink = false;  // 稍后设置
    
    // 加载视觉mesh
    if (!link.visuals.empty()) {
        for (size_t i = 0; i < link.visuals.size(); ++i) {
            const auto& visual = link.visuals[i];
            
            if (visual.geometryType == "mesh" && !visual.meshFilename.empty()) {
                // 解析mesh路径（相对于URDF文件所在目录）
                std::string meshPath = visual.meshFilename;
                
                // 处理路径：mesh路径是相对于URDF文件所在目录的
                if (meshPath.find("package://") == 0) {
                    // ROS package://路径：暂时不支持，记录警告
                    Logger::GetInstance().WarningFormat("[URDFLoadSystem] package:// paths not supported for link '%s': %s", 
                        link.name.c_str(), meshPath.c_str());
                    continue;
                } else if (meshPath[0] == '/' || (meshPath.length() > 1 && meshPath[1] == ':')) {
                    // 绝对路径（Unix: 以/开头，Windows: 以C:等开头），直接使用
                    // 不做处理
                } else {
                    // 相对路径：相对于URDF文件所在目录
                    // 处理../和./等相对路径
                    if (!meshBasePath.empty()) {
                        // 规范化路径：处理../和./
                        std::string basePath = meshBasePath;
                        // 移除末尾的/或\（如果有）
                        while (!basePath.empty() && (basePath.back() == '/' || basePath.back() == '\\')) {
                            basePath.pop_back();
                        }
                        
                        // 处理../前缀
                        size_t upLevels = 0;
                        std::string remainingPath = meshPath;
                        while (remainingPath.find("../") == 0) {
                            upLevels++;
                            remainingPath = remainingPath.substr(3);
                        }
                        
                        // 移除basePath末尾的目录层级
                        for (size_t i = 0; i < upLevels && !basePath.empty(); ++i) {
                            size_t lastSep = basePath.find_last_of("/\\");
                            if (lastSep != std::string::npos) {
                                basePath = basePath.substr(0, lastSep);
                            } else {
                                basePath.clear();
                                break;
                            }
                        }
                        
                        // 拼接路径
                        if (!basePath.empty()) {
                            meshPath = basePath + "/" + remainingPath;
                        } else {
                            meshPath = remainingPath;
                        }
                        
                        // 规范化路径分隔符（统一使用/）
                        std::replace(meshPath.begin(), meshPath.end(), '\\', '/');
                    }
                }
                
                // 加载mesh
                auto mesh = MeshLoader::LoadMeshFromFile(meshPath);
                if (mesh) {
                    // 直接设置visualMesh（不使用visualMeshes，简化处理）
                    linkComp.visualMesh = mesh;
                } else {
                    Logger::GetInstance().WarningFormat("[URDFLoadSystem] Failed to load mesh for link '%s': %s", 
                        link.name.c_str(), meshPath.c_str());
                }
            }
        }
    }
    
    // 如果没有mesh，尝试从visuals创建默认几何体（box、cylinder等）
    if (!linkComp.visualMesh && !link.visuals.empty()) {
        for (const auto& visual : link.visuals) {
            if (visual.geometryType == "box" && visual.size.norm() > 0.0f) {
                // 创建box mesh
                linkComp.visualMesh = MeshLoader::CreateCube(
                    visual.size.x(), visual.size.y(), visual.size.z(), Color::White());
                break;
            } else if (visual.geometryType == "cylinder" && visual.radius > 0.0f && visual.length > 0.0f) {
                // 创建cylinder mesh
                linkComp.visualMesh = MeshLoader::CreateCylinder(
                    visual.radius, visual.radius, visual.length, 32, Color::White());
                break;
            } else if (visual.geometryType == "sphere" && visual.radius > 0.0f) {
                // 创建sphere mesh
                linkComp.visualMesh = MeshLoader::CreateSphere(
                    visual.radius, 32, 16, Color::White());
                break;
            }
        }
    }
    
    // 现在添加LinkComponent到world（在mesh加载完成后）
    m_world->AddComponent(linkEntity, linkComp);
    
    // 添加TransformComponent
    TransformComponent transformComp;
    
    // 只应用第一个visual的origin（位置和旋转）到link的Transform
    // scale不应该应用到这里，因为标准的变换矩阵顺序是 T×R×S，会导致position被scale影响
    // scale应该单独应用到mesh子实体
    if (!link.visuals.empty()) {
        const auto& visual = link.visuals[0];
        // 只应用origin的位置和旋转，不应用scale
        if (visual.origin.norm() > 0.001f || visual.originRotation.norm() > 0.001f) {
            transformComp.SetPosition(visual.origin);
            transformComp.SetRotation(visual.originRotation);
        }
    }
    
    m_world->AddComponent(linkEntity, transformComp);
    
    // 从world中获取LinkComponent（确保使用world中的版本）
    if (!m_world->HasComponent<LinkComponent>(linkEntity)) {
        Logger::GetInstance().ErrorFormat("[URDFLoadSystem] LinkComponent not found in world for link '%s'", link.name.c_str());
        return linkEntity;
    }
    
    auto& worldLinkComp = m_world->GetComponent<LinkComponent>(linkEntity);
    
    // 如果world中的visualMesh为空，但局部变量linkComp有mesh，说明AddComponent复制时丢失了数据
    // 这种情况下，我们需要重新设置world中的visualMesh
    if (!worldLinkComp.visualMesh && linkComp.visualMesh) {
        Logger::GetInstance().WarningFormat("[URDFLoadSystem] Link '%s': visualMesh lost during AddComponent, restoring from local copy (ptr: %p)", 
            link.name.c_str(), linkComp.visualMesh.get());
        worldLinkComp.visualMesh = linkComp.visualMesh;
    }
    
    // 添加MeshRenderComponent（如果有mesh）
    // 如果visual有scale，需要创建子实体专门用于mesh，scale只影响mesh
    if (worldLinkComp.visualMesh) {
        // 检查是否有visual以及是否有scale
        bool hasScale = false;
        Vector3 meshScale = Vector3::Ones();
        if (!link.visuals.empty()) {
            meshScale = link.visuals[0].scale;
            hasScale = meshScale != Vector3::Ones();
        }
        
        EntityID meshEntity = linkEntity;
        
        // 如果有scale，创建子实体专门用于mesh
        if (hasScale) {
            meshEntity = m_world->CreateEntity();
            TransformComponent meshTransformComp;
            // 子实体只应用scale，位置和旋转为0/Identity（相对于父实体）
            meshTransformComp.SetScale(meshScale);
            meshTransformComp.parentEntity = linkEntity;
            m_world->AddComponent(meshEntity, meshTransformComp);
        }
        
        MeshRenderComponent meshRenderComp;
        meshRenderComp.mesh = worldLinkComp.visualMesh;
        
        // 创建默认材质（如果没有从URDF加载材质）
        // 使用Phong着色器以显示光照效果
        auto& shaderCache = ShaderCache::GetInstance();
        auto shader = shaderCache.GetShader("material_phong");
        if (!shader) {
            // 如果缓存中没有，尝试加载
            shader = shaderCache.LoadShader("material_phong", "shaders/material_phong.vert", "shaders/material_phong.frag");
        }
        
        if (!shader) {
            // 如果加载失败，回退到basic着色器
            Logger::GetInstance().WarningFormat("[URDFLoadSystem] Failed to load phong shader for link '%s', falling back to basic shader", 
                link.name.c_str());
            shader = shaderCache.GetShader("basic");
            if (!shader) {
                shader = shaderCache.LoadShader("basic", "shaders/basic.vert", "shaders/basic.frag");
            }
        }
        
        if (shader) {
            auto material = std::make_shared<Material>();
            material->SetShader(shader);
            // 设置材质属性以显示光照效果
            material->SetAmbientColor(Color(0.3f, 0.3f, 0.35f, 1.0f));  // 环境光（柔和的蓝灰色）
            material->SetDiffuseColor(Color(0.7f, 0.7f, 0.8f, 1.0f));  // 漫反射（浅蓝灰色）
            material->SetSpecularColor(Color(0.5f, 0.5f, 0.5f, 1.0f)); // 镜面反射
            material->SetShininess(32.0f);  // 光泽度
            meshRenderComp.material = material;
        } else {
            Logger::GetInstance().WarningFormat("[URDFLoadSystem] Failed to load any shader for link '%s', mesh may not render correctly", 
                link.name.c_str());
        }
        
        meshRenderComp.visible = true;
        meshRenderComp.resourcesLoaded = true;  // 资源已加载
        m_world->AddComponent(meshEntity, meshRenderComp);
    } else {
        Logger::GetInstance().WarningFormat("[URDFLoadSystem] Link '%s' has no visual mesh or geometry", link.name.c_str());
    }
    
    return linkEntity;
}

EntityID URDFLoadSystem::CreateJointEntity(const Render::Robot::URDFJoint& joint, EntityID parentLinkEntity, EntityID childLinkEntity) {
    if (!m_world) {
        return EntityID::Invalid();
    }
    
    EntityID jointEntity = m_world->CreateEntity();
    
    // 添加JointComponent
    JointComponent jointComp;
    jointComp.jointName = joint.name;
    jointComp.parentLink = joint.parentLink;
    jointComp.childLink = joint.childLink;
    jointComp.type = joint.type;  // 类型已匹配
    jointComp.axis = joint.axis;
    jointComp.limits = joint.limits;
    jointComp.parentLinkEntity = parentLinkEntity;
    jointComp.childLinkEntity = childLinkEntity;
    
    m_world->AddComponent(jointEntity, jointComp);
    
    // 添加TransformComponent
    TransformComponent transformComp;
    m_world->AddComponent(jointEntity, transformComp);
    
    return jointEntity;
}

Ref<Mesh> URDFLoadSystem::LoadLinkMesh(const Render::Robot::URDFLink& /*link*/, const std::string& /*meshBasePath*/) {
    // 已经在CreateLinkEntity中实现
    return nullptr;
}

void RobotRenderSystem::Update(float deltaTime) {
    (void)deltaTime;
    // 渲染通常在专门的Render方法中处理
}

void RobotRenderSystem::RenderRobots(Renderer* renderer, const Camera& camera) {
    if (!m_world || !renderer) {
        return;
    }
    
    // 获取JointTFSystem
    if (!m_jointTFSystem) {
        m_jointTFSystem = m_world->GetSystem<JointTFSystem>();
    }
    
    // 获取所有机器人实体
    auto robots = m_world->Query<RobotComponent>();
    
    for (const auto& robotEntity : robots) {
        const auto& robotComp = m_world->GetComponent<RobotComponent>(robotEntity);
        if (!robotComp.model) {
            continue;
        }
        
        // 如果有JointTFSystem，获取TF并可视化
        if (m_jointTFSystem) {
            const auto& tfs = m_jointTFSystem->GetAllTFs(robotEntity);
            m_tfVisualizer.Render(renderer, tfs, camera);
        }
        
        // 渲染link的mesh（通过MeshRenderSystem自动处理）
        // 这里主要处理TF可视化
    }
}

// ============================================================================
// URDFLoadSystem 辅助函数实现
// ============================================================================

Vector3 URDFLoadSystem::ConvertURDFInertiaToBullet(const Matrix3& inertiaMatrix) {
    // 检查是否接近对角矩阵
    float offDiagonalMax = std::max({
        std::abs(inertiaMatrix(0, 1)), 
        std::abs(inertiaMatrix(0, 2)), 
        std::abs(inertiaMatrix(1, 2))
    });
    
    float diagonalMin = std::min({
        std::abs(inertiaMatrix(0, 0)), 
        std::abs(inertiaMatrix(1, 1)), 
        std::abs(inertiaMatrix(2, 2))
    });
    
    // 如果非对角元素相对于对角元素很小（<1%），直接提取对角元素
    if (diagonalMin > 0.0f && offDiagonalMax < diagonalMin * 0.01f) {
        return Vector3(inertiaMatrix(0, 0), inertiaMatrix(1, 1), inertiaMatrix(2, 2));
    }
    
    // 否则进行特征值分解（对角化）
    // URDF惯性矩阵是对称矩阵，使用SelfAdjointEigenSolver
    Eigen::SelfAdjointEigenSolver<Matrix3> solver(inertiaMatrix);
    if (solver.info() == Eigen::Success) {
        Vector3 eigenvalues = solver.eigenvalues();
        // 特征值是主惯性值（按升序排列，取绝对值以确保为正）
        return Vector3(
            std::abs(eigenvalues(0)), 
            std::abs(eigenvalues(1)), 
            std::abs(eigenvalues(2))
        );
    }
    
    // 如果分解失败，回退到对角元素
    Logger::GetInstance().Warning("[URDFLoadSystem] Eigenvalue decomposition failed, using diagonal elements");
    return Vector3(inertiaMatrix(0, 0), inertiaMatrix(1, 1), inertiaMatrix(2, 2));
}

} // namespace Robot
} // namespace ECS
} // namespace Render
