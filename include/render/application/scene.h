#pragma once

#include <functional>
#include <memory>
#include <string_view>

#include "render/application/app_context.h"
#include "render/application/scene_types.h"

namespace Render {

namespace ECS {
class World;
}

namespace Application {

class ModuleRegistry;

class Scene {
public:
    virtual ~Scene() = default;

    virtual std::string_view Name() const = 0;

    virtual void OnAttach(AppContext& ctx, ModuleRegistry& modules) = 0;
    virtual void OnDetach(AppContext& ctx) = 0;

    virtual SceneResourceManifest BuildManifest() const = 0;

    virtual void OnEnter(const SceneEnterArgs& args) = 0;
    virtual void OnUpdate(const FrameUpdateArgs& frame) = 0;
    virtual SceneSnapshot OnExit(const SceneExitArgs& args) = 0;

    virtual bool WantsOverlay() const { return false; }
    virtual SceneFlags DefaultFlags() const { return SceneFlags::None; }
};

using ScenePtr = std::unique_ptr<Scene>;
using SceneFactory = std::function<ScenePtr()>;

} // namespace Application
} // namespace Render


