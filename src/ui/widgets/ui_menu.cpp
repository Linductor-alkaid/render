#include "render/ui/widgets/ui_menu.h"
#include <SDL3/SDL_scancode.h>
#include <algorithm>

namespace Render::UI {

UIMenu::UIMenu(std::string id)
    : UIWidget(std::move(id)) {
    
    // 菜单默认是垂直布局
    SetLayoutDirection(UILayoutDirection::Vertical);
    SetLayoutMode(UILayoutMode::Flex);
    SetJustifyContent(UIFlexJustifyContent::FlexStart);
    SetAlignItems(UIFlexAlignItems::Stretch);
    SetSpacing(0.0f);
    
    // 菜单默认隐藏
    SetVisibility(UIVisibility::Hidden);
}

UIMenuItem* UIMenu::AddMenuItem(std::string id, const std::string& label) {
    auto menuItem = std::make_unique<UIMenuItem>(std::move(id), UIMenuItemType::Normal);
    menuItem->SetLabel(label);
    
    UIMenuItem* ptr = menuItem.get();
    AddChild(std::move(menuItem));
    m_menuItems.push_back(ptr);
    
    UpdateLayout();
    return ptr;
}

UIMenuItem* UIMenu::AddCheckableItem(std::string id, const std::string& label, bool checked) {
    auto menuItem = std::make_unique<UIMenuItem>(std::move(id), UIMenuItemType::Checkable);
    menuItem->SetLabel(label);
    menuItem->SetChecked(checked);
    
    UIMenuItem* ptr = menuItem.get();
    AddChild(std::move(menuItem));
    m_menuItems.push_back(ptr);
    
    UpdateLayout();
    return ptr;
}

UIMenuItem* UIMenu::AddSeparator(std::string id) {
    if (id.empty()) {
        // 生成唯一ID
        static int separatorCount = 0;
        id = "separator_" + std::to_string(separatorCount++);
    }
    
    auto separator = std::make_unique<UIMenuItem>(std::move(id), UIMenuItemType::Separator);
    
    UIMenuItem* ptr = separator.get();
    AddChild(std::move(separator));
    m_menuItems.push_back(ptr);
    
    UpdateLayout();
    return ptr;
}

UIMenuItem* UIMenu::AddSubMenuItem(std::string id, const std::string& label, std::shared_ptr<UIMenu> subMenu) {
    auto menuItem = std::make_unique<UIMenuItem>(std::move(id), UIMenuItemType::SubMenu);
    menuItem->SetLabel(label);
    menuItem->SetSubMenu(std::move(subMenu));
    
    UIMenuItem* ptr = menuItem.get();
    AddChild(std::move(menuItem));
    m_menuItems.push_back(ptr);
    
    UpdateLayout();
    return ptr;
}

void UIMenu::RemoveMenuItem(const std::string& id) {
    auto it = std::find_if(m_menuItems.begin(), m_menuItems.end(),
        [&id](const UIMenuItem* item) { return item->GetId() == id; });
    
    if (it != m_menuItems.end()) {
        m_menuItems.erase(it);
        RemoveChild(id);
        UpdateLayout();
    }
}

void UIMenu::ClearMenuItems() {
    m_menuItems.clear();
    
    // 移除所有子节点
    std::vector<std::string> childIds;
    ForEachChild([&childIds](const UIWidget& child) {
        childIds.push_back(child.GetId());
    });
    
    for (const auto& id : childIds) {
        RemoveChild(id);
    }
    
    UpdateLayout();
}

UIMenuItem* UIMenu::GetMenuItem(const std::string& id) const {
    auto it = std::find_if(m_menuItems.begin(), m_menuItems.end(),
        [&id](const UIMenuItem* item) { return item->GetId() == id; });
    
    return it != m_menuItems.end() ? *it : nullptr;
}

void UIMenu::Open() {
    if (m_isOpen) {
        return;
    }
    
    m_isOpen = true;
    SetVisibility(UIVisibility::Visible);
    UpdateLayout();
    UpdateScrollState();
    
    if (m_onOpened) {
        m_onOpened(*this);
    }
}

void UIMenu::Close() {
    if (!m_isOpen) {
        return;
    }
    
    m_isOpen = false;
    SetVisibility(UIVisibility::Hidden);
    m_selectedIndex = -1;
    
    // 关闭所有子菜单
    for (auto* item : m_menuItems) {
        if (item->HasSubMenu()) {
            item->GetSubMenu()->Close();
        }
    }
    
    if (m_onClosed) {
        m_onClosed(*this);
    }
}

void UIMenu::SetScrollOffset(float offset) {
    if (!m_needsScroll) {
        m_scrollOffset = 0.0f;
        return;
    }
    
    // 计算最大滚动偏移
    float totalHeight = 0.0f;
    for (const auto* item : m_menuItems) {
        totalHeight += item->GetPreferredSize().y();
    }
    
    float maxOffset = std::max(0.0f, totalHeight - m_maxHeight);
    m_scrollOffset = std::clamp(offset, 0.0f, maxOffset);
    
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIMenu::OnMouseWheel(const Vector2& offset) {
    if (m_needsScroll) {
        SetScrollOffset(m_scrollOffset - offset.y() * 20.0f);
    }
}

void UIMenu::OnKey(int scancode, bool pressed, bool repeat) {
    if (!pressed) {
        return;
    }
    
    HandleKeyboardNavigation(scancode);
}

void UIMenu::UpdateLayout() {
    // 计算菜单的首选尺寸
    float maxWidth = m_minWidth;
    float totalHeight = 0.0f;
    
    for (const auto* item : m_menuItems) {
        const Vector2 itemSize = item->GetPreferredSize();
        maxWidth = std::max(maxWidth, itemSize.x());
        totalHeight += itemSize.y();
    }
    
    // 添加内边距和间距
    totalHeight += GetSpacing() * static_cast<float>(m_menuItems.size() - 1);
    
    SetPreferredSize({maxWidth, std::min(totalHeight, m_maxHeight)});
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIMenu::UpdateScrollState() {
    float totalHeight = 0.0f;
    for (const auto* item : m_menuItems) {
        totalHeight += item->GetPreferredSize().y();
    }
    
    m_needsScroll = totalHeight > m_maxHeight;
    
    if (!m_needsScroll) {
        m_scrollOffset = 0.0f;
    }
}

void UIMenu::HandleKeyboardNavigation(int scancode) {
    if (m_menuItems.empty()) {
        return;
    }
    
    switch (scancode) {
        case SDL_SCANCODE_UP:
            // 向上导航
            do {
                m_selectedIndex = (m_selectedIndex - 1 + static_cast<int>(m_menuItems.size())) 
                                % static_cast<int>(m_menuItems.size());
            } while (m_menuItems[m_selectedIndex]->IsSeparator());
            break;
            
        case SDL_SCANCODE_DOWN:
            // 向下导航
            do {
                m_selectedIndex = (m_selectedIndex + 1) % static_cast<int>(m_menuItems.size());
            } while (m_menuItems[m_selectedIndex]->IsSeparator());
            break;
            
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_KP_ENTER:
            // 激活当前选中的菜单项
            if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_menuItems.size())) {
                auto* item = m_menuItems[m_selectedIndex];
                // 使用公开的 Click 方法
                item->Click();
            }
            break;
            
        case SDL_SCANCODE_RIGHT:
            // 打开子菜单
            if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_menuItems.size())) {
                auto* item = m_menuItems[m_selectedIndex];
                if (item->HasSubMenu()) {
                    item->GetSubMenu()->Open();
                }
            }
            break;
            
        case SDL_SCANCODE_LEFT:
        case SDL_SCANCODE_ESCAPE:
            // 关闭菜单
            Close();
            break;
            
        default:
            break;
    }
}

} // namespace Render::UI

