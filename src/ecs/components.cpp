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
#include "render/ecs/component_registry.h"
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

bool TransformComponent::ConnectChangeCallback(EntityID entity, std::shared_ptr<World> worldPtr) {
    // 检查transform是否有效
    if (!transform) {
        Logger::GetInstance().WarningFormat(
            "[TransformComponent] ConnectChangeCallback: Transform is null"
        );
        return false;
    }
    
    // 检查worldPtr是否有效
    if (!worldPtr) {
        Logger::GetInstance().WarningFormat(
            "[TransformComponent] ConnectChangeCallback: World pointer is null"
        );
        return false;
    }
    
    // 设置Transform的变化回调
    // 当Transform变化时，触发ComponentRegistry的组件变化事件
    transform->SetChangeCallback(
        [worldPtr, entity](const Transform* transform) {
            Logger::GetInstance().DebugFormat(
                "[TransformComponent] Transform change callback invoked for entity %u", entity.index
            );
            
            // 注意：回调在Transform的锁释放后调用，所以可以安全地访问World
            // 但是需要检查实体和组件是否仍然有效
            
            // 快速检查：实体是否有效
            if (!worldPtr->IsValidEntity(entity)) {
                Logger::GetInstance().DebugFormat(
                    "[TransformComponent] Entity %u is invalid, skipping component change event", entity.index
                );
                return;
            }
            
            // 快速检查：组件是否存在
            if (!worldPtr->GetComponentRegistry().HasComponent<TransformComponent>(entity)) {
                Logger::GetInstance().DebugFormat(
                    "[TransformComponent] Entity %u has no TransformComponent, skipping component change event", entity.index
                );
                return;
            }
            
            // 获取组件的最新值并触发事件
            try {
                const TransformComponent& comp = 
                    worldPtr->GetComponentRegistry().GetComponent<TransformComponent>(entity);
                
                Logger::GetInstance().DebugFormat(
                    "[TransformComponent] Calling OnComponentChanged for entity %u", entity.index
                );
                
                // 触发组件变化事件
                worldPtr->GetComponentRegistry().OnComponentChanged(entity, comp);
            } catch (const std::exception& e) {
                Logger::GetInstance().WarningFormat(
                    "[TransformComponent] Exception in change callback for entity %u: %s", entity.index, e.what()
                );
            } catch (...) {
                // 忽略异常，避免影响Transform的变化通知
                // 可能的原因：组件已被移除，或实体已被销毁
                Logger::GetInstance().WarningFormat(
                    "[TransformComponent] Unknown exception in change callback for entity %u", entity.index
                );
            }
        }
    );
    
    Logger::GetInstance().DebugFormat(
        "[TransformComponent] Connected change callback for entity %u", entity.index
    );
    return true;
}

void TransformComponent::DisconnectChangeCallback() {
    if (transform) {
        transform->ClearChangeCallback();
    }
}

} // namespace ECS
} // namespace Render

