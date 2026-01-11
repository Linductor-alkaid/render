/*
 * Copyright (c) 2026 Li Chaoyu
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
#include "render/editor/editor_state.h"

#include "render/logger.h"

namespace Render::Editor {

EditorState::EditorState() : m_currentMode(EditorMode::SceneEdit), m_canSwitch(true) {
}

bool EditorState::SetMode(EditorMode mode) {
    if (mode == m_currentMode) {
        return true; // 已经是目标模式
    }

    if (!m_canSwitch) {
        Logger::GetInstance().Warning(
            "[EditorState] Cannot switch mode: switching is currently disabled");
        return false;
    }

    EditorMode oldMode = m_currentMode;

    // 保存当前模式的状态
    SaveState(m_currentMode);

    // 切换模式
    m_currentMode = mode;

    // 恢复新模式的状态（如果存在）
    RestoreState(mode);

    // 触发回调
    if (m_modeChangeCallback) {
        m_modeChangeCallback(oldMode, mode);
    }

    Logger::GetInstance().InfoFormat(
        "[EditorState] Mode switched from %s to %s",
        EditorModeToString(oldMode).c_str(),
        EditorModeToString(mode).c_str());

    return true;
}

void EditorState::SaveState(EditorMode mode) {
    // 目前使用空字符串作为占位符,未来可以扩展为更复杂的状态序列化
    // 例如：保存当前场景、选中的实体、视图设置等
    m_stateSnapshots[mode] = ""; // 占位符

    Logger::GetInstance().DebugFormat(
        "[EditorState] State saved for mode: %s",
        EditorModeToString(mode).c_str());
}

void EditorState::RestoreState(EditorMode mode) {
    auto it = m_stateSnapshots.find(mode);
    if (it != m_stateSnapshots.end()) {
        // 恢复状态（目前是占位符实现）
        Logger::GetInstance().DebugFormat(
            "[EditorState] State restored for mode: %s",
            EditorModeToString(mode).c_str());
    } else {
        Logger::GetInstance().DebugFormat(
            "[EditorState] No saved state found for mode: %s, using defaults",
            EditorModeToString(mode).c_str());
    }
}

void EditorState::ClearState(EditorMode mode) {
    m_stateSnapshots.erase(mode);
    Logger::GetInstance().DebugFormat(
        "[EditorState] State cleared for mode: %s",
        EditorModeToString(mode).c_str());
}

void EditorState::ClearAllStates() {
    m_stateSnapshots.clear();
    Logger::GetInstance().Debug("[EditorState] All states cleared");
}

void EditorState::SetModeChangeCallback(std::function<void(EditorMode, EditorMode)> callback) {
    m_modeChangeCallback = std::move(callback);
}

} // namespace Render::Editor
