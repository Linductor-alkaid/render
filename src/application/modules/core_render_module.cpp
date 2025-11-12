#include "render/application/modules/core_render_module.h"

#include "render/application/app_context.h"
#include "render/application/event_bus.h"
#include "render/application/events/frame_events.h"
#include "render/async_resource_loader.h"
#include "render/ecs/world.h"
#include "render/logger.h"

namespace Render::Application {

int CoreRenderModule::Priority(ModulePhase phase) const {
    switch (phase) {
        case ModulePhase::PreFrame:
            return 100;
        case ModulePhase::PostFrame:
            return 0;
        case ModulePhase::Register:
        default:
            return 0;
    }
}

void CoreRenderModule::OnRegister(ECS::World& world, AppContext& ctx) {
    if (!world.IsInitialized()) {
        world.Initialize();
    }
    if (!ctx.uniformManager || !ctx.renderer) {
        Logger::GetInstance().Warning("[CoreRenderModule] Missing renderer/uniform manager.");
    }
    Logger::GetInstance().Info("[CoreRenderModule] Registered");
    m_registered = true;
    m_loggedAsyncLoaderMissing = false;
}

void CoreRenderModule::OnUnregister(ECS::World&, AppContext&) {
    Logger::GetInstance().Info("[CoreRenderModule] Unregistered");
    m_registered = false;
    m_loggedAsyncLoaderMissing = false;
}

void CoreRenderModule::OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx) {
    if (!m_registered) {
        return;
    }
    Logger::GetInstance().DebugFormat(
        "[CoreRenderModule] OnPreFrame frame=%llu dt=%.4f abs=%.2f",
        static_cast<unsigned long long>(frame.frameIndex),
        frame.deltaTime,
        frame.absoluteTime);
    if (ctx.globalEventBus) {
        FrameBeginEvent beginEvent{};
        beginEvent.frame = frame;
        ctx.globalEventBus->Publish(beginEvent);

        FrameTickEvent tickEvent{};
        tickEvent.frame = frame;
        ctx.globalEventBus->Publish(tickEvent);
    }

    if (ctx.asyncLoader) {
        if (ctx.asyncLoader->IsInitialized()) {
            size_t processed = ctx.asyncLoader->ProcessCompletedTasks(m_asyncTasksPerFrame);
            if (processed > 0) {
                Logger::GetInstance().DebugFormat(
                    "[CoreRenderModule] ProcessCompletedTasks processed=%zu",
                    processed);
            }
        } else if (!m_loggedAsyncLoaderMissing) {
            Logger::GetInstance().Warning("[CoreRenderModule] AsyncResourceLoader not initialized; async tasks will be skipped.");
            m_loggedAsyncLoaderMissing = true;
        }
    } else if (!m_loggedAsyncLoaderMissing) {
        Logger::GetInstance().Warning("[CoreRenderModule] AppContext.asyncLoader is null; async tasks will be skipped.");
        m_loggedAsyncLoaderMissing = true;
    }

    ctx.lastFrame = frame;
}

void CoreRenderModule::OnPostFrame(const FrameUpdateArgs& frame, AppContext& ctx) {
    if (!m_registered) {
        return;
    }

    if (ctx.globalEventBus) {
        FrameEndEvent endEvent{};
        endEvent.frame = frame;
        ctx.globalEventBus->Publish(endEvent);
    }
}

} // namespace Render::Application


