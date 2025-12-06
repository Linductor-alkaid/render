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
#include <memory>
#include <string>
#include <vector>

#include "render/ui/ui_widget.h"
#include "render/ui/widgets/ui_menu_item.h"

namespace Render::UI {

/**
 * @brief 菜单类
 * 
 * 表示一个菜单容器，包含多个菜单项。
 * 支持嵌套子菜单、分隔符、快捷键显示等功能。
 * 参考 Blender 的菜单系统实现。
 */
class UIMenu : public UIWidget {
public:
    explicit UIMenu(std::string id);
    ~UIMenu() override = default;

    // 菜单项管理
    UIMenuItem* AddMenuItem(std::string id, const std::string& label);
    UIMenuItem* AddCheckableItem(std::string id, const std::string& label, bool checked = false);
    UIMenuItem* AddSeparator(std::string id = "");
    UIMenuItem* AddSubMenuItem(std::string id, const std::string& label, std::shared_ptr<UIMenu> subMenu);
    
    void RemoveMenuItem(const std::string& id);
    void ClearMenuItems();

    [[nodiscard]] UIMenuItem* GetMenuItem(const std::string& id) const;
    [[nodiscard]] const std::vector<UIMenuItem*>& GetMenuItems() const noexcept { return m_menuItems; }

    // 菜单显示控制
    void Open();
    void Close();
    [[nodiscard]] bool IsOpen() const noexcept { return m_isOpen; }

    // 菜单尺寸
    void SetMinWidth(float width) { m_minWidth = width; }
    [[nodiscard]] float GetMinWidth() const noexcept { return m_minWidth; }

    void SetMaxHeight(float height) { m_maxHeight = height; }
    [[nodiscard]] float GetMaxHeight() const noexcept { return m_maxHeight; }

    // 滚动支持
    [[nodiscard]] bool NeedsScroll() const noexcept { return m_needsScroll; }
    void SetScrollOffset(float offset);
    [[nodiscard]] float GetScrollOffset() const noexcept { return m_scrollOffset; }

    // 回调
    void SetOnOpened(std::function<void(UIMenu&)> handler) { m_onOpened = std::move(handler); }
    void SetOnClosed(std::function<void(UIMenu&)> handler) { m_onClosed = std::move(handler); }

protected:
    void OnMouseWheel(const Vector2& offset) override;
    void OnKey(int scancode, bool pressed, bool repeat) override;

private:
    void UpdateLayout();
    void UpdateScrollState();
    void HandleKeyboardNavigation(int scancode);

    std::vector<UIMenuItem*> m_menuItems;
    bool m_isOpen = false;
    float m_minWidth = 120.0f;
    float m_maxHeight = 400.0f;
    float m_scrollOffset = 0.0f;
    bool m_needsScroll = false;
    int m_selectedIndex = -1;

    std::function<void(UIMenu&)> m_onOpened;
    std::function<void(UIMenu&)> m_onClosed;
};

} // namespace Render::UI

