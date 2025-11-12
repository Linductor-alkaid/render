#include "render/ui/ui_input_router.h"

#include <queue>

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>

#include "render/logger.h"
#include "render/ui/uicanvas.h"
#include "render/ui/ui_widget.h"
#include "render/ui/ui_widget_tree.h"
#include "render/ui/widgets/ui_text_field.h"

namespace Render::UI {

void UIInputRouter::Initialize(UIWidgetTree* widgetTree, UICanvas* canvas) {
    m_widgetTree = widgetTree;
    m_canvas = canvas;
    m_focusWidget = nullptr;
    m_hoverWidget = nullptr;
    m_lastCursorPosition.reset();
    m_mouseMoveQueue.clear();
    m_mouseButtonQueue.clear();
    m_mouseWheelQueue.clear();
    m_keyQueue.clear();
    m_textQueue.clear();
    Logger::GetInstance().Info("[UIInputRouter] Initialized");
}

void UIInputRouter::Shutdown() {
    m_widgetTree = nullptr;
    m_canvas = nullptr;
    m_focusWidget = nullptr;
    m_hoverWidget = nullptr;
    m_lastCursorPosition.reset();
    m_mouseMoveQueue.clear();
    m_mouseButtonQueue.clear();
    m_mouseWheelQueue.clear();
    m_keyQueue.clear();
    m_textQueue.clear();
    Logger::GetInstance().Info("[UIInputRouter] Shutdown");
}

void UIInputRouter::SetWidgetTree(UIWidgetTree* widgetTree) {
    if (m_widgetTree == widgetTree) {
        return;
    }
    Logger::GetInstance().InfoFormat("[UIInputRouter] SetWidgetTree old=%p new=%p",
                                     static_cast<void*>(m_widgetTree),
                                     static_cast<void*>(widgetTree));
    m_widgetTree = widgetTree;
    m_focusWidget = nullptr;
    m_hoverWidget = nullptr;
}

void UIInputRouter::SetCanvas(UICanvas* canvas) {
    if (m_canvas == canvas) {
        return;
    }
    Logger::GetInstance().InfoFormat("[UIInputRouter] SetCanvas old=%p new=%p",
                                     static_cast<void*>(m_canvas),
                                     static_cast<void*>(canvas));
    m_canvas = canvas;
}

void UIInputRouter::SetWindow(SDL_Window* window) {
    if (m_window != window) {
        if (window) {
            Logger::GetInstance().InfoFormat("[UIInputRouter] Attached window=%p (id=%u)",
                                             static_cast<void*>(window),
                                             SDL_GetWindowID(window));
        } else {
            Logger::GetInstance().Warning("[UIInputRouter] Detaching SDL_Window (null). Text input will be disabled.");
        }
    }
    m_window = window;
    if (m_window) {
        m_loggedMissingWindow = false;
    }
    m_loggedStartTextInputFailure = false;
    m_loggedStopTextInputFailure = false;
}

void UIInputRouter::BeginFrame() {
    // 清理上一帧状态
    m_mouseMoveQueue.clear();
    m_mouseButtonQueue.clear();
    m_mouseWheelQueue.clear();
    m_keyQueue.clear();
    m_textQueue.clear();
}

void UIInputRouter::EndFrame() {
    Logger::GetInstance().InfoFormat(
        "[UIInputRouter] EndFrame queues: move=%zu button=%zu wheel=%zu key=%zu text=%zu focus=%s",
        m_mouseMoveQueue.size(),
        m_mouseButtonQueue.size(),
        m_mouseWheelQueue.size(),
        m_keyQueue.size(),
        m_textQueue.size(),
        m_focusWidget ? m_focusWidget->GetId().c_str() : "null");
    DispatchMouseEvents();
    DispatchKeyboardEvents();
}

void UIInputRouter::QueueMouseMove(const Vector2& position, const Vector2& delta) {
    m_mouseMoveQueue.push_back(UIMouseMoveEvent{position, delta});
    m_lastCursorPosition = position;
}

void UIInputRouter::QueueMouseButton(uint8_t button, bool pressed, const Vector2& position) {
    Logger::GetInstance().InfoFormat("[UIInputRouter] QueueMouseButton button=%u pressed=%s pos=(%.1f, %.1f)",
                                     button,
                                     pressed ? "true" : "false",
                                     position.x(),
                                     position.y());
    m_mouseButtonQueue.push_back(UIMouseButtonEvent{position, button, pressed, !pressed, false});
    m_lastCursorPosition = position;
}

void UIInputRouter::QueueMouseWheel(const Vector2& offset, bool precise) {
    m_mouseWheelQueue.push_back(UIMouseWheelEvent{offset, precise});
}

void UIInputRouter::QueueKey(int scancode, bool pressed, bool repeat) {
    m_keyQueue.push_back(UIKeyEvent{scancode, pressed, repeat});
}

void UIInputRouter::QueueTextInput(const std::string& text) {
    if (!text.empty()) {
        Logger::GetInstance().InfoFormat("[UIInputRouter] QueueTextInput \"%s\"", text.c_str());
        m_textQueue.push_back(UITextInputEvent{text});
    }
}

void UIInputRouter::SetFocusWidget(UIWidget* widget) {
    if (m_focusWidget == widget) {
        return;
    }
    RequestFocus(widget);
}

UIWidget* UIInputRouter::HitTest(const Vector2& point) const {
    if (!m_widgetTree) {
        return nullptr;
    }
    UIWidget* result = nullptr;

    std::queue<UIWidget*> queue;
    if (auto* root = m_widgetTree->GetRoot()) {
        queue.push(root);
    }

    while (!queue.empty()) {
        UIWidget* current = queue.front();
        queue.pop();

        const Rect& rect = current->GetLayoutRect();
        if (rect.width > 0.0f && rect.height > 0.0f) {
            if (point.x() >= rect.x && point.x() <= rect.x + rect.width &&
                point.y() >= rect.y && point.y() <= rect.y + rect.height) {
                result = current;
            }
        }

        current->ForEachChild([&](UIWidget& child) {
            queue.push(&child);
        });
    }

    return result;
}

void UIInputRouter::DispatchMouseEvents() {
    if (!m_widgetTree || !m_canvas) {
        return;
    }

    // 更新 hover
    if (m_lastCursorPosition.has_value()) {
        UIWidget* hitWidget = HitTest(*m_lastCursorPosition);
        Logger::GetInstance().InfoFormat("[UIInputRouter] HitTest hover point=(%.1f, %.1f) -> %s",
                                         m_lastCursorPosition->x(),
                                         m_lastCursorPosition->y(),
                                         hitWidget ? hitWidget->GetId().c_str() : "null");
        if (hitWidget != m_hoverWidget) {
            if (m_hoverWidget) {
                if (ShouldLog()) {
                    Logger::GetInstance().DebugFormat("[UIInputRouter] MouseLeave -> %s", m_hoverWidget->GetId().c_str());
                }
                m_hoverWidget->OnMouseLeave();
            }
            if (hitWidget) {
                if (ShouldLog()) {
                    Logger::GetInstance().DebugFormat("[UIInputRouter] MouseEnter -> %s", hitWidget->GetId().c_str());
                }
                hitWidget->OnMouseEnter();
            }
            m_hoverWidget = hitWidget;
        }
    }

    for (auto& buttonEvent : m_mouseButtonQueue) {
        UIWidget* hitWidget = HitTest(buttonEvent.position);
        Logger::GetInstance().InfoFormat("[UIInputRouter] DispatchMouseButton button=%u pressed=%s pos=(%.1f, %.1f) hit=%s focus=%s",
                                         buttonEvent.button,
                                         buttonEvent.pressed ? "true" : "false",
                                         buttonEvent.position.x(),
                                         buttonEvent.position.y(),
                                         hitWidget ? hitWidget->GetId().c_str() : "null",
                                         m_focusWidget ? m_focusWidget->GetId().c_str() : "null");
        bool insideCapture = (hitWidget && hitWidget == m_capturedWidget);

        UIWidget* receiver = hitWidget;
        if (!receiver && m_capturedWidget) {
            receiver = m_capturedWidget;
        }

        if (!receiver) {
            if (buttonEvent.pressed && buttonEvent.button == SDL_BUTTON_LEFT) {
                if (ShouldLog()) {
                    Logger::GetInstance().Debug("[UIInputRouter] MouseDown on empty area, clearing focus.");
                }
                ClearFocus();
            }
            continue;
        }

        if (ShouldLog()) {
            Logger::GetInstance().DebugFormat("[UIInputRouter] Mouse%s button=%u pos=(%.2f, %.2f) target=%s",
                                              buttonEvent.pressed ? "Down" : "Up",
                                              buttonEvent.button,
                                              buttonEvent.position.x(),
                                              buttonEvent.position.y(),
                                              receiver->GetId().c_str());
        }

        receiver->OnMouseButton(buttonEvent.button, buttonEvent.pressed, buttonEvent.position);

        if (buttonEvent.pressed) {
            m_capturedWidget = receiver;
            m_capturedButton = buttonEvent.button;
            RequestFocus(receiver);
        } else {
            bool wasCaptured = (m_capturedWidget == receiver) && (buttonEvent.button == m_capturedButton);
            if (wasCaptured && insideCapture) {
                if (ShouldLog()) {
                    Logger::GetInstance().DebugFormat("[UIInputRouter] MouseClick button=%u pos=(%.2f, %.2f) target=%s",
                                                      buttonEvent.button,
                                                      buttonEvent.position.x(),
                                                      buttonEvent.position.y(),
                                                      receiver->GetId().c_str());
                }
                receiver->OnMouseClick(buttonEvent.button, buttonEvent.position);
            }
            if (wasCaptured) {
                m_capturedWidget = nullptr;
                m_capturedButton = 0;
            }
        }
    }

    for (auto& wheelEvent : m_mouseWheelQueue) {
        if (m_hoverWidget) {
            if (ShouldLog()) {
                Logger::GetInstance().DebugFormat("[UIInputRouter] MouseWheel offset=(%.2f, %.2f) target=%s",
                                                  wheelEvent.offset.x(),
                                                  wheelEvent.offset.y(),
                                                  m_hoverWidget->GetId().c_str());
            }
            m_hoverWidget->OnMouseWheel(wheelEvent.offset);
        }
    }
}

void UIInputRouter::DispatchKeyboardEvents() {
    for (auto& keyEvent : m_keyQueue) {
        if (m_focusWidget) {
            if (ShouldLog()) {
                Logger::GetInstance().DebugFormat("[UIInputRouter] Key %s scancode=%d repeat=%s target=%s",
                                                  keyEvent.pressed ? "Down" : "Up",
                                                  keyEvent.scancode,
                                                  keyEvent.repeat ? "true" : "false",
                                                  m_focusWidget->GetId().c_str());
            }
            m_focusWidget->OnKey(keyEvent.scancode, keyEvent.pressed, keyEvent.repeat);
        }
    }

    for (auto& textEvent : m_textQueue) {
        if (m_focusWidget) {
            Logger::GetInstance().InfoFormat("[UIInputRouter] DispatchTextInput \"%s\" -> %s",
                                             textEvent.text.c_str(),
                                             m_focusWidget->GetId().c_str());
            m_focusWidget->OnTextInput(textEvent.text);
        }
    }
}

void UIInputRouter::RequestFocus(UIWidget* widget) {
    if (m_focusWidget == widget) {
        return;
    }
    Logger::GetInstance().InfoFormat("[UIInputRouter] RequestFocus current=%s target=%s",
                                     m_focusWidget ? m_focusWidget->GetId().c_str() : "null",
                                     widget ? widget->GetId().c_str() : "null");
    UIWidget* previous = m_focusWidget;
    if (previous) {
        if (ShouldLog()) {
            Logger::GetInstance().DebugFormat("[UIInputRouter] FocusLost -> %s", previous->GetId().c_str());
        }
        if (m_window && dynamic_cast<UITextField*>(previous) != nullptr) {
            if (!SDL_StopTextInput(m_window)) {
                if (!m_loggedStopTextInputFailure) {
                    Logger::GetInstance().WarningFormat(
                        "[UIInputRouter] SDL_StopTextInput failed: %s", SDL_GetError());
                    m_loggedStopTextInputFailure = true;
                }
            } else {
                m_loggedStopTextInputFailure = false;
            }
        }
        previous->OnFocusLost();
    }

    m_focusWidget = widget;
    if (m_focusWidget) {
        if (ShouldLog()) {
            Logger::GetInstance().DebugFormat("[UIInputRouter] FocusGained -> %s", m_focusWidget->GetId().c_str());
        }
        const bool requiresTextInput = dynamic_cast<UITextField*>(m_focusWidget) != nullptr;
        if (requiresTextInput) {
            if (!m_window) {
                if (!m_loggedMissingWindow) {
                    Logger::GetInstance().Warning("[UIInputRouter] Cannot start text input: SDL_Window is null.");
                    m_loggedMissingWindow = true;
                }
            } else if (!SDL_StartTextInput(m_window)) {
                if (!m_loggedStartTextInputFailure) {
                    Logger::GetInstance().WarningFormat(
                        "[UIInputRouter] SDL_StartTextInput failed: %s", SDL_GetError());
                    m_loggedStartTextInputFailure = true;
                }
            } else {
                Logger::GetInstance().InfoFormat("[UIInputRouter] SDL_StartTextInput success for %s (window=%p)",
                                                 m_focusWidget->GetId().c_str(),
                                                 static_cast<void*>(m_window));
                m_loggedStartTextInputFailure = false;
            }
        }
        m_focusWidget->OnFocusGained();
    }
}

void UIInputRouter::ClearFocus() {
    if (m_focusWidget) {
        Logger::GetInstance().InfoFormat("[UIInputRouter] FocusCleared -> %s", m_focusWidget->GetId().c_str());
        if (m_window && dynamic_cast<UITextField*>(m_focusWidget) != nullptr) {
            SDL_StopTextInput(m_window);
        }
        m_focusWidget->OnFocusLost();
        m_focusWidget = nullptr;
    }
    m_capturedWidget = nullptr;
    m_capturedButton = 0;
}

} // namespace Render::UI


