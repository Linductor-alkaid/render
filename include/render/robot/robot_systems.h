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

#include "render/robot/urdf_loader.h"
#include "render/robot/tf_visualizer.h"
#include "render/robot/joint_tf_system.h"
#include "render/robot/robot_model.h"
#include "render/ecs/system.h"
#include "render/ecs/entity.h"
#include "render/renderer.h"
#include "render/camera.h"
#include <string>
#include <unordered_map>
#include <memory>

namespace Render {
namespace ECS {

// 前向声明
class World;

namespace Robot {

/**
 * @brief URDF加载系统
 * 
 * 负责加载URDF文件并创建ECS实体
 */
class URDFLoadSystem : public System {
public:
    URDFLoadSystem() = default;
    ~URDFLoadSystem() override = default;
    
    void OnCreate(World* world) override {
        System::OnCreate(world);
    }
    
    void Update(float deltaTime) override {
        (void)deltaTime;
        // 加载系统通常不需要每帧更新
    }
    
    [[nodiscard]] int GetPriority() const override { return 5; }  // 高优先级
    
    /**
     * @brief 加载URDF文件并创建机器人实体
     * @param urdfPath URDF文件路径
     * @param meshBasePath mesh文件基准路径
     * @return 机器人实体ID，失败返回Invalid
     */
    EntityID LoadRobot(const std::string& urdfPath, const std::string& meshBasePath = "");
    
    /**
     * @brief 卸载机器人
     * @param robotEntity 机器人实体ID
     */
    void UnloadRobot(EntityID robotEntity);
    
    /**
     * @brief 检查机器人是否已加载
     */
    bool IsRobotLoaded(EntityID robotEntity) const;

private:
    /**
     * @brief 创建link实体
     */
    EntityID CreateLinkEntity(const Render::Robot::URDFLink& link, const std::string& meshBasePath);
    
    /**
     * @brief 创建joint实体
     */
    EntityID CreateJointEntity(const Render::Robot::URDFJoint& joint, EntityID parentLinkEntity, EntityID childLinkEntity);
    
    /**
     * @brief 加载link的mesh
     */
    Ref<Mesh> LoadLinkMesh(const Render::Robot::URDFLink& link, const std::string& meshBasePath);
    
    std::unordered_map<EntityID, Ref<Render::Robot::RobotModel>, EntityID::Hash> m_robots;
};

/**
 * @brief 机器人渲染系统
 * 
 * 负责渲染机器人模型和TF可视化
 */
class RobotRenderSystem : public System {
public:
    RobotRenderSystem() = default;
    ~RobotRenderSystem() override = default;
    
    void OnCreate(World* world) override {
        System::OnCreate(world);
        m_tfVisualizer.Initialize();
    }
    
    void Update(float deltaTime) override;
    
    [[nodiscard]] int GetPriority() const override { return 200; }  // 在普通渲染系统之后
    
    /**
     * @brief 渲染机器人
     * @param renderer 渲染器
     * @param camera 相机
     */
    void RenderRobots(Renderer* renderer, const Camera& camera);
    
    /**
     * @brief 获取TF可视化器
     */
    Render::Robot::TFVisualizer& GetTFVisualizer() { return m_tfVisualizer; }
    const Render::Robot::TFVisualizer& GetTFVisualizer() const { return m_tfVisualizer; }

private:
    Render::Robot::TFVisualizer m_tfVisualizer;
    JointTFSystem* m_jointTFSystem = nullptr;  // 从World获取（延迟初始化）
};

} // namespace Robot
} // namespace ECS
} // namespace Render
