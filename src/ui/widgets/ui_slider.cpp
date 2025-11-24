#include "render/ui/widgets/ui_slider.h"

#include <algorithm>
#include <cmath>

#include "render/logger.h"

namespace Render::UI {

UISlider::UISlider(std::string id)
    : UIWidget(std::move(id)) {
    // 设置默认尺寸
    SetPreferredSize(Vector2(200.0f, 24.0f));
    SetMinSize(Vector2(100.0f, 24.0f));
}

void UISlider::SetLabel(std::string label) {
    if (m_label == label) {
        return;
    }
    m_label = std::move(label);
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UISlider::SetValue(float value) {
    float clampedValue = ClampValue(value);
    clampedValue = SnapValue(clampedValue);
    
    if (std::abs(m_value - clampedValue) < 0.0001f) {
        return;
    }
    
    m_value = clampedValue;
    MarkDirty(UIWidgetDirtyFlag::Layout);
    
    if (m_onChanged) {
        m_onChanged(*this, m_value);
    }
}

void UISlider::SetMinValue(float minValue) noexcept {
    if (m_minValue == minValue) {
        return;
    }
    m_minValue = minValue;
    if (m_minValue > m_maxValue) {
        m_maxValue = m_minValue;
    }
    SetValue(m_value); // 重新限制当前值
}

void UISlider::SetMaxValue(float maxValue) noexcept {
    if (m_maxValue == maxValue) {
        return;
    }
    m_maxValue = maxValue;
    if (m_maxValue < m_minValue) {
        m_minValue = m_maxValue;
    }
    SetValue(m_value); // 重新限制当前值
}

void UISlider::SetStep(float step) noexcept {
    if (m_step == step) {
        return;
    }
    m_step = std::max(0.0f, step);
    SetValue(m_value); // 重新对齐到步长
}

void UISlider::SetOrientation(Orientation orientation) noexcept {
    if (m_orientation == orientation) {
        return;
    }
    m_orientation = orientation;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UISlider::SetShowValue(bool showValue) noexcept {
    if (m_showValue == showValue) {
        return;
    }
    m_showValue = showValue;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UISlider::OnMouseEnter() {
    SetHovered(true);
}

void UISlider::OnMouseLeave() {
    SetHovered(false);
    // 注意：不要在这里结束拖拽，因为鼠标可能移出控件但仍在拖拽
    // 拖拽应该在鼠标按钮释放时结束
}

void UISlider::OnMouseMove(const Vector2& position, const Vector2& delta) {
    (void)delta; // 未使用
    // 如果正在拖拽，更新值
    if (m_dragging) {
        UpdateValueFromPosition(position);
    }
}

void UISlider::OnMouseButton(uint8_t button, bool pressed, const Vector2& position) {
    if (button != SDL_BUTTON_LEFT || !IsEnabled()) {
        return;
    }
    
    if (pressed) {
        // 开始拖拽
        SetDragging(true);
        m_dragStartPosition = position;
        m_dragStartValue = m_value;
        UpdateValueFromPosition(position);
    } else {
        // 结束拖拽
        if (m_dragging) {
            SetDragging(false);
        }
    }
}

void UISlider::OnMouseClick(uint8_t button, const Vector2& position) {
    if (button != SDL_BUTTON_LEFT || !IsEnabled()) {
        return;
    }
    
    // 如果不在拖拽状态，直接更新值（点击跳转）
    if (!m_dragging) {
        UpdateValueFromPosition(position);
    }
}

void UISlider::OnFocusLost() {
    SetHovered(false);
    if (m_dragging) {
        SetDragging(false);
    }
}

void UISlider::SetHovered(bool hovered) {
    if (m_hovered == hovered) {
        return;
    }
    m_hovered = hovered;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UISlider::SetDragging(bool dragging) {
    if (m_dragging == dragging) {
        return;
    }
    m_dragging = dragging;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UISlider::UpdateValueFromPosition(const Vector2& position) {
    const Rect& rect = GetLayoutRect();
    if (rect.width <= 0.0f || rect.height <= 0.0f) {
        return;
    }
    
    float normalizedValue = 0.0f;
    
    if (m_orientation == Orientation::Horizontal) {
        // 水平滑块：从左到右
        float localX = position.x() - rect.x;
        normalizedValue = std::clamp(localX / rect.width, 0.0f, 1.0f);
    } else {
        // 垂直滑块：从下到上（Y轴向下为正）
        float localY = position.y() - rect.y;
        normalizedValue = 1.0f - std::clamp(localY / rect.height, 0.0f, 1.0f);
    }
    
    // 将归一化值转换为实际值
    float range = m_maxValue - m_minValue;
    float newValue = m_minValue + normalizedValue * range;
    
    SetValue(newValue);
}

float UISlider::ClampValue(float value) const {
    return std::clamp(value, m_minValue, m_maxValue);
}

float UISlider::SnapValue(float value) const {
    if (m_step <= 0.0f) {
        return value;
    }
    
    // 对齐到步长
    float steps = std::round((value - m_minValue) / m_step);
    return m_minValue + steps * m_step;
}

float UISlider::GetNormalizedValue() const {
    float range = m_maxValue - m_minValue;
    if (range <= 0.0f) {
        return 0.0f;
    }
    return (m_value - m_minValue) / range;
}

} // namespace Render::UI

