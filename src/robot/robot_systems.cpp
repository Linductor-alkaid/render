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
#include "render/mesh_loader.h"
#include "render/material.h"
#include "render/shader_cache.h"
#include "render/logger.h"
#include "render/file_utils.h"
#include <algorithm>

namespace Render {
namespace ECS {
namespace Robot {

EntityID URDFLoadSystem::LoadRobot(const std::string& urdfPath, const std::string& meshBasePath) {
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
    for (const auto& pair : model->links) {
        const std::string& linkName = pair.first;
        const Render::Robot::URDFLink& link = pair.second;
        EntityID linkEntity = CreateLinkEntity(link, urdfDir);
        if (linkEntity.IsValid()) {
            robotComp.linkEntityMap[linkName] = linkEntity;
            model->linkEntities[linkName] = linkEntity;
            
            // 如果是基座link，设置父子关系
            if (linkName == model->baseLink) {
                robotComp.baseLinkEntity = linkEntity;
                
                // 设置link为机器人的子实体
                // 基座link的位置就是机器人的位置（相对于机器人实体为(0,0,0)）
                if (m_world->HasComponent<TransformComponent>(linkEntity)) {
                    auto& linkTransformComp = m_world->GetComponent<TransformComponent>(linkEntity);
                    linkTransformComp.SetPosition(Vector3::Zero());
                    linkTransformComp.SetRotation(Quaternion::Identity());
                    linkTransformComp.parentEntity = robotEntity;
                }
            }
        }
    }
    
    // 更新RobotComponent
    m_world->GetComponent<RobotComponent>(robotEntity) = robotComp;
    
    // 创建所有joint实体并设置父子关系
    for (const auto& pair : model->joints) {
        const Render::Robot::URDFJoint& joint = pair.second;
        
        auto parentIt = robotComp.linkEntityMap.find(joint.parentLink);
        auto childIt = robotComp.linkEntityMap.find(joint.childLink);
        
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

} // namespace Robot
} // namespace ECS
} // namespace Render
