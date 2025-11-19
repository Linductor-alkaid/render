#pragma once

#include <string_view>
#include <vector>

#include "render/application/app_module.h"
#include "render/application/events/frame_events.h"

namespace Render {
namespace ECS {
class System;
} // namespace ECS
} // namespace Render

namespace Render::Application {

class CoreRenderModule final : public AppModule {
public:
    CoreRenderModule() = default;
    ~CoreRenderModule() override = default;

    std::string_view Name() const override { return "CoreRenderModule"; }
    ModuleDependencies Dependencies() const override { return {}; }

    int Priority(ModulePhase phase) const override;

    void OnRegister(ECS::World& world, AppContext& ctx) override;
    void OnUnregister(ECS::World& world, AppContext& ctx) override;

    void OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx) override;
    void OnPostFrame(const FrameUpdateArgs& frame, AppContext& ctx) override;

private:
    void RegisterCoreSystems(ECS::World& world, AppContext& ctx);
    void RegisterCoreComponents(ECS::World& world, AppContext& ctx);

    bool m_registered = false;
    size_t m_asyncTasksPerFrame = 32;
    bool m_loggedAsyncLoaderMissing = false;
    bool m_systemsRegistered = false;
};

} // namespace Render::Application


