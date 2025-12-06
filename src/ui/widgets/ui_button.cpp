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
#include "render/ui/widgets/ui_button.h"

namespace Render::UI {

UIButton::UIButton(std::string id)
    : UIWidget(std::move(id)) {}

void UIButton::SetLabel(std::string label) {
    if (m_label == label) {
        return;
    }
    m_label = std::move(label);
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIButton::OnMouseEnter() {
    SetHovered(true);
}

void UIButton::OnMouseLeave() {
    SetHovered(false);
}

void UIButton::OnMouseButton(uint8_t button, bool pressed, const Vector2&) {
    if (button != SDL_BUTTON_LEFT || !IsEnabled()) {
        return;
    }
    SetPressed(pressed);
}

void UIButton::OnMouseClick(uint8_t button, const Vector2&) {
    if (button != SDL_BUTTON_LEFT || !IsEnabled()) {
        return;
    }
    if (m_onClicked) {
        m_onClicked(*this);
    }
}

void UIButton::OnFocusLost() {
    SetPressed(false);
}

void UIButton::SetPressed(bool pressed) {
    if (m_pressed == pressed) {
        return;
    }
    m_pressed = pressed;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIButton::SetHovered(bool hovered) {
    if (m_hovered == hovered) {
        return;
    }
    m_hovered = hovered;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

} // namespace Render::UI


