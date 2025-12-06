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

#include <memory>
#include <string>

#include "render/ui/ui_widget.h"
#include "render/ui/widgets/ui_menu.h"
#include "render/ui/widgets/ui_button.h"

namespace Render::UI {

/**
 * @brief 菜单定位方式
 */
enum class UIMenuPlacement : uint8_t {
    Below,      // 在触发器下方
    Above,      // 在触发器上方
    Left,       // 在触发器左侧
    Right,      // 在触发器右侧
    Auto        // 自动选择（根据空间）
};

/**
 * @brief 下拉菜单类
 * 
 * 从按钮触发的下拉菜单，支持多种定位方式和自动布局。
 * 参考 Blender 的 UI_WTYPE_PULLDOWN 实现。
 */
class UIPullDownMenu : public UIWidget {
public:
    explicit UIPullDownMenu(std::string id);
    ~UIPullDownMenu() override = default;

    // 触发按钮
    void SetLabel(const std::string& label);
    [[nodiscard]] const std::string& GetLabel() const;

    void SetIcon(const std::string& iconPath);
    [[nodiscard]] const std::string& GetIcon() const;

    [[nodiscard]] UIButton* GetButton() const noexcept { return m_button; }

    // 菜单内容
    void SetMenu(std::shared_ptr<UIMenu> menu);
    [[nodiscard]] std::shared_ptr<UIMenu> GetMenu() const noexcept { return m_menu; }

    // 定位设置
    void SetPlacement(UIMenuPlacement placement) { m_placement = placement; }
    [[nodiscard]] UIMenuPlacement GetPlacement() const noexcept { return m_placement; }

    // 菜单状态
    void OpenMenu();
    void CloseMenu();
    [[nodiscard]] bool IsMenuOpen() const;

    // 回调
    void SetOnMenuOpened(std::function<void(UIPullDownMenu&)> handler) { m_onMenuOpened = std::move(handler); }
    void SetOnMenuClosed(std::function<void(UIPullDownMenu&)> handler) { m_onMenuClosed = std::move(handler); }

protected:
    void OnChildAdded(UIWidget& child) override;

private:
    void OnButtonClicked();
    void UpdateMenuPosition();
    Rect CalculateMenuRect();
    UIMenuPlacement DetermineAutoPlacement();

    UIButton* m_button = nullptr;
    std::shared_ptr<UIMenu> m_menu;
    UIMenuPlacement m_placement = UIMenuPlacement::Below;

    std::function<void(UIPullDownMenu&)> m_onMenuOpened;
    std::function<void(UIPullDownMenu&)> m_onMenuClosed;
};

} // namespace Render::UI

