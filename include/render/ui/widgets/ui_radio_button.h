#pragma once

#include <functional>
#include <string>
#include <vector>
#include <memory>

#include <SDL3/SDL_mouse.h>

#include "render/ui/ui_widget.h"

namespace Render::UI {

/**
 * @brief 单选按钮组管理器
 * 管理一组单选按钮，确保同一时刻只有一个按钮被选中
 */
class UIRadioButtonGroup {
public:
    UIRadioButtonGroup() = default;
    ~UIRadioButtonGroup() = default;

    /**
     * @brief 注册单选按钮到组
     */
    void RegisterButton(class UIRadioButton* button);

    /**
     * @brief 从组中移除单选按钮
     */
    void UnregisterButton(class UIRadioButton* button);

    /**
     * @brief 选择指定的单选按钮（取消选择同组其他按钮）
     */
    void SelectButton(class UIRadioButton* button);

    /**
     * @brief 获取当前选中的按钮
     */
    [[nodiscard]] class UIRadioButton* GetSelectedButton() const noexcept { return m_selectedButton; }

    /**
     * @brief 获取组中所有按钮
     */
    [[nodiscard]] const std::vector<class UIRadioButton*>& GetButtons() const noexcept { return m_buttons; }

private:
    std::vector<class UIRadioButton*> m_buttons;
    class UIRadioButton* m_selectedButton = nullptr;
};

/**
 * @brief 单选按钮控件
 * 参考 Blender 的 UI_WTYPE_RADIO 实现
 * 支持单选组，确保同一组中只有一个按钮被选中
 */
class UIRadioButton : public UIWidget {
public:
    using ChangeHandler = std::function<void(UIRadioButton&, bool selected)>;

    explicit UIRadioButton(std::string id);

    /**
     * @brief 设置标签文本
     */
    void SetLabel(std::string label);
    [[nodiscard]] const std::string& GetLabel() const noexcept { return m_label; }

    /**
     * @brief 设置选中状态
     */
    void SetSelected(bool selected);
    [[nodiscard]] bool IsSelected() const noexcept { return m_selected; }

    /**
     * @brief 设置单选按钮组
     */
    void SetGroup(UIRadioButtonGroup* group);
    [[nodiscard]] UIRadioButtonGroup* GetGroup() const noexcept { return m_group; }

    /**
     * @brief 设置状态改变回调
     */
    void SetOnChanged(ChangeHandler handler) { m_onChanged = std::move(handler); }

    /**
     * @brief 获取悬停状态
     */
    [[nodiscard]] bool IsHovered() const noexcept { return m_hovered; }

protected:
    void OnMouseEnter() override;
    void OnMouseLeave() override;
    void OnMouseClick(uint8_t button, const Vector2& position) override;
    void OnFocusLost() override;

    // 允许 UIRadioButtonGroup 访问
    friend class UIRadioButtonGroup;
    void SetSelectedInternal(bool selected, bool notifyGroup);

private:
    void SetHovered(bool hovered);

    std::string m_label;
    bool m_selected = false;
    bool m_hovered = false;
    UIRadioButtonGroup* m_group = nullptr;
    ChangeHandler m_onChanged;
};

} // namespace Render::UI

