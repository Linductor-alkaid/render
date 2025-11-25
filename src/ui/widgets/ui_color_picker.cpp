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

void UIColorPicker::SetColorMode(ColorMode mode) noexcept {
    if (m_colorMode == mode) {
        return;
    }
    m_colorMode = mode;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIColorPicker::OnMouseEnter() {
    SetHovered(true);
}

void UIColorPicker::OnMouseLeave() {
    SetHovered(false);
    // 注意：不要在这里结束拖拽，因为鼠标可能移出控件但仍在拖拽
}

void UIColorPicker::OnMouseMove(const Vector2& position, const Vector2& delta) {
    (void)delta; // 未使用
    
    // 如果正在拖拽，更新颜色值
    if (m_dragging) {
        UpdateColorFromPosition(position);
    }
}

void UIColorPicker::OnMouseButton(uint8_t button, bool pressed, const Vector2& position) {
    if (button != SDL_BUTTON_LEFT || !IsEnabled()) {
        return;
    }
    
    if (pressed) {
        // 开始拖拽
        m_dragZone = GetZoneAtPosition(position);
        if (m_dragZone != InteractionZone::None && m_dragZone != InteractionZone::Preview) {
            SetDragging(true);
            m_dragStartPosition = position;
            m_dragStartColor = m_color;
            UpdateColorFromPosition(position);
        }
    } else {
        // 结束拖拽
        if (m_dragging) {
            SetDragging(false);
            m_dragZone = InteractionZone::None;
        }
    }
}

void UIColorPicker::OnMouseClick(uint8_t button, const Vector2& position) {
    if (button != SDL_BUTTON_LEFT || !IsEnabled()) {
        return;
    }
    
    // 如果点击预览区域，可以在此处添加打开颜色选择面板的逻辑
    InteractionZone zone = GetZoneAtPosition(position);
    if (zone == InteractionZone::Preview) {
        // 未来可以在此处打开颜色选择面板
        Logger::GetInstance().Info("[ColorPicker] Preview area clicked");
    }
}

void UIColorPicker::OnFocusLost() {
    SetHovered(false);
    if (m_dragging) {
        SetDragging(false);
        m_dragZone = InteractionZone::None;
    }
}

void UIColorPicker::SetHovered(bool hovered) {
    if (m_hovered == hovered) {
        return;
    }
    m_hovered = hovered;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIColorPicker::SetDragging(bool dragging) {
    if (m_dragging == dragging) {
        return;
    }
    m_dragging = dragging;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIColorPicker::NotifyColorChanged() {
    if (m_onChanged) {
        m_onChanged(*this, m_color);
    }
}

UIColorPicker::InteractionZone UIColorPicker::GetZoneAtPosition(const Vector2& position) const {
    const Rect& rect = GetLayoutRect();
    
    // 计算各区域的布局（参考渲染器中的布局）
    const float padding = 8.0f;
    const float previewSize = rect.height - padding * 2.0f;
    const float textStartX = m_showPreview ? (previewSize + padding * 2.0f) : padding;
    
    // 检查是否在预览区域
    if (m_showPreview) {
        const float previewX = rect.x + padding;
        const float previewY = rect.y + padding;
        if (position.x() >= previewX && position.x() <= previewX + previewSize &&
            position.y() >= previewY && position.y() <= previewY + previewSize) {
            return InteractionZone::Preview;
        }
    }
    
    // 检查是否在文本/滑块区域
    // 将文本区域划分为多个通道区域（每个通道占一定高度）
    const float sliderStartX = rect.x + textStartX;
    const float sliderWidth = rect.width - textStartX - padding;
    const float channelCount = m_showAlpha ? 4.0f : 3.0f;
    const float channelHeight = rect.height / channelCount;
    
    if (position.x() >= sliderStartX && position.x() <= sliderStartX + sliderWidth) {
        float relativeY = position.y() - rect.y;
        
        if (relativeY >= 0 && relativeY < channelHeight) {
            return InteractionZone::RedChannel;
        } else if (relativeY >= channelHeight && relativeY < channelHeight * 2.0f) {
            return InteractionZone::GreenChannel;
        } else if (relativeY >= channelHeight * 2.0f && relativeY < channelHeight * 3.0f) {
            return InteractionZone::BlueChannel;
        } else if (m_showAlpha && relativeY >= channelHeight * 3.0f && relativeY < channelHeight * 4.0f) {
            return InteractionZone::AlphaChannel;
        }
    }
    
    return InteractionZone::None;
}

void UIColorPicker::UpdateColorFromPosition(const Vector2& position) {
    if (m_dragZone == InteractionZone::None || m_dragZone == InteractionZone::Preview) {
        return;
    }
    
    const Rect& rect = GetLayoutRect();
    const float padding = 8.0f;
    const float previewSize = rect.height - padding * 2.0f;
    const float textStartX = m_showPreview ? (previewSize + padding * 2.0f) : padding;
    const float sliderStartX = rect.x + textStartX;
    const float sliderWidth = rect.width - textStartX - padding;
    
    if (sliderWidth <= 0.0f) {
        return;
    }
    
    // 计算归一化值（0.0 - 1.0）
    float localX = position.x() - sliderStartX;
    float normalizedValue = std::clamp(localX / sliderWidth, 0.0f, 1.0f);
    
    UpdateChannelValue(m_dragZone, normalizedValue);
}

void UIColorPicker::UpdateChannelValue(InteractionZone zone, float normalizedValue) {
    Color newColor = m_color;
    
    switch (zone) {
        case InteractionZone::RedChannel:
            newColor.r = normalizedValue;
            break;
        case InteractionZone::GreenChannel:
            newColor.g = normalizedValue;
            break;
        case InteractionZone::BlueChannel:
            newColor.b = normalizedValue;
            break;
        case InteractionZone::AlphaChannel:
            newColor.a = normalizedValue;
            break;
        default:
            return;
    }
    
    SetColor(newColor);
}

// HSV 转换函数（参考 Blender 实现）
void UIColorPicker::RGBToHSV(float r, float g, float b, float& h, float& s, float& v) const {
    float cmax = std::max({r, g, b});
    float cmin = std::min({r, g, b});
    float diff = cmax - cmin;
    
    // 计算色调 (Hue)
    if (diff < 0.0001f) {
        h = 0.0f;
    } else if (cmax == r) {
        h = 60.0f * fmod((g - b) / diff, 6.0f);
    } else if (cmax == g) {
        h = 60.0f * ((b - r) / diff + 2.0f);
    } else {
        h = 60.0f * ((r - g) / diff + 4.0f);
    }
    
    if (h < 0.0f) {
        h += 360.0f;
    }
    
    // 计算饱和度 (Saturation)
    if (cmax < 0.0001f) {
        s = 0.0f;
    } else {
        s = diff / cmax;
    }
    
    // 计算明度 (Value)
    v = cmax;
}

void UIColorPicker::HSVToRGB(float h, float s, float v, float& r, float& g, float& b) const {
    if (s < 0.0001f) {
        r = g = b = v;
        return;
    }
    
    h = fmod(h, 360.0f);
    if (h < 0.0f) {
        h += 360.0f;
    }
    
    float c = v * s;
    float x = c * (1.0f - fabs(fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    
    if (h < 60.0f) {
        r = c; g = x; b = 0.0f;
    } else if (h < 120.0f) {
        r = x; g = c; b = 0.0f;
    } else if (h < 180.0f) {
        r = 0.0f; g = c; b = x;
    } else if (h < 240.0f) {
        r = 0.0f; g = x; b = c;
    } else if (h < 300.0f) {
        r = x; g = 0.0f; b = c;
    } else {
        r = c; g = 0.0f; b = x;
    }
    
    r += m;
    g += m;
    b += m;
}

} // namespace Render::UI

