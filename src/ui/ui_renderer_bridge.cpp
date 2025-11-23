#include "render/ui/ui_renderer_bridge.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include <SDL3/SDL_timer.h>

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

    // 初始化几何图形渲染器
    if (!m_geometryRenderer.Initialize()) {
        Logger::GetInstance().Warning("[UIRendererBridge] Failed to initialize geometry renderer.");
    }

    // 预先创建光标纹理，避免在渲染时频繁检查
    EnsureSolidTexture();

    m_initialized = true;
}

void UIRendererBridge::Shutdown(Render::Application::AppContext&) {
    if (!m_initialized) {
        return;
    }

    m_geometryRenderer.Shutdown();
    m_solidTexture.reset();
    m_solidTextureValid = false;
    m_initialized = false;
}

void UIRendererBridge::PrepareFrame(const Render::Application::FrameUpdateArgs& frame,
                                    UICanvas& canvas,
                                    UIWidgetTree& /*tree*/,
                                    Render::Application::AppContext& ctx) {
    if (!m_initialized) {
        Initialize(ctx);
    }

    if (!m_initialized) {
        return;
    }

    // 重置几何图形渲染器的对象池索引，准备新的一帧
    m_geometryRenderer.ResetSpritePool();

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
    
    // 调试：统计命令类型
    size_t cursorCmdCount = 0;
    size_t atlasCmdCount = 0;
    for (const auto& cmd : commands) {
        if (cmd.type == UIRenderCommandType::Sprite) {
            if (cmd.sprite.isCursor) {
                cursorCmdCount++;
            } else {
                atlasCmdCount++;
            }
        }
    }
    Logger::GetInstance().DebugFormat("[UIRendererBridge] Command stats: cursor=%zu, atlas=%zu, total=%zu",
                                     cursorCmdCount, atlasCmdCount, commands.size());

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
            if (!cmd.sprite.texture || !cmd.sprite.texture->IsValid()) {
                continue;
            }
            
            // 使用isCursor标志位来明确区分光标命令和图集命令
            // 这是最可靠的方法，避免了复杂的纹理比较逻辑
            Ref<Texture> renderTexture;
            
            if (cmd.sprite.isCursor) {
                // 光标命令：必须使用m_solidTexture（1x1纯色纹理）
                // 进行简单的验证，确保纹理有效
                if (!m_solidTexture || !m_solidTexture->IsValid() || 
                    m_solidTexture->GetWidth() != 1 || m_solidTexture->GetHeight() != 1) {
                    // 光标纹理无效，跳过渲染
                    static bool loggedInvalidCursorTexture = false;
                    if (!loggedInvalidCursorTexture) {
                        Logger::GetInstance().Warning(
                            "[UIRendererBridge] Cursor command has invalid solid texture. Skipping render.");
                        loggedInvalidCursorTexture = true;
                    }
                    continue;
                }
                
                // 关键：光标命令必须使用m_solidTexture，忽略命令中的纹理
                // 这样可以确保光标命令永远不会使用图集纹理
                renderTexture = m_solidTexture;
                
                // 安全检查：如果命令中的纹理是图集纹理，这是严重错误
                if (m_uiAtlas && m_uiAtlas->GetTexture() && cmd.sprite.texture == m_uiAtlas->GetTexture()) {
                    static int errorCount = 0;
                    errorCount++;
                    Logger::GetInstance().ErrorFormat(
                        "[UIRendererBridge] CRITICAL #%d: Cursor command has atlas texture! "
                        "Cmd texture ID=%u, Atlas texture ID=%u, Solid texture ID=%u. "
                        "This is a bug in BuildCommands.",
                        errorCount,
                        cmd.sprite.texture ? cmd.sprite.texture->GetID() : 0,
                        m_uiAtlas->GetTexture() ? m_uiAtlas->GetTexture()->GetID() : 0,
                        m_solidTexture ? m_solidTexture->GetID() : 0);
                }
            } else {
                // 图集命令：必须使用m_uiAtlas->GetTexture()（512x512图集纹理）
                if (!m_uiAtlas || !m_uiAtlas->GetTexture() || !m_uiAtlas->GetTexture()->IsValid()) {
                    // 图集无效，跳过渲染
                    static bool loggedInvalidAtlas = false;
                    if (!loggedInvalidAtlas) {
                        Logger::GetInstance().Warning(
                            "[UIRendererBridge] Atlas command has invalid atlas. Skipping render.");
                        loggedInvalidAtlas = true;
                    }
                    continue;
                }
                
                // 关键：图集命令必须使用m_uiAtlas->GetTexture()，忽略命令中的纹理
                // 这样可以确保图集命令永远不会使用光标纹理
                Ref<Texture> atlasTexture = m_uiAtlas->GetTexture();
                renderTexture = atlasTexture;
                
                // 安全检查：如果命令中的纹理是光标纹理，这是严重错误
                if (m_solidTexture && cmd.sprite.texture == m_solidTexture) {
                    static int errorCount = 0;
                    errorCount++;
                    Logger::GetInstance().ErrorFormat(
                        "[UIRendererBridge] CRITICAL #%d: Atlas command has solid texture! "
                        "Cmd texture ID=%u, Atlas texture ID=%u, Solid texture ID=%u. "
                        "This is a bug in BuildCommands.",
                        errorCount,
                        cmd.sprite.texture ? cmd.sprite.texture->GetID() : 0,
                        m_uiAtlas->GetTexture() ? m_uiAtlas->GetTexture()->GetID() : 0,
                        m_solidTexture ? m_solidTexture->GetID() : 0);
                }
            }
            
            // 渲染sprite
            // 调试：记录纹理信息（仅在前几次）
            static int debugRenderCount = 0;
            if (debugRenderCount < 10) {
                Logger::GetInstance().DebugFormat(
                    "[UIRendererBridge] Rendering sprite: isCursor=%s, textureID=%u, size=%dx%d, sourceRect=(%.2f,%.2f,%.2f,%.2f)",
                    cmd.sprite.isCursor ? "true" : "false",
                    renderTexture ? renderTexture->GetID() : 0,
                    renderTexture ? renderTexture->GetWidth() : 0,
                    renderTexture ? renderTexture->GetHeight() : 0,
                    cmd.sprite.sourceRect.x, cmd.sprite.sourceRect.y,
                    cmd.sprite.sourceRect.width, cmd.sprite.sourceRect.height);
                debugRenderCount++;
            }
            
            SpriteRenderable sprite;
            sprite.SetTransform(cmd.sprite.transform);
            sprite.SetLayerID(cmd.sprite.layerID);
            sprite.SetTexture(renderTexture);
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
        case UIRenderCommandType::Line:
            m_geometryRenderer.RenderLine(cmd.line, view, projection, ctx.renderer);
            break;
        case UIRenderCommandType::BezierCurve:
            m_geometryRenderer.RenderBezierCurve(cmd.bezierCurve, view, projection, ctx.renderer);
            break;
        case UIRenderCommandType::Rectangle:
            m_geometryRenderer.RenderRectangle(cmd.rectangle, view, projection, ctx.renderer);
            break;
        case UIRenderCommandType::Circle:
            m_geometryRenderer.RenderCircle(cmd.circle, view, projection, ctx.renderer);
            break;
        case UIRenderCommandType::RoundedRectangle:
            m_geometryRenderer.RenderRoundedRectangle(cmd.roundedRectangle, view, projection, ctx.renderer);
            break;
        case UIRenderCommandType::Polygon:
            m_geometryRenderer.RenderPolygon(cmd.polygon, view, projection, ctx.renderer);
            break;
        default:
            break;
        }
    }

    if (hasTextCommands) {
        textRenderer.End();
    }

    // 注意：这里不调用FlushRenderQueue()，因为：
    // 1. 主循环会在所有模块更新后统一调用FlushRenderQueue()
    // 2. 这样可以确保所有UI元素和其他渲染对象在同一帧内按正确顺序渲染
    // 3. 避免重复调用FlushRenderQueue()导致渲染队列被清空两次，从而引起纹理闪动
    // 
    // 关于对象生命周期：虽然Flush中创建了局部变量（如SpriteRenderable），
    // 但这些对象提交到渲染队列后，主循环的FlushRenderQueue()会在同一帧内处理它们，
    // 所以不会有生命周期问题
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
    // 使用缓存标志避免频繁检查IsValid()，这可能导致纹理状态变化
    if (m_solidTextureValid && m_solidTexture && m_solidTexture->IsValid()) {
        // 简单验证：确保纹理仍然是1x1
        if (m_solidTexture->GetWidth() == 1 && m_solidTexture->GetHeight() == 1) {
            return;  // 纹理仍然有效
        } else {
            // 纹理状态发生了变化，重置它
            static bool loggedTextureStateChange = false;
            if (!loggedTextureStateChange) {
                Logger::GetInstance().WarningFormat(
                    "[UIRendererBridge] Solid texture size changed to %dx%d! Resetting...",
                    m_solidTexture->GetWidth(), m_solidTexture->GetHeight());
                loggedTextureStateChange = true;
            }
            m_solidTexture.reset();
            m_solidTextureValid = false;
        }
    }

    // 如果纹理存在但无效，重置它
    if (m_solidTexture && !m_solidTexture->IsValid()) {
        m_solidTexture.reset();
        m_solidTextureValid = false;
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
        m_solidTextureValid = false;
    } else {
        // 验证纹理确实创建成功且有效
        if (m_solidTexture->IsValid() && 
            m_solidTexture->GetWidth() == 1 && 
            m_solidTexture->GetHeight() == 1) {
            m_loggedSolidTexture = false;
            m_solidTextureValid = true;  // 缓存有效性状态
        } else {
            // 纹理创建失败或尺寸不正确
            if (!m_loggedSolidTexture) {
                Logger::GetInstance().WarningFormat(
                    "[UIRendererBridge] Solid UI texture created but invalid or wrong size: %dx%d (expected 1x1).",
                    m_solidTexture->GetWidth(), m_solidTexture->GetHeight());
                m_loggedSolidTexture = true;
            }
            m_solidTexture.reset();
            m_solidTextureValid = false;
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

            // 将像素坐标转换为归一化 UV 坐标
            // frame.uv 包含像素坐标（x, y, width, height），需要归一化到 [0, 1] 范围
            Rect normalizedUV = frame.uv;
            if (m_uiAtlas->GetTexture()) {
                float texWidth = static_cast<float>(m_uiAtlas->GetTexture()->GetWidth());
                float texHeight = static_cast<float>(m_uiAtlas->GetTexture()->GetHeight());
                if (texWidth > 0.0f && texHeight > 0.0f) {
                    // 如果 UV 值大于1.0，说明是像素坐标，需要归一化
                    if (normalizedUV.x > 1.0f || normalizedUV.y > 1.0f || 
                        normalizedUV.width > 1.0f || normalizedUV.height > 1.0f) {
                        normalizedUV.x /= texWidth;
                        normalizedUV.y /= texHeight;
                        normalizedUV.width /= texWidth;
                        normalizedUV.height /= texHeight;
                    }
                }
            }

            UISpriteCommand cmd;
            cmd.transform = CreateRef<Transform>();
            cmd.transform->SetPosition(Vector3(position.x(), position.y(), static_cast<float>(-depth) * 0.001f));
            cmd.texture = m_uiAtlas->GetTexture();
            cmd.sourceRect = normalizedUV;
            cmd.tint = tint;
            cmd.layerID = 800;
            cmd.depth = static_cast<float>(depth);
            cmd.isCursor = false;  // 明确标识这是图集命令，不是光标命令
            
            // 调试：验证图集命令的纹理确实是图集纹理
            if (m_solidTexture && cmd.texture == m_solidTexture) {
                static bool loggedAtlasCmdWrongTexture = false;
                if (!loggedAtlasCmdWrongTexture) {
                    Logger::GetInstance().ErrorFormat(
                        "[UIRendererBridge] CRITICAL: Atlas command (frame='%s') has solid texture! "
                        "Atlas texture ID=%u, Solid texture ID=%u. This should never happen!",
                        frameName.c_str(),
                        m_uiAtlas->GetTexture() ? m_uiAtlas->GetTexture()->GetID() : 0,
                        m_solidTexture ? m_solidTexture->GetID() : 0);
                    loggedAtlasCmdWrongTexture = true;
                }
            }

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
            
            // 使用圆角矩形渲染按钮背景和边框
            UIRoundedRectangleCommand rectCmd;
            rectCmd.rect = Rect(position.x(), position.y(), size.x(), size.y());
            rectCmd.cornerRadius = 6.0f;  // 圆角半径
            rectCmd.fillColor = colorSet.inner;
            rectCmd.strokeColor = colorSet.outline;
            rectCmd.strokeWidth = 1.0f;
            rectCmd.filled = true;
            rectCmd.stroked = true;
            rectCmd.segments = 8;
            rectCmd.layerID = 800;
            rectCmd.depth = static_cast<float>(depth);
            m_commandBuffer.AddRoundedRectangle(rectCmd);

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
                UIDebugRectCommand debugRectCmd;
                debugRectCmd.rect = Rect(position.x(), position.y(), size.x(), size.y());
                debugRectCmd.color = Color(1.0f, 0.5f, 0.0f, 0.4f);
                debugRectCmd.thickness = 2.0f;
                debugRectCmd.layerID = 999;
                debugRectCmd.depth = static_cast<float>(depth);
                m_commandBuffer.AddDebugRect(debugRectCmd);
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
            
            // 使用圆角矩形渲染文本框背景和边框
            UIRoundedRectangleCommand rectCmd;
            rectCmd.rect = Rect(position.x(), position.y(), size.x(), size.y());
            rectCmd.cornerRadius = 6.0f;  // 圆角半径
            rectCmd.fillColor = colorSet.inner;
            rectCmd.strokeColor = colorSet.outline;
            rectCmd.strokeWidth = 1.0f;
            rectCmd.filled = true;
            rectCmd.stroked = true;
            rectCmd.segments = 8;
            rectCmd.layerID = 800;
            rectCmd.depth = static_cast<float>(depth);
            m_commandBuffer.AddRoundedRectangle(rectCmd);

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
            float baseCaretHeight = textHeight;
            baseCaretHeight = std::max(baseCaretHeight, 1.0f);
            const float highlightTop = position.y() + UITextField::kPaddingTop;
            const float textStartY = highlightTop;

            if (!caretPositions.empty()) {
                auto* mutableField = const_cast<UITextField*>(textFieldWidget);
                mutableField->UpdateCaretMetrics(caretPositions, baseCaretHeight);
            }

            if (!actualText.empty() && textFieldWidget->HasSelection()) {
                auto [startIndex, endIndex] = textFieldWidget->GetSelectionIndices();
                if (startIndex < caretPositions.size() && endIndex <= caretPositions.size()) {
                    const float x0 = textStartX + caretPositions[startIndex];
                    const float x1 = textStartX + caretPositions[endIndex];
                    const float selectionWidth = std::max(x1 - x0, 0.0f);
                    if (selectionWidth > 0.5f) {
                        EnsureSolidTexture();
                        // 确保纹理有效，避免使用错误的纹理
                        if (m_solidTexture && m_solidTextureValid && m_solidTexture->IsValid()) {
                            UISpriteCommand selectionCmd;
                            selectionCmd.transform = CreateRef<Transform>();
                            selectionCmd.transform->SetPosition(Vector3(x0,
                                                                        highlightTop,
                                                                        static_cast<float>(-depth) * 0.001f));
                            selectionCmd.texture = m_solidTexture;
                            selectionCmd.sourceRect = Rect(0.0f, 0.0f, 1.0f, 1.0f);
                            selectionCmd.size = Vector2(selectionWidth, baseCaretHeight);
                            selectionCmd.tint = Color(0.28f, 0.44f, 0.78f, 0.45f);
                            selectionCmd.layerID = 800;
                            selectionCmd.depth = static_cast<float>(depth) - 0.1f;
                            selectionCmd.isCursor = false;  // 选中区域不是光标
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
                // 确保光标纹理已创建（在闪烁检查之外，避免每次闪烁都检查）
                EnsureSolidTexture();
                
                // 计算光标闪烁：每500毫秒切换一次可见性
                constexpr Uint64 kCursorBlinkIntervalMs = 500;
                Uint64 currentTicks = SDL_GetTicks();
                bool cursorVisible = (currentTicks / kCursorBlinkIntervalMs) % 2 == 0;
                
                // 使用缓存的有效性标志，避免频繁检查IsValid()导致纹理状态变化
                // 简单验证：确保光标纹理有效
                bool isValidSolidTexture = m_solidTextureValid && 
                                         m_solidTexture && 
                                         m_solidTexture->IsValid() &&
                                         m_solidTexture->GetWidth() == 1 &&
                                         m_solidTexture->GetHeight() == 1;
                
                // 如果纹理无效，记录原因（仅一次）
                if (isFocused && cursorVisible && !isValidSolidTexture) {
                    static bool loggedInvalidTexture = false;
                    if (!loggedInvalidTexture) {
                        if (!m_solidTexture) {
                            Logger::GetInstance().Warning("[UIRendererBridge] Cursor texture is null!");
                        } else if (!m_solidTexture->IsValid()) {
                            Logger::GetInstance().Warning("[UIRendererBridge] Cursor texture is invalid!");
                        } else if (m_solidTexture->GetWidth() != 1 || m_solidTexture->GetHeight() != 1) {
                            Logger::GetInstance().WarningFormat(
                                "[UIRendererBridge] Cursor texture size is %dx%d (expected 1x1)!",
                                m_solidTexture->GetWidth(), m_solidTexture->GetHeight());
                        }
                        loggedInvalidTexture = true;
                    }
                }
                
                if (cursorVisible && isValidSolidTexture) {
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
                    
                    // 获取实际文本大小，确保光标高度与文本高度完全一致
                    float actualTextHeight = textHeight;
                    if (m_defaultFont && !drawText.empty()) {
                        // 创建临时Text对象获取实际文本大小
                        auto tempText = CreateRef<Text>(m_defaultFont);
                        tempText->SetString(drawText);
                        tempText->SetAlignment(TextAlignment::Left);
                        tempText->EnsureUpdated();
                        Vector2 actualTextSize = tempText->GetSize();
                        if (actualTextSize.y() > 0.0f) {
                            actualTextHeight = actualTextSize.y();
                        }
                    }
                    
                    // 光标位置：与文本的上下边界对齐
                    // TextRenderer的Left对齐：传入position = P（左上角），最终渲染位置 = P + textSize/2（文本中心）
                    // 所以文本的实际顶部 = 文本中心 - textSize.y/2 = (textStartY + textSize.y/2) - textSize.y/2 = textStartY
                    // 文本的实际底部 = 文本顶部 + textSize.y = textStartY + textSize.y
                    // 文本中心 = textStartY + actualTextHeight/2
                    // 
                    // SpriteRenderable也以中心为锚点，所以光标中心应该在文本中心位置
                    // 这样光标顶部 = 光标中心 - caretHeight/2 = (textStartY + actualTextHeight/2) - actualTextHeight/2 = textStartY
                    // 光标底部 = 光标中心 + caretHeight/2 = (textStartY + actualTextHeight/2) + actualTextHeight/2 = textStartY + actualTextHeight
                    float caretX = textStartX + caretOffset;
                    float caretY = textStartY + actualTextHeight * 0.5f; // 光标中心在文本中心位置
                    
                    // 光标尺寸
                    float caretWidth = std::max(UITextField::kCaretWidth, 2.0f);
                    // 光标高度：使用实际文本高度，确保与文本的上下边界完全对齐
                    float finalCaretHeight = actualTextHeight;
                    // 确保最小高度
                    finalCaretHeight = std::max(finalCaretHeight, 12.0f);
                    
                    // 光标应该使用比文本更靠前的深度值，确保光标始终在文本之上
                    // 文本使用：depth = static_cast<float>(depth)，Z = static_cast<float>(-depth) * 0.001f
                    float textDepth = static_cast<float>(depth);
                    // 光标使用更小的深度值（更靠前），确保在文本之上
                    float caretDepth = textDepth - 0.01f;
                    
                    // 根据背景颜色自动反色光标
                    // 计算背景颜色的亮度（使用相对亮度公式）
                    const Color& bgColor = colorSet.inner;
                    float luminance = 0.299f * bgColor.r + 0.587f * bgColor.g + 0.114f * bgColor.b;
                    // 如果背景较亮（亮度 > 0.5），使用黑色光标；否则使用白色光标
                    Color caretColor = (luminance > 0.5f) 
                        ? Color(0.0f, 0.0f, 0.0f, 1.0f)  // 黑色光标（浅色背景）
                        : Color(1.0f, 1.0f, 1.0f, 1.0f); // 白色光标（深色背景）
                    
                    // 创建光标命令（纹理验证已在上面完成）
                    UISpriteCommand caretCmd;
                    caretCmd.transform = CreateRef<Transform>();
                    caretCmd.transform->SetPosition(Vector3(caretX, caretY, static_cast<float>(-caretDepth) * 0.001f));
                    caretCmd.texture = m_solidTexture;
                    caretCmd.sourceRect = Rect(0.0f, 0.0f, 1.0f, 1.0f);
                    caretCmd.size = Vector2(caretWidth, finalCaretHeight);
                    caretCmd.tint = caretColor;
                    caretCmd.layerID = 800;
                    caretCmd.depth = caretDepth;
                    caretCmd.isCursor = true;  // 明确标识这是光标命令
                    m_commandBuffer.AddSprite(caretCmd);
                }
            }

            if (m_debugConfig && m_debugConfig->drawDebugRects) {
                UIDebugRectCommand debugRectCmd;
                debugRectCmd.rect = Rect(position.x(), position.y(), size.x(), size.y());
                debugRectCmd.color = Color(0.4f, 0.8f, 0.4f, 0.35f);
                debugRectCmd.thickness = 2.0f;
                debugRectCmd.layerID = 999;
                debugRectCmd.depth = static_cast<float>(depth);
                m_commandBuffer.AddDebugRect(debugRectCmd);
            }
        }

        widget.ForEachChild([&](const UIWidget& child) {
            build(child, depth + 1);
        });
    };

    build(*root, 0);
}

} // namespace Render::UI


