#include "render/application/modules/debug_hud_module.h"

#include "render/application/app_context.h"
#include "render/application/event_bus.h"
#include "render/logger.h"
#include "render/renderer.h"
#include "render/render_state.h"

namespace Render::Application {

int DebugHUDModule::Priority(ModulePhase phase) const {
    switch (phase) {
        case ModulePhase::PostFrame:
            return -50;
        default:
            return 0;
    }
}

void DebugHUDModule::OnRegister(ECS::World&, AppContext&) {
    m_registered = true;
    m_accumulatedTime = 0.0f;
    m_frameCounter = 0;
    m_smoothedFPS = 0.0f;
    Logger::GetInstance().Info("[DebugHUDModule] Registered");
}

void DebugHUDModule::OnUnregister(ECS::World&, AppContext&) {
    if (!m_registered) {
        return;
    }
    m_registered = false;
    Logger::GetInstance().Info("[DebugHUDModule] Unregistered");
}

void DebugHUDModule::OnPostFrame(const FrameUpdateArgs& frame, AppContext& ctx) {
    if (!m_registered || !ctx.renderer) {
        return;
    }

    m_accumulatedTime += frame.deltaTime;
    ++m_frameCounter;
    if (m_accumulatedTime >= 0.5f) {
        const float fps = static_cast<float>(m_frameCounter) / m_accumulatedTime;
        const float smoothing = 0.1f;
        m_smoothedFPS = (1.0f - smoothing) * m_smoothedFPS + smoothing * fps;
        m_accumulatedTime = 0.0f;
        m_frameCounter = 0;
    }

    DrawHUD(frame, ctx);
}

void DebugHUDModule::DrawHUD(const FrameUpdateArgs&, AppContext& ctx) {
    auto stats = ctx.renderer->GetStats();
    static bool loggedOnce = false;
    if (!loggedOnce) {
        Logger::GetInstance().Info("[DebugHUD] Debug HUD active");
        loggedOnce = true;
    }

    Logger::GetInstance().DebugFormat("[DebugHUD] FPS: %.2f frameTime: %.2fms drawCalls: %u batches: %u",
                                      m_smoothedFPS,
                                      stats.frameTime,
                                      stats.drawCalls,
                                      stats.batchCount);
}

} // namespace Render::Application


