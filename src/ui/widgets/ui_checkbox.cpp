#include "render/ui/widgets/ui_checkbox.h"

#include "render/logger.h"

namespace Render::UI {

UICheckBox::UICheckBox(std::string id)
    : UIWidget(std::move(id)) {
    // 设置默认尺寸
    SetPreferredSize(Vector2(200.0f, 24.0f));
    SetMinSize(Vector2(24.0f, 24.0f));
}

void UICheckBox::SetLabel(std::string label) {
    if (m_label == label) {
        return;
    }
    m_label = std::move(label);
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UICheckBox::SetChecked(bool checked) {
    State newState = checked ? State::Checked : State::Unchecked;
    if (m_state == newState) {
        return;
    }
    m_state = newState;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UICheckBox::SetState(State state) {
    if (m_state == state) {
        return;
    }
    m_state = state;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UICheckBox::SetTristate(bool tristate) noexcept {
    if (m_tristate == tristate) {
        return;
    }
    m_tristate = tristate;
    // 如果不支持三态且当前是不确定状态，则切换到未选中
    if (!m_tristate && m_state == State::Indeterminate) {
        m_state = State::Unchecked;
    }
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UICheckBox::SetLabelPosition(bool labelOnLeft) noexcept {
    if (m_labelOnLeft == labelOnLeft) {
        return;
    }
    m_labelOnLeft = labelOnLeft;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UICheckBox::OnMouseEnter() {
    SetHovered(true);
}

void UICheckBox::OnMouseLeave() {
    SetHovered(false);
}

void UICheckBox::OnMouseClick(uint8_t button, const Vector2&) {
    if (button != SDL_BUTTON_LEFT || !IsEnabled()) {
        return;
    }
    ToggleState();
}

void UICheckBox::OnFocusLost() {
    SetHovered(false);
}

void UICheckBox::SetHovered(bool hovered) {
    if (m_hovered == hovered) {
        return;
    }
    m_hovered = hovered;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UICheckBox::ToggleState() {
    State oldState = m_state;
    
    if (m_tristate) {
        // 三态循环：未选中 -> 选中 -> 不确定 -> 未选中
        switch (m_state) {
        case State::Unchecked:
            m_state = State::Checked;
            break;
        case State::Checked:
            m_state = State::Indeterminate;
            break;
        case State::Indeterminate:
            m_state = State::Unchecked;
            break;
        }
    } else {
        // 两态切换：未选中 <-> 选中
        m_state = (m_state == State::Checked) ? State::Unchecked : State::Checked;
    }
    
    if (m_state != oldState) {
        MarkDirty(UIWidgetDirtyFlag::Layout);
        
        // 调用回调
        if (m_onChanged) {
            m_onChanged(*this, m_state == State::Checked);
        }
    }
}

} // namespace Render::UI

