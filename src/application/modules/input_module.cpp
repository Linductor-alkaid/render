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

        if (router) {
            switch (event.type) {
        case SDL_EVENT_QUIT:
            m_quitRequested = true;
            break;
                case SDL_EVENT_MOUSE_MOTION: {
                    Render::Vector2 position(static_cast<float>(event.motion.x),
                                             static_cast<float>(event.motion.y));
                    Render::Vector2 delta(static_cast<float>(event.motion.xrel),
                                          static_cast<float>(event.motion.yrel));
                    router->QueueMouseMove(position, delta);
                    break;
                }
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP: {
                    Render::Vector2 position(static_cast<float>(event.button.x),
                                             static_cast<float>(event.button.y));
                    bool pressed = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                    router->QueueMouseButton(event.button.button, pressed, position);
                    break;
                }
                case SDL_EVENT_MOUSE_WHEEL: {
                    Render::Vector2 offset(static_cast<float>(event.wheel.x),
                                           static_cast<float>(event.wheel.y));
                    router->QueueMouseWheel(offset, false);
                    break;
                }
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP: {
                    bool pressed = (event.type == SDL_EVENT_KEY_DOWN);
                    bool repeat = pressed && event.key.repeat != 0;
                    router->QueueKey(static_cast<int>(event.key.scancode), pressed, repeat);
                    break;
                }
                case SDL_EVENT_TEXT_INPUT: {
                    router->QueueTextInput(event.text.text ? event.text.text : "");
                    break;
                }
                default:
                    break;
            }
        }

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
            }
            break;
        case SDL_EVENT_KEY_UP: {
            int scancode = static_cast<int>(event.key.scancode);
            m_keysDown.erase(scancode);
            m_keysReleased.insert(scancode);
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

} // namespace Render::Application


