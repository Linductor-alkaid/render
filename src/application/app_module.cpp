#include "render/application/app_module.h"

#include "render/ecs/world.h"

namespace Render::Application {

int AppModule::Priority(ModulePhase) const {
    return 0;
}

void AppModule::OnUnregister(ECS::World&, AppContext&) {}

void AppModule::OnPreFrame(const FrameUpdateArgs&, AppContext&) {}

void AppModule::OnPostFrame(const FrameUpdateArgs&, AppContext&) {}

} // namespace Render::Application


