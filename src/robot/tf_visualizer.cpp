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

#include "render/robot/tf_visualizer.h"
#include "render/mesh_loader.h"
#include "render/shader.h"
#include "render/logger.h"
#include "render/geometry_preset.h"
#include "render/mesh.h"
#include "render/types.h"
#include <vector>

namespace Render {
namespace Robot {

TFVisualizer::TFVisualizer() {
    // 默认值已在头文件中初始化
}

bool TFVisualizer::Initialize() {
    if (m_initialized) {
        return true;
    }
    
    // 创建坐标轴mesh
    CreateAxisMesh();
    
    // 尝试加载线框着色器
    m_lineShader = std::make_shared<Shader>();
    // 注意：这里可以加载专门的线框着色器，暂时留空
    // 实际使用时可以通过ShaderCache加载或从文件加载
    
    m_initialized = true;
    return true;
}

void TFVisualizer::CreateAxisMesh() {
    // 创建坐标轴mesh：3条线（X/Y/Z轴）
    // X轴：红色，从(0,0,0)到(length,0,0)
    // Y轴：绿色，从(0,0,0)到(0,length,0)
    // Z轴：蓝色，从(0,0,0)到(0,0,length)
    
    std::vector<Vector3> vertices;
    std::vector<Vector3> colors;
    std::vector<uint32_t> indices;
    
    float length = m_axisLength;
    
    // X轴（红色）
    vertices.push_back(Vector3::Zero());
    vertices.push_back(Vector3(length, 0, 0));
    colors.push_back(Vector3(1, 0, 0));  // 红色
    colors.push_back(Vector3(1, 0, 0));
    indices.push_back(0);
    indices.push_back(1);
    
    // Y轴（绿色）
    vertices.push_back(Vector3::Zero());
    vertices.push_back(Vector3(0, length, 0));
    colors.push_back(Vector3(0, 1, 0));  // 绿色
    colors.push_back(Vector3(0, 1, 0));
    indices.push_back(2);
    indices.push_back(3);
    
    // Z轴（蓝色）
    vertices.push_back(Vector3::Zero());
    vertices.push_back(Vector3(0, 0, length));
    colors.push_back(Vector3(0, 0, 1));  // 蓝色
    colors.push_back(Vector3(0, 0, 1));
    indices.push_back(4);
    indices.push_back(5);
    
    // 创建mesh
    m_axisMesh = std::make_shared<Mesh>();
    if (m_axisMesh) {
        // 转换为Vertex格式
        std::vector<Vertex> vertexData;
        for (size_t i = 0; i < vertices.size(); ++i) {
            Vertex v;
            v.position = vertices[i];
            v.color = Color(colors[i].x(), colors[i].y(), colors[i].z(), 1.0f);
            vertexData.push_back(v);
        }
        
        // 设置顶点和索引数据
        m_axisMesh->SetVertices(vertexData);
        m_axisMesh->SetIndices(indices);
        m_axisMesh->Upload();
    }
}

void TFVisualizer::Render(
    Renderer* renderer,
    const std::unordered_map<std::string, ECS::Robot::JointTF>& tfs,
    const Camera& camera)
{
    if (!m_enabled || !renderer) {
        return;
    }
    
    if (!m_initialized) {
        if (!Initialize()) {
            Logger::GetInstance().Warning("[TFVisualizer] Failed to initialize");
            return;
        }
    }
    
    // 保存当前渲染状态
    // 注意：这里假设Renderer有保存/恢复状态的功能
    // 如果没有，可能需要手动管理
    
    // 设置线宽
    glLineWidth(m_lineWidth);
    
    // 绘制坐标轴
    if (m_showAxes && m_axisMesh) {
        for (const auto& pair : tfs) {
            const auto& jointName = pair.first;
            const auto& jointTF = pair.second;
            DrawAxis(jointTF.transform, renderer);
        }
    }
    
    // 绘制连接线（从父关节到子关节）
    if (m_showConnections) {
        // 这里需要知道父子关系，可以从RobotModel获取
        // 为了简化，这里先跳过，后续可以在RobotRenderSystem中实现
    }
    
    // 恢复渲染状态
    glLineWidth(1.0f);
}

void TFVisualizer::DrawAxis(const Transform& tf, Renderer* renderer) {
    if (!m_axisMesh || !renderer) {
        return;
    }
    
    // 注意：实际的坐标轴绘制需要通过Renderable系统或直接使用OpenGL
    // 这里先留空，后续可以通过创建Renderable对象来实现
    // 或者使用简单的OpenGL直接绘制
    
    // 简化实现：使用OpenGL直接绘制
    // 注意：这需要在正确的渲染上下文中调用
    Vector3 pos = tf.GetWorldPosition();
    Quaternion rot = tf.GetWorldRotation();
    
    // 计算坐标轴方向（世界空间）
    Vector3 xAxis = rot * Vector3(m_axisLength, 0, 0);
    Vector3 yAxis = rot * Vector3(0, m_axisLength, 0);
    Vector3 zAxis = rot * Vector3(0, 0, m_axisLength);
    
    // 使用OpenGL直接绘制（需要在正确的上下文中）
    // 这里先留空，实际实现需要：
    // 1. 使用Renderer的接口
    // 2. 或者创建专门的Renderable对象
    // 3. 或者使用现有的线框渲染系统
}

void TFVisualizer::DrawConnection(const Transform& parentTF, const Transform& childTF, Renderer* renderer) {
    if (!renderer) {
        return;
    }
    
    // 绘制从父关节到子关节的线
    Vector3 parentPos = parentTF.GetWorldPosition();
    Vector3 childPos = childTF.GetWorldPosition();
    
    // 使用简单的OpenGL绘制
    // 实际实现可能需要使用Renderer的接口
    // 这里先留空，后续实现
}

} // namespace Robot
} // namespace Render
