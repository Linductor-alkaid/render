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
#include "render/application/operation_mapping.h"

#include <SDL3/SDL.h>

namespace Render::Application {

// ShortcutContext 实现
void ShortcutContext::RegisterShortcut(const KeyCombo& combo, Events::OperationType operation) {
    m_shortcuts[combo] = operation;
    m_operations[operation].push_back(combo);
}

bool ShortcutContext::HasShortcut(const KeyCombo& combo) const {
    return m_shortcuts.find(combo) != m_shortcuts.end();
}

Events::OperationType ShortcutContext::GetOperation(const KeyCombo& combo) const {
    auto it = m_shortcuts.find(combo);
    if (it != m_shortcuts.end()) {
        return it->second;
    }
    return Events::OperationType::Select;  // 默认操作
}

std::vector<KeyCombo> ShortcutContext::GetShortcuts(Events::OperationType operation) const {
    auto it = m_operations.find(operation);
    if (it != m_operations.end()) {
        return it->second;
    }
    return {};
}

bool ShortcutContext::CheckConflict(const KeyCombo& combo, Events::OperationType operation) const {
    auto it = m_shortcuts.find(combo);
    if (it != m_shortcuts.end() && it->second != operation) {
        return true;  // 冲突
    }
    return false;
}

// OperationMappingManager 实现
OperationMappingManager::OperationMappingManager() {
    InitializeBlenderDefaults();
}

void OperationMappingManager::AddContext(std::shared_ptr<ShortcutContext> context) {
    m_contexts[std::string(context->GetName())] = context;
}

void OperationMappingManager::SetCurrentContext(std::string_view contextName) {
    m_currentContext = std::string(contextName);
}

std::shared_ptr<ShortcutContext> OperationMappingManager::GetCurrentContext() const {
    if (m_currentContext.empty()) {
        return nullptr;
    }
    auto it = m_contexts.find(m_currentContext);
    if (it != m_contexts.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<ShortcutContext> OperationMappingManager::GetContext(std::string_view contextName) const {
    auto it = m_contexts.find(std::string(contextName));
    if (it != m_contexts.end()) {
        return it->second;
    }
    return nullptr;
}

Events::OperationType OperationMappingManager::GetOperationFromKey(const KeyCombo& combo) const {
    auto context = GetCurrentContext();
    if (context) {
        return context->GetOperation(combo);
    }
    return Events::OperationType::Select;
}

KeyCombo OperationMappingManager::MakeCombo(int scancode, bool ctrl, bool shift, bool alt) {
    KeyCombo combo;
    combo.scancode = scancode;
    combo.ctrl = ctrl;
    combo.shift = shift;
    combo.alt = alt;
    return combo;
}

void OperationMappingManager::InitializeBlenderDefaults() {
    // 创建默认上下文（Object Mode）
    auto objectMode = std::make_shared<ShortcutContext>("ObjectMode");
    
    // Blender风格快捷键映射
    // G - 移动
    objectMode->RegisterShortcut(MakeCombo(SDL_SCANCODE_G), Events::OperationType::Move);
    // R - 旋转
    objectMode->RegisterShortcut(MakeCombo(SDL_SCANCODE_R), Events::OperationType::Rotate);
    // S - 缩放
    objectMode->RegisterShortcut(MakeCombo(SDL_SCANCODE_S), Events::OperationType::Scale);
    // X - 删除
    objectMode->RegisterShortcut(MakeCombo(SDL_SCANCODE_X), Events::OperationType::Delete);
    // Shift+D - 复制
    objectMode->RegisterShortcut(MakeCombo(SDL_SCANCODE_D, false, true), Events::OperationType::Duplicate);
    // Esc - 取消
    objectMode->RegisterShortcut(MakeCombo(SDL_SCANCODE_ESCAPE), Events::OperationType::Cancel);
    // Enter - 确认
    objectMode->RegisterShortcut(MakeCombo(SDL_SCANCODE_RETURN), Events::OperationType::Confirm);
    
    AddContext(objectMode);
    SetCurrentContext("ObjectMode");
    
    // 创建编辑模式上下文
    auto editMode = std::make_shared<ShortcutContext>("EditMode");
    
    // 编辑模式使用相同的快捷键
    editMode->RegisterShortcut(MakeCombo(SDL_SCANCODE_G), Events::OperationType::Move);
    editMode->RegisterShortcut(MakeCombo(SDL_SCANCODE_R), Events::OperationType::Rotate);
    editMode->RegisterShortcut(MakeCombo(SDL_SCANCODE_S), Events::OperationType::Scale);
    editMode->RegisterShortcut(MakeCombo(SDL_SCANCODE_X), Events::OperationType::Delete);
    editMode->RegisterShortcut(MakeCombo(SDL_SCANCODE_D, false, true), Events::OperationType::Duplicate);
    editMode->RegisterShortcut(MakeCombo(SDL_SCANCODE_ESCAPE), Events::OperationType::Cancel);
    editMode->RegisterShortcut(MakeCombo(SDL_SCANCODE_RETURN), Events::OperationType::Confirm);
    
    AddContext(editMode);
}

} // namespace Render::Application

