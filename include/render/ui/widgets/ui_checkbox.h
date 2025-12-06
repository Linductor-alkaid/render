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

#include <functional>
#include <string>

#include <SDL3/SDL_mouse.h>

#include "render/ui/ui_widget.h"

namespace Render::UI {

/**
 * @brief 复选框控件
 * 参考 Blender 的 UI_WTYPE_CHECKBOX 实现
 * 支持选中/未选中/不确定三种状态
 */
class UICheckBox : public UIWidget {
public:
    using ChangeHandler = std::function<void(UICheckBox&, bool checked)>;

    /**
     * @brief 复选框状态
     */
    enum class State {
        Unchecked,    // 未选中
        Checked,      // 选中
        Indeterminate // 不确定（三态）
    };

    explicit UICheckBox(std::string id);

    /**
     * @brief 设置标签文本
     */
    void SetLabel(std::string label);
    [[nodiscard]] const std::string& GetLabel() const noexcept { return m_label; }

    /**
     * @brief 设置复选框状态
     */
    void SetChecked(bool checked);
    void SetState(State state);
    [[nodiscard]] State GetState() const noexcept { return m_state; }
    [[nodiscard]] bool IsChecked() const noexcept { return m_state == State::Checked; }

    /**
     * @brief 设置是否支持三态（不确定状态）
     */
    void SetTristate(bool tristate) noexcept;
    [[nodiscard]] bool IsTristate() const noexcept { return m_tristate; }

    /**
     * @brief 设置状态改变回调
     */
    void SetOnChanged(ChangeHandler handler) { m_onChanged = std::move(handler); }

    /**
     * @brief 获取悬停状态
     */
    [[nodiscard]] bool IsHovered() const noexcept { return m_hovered; }

    /**
     * @brief 设置标签位置（左侧或右侧）
     */
    void SetLabelPosition(bool labelOnLeft) noexcept;
    [[nodiscard]] bool IsLabelOnLeft() const noexcept { return m_labelOnLeft; }

protected:
    void OnMouseEnter() override;
    void OnMouseLeave() override;
    void OnMouseClick(uint8_t button, const Vector2& position) override;
    void OnFocusLost() override;

private:
    void SetHovered(bool hovered);
    void ToggleState();

    std::string m_label;
    State m_state = State::Unchecked;
    bool m_tristate = false;
    bool m_hovered = false;
    bool m_labelOnLeft = false; // false = 标签在右侧（默认），true = 标签在左侧
    ChangeHandler m_onChanged;
};

} // namespace Render::UI

