#pragma once

#include <functional>
#include <string>

#include <SDL3/SDL_mouse.h>

#include "render/ui/ui_widget.h"

namespace Render::UI {

/**
 * @brief 滑块控件（Slider）
 * 参考 Blender 的 UI_WTYPE_SLIDER 实现
 * 支持水平/垂直滑块，支持拖拽交互，支持数值显示
 */
class UISlider : public UIWidget {
public:
    using ChangeHandler = std::function<void(UISlider&, float value)>;

    /**
     * @brief 滑块方向
     */
    enum class Orientation {
        Horizontal, // 水平
        Vertical    // 垂直
    };

    explicit UISlider(std::string id);

    /**
     * @brief 设置标签文本
     */
    void SetLabel(std::string label);
    [[nodiscard]] const std::string& GetLabel() const noexcept { return m_label; }

    /**
     * @brief 设置当前值
     */
    void SetValue(float value);
    [[nodiscard]] float GetValue() const noexcept { return m_value; }

    /**
     * @brief 设置最小值
     */
    void SetMinValue(float minValue) noexcept;
    [[nodiscard]] float GetMinValue() const noexcept { return m_minValue; }

    /**
     * @brief 设置最大值
     */
    void SetMaxValue(float maxValue) noexcept;
    [[nodiscard]] float GetMaxValue() const noexcept { return m_maxValue; }

    /**
     * @brief 设置步长
     */
    void SetStep(float step) noexcept;
    [[nodiscard]] float GetStep() const noexcept { return m_step; }

    /**
     * @brief 设置方向
     */
    void SetOrientation(Orientation orientation) noexcept;
    [[nodiscard]] Orientation GetOrientation() const noexcept { return m_orientation; }

    /**
     * @brief 设置是否显示数值
     */
    void SetShowValue(bool showValue) noexcept;
    [[nodiscard]] bool IsShowValue() const noexcept { return m_showValue; }

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
     * @brief 获取归一化值（0.0-1.0）
     */
    [[nodiscard]] float GetNormalizedValue() const;

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
    void UpdateValueFromPosition(const Vector2& position);
    float ClampValue(float value) const;
    float SnapValue(float value) const;

    std::string m_label;
    float m_value = 0.0f;
    float m_minValue = 0.0f;
    float m_maxValue = 100.0f;
    float m_step = 1.0f;
    Orientation m_orientation = Orientation::Horizontal;
    bool m_showValue = true;
    bool m_hovered = false;
    bool m_dragging = false;
    Vector2 m_dragStartPosition{};
    float m_dragStartValue = 0.0f;
    
    ChangeHandler m_onChanged;
};

} // namespace Render::UI

