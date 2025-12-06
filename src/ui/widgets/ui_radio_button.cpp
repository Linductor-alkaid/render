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
#include "render/ui/widgets/ui_radio_button.h"

#include <algorithm>

#include "render/logger.h"

namespace Render::UI {

// UIRadioButtonGroup 实现

void UIRadioButtonGroup::RegisterButton(UIRadioButton* button) {
    if (!button) {
        return;
    }
    
    auto it = std::find(m_buttons.begin(), m_buttons.end(), button);
    if (it == m_buttons.end()) {
        m_buttons.push_back(button);
    }
}

void UIRadioButtonGroup::UnregisterButton(UIRadioButton* button) {
    if (!button) {
        return;
    }
    
    auto it = std::find(m_buttons.begin(), m_buttons.end(), button);
    if (it != m_buttons.end()) {
        m_buttons.erase(it);
    }
    
    if (m_selectedButton == button) {
        m_selectedButton = nullptr;
    }
}

void UIRadioButtonGroup::SelectButton(UIRadioButton* button) {
    if (!button) {
        return;
    }
    
    // 取消选择当前选中的按钮
    if (m_selectedButton && m_selectedButton != button) {
        m_selectedButton->SetSelectedInternal(false, false);
    }
    
    // 选择新按钮
    m_selectedButton = button;
    button->SetSelectedInternal(true, false);
}

// UIRadioButton 实现

UIRadioButton::UIRadioButton(std::string id)
    : UIWidget(std::move(id)) {
    // 设置默认尺寸
    SetPreferredSize(Vector2(200.0f, 24.0f));
    SetMinSize(Vector2(24.0f, 24.0f));
}

void UIRadioButton::SetLabel(std::string label) {
    if (m_label == label) {
        return;
    }
    m_label = std::move(label);
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIRadioButton::SetSelected(bool selected) {
    if (m_selected == selected) {
        return;
    }
    
    SetSelectedInternal(selected, true);
}

void UIRadioButton::SetGroup(UIRadioButtonGroup* group) {
    // 从旧组中移除
    if (m_group) {
        m_group->UnregisterButton(this);
    }
    
    // 添加到新组
    m_group = group;
    if (m_group) {
        m_group->RegisterButton(this);
    }
}

void UIRadioButton::OnMouseEnter() {
    SetHovered(true);
}

void UIRadioButton::OnMouseLeave() {
    SetHovered(false);
}

void UIRadioButton::OnMouseClick(uint8_t button, const Vector2&) {
    if (button != SDL_BUTTON_LEFT || !IsEnabled()) {
        return;
    }
    
    // 如果已经有组，通过组来选择（会自动取消选择同组其他按钮）
    if (m_group) {
        m_group->SelectButton(this);
    } else {
        // 如果没有组，直接切换状态
        SetSelected(!m_selected);
    }
}

void UIRadioButton::OnFocusLost() {
    SetHovered(false);
}

void UIRadioButton::SetHovered(bool hovered) {
    if (m_hovered == hovered) {
        return;
    }
    m_hovered = hovered;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIRadioButton::SetSelectedInternal(bool selected, bool notifyGroup) {
    if (m_selected == selected) {
        return;
    }
    
    m_selected = selected;
    MarkDirty(UIWidgetDirtyFlag::Layout);
    
    // 如果通过组选择，不需要通知组（避免循环）
    if (notifyGroup && m_group && selected) {
        m_group->SelectButton(this);
    }
    
    // 调用回调
    if (m_onChanged) {
        m_onChanged(*this, m_selected);
    }
}

} // namespace Render::UI

