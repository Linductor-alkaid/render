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
    // 正交投影矩阵参数：left, right, bottom, top, near, far
    // 对于UI坐标系统（Y向下为正，原点在左上角）：left=0, right=width, bottom=height, top=0
    // 注意：虽然top < bottom看起来不符合常规，但这是为了匹配窗口坐标系统（Y向下为正）
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
            
            // 根据offset.x判断对齐方式：-1.0f = Center, -2.0f = Right, 其他 = Left
            // 这是一个临时方案，更好的方法是在UITextCommand中添加alignment字段
            TextAlignment alignment = TextAlignment::Left;
            if (cmd.text.offset.x() < -1.5f) {
                alignment = TextAlignment::Right;
            } else if (cmd.text.offset.x() < -0.5f) {
                alignment = TextAlignment::Center;
            } else {
                alignment = TextAlignment::Left;
            }
            text->SetAlignment(alignment);
            
            text->EnsureUpdated();
            
            // 获取文本大小
            Vector2 textSize = text->GetSize();
            
            // TextRenderer的Draw方法会根据对齐方式调整anchor，然后添加offset = textSize/2
            // 对于不同的对齐方式（假设传入position = P，文本大小为S）：
            // - Left:   anchor = (P.x, P.y), 最终位置 = (P.x + S.x/2, P.y + S.y/2) (文本中心)
            // - Center: anchor = (P.x - S.x/2, P.y), 最终位置 = (P.x, P.y + S.y/2) (文本中心)
            // - Right:  anchor = (P.x - S.x, P.y), 最终位置 = (P.x - S.x/2, P.y + S.y/2) (文本中心)
            // 
            // 关键理解：TextRenderer的最终位置总是文本中心，不管对齐方式如何
            // - Left对齐：如果想左上角在P，传入P，文本中心会在P + S/2
            // - Center对齐：如果想中心在P，传入P，文本中心会在(P.x, P.y + S.y/2) ❌ Y轴有偏差！
            // - Right对齐：如果想右上角在P，传入P，文本中心会在(P.x - S.x/2, P.y + S.y/2)
            //
            // 所以对于Center对齐，如果UI代码传递的是期望的中心位置，需要补偿Y轴的offset
            Vector3 pos = transform->GetPosition();
            
            // 根据对齐方式调整位置
            // 对于Center对齐，虽然按钮文本在BuildCommands中已经补偿了Y轴offset
            // 但由于tempText和实际text对象可能有差异，这里再次补偿以确保准确性
            if (alignment == TextAlignment::Center) {
                // 对于Center对齐，UI代码传递的位置已经补偿了Y轴offset
                // 但为了保险起见，如果发现位置是按钮中心位置，可以再次微调
                // 或者完全依赖BuildCommands中的补偿，这里不做额外处理
            }
            
            // offset.y用于额外的Y轴偏移
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
        // 添加成功创建的日志（仅第一次）
        static bool loggedSuccess = false;
        if (!loggedSuccess) {
            Logger::GetInstance().Info("[UIRendererBridge] Solid UI texture created successfully.");
            loggedSuccess = true;
        }
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
    
    // 预先创建光标纹理，确保光标可以渲染
    EnsureSolidTexture();

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
                // 面板文本位置：左上角偏移，使用Left对齐
                textCmd.transform->SetPosition(Vector3(position.x() + 24.0f,
                                                       position.y() + 24.0f,
                                                       static_cast<float>(-depth) * 0.001f));
                textCmd.text = "UI Panel";
                textCmd.font = m_defaultFont;
                textCmd.color = Color(0.9f, 0.9f, 0.9f, 1.0f);
                // offset.x = 0表示Left对齐（默认）
                textCmd.offset = Vector2::Zero();
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
            // 使用主题系统获取颜色
            const UITheme* theme = nullptr;
            if (m_themeManager) {
                theme = &m_themeManager->GetCurrentTheme();
            }
            
            const UIThemeColorSet& colorSet = theme 
                ? theme->GetWidgetColorSet("button", 
                                          buttonWidget->IsHovered(), 
                                          buttonWidget->IsPressed(), 
                                          !buttonWidget->IsEnabled(), 
                                          false)
                : UITheme::CreateDefault().GetWidgetColorSet("button", 
                                                            buttonWidget->IsHovered(), 
                                                            buttonWidget->IsPressed(), 
                                                            !buttonWidget->IsEnabled(), 
                                                            false);
            
            Color buttonTint = colorSet.inner;
            pushSprite("button_base", buttonTint);

            if (m_defaultFont) {
                std::string label = buttonWidget->GetLabel();
                if (label.empty()) {
                    label = "Button";
                }
                
                // 先创建临时Text对象获取文本大小，用于精确计算位置
                auto tempText = CreateRef<Text>(m_defaultFont);
                tempText->SetString(label);
                tempText->SetAlignment(TextAlignment::Center);
                tempText->EnsureUpdated();
                Vector2 textSize = tempText->GetSize();
                
                UITextCommand textCmd;
                textCmd.transform = CreateRef<Transform>();
                // 按钮文本位置：居中在按钮内
                // TextRenderer的Center对齐：最终位置 = (position.x, position.y + textSize.y/2)
                // 如果想让文本中心在(centerX, centerY)，需要传入(centerX, centerY - textSize.y/2)
                float centerX = position.x() + size.x() * 0.5f;
                float centerY = position.y() + size.y() * 0.5f;
                // 提前补偿Y轴offset，这样Flush中就不需要再补偿了
                float textY = centerY - textSize.y() * 0.5f;
                textCmd.transform->SetPosition(Vector3(centerX,
                                                       textY,
                                                       static_cast<float>(-depth) * 0.001f));
                textCmd.text = label;
                textCmd.font = m_defaultFont;
                // 使用主题系统获取文本颜色
                textCmd.color = colorSet.text;
                // 使用offset.x作为对齐标记：-1.0f = Center对齐
                // 由于已经提前补偿了Y轴，Flush中不需要再补偿
                textCmd.offset = Vector2(-1.0f, 0.0f);
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
            // 使用主题系统获取颜色
            const UITheme* theme = nullptr;
            if (m_themeManager) {
                theme = &m_themeManager->GetCurrentTheme();
            }
            
            const bool enabled = textFieldWidget->IsEnabled();
            const UIThemeColorSet& colorSet = theme
                ? theme->GetWidgetColorSet("textField",
                                          false,  // hover
                                          false,  // pressed
                                          !enabled,
                                          textFieldWidget->IsFocused())
                : UITheme::CreateDefault().GetWidgetColorSet("textField",
                                                            false,
                                                            false,
                                                            !enabled,
                                                            textFieldWidget->IsFocused());
            
            Color fieldTint = colorSet.inner;
            pushSprite("button_base", fieldTint);

            const std::string& actualText = textFieldWidget->GetText();
            std::string drawText = actualText;
            bool isFocused = textFieldWidget->IsFocused();
            // 占位符逻辑：只有在未获得焦点且文本为空时才显示占位符
            bool isPlaceholder = drawText.empty() && !isFocused;
            if (isPlaceholder) {
                drawText = textFieldWidget->GetPlaceholder();
            }

            // 使用主题系统获取文本颜色
            Color textColor = colorSet.text;
            if (isPlaceholder) {
                // 占位符使用较淡的颜色
                textColor = Color(colorSet.text.r * 0.7f, 
                                 colorSet.text.g * 0.7f, 
                                 colorSet.text.b * 0.7f, 
                                 colorSet.text.a * 0.7f);
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
                // offsets 的大小应该等于可能的插入位置数量
                // 对于 N 个字符，offsets 有 N+1 个元素（包括开始和结束位置）
                // 所以 caretPositions 也应该有 offsets.size() 个元素
                caretPositions.reserve(offsets.size());
                const char* textPtr = actualText.c_str();
                
                // 为每个偏移量计算光标位置
                // 第一个位置总是 0.0f（文本开始）
                for (size_t i = 0; i < offsets.size(); ++i) {
                    size_t offset = offsets[i];
                    if (offset == 0) {
                        // 起始位置
                        caretPositions.push_back(0.0f);
                    } else {
                        // 测量从开始到 offset 的文本宽度
                        int measuredWidth = 0;
                        size_t measuredLength = 0;
                        if (m_defaultFont->MeasureString(textPtr, offset, std::numeric_limits<int>::max(), measuredWidth, measuredLength)) {
                            caretPositions.push_back(static_cast<float>(measuredWidth));
                        } else if (!caretPositions.empty()) {
                            // 如果测量失败，使用最后一个位置
                            caretPositions.push_back(caretPositions.back());
                        } else {
                            // 如果还没有任何位置，使用 0
                            caretPositions.push_back(0.0f);
                        }
                    }
                }
                
                // 确保至少有一个位置（即使文本为空）
                if (caretPositions.empty()) {
                    caretPositions.push_back(0.0f);
                }
                
                textHeight = std::max(textHeightRaw, 1.0f);
            } else {
                // 没有字体时，至少提供一个位置
                caretPositions = {0.0f};
            }

            // 光标高度应该使用文本高度，而不是整个内容区域的高度
            // contentHeight 是整个widget的内容高度，太大了
            float caretHeight = textHeight;
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
                // 文本输入框的文本位置：左上角对齐
                // TextRenderer的Left对齐会将position作为左上角，所以直接传入左上角位置
                textCmd.transform->SetPosition(Vector3(textStartX,
                                                       textStartY,
                                                       static_cast<float>(-depth) * 0.001f));
                textCmd.text = drawText;
                textCmd.font = m_defaultFont;
                textCmd.color = textColor;
                // offset.x = 0表示Left对齐（默认）
                textCmd.offset = Vector2::Zero();
                textCmd.layerID = 800;
                textCmd.depth = static_cast<float>(depth);
                m_commandBuffer.AddText(textCmd);
            }

            // 在文本之后渲染光标，确保光标在文本之上（后渲染的会覆盖先渲染的）
            // 注意：isFocused 变量已在上面定义（第538行）
            if (isFocused) {
                // 计算光标位置
                size_t caretIndex = textFieldWidget->GetCaretIndex();
                float caretOffset = 0.0f;
                
                // 确保 caretPositions 不为空（应该总是至少有一个元素）
                if (caretPositions.empty()) {
                    caretPositions.push_back(0.0f);
                }
                
                // 确保索引在有效范围内
                size_t clampedIndex = std::min(caretIndex, caretPositions.size() - 1);
                caretOffset = caretPositions[clampedIndex];
                
                // 确保光标纹理已创建
                EnsureSolidTexture();
                
                if (m_solidTexture && m_solidTexture->IsValid()) {
                    UISpriteCommand caretCmd;
                    caretCmd.transform = CreateRef<Transform>();
                    // 光标位置：文本起始位置 + 光标偏移
                    // 文本的左上角位置是 (textStartX, textStartY)
                    // 光标应该与文本的基线对齐，所以Y坐标与文本Y坐标相同
                    float caretX = textStartX + caretOffset;
                    float caretY = textStartY; // 与文本的左上角Y坐标对齐
                    
                    // 光标尺寸
                    float caretWidth = std::max(UITextField::kCaretWidth, 3.0f);
                    float finalCaretHeight = std::min(textHeight, 100.0f);
                    finalCaretHeight = std::max(finalCaretHeight, 16.0f);
                    
                    // 使用与文本完全相同的深度值和Z坐标计算方式
                    // 文本使用：depth = static_cast<float>(depth)，Z = static_cast<float>(-depth) * 0.001f
                    float textDepth = static_cast<float>(depth);
                    // 光标使用相同的深度值，但通过渲染顺序确保在文本之上
                    caretCmd.transform->SetPosition(Vector3(caretX, caretY, static_cast<float>(-textDepth) * 0.001f));
                    caretCmd.texture = m_solidTexture;
                    caretCmd.sourceRect = Rect(0.0f, 0.0f, 1.0f, 1.0f);
                    caretCmd.size = Vector2(caretWidth, finalCaretHeight);
                    caretCmd.tint = Color(0.0f, 1.0f, 1.0f, 1.0f); // 青色，更明显
                    caretCmd.layerID = 800;
                    caretCmd.depth = textDepth; // 使用与文本相同的深度值
                    m_commandBuffer.AddSprite(caretCmd);
                    
                    Logger::GetInstance().InfoFormat(
                        "[UIRendererBridge] Rendering caret for %s: index=%zu offset=%.1f pos=(%.1f,%.1f) size=(%.1f,%.1f) depth=%.3f",
                        textFieldWidget->GetId().c_str(),
                        caretIndex,
                        caretOffset,
                        caretX,
                        caretY,
                        caretWidth,
                        finalCaretHeight,
                        textDepth);
                }
            }

            // 诊断信息：输出焦点状态
            Logger::GetInstance().InfoFormat(
                "[UIRendererBridge] TextField %s: IsFocused=%s",
                textFieldWidget->GetId().c_str(),
                isFocused ? "true" : "false");

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


