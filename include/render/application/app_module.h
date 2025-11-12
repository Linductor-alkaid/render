#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "render/application/app_context.h"

namespace Render {

namespace ECS {
class World;
}

namespace Application {

enum class ModulePhase {
    Register,
    PreFrame,
    PostFrame,
};

using ModuleDependencies = std::vector<std::string>;

class AppModule {
public:
    virtual ~AppModule() = default;

    virtual std::string_view Name() const = 0;
    virtual ModuleDependencies Dependencies() const { return {}; }

    virtual int Priority(ModulePhase phase) const;

    virtual void OnRegister(ECS::World& world, AppContext& ctx) = 0;
    virtual void OnUnregister(ECS::World& world, AppContext& ctx);

    virtual void OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx);
    virtual void OnPostFrame(const FrameUpdateArgs& frame, AppContext& ctx);
};

} // namespace Application
} // namespace Render


