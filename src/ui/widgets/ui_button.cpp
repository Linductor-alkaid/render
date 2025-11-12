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


