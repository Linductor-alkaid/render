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
#include "render/ui/widgets/ui_menu_item.h"
#include "render/ui/widgets/ui_menu.h"

namespace Render::UI {

UIMenuItem::UIMenuItem(std::string id, UIMenuItemType type)
    : UIWidget(std::move(id))
    , m_type(type) {
    
    // 分隔符默认设置较小的高度
    if (m_type == UIMenuItemType::Separator) {
        SetPreferredSize({100.0f, 8.0f});
        SetEnabled(false);
    } else {
        SetPreferredSize({120.0f, 28.0f});
    }
}

void UIMenuItem::SetLabel(std::string label) {
    if (m_label == label) {
        return;
    }
    m_label = std::move(label);
    MarkDirty(UIWidgetDirtyFlag::Layout | UIWidgetDirtyFlag::Visual);
}

void UIMenuItem::SetIcon(std::string iconPath) {
    if (m_iconPath == iconPath) {
        return;
    }
    m_iconPath = std::move(iconPath);
    MarkDirty(UIWidgetDirtyFlag::Visual);
}

void UIMenuItem::SetShortcut(std::string shortcut) {
    if (m_shortcut == shortcut) {
        return;
    }
    m_shortcut = std::move(shortcut);
    MarkDirty(UIWidgetDirtyFlag::Layout | UIWidgetDirtyFlag::Visual);
}

void UIMenuItem::SetTooltip(std::string tooltip) {
    m_tooltip = std::move(tooltip);
}

void UIMenuItem::SetCheckable(bool checkable) {
    if (checkable && m_type != UIMenuItemType::Checkable) {
        m_type = UIMenuItemType::Checkable;
        MarkDirty(UIWidgetDirtyFlag::Visual);
    } else if (!checkable && m_type == UIMenuItemType::Checkable) {
        m_type = UIMenuItemType::Normal;
        m_checked = false;
        MarkDirty(UIWidgetDirtyFlag::Visual);
    }
}

void UIMenuItem::SetChecked(bool checked) {
    if (m_type != UIMenuItemType::Checkable) {
        return;
    }
    if (m_checked == checked) {
        return;
    }
    m_checked = checked;
    MarkDirty(UIWidgetDirtyFlag::Visual);
    
    if (m_onCheckChanged) {
        m_onCheckChanged(*this, m_checked);
    }
}

void UIMenuItem::SetSeparator(bool separator) {
    if (separator && m_type != UIMenuItemType::Separator) {
        m_type = UIMenuItemType::Separator;
        SetPreferredSize({100.0f, 8.0f});
        SetEnabled(false);
        MarkDirty(UIWidgetDirtyFlag::Layout | UIWidgetDirtyFlag::Visual);
    } else if (!separator && m_type == UIMenuItemType::Separator) {
        m_type = UIMenuItemType::Normal;
        SetPreferredSize({120.0f, 28.0f});
        SetEnabled(true);
        MarkDirty(UIWidgetDirtyFlag::Layout | UIWidgetDirtyFlag::Visual);
    }
}

void UIMenuItem::SetSubMenu(std::shared_ptr<UIMenu> subMenu) {
    if (m_subMenu == subMenu) {
        return;
    }
    m_subMenu = std::move(subMenu);
    
    if (m_subMenu) {
        m_type = UIMenuItemType::SubMenu;
    } else if (m_type == UIMenuItemType::SubMenu) {
        m_type = UIMenuItemType::Normal;
    }
    
    MarkDirty(UIWidgetDirtyFlag::Visual);
}

void UIMenuItem::OnMouseEnter() {
    SetHovered(true);
}

void UIMenuItem::OnMouseLeave() {
    SetHovered(false);
}

void UIMenuItem::OnMouseButton(uint8_t button, bool pressed, const Vector2&) {
    // button 1 = 左键
    if (button != 1 || !IsEnabled() || IsSeparator()) {
        return;
    }
    SetPressed(pressed);
}

void UIMenuItem::OnMouseClick(uint8_t button, const Vector2&) {
    // button 1 = 左键
    if (button != 1 || !IsEnabled() || IsSeparator()) {
        return;
    }
    TriggerClick();
}

void UIMenuItem::OnFocusLost() {
    SetPressed(false);
}

void UIMenuItem::Click() {
    if (!IsEnabled() || IsSeparator()) {
        return;
    }
    TriggerClick();
}

void UIMenuItem::SetPressed(bool pressed) {
    if (m_pressed == pressed) {
        return;
    }
    m_pressed = pressed;
    MarkDirty(UIWidgetDirtyFlag::Visual);
}

void UIMenuItem::SetHovered(bool hovered) {
    if (m_hovered == hovered) {
        return;
    }
    m_hovered = hovered;
    MarkDirty(UIWidgetDirtyFlag::Visual);
}

void UIMenuItem::TriggerClick() {
    // 如果是可选中项，切换选中状态
    if (m_type == UIMenuItemType::Checkable) {
        SetChecked(!m_checked);
    }
    
    // 触发点击回调
    if (m_onClicked) {
        m_onClicked(*this);
    }
    
    // 如果有子菜单，打开子菜单
    if (m_subMenu) {
        m_subMenu->Open();
    }
}

} // namespace Render::UI

