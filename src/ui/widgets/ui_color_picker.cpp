#include "render/ui/widgets/ui_color_picker.h"

#include <algorithm>
#include <cmath>

#include "render/logger.h"

namespace Render::UI {

UIColorPicker::UIColorPicker(std::string id)
    : UIWidget(std::move(id)) {
    // 设置默认尺寸（颜色选择器通常比较宽，用于显示色块和滑块）
    SetPreferredSize(Vector2(300.0f, 200.0f));
    SetMinSize(Vector2(200.0f, 150.0f));
}

void UIColorPicker::SetColor(const Color& color) {
    if (std::abs(m_color.r - color.r) < 0.001f &&
        std::abs(m_color.g - color.g) < 0.001f &&
        std::abs(m_color.b - color.b) < 0.001f &&
        std::abs(m_color.a - color.a) < 0.001f) {
        return;
    }
    
    m_color = color;
    // 限制颜色值在有效范围内
    m_color.r = std::clamp(m_color.r, 0.0f, 1.0f);
    m_color.g = std::clamp(m_color.g, 0.0f, 1.0f);
    m_color.b = std::clamp(m_color.b, 0.0f, 1.0f);
    m_color.a = std::clamp(m_color.a, 0.0f, 1.0f);
    
    MarkDirty(UIWidgetDirtyFlag::Layout);
    NotifyColorChanged();
}

void UIColorPicker::SetRGB(float r, float g, float b) {
    SetRGBA(r, g, b, m_color.a);
}

void UIColorPicker::SetRGBA(float r, float g, float b, float a) {
    Color newColor(
        std::clamp(r, 0.0f, 1.0f),
        std::clamp(g, 0.0f, 1.0f),
        std::clamp(b, 0.0f, 1.0f),
        std::clamp(a, 0.0f, 1.0f)
    );
    SetColor(newColor);
}

void UIColorPicker::SetShowAlpha(bool showAlpha) noexcept {
    if (m_showAlpha == showAlpha) {
        return;
    }
    m_showAlpha = showAlpha;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIColorPicker::SetShowPreview(bool showPreview) noexcept {
    if (m_showPreview == showPreview) {
        return;
    }
    m_showPreview = showPreview;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIColorPicker::OnMouseEnter() {
    SetHovered(true);
}

void UIColorPicker::OnMouseLeave() {
    SetHovered(false);
}

void UIColorPicker::OnMouseClick(uint8_t button, const Vector2& position) {
    (void)button;
    (void)position;
    // 颜色选择器的交互将在后续版本中实现（点击打开颜色选择面板）
    // 当前版本只显示颜色预览
}

void UIColorPicker::OnFocusLost() {
    SetHovered(false);
}

void UIColorPicker::SetHovered(bool hovered) {
    if (m_hovered == hovered) {
        return;
    }
    m_hovered = hovered;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIColorPicker::NotifyColorChanged() {
    if (m_onChanged) {
        m_onChanged(*this, m_color);
    }
}

} // namespace Render::UI

