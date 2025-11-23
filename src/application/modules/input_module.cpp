#include "render/application/modules/input_module.h"

#include <SDL3/SDL.h>

#include "render/application/app_context.h"
#include "render/application/event_bus.h"
#include "render/application/events/input_events.h"
#include "render/logger.h"
#include "render/ui/ui_input_router.h"
#include "render/types.h"

namespace Render::Application {

InputModule::InputModule() = default;
InputModule::~InputModule() = default;

int InputModule::Priority(ModulePhase phase) const {
    switch (phase) {
        case ModulePhase::PreFrame:
            return 200;
        default:
            return 0;
    }
}

void InputModule::OnRegister(ECS::World&, AppContext& ctx) {
    if (m_registered) {
        return;
    }
    m_registered = true;
    Logger::GetInstance().Info("[InputModule] Registered");
    ClearTransientStates();
}

void InputModule::OnUnregister(ECS::World&, AppContext&) {
    if (!m_registered) {
        return;
    }
    m_registered = false;
    Logger::GetInstance().Info("[InputModule] Unregistered");
    m_keysDown.clear();
    m_keysPressed.clear();
    m_keysReleased.clear();
}

void InputModule::OnPreFrame(const FrameUpdateArgs&, AppContext& ctx) {
    if (!m_registered) {
        return;
    }

    ClearTransientStates();
    m_quitRequested = false;

    UI::UIInputRouter* router = ctx.uiInputRouter;
    if (router) {
        router->BeginFrame();
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ProcessSDLEvent(event);
        
        // 处理Blender风格操作
        ProcessBlenderOperations(event, ctx);
        ProcessMouseGesture(event, ctx);

        // 处理退出事件
        if (event.type == SDL_EVENT_QUIT) {
            m_quitRequested = true;
        }
        
        // 注意：不再直接调用 router->QueueXXX()，因为 UIInputRouter 现在通过 EventBus 订阅事件
        // 这样可以避免重复处理，并实现更好的解耦

        if (ctx.globalEventBus) {
            switch (event.type) {
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP: {
                    Events::KeyEvent keyEvent;
                    keyEvent.scancode = static_cast<int>(event.key.scancode);
                    keyEvent.repeat = event.key.repeat != 0;
                    keyEvent.state = (event.type == SDL_EVENT_KEY_DOWN) ? Events::KeyState::Pressed : Events::KeyState::Released;
                    ctx.globalEventBus->Publish(keyEvent);
                    break;
                }
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP: {
                    Events::MouseButtonEvent mouseEvent;
                    mouseEvent.button = event.button.button;
                    mouseEvent.x = static_cast<int32_t>(event.button.x);
                    mouseEvent.y = static_cast<int32_t>(event.button.y);
                    mouseEvent.state = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
                                           ? Events::MouseButtonState::Pressed
                                           : Events::MouseButtonState::Released;
                    ctx.globalEventBus->Publish(mouseEvent);
                    break;
                }
                case SDL_EVENT_MOUSE_MOTION: {
                    Events::MouseMotionEvent motionEvent;
                    motionEvent.x = static_cast<int32_t>(event.motion.x);
                    motionEvent.y = static_cast<int32_t>(event.motion.y);
                    motionEvent.dx = static_cast<int32_t>(event.motion.xrel);
                    motionEvent.dy = static_cast<int32_t>(event.motion.yrel);
                    ctx.globalEventBus->Publish(motionEvent);
                    break;
                }
                case SDL_EVENT_MOUSE_WHEEL: {
                    Events::MouseWheelEvent wheelEvent;
                    wheelEvent.x = static_cast<int32_t>(event.wheel.x);
                    wheelEvent.y = static_cast<int32_t>(event.wheel.y);
                    wheelEvent.precise = false;
                    ctx.globalEventBus->Publish(wheelEvent);
                    break;
                }
                case SDL_EVENT_TEXT_INPUT: {
                    Events::TextInputEvent textEvent;
                    if (event.text.text) {
                        textEvent.text = event.text.text;
                    }
                    ctx.globalEventBus->Publish(textEvent);
                    break;
                }
                default:
                    break;
            }
        }
    }
}

bool InputModule::IsKeyDown(int scancode) const {
    return m_keysDown.find(scancode) != m_keysDown.end();
}

bool InputModule::WasKeyPressed(int scancode) const {
    return m_keysPressed.find(scancode) != m_keysPressed.end();
}

bool InputModule::WasKeyReleased(int scancode) const {
    return m_keysReleased.find(scancode) != m_keysReleased.end();
}

void InputModule::ProcessSDLEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
            if (event.key.repeat == 0) {
                int scancode = static_cast<int>(event.key.scancode);
                m_keysDown.insert(scancode);
                m_keysPressed.insert(scancode);
                
                // 更新修饰键状态
                uint32_t mods = event.key.mod;
                m_ctrlDown = (mods & SDL_KMOD_CTRL) != 0;
                m_shiftDown = (mods & SDL_KMOD_SHIFT) != 0;
                m_altDown = (mods & SDL_KMOD_ALT) != 0;
            }
            break;
        case SDL_EVENT_KEY_UP: {
            int scancode = static_cast<int>(event.key.scancode);
            m_keysDown.erase(scancode);
            m_keysReleased.insert(scancode);
            
            // 更新修饰键状态
            uint32_t mods = event.key.mod;
            m_ctrlDown = (mods & SDL_KMOD_CTRL) != 0;
            m_shiftDown = (mods & SDL_KMOD_SHIFT) != 0;
            m_altDown = (mods & SDL_KMOD_ALT) != 0;
            break;
        }
        default:
            break;
    }
}

void InputModule::ClearTransientStates() {
    m_keysPressed.clear();
    m_keysReleased.clear();
}

void InputModule::SetShortcutContext(std::string_view contextName) {
    m_operationMapping.SetCurrentContext(contextName);
}

KeyCombo InputModule::GetCurrentKeyCombo() const {
    KeyCombo combo;
    // 获取当前按下的非修饰键
    for (int scancode : m_keysDown) {
        // 跳过修饰键
        if (scancode == SDL_SCANCODE_LCTRL || scancode == SDL_SCANCODE_RCTRL ||
            scancode == SDL_SCANCODE_LSHIFT || scancode == SDL_SCANCODE_RSHIFT ||
            scancode == SDL_SCANCODE_LALT || scancode == SDL_SCANCODE_RALT) {
            continue;
        }
        combo.scancode = scancode;
        break;
    }
    combo.ctrl = m_ctrlDown;
    combo.shift = m_shiftDown;
    combo.alt = m_altDown;
    return combo;
}

void InputModule::ProcessBlenderOperations(const SDL_Event& event, AppContext& ctx) {
    if (!ctx.globalEventBus) {
        return;
    }
    
    if (event.type == SDL_EVENT_KEY_DOWN && event.key.repeat == 0) {
        // 从事件中获取按键组合
        KeyCombo combo;
        combo.scancode = static_cast<int>(event.key.scancode);
        uint32_t mods = event.key.mod;
        combo.ctrl = (mods & SDL_KMOD_CTRL) != 0;
        combo.shift = (mods & SDL_KMOD_SHIFT) != 0;
        combo.alt = (mods & SDL_KMOD_ALT) != 0;
        
        Events::OperationType operation = m_operationMapping.GetOperationFromKey(combo);
        
        if (operation != Events::OperationType::Select) {
            Events::OperationEvent opEvent;
            opEvent.type = operation;
            opEvent.isStart = true;
            
            // 获取当前上下文名称
            auto context = m_operationMapping.GetCurrentContext();
            if (context) {
                opEvent.context = std::string(context->GetName());
            }
            
            ctx.globalEventBus->Publish(opEvent);
        }
    } else if (event.type == SDL_EVENT_KEY_UP) {
        // 从事件中获取按键组合
        KeyCombo combo;
        combo.scancode = static_cast<int>(event.key.scancode);
        uint32_t mods = event.key.mod;
        combo.ctrl = (mods & SDL_KMOD_CTRL) != 0;
        combo.shift = (mods & SDL_KMOD_SHIFT) != 0;
        combo.alt = (mods & SDL_KMOD_ALT) != 0;
        
        Events::OperationType operation = m_operationMapping.GetOperationFromKey(combo);
        
        if (operation != Events::OperationType::Select) {
            Events::OperationEvent opEvent;
            opEvent.type = operation;
            opEvent.isStart = false;
            
            auto context = m_operationMapping.GetCurrentContext();
            if (context) {
                opEvent.context = std::string(context->GetName());
            }
            
            ctx.globalEventBus->Publish(opEvent);
        }
    }
}

void InputModule::ProcessMouseGesture(const SDL_Event& event, AppContext& ctx) {
    if (!ctx.globalEventBus) {
        return;
    }
    
    switch (event.type) {
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            uint32_t button = event.button.button;
            int x = static_cast<int>(event.button.x);
            int y = static_cast<int>(event.button.y);
            
            Events::GestureType gestureType = Events::GestureType::Click;
            
            // 根据按钮和修饰键确定手势类型
            if (button == SDL_BUTTON_MIDDLE) {
                if (m_altDown) {
                    gestureType = Events::GestureType::Rotate;
                } else {
                    gestureType = Events::GestureType::Pan;
                }
            } else if (button == SDL_BUTTON_LEFT) {
                if (m_shiftDown) {
                    gestureType = Events::GestureType::BoxSelect;
                } else {
                    gestureType = Events::GestureType::Drag;
                }
            } else if (button == SDL_BUTTON_RIGHT) {
                gestureType = Events::GestureType::Click;
            }
            
            StartGesture(gestureType, x, y, button, ctx);
            break;
        }
        case SDL_EVENT_MOUSE_MOTION: {
            if (m_gestureState.active) {
                int x = static_cast<int>(event.motion.x);
                int y = static_cast<int>(event.motion.y);
                UpdateGesture(x, y, ctx);
            }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            if (m_gestureState.active) {
                EndGesture(ctx);
            }
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL: {
            // 滚轮缩放
            Events::GestureEvent gestureEvent;
            gestureEvent.type = Events::GestureType::Zoom;
            gestureEvent.isActive = true;
            gestureEvent.ctrl = m_ctrlDown;
            gestureEvent.shift = m_shiftDown;
            gestureEvent.alt = m_altDown;
            gestureEvent.deltaX = static_cast<int32_t>(event.wheel.x);
            gestureEvent.deltaY = static_cast<int32_t>(event.wheel.y);
            ctx.globalEventBus->Publish(gestureEvent);
            break;
        }
        default:
            break;
    }
}

void InputModule::StartGesture(Events::GestureType type, int x, int y, uint32_t button, AppContext& ctx) {
    m_gestureState.type = type;
    m_gestureState.startX = x;
    m_gestureState.startY = y;
    m_gestureState.lastX = x;
    m_gestureState.lastY = y;
    m_gestureState.button = button;
    m_gestureState.active = true;
    
    Events::GestureEvent gestureEvent;
    gestureEvent.type = type;
    gestureEvent.startX = x;
    gestureEvent.startY = y;
    gestureEvent.currentX = x;
    gestureEvent.currentY = y;
    gestureEvent.isActive = true;
    gestureEvent.button = button;
    gestureEvent.ctrl = m_ctrlDown;
    gestureEvent.shift = m_shiftDown;
    gestureEvent.alt = m_altDown;
    
    if (ctx.globalEventBus) {
        ctx.globalEventBus->Publish(gestureEvent);
    }
}

void InputModule::UpdateGesture(int x, int y, AppContext& ctx) {
    m_gestureState.lastX = x;
    m_gestureState.lastY = y;
    
    Events::GestureEvent gestureEvent;
    gestureEvent.type = m_gestureState.type;
    gestureEvent.startX = m_gestureState.startX;
    gestureEvent.startY = m_gestureState.startY;
    gestureEvent.currentX = x;
    gestureEvent.currentY = y;
    gestureEvent.deltaX = x - m_gestureState.startX;
    gestureEvent.deltaY = y - m_gestureState.startY;
    gestureEvent.isActive = true;
    gestureEvent.button = m_gestureState.button;
    gestureEvent.ctrl = m_ctrlDown;
    gestureEvent.shift = m_shiftDown;
    gestureEvent.alt = m_altDown;
    
    if (ctx.globalEventBus) {
        ctx.globalEventBus->Publish(gestureEvent);
    }
}

void InputModule::EndGesture(AppContext& ctx) {
    Events::GestureEvent gestureEvent;
    gestureEvent.type = m_gestureState.type;
    gestureEvent.startX = m_gestureState.startX;
    gestureEvent.startY = m_gestureState.startY;
    gestureEvent.currentX = m_gestureState.lastX;
    gestureEvent.currentY = m_gestureState.lastY;
    gestureEvent.deltaX = m_gestureState.lastX - m_gestureState.startX;
    gestureEvent.deltaY = m_gestureState.lastY - m_gestureState.startY;
    gestureEvent.isActive = false;
    gestureEvent.button = m_gestureState.button;
    gestureEvent.ctrl = m_ctrlDown;
    gestureEvent.shift = m_shiftDown;
    gestureEvent.alt = m_altDown;
    
    if (ctx.globalEventBus) {
        ctx.globalEventBus->Publish(gestureEvent);
    }
    
    m_gestureState.active = false;
}

} // namespace Render::Application


