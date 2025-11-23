#pragma once

#include "render/ui/ui_debug_config.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "render/types.h"
#include "render/application/event_bus.h"
#include "render/application/events/input_events.h"

struct SDL_Window;

namespace Render::Application {
struct AppContext;
}

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

    void Initialize(UIWidgetTree* widgetTree, UICanvas* canvas, Application::EventBus* eventBus = nullptr);
    void Shutdown();

    void SetWidgetTree(UIWidgetTree* widgetTree);
    void SetCanvas(UICanvas* canvas);
    void SetDebugConfig(const UIDebugConfig* config) { m_debugConfig = config; }
    void SetWindow(SDL_Window* window);

    void BeginFrame();
    void EndFrame();

    // 兼容旧接口（已废弃，建议使用 EventBus）
    void QueueMouseMove(const Vector2& position, const Vector2& delta);
    void QueueMouseButton(uint8_t button, bool pressed, const Vector2& position);
    void QueueMouseWheel(const Vector2& offset, bool precise);
    void QueueKey(int scancode, bool pressed, bool repeat);
    void QueueTextInput(const std::string& text);
    
    // 手势处理
    void HandleGesture(const Application::Events::GestureEvent& gesture);

    UIWidget* GetFocusWidget() const { return m_focusWidget; }
    void SetFocusWidget(UIWidget* widget);

private:
    // EventBus 订阅回调
    void OnMouseMotion(const Application::Events::MouseMotionEvent& event);
    void OnMouseButton(const Application::Events::MouseButtonEvent& event);
    void OnMouseWheel(const Application::Events::MouseWheelEvent& event);
    void OnKey(const Application::Events::KeyEvent& event);
    void OnTextInput(const Application::Events::TextInputEvent& event);
    void OnGesture(const Application::Events::GestureEvent& event);
    
    Vector2 ConvertWindowToUICoordinates(const Vector2& windowPoint) const;
    UIWidget* HitTest(const Vector2& point) const;
    void DispatchMouseEvents();
    void DispatchKeyboardEvents();

    void RequestFocus(UIWidget* widget);
    void ClearFocus();
    
    // 手势识别辅助
    void ProcessGesture(const Application::Events::GestureEvent& gesture);
    void HandleDragGesture(const Application::Events::GestureEvent& gesture);
    void HandleClickGesture(const Application::Events::GestureEvent& gesture);
    void HandlePanGesture(const Application::Events::GestureEvent& gesture);
    void HandleZoomGesture(const Application::Events::GestureEvent& gesture);

private:
    UIWidgetTree* m_widgetTree = nullptr;
    UICanvas* m_canvas = nullptr;
    SDL_Window* m_window = nullptr;
    Application::EventBus* m_eventBus = nullptr;

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
    
    // EventBus 订阅 ID
    std::vector<Application::EventBus::ListenerId> m_subscriptionIds;
    
    // 手势状态
    struct GestureTracking {
        Application::Events::GestureType type = Application::Events::GestureType::Click;
        Vector2 startPosition = Vector2::Zero();
        Vector2 lastPosition = Vector2::Zero();
        bool active = false;
        uint32_t button = 0;
    } m_gestureTracking;

    bool ShouldLog() const { return m_debugConfig && m_debugConfig->logInputEvents; }
};

} // namespace Render::UI


