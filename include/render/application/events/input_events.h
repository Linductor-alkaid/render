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

// Blender风格操作事件
enum class OperationType {
    Select,      // 选择
    Add,         // 添加（Shift+选择）
    Delete,      // 删除
    Move,        // 移动（G键）
    Rotate,      // 旋转（R键）
    Scale,       // 缩放（S键）
    Duplicate,   // 复制（Shift+D）
    Cancel,      // 取消（Esc或右键）
    Confirm      // 确认（Enter或左键）
};

struct OperationEvent final : public EventBase {
    OperationType type = OperationType::Select;
    int32_t x = 0;  // 鼠标位置
    int32_t y = 0;
    bool isStart = true;  // 操作开始还是结束
    std::string context;  // 操作上下文（如"ObjectMode", "EditMode"）
};

// 手势事件（用于Blender风格操作）
enum class GestureType {
    Drag,        // 拖拽
    Click,       // 点击
    DoubleClick, // 双击
    Pan,         // 平移（中键拖拽）
    Rotate,      // 旋转（中键拖拽+Alt）
    Zoom,        // 缩放（滚轮或Ctrl+中键拖拽）
    BoxSelect,   // 框选（拖拽选择框）
    LassoSelect  // 套索选择
};

struct GestureEvent final : public EventBase {
    GestureType type = GestureType::Click;
    int32_t startX = 0;
    int32_t startY = 0;
    int32_t currentX = 0;
    int32_t currentY = 0;
    int32_t deltaX = 0;
    int32_t deltaY = 0;
    bool isActive = false;  // 手势是否正在进行
    uint32_t button = 0;   // 鼠标按钮（1=左键, 2=中键, 3=右键）
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
};

} // namespace Render::Application::Events


