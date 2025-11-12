#pragma once

#include <memory>
#include <string_view>

#include "render/application/app_module.h"
#include "render/ui/ui_debug_config.h"

namespace Render::UI {
class UICanvas;
struct UILayoutContext;
class UIRendererBridge;
class UIWidgetTree;
class UIInputRouter;
} // namespace Render::UI

namespace Render::Application {

class UIRuntimeModule final : public AppModule {
public:
    UIRuntimeModule();
    ~UIRuntimeModule() override;

    std::string_view Name() const override;
    ModuleDependencies Dependencies() const override;
    int Priority(ModulePhase phase) const override;

    void OnRegister(ECS::World& world, AppContext& ctx) override;
    void OnUnregister(ECS::World& world, AppContext& ctx) override;

    void OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx) override;
    void OnPostFrame(const FrameUpdateArgs& frame, AppContext& ctx) override;

    void SetDebugOptions(const UI::UIDebugConfig& config);

private:
    void EnsureInitialized(AppContext& ctx);
    void EnsureSampleWidgets();
    void Shutdown(AppContext& ctx);

    std::unique_ptr<UI::UICanvas> m_canvas;
    std::unique_ptr<UI::UILayoutContext> m_layoutContext;
    std::unique_ptr<UI::UIRendererBridge> m_rendererBridge;
    std::unique_ptr<UI::UIWidgetTree> m_widgetTree;
    std::unique_ptr<UI::UIInputRouter> m_inputRouter;
    UI::UIDebugConfig m_debugConfig{};
    bool m_registered = false;
};

} // namespace Render::Application


