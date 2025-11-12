#pragma once

#include <memory>
#include <string_view>

#include "render/application/app_module.h"

namespace Render {
class Renderer;
class RenderState;
} // namespace Render

namespace Render::Application {

class DebugHUDModule final : public AppModule {
public:
    DebugHUDModule() = default;
    ~DebugHUDModule() override = default;

    std::string_view Name() const override { return "DebugHUDModule"; }
    ModuleDependencies Dependencies() const override { return {"CoreRenderModule"}; }
    int Priority(ModulePhase phase) const override;

    void OnRegister(ECS::World& world, AppContext& ctx) override;
    void OnUnregister(ECS::World& world, AppContext& ctx) override;

    void OnPostFrame(const FrameUpdateArgs& frame, AppContext& ctx) override;

private:
    void DrawHUD(const FrameUpdateArgs& frame, AppContext& ctx);

    bool m_registered = false;
    float m_accumulatedTime = 0.0f;
    uint32_t m_frameCounter = 0;
    float m_smoothedFPS = 0.0f;
};

} // namespace Render::Application


