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

#include "render/robot/joint_tf_system.h"
#include "render/renderer.h"
#include "render/camera.h"
#include "render/mesh.h"
#include "render/shader.h"
#include "render/types.h"
#include <unordered_map>
#include <string>

namespace Render {
namespace Robot {

/**
 * @brief TF可视化器
 * 
 * 渲染关节坐标系（坐标轴）和连接线
 */
class TFVisualizer {
public:
    TFVisualizer();
    ~TFVisualizer() = default;
    
    /**
     * @brief 设置是否启用可视化
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    [[nodiscard]] bool IsEnabled() const { return m_enabled; }
    
    /**
     * @brief 设置是否显示坐标轴
     */
    void SetShowAxes(bool show) { m_showAxes = show; }
    [[nodiscard]] bool IsShowAxes() const { return m_showAxes; }
    
    /**
     * @brief 设置是否显示连接线
     */
    void SetShowConnections(bool show) { m_showConnections = show; }
    [[nodiscard]] bool IsShowConnections() const { return m_showConnections; }
    
    /**
     * @brief 设置坐标轴长度
     */
    void SetAxisLength(float length) { m_axisLength = length; }
    [[nodiscard]] float GetAxisLength() const { return m_axisLength; }
    
    /**
     * @brief 设置线宽
     */
    void SetLineWidth(float width) { m_lineWidth = width; }
    [[nodiscard]] float GetLineWidth() const { return m_lineWidth; }
    
    /**
     * @brief 渲染TF
     * @param renderer 渲染器
     * @param tfs 关节TF映射
     * @param camera 相机（用于计算投影）
     */
    void Render(
        Renderer* renderer,
        const std::unordered_map<std::string, ECS::Robot::JointTF>& tfs,
        const Camera& camera
    );
    
    /**
     * @brief 初始化（创建坐标轴mesh等）
     */
    bool Initialize();

private:
    /**
     * @brief 创建坐标轴mesh
     */
    void CreateAxisMesh();
    
    /**
     * @brief 绘制坐标轴
     * @param tf 关节变换
     * @param renderer 渲染器
     */
    void DrawAxis(const Transform& tf, Renderer* renderer);
    
    /**
     * @brief 绘制连接线
     * @param parentTF 父关节TF
     * @param childTF 子关节TF
     * @param renderer 渲染器
     */
    void DrawConnection(const Transform& parentTF, const Transform& childTF, Renderer* renderer);
    
    bool m_enabled = true;
    bool m_showAxes = true;
    bool m_showConnections = true;
    float m_axisLength = 0.1f;
    float m_lineWidth = 2.0f;
    
    Ref<Mesh> m_axisMesh;  // 坐标轴mesh（X/Y/Z轴）
    Ref<Shader> m_lineShader;  // 线框着色器
    bool m_initialized = false;
};

} // namespace Robot
} // namespace Render
