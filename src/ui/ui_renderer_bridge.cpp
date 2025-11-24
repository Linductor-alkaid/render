#include "render/ui/ui_renderer_bridge.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
#include "render/ui/widgets/ui_checkbox.h"
#include "render/ui/widgets/ui_toggle.h"
#include "render/ui/widgets/ui_slider.h"
#include "render/ui/widgets/ui_radio_button.h"
#include "render/ui/widgets/ui_color_picker.h"
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

// 计算颜色的相对亮度（使用标准公式：0.299*R + 0.587*G + 0.114*B）
float GetColorLuminance(const Color& color) {
    return 0.299f * color.r + 0.587f * color.g + 0.114f * color.b;
}

// 获取选中颜色：默认黑色，但如果底部颜色太深则替换为较亮的颜色
Color GetSelectedColor(const Color& backgroundColor) {
    const Color defaultColor(0.0f, 0.0f, 0.0f, 1.0f);  // 默认黑色
    const float backgroundColorLuminance = GetColorLuminance(backgroundColor);
    const float threshold = 0.3f;  // 亮度阈值，低于此值认为颜色太深
    
    if (backgroundColorLuminance < threshold) {
        // 颜色太深，使用较亮的颜色（基于背景色亮度调整）
        // 计算一个比背景色更亮的颜色，但保持一定的对比度
        float targetLuminance = std::max(0.7f, backgroundColorLuminance + 0.4f);
        float scale = targetLuminance / (backgroundColorLuminance + 0.001f);  // 避免除零
        return Color(
            std::min(1.0f, backgroundColor.r * scale),
            std::min(1.0f, backgroundColor.g * scale),
            std::min(1.0f, backgroundColor.b * scale),
            1.0f
        );
    }
    
    return defaultColor;
}

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
    m_geometryRenderer.ResetMeshPool();

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
            
            // ========== 严格验证：防止渲染整个 atlas 纹理 ==========
            // 这是最后一道防线，必须在这里彻底阻止错误的渲染
            
            if (!cmd.sprite.isCursor && m_uiAtlas && renderTexture == m_uiAtlas->GetTexture()) {
                const Rect& src = cmd.sprite.sourceRect;
                float posX = cmd.sprite.transform ? cmd.sprite.transform->GetPosition().x() : 0.0f;
                float posY = cmd.sprite.transform ? cmd.sprite.transform->GetPosition().y() : 0.0f;
                
                // 检查0：如果纹理尺寸是 700x701（test_sprite_atlas 的尺寸），这是错误的 atlas 纹理
                bool isWrongAtlasTexture = false;
                if (renderTexture) {
                    int texWidth = renderTexture->GetWidth();
                    int texHeight = renderTexture->GetHeight();
                    isWrongAtlasTexture = (texWidth == 700 && texHeight == 701);
                }
                
                // 检查1：sourceRect 是否覆盖了整个纹理（超过 85%，降低阈值更严格）
                constexpr float kMaxUVCoverage = 0.85f;
                bool coversFullTexture = (src.x <= 0.05f && src.y <= 0.05f && 
                                         src.width >= kMaxUVCoverage && src.height >= kMaxUVCoverage);
                
                // 检查2：位置是否在可疑区域（左上角附近，可能是错误渲染的位置）
                bool isSuspiciousPosition = (posX >= -50.0f && posX <= 50.0f && 
                                            posY >= -50.0f && posY <= 50.0f);
                
                // 检查3：sprite 尺寸是否异常大（接近整个纹理尺寸）
                bool suspiciousSize = false;
                if (renderTexture) {
                    float texWidth = static_cast<float>(renderTexture->GetWidth());
                    float texHeight = static_cast<float>(renderTexture->GetHeight());
                    // 如果 sprite 尺寸接近纹理尺寸（超过 70%），这是可疑的
                    if (texWidth > 0.0f && texHeight > 0.0f) {
                        float sizeRatioX = cmd.sprite.size.x() / texWidth;
                        float sizeRatioY = cmd.sprite.size.y() / texHeight;
                        suspiciousSize = (sizeRatioX >= 0.7f || sizeRatioY >= 0.7f);
                    }
                }
                
                // 如果满足多个可疑条件，拒绝渲染
                int suspiciousFlags = 0;
                if (coversFullTexture) suspiciousFlags++;
                if (isSuspiciousPosition) suspiciousFlags++;
                if (suspiciousSize) suspiciousFlags++;
                
                // 关键：如果 sourceRect 覆盖整个纹理，或者使用了错误的 atlas 纹理，都拒绝渲染
                if (coversFullTexture || isWrongAtlasTexture || (suspiciousFlags >= 2)) {
                    static int blockedCount = 0;
                    blockedCount++;
                    
                    if (blockedCount <= 5) {  // 只记录前5次
                        Logger::GetInstance().ErrorFormat(
                            "[UIRendererBridge] BLOCKED #%d: Rejecting suspicious atlas sprite render! "
                            "sourceRect=(%.3f,%.3f,%.3f,%.3f), position=(%.1f,%.1f), size=(%.1f,%.1f), "
                            "textureSize=%dx%d, coversFullTexture=%s, wrongAtlasTexture=%s, suspiciousPosition=%s, suspiciousSize=%s",
                            blockedCount,
                            src.x, src.y, src.width, src.height,
                            posX, posY,
                            cmd.sprite.size.x(), cmd.sprite.size.y(),
                            renderTexture ? renderTexture->GetWidth() : 0,
                            renderTexture ? renderTexture->GetHeight() : 0,
                            coversFullTexture ? "YES" : "NO",
                            isWrongAtlasTexture ? "YES" : "NO",
                            isSuspiciousPosition ? "YES" : "NO",
                            suspiciousSize ? "YES" : "NO");
                    }
                    // 彻底拒绝渲染
                    continue;
                }
            }
            
            // 额外检查：确保非光标命令不使用 solidTexture
            if (!cmd.sprite.isCursor && m_solidTexture && renderTexture == m_solidTexture) {
                static bool loggedWrongTexture = false;
                if (!loggedWrongTexture) {
                    Logger::GetInstance().ErrorFormat(
                        "[UIRendererBridge] CRITICAL: Non-cursor sprite using solid texture! "
                        "This should never happen. Position=(%.1f,%.1f), size=(%.1f,%.1f)",
                        cmd.sprite.transform ? cmd.sprite.transform->GetPosition().x() : 0.0f,
                        cmd.sprite.transform ? cmd.sprite.transform->GetPosition().y() : 0.0f,
                        cmd.sprite.size.x(), cmd.sprite.size.y());
                    loggedWrongTexture = true;
                }
                // 拒绝渲染
                continue;
            }
            
            SpriteRenderable sprite;
            sprite.SetTransform(cmd.sprite.transform);
            sprite.SetLayerID(cmd.sprite.layerID);
            // 将 depth 值转换为 RenderPriority，确保正确的渲染顺序
            // depth 值越大，RenderPriority 越小（先渲染），depth 值越小，RenderPriority 越大（后渲染）
            sprite.SetRenderPriority(static_cast<int32_t>(-cmd.sprite.depth * 1000.0f));
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

    // 验证 atlas 纹理大小：ui_core atlas 应该是 512x512，不是 700x701（test_sprite_atlas）
    if (atlas->GetTexture()) {
        int width = atlas->GetTexture()->GetWidth();
        int height = atlas->GetTexture()->GetHeight();
        
        if (width == 700 && height == 701) {
            Logger::GetInstance().ErrorFormat(
                "[UIRendererBridge] CRITICAL: Wrong atlas loaded! 'ui_core' atlas has texture size %dx%d (test_sprite_atlas size)! "
                "This atlas should be 512x512. The wrong atlas texture is being used! "
                "This may be caused by texture name collision. Atlas will be used but all full-texture frames will be rejected.",
                width, height);
            // 不直接 return，而是继续使用但会在渲染时严格拒绝整个纹理的渲染
            // 这样可以避免 UI 完全不渲染，同时防止错误的纹理显示
        } else if (width != 512 || height != 512) {
            Logger::GetInstance().WarningFormat(
                "[UIRendererBridge] Warning: 'ui_core' atlas has unexpected texture size %dx%d (expected 512x512).",
                width, height);
        } else {
            Logger::GetInstance().InfoFormat(
                "[UIRendererBridge] 'ui_core' atlas loaded successfully with texture size %dx%d.",
                width, height);
        }
        
        // 验证所有 frame 的 UV 坐标，确保没有覆盖整个纹理的 frame
        const auto& allFrames = atlas->GetAllFrames();
        float texWidth = static_cast<float>(width);
        float texHeight = static_cast<float>(height);
        constexpr float kMaxUVCoverage = 0.90f;
        
        for (const auto& [frameName, frame] : allFrames) {
            // 跳过保留关键字
            if (frameName == "meta" || frameName == "frames" || frameName == "animations") {
                Logger::GetInstance().ErrorFormat(
                    "[UIRendererBridge] CRITICAL: Atlas contains reserved keyword '%s' as frame name! "
                    "This is invalid. Frame will be ignored.",
                    frameName.c_str());
                continue;
            }
            
            // 检查 frame 的 UV 坐标
            Rect uv = frame.uv;
            bool isPixelCoordinates = (uv.x >= 0.0f && uv.x < texWidth) &&
                                      (uv.y >= 0.0f && uv.y < texHeight);
            
            if (isPixelCoordinates) {
                // 归一化 UV 坐标
                float normalizedX = uv.x / texWidth;
                float normalizedY = uv.y / texHeight;
                float normalizedW = uv.width / texWidth;
                float normalizedH = uv.height / texHeight;
                
                // 检查是否覆盖整个纹理
                if (normalizedX <= 0.05f && normalizedY <= 0.05f &&
                    normalizedW >= kMaxUVCoverage && normalizedH >= kMaxUVCoverage) {
                    Logger::GetInstance().ErrorFormat(
                        "[UIRendererBridge] CRITICAL: Frame '%s' in atlas covers entire texture! "
                        "UV=(%.1f,%.1f,%.1f,%.1f) normalized=(%.3f,%.3f,%.3f,%.3f). "
                        "This frame will be rejected during rendering.",
                        frameName.c_str(),
                        uv.x, uv.y, uv.width, uv.height,
                        normalizedX, normalizedY, normalizedW, normalizedH);
                }
            } else {
                // 已经是归一化坐标
                if (uv.x <= 0.05f && uv.y <= 0.05f &&
                    uv.width >= kMaxUVCoverage && uv.height >= kMaxUVCoverage) {
                    Logger::GetInstance().ErrorFormat(
                        "[UIRendererBridge] CRITICAL: Frame '%s' in atlas covers entire texture! "
                        "UV=(%.3f,%.3f,%.3f,%.3f). This frame will be rejected during rendering.",
                        frameName.c_str(), uv.x, uv.y, uv.width, uv.height);
                }
            }
        }
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
            // 严格验证：拒绝渲染保留关键字（JSON metadata 字段）
            if (frameName == "meta" || frameName == "frames" || frameName == "animations") {
                static bool loggedReservedFrame = false;
                if (!loggedReservedFrame) {
                    Logger::GetInstance().ErrorFormat(
                        "[UIRendererBridge] CRITICAL: Attempting to render reserved keyword '%s' as frame! "
                        "Widget ID: '%s'. Reserved keywords are JSON metadata fields, not sprite frames!",
                        frameName.c_str(), id.c_str());
                    loggedReservedFrame = true;
                }
                return;
            }
            
            // 严格验证：frame 必须存在于 atlas 中
            if (!m_uiAtlas->HasFrame(frameName)) {
                static std::unordered_set<std::string> loggedMissingFrames;
                if (loggedMissingFrames.find(frameName) == loggedMissingFrames.end()) {
                    Logger::GetInstance().WarningFormat(
                        "[UIRendererBridge] Frame '%s' missing in atlas (widget ID: '%s'). Sprite will not be rendered.",
                        frameName.c_str(), id.c_str());
                    loggedMissingFrames.insert(frameName);
                }
                return;
            }

            const auto& frame = m_uiAtlas->GetFrame(frameName);

            // 严格验证：frame 的 UV 区域必须有效且不是整个纹理
            Rect normalizedUV = frame.uv;
            float texWidth = 0.0f;
            float texHeight = 0.0f;
            
            if (!m_uiAtlas->GetTexture() || !m_uiAtlas->GetTexture()->IsValid()) {
                static bool loggedInvalidAtlasTexture = false;
                if (!loggedInvalidAtlasTexture) {
                    Logger::GetInstance().ErrorFormat(
                        "[UIRendererBridge] Atlas texture is invalid when rendering frame '%s' (widget ID: '%s').",
                        frameName.c_str(), id.c_str());
                    loggedInvalidAtlasTexture = true;
                }
                return;
            }
            
            texWidth = static_cast<float>(m_uiAtlas->GetTexture()->GetWidth());
            texHeight = static_cast<float>(m_uiAtlas->GetTexture()->GetHeight());
            
            if (texWidth <= 0.0f || texHeight <= 0.0f) {
                static bool loggedInvalidTextureSize = false;
                if (!loggedInvalidTextureSize) {
                    Logger::GetInstance().ErrorFormat(
                        "[UIRendererBridge] Atlas texture has invalid size %fx%f when rendering frame '%s' (widget ID: '%s').",
                        texWidth, texHeight, frameName.c_str(), id.c_str());
                    loggedInvalidTextureSize = true;
                }
                return;
            }

            // 将像素坐标转换为归一化 UV 坐标
            // 如果 UV 值大于1.0或大于纹理尺寸，说明是像素坐标，需要归一化
            bool isPixelCoordinates = (normalizedUV.x >= 0.0f && normalizedUV.x < texWidth) &&
                                      (normalizedUV.y >= 0.0f && normalizedUV.y < texHeight);
            
            if (isPixelCoordinates) {
                normalizedUV.x /= texWidth;
                normalizedUV.y /= texHeight;
                normalizedUV.width /= texWidth;
                normalizedUV.height /= texHeight;
            }
            
            // 严格验证：归一化后的 UV 必须在有效范围内 [0, 1]
            if (normalizedUV.x < 0.0f || normalizedUV.y < 0.0f ||
                normalizedUV.x + normalizedUV.width > 1.0f ||
                normalizedUV.y + normalizedUV.height > 1.0f) {
                static bool loggedInvalidUV = false;
                if (!loggedInvalidUV) {
                    Logger::GetInstance().ErrorFormat(
                        "[UIRendererBridge] Invalid UV coordinates for frame '%s' (widget ID: '%s'): "
                        "normalizedUV=(%.3f, %.3f, %.3f, %.3f), textureSize=(%.0f, %.0f)",
                        frameName.c_str(), id.c_str(),
                        normalizedUV.x, normalizedUV.y, normalizedUV.width, normalizedUV.height,
                        texWidth, texHeight);
                    loggedInvalidUV = true;
                }
                return;
            }
            
            // 严格验证：防止渲染整个纹理（UV 覆盖范围接近 [0,0,1,1]）
            // 如果 UV 覆盖范围超过 90%，直接拒绝（降低阈值，更严格）
            constexpr float kMaxUVCoverage = 0.90f;
            bool coversFullTexture = (normalizedUV.x <= 0.05f && normalizedUV.y <= 0.05f &&
                                      normalizedUV.width >= kMaxUVCoverage && normalizedUV.height >= kMaxUVCoverage);
            
            // 额外检查：如果 UV 覆盖范围很大（超过 80%），且位置在可疑区域，也拒绝
            bool largeCoverage = (normalizedUV.width >= 0.80f || normalizedUV.height >= 0.80f);
            bool suspiciousPosition = (position.x() >= -50.0f && position.x() <= 50.0f &&
                                      position.y() >= -50.0f && position.y() <= 50.0f);
            
            if (coversFullTexture || (largeCoverage && suspiciousPosition)) {
                static int blockedCount = 0;
                blockedCount++;
                
                if (blockedCount <= 3) {  // 只记录前3次
                    Logger::GetInstance().ErrorFormat(
                        "[UIRendererBridge] BLOCKED #%d: Frame '%s' (widget ID: '%s') rejected! "
                        "UV=(%.3f, %.3f, %.3f, %.3f), position=(%.1f, %.1f), "
                        "coversFullTexture=%s, largeCoverage=%s, suspiciousPosition=%s. "
                        "Sprite will not be rendered.",
                        blockedCount,
                        frameName.c_str(), id.c_str(),
                        normalizedUV.x, normalizedUV.y, normalizedUV.width, normalizedUV.height,
                        position.x(), position.y(),
                        coversFullTexture ? "YES" : "NO",
                        largeCoverage ? "YES" : "NO",
                        suspiciousPosition ? "YES" : "NO");
                }
                return;
            }

            // 验证：确保 widget 尺寸合理
            Vector2 finalSize = size;
            if (finalSize.x() <= 0.0f || finalSize.y() <= 0.0f) {
                finalSize = frame.size;
            }
            
            // 创建 sprite 命令
            UISpriteCommand cmd;
            cmd.transform = CreateRef<Transform>();
            cmd.transform->SetPosition(Vector3(position.x(), position.y(), static_cast<float>(-depth) * 0.001f));
            cmd.texture = m_uiAtlas->GetTexture();
            cmd.sourceRect = normalizedUV;
            cmd.tint = tint;
            cmd.layerID = 800;
            cmd.depth = static_cast<float>(depth);
            cmd.isCursor = false;  // 明确标识这是图集命令，不是光标命令
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
        } else if (const auto* checkboxWidget = dynamic_cast<const UICheckBox*>(&widget)) {
            // 使用主题系统获取颜色
            const UITheme* theme = nullptr;
            if (m_themeManager) {
                theme = &m_themeManager->GetCurrentTheme();
            }
            
            const bool enabled = checkboxWidget->IsEnabled();
            const bool hovered = checkboxWidget->IsHovered();
            const bool checked = checkboxWidget->IsChecked();
            const bool indeterminate = checkboxWidget->GetState() == UICheckBox::State::Indeterminate;
            
            // Checkbox 使用 button 主题颜色
            const UIThemeColorSet& colorSet = theme
                ? theme->GetWidgetColorSet("button",
                                          hovered,
                                          false,  // pressed
                                          !enabled,
                                          false)  // active
                : UITheme::CreateDefault().GetWidgetColorSet("button",
                                                            hovered,
                                                            false,
                                                            !enabled,
                                                            false);
            
            // 计算复选框框的大小（正方形，高度为控件高度）
            const float checkboxSize = size.y();
            const float checkboxPadding = 4.0f;
            const float checkboxBoxSize = checkboxSize - checkboxPadding * 2.0f;
            
            // 根据标签位置确定复选框框的位置
            float checkboxX, checkboxY;
            float labelX, labelY;
            
            if (checkboxWidget->IsLabelOnLeft()) {
                // 标签在左侧，复选框在右侧
                checkboxX = position.x() + size.x() - checkboxSize + checkboxPadding;
                checkboxY = position.y() + checkboxPadding;
                labelX = position.x() + checkboxPadding;
                labelY = position.y() + size.y() * 0.5f;
            } else {
                // 标签在右侧，复选框在左侧（默认）
                checkboxX = position.x() + checkboxPadding;
                checkboxY = position.y() + checkboxPadding;
                labelX = position.x() + checkboxSize + checkboxPadding * 2.0f;
                labelY = position.y() + size.y() * 0.5f;
            }
            
            // 绘制复选框框（圆角矩形）
            UIRoundedRectangleCommand boxCmd;
            boxCmd.rect = Rect(checkboxX, checkboxY, checkboxBoxSize, checkboxBoxSize);
            boxCmd.cornerRadius = 4.0f;
            boxCmd.fillColor = colorSet.inner;
            boxCmd.strokeColor = colorSet.outline;
            boxCmd.strokeWidth = 1.5f;
            boxCmd.filled = true;
            boxCmd.stroked = true;
            boxCmd.segments = 16; // 增加分段数
            boxCmd.layerID = 800;
            boxCmd.depth = static_cast<float>(depth) + 0.01f; // 框在底层（更大的depth值表示更靠后）
            m_commandBuffer.AddRoundedRectangle(boxCmd);
            
            // 绘制选中标记或不确定标记（确保在框之上）
            if (indeterminate) {
                // 不确定状态：绘制一条水平线
                const float lineY = checkboxY + checkboxBoxSize * 0.5f;
                const float lineWidth = checkboxBoxSize * 0.6f;
                const float lineHeight = 3.0f; // 稍微加粗
                const float lineX = checkboxX + (checkboxBoxSize - lineWidth) * 0.5f;
                
                UIRectangleCommand lineCmd;
                lineCmd.rect = Rect(lineX, lineY - lineHeight * 0.5f, lineWidth, lineHeight);
                lineCmd.fillColor = colorSet.text;
                lineCmd.strokeColor = Color(0, 0, 0, 0);
                lineCmd.strokeWidth = 0.0f;
                lineCmd.filled = true;
                lineCmd.stroked = false;
                lineCmd.layerID = 800; // 使用相同的layerID，通过RenderPriority控制顺序
                lineCmd.depth = static_cast<float>(depth) - 0.01f; // 标记在框之上（更小的depth值表示更靠前）
                m_commandBuffer.AddRectangle(lineCmd);
            } else if (checked) {
                // 选中状态：绘制对勾（使用两条线段组成，加粗）
                const float checkSize = checkboxBoxSize * 0.6f;
                const float checkX = checkboxX + checkboxBoxSize * 0.5f;
                const float checkY = checkboxY + checkboxBoxSize * 0.5f;
                const float lineWidth = 3.0f; // 加粗对勾
                
                // 对勾的第一段（左下到中间）
                Vector2 p1(checkX - checkSize * 0.3f, checkY);
                Vector2 p2(checkX - checkSize * 0.1f, checkY + checkSize * 0.3f);
                UILineCommand line1Cmd;
                line1Cmd.start = p1;
                line1Cmd.end = p2;
                line1Cmd.width = lineWidth;
                line1Cmd.color = colorSet.text;
                line1Cmd.layerID = 800; // 使用相同的layerID，通过RenderPriority控制顺序
                line1Cmd.depth = static_cast<float>(depth) - 0.01f; // 标记在框之上（更小的depth值表示更靠前）
                m_commandBuffer.AddLine(line1Cmd);
                
                // 对勾的第二段（中间到右上）
                Vector2 p3(checkX + checkSize * 0.3f, checkY - checkSize * 0.2f);
                UILineCommand line2Cmd;
                line2Cmd.start = p2;
                line2Cmd.end = p3;
                line2Cmd.width = lineWidth;
                line2Cmd.color = colorSet.text;
                line2Cmd.layerID = 800; // 使用相同的layerID，通过RenderPriority控制顺序
                line2Cmd.depth = static_cast<float>(depth) - 0.01f; // 标记在框之上（更小的depth值表示更靠前）
                m_commandBuffer.AddLine(line2Cmd);
            }
            
            // 绘制标签文本
            if (m_defaultFont && !checkboxWidget->GetLabel().empty()) {
                std::string label = checkboxWidget->GetLabel();
                
                auto tempText = CreateRef<Text>(m_defaultFont);
                tempText->SetString(label);
                tempText->SetAlignment(TextAlignment::Left);
                tempText->EnsureUpdated();
                Vector2 textSize = tempText->GetSize();
                
                UITextCommand textCmd;
                textCmd.transform = CreateRef<Transform>();
                float textY = labelY - textSize.y() * 0.5f;
                textCmd.transform->SetPosition(Vector3(labelX,
                                                       textY,
                                                       static_cast<float>(-depth) * 0.001f));
                textCmd.text = label;
                textCmd.font = m_defaultFont;
                textCmd.color = colorSet.text;
                textCmd.offset = Vector2(0.0f, 0.0f); // Left对齐
                textCmd.layerID = 800;
                textCmd.depth = static_cast<float>(depth);
                
                m_commandBuffer.AddText(textCmd);
            }
            
            if (m_debugConfig && m_debugConfig->drawDebugRects) {
                UIDebugRectCommand debugRectCmd;
                debugRectCmd.rect = Rect(position.x(), position.y(), size.x(), size.y());
                debugRectCmd.color = Color(0.8f, 0.4f, 0.8f, 0.35f);
                debugRectCmd.thickness = 2.0f;
                debugRectCmd.layerID = 999;
                debugRectCmd.depth = static_cast<float>(depth);
                m_commandBuffer.AddDebugRect(debugRectCmd);
            }
        } else if (const auto* toggleWidget = dynamic_cast<const UIToggle*>(&widget)) {
            // 使用主题系统获取颜色
            const UITheme* theme = nullptr;
            if (m_themeManager) {
                theme = &m_themeManager->GetCurrentTheme();
            }
            
            const bool widgetEnabled = toggleWidget->IsEnabled();
            const bool toggled = toggleWidget->IsToggled();
            const bool hovered = toggleWidget->IsHovered();
            const float animationProgress = toggleWidget->GetAnimationProgress();
            
            // Toggle 使用 button 主题颜色
            const UIThemeColorSet& colorSet = theme
                ? theme->GetWidgetColorSet("button",
                                          hovered,
                                          false,  // pressed
                                          !widgetEnabled,
                                          toggled)  // active (使用toggled状态)
                : UITheme::CreateDefault().GetWidgetColorSet("button",
                                                            hovered,
                                                            false,
                                                            !widgetEnabled,
                                                            toggled);
            
            // 计算开关轨道的大小和位置（优化尺寸比例）
            const float trackHeight = size.y() * 0.7f; // 轨道高度为控件高度的70%（更粗一些）
            const float trackWidth = size.y() * 2.0f;  // 轨道宽度为控件高度的2倍（更宽一些）
            const float trackPadding = (size.y() - trackHeight) * 0.5f;
            
            // 根据标签位置确定轨道的位置
            float trackX, trackY;
            float labelX, labelY;
            
            if (toggleWidget->IsLabelOnLeft()) {
                // 标签在左侧，轨道在右侧
                trackX = position.x() + size.x() - trackWidth - trackPadding;
                trackY = position.y() + trackPadding;
                labelX = position.x() + trackPadding;
                labelY = position.y() + size.y() * 0.5f;
            } else {
                // 标签在右侧，轨道在左侧（默认）
                trackX = position.x() + trackPadding;
                trackY = position.y() + trackPadding;
                labelX = position.x() + trackWidth + trackPadding * 3.0f;
                labelY = position.y() + size.y() * 0.5f;
            }
            
            // 绘制开关轨道背景（圆角矩形，胶囊形）
            // 根据动画进度平滑混合颜色：关闭时浅灰色，开启时使用选中颜色
            Color trackColor;
            float t = animationProgress; // 直接使用动画进度
            Color offColor = Color(0.85f, 0.85f, 0.85f, 1.0f); // 关闭状态：浅灰色
            // 开启状态：默认黑色，但如果底部颜色太深则替换为较亮的颜色
            Color onColor = GetSelectedColor(colorSet.inner);
            trackColor = Color(
                offColor.r * (1.0f - t) + onColor.r * t,
                offColor.g * (1.0f - t) + onColor.g * t,
                offColor.b * (1.0f - t) + onColor.b * t,
                offColor.a * (1.0f - t) + onColor.a * t
            );
            
            UIRoundedRectangleCommand trackCmd;
            trackCmd.rect = Rect(trackX, trackY, trackWidth, trackHeight);
            trackCmd.cornerRadius = trackHeight * 0.5f; // 完全圆角，形成胶囊形
            trackCmd.fillColor = trackColor;
            trackCmd.strokeColor = hovered ? Color(0.5f, 0.5f, 0.5f, 1.0f) : Color(0.7f, 0.7f, 0.7f, 1.0f); // 灰色描边，悬停时稍深
            trackCmd.strokeWidth = hovered ? 1.5f : 1.0f;
            trackCmd.filled = true;
            trackCmd.stroked = true;
            trackCmd.segments = 32; // 增加分段数，减少透明线
            trackCmd.layerID = 800;
            trackCmd.depth = static_cast<float>(depth) + 0.01f; // 轨道在底层（更大的depth值表示更靠后）
            m_commandBuffer.AddRoundedRectangle(trackCmd);
            
            // 绘制开关按钮（圆形滑块，优化大小和颜色）
            const float buttonRadius = trackHeight * 0.45f; // 按钮半径为轨道高度的45%（更大一些）
            const float buttonPadding = 2.0f; // 固定内边距
            const float buttonTravel = trackWidth - buttonRadius * 2.0f - buttonPadding * 2.0f;
            
            // 根据动画进度计算按钮位置（使用缓动效果）
            float buttonX = trackX + buttonPadding + buttonRadius + buttonTravel * animationProgress;
            float buttonY = trackY + trackHeight * 0.5f;
            
            // 按钮颜色：白色，与轨道形成对比
            Color buttonColor = Color(1.0f, 1.0f, 1.0f, 1.0f); // 白色按钮
            if (hovered) {
                buttonColor = Color(0.98f, 0.98f, 0.98f, 1.0f); // 悬停时稍微变暗
            }
            
            // 绘制按钮阴影（轻微偏移的深色圆）
            if (animationProgress > 0.1f && animationProgress < 0.9f) {
                UICircleCommand shadowCmd;
                shadowCmd.center = Vector2(buttonX + 1.0f, buttonY + 1.0f);
                shadowCmd.radius = buttonRadius * 0.95f;
                shadowCmd.fillColor = Color(0.0f, 0.0f, 0.0f, 0.15f); // 半透明黑色阴影
                shadowCmd.strokeColor = Color(0, 0, 0, 0);
                shadowCmd.strokeWidth = 0.0f;
                shadowCmd.filled = true;
                shadowCmd.stroked = false;
                shadowCmd.segments = 32; // 增加分段数
                shadowCmd.layerID = 800; // 使用相同的layerID，通过RenderPriority控制顺序
                shadowCmd.depth = static_cast<float>(depth) - 0.01f; // 阴影在按钮下方但在轨道上方
                m_commandBuffer.AddCircle(shadowCmd);
            }
            
            // 绘制按钮主体（确保在最上层）
            UICircleCommand buttonCmd;
            buttonCmd.center = Vector2(buttonX, buttonY);
            buttonCmd.radius = buttonRadius;
            buttonCmd.fillColor = buttonColor;
            buttonCmd.strokeColor = Color(0.75f, 0.75f, 0.75f, 1.0f); // 浅灰色描边
            buttonCmd.strokeWidth = 1.0f;
            buttonCmd.filled = true;
            buttonCmd.stroked = true;
            buttonCmd.segments = 32; // 增加分段数，减少透明线
            buttonCmd.layerID = 800; // 使用相同的layerID，通过RenderPriority控制顺序
            buttonCmd.depth = static_cast<float>(depth) - 0.02f; // 按钮在最上层（更小的depth值表示更靠前）
            m_commandBuffer.AddCircle(buttonCmd);
            
            // 绘制标签文本
            if (m_defaultFont && !toggleWidget->GetLabel().empty()) {
                std::string label = toggleWidget->GetLabel();
                
                auto tempText = CreateRef<Text>(m_defaultFont);
                tempText->SetString(label);
                tempText->SetAlignment(TextAlignment::Left);
                tempText->EnsureUpdated();
                Vector2 textSize = tempText->GetSize();
                
                UITextCommand textCmd;
                textCmd.transform = CreateRef<Transform>();
                float textY = labelY - textSize.y() * 0.5f;
                textCmd.transform->SetPosition(Vector3(labelX,
                                                       textY,
                                                       static_cast<float>(-depth) * 0.001f));
                textCmd.text = label;
                textCmd.font = m_defaultFont;
                textCmd.color = colorSet.text;
                textCmd.offset = Vector2(0.0f, 0.0f); // Left对齐
                textCmd.layerID = 800;
                textCmd.depth = static_cast<float>(depth);
                
                m_commandBuffer.AddText(textCmd);
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
        } else if (const auto* sliderWidget = dynamic_cast<const UISlider*>(&widget)) {
            // 使用主题系统获取颜色
            const UITheme* theme = nullptr;
            if (m_themeManager) {
                theme = &m_themeManager->GetCurrentTheme();
            }
            
            const bool enabled = sliderWidget->IsEnabled();
            const bool hovered = sliderWidget->IsHovered();
            const bool dragging = sliderWidget->IsDragging();
            
            // Slider 使用 button 主题颜色
            const UIThemeColorSet& colorSet = theme
                ? theme->GetWidgetColorSet("button",
                                          hovered || dragging,
                                          dragging,  // pressed
                                          !enabled,
                                          false)     // active
                : UITheme::CreateDefault().GetWidgetColorSet("button",
                                                            hovered || dragging,
                                                            dragging,
                                                            !enabled,
                                                            false);
            
            const float normalizedValue = sliderWidget->GetNormalizedValue();
            const bool isHorizontal = sliderWidget->GetOrientation() == UISlider::Orientation::Horizontal;
            
            // 绘制滑块轨道背景
            UIRoundedRectangleCommand trackCmd;
            trackCmd.rect = Rect(position.x(), position.y(), size.x(), size.y());
            trackCmd.cornerRadius = isHorizontal ? size.y() * 0.5f : size.x() * 0.5f; // 完全圆角
            trackCmd.fillColor = Color(0.85f, 0.85f, 0.85f, 1.0f); // 浅灰色轨道背景
            trackCmd.strokeColor = Color(0.7f, 0.7f, 0.7f, 1.0f); // 稍深的灰色描边
            trackCmd.strokeWidth = 1.0f;
            trackCmd.filled = true;
            trackCmd.stroked = true;
            trackCmd.segments = 32; // 增加分段数，减少透明线
            trackCmd.layerID = 800;
            trackCmd.depth = static_cast<float>(depth) + 0.01f; // 轨道在最底层（更大的depth值表示更靠后）
            m_commandBuffer.AddRoundedRectangle(trackCmd);
            
            // 绘制滑块填充部分（已选择的部分）
            if (normalizedValue > 0.0f) {
                // 计算手柄位置，填充部分应该延伸到手柄中心
                const float handleRadius = isHorizontal ? size.y() * 0.4f : size.x() * 0.4f;
                const float cornerRadius = isHorizontal ? size.y() * 0.5f : size.x() * 0.5f; // 填充区域的圆角半径
                float fillSize;
                if (isHorizontal) {
                    // 水平滑轨：填充部分延伸到手柄中心位置，确保完全覆盖
                    // 由于填充区域使用完全圆角，右端圆角会从右边缘向内收缩cornerRadius
                    // 所以需要延伸到手柄中心+手柄半径+圆角半径，确保圆角部分也能完全覆盖
                    float handleCenterX = position.x() + size.x() * normalizedValue;
                    fillSize = handleCenterX - position.x() + handleRadius + cornerRadius; // 延伸到手柄中心+半径+圆角半径，确保完全覆盖
                    fillSize = std::min(fillSize, size.x()); // 限制在轨道范围内
                } else {
                    // 垂直滑块：从底部开始，延伸到手柄中心位置
                    // 由于填充区域使用完全圆角，顶端圆角会从顶部边缘向内收缩cornerRadius
                    // 所以需要延伸到手柄中心+手柄半径+圆角半径，确保圆角部分也能完全覆盖
                    float handleCenterY = position.y() + size.y() * (1.0f - normalizedValue);
                    // 填充部分从底部开始，延伸到手柄中心+半径+圆角半径
                    fillSize = (position.y() + size.y()) - handleCenterY + handleRadius + cornerRadius;
                    fillSize = std::max(0.0f, std::min(fillSize, size.y()));
                }
                
                UIRoundedRectangleCommand fillCmd;
                if (isHorizontal) {
                    // 水平滑轨：填充部分从左侧开始，只左端圆角，右端直边
                    // 使用完全圆角，但通过调整宽度确保完全覆盖轨道
                    fillCmd.rect = Rect(position.x(), position.y(), fillSize, size.y());
                    fillCmd.cornerRadius = size.y() * 0.5f; // 完全圆角
                } else {
                    // 垂直滑块：从底部开始，只底端圆角，顶端直边
                    fillCmd.rect = Rect(position.x(), position.y() + size.y() - fillSize, size.x(), fillSize);
                    fillCmd.cornerRadius = size.x() * 0.5f; // 完全圆角
                }
                fillCmd.fillColor = Color(0.3f, 0.6f, 1.0f, 1.0f); // 蓝色填充部分
                fillCmd.strokeColor = Color(0, 0, 0, 0); // 无描边
                fillCmd.strokeWidth = 0.0f;
                fillCmd.filled = true;
                fillCmd.stroked = false;
                fillCmd.segments = 32; // 增加分段数，确保圆角平滑
                fillCmd.layerID = 800; // 使用相同的layerID，通过RenderPriority控制顺序
                fillCmd.depth = static_cast<float>(depth) - 0.01f; // 填充在轨道之上（更小的depth值表示更靠前）
                m_commandBuffer.AddRoundedRectangle(fillCmd);
            }
            
            // 绘制滑块手柄（圆形）
            const float handleRadius = isHorizontal ? size.y() * 0.4f : size.x() * 0.4f;
            float handleX, handleY;
            
            if (isHorizontal) {
                handleX = position.x() + size.x() * normalizedValue;
                handleY = position.y() + size.y() * 0.5f;
            } else {
                handleX = position.x() + size.x() * 0.5f;
                handleY = position.y() + size.y() * (1.0f - normalizedValue);
            }
            
            // 限制手柄在轨道内
            if (isHorizontal) {
                handleX = std::clamp(handleX, position.x() + handleRadius, position.x() + size.x() - handleRadius);
            } else {
                handleY = std::clamp(handleY, position.y() + handleRadius, position.y() + size.y() - handleRadius);
            }
            
            // 绘制滑块手柄（圆角矩形，胶囊形）
            const float handleWidth = handleRadius * 2.0f;
            const float handleHeight = handleRadius * 2.0f;
            UIRoundedRectangleCommand handleCmd;
            if (isHorizontal) {
                handleCmd.rect = Rect(handleX - handleRadius, handleY - handleRadius, handleWidth, handleHeight);
            } else {
                handleCmd.rect = Rect(handleX - handleRadius, handleY - handleRadius, handleWidth, handleHeight);
            }
            handleCmd.cornerRadius = handleRadius; // 完全圆角，形成胶囊形
            handleCmd.fillColor = dragging ? Color(0.2f, 0.5f, 0.9f, 1.0f) : Color(1.0f, 1.0f, 1.0f, 1.0f); // 白色手柄，拖拽时变深蓝色
            handleCmd.strokeColor = dragging ? Color(0.15f, 0.4f, 0.8f, 1.0f) : Color(0.8f, 0.8f, 0.8f, 1.0f); // 浅灰色描边，拖拽时变深蓝色
            handleCmd.strokeWidth = 1.5f;
            handleCmd.filled = true;
            handleCmd.stroked = true;
            handleCmd.segments = 32; // 增加分段数，减少透明线
            handleCmd.layerID = 800; // 使用相同的layerID，通过RenderPriority控制顺序
            handleCmd.depth = static_cast<float>(depth) - 0.02f; // 手柄在最上层（更小的depth值表示更靠前）
            m_commandBuffer.AddRoundedRectangle(handleCmd);
            
            // 绘制标签和数值文本
            if (m_defaultFont) {
                std::string displayText;
                if (!sliderWidget->GetLabel().empty()) {
                    displayText = sliderWidget->GetLabel();
                    if (sliderWidget->IsShowValue()) {
                        displayText += ": ";
                    }
                }
                if (sliderWidget->IsShowValue()) {
                    char valueStr[32];
                    snprintf(valueStr, sizeof(valueStr), "%.2f", sliderWidget->GetValue());
                    displayText += valueStr;
                }
                
                if (!displayText.empty()) {
                    auto tempText = CreateRef<Text>(m_defaultFont);
                    tempText->SetString(displayText);
                    tempText->SetAlignment(TextAlignment::Left);
                    tempText->EnsureUpdated();
                    Vector2 textSize = tempText->GetSize();
                    
                    UITextCommand textCmd;
                    textCmd.transform = CreateRef<Transform>();
                    float textX = position.x() + (isHorizontal ? 8.0f : size.x() * 0.5f);
                    float textY = position.y() + size.y() * 0.5f - textSize.y() * 0.5f;
                    textCmd.transform->SetPosition(Vector3(textX,
                                                           textY,
                                                           static_cast<float>(-depth) * 0.001f));
                    textCmd.text = displayText;
                    textCmd.font = m_defaultFont;
                    textCmd.color = colorSet.text;
                    textCmd.offset = Vector2(0.0f, 0.0f); // Left对齐
                    textCmd.layerID = 800;
                    textCmd.depth = static_cast<float>(depth);
                    
                    m_commandBuffer.AddText(textCmd);
                }
            }
            
            if (m_debugConfig && m_debugConfig->drawDebugRects) {
                UIDebugRectCommand debugRectCmd;
                debugRectCmd.rect = Rect(position.x(), position.y(), size.x(), size.y());
                debugRectCmd.color = Color(0.8f, 0.8f, 0.4f, 0.35f);
                debugRectCmd.thickness = 2.0f;
                debugRectCmd.layerID = 999;
                debugRectCmd.depth = static_cast<float>(depth);
                m_commandBuffer.AddDebugRect(debugRectCmd);
            }
        } else if (const auto* radioWidget = dynamic_cast<const UIRadioButton*>(&widget)) {
            // 使用主题系统获取颜色
            const UITheme* theme = nullptr;
            if (m_themeManager) {
                theme = &m_themeManager->GetCurrentTheme();
            }
            
            const bool enabled = radioWidget->IsEnabled();
            const bool hovered = radioWidget->IsHovered();
            const bool selected = radioWidget->IsSelected();
            
            // RadioButton 使用 button 主题颜色
            const UIThemeColorSet& colorSet = theme
                ? theme->GetWidgetColorSet("button",
                                          hovered,
                                          false,  // pressed
                                          !enabled,
                                          selected)  // active (选中状态)
                : UITheme::CreateDefault().GetWidgetColorSet("button",
                                                            hovered,
                                                            false,
                                                            !enabled,
                                                            selected);
            
            // 计算单选按钮圆圈的大小（圆形，直径为控件高度）
            const float circleSize = size.y();
            const float circlePadding = 4.0f;
            const float circleRadius = (circleSize - circlePadding * 2.0f) * 0.5f;
            const float circleX = position.x() + circlePadding + circleRadius;
            const float circleY = position.y() + size.y() * 0.5f;
            
            // 绘制外圈（圆形边框）
            UICircleCommand outerCmd;
            outerCmd.center = Vector2(circleX, circleY);
            outerCmd.radius = circleRadius;
            outerCmd.fillColor = Color(0, 0, 0, 0); // 透明填充
            outerCmd.strokeColor = colorSet.outline;
            outerCmd.strokeWidth = 2.0f;
            outerCmd.filled = false;
            outerCmd.stroked = true;
            outerCmd.segments = 32; // 增加分段数，减少锯齿
            outerCmd.layerID = 800;
            outerCmd.depth = static_cast<float>(depth) + 0.01f; // 外圈在底层（更大的depth值表示更靠后）
            m_commandBuffer.AddCircle(outerCmd);
            
            // 如果选中，绘制内圈（实心圆，确保是圆形）
            if (selected) {
                const float innerRadius = circleRadius * 0.55f; // 稍微增大内圆半径，更明显
                UICircleCommand innerCmd;
                innerCmd.center = Vector2(circleX, circleY);
                innerCmd.radius = innerRadius;
                // 选中颜色：默认黑色，但如果底部颜色太深则替换为较亮的颜色
                innerCmd.fillColor = GetSelectedColor(colorSet.inner);
                innerCmd.strokeColor = Color(0, 0, 0, 0); // 无描边
                innerCmd.strokeWidth = 0.0f;
                innerCmd.filled = true;
                innerCmd.stroked = false;
                innerCmd.segments = 32; // 增加分段数，确保是圆形
                innerCmd.layerID = 800; // 使用相同的layerID，通过RenderPriority控制顺序
                innerCmd.depth = static_cast<float>(depth) - 0.01f; // 内圈在外圈之上（更小的depth值表示更靠前）
                m_commandBuffer.AddCircle(innerCmd);
            }
            
            // 绘制标签文本
            if (m_defaultFont && !radioWidget->GetLabel().empty()) {
                std::string label = radioWidget->GetLabel();
                
                auto tempText = CreateRef<Text>(m_defaultFont);
                tempText->SetString(label);
                tempText->SetAlignment(TextAlignment::Left);
                tempText->EnsureUpdated();
                Vector2 textSize = tempText->GetSize();
                
                UITextCommand textCmd;
                textCmd.transform = CreateRef<Transform>();
                float labelX = position.x() + circleSize + circlePadding * 2.0f;
                float labelY = position.y() + size.y() * 0.5f - textSize.y() * 0.5f;
                textCmd.transform->SetPosition(Vector3(labelX,
                                                       labelY,
                                                       static_cast<float>(-depth) * 0.001f));
                textCmd.text = label;
                textCmd.font = m_defaultFont;
                textCmd.color = colorSet.text;
                textCmd.offset = Vector2(0.0f, 0.0f); // Left对齐
                textCmd.layerID = 800;
                textCmd.depth = static_cast<float>(depth);
                
                m_commandBuffer.AddText(textCmd);
            }
            
            if (m_debugConfig && m_debugConfig->drawDebugRects) {
                UIDebugRectCommand debugRectCmd;
                debugRectCmd.rect = Rect(position.x(), position.y(), size.x(), size.y());
                debugRectCmd.color = Color(0.4f, 0.4f, 0.8f, 0.35f);
                debugRectCmd.thickness = 2.0f;
                debugRectCmd.layerID = 999;
                debugRectCmd.depth = static_cast<float>(depth);
                m_commandBuffer.AddDebugRect(debugRectCmd);
            }
        } else if (const auto* colorPickerWidget = dynamic_cast<const UIColorPicker*>(&widget)) {
            // 使用主题系统获取颜色
            const UITheme* theme = nullptr;
            if (m_themeManager) {
                theme = &m_themeManager->GetCurrentTheme();
            }
            
            const bool enabled = colorPickerWidget->IsEnabled();
            const bool hovered = colorPickerWidget->IsHovered();
            
            // ColorPicker 使用 button 主题颜色
            const UIThemeColorSet& colorSet = theme
                ? theme->GetWidgetColorSet("button",
                                          hovered,
                                          false,  // pressed
                                          !enabled,
                                          false)  // active
                : UITheme::CreateDefault().GetWidgetColorSet("button",
                                                            hovered,
                                                            false,
                                                            !enabled,
                                                            false);
            
            // 绘制颜色预览色块（如果启用）
            if (colorPickerWidget->IsShowPreview()) {
                const float previewPadding = 8.0f;
                const float previewSize = size.y() - previewPadding * 2.0f;
                const float previewX = position.x() + previewPadding;
                const float previewY = position.y() + previewPadding;
                
                // 绘制颜色预览（圆角矩形）
                const Color& previewColor = colorPickerWidget->GetColor();
                UIRoundedRectangleCommand previewCmd;
                previewCmd.rect = Rect(previewX, previewY, previewSize, previewSize);
                previewCmd.cornerRadius = 4.0f;
                previewCmd.fillColor = previewColor;
                previewCmd.strokeColor = colorSet.outline;
                previewCmd.strokeWidth = 2.0f;
                previewCmd.filled = true;
                previewCmd.stroked = true;
                previewCmd.segments = 8;
                previewCmd.layerID = 800;
                previewCmd.depth = static_cast<float>(depth);
                m_commandBuffer.AddRoundedRectangle(previewCmd);
            }
            
            // 绘制RGB值文本
            if (m_defaultFont) {
                char colorText[64];
                if (colorPickerWidget->IsShowAlpha()) {
                    snprintf(colorText, sizeof(colorText), "R:%.2f G:%.2f B:%.2f A:%.2f",
                            colorPickerWidget->GetR(),
                            colorPickerWidget->GetG(),
                            colorPickerWidget->GetB(),
                            colorPickerWidget->GetA());
                } else {
                    snprintf(colorText, sizeof(colorText), "R:%.2f G:%.2f B:%.2f",
                            colorPickerWidget->GetR(),
                            colorPickerWidget->GetG(),
                            colorPickerWidget->GetB());
                }
                
                auto tempText = CreateRef<Text>(m_defaultFont);
                tempText->SetString(colorText);
                tempText->SetAlignment(TextAlignment::Left);
                tempText->EnsureUpdated();
                Vector2 textSize = tempText->GetSize();
                
                UITextCommand textCmd;
                textCmd.transform = CreateRef<Transform>();
                float textX = position.x() + (colorPickerWidget->IsShowPreview() ? size.y() + 8.0f : 8.0f);
                float textY = position.y() + size.y() * 0.5f - textSize.y() * 0.5f;
                textCmd.transform->SetPosition(Vector3(textX,
                                                       textY,
                                                       static_cast<float>(-depth) * 0.001f));
                textCmd.text = colorText;
                textCmd.font = m_defaultFont;
                textCmd.color = colorSet.text;
                textCmd.offset = Vector2(0.0f, 0.0f); // Left对齐
                textCmd.layerID = 800;
                textCmd.depth = static_cast<float>(depth);
                
                m_commandBuffer.AddText(textCmd);
            }
            
            if (m_debugConfig && m_debugConfig->drawDebugRects) {
                UIDebugRectCommand debugRectCmd;
                debugRectCmd.rect = Rect(position.x(), position.y(), size.x(), size.y());
                debugRectCmd.color = Color(0.8f, 0.4f, 0.4f, 0.35f);
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


