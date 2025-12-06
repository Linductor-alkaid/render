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

#include <functional>
#include <string>

#include <SDL3/SDL_mouse.h>

#include "render/ui/ui_widget.h"
#include "render/application/app_context.h"

namespace Render::UI {

/**
 * @brief 开关按钮控件（Toggle Switch）
 * 参考现代UI设计，支持开/关两种状态，支持动画过渡
 * 类似 iOS/Android 的开关控件
 */
class UIToggle : public UIWidget {
public:
    using ChangeHandler = std::function<void(UIToggle&, bool enabled)>;

    explicit UIToggle(std::string id);

    /**
     * @brief 设置标签文本
     */
    void SetLabel(std::string label);
    [[nodiscard]] const std::string& GetLabel() const noexcept { return m_label; }

    /**
     * @brief 设置开关状态
     */
    void SetToggled(bool toggled);
    [[nodiscard]] bool IsToggled() const noexcept { return m_toggled; }

    /**
     * @brief 设置状态改变回调
     */
    void SetOnChanged(ChangeHandler handler) { m_onChanged = std::move(handler); }

    /**
     * @brief 获取悬停状态
     */
    [[nodiscard]] bool IsHovered() const noexcept { return m_hovered; }

    /**
     * @brief 设置标签位置（左侧或右侧）
     */
    void SetLabelPosition(bool labelOnLeft) noexcept;
    [[nodiscard]] bool IsLabelOnLeft() const noexcept { return m_labelOnLeft; }

    /**
     * @brief 设置动画过渡时间（秒）
     */
    void SetAnimationDuration(float duration) noexcept;
    [[nodiscard]] float GetAnimationDuration() const noexcept { return m_animationDuration; }

    /**
     * @brief 获取当前动画进度（0.0-1.0）
     */
    [[nodiscard]] float GetAnimationProgress() const noexcept { return m_animationProgress; }

    /**
     * @brief 更新动画（需要在外部每帧调用）
     */
    void UpdateAnimation(float deltaTime);

protected:
    void OnMouseEnter() override;
    void OnMouseLeave() override;
    void OnMouseClick(uint8_t button, const Vector2& position) override;
    void OnFocusLost() override;

private:
    void SetHovered(bool hovered);
    void ToggleState();

    std::string m_label;
    bool m_toggled = false;
    bool m_hovered = false;
    bool m_labelOnLeft = false; // false = 标签在右侧（默认），true = 标签在左侧
    
    // 动画相关
    bool m_animating = false;
    float m_animationDuration = 0.2f; // 默认200ms动画
    float m_animationProgress = 0.0f; // 0.0 = 关闭，1.0 = 开启
    float m_animationStartProgress = 0.0f;
    float m_animationTargetProgress = 0.0f;
    float m_animationTime = 0.0f;
    
    ChangeHandler m_onChanged;
};

} // namespace Render::UI

