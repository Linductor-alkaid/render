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
#include <LinearMath/btIDebugDraw.h>
#include <vector>

namespace Render {
namespace ECS {

// 前向声明
class World;

/**
 * @brief 物理调试渲染器
 * 
 * 实现 Bullet 的 btIDebugDraw 接口，用于可视化物理世界
 */
class PhysicsDebugRenderer : public btIDebugDraw {
public:
    PhysicsDebugRenderer();
    ~PhysicsDebugRenderer() override = default;
    
    // ==================== btIDebugDraw 接口实现 ====================
    
    void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override;
    
    void drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, 
                         btScalar distance, int lifeTime, const btVector3& color) override;
    
    void reportErrorWarning(const char* warningString) override;
    
    void draw3dText(const btVector3& location, const char* textString) override;
    
    void setDebugMode(int debugMode) override { m_debugMode = debugMode; }
    
    int getDebugMode() const override { return m_debugMode; }
    
    // ==================== 控制接口 ====================
    
    /**
     * @brief 设置是否启用调试绘制
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    
    /**
     * @brief 获取是否启用
     */
    [[nodiscard]] bool IsEnabled() const { return m_enabled; }
    
    /**
     * @brief 清空调试绘制数据（每帧调用）
     */
    void Clear();
    
    // ==================== 调试模式设置 ====================
    
    /**
     * @brief 设置显示碰撞体线框
     */
    void SetShowWireframe(bool show);
    
    /**
     * @brief 设置显示 AABB
     */
    void SetShowAABB(bool show);
    
    /**
     * @brief 设置显示接触点
     */
    void SetShowContacts(bool show);
    
    /**
     * @brief 设置显示约束
     */
    void SetShowConstraints(bool show);
    
    // ==================== 调试绘制数据结构 ====================
    
    /**
     * @brief 调试线条
     */
    struct DebugLine {
        Vector3 from;
        Vector3 to;
        Color color;
    };
    
    /**
     * @brief 调试接触点
     */
    struct DebugContact {
        Vector3 point;
        Vector3 normal;
        float distance;
        Color color;
    };
    
    /**
     * @brief 获取当前帧的调试线条数据（用于后续渲染）
     */
    [[nodiscard]] const std::vector<DebugLine>& GetDebugLines() const { return m_debugLines; }
    
    /**
     * @brief 获取当前帧的接触点数据
     */
    [[nodiscard]] const std::vector<DebugContact>& GetDebugContacts() const { return m_debugContacts; }
    
private:
    
    int m_debugMode = DBG_DrawWireframe | DBG_DrawAabb;  ///< 调试模式
    bool m_enabled = false;                               ///< 是否启用
    
    std::vector<DebugLine> m_debugLines;       ///< 调试线条列表
    std::vector<DebugContact> m_debugContacts; ///< 调试接触点列表
};

} // namespace ECS
} // namespace Render
