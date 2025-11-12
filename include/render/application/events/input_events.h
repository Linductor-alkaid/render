#pragma once

#include <cstdint>
#include <string>

#include "render/application/event_bus.h"

namespace Render::Application::Events {

enum class KeyState {
    Pressed,
    Released
};

struct KeyEvent final : public EventBase {
    int scancode = 0;
    bool repeat = false;
    KeyState state = KeyState::Pressed;
};

enum class MouseButtonState {
    Pressed,
    Released
};

struct MouseButtonEvent final : public EventBase {
    uint8_t button = 0;
    int32_t x = 0;
    int32_t y = 0;
    MouseButtonState state = MouseButtonState::Pressed;
};

struct MouseMotionEvent final : public EventBase {
    int32_t x = 0;
    int32_t y = 0;
    int32_t dx = 0;
    int32_t dy = 0;
};

struct MouseWheelEvent final : public EventBase {
    int32_t x = 0;
    int32_t y = 0;
    bool precise = false;
};

struct TextInputEvent final : public EventBase {
    std::string text;
};

} // namespace Render::Application::Events


