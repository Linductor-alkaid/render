#pragma once

#include <functional>
#include <string>

#include <SDL3/SDL_mouse.h>

#include "render/ui/ui_widget.h"

namespace Render::UI {

class UIButton : public UIWidget {
public:
    using ClickHandler = std::function<void(UIButton&)>;

    explicit UIButton(std::string id);

    void SetLabel(std::string label);
    [[nodiscard]] const std::string& GetLabel() const noexcept { return m_label; }

    void SetOnClicked(ClickHandler handler) { m_onClicked = std::move(handler); }

    [[nodiscard]] bool IsHovered() const noexcept { return m_hovered; }
    [[nodiscard]] bool IsPressed() const noexcept { return m_pressed; }

protected:
    void OnMouseEnter() override;
    void OnMouseLeave() override;
    void OnMouseButton(uint8_t button, bool pressed, const Vector2& position) override;
    void OnMouseClick(uint8_t button, const Vector2& position) override;
    void OnFocusLost() override;

private:
    void SetPressed(bool pressed);
    void SetHovered(bool hovered);

    std::string m_label = "Button";
    ClickHandler m_onClicked;
    bool m_hovered = false;
    bool m_pressed = false;
};

} // namespace Render::UI


