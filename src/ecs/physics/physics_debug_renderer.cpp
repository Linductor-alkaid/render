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

#include "render/ecs/physics/physics_debug_renderer.h"
#include "render/logger.h"
#include <btBulletDynamicsCommon.h>
#include <LinearMath/btVector3.h>

namespace Render {
namespace ECS {

PhysicsDebugRenderer::PhysicsDebugRenderer() {
    m_debugMode = DBG_DrawWireframe | DBG_DrawAabb;
    m_enabled = false;
}

void PhysicsDebugRenderer::drawLine(const btVector3& from, const btVector3& to, const btVector3& color) {
    if (!m_enabled) {
        return;
    }
    
    PhysicsDebugRenderer::DebugLine line;
    line.from = Vector3(from.x(), from.y(), from.z());
    line.to = Vector3(to.x(), to.y(), to.z());
    line.color = Color(color.x(), color.y(), color.z(), 1.0f);
    
    m_debugLines.push_back(line);
}

void PhysicsDebugRenderer::drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, 
                                            btScalar distance, int lifeTime, const btVector3& color) {
    if (!m_enabled) {
        return;
    }
    
    if (!(m_debugMode & DBG_DrawContactPoints)) {
        return;
    }
    
    PhysicsDebugRenderer::DebugContact contact;
    contact.point = Vector3(PointOnB.x(), PointOnB.y(), PointOnB.z());
    contact.normal = Vector3(normalOnB.x(), normalOnB.y(), normalOnB.z());
    contact.distance = distance;
    contact.color = Color(color.x(), color.y(), color.z(), 1.0f);
    
    m_debugContacts.push_back(contact);
}

void PhysicsDebugRenderer::reportErrorWarning(const char* warningString) {
    Logger::GetInstance().WarningFormat("[PhysicsDebugRenderer] %s", warningString);
}

void PhysicsDebugRenderer::draw3dText(const btVector3& location, const char* textString) {
    // 3D文本绘制暂不支持，如果需要可以后续实现
    (void)location;
    (void)textString;
}

void PhysicsDebugRenderer::Clear() {
    m_debugLines.clear();
    m_debugContacts.clear();
}

void PhysicsDebugRenderer::SetShowWireframe(bool show) {
    if (show) {
        m_debugMode |= DBG_DrawWireframe;
    } else {
        m_debugMode &= ~DBG_DrawWireframe;
    }
}

void PhysicsDebugRenderer::SetShowAABB(bool show) {
    if (show) {
        m_debugMode |= DBG_DrawAabb;
    } else {
        m_debugMode &= ~DBG_DrawAabb;
    }
}

void PhysicsDebugRenderer::SetShowContacts(bool show) {
    if (show) {
        m_debugMode |= DBG_DrawContactPoints;
    } else {
        m_debugMode &= ~DBG_DrawContactPoints;
    }
}

void PhysicsDebugRenderer::SetShowConstraints(bool show) {
    if (show) {
        m_debugMode |= DBG_DrawConstraints;
    } else {
        m_debugMode &= ~DBG_DrawConstraints;
    }
}

} // namespace ECS
} // namespace Render
