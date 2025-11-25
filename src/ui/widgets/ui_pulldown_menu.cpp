#include "render/ui/widgets/ui_pulldown_menu.h"

namespace Render::UI {

UIPullDownMenu::UIPullDownMenu(std::string id)
    : UIWidget(std::move(id)) {
    
    // 创建触发按钮
    auto button = std::make_unique<UIButton>(GetId() + "_button");
    m_button = button.get();
    
    // 绑定按钮点击事件
    m_button->SetOnClicked([this](UIButton&) {
        OnButtonClicked();
    });
    
    // 设置布局：垂直布局，按钮在上，菜单在下
    SetLayoutDirection(UILayoutDirection::Vertical);
    SetLayoutMode(UILayoutMode::Flex);
    SetJustifyContent(UIFlexJustifyContent::FlexStart);
    SetAlignItems(UIFlexAlignItems::FlexStart);
    SetSpacing(2.0f);
    
    AddChild(std::move(button));
}

void UIPullDownMenu::SetLabel(const std::string& label) {
    if (m_button) {
        m_button->SetLabel(label);
    }
}

const std::string& UIPullDownMenu::GetLabel() const {
    static const std::string empty;
    return m_button ? m_button->GetLabel() : empty;
}

void UIPullDownMenu::SetIcon(const std::string& /*iconPath*/) {
    // 图标设置可以在未来扩展 UIButton 支持图标时实现
    // 暂时保留接口
}

const std::string& UIPullDownMenu::GetIcon() const {
    static const std::string empty;
    return empty;
}

void UIPullDownMenu::SetMenu(std::shared_ptr<UIMenu> menu) {
    // 如果已有菜单，先移除
    if (m_menu) {
        auto menuWidget = FindById(m_menu->GetId());
        if (menuWidget) {
            RemoveChild(m_menu->GetId());
        }
    }
    
    m_menu = std::move(menu);
    
    if (m_menu) {
        // 将菜单作为子节点添加（默认隐藏）
        m_menu->SetVisibility(UIVisibility::Hidden);
        // 注意：菜单通过 shared_ptr 管理，这里不添加到Widget树，而是单独管理
        // AddChild 需要 unique_ptr，所以菜单的显示需要特殊处理
    }
}

void UIPullDownMenu::OpenMenu() {
    if (!m_menu || IsMenuOpen()) {
        return;
    }
    
    // 更新菜单位置
    UpdateMenuPosition();
    
    // 打开菜单
    m_menu->Open();
    
    if (m_onMenuOpened) {
        m_onMenuOpened(*this);
    }
}

void UIPullDownMenu::CloseMenu() {
    if (!m_menu || !IsMenuOpen()) {
        return;
    }
    
    m_menu->Close();
    
    if (m_onMenuClosed) {
        m_onMenuClosed(*this);
    }
}

bool UIPullDownMenu::IsMenuOpen() const {
    return m_menu && m_menu->IsOpen();
}

void UIPullDownMenu::OnChildAdded(UIWidget& child) {
    UIWidget::OnChildAdded(child);
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIPullDownMenu::OnButtonClicked() {
    if (IsMenuOpen()) {
        CloseMenu();
    } else {
        OpenMenu();
    }
}

void UIPullDownMenu::UpdateMenuPosition() {
    if (!m_menu || !m_button) {
        return;
    }
    
    const Rect menuRect = CalculateMenuRect();
    m_menu->SetLayoutRect(menuRect);
}

Rect UIPullDownMenu::CalculateMenuRect() {
    if (!m_button) {
        return Rect{};
    }
    
    const Rect buttonRect = m_button->GetLayoutRect();
    const Vector2 menuSize = m_menu->GetPreferredSize();
    
    UIMenuPlacement placement = m_placement;
    if (placement == UIMenuPlacement::Auto) {
        placement = DetermineAutoPlacement();
    }
    
    Rect menuRect;
    
    switch (placement) {
        case UIMenuPlacement::Below:
            menuRect.x = buttonRect.x;
            menuRect.y = buttonRect.y + buttonRect.height + GetSpacing();
            menuRect.width = std::max(menuSize.x(), buttonRect.width);
            menuRect.height = menuSize.y();
            break;
            
        case UIMenuPlacement::Above:
            menuRect.x = buttonRect.x;
            menuRect.y = buttonRect.y - menuSize.y() - GetSpacing();
            menuRect.width = std::max(menuSize.x(), buttonRect.width);
            menuRect.height = menuSize.y();
            break;
            
        case UIMenuPlacement::Left:
            menuRect.x = buttonRect.x - menuSize.x() - GetSpacing();
            menuRect.y = buttonRect.y;
            menuRect.width = menuSize.x();
            menuRect.height = menuSize.y();
            break;
            
        case UIMenuPlacement::Right:
            menuRect.x = buttonRect.x + buttonRect.width + GetSpacing();
            menuRect.y = buttonRect.y;
            menuRect.width = menuSize.x();
            menuRect.height = menuSize.y();
            break;
            
        default:
            // 默认在下方
            menuRect.x = buttonRect.x;
            menuRect.y = buttonRect.y + buttonRect.height + GetSpacing();
            menuRect.width = std::max(menuSize.x(), buttonRect.width);
            menuRect.height = menuSize.y();
            break;
    }
    
    return menuRect;
}

UIMenuPlacement UIPullDownMenu::DetermineAutoPlacement() {
    // 简单实现：默认在下方
    // 未来可以根据可用空间智能选择位置
    // 例如：检查父容器边界，如果下方空间不足则显示在上方
    
    return UIMenuPlacement::Below;
}

} // namespace Render::UI

