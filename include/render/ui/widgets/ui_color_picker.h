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
 * 支持 RGB 颜色选择，支持颜色预览
 * 注：HSV 颜色选择和颜色历史功能将在后续版本中实现
 */
class UIColorPicker : public UIWidget {
public:
    using ChangeHandler = std::function<void(UIColorPicker&, const Color& color)>;

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
     * @brief 设置是否显示颜色预览（色块）
     */
    void SetShowPreview(bool showPreview) noexcept;
    [[nodiscard]] bool IsShowPreview() const noexcept { return m_showPreview; }

protected:
    void OnMouseEnter() override;
    void OnMouseLeave() override;
    void OnMouseClick(uint8_t button, const Vector2& position) override;
    void OnFocusLost() override;

private:
    void SetHovered(bool hovered);
    void NotifyColorChanged();

    Color m_color = Color(1.0f, 1.0f, 1.0f, 1.0f); // 默认白色
    bool m_showAlpha = false;
    bool m_showPreview = true;
    bool m_hovered = false;
    ChangeHandler m_onChanged;
};

} // namespace Render::UI

