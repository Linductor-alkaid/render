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
#include <vector>

#include <SDL3/SDL_mouse.h>

#include "render/ui/ui_widget.h"
#include "render/types.h"

namespace Render::UI {

/**
 * @brief 颜色选择器控件（Color Picker）
 * 参考 Blender 的 UI_WTYPE_SWATCH 和 UI_WTYPE_RGB_PICKER 实现
 * 支持 RGB 颜色选择，支持颜色预览，支持鼠标拖拽交互
 * 注：HSV 颜色选择和颜色历史功能将在后续版本中实现
 */
class UIColorPicker : public UIWidget {
public:
    using ChangeHandler = std::function<void(UIColorPicker&, const Color& color)>;

    /**
     * @brief 颜色选择模式
     */
    enum class ColorMode {
        RGB,  // RGB 模式
        HSV   // HSV 模式（暂未完全实现）
    };

    /**
     * @brief 交互区域
     */
    enum class InteractionZone {
        None,       // 无交互区域
        Preview,    // 颜色预览块
        RedChannel, // 红色通道
        GreenChannel, // 绿色通道
        BlueChannel,  // 蓝色通道
        AlphaChannel  // Alpha 通道
    };

    explicit UIColorPicker(std::string id);

    /**
     * @brief 设置当前颜色
     */
    void SetColor(const Color& color);
    [[nodiscard]] const Color& GetColor() const noexcept { return m_color; }

    /**
     * @brief 设置RGB值
     */
    void SetRGB(float r, float g, float b);
    void SetRGBA(float r, float g, float b, float a);

    /**
     * @brief 获取RGB值
     */
    [[nodiscard]] float GetR() const noexcept { return m_color.r; }
    [[nodiscard]] float GetG() const noexcept { return m_color.g; }
    [[nodiscard]] float GetB() const noexcept { return m_color.b; }
    [[nodiscard]] float GetA() const noexcept { return m_color.a; }

    /**
     * @brief 设置是否显示Alpha通道
     */
    void SetShowAlpha(bool showAlpha) noexcept;
    [[nodiscard]] bool IsShowAlpha() const noexcept { return m_showAlpha; }

    /**
     * @brief 设置状态改变回调
     */
    void SetOnChanged(ChangeHandler handler) { m_onChanged = std::move(handler); }

    /**
     * @brief 获取悬停状态
     */
    [[nodiscard]] bool IsHovered() const noexcept { return m_hovered; }

    /**
     * @brief 获取是否正在拖拽
     */
    [[nodiscard]] bool IsDragging() const noexcept { return m_dragging; }

    /**
     * @brief 设置是否显示颜色预览（色块）
     */
    void SetShowPreview(bool showPreview) noexcept;
    [[nodiscard]] bool IsShowPreview() const noexcept { return m_showPreview; }

    /**
     * @brief 设置颜色模式
     */
    void SetColorMode(ColorMode mode) noexcept;
    [[nodiscard]] ColorMode GetColorMode() const noexcept { return m_colorMode; }

protected:
    void OnMouseEnter() override;
    void OnMouseLeave() override;
    void OnMouseMove(const Vector2& position, const Vector2& delta) override;
    void OnMouseButton(uint8_t button, bool pressed, const Vector2& position) override;
    void OnMouseClick(uint8_t button, const Vector2& position) override;
    void OnFocusLost() override;

private:
    void SetHovered(bool hovered);
    void SetDragging(bool dragging);
    void NotifyColorChanged();
    
    // 交互辅助函数
    InteractionZone GetZoneAtPosition(const Vector2& position) const;
    void UpdateColorFromPosition(const Vector2& position);
    void UpdateChannelValue(InteractionZone zone, float normalizedValue);
    
    // HSV 转换辅助函数（参考 Blender 实现）
    void RGBToHSV(float r, float g, float b, float& h, float& s, float& v) const;
    void HSVToRGB(float h, float s, float v, float& r, float& g, float& b) const;

    Color m_color = Color(1.0f, 1.0f, 1.0f, 1.0f); // 默认白色
    bool m_showAlpha = false;
    bool m_showPreview = true;
    bool m_hovered = false;
    bool m_dragging = false;
    ColorMode m_colorMode = ColorMode::RGB;
    InteractionZone m_dragZone = InteractionZone::None;
    Vector2 m_dragStartPosition{};
    Color m_dragStartColor{};
    
    ChangeHandler m_onChanged;
};

} // namespace Render::UI

