#include "render/application/modules/ui_runtime_module.h"

#include <memory>

#include "render/logger.h"
#include "render/renderer.h"
#include "render/ui/uicanvas.h"
#include "render/ui/ui_layout.h"
#include "render/ui/ui_input_router.h"
#include "render/ui/ui_renderer_bridge.h"
#include "render/ui/ui_widget.h"
#include "render/ui/ui_widget_tree.h"
#include "render/ui/ui_debug_config.h"
#include "render/ui/widgets/ui_button.h"
#include "render/ui/widgets/ui_text_field.h"

namespace Render::Application {

UIRuntimeModule::UIRuntimeModule() = default;

UIRuntimeModule::~UIRuntimeModule() = default;

std::string_view UIRuntimeModule::Name() const {
    return "UIRuntimeModule";
}

ModuleDependencies UIRuntimeModule::Dependencies() const {
    return {"CoreRenderModule"};
}

int UIRuntimeModule::Priority(ModulePhase phase) const {
    switch (phase) {
    case ModulePhase::PreFrame:
        return 50;
    case ModulePhase::PostFrame:
        return 50;
    default:
        return 0;
    }
}

void UIRuntimeModule::OnRegister(ECS::World&, AppContext& ctx) {
    if (m_registered) {
        return;
    }

    EnsureInitialized(ctx);
    m_registered = true;

    Logger::GetInstance().Info("[UIRuntimeModule] Registered.");
}

void UIRuntimeModule::OnUnregister(ECS::World&, AppContext& ctx) {
    if (!m_registered) {
        return;
    }

    Shutdown(ctx);
    m_registered = false;

    Logger::GetInstance().Info("[UIRuntimeModule] Unregistered.");
}

void UIRuntimeModule::OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx) {
    if (!m_registered) {
        return;
    }

    EnsureInitialized(ctx);
    if (!m_canvas || !m_rendererBridge) {
        return;
    }

    m_canvas->BeginFrame(frame, ctx);
    if (m_widgetTree && !m_widgetTree->GetRoot()) {
        auto root = std::make_unique<UI::UIWidget>("ui.root");
        m_widgetTree->SetRoot(std::move(root));
    }

    if (m_inputRouter) {
        m_inputRouter->SetWidgetTree(m_widgetTree.get());
        m_inputRouter->SetCanvas(m_canvas.get());
        ctx.uiInputRouter = m_inputRouter.get();
    }

    EnsureSampleWidgets();

    if (m_layoutContext) {
        const auto& canvasState = m_canvas->GetState();
        Vector2 canvasSize = canvasState.WindowSize();
        if (canvasSize.x() <= 0.0f || canvasSize.y() <= 0.0f) {
            canvasSize = Vector2(1280.0f, 720.0f);
        }
        UI::UILayoutEngine::SyncTree(*m_widgetTree, canvasSize, *m_layoutContext);
    }

    m_rendererBridge->PrepareFrame(frame, *m_canvas, *m_widgetTree, ctx);
}

void UIRuntimeModule::OnPostFrame(const FrameUpdateArgs& frame, AppContext& ctx) {
    if (!m_registered) {
        return;
    }

    if (!m_canvas || !m_rendererBridge || !m_widgetTree) {
        return;
    }

    m_rendererBridge->Flush(frame, *m_canvas, *m_widgetTree, ctx);
    m_canvas->EndFrame(frame, ctx);

    if (m_inputRouter) {
        m_inputRouter->EndFrame();
    }
}

void UIRuntimeModule::SetDebugOptions(const UI::UIDebugConfig& config) {
    m_debugConfig = config;
    if (m_rendererBridge) {
        m_rendererBridge->SetDebugConfig(&m_debugConfig);
    }
    if (m_inputRouter) {
        m_inputRouter->SetDebugConfig(&m_debugConfig);
    }
}

void UIRuntimeModule::EnsureInitialized(AppContext& ctx) {
    if (!ctx.IsValid()) {
        return;
    }

    if (!m_canvas) {
        m_canvas = std::make_unique<UI::UICanvas>();
        m_canvas->Initialize(ctx);
    }

    if (!m_layoutContext) {
        m_layoutContext = std::make_unique<UI::UILayoutContext>();
    }

    if (!m_rendererBridge) {
        m_rendererBridge = std::make_unique<UI::UIRendererBridge>();
        m_rendererBridge->Initialize(ctx);
        m_rendererBridge->SetDebugConfig(&m_debugConfig);
    }

    if (!m_widgetTree) {
        m_widgetTree = std::make_unique<UI::UIWidgetTree>();
        m_widgetTree->SetRoot(std::make_unique<UI::UIWidget>("ui.root"));
    }

    if (!m_inputRouter) {
        m_inputRouter = std::make_unique<UI::UIInputRouter>();
        m_inputRouter->Initialize(m_widgetTree.get(), m_canvas.get());
    } else {
        m_inputRouter->SetWidgetTree(m_widgetTree.get());
        m_inputRouter->SetCanvas(m_canvas.get());
    }
    SDL_Window* window = nullptr;
    if (ctx.renderer) {
        if (auto context = ctx.renderer->GetContext()) {
            window = context->GetWindow();
        }
    }
    m_inputRouter->SetWindow(window);
    m_inputRouter->SetDebugConfig(&m_debugConfig);

    ctx.uiInputRouter = m_inputRouter.get();
}

void UIRuntimeModule::EnsureSampleWidgets() {
    if (!m_widgetTree) {
        return;
    }

    auto* root = m_widgetTree->GetRoot();
    if (!root) {
        return;
    }

    if (!root->FindById("ui.panel")) {
        auto panel = std::make_unique<UI::UIWidget>("ui.panel");
        panel->SetPreferredSize(Vector2(360.0f, 220.0f));
        panel->SetMinSize(Vector2(200.0f, 140.0f));
        panel->SetPadding(Vector4(24.0f, 24.0f, 24.0f, 24.0f));

        auto button = std::make_unique<UI::UIButton>("ui.panel.button");
        button->SetPreferredSize(Vector2(220.0f, 56.0f));
        button->SetMinSize(Vector2(140.0f, 40.0f));
        button->SetLabel("Submit");
        button->SetOnClicked([](UI::UIButton& btn) {
            Logger::GetInstance().InfoFormat("[UIRuntimeModule] Button '%s' clicked.", btn.GetId().c_str());
        });

        auto textField = std::make_unique<UI::UITextField>("ui.panel.input");
        textField->SetPreferredSize(Vector2(220.0f, 64.0f));
        textField->SetMinSize(Vector2(140.0f, 48.0f));
        textField->SetPlaceholder("Type here...");
        textField->SetOnTextChanged([](UI::UITextField& field, const std::string& value) {
            Logger::GetInstance().InfoFormat("[UIRuntimeModule] TextField '%s' text=\"%s\"",
                                             field.GetId().c_str(),
                                             value.c_str());
        });

        panel->AddChild(std::move(button));
        panel->AddChild(std::move(textField));
        root->AddChild(std::move(panel));
    }
}

void UIRuntimeModule::Shutdown(AppContext& ctx) {
    if (m_rendererBridge) {
        m_rendererBridge->Shutdown(ctx);
        m_rendererBridge.reset();
    }

    if (m_canvas) {
        m_canvas->Shutdown(ctx);
        m_canvas.reset();
    }

    if (m_layoutContext) {
        m_layoutContext.reset();
    }

    if (m_widgetTree) {
        m_widgetTree.reset();
    }

    if (m_inputRouter) {
        m_inputRouter->Shutdown();
        m_inputRouter.reset();
    }

    ctx.uiInputRouter = nullptr;
}

} // namespace Render::Application


