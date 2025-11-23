#include "render/ui/ui_input_router.h"

#include <queue>
#include <algorithm>

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>

#include "render/logger.h"
#include "render/ui/uicanvas.h"
#include "render/ui/ui_widget.h"
#include "render/ui/ui_widget_tree.h"
#include "render/ui/widgets/ui_text_field.h"
#include "render/application/event_bus.h"
#include "render/application/events/input_events.h"

namespace Render::UI {

void UIInputRouter::Initialize(UIWidgetTree* widgetTree, UICanvas* canvas, Application::EventBus* eventBus) {
    m_widgetTree = widgetTree;
    m_canvas = canvas;
    m_eventBus = eventBus;
    m_focusWidget = nullptr;
    m_hoverWidget = nullptr;
    m_lastCursorPosition.reset();
    m_mouseMoveQueue.clear();
    m_mouseButtonQueue.clear();
    m_mouseWheelQueue.clear();
    m_keyQueue.clear();
    m_textQueue.clear();
    m_gestureTracking = GestureTracking{};
    
    // 订阅 EventBus 事件
    if (m_eventBus) {
        auto mouseMotionId = m_eventBus->Subscribe<Application::Events::MouseMotionEvent>(
            [this](const Application::Events::MouseMotionEvent& event) {
                OnMouseMotion(event);
            },
            100  // 高优先级，UI 优先处理
        );
        m_subscriptionIds.push_back(mouseMotionId);
        
        auto mouseButtonId = m_eventBus->Subscribe<Application::Events::MouseButtonEvent>(
            [this](const Application::Events::MouseButtonEvent& event) {
                OnMouseButton(event);
            },
            100
        );
        m_subscriptionIds.push_back(mouseButtonId);
        
        auto mouseWheelId = m_eventBus->Subscribe<Application::Events::MouseWheelEvent>(
            [this](const Application::Events::MouseWheelEvent& event) {
                OnMouseWheel(event);
            },
            100
        );
        m_subscriptionIds.push_back(mouseWheelId);
        
        auto keyId = m_eventBus->Subscribe<Application::Events::KeyEvent>(
            [this](const Application::Events::KeyEvent& event) {
                OnKey(event);
            },
            100
        );
        m_subscriptionIds.push_back(keyId);
        
        auto textId = m_eventBus->Subscribe<Application::Events::TextInputEvent>(
            [this](const Application::Events::TextInputEvent& event) {
                OnTextInput(event);
            },
            100
        );
        m_subscriptionIds.push_back(textId);
        
        auto gestureId = m_eventBus->Subscribe<Application::Events::GestureEvent>(
            [this](const Application::Events::GestureEvent& event) {
                OnGesture(event);
            },
            100
        );
        m_subscriptionIds.push_back(gestureId);
        
        Logger::GetInstance().InfoFormat("[UIInputRouter] Initialized with EventBus (subscriptions=%zu)", 
                                        m_subscriptionIds.size());
    } else {
        Logger::GetInstance().Info("[UIInputRouter] Initialized without EventBus (using legacy Queue methods)");
    }
}

void UIInputRouter::Shutdown() {
    // 取消 EventBus 订阅
    if (m_eventBus) {
        for (auto id : m_subscriptionIds) {
            m_eventBus->Unsubscribe(id);
        }
        m_subscriptionIds.clear();
    }
    
    m_widgetTree = nullptr;
    m_canvas = nullptr;
    m_eventBus = nullptr;
    m_focusWidget = nullptr;
    m_hoverWidget = nullptr;
    m_lastCursorPosition.reset();
    m_mouseMoveQueue.clear();
    m_mouseButtonQueue.clear();
    m_mouseWheelQueue.clear();
    m_keyQueue.clear();
    m_textQueue.clear();
    m_gestureTracking = GestureTracking{};
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
        if (ShouldLog()) {
            Logger::GetInstance().DebugFormat("[UIInputRouter] QueueTextInput \"%s\" (queue size before: %zu)", 
                                             text.c_str(), m_textQueue.size());
        }
        m_textQueue.push_back(UITextInputEvent{text});
    }
}

// EventBus 事件回调
void UIInputRouter::OnMouseMotion(const Application::Events::MouseMotionEvent& event) {
    Vector2 position(static_cast<float>(event.x), static_cast<float>(event.y));
    Vector2 delta(static_cast<float>(event.dx), static_cast<float>(event.dy));
    QueueMouseMove(position, delta);
}

void UIInputRouter::OnMouseButton(const Application::Events::MouseButtonEvent& event) {
    Vector2 position(static_cast<float>(event.x), static_cast<float>(event.y));
    bool pressed = (event.state == Application::Events::MouseButtonState::Pressed);
    QueueMouseButton(event.button, pressed, position);
}

void UIInputRouter::OnMouseWheel(const Application::Events::MouseWheelEvent& event) {
    Vector2 offset(static_cast<float>(event.x), static_cast<float>(event.y));
    QueueMouseWheel(offset, event.precise);
}

void UIInputRouter::OnKey(const Application::Events::KeyEvent& event) {
    bool pressed = (event.state == Application::Events::KeyState::Pressed);
    QueueKey(event.scancode, pressed, event.repeat);
}

void UIInputRouter::OnTextInput(const Application::Events::TextInputEvent& event) {
    if (ShouldLog()) {
        Logger::GetInstance().DebugFormat("[UIInputRouter] OnTextInput from EventBus text=\"%s\"", event.text.c_str());
    }
    QueueTextInput(event.text);
}

void UIInputRouter::OnGesture(const Application::Events::GestureEvent& event) {
    HandleGesture(event);
}

void UIInputRouter::HandleGesture(const Application::Events::GestureEvent& gesture) {
    if (ShouldLog()) {
        Logger::GetInstance().DebugFormat("[UIInputRouter] HandleGesture type=%d active=%s start=(%d,%d) current=(%d,%d)",
                                         static_cast<int>(gesture.type),
                                         gesture.isActive ? "true" : "false",
                                         gesture.startX, gesture.startY,
                                         gesture.currentX, gesture.currentY);
    }
    
    ProcessGesture(gesture);
}

void UIInputRouter::ProcessGesture(const Application::Events::GestureEvent& gesture) {
    switch (gesture.type) {
    case Application::Events::GestureType::Drag:
        HandleDragGesture(gesture);
        break;
    case Application::Events::GestureType::Click:
    case Application::Events::GestureType::DoubleClick:
        HandleClickGesture(gesture);
        break;
    case Application::Events::GestureType::Pan:
        HandlePanGesture(gesture);
        break;
    case Application::Events::GestureType::Zoom:
        HandleZoomGesture(gesture);
        break;
    case Application::Events::GestureType::Rotate:
        // 旋转手势通常用于3D视图，UI不需要处理
        break;
    case Application::Events::GestureType::BoxSelect:
    case Application::Events::GestureType::LassoSelect:
        // 框选和套索选择通常用于场景视图，UI不需要处理
        break;
    }
}

void UIInputRouter::HandleDragGesture(const Application::Events::GestureEvent& gesture) {
    Vector2 position(static_cast<float>(gesture.currentX), static_cast<float>(gesture.currentY));
    Vector2 delta(static_cast<float>(gesture.deltaX), static_cast<float>(gesture.deltaY));
    
    if (gesture.isActive) {
        // 拖拽开始或进行中
        if (!m_gestureTracking.active) {
            m_gestureTracking.active = true;
            m_gestureTracking.type = gesture.type;
            m_gestureTracking.startPosition = Vector2(static_cast<float>(gesture.startX), static_cast<float>(gesture.startY));
            m_gestureTracking.button = gesture.button;
            
            // 发送鼠标按下事件
            QueueMouseButton(gesture.button, true, m_gestureTracking.startPosition);
        }
        
        // 发送鼠标移动事件
        QueueMouseMove(position, delta);
        m_gestureTracking.lastPosition = position;
    } else {
        // 拖拽结束
        if (m_gestureTracking.active && m_gestureTracking.type == Application::Events::GestureType::Drag) {
            // 发送鼠标释放事件
            QueueMouseButton(gesture.button, false, position);
            m_gestureTracking.active = false;
        }
    }
}

void UIInputRouter::HandleClickGesture(const Application::Events::GestureEvent& gesture) {
    Vector2 position(static_cast<float>(gesture.currentX), static_cast<float>(gesture.currentY));
    
    if (!gesture.isActive) {
        // 点击完成
        if (ShouldLog()) {
            Logger::GetInstance().DebugFormat("[UIInputRouter] ClickGesture completed at (%.1f, %.1f) button=%u",
                                             position.x(), position.y(), gesture.button);
        }
        
        // 发送点击事件
        QueueMouseButton(gesture.button, true, position);
        QueueMouseButton(gesture.button, false, position);
    }
}

void UIInputRouter::HandlePanGesture(const Application::Events::GestureEvent& gesture) {
    // 平移手势通常用于3D视图，但也可以用于可滚动的UI区域
    Vector2 position(static_cast<float>(gesture.currentX), static_cast<float>(gesture.currentY));
    Vector2 delta(static_cast<float>(gesture.deltaX), static_cast<float>(gesture.deltaY));
    
    if (gesture.isActive) {
        // 发送鼠标移动事件（中键拖拽）
        QueueMouseMove(position, delta);
        
        // 如果中键按下，发送按钮事件
        if (!m_gestureTracking.active) {
            m_gestureTracking.active = true;
            m_gestureTracking.type = gesture.type;
            m_gestureTracking.startPosition = Vector2(static_cast<float>(gesture.startX), static_cast<float>(gesture.startY));
            m_gestureTracking.button = gesture.button;
            QueueMouseButton(gesture.button, true, m_gestureTracking.startPosition);
        }
    } else {
        // 平移结束
        if (m_gestureTracking.active && m_gestureTracking.type == Application::Events::GestureType::Pan) {
            QueueMouseButton(gesture.button, false, position);
            m_gestureTracking.active = false;
        }
    }
}

void UIInputRouter::HandleZoomGesture(const Application::Events::GestureEvent& gesture) {
    // 缩放手势（滚轮）
    Vector2 offset(static_cast<float>(gesture.deltaX), static_cast<float>(gesture.deltaY));
    QueueMouseWheel(offset, false);
}

void UIInputRouter::SetFocusWidget(UIWidget* widget) {
    if (m_focusWidget == widget) {
        return;
    }
    RequestFocus(widget);
}

Vector2 UIInputRouter::ConvertWindowToUICoordinates(const Vector2& windowPoint) const {
    if (!m_canvas) {
        return windowPoint;
    }
    
    const auto& state = m_canvas->GetState();
    Vector2 windowSize = state.WindowSize();
    
    // 如果窗口大小为0，直接返回原始坐标
    if (windowSize.x() <= 0.0f || windowSize.y() <= 0.0f) {
        return windowPoint;
    }
    
    // UI坐标系统与窗口坐标系统应该一致（都是Y向下为正，原点在左上角）
    // 由于UI渲染使用窗口大小作为canvas大小，坐标应该直接匹配
    // 但如果存在DPI缩放，需要处理
    Vector2 uiPoint = windowPoint;
    
    // 注意：如果UI使用了scaleFactor进行缩放，那么鼠标坐标也需要相应缩放
    // 但通常UI应该使用窗口的实际像素大小，所以这里不需要转换
    // 如果将来需要支持UI缩放，可以在这里添加相应的转换逻辑
    
    return uiPoint;
}

UIWidget* UIInputRouter::HitTest(const Vector2& point) const {
    if (!m_widgetTree) {
        return nullptr;
    }
    
    // 将窗口坐标转换为UI坐标
    Vector2 uiPoint = ConvertWindowToUICoordinates(point);
    
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
            // 使用转换后的UI坐标进行碰撞检测
            if (uiPoint.x() >= rect.x && uiPoint.x() <= rect.x + rect.width &&
                uiPoint.y() >= rect.y && uiPoint.y() <= rect.y + rect.height) {
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


