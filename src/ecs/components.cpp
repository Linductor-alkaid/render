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
#include "render/ecs/components.h"
#include "render/ecs/world.h"
#include "render/logger.h"

namespace Render {
namespace ECS {

// ============================================================
// TransformComponent 实现
// ============================================================

bool TransformComponent::SetParentEntity(World* world, EntityID parent) {
    if (!world || !transform) {
        Logger::GetInstance().WarningFormat(
            "[TransformComponent] SetParentEntity: Invalid world or transform"
        );
        return false;
    }
    
    if (parent.IsValid()) {
        // 验证父实体存在
        if (!world->IsValidEntity(parent)) {
            Logger::GetInstance().WarningFormat(
                "[TransformComponent] SetParentEntity: Parent entity %u is invalid", 
                parent.index
            );
            return false;
        }
        
        // 验证父实体有 TransformComponent
        if (!world->HasComponent<TransformComponent>(parent)) {
            Logger::GetInstance().WarningFormat(
                "[TransformComponent] SetParentEntity: Parent entity %u has no TransformComponent", 
                parent.index
            );
            return false;
        }
        
        // 获取父 Transform
        auto& parentComp = world->GetComponent<TransformComponent>(parent);
        if (!parentComp.transform) {
            Logger::GetInstance().WarningFormat(
                "[TransformComponent] SetParentEntity: Parent entity %u has null Transform", 
                parent.index
            );
            return false;
        }
        
        // 设置父 Transform（会进行循环引用检查）
        bool success = transform->SetParent(parentComp.transform.get());
        if (!success) {
            Logger::GetInstance().WarningFormat(
                "[TransformComponent] SetParentEntity: Transform::SetParent failed "
                "(possible circular reference or depth limit)"
            );
            return false;
        }
        
        // 存储父实体 ID
        parentEntity = parent;
        
        Logger::GetInstance().DebugFormat(
            "[TransformComponent] Set parent entity %u successfully", parent.index
        );
        return true;
    } else {
        // 清除父对象
        parentEntity = EntityID::Invalid();
        transform->SetParent(nullptr);
        
        Logger::GetInstance().DebugFormat(
            "[TransformComponent] Cleared parent entity"
        );
        return true;
    }
}

bool TransformComponent::ValidateParentEntity(World* world) {
    if (!parentEntity.IsValid()) {
        // 无父实体，确保 Transform 也没有父对象
        if (transform && transform->GetParent() != nullptr) {
            Logger::GetInstance().WarningFormat(
                "[TransformComponent] Inconsistent state: no parentEntity but has parent pointer, fixing"
            );
            transform->SetParent(nullptr);
            return false;
        }
        return true;
    }
    
    // 有父实体 ID
    if (!world || !world->IsValidEntity(parentEntity)) {
        // 父实体已销毁
        Logger::GetInstance().DebugFormat(
            "[TransformComponent] Parent entity %u is no longer valid, clearing", 
            parentEntity.index
        );
        parentEntity = EntityID::Invalid();
        if (transform) {
            transform->SetParent(nullptr);
        }
        return false;
    }
    
    // 验证父实体仍有 TransformComponent
    if (!world->HasComponent<TransformComponent>(parentEntity)) {
        Logger::GetInstance().WarningFormat(
            "[TransformComponent] Parent entity %u lost TransformComponent, clearing", 
            parentEntity.index
        );
        parentEntity = EntityID::Invalid();
        if (transform) {
            transform->SetParent(nullptr);
        }
        return false;
    }
    
    // 验证 Transform 指针一致性
    auto& parentComp = world->GetComponent<TransformComponent>(parentEntity);
    if (transform && parentComp.transform) {
        if (transform->GetParent() != parentComp.transform.get()) {
            // Transform 指针不一致，需要重新同步
            Logger::GetInstance().DebugFormat(
                "[TransformComponent] Transform parent pointer inconsistent, will resync"
            );
            return false;  // 返回 false 表示需要重新同步
        }
    }
    
    return true;
}

} // namespace ECS
} // namespace Render

