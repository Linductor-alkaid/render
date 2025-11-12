#include "render/ui/ui_renderer_bridge.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include "render/application/app_context.h"
#include "render/logger.h"
#include "render/math_utils.h"
#include "render/render_state.h"
#include "render/resource_manager.h"
#include "render/renderer.h"
#include "render/text/text_renderer.h"
#include "render/ui/uicanvas.h"
#include "render/ui/ui_widget.h"
#include "render/ui/ui_widget_tree.h"
#include "render/ui/widgets/ui_button.h"
#include "render/ui/widgets/ui_text_field.h"
#include "render/uniform_manager.h"

namespace Render::UI {

namespace {
constexpr const char* kUniformViewport = "uUiViewport";
constexpr const char* kUniformScale = "uUiScale";
constexpr const char* kUniformDpi = "uUiDpiScale";
constexpr const char* kUniformTime = "uUiTime";
constexpr const char* kUniformDeltaTime = "uUiDeltaTime";
constexpr const char* kUniformFocus = "uUiFocus";
constexpr const char* kUniformCursor = "uUiCursor";
constexpr const char* kDefaultFontName = "ui.default";
constexpr const char* kDefaultFontPath = "assets/fonts/NotoSansSC-Regular.ttf";
constexpr float kDefaultFontSize = 28.0f;

void LogWidgetRecursive(const UIWidget& widget, int depth, size_t& totalCount, size_t& visibleCount) {
    ++totalCount;
    const bool visible = widget.IsVisible();
    if (visible) {
        ++visibleCount;
    }

    std::string indent(static_cast<size_t>(depth) * 2, ' ');
    Logger::GetInstance().DebugFormat("[UIRendererBridge] %s- %s (%s)", indent.c_str(),
                                      widget.GetId().c_str(), visible ? "visible" : "hidden");

    widget.ForEachChild([&](const UIWidget& child) {
        LogWidgetRecursive(child, depth + 1, totalCount, visibleCount);
    });
}
} // namespace

void UIRendererBridge::Initialize(Render::Application::AppContext& ctx) {
    if (m_initialized) {
        return;
    }

    if (!ctx.uniformManager) {
        Logger::GetInstance().Warning("[UIRendererBridge] UniformManager unavailable, per-frame UI uniforms will be skipped.");
        m_initialized = true;
        return;
    }

    m_initialized = true;
}

void UIRendererBridge::Shutdown(Render::Application::AppContext&) {
    if (!m_initialized) {
        return;
    }

    m_initialized = false;
}

void UIRendererBridge::PrepareFrame(const Render::Application::FrameUpdateArgs& frame,
                                    UICanvas& canvas,
                                    UIWidgetTree& tree,
                                    Render::Application::AppContext& ctx) {
    if (!m_initialized) {
        Initialize(ctx);
    }

    if (!m_initialized) {
        return;
    }

    UploadPerFrameUniforms(frame, canvas, ctx);
}

void UIRendererBridge::Flush(const Render::Application::FrameUpdateArgs&,
                             UICanvas& canvas,
                             UIWidgetTree& tree,
                             Render::Application::AppContext& ctx) {
    if (!m_initialized) {
        return;
    }

    const UIWidget* root = tree.GetRoot();
    if (!root) {
        Logger::GetInstance().Debug("[UIRendererBridge] Widget tree is empty.");
        return;
    }

    EnsureAtlas(ctx);
    BuildCommands(canvas, tree, ctx);

    size_t totalCount = 0;
    size_t visibleCount = 0;
    LogWidgetRecursive(*root, 0, totalCount, visibleCount);
    Logger::GetInstance().InfoFormat("[UIRendererBridge] Frame widget summary: total=%zu, visible=%zu",
                                     totalCount, visibleCount);

    const auto& commands = m_commandBuffer.GetCommands();
    if (commands.empty()) {
        return;
    }

    if (!ctx.renderer) {
        Logger::GetInstance().Warning("[UIRendererBridge] Renderer is null, cannot submit UI sprites.");
        return;
    }

    Vector2 canvasSize = canvas.GetState().WindowSize();
    if (canvasSize.x() <= 0.0f || canvasSize.y() <= 0.0f) {
        canvasSize = Vector2(1280.0f, 720.0f);
    }

    Matrix4 view = Matrix4::Identity();
    Matrix4 projection = MathUtils::Orthographic(0.0f,
                                                 canvasSize.x(),
                                                 canvasSize.y(),
                                                 0.0f,
                                                 -1.0f,
                                                 1.0f);

    EnsureTextResources(ctx);

    TextRenderer textRenderer(ctx.renderer);
    bool hasTextCommands = false;

    for (const auto& cmd : commands) {
        switch (cmd.type) {
        case UIRenderCommandType::Sprite: {
            if (!cmd.sprite.texture) {
                continue;
            }
            SpriteRenderable sprite;
            sprite.SetTransform(cmd.sprite.transform);
            sprite.SetLayerID(cmd.sprite.layerID);
            sprite.SetTexture(cmd.sprite.texture);
            sprite.SetSourceRect(cmd.sprite.sourceRect);
            sprite.SetSize(cmd.sprite.size);
            sprite.SetTintColor(cmd.sprite.tint);
            sprite.SetViewProjectionOverride(view, projection);
            sprite.SubmitToRenderer(ctx.renderer);
            break;
        }
        case UIRenderCommandType::Text: {
            const auto& transform = cmd.text.transform;
            if (!transform) {
                break;
            }
            Ref<Render::Font> font = cmd.text.font ? cmd.text.font : m_defaultFont;
            if (!font) {
                if (!m_loggedMissingFont) {
                    Logger::GetInstance().Warning("[UIRendererBridge] No font available for text command.");
                    m_loggedMissingFont = true;
                }
                break;
            }
            if (!hasTextCommands) {
                textRenderer.Begin();
                hasTextCommands = true;
            }
            auto text = CreateRef<Text>(font);
            text->SetString(cmd.text.text);
            text->SetColor(cmd.text.color);
            Vector3 pos = transform->GetPosition();
            pos.x() += cmd.text.offset.x();
            pos.y() += cmd.text.offset.y();
            pos.z() = static_cast<float>(-cmd.text.depth) * 0.001f;
            textRenderer.Draw(text, pos);
            break;
        }
        case UIRenderCommandType::DebugRect:
            DrawDebugRect(cmd.debugRect, projection, ctx);
            break;
        default:
            break;
        }
    }

    if (hasTextCommands) {
        textRenderer.End();
    }
}

void UIRendererBridge::UploadPerFrameUniforms(const Render::Application::FrameUpdateArgs&,
                                              UICanvas& canvas,
                                              Render::Application::AppContext& ctx) {
    if (!ctx.uniformManager) {
        return;
    }

    auto* uniforms = ctx.uniformManager;
    const auto& state = canvas.GetState();

    if (uniforms->HasUniform(kUniformViewport)) {
        uniforms->SetVector2(kUniformViewport, state.WindowSize());
    }

    if (uniforms->HasUniform(kUniformScale)) {
        uniforms->SetFloat(kUniformScale, state.scaleFactor);
    }

    if (uniforms->HasUniform(kUniformDpi)) {
        uniforms->SetFloat(kUniformDpi, state.dpiScale);
    }

    if (uniforms->HasUniform(kUniformTime)) {
        uniforms->SetFloat(kUniformTime, state.absoluteTime);
    }

    if (uniforms->HasUniform(kUniformDeltaTime)) {
        uniforms->SetFloat(kUniformDeltaTime, state.deltaTime);
    }

    if (uniforms->HasUniform(kUniformFocus)) {
        uniforms->SetBool(kUniformFocus, state.hasFocus);
    }

    if (uniforms->HasUniform(kUniformCursor)) {
        uniforms->SetVector2(kUniformCursor, state.cursorPosition);
    }
}

void UIRendererBridge::EnsureAtlas(Render::Application::AppContext& ctx) {
    if (m_uiAtlas) {
        return;
    }

    if (!ctx.resourceManager) {
        if (!m_loggedMissingAtlas) {
            Logger::GetInstance().Warning("[UIRendererBridge] ResourceManager unavailable, cannot load UI atlas.");
            m_loggedMissingAtlas = true;
        }
        return;
    }

    auto atlas = ctx.resourceManager->GetSpriteAtlas("ui_core");
    if (!atlas) {
        if (!m_loggedMissingAtlas) {
            Logger::GetInstance().Warning("[UIRendererBridge] Sprite atlas 'ui_core' not found. UI elements will not render.");
            m_loggedMissingAtlas = true;
        }
        return;
    }

    m_uiAtlas = atlas;
}

void UIRendererBridge::EnsureTextResources(Render::Application::AppContext& ctx) {
    if (m_defaultFont) {
        return;
    }

    if (!ctx.resourceManager) {
        if (!m_loggedMissingFont) {
            Logger::GetInstance().Warning("[UIRendererBridge] ResourceManager unavailable, cannot load default UI font.");
            m_loggedMissingFont = true;
        }
        return;
    }

    auto font = ctx.resourceManager->GetFont(kDefaultFontName);
    if (!font) {
        auto newFont = CreateRef<Render::Font>();
        if (!newFont->LoadFromFile(kDefaultFontPath, kDefaultFontSize)) {
            if (!m_loggedMissingFont) {
                Logger::GetInstance().WarningFormat(
                    "[UIRendererBridge] Failed to load default font from '%s'. UI text will be skipped.",
                    kDefaultFontPath);
                m_loggedMissingFont = true;
            }
            return;
        }

        if (!ctx.resourceManager->RegisterFont(kDefaultFontName, newFont)) {
            if (!m_loggedMissingFont) {
                Logger::GetInstance().WarningFormat(
                    "[UIRendererBridge] Unable to register default font '%s'. UI text will be skipped.",
                    kDefaultFontName);
                m_loggedMissingFont = true;
            }
            return;
        }

        font = std::move(newFont);
    }

    if (!font->IsValid()) {
        if (!m_loggedMissingFont) {
            Logger::GetInstance().WarningFormat(
                "[UIRendererBridge] Default font '%s' is invalid. UI text will be skipped.", kDefaultFontName);
            m_loggedMissingFont = true;
        }
        return;
    }

    m_defaultFont = font;
    m_loggedMissingFont = false;
}

void UIRendererBridge::EnsureDebugTexture() {
    if (m_debugTexture && m_debugTexture->IsValid()) {
        return;
    }

    if (!m_debugConfig || !m_debugConfig->drawDebugRects) {
        return;
    }

    if (!m_debugTexture) {
        m_debugTexture = CreateRef<Render::Texture>();
    }

    const uint32_t pixel = 0xFFFFFFFFu;
    if (!m_debugTexture->CreateFromData(&pixel, 1, 1, Render::TextureFormat::RGBA, false)) {
        if (!m_loggedDebugRectShader) {
            Logger::GetInstance().Warning("[UIRendererBridge] Failed to create debug texture.");
            m_loggedDebugRectShader = true;
        }
        m_debugTexture.reset();
    }
}

void UIRendererBridge::EnsureSolidTexture() {
    if (m_solidTexture && m_solidTexture->IsValid()) {
        return;
    }

    if (!m_solidTexture) {
        m_solidTexture = CreateRef<Render::Texture>();
    }

    const uint32_t pixel = 0xFFFFFFFFu;
    if (!m_solidTexture->CreateFromData(&pixel, 1, 1, Render::TextureFormat::RGBA, false)) {
        if (!m_loggedSolidTexture) {
            Logger::GetInstance().Warning("[UIRendererBridge] Failed to create solid UI texture.");
            m_loggedSolidTexture = true;
        }
        m_solidTexture.reset();
    } else {
        m_loggedSolidTexture = false;
    }
}

void UIRendererBridge::DrawDebugRect(const UIDebugRectCommand& cmd,
                                     const Matrix4& projection,
                                     Render::Application::AppContext& ctx) {
    if (!m_debugConfig || !m_debugConfig->drawDebugRects) {
        return;
    }
    if (!ctx.renderer) {
        return;
    }

    EnsureDebugTexture();
    if (!m_debugTexture || !m_debugTexture->IsValid()) {
        if (!m_loggedDebugRectShader) {
            Logger::GetInstance().Warning("[UIRendererBridge] Debug texture unavailable, skipping debug rect rendering.");
            m_loggedDebugRectShader = true;
        }
        return;
    }

    auto drawEdge = [&](float x, float y, float width, float height) {
        auto transform = CreateRef<Transform>();
        transform->SetPosition(Vector3(x, y, static_cast<float>(-cmd.depth) * 0.001f));

        SpriteRenderable sprite;
        sprite.SetTransform(transform);
        sprite.SetLayerID(cmd.layerID);
        sprite.SetTexture(m_debugTexture);
        sprite.SetSourceRect(Rect(0.0f, 0.0f, 1.0f, 1.0f));
        sprite.SetSize(Vector2(std::max(width, 1.0f), std::max(height, 1.0f)));
        sprite.SetTintColor(cmd.color);
        sprite.SetViewProjectionOverride(Matrix4::Identity(), projection);
        sprite.SubmitToRenderer(ctx.renderer);
    };

    const float x = cmd.rect.x;
    const float y = cmd.rect.y;
    const float width = cmd.rect.width;
    const float height = cmd.rect.height;
    const float thickness = std::max(cmd.thickness, 1.0f);

    drawEdge(x, y, width, thickness);                         // bottom
    drawEdge(x, y + height - thickness, width, thickness);    // top
    drawEdge(x, y, thickness, height);                        // left
    drawEdge(x + width - thickness, y, thickness, height);    // right
}

void UIRendererBridge::BuildCommands(UICanvas& canvas,
                                     UIWidgetTree& tree,
                                     Render::Application::AppContext& ctx) {
    m_commandBuffer.Clear();

    EnsureTextResources(ctx);

    if (!m_uiAtlas || !ctx.renderer) {
        return;
    }

    const UIWidget* root = tree.GetRoot();
    if (!root) {
        return;
    }

    std::function<void(const UIWidget&, int)> build = [&](const UIWidget& widget, int depth) {
        const std::string& id = widget.GetId();
        const Rect& rect = widget.GetLayoutRect();
        Vector2 position(rect.x, rect.y);
        Vector2 size(rect.width, rect.height);

        auto pushSprite = [&](const std::string& frameName, const Color& tint = Color::White()) {
            if (!m_uiAtlas->HasFrame(frameName)) {
                if (!m_loggedMissingAtlas) {
                    Logger::GetInstance().WarningFormat("[UIRendererBridge] Frame '%s' missing in atlas.", frameName.c_str());
                    m_loggedMissingAtlas = true;
                }
                return;
            }

            const auto& frame = m_uiAtlas->GetFrame(frameName);

            UISpriteCommand cmd;
            cmd.transform = CreateRef<Transform>();
            cmd.transform->SetPosition(Vector3(position.x(), position.y(), static_cast<float>(-depth) * 0.001f));
            cmd.texture = m_uiAtlas->GetTexture();
            cmd.sourceRect = frame.uv;
            cmd.tint = tint;
            cmd.layerID = 800;
            cmd.depth = static_cast<float>(depth);

            Vector2 finalSize = size;
            if (finalSize.x() <= 0.0f || finalSize.y() <= 0.0f) {
                finalSize = frame.size;
            }
            cmd.size = finalSize;

            m_commandBuffer.AddSprite(cmd);
        };

        if (id == "ui.panel") {
            pushSprite("panel_background");
            if (m_defaultFont) {
                UITextCommand textCmd;
                textCmd.transform = CreateRef<Transform>();
                textCmd.transform->SetPosition(Vector3(position.x() + 24.0f,
                                                       position.y() + 24.0f,
                                                       static_cast<float>(-depth) * 0.001f));
                textCmd.text = "UI Panel";
                textCmd.font = m_defaultFont;
                textCmd.color = Color(0.9f, 0.9f, 0.9f, 1.0f);
                textCmd.layerID = 800;
                textCmd.depth = static_cast<float>(depth);
                m_commandBuffer.AddText(textCmd);
            }
            if (m_debugConfig && m_debugConfig->drawDebugRects) {
                UIDebugRectCommand rectCmd;
                rectCmd.rect = Rect(position.x(), position.y(), size.x(), size.y());
                rectCmd.color = Color(0.0f, 0.75f, 1.0f, 0.4f);
                rectCmd.thickness = 2.0f;
                rectCmd.layerID = 999;
                rectCmd.depth = static_cast<float>(depth);
                m_commandBuffer.AddDebugRect(rectCmd);
            }
        } else if (const auto* buttonWidget = dynamic_cast<const UIButton*>(&widget)) {
            Color buttonTint(0.94f, 0.94f, 0.98f, 1.0f);
            if (buttonWidget->IsPressed()) {
                buttonTint = Color(0.82f, 0.85f, 0.95f, 1.0f);
            } else if (buttonWidget->IsHovered()) {
                buttonTint = Color(0.98f, 0.98f, 1.0f, 1.0f);
            }
            pushSprite("button_base", buttonTint);

            if (m_defaultFont) {
                std::string label = buttonWidget->GetLabel();
                if (label.empty()) {
                    label = "Button";
                }
                UITextCommand textCmd;
                textCmd.transform = CreateRef<Transform>();
                textCmd.transform->SetPosition(Vector3(position.x() + 24.0f,
                                                       position.y() + 24.0f,
                                                       static_cast<float>(-depth) * 0.001f));
                textCmd.text = label;
                textCmd.font = m_defaultFont;
                textCmd.color = buttonWidget->IsPressed() ? Color(0.15f, 0.15f, 0.15f, 1.0f)
                                                          : Color(0.2f, 0.2f, 0.2f, 1.0f);
                textCmd.layerID = 800;
                textCmd.depth = static_cast<float>(depth);
                m_commandBuffer.AddText(textCmd);
            }
            if (m_debugConfig && m_debugConfig->drawDebugRects) {
                UIDebugRectCommand rectCmd;
                rectCmd.rect = Rect(position.x(), position.y(), size.x(), size.y());
                rectCmd.color = Color(1.0f, 0.5f, 0.0f, 0.4f);
                rectCmd.thickness = 2.0f;
                rectCmd.layerID = 999;
                rectCmd.depth = static_cast<float>(depth);
                m_commandBuffer.AddDebugRect(rectCmd);
            }
        } else if (const auto* textFieldWidget = dynamic_cast<const UITextField*>(&widget)) {
            const bool enabled = textFieldWidget->IsEnabled();
            Color fieldTint = textFieldWidget->IsFocused() ? Color(0.18f, 0.2f, 0.3f, 1.0f)
                                                           : Color(0.14f, 0.16f, 0.22f, 1.0f);
            if (!enabled) {
                fieldTint = Color(0.12f, 0.12f, 0.14f, 1.0f);
            }
            pushSprite("button_base", fieldTint);

            const std::string& actualText = textFieldWidget->GetText();
            std::string drawText = actualText;
            bool isPlaceholder = drawText.empty();
            if (isPlaceholder) {
                drawText = textFieldWidget->GetPlaceholder();
            }

            Color textColor = enabled ? Color(0.88f, 0.88f, 0.92f, 1.0f) : Color(0.56f, 0.56f, 0.6f, 1.0f);
            if (isPlaceholder) {
                textColor = Color(0.6f, 0.6f, 0.65f, 1.0f);
            }

            const float textStartX = position.x() + UITextField::kPaddingLeft;
            const float contentWidth = std::max(size.x() - (UITextField::kPaddingLeft + UITextField::kPaddingRight), 0.0f);
            const float contentHeight = std::max(size.y() - (UITextField::kPaddingTop + UITextField::kPaddingBottom), 0.0f);
            float textHeightRaw = 24.0f;
            if (m_defaultFont) {
                int lineSkip = m_defaultFont->GetLineSkip();
                int fontHeight = m_defaultFont->GetHeight();
                if (lineSkip > 0 || fontHeight > 0) {
                    textHeightRaw = static_cast<float>(std::max(lineSkip, fontHeight));
                }
            }

            std::vector<float> caretPositions;
            float textHeight = textHeightRaw;

            if (m_defaultFont) {
                const auto& offsets = textFieldWidget->GetCodepointOffsets();
                caretPositions.reserve(offsets.size());
                const char* textPtr = actualText.c_str();
                for (size_t offset : offsets) {
                    int measuredWidth = 0;
                    size_t measuredLength = 0;
                    if (m_defaultFont->MeasureString(textPtr, offset, std::numeric_limits<int>::max(), measuredWidth, measuredLength)) {
                        caretPositions.push_back(static_cast<float>(measuredWidth));
                    } else if (!caretPositions.empty()) {
                        caretPositions.push_back(caretPositions.back());
                    } else {
                        caretPositions.push_back(0.0f);
                    }
                }
                if (caretPositions.empty()) {
                    caretPositions.push_back(0.0f);
                }
                textHeight = std::max(textHeightRaw, 1.0f);
            } else {
                caretPositions = {0.0f};
            }

            float caretHeight = contentHeight > 0.0f ? contentHeight : textHeight;
            caretHeight = std::max(caretHeight, 1.0f);
            const float highlightTop = position.y() + UITextField::kPaddingTop;
            const float textStartY = highlightTop;

            if (!caretPositions.empty()) {
                auto* mutableField = const_cast<UITextField*>(textFieldWidget);
                mutableField->UpdateCaretMetrics(caretPositions, caretHeight);
            }

            if (!actualText.empty() && textFieldWidget->HasSelection()) {
                auto [startIndex, endIndex] = textFieldWidget->GetSelectionIndices();
                if (startIndex < caretPositions.size() && endIndex <= caretPositions.size()) {
                    const float x0 = textStartX + caretPositions[startIndex];
                    const float x1 = textStartX + caretPositions[endIndex];
                    const float selectionWidth = std::max(x1 - x0, 0.0f);
                    if (selectionWidth > 0.5f) {
                        EnsureSolidTexture();
                        if (m_solidTexture) {
                            UISpriteCommand selectionCmd;
                            selectionCmd.transform = CreateRef<Transform>();
                            selectionCmd.transform->SetPosition(Vector3(x0,
                                                                        highlightTop,
                                                                        static_cast<float>(-depth) * 0.001f));
                            selectionCmd.texture = m_solidTexture;
                            selectionCmd.sourceRect = Rect(0.0f, 0.0f, 1.0f, 1.0f);
                            selectionCmd.size = Vector2(selectionWidth, caretHeight);
                            selectionCmd.tint = Color(0.28f, 0.44f, 0.78f, 0.45f);
                            selectionCmd.layerID = 800;
                            selectionCmd.depth = static_cast<float>(depth) - 0.1f;
                            m_commandBuffer.AddSprite(selectionCmd);
                        }
                    }
                }
            }

            if (m_defaultFont && !drawText.empty()) {
                UITextCommand textCmd;
                textCmd.transform = CreateRef<Transform>();
                textCmd.transform->SetPosition(Vector3(textStartX,
                                                       textStartY,
                                                       static_cast<float>(-depth) * 0.001f));
                textCmd.text = drawText;
                textCmd.font = m_defaultFont;
                textCmd.color = textColor;
                textCmd.offset = Vector2(0.0f, -4.0f);
                textCmd.layerID = 800;
                textCmd.depth = static_cast<float>(depth);
                m_commandBuffer.AddText(textCmd);
            }

            if (textFieldWidget->IsFocused()) {
                double absoluteTime = canvas.GetState().absoluteTime;
                constexpr double kBlinkPeriod = 0.55;
                const double phase = std::fmod(std::max(absoluteTime, 0.0), kBlinkPeriod * 2.0);
                if (phase < kBlinkPeriod) {
                    size_t caretIndex = textFieldWidget->GetCaretIndex();
                    float caretOffset = 0.0f;
                    if (!caretPositions.empty()) {
                        size_t clampedIndex = std::min(caretIndex, caretPositions.size() - 1);
                        caretOffset = caretPositions[clampedIndex];
                    }
                    EnsureSolidTexture();
                    if (m_solidTexture) {
                        UISpriteCommand caretCmd;
                        caretCmd.transform = CreateRef<Transform>();
                        caretCmd.transform->SetPosition(Vector3(textStartX + caretOffset,
                                                                highlightTop,
                                                                static_cast<float>(-depth) * 0.001f));
                        caretCmd.texture = m_solidTexture;
                        caretCmd.sourceRect = Rect(0.0f, 0.0f, 1.0f, 1.0f);
                        caretCmd.size = Vector2(std::max(UITextField::kCaretWidth, 1.0f), caretHeight);
                        caretCmd.tint = Color(0.96f, 0.96f, 1.0f, 1.0f);
                        caretCmd.layerID = 800;
                        caretCmd.depth = static_cast<float>(depth) - 0.05f;
                        m_commandBuffer.AddSprite(caretCmd);
                    }
                }
            }

            if (m_debugConfig && m_debugConfig->drawDebugRects) {
                UIDebugRectCommand rectCmd;
                rectCmd.rect = Rect(position.x(), position.y(), size.x(), size.y());
                rectCmd.color = Color(0.4f, 0.8f, 0.4f, 0.35f);
                rectCmd.thickness = 2.0f;
                rectCmd.layerID = 999;
                rectCmd.depth = static_cast<float>(depth);
                m_commandBuffer.AddDebugRect(rectCmd);
            }
        }

        widget.ForEachChild([&](const UIWidget& child) {
            build(child, depth + 1);
        });
    };

    build(*root, 0);
}

} // namespace Render::UI


