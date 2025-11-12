#pragma once

#include "render/ui/ui_debug_config.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "render/types.h"

struct SDL_Window;

namespace Render::UI {
class UIWidgetTree;
class UICanvas;
class UIWidget;

/**
 * @brief UI 输入事件类型
 */
enum class UIInputEventType {
    MouseMove,
    MouseButton,
    MouseWheel,
    Key,
    Text
};

struct UIMouseMoveEvent {
    Vector2 position = Vector2::Zero();
    Vector2 delta = Vector2::Zero();
};

struct UIMouseButtonEvent {
    Vector2 position = Vector2::Zero();
    uint8_t button = 0;
    bool pressed = false;
    bool released = false;
    bool handled = false;
};

struct UIMouseWheelEvent {
    Vector2 offset = Vector2::Zero();
    bool precise = false;
};

struct UIKeyEvent {
    int scancode = 0;
    bool pressed = false;
    bool repeat = false;
};

struct UITextInputEvent {
    std::string text;
};

/**
 * @brief UI Input Router 负责将原始输入事件转换为 Widget 事件
 */
class UIInputRouter {
public:
    UIInputRouter() = default;
    ~UIInputRouter() = default;

    void Initialize(UIWidgetTree* widgetTree, UICanvas* canvas);
    void Shutdown();

    void SetWidgetTree(UIWidgetTree* widgetTree);
    void SetCanvas(UICanvas* canvas);
    void SetDebugConfig(const UIDebugConfig* config) { m_debugConfig = config; }
    void SetWindow(SDL_Window* window);

    void BeginFrame();
    void EndFrame();

    void QueueMouseMove(const Vector2& position, const Vector2& delta);
    void QueueMouseButton(uint8_t button, bool pressed, const Vector2& position);
    void QueueMouseWheel(const Vector2& offset, bool precise);
    void QueueKey(int scancode, bool pressed, bool repeat);
    void QueueTextInput(const std::string& text);

    UIWidget* GetFocusWidget() const { return m_focusWidget; }
    void SetFocusWidget(UIWidget* widget);

private:
    UIWidget* HitTest(const Vector2& point) const;
    void DispatchMouseEvents();
    void DispatchKeyboardEvents();

    void RequestFocus(UIWidget* widget);
    void ClearFocus();

private:
    UIWidgetTree* m_widgetTree = nullptr;
    UICanvas* m_canvas = nullptr;
    SDL_Window* m_window = nullptr;

    UIWidget* m_focusWidget = nullptr;
    UIWidget* m_hoverWidget = nullptr;
    UIWidget* m_capturedWidget = nullptr;
    uint8_t m_capturedButton = 0;

    std::optional<Vector2> m_lastCursorPosition;

    std::vector<UIMouseMoveEvent> m_mouseMoveQueue;
    std::vector<UIMouseButtonEvent> m_mouseButtonQueue;
    std::vector<UIMouseWheelEvent> m_mouseWheelQueue;
    std::vector<UIKeyEvent> m_keyQueue;
    std::vector<UITextInputEvent> m_textQueue;

    const UIDebugConfig* m_debugConfig = nullptr;
    bool m_loggedMissingWindow = false;
    bool m_loggedStartTextInputFailure = false;
    bool m_loggedStopTextInputFailure = false;

    bool ShouldLog() const { return m_debugConfig && m_debugConfig->logInputEvents; }
};

} // namespace Render::UI


