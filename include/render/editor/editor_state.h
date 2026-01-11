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
#pragma once

#include <string>
#include <unordered_map>
#include <functional>

namespace Render {
namespace Editor {

/**
 * @brief 编辑器模式枚举
 */
enum class EditorMode {
    SceneEdit,      // 场景编辑模式
    AnimationEdit,  // 动画编辑模式
    PhysicsEdit,    // 物理编辑模式
    URDFEdit,       // URDF编辑模式
    Play            // 播放模式
};

/**
 * @brief 编辑器状态机
 * 
 * 管理编辑器的模式切换和状态保存/恢复。
 */
class EditorState {
public:
    EditorState();
    ~EditorState() = default;

    EditorState(const EditorState&) = delete;
    EditorState& operator=(const EditorState&) = delete;

    /**
     * @brief 获取当前编辑模式
     */
    [[nodiscard]] EditorMode GetCurrentMode() const noexcept { return m_currentMode; }

    /**
     * @brief 设置编辑模式
     * @param mode 目标模式
     * @return 是否成功切换
     */
    bool SetMode(EditorMode mode);

    /**
     * @brief 检查是否可以切换模式
     * @return 如果可以切换返回true
     */
    [[nodiscard]] bool CanSwitchMode() const noexcept { return m_canSwitch; }

    /**
     * @brief 设置是否可以切换模式（例如：有未保存更改时设置为false）
     */
    void SetCanSwitch(bool canSwitch) noexcept { m_canSwitch = canSwitch; }

    /**
     * @brief 保存当前模式的状态
     */
    void SaveState(EditorMode mode);

    /**
     * @brief 恢复指定模式的状态
     */
    void RestoreState(EditorMode mode);

    /**
     * @brief 清除指定模式的状态
     */
    void ClearState(EditorMode mode);

    /**
     * @brief 清除所有状态
     */
    void ClearAllStates();

    /**
     * @brief 设置模式切换回调
     * @param callback 回调函数,参数为(旧模式,新模式)
     */
    void SetModeChangeCallback(std::function<void(EditorMode, EditorMode)> callback);

private:
    EditorMode m_currentMode = EditorMode::SceneEdit;
    bool m_canSwitch = true;

    // 每个模式的状态快照(使用字符串存储,未来可以扩展为更复杂的结构)
    std::unordered_map<EditorMode, std::string> m_stateSnapshots;

    // 模式切换回调
    std::function<void(EditorMode, EditorMode)> m_modeChangeCallback;
};

/**
 * @brief 将EditorMode转换为字符串
 */
inline std::string EditorModeToString(EditorMode mode) {
    switch (mode) {
        case EditorMode::SceneEdit: return "SceneEdit";
        case EditorMode::AnimationEdit: return "AnimationEdit";
        case EditorMode::PhysicsEdit: return "PhysicsEdit";
        case EditorMode::URDFEdit: return "URDFEdit";
        case EditorMode::Play: return "Play";
        default: return "Unknown";
    }
}

/**
 * @brief 从字符串解析EditorMode
 */
inline EditorMode EditorModeFromString(const std::string& str) {
    if (str == "SceneEdit") return EditorMode::SceneEdit;
    if (str == "AnimationEdit") return EditorMode::AnimationEdit;
    if (str == "PhysicsEdit") return EditorMode::PhysicsEdit;
    if (str == "URDFEdit") return EditorMode::URDFEdit;
    if (str == "Play") return EditorMode::Play;
    return EditorMode::SceneEdit; // 默认返回场景编辑模式
}

} // namespace Editor
} // namespace Render
