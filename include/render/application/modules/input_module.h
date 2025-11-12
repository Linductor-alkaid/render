#pragma once

#include <memory>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "render/application/app_module.h"

union SDL_Event;

namespace Render::Application {

class InputModule final : public AppModule {
public:
    InputModule();
    ~InputModule() override;

    std::string_view Name() const override { return "InputModule"; }
    ModuleDependencies Dependencies() const override { return {"CoreRenderModule"}; }
    int Priority(ModulePhase phase) const override;

    void OnRegister(ECS::World& world, AppContext& ctx) override;
    void OnUnregister(ECS::World& world, AppContext& ctx) override;

    void OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx) override;

    bool IsKeyDown(int scancode) const;
    bool WasKeyPressed(int scancode) const;
    bool WasKeyReleased(int scancode) const;
    bool WasQuitRequested() const { return m_quitRequested; }

private:
    void ProcessSDLEvent(const SDL_Event& event);
    void BroadcastEvents(AppContext& ctx);
    void ClearTransientStates();

    std::unordered_set<int> m_keysDown;
    std::unordered_set<int> m_keysPressed;
    std::unordered_set<int> m_keysReleased;
    bool m_quitRequested = false;
    bool m_registered = false;
};

} // namespace Render::Application


