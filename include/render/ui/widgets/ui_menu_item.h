#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "render/ui/ui_widget.h"

namespace Render::UI {

class UIMenu;

/**
 * @brief 菜单项类型
 */
enum class UIMenuItemType : uint8_t {
    Normal,      // 普通菜单项
    Checkable,   // 可选中菜单项
    Separator,   // 分隔符
    SubMenu      // 子菜单
};

/**
 * @brief 菜单项类
 * 
 * 表示菜单中的单个项目，可以是普通项、可选中项、分隔符或子菜单。
 * 参考 Blender 的 UI_WTYPE_MENU_ITEM 实现。
 */
class UIMenuItem : public UIWidget {
public:
    using ClickHandler = std::function<void(UIMenuItem&)>;

    explicit UIMenuItem(std::string id, UIMenuItemType type = UIMenuItemType::Normal);
    ~UIMenuItem() override = default;

    // 基本属性
    void SetLabel(std::string label);
    [[nodiscard]] const std::string& GetLabel() const noexcept { return m_label; }

    void SetIcon(std::string iconPath);
    [[nodiscard]] const std::string& GetIcon() const noexcept { return m_iconPath; }

    void SetShortcut(std::string shortcut);
    [[nodiscard]] const std::string& GetShortcut() const noexcept { return m_shortcut; }

    void SetTooltip(std::string tooltip);
    [[nodiscard]] const std::string& GetTooltip() const noexcept { return m_tooltip; }

    // 类型和状态
    [[nodiscard]] UIMenuItemType GetType() const noexcept { return m_type; }
    
    void SetCheckable(bool checkable);
    [[nodiscard]] bool IsCheckable() const noexcept { return m_type == UIMenuItemType::Checkable; }
    
    void SetChecked(bool checked);
    [[nodiscard]] bool IsChecked() const noexcept { return m_checked; }

    void SetSeparator(bool separator);
    [[nodiscard]] bool IsSeparator() const noexcept { return m_type == UIMenuItemType::Separator; }

    // 子菜单
    void SetSubMenu(std::shared_ptr<UIMenu> subMenu);
    [[nodiscard]] std::shared_ptr<UIMenu> GetSubMenu() const noexcept { return m_subMenu; }
    [[nodiscard]] bool HasSubMenu() const noexcept { return m_subMenu != nullptr; }

    // 交互状态
    [[nodiscard]] bool IsHovered() const noexcept { return m_hovered; }
    [[nodiscard]] bool IsPressed() const noexcept { return m_pressed; }

    // 回调
    void SetOnClicked(ClickHandler handler) { m_onClicked = std::move(handler); }
    void SetOnCheckChanged(std::function<void(UIMenuItem&, bool)> handler) { m_onCheckChanged = std::move(handler); }

    // 程序化触发点击（用于键盘导航等）
    void Click();

protected:
    void OnMouseEnter() override;
    void OnMouseLeave() override;
    void OnMouseButton(uint8_t button, bool pressed, const Vector2& position) override;
    void OnMouseClick(uint8_t button, const Vector2& position) override;
    void OnFocusLost() override;

private:
    void SetPressed(bool pressed);
    void SetHovered(bool hovered);
    void TriggerClick();

    UIMenuItemType m_type;
    std::string m_label;
    std::string m_iconPath;
    std::string m_shortcut;
    std::string m_tooltip;
    bool m_checked = false;
    bool m_hovered = false;
    bool m_pressed = false;
    
    std::shared_ptr<UIMenu> m_subMenu;
    ClickHandler m_onClicked;
    std::function<void(UIMenuItem&, bool)> m_onCheckChanged;
};

} // namespace Render::UI

