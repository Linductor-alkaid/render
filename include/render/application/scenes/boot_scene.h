#pragma once

#include <string_view>

#include "render/application/event_bus.h"
#include "render/application/scene.h"
#include "render/application/scene_graph.h"

namespace Render {

namespace Application {

class BootScene final : public Scene {
public:
    BootScene() = default;
    ~BootScene() override = default;

    std::string_view Name() const override { return "BootScene"; }

    void OnAttach(AppContext& ctx, ModuleRegistry& modules) override;
    void OnDetach(AppContext& ctx) override;

    SceneResourceManifest BuildManifest() const override;

    void OnEnter(const SceneEnterArgs& args) override;
    void OnUpdate(const FrameUpdateArgs& frame) override;
    SceneSnapshot OnExit(const SceneExitArgs& args) override;

private:
    AppContext* m_context = nullptr;
    EventBus::ListenerId m_frameListener = 0;
    SceneGraph m_sceneGraph;

    void SubscribeFrameEvents();
    void UnsubscribeFrameEvents();
};

} // namespace Application
} // namespace Render


