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
#include "render/ui/ui_theme.h"
#include "render/ui/widgets/ui_button.h"
#include "render/ui/widgets/ui_text_field.h"
#include "render/ui/widgets/ui_checkbox.h"
#include "render/ui/widgets/ui_toggle.h"
#include "render/ui/widgets/ui_slider.h"
#include "render/ui/widgets/ui_radio_button.h"
#include "render/ui/widgets/ui_color_picker.h"
#include "render/ui/widgets/ui_toggle.h"

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
    
    // 更新所有 UIToggle 的动画
    if (m_widgetTree && m_widgetTree->GetRoot()) {
        UpdateToggleAnimations(*m_widgetTree->GetRoot(), frame.deltaTime);
    }

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
        
        // 初始化主题管理器并设置到渲染桥接器
        auto& themeManager = UI::UIThemeManager::GetInstance();
        themeManager.InitializeDefaults();
        m_rendererBridge->SetThemeManager(&themeManager);
    }

    if (!m_widgetTree) {
        m_widgetTree = std::make_unique<UI::UIWidgetTree>();
        m_widgetTree->SetRoot(std::make_unique<UI::UIWidget>("ui.root"));
    }

    if (!m_inputRouter) {
        m_inputRouter = std::make_unique<UI::UIInputRouter>();
        m_inputRouter->Initialize(m_widgetTree.get(), m_canvas.get(), ctx.globalEventBus);
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
        // 创建主面板，使用 Flex 布局（垂直方向）
        auto panel = std::make_unique<UI::UIWidget>("ui.panel");
        panel->SetPreferredSize(Vector2(800.0f, 680.0f)); // 增大尺寸以容纳所有控件
        panel->SetMinSize(Vector2(400.0f, 300.0f));
        panel->SetPadding(Vector4(20.0f, 20.0f, 20.0f, 20.0f)); // 减小 padding 以节省空间
        panel->SetLayoutMode(UI::UILayoutMode::Flex);  // 明确设置布局模式
        panel->SetLayoutDirection(UI::UILayoutDirection::Vertical);
        panel->SetJustifyContent(UI::UIFlexJustifyContent::FlexStart);
        panel->SetAlignItems(UI::UIFlexAlignItems::Stretch);
        panel->SetSpacing(12.0f); // 减小间距

        // 创建标题区域（Flex 布局 - 水平）
        auto titleRow = std::make_unique<UI::UIWidget>("ui.panel.titleRow");
        titleRow->SetPreferredSize(Vector2(0.0f, 48.0f));
        titleRow->SetMinSize(Vector2(200.0f, 40.0f));
        titleRow->SetLayoutMode(UI::UILayoutMode::Flex);
        titleRow->SetLayoutDirection(UI::UILayoutDirection::Horizontal);
        titleRow->SetJustifyContent(UI::UIFlexJustifyContent::Center);
        titleRow->SetAlignItems(UI::UIFlexAlignItems::Center);
        titleRow->SetSpacing(8.0f);

        // 创建文本输入框
        auto textField = std::make_unique<UI::UITextField>("ui.panel.input");
        textField->SetPreferredSize(Vector2(0.0f, 48.0f)); // 减小高度
        textField->SetMinSize(Vector2(200.0f, 40.0f));
        textField->SetFlexGrow(0.0f); // 不拉伸
        textField->SetPlaceholder("Type here...");
        textField->SetOnTextChanged([](UI::UITextField& field, const std::string& value) {
            Logger::GetInstance().InfoFormat("[UIRuntimeModule] TextField '%s' text=\"%s\"",
                                             field.GetId().c_str(),
                                             value.c_str());
        });

        // 创建 Grid 布局演示区域
        auto gridDemo = std::make_unique<UI::UIWidget>("ui.panel.gridDemo");
        gridDemo->SetPreferredSize(Vector2(0.0f, 120.0f)); // 减小高度以节省空间
        gridDemo->SetMinSize(Vector2(200.0f, 100.0f));
        gridDemo->SetLayoutMode(UI::UILayoutMode::Grid);  // 使用 Grid 布局
        gridDemo->SetGridColumns(3);  // 3列
        gridDemo->SetGridRows(2);     // 2行
        gridDemo->SetGridCellSpacing(Vector2(6.0f, 6.0f)); // 减小间距
        gridDemo->SetPadding(Vector4(6.0f, 6.0f, 6.0f, 6.0f));

        // 创建 Grid 中的按钮
        for (int i = 0; i < 6; ++i) {
            auto gridButton = std::make_unique<UI::UIButton>("ui.panel.gridButton" + std::to_string(i));
            gridButton->SetPreferredSize(Vector2(0.0f, 0.0f)); // 填充网格单元格
            gridButton->SetMinSize(Vector2(60.0f, 40.0f));
            gridButton->SetLabel("Grid " + std::to_string(i + 1));
            gridButton->SetOnClicked([](UI::UIButton& btn) {
                Logger::GetInstance().InfoFormat("[UIRuntimeModule] Grid Button '%s' clicked.", btn.GetId().c_str());
            });
            gridDemo->AddChild(std::move(gridButton));
        }

        // 创建水平布局容器（用于按钮行）
        auto buttonRow = std::make_unique<UI::UIWidget>("ui.panel.buttonRow");
        buttonRow->SetPreferredSize(Vector2(0.0f, 40.0f)); // 减小高度
        buttonRow->SetMinSize(Vector2(200.0f, 36.0f));
        buttonRow->SetLayoutMode(UI::UILayoutMode::Flex);
        buttonRow->SetLayoutDirection(UI::UILayoutDirection::Horizontal);
        buttonRow->SetJustifyContent(UI::UIFlexJustifyContent::SpaceEvenly);
        buttonRow->SetAlignItems(UI::UIFlexAlignItems::Center);
        buttonRow->SetSpacing(12.0f);

        auto button1 = std::make_unique<UI::UIButton>("ui.panel.button1");
        button1->SetPreferredSize(Vector2(120.0f, 36.0f)); // 减小尺寸
        button1->SetMinSize(Vector2(100.0f, 32.0f));
        button1->SetLabel("Submit");
        button1->SetOnClicked([](UI::UIButton& btn) {
            Logger::GetInstance().InfoFormat("[UIRuntimeModule] Button '%s' clicked.", btn.GetId().c_str());
        });

        auto button2 = std::make_unique<UI::UIButton>("ui.panel.button2");
        button2->SetPreferredSize(Vector2(120.0f, 36.0f)); // 减小尺寸
        button2->SetMinSize(Vector2(100.0f, 32.0f));
        button2->SetLabel("Cancel");
        button2->SetOnClicked([](UI::UIButton& btn) {
            Logger::GetInstance().InfoFormat("[UIRuntimeModule] Button '%s' clicked.", btn.GetId().c_str());
        });

        buttonRow->AddChild(std::move(button1));
        buttonRow->AddChild(std::move(button2));

        // 创建高级控件演示区域 - 使用水平布局减少高度
        auto advancedControlsContainer = std::make_unique<UI::UIWidget>("ui.panel.advancedControlsContainer");
        advancedControlsContainer->SetPreferredSize(Vector2(0.0f, 0.0f)); // 自适应高度
        advancedControlsContainer->SetLayoutMode(UI::UILayoutMode::Flex);
        advancedControlsContainer->SetLayoutDirection(UI::UILayoutDirection::Horizontal); // 改为水平布局
        advancedControlsContainer->SetJustifyContent(UI::UIFlexJustifyContent::SpaceEvenly);
        advancedControlsContainer->SetAlignItems(UI::UIFlexAlignItems::FlexStart);
        advancedControlsContainer->SetSpacing(16.0f);
        
        // 左列：CheckBox, Toggle, Slider, RadioButtons
        auto advancedControlsCol1 = std::make_unique<UI::UIWidget>("ui.panel.advancedControls1");
        advancedControlsCol1->SetPreferredSize(Vector2(0.0f, 0.0f)); // 自适应
        advancedControlsCol1->SetFlexGrow(1.0f); // 平分空间
        advancedControlsCol1->SetLayoutMode(UI::UILayoutMode::Flex);
        advancedControlsCol1->SetLayoutDirection(UI::UILayoutDirection::Vertical);
        advancedControlsCol1->SetJustifyContent(UI::UIFlexJustifyContent::FlexStart);
        advancedControlsCol1->SetAlignItems(UI::UIFlexAlignItems::Stretch);
        advancedControlsCol1->SetSpacing(8.0f);
        
        // 右列：ColorPicker
        auto advancedControlsCol2 = std::make_unique<UI::UIWidget>("ui.panel.advancedControls2");
        advancedControlsCol2->SetPreferredSize(Vector2(0.0f, 0.0f)); // 自适应
        advancedControlsCol2->SetFlexGrow(1.0f); // 平分空间
        advancedControlsCol2->SetLayoutMode(UI::UILayoutMode::Flex);
        advancedControlsCol2->SetLayoutDirection(UI::UILayoutDirection::Vertical);
        advancedControlsCol2->SetJustifyContent(UI::UIFlexJustifyContent::FlexStart);
        advancedControlsCol2->SetAlignItems(UI::UIFlexAlignItems::Stretch);
        advancedControlsCol2->SetSpacing(8.0f);

        // Checkbox 演示
        auto checkbox = std::make_unique<UI::UICheckBox>("ui.panel.checkbox");
        checkbox->SetLabel("Enable Feature");
        checkbox->SetState(UI::UICheckBox::State::Checked);
        checkbox->SetOnChanged([](UI::UICheckBox& cb, bool checked) {
            Logger::GetInstance().InfoFormat("[UIRuntimeModule] CheckBox '%s' changed to %s",
                                            cb.GetId().c_str(), checked ? "checked" : "unchecked");
        });

        // Toggle 演示
        auto toggle = std::make_unique<UI::UIToggle>("ui.panel.toggle");
        toggle->SetLabel("Dark Mode");
        toggle->SetToggled(false); // 初始状态为关闭
        toggle->SetOnChanged([](UI::UIToggle& tg, bool toggled) {
            Logger::GetInstance().InfoFormat("[UIRuntimeModule] Toggle '%s' changed to %s",
                                            tg.GetId().c_str(), toggled ? "on" : "off");
        });

        // Slider 演示
        auto slider = std::make_unique<UI::UISlider>("ui.panel.slider");
        slider->SetLabel("Volume");
        slider->SetMinValue(0.0f);
        slider->SetMaxValue(100.0f);
        slider->SetValue(50.0f);
        slider->SetStep(1.0f);
        slider->SetShowValue(true);
        slider->SetOnChanged([](UI::UISlider& sl, float value) {
            Logger::GetInstance().InfoFormat("[UIRuntimeModule] Slider '%s' value changed to %.2f",
                                            sl.GetId().c_str(), value);
        });

        // RadioButton 组演示（创建组并保存为成员变量）
        if (!m_sampleRadioGroup) {
            m_sampleRadioGroup = std::make_unique<UI::UIRadioButtonGroup>();
        }
        auto radio1 = std::make_unique<UI::UIRadioButton>("ui.panel.radio1");
        radio1->SetLabel("Option 1");
        radio1->SetGroup(m_sampleRadioGroup.get());
        radio1->SetSelected(true);
        
        auto radio2 = std::make_unique<UI::UIRadioButton>("ui.panel.radio2");
        radio2->SetLabel("Option 2");
        radio2->SetGroup(m_sampleRadioGroup.get());
        
        auto radio3 = std::make_unique<UI::UIRadioButton>("ui.panel.radio3");
        radio3->SetLabel("Option 3");
        radio3->SetGroup(m_sampleRadioGroup.get());

        // ColorPicker 演示 - 调整尺寸
        auto colorPicker = std::make_unique<UI::UIColorPicker>("ui.panel.colorPicker");
        colorPicker->SetColor(Color(0.2f, 0.6f, 0.9f, 1.0f));
        colorPicker->SetShowPreview(true);
        colorPicker->SetShowAlpha(false);
        colorPicker->SetPreferredSize(Vector2(0.0f, 120.0f)); // 宽度自适应，高度固定
        colorPicker->SetOnChanged([](UI::UIColorPicker& cp, const Color& color) {
            Logger::GetInstance().InfoFormat("[UIRuntimeModule] ColorPicker '%s' color changed to (%.2f, %.2f, %.2f, %.2f)",
                                            cp.GetId().c_str(), color.r, color.g, color.b, color.a);
        });

        // 左列添加控件（小控件）
        advancedControlsCol1->AddChild(std::move(checkbox));
        advancedControlsCol1->AddChild(std::move(toggle));
        advancedControlsCol1->AddChild(std::move(slider));
        advancedControlsCol1->AddChild(std::move(radio1));
        advancedControlsCol1->AddChild(std::move(radio2));
        advancedControlsCol1->AddChild(std::move(radio3));
        
        // 右列添加颜色选择器
        advancedControlsCol2->AddChild(std::move(colorPicker));
        
        // 将两列添加到容器
        advancedControlsContainer->AddChild(std::move(advancedControlsCol1));
        advancedControlsContainer->AddChild(std::move(advancedControlsCol2));

        // 组装 UI 树
        panel->AddChild(std::move(textField));
        panel->AddChild(std::move(gridDemo));
        panel->AddChild(std::move(buttonRow));
        panel->AddChild(std::move(advancedControlsContainer));
        root->AddChild(std::move(panel));
    }
}

void UIRuntimeModule::UpdateToggleAnimations(UI::UIWidget& widget, float deltaTime) {
    if (auto* toggle = dynamic_cast<UI::UIToggle*>(&widget)) {
        toggle->UpdateAnimation(deltaTime);
    }
    
    widget.ForEachChild([&](UI::UIWidget& child) {
        UpdateToggleAnimations(child, deltaTime);
    });
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


