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
 * @brief UI input event types
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
 * @brief UI Input Router handles routing raw input events to widgets
 * 
 * Responsibilities:
 * - Convert window coordinates to UI coordinates
 * - Perform hit testing to find widgets under cursor
 * - Route mouse/keyboard/text events to appropriate widgets
 * - Manage focus and hover states
 * - Handle mouse capture for drag operations
 * - Detect and fix "dangling capture" when mouse is released outside window
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

    // Legacy interface (deprecated, recommend using EventBus instead)
    void QueueMouseMove(const Vector2& position, const Vector2& delta);
    void QueueMouseButton(uint8_t button, bool pressed, const Vector2& position);
    void QueueMouseWheel(const Vector2& offset, bool precise);
    void QueueKey(int scancode, bool pressed, bool repeat);
    void QueueTextInput(const std::string& text);
    
    // Gesture handling
    void HandleGesture(const Application::Events::GestureEvent& gesture);

    UIWidget* GetFocusWidget() const { return m_focusWidget; }
    void SetFocusWidget(UIWidget* widget);

private:
    // EventBus subscription callbacks
    void OnMouseMotion(const Application::Events::MouseMotionEvent& event);
    void OnMouseButton(const Application::Events::MouseButtonEvent& event);
    void OnMouseWheel(const Application::Events::MouseWheelEvent& event);
    void OnKey(const Application::Events::KeyEvent& event);
    void OnTextInput(const Application::Events::TextInputEvent& event);
    void OnGesture(const Application::Events::GestureEvent& event);
    
    // Core routing logic
    Vector2 ConvertWindowToUICoordinates(const Vector2& windowPoint) const;
    UIWidget* HitTest(const Vector2& point) const;
    void DispatchMouseEvents();
    void DispatchKeyboardEvents();

    // Focus management
    void RequestFocus(UIWidget* widget);
    void ClearFocus();
    
    /**
     * @brief Check and fix "dangling mouse capture" state
     * 
     * This handles the case where mouse button is released outside the window,
     * causing the application to miss the MouseButtonUp event. Without this fix,
     * widgets like sliders would continue tracking the mouse even after release.
     * 
     * Called automatically by BeginFrame() every frame.
     */
    void CheckAndFixDanglingCapture();
    
    // Gesture recognition helpers
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

    // Widget state tracking
    UIWidget* m_focusWidget = nullptr;      // Widget with keyboard focus
    UIWidget* m_hoverWidget = nullptr;      // Widget under cursor
    UIWidget* m_capturedWidget = nullptr;   // Widget capturing mouse (for drag operations)
    uint8_t m_capturedButton = 0;           // Which button is captured

    std::optional<Vector2> m_lastCursorPosition;

    // Event queues (processed in EndFrame)
    std::vector<UIMouseMoveEvent> m_mouseMoveQueue;
    std::vector<UIMouseButtonEvent> m_mouseButtonQueue;
    std::vector<UIMouseWheelEvent> m_mouseWheelQueue;
    std::vector<UIKeyEvent> m_keyQueue;
    std::vector<UITextInputEvent> m_textQueue;

    // Debug configuration
    const UIDebugConfig* m_debugConfig = nullptr;
    
    // Logging state (to avoid spam)
    bool m_loggedMissingWindow = false;
    bool m_loggedStartTextInputFailure = false;
    bool m_loggedStopTextInputFailure = false;
    
    // EventBus subscription IDs
    std::vector<Application::EventBus::ListenerId> m_subscriptionIds;
    
    // Gesture tracking state
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