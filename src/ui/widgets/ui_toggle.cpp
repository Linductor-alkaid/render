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
#include "render/ui/widgets/ui_toggle.h"

#include "render/application/app_context.h"
#include "render/logger.h"

namespace Render::UI {

UIToggle::UIToggle(std::string id)
    : UIWidget(std::move(id)) {
    // 设置默认尺寸（开关按钮通常比复选框宽一些）
    SetPreferredSize(Vector2(200.0f, 24.0f));
    SetMinSize(Vector2(50.0f, 24.0f));
    m_animationProgress = m_toggled ? 1.0f : 0.0f;
}

void UIToggle::SetLabel(std::string label) {
    if (m_label == label) {
        return;
    }
    m_label = std::move(label);
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIToggle::SetToggled(bool toggled) {
    if (m_toggled == toggled && !m_animating) {
        return;
    }
    
    bool oldToggled = m_toggled;
    m_toggled = toggled;
    
    // 启动动画
    m_animating = true;
    m_animationStartProgress = m_animationProgress;
    m_animationTargetProgress = toggled ? 1.0f : 0.0f;
    m_animationTime = 0.0f;
    
    MarkDirty(UIWidgetDirtyFlag::Layout);
    
    // 如果动画时间为0，立即完成
    if (m_animationDuration <= 0.0f) {
        m_animationProgress = m_animationTargetProgress;
        m_animating = false;
    }
    
    // 调用回调（仅在状态真正改变时）
    if (oldToggled != toggled && m_onChanged) {
        m_onChanged(*this, toggled);
    }
}

void UIToggle::SetLabelPosition(bool labelOnLeft) noexcept {
    if (m_labelOnLeft == labelOnLeft) {
        return;
    }
    m_labelOnLeft = labelOnLeft;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIToggle::SetAnimationDuration(float duration) noexcept {
    m_animationDuration = std::max(0.0f, duration);
    if (m_animationDuration <= 0.0f && m_animating) {
        // 如果动画时间为0，立即完成动画
        m_animationProgress = m_animationTargetProgress;
        m_animating = false;
        MarkDirty(UIWidgetDirtyFlag::Layout);
    }
}

void UIToggle::OnMouseEnter() {
    SetHovered(true);
}

void UIToggle::OnMouseLeave() {
    SetHovered(false);
}

void UIToggle::OnMouseClick(uint8_t button, const Vector2&) {
    if (button != SDL_BUTTON_LEFT || !UIWidget::IsEnabled()) {
        return;
    }
    ToggleState();
}

void UIToggle::OnFocusLost() {
    SetHovered(false);
}

void UIToggle::SetHovered(bool hovered) {
    if (m_hovered == hovered) {
        return;
    }
    m_hovered = hovered;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIToggle::ToggleState() {
    SetToggled(!m_toggled);
}

void UIToggle::UpdateAnimation(float deltaTime) {
    if (!m_animating) {
        return;
    }
    
    m_animationTime += deltaTime;
    
    if (m_animationTime >= m_animationDuration) {
        // 动画完成
        m_animationProgress = m_animationTargetProgress;
        m_animating = false;
        m_animationTime = m_animationDuration;
    } else {
        // 使用缓动函数（ease-in-out）
        float t = m_animationTime / m_animationDuration;
        // 简单的 ease-in-out 曲线
        t = t * t * (3.0f - 2.0f * t);
        m_animationProgress = m_animationStartProgress + (m_animationTargetProgress - m_animationStartProgress) * t;
    }
    
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

} // namespace Render::UI

