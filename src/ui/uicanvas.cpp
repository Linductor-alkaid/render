#include "render/ui/uicanvas.h"

#include <algorithm>
#include <cmath>

#include "render/application/app_context.h"
#include "render/application/scene_types.h"
#include "render/renderer.h"

namespace Render::UI {

void UICanvas::Initialize(Application::AppContext& ctx) {
    if (m_initialized) {
        return;
    }

    ctx.ValidateOrThrow("UICanvas::Initialize");
    m_initialized = true;

    Application::FrameUpdateArgs initFrame{};
    SyncWithRenderer(ctx, initFrame);
}

void UICanvas::Shutdown(Application::AppContext&) {
    if (!m_initialized) {
        return;
    }

    m_initialized = false;
}

void UICanvas::BeginFrame(const Application::FrameUpdateArgs& frame, Application::AppContext& ctx) {
    if (!m_initialized) {
        Initialize(ctx);
    }

    m_state.absoluteTime = static_cast<float>(frame.absoluteTime);
    m_state.deltaTime = frame.deltaTime;

    SyncWithRenderer(ctx, frame);
}

void UICanvas::EndFrame(const Application::FrameUpdateArgs&, Application::AppContext&) {
    // 预留：将来可以在此处理动画过期、状态清理等逻辑
}

void UICanvas::SetScaleMode(UIScaleMode mode) {
    m_config.scaleMode = mode;
}

void UICanvas::SetReferenceResolution(int32_t width, int32_t height) {
    m_config.SetReferenceResolution(width, height);
}

void UICanvas::SetReferenceDpi(float dpi) {
    if (dpi > 0.0f) {
        m_config.referenceDpi = dpi;
    }
}

void UICanvas::SetFocus(bool focus) {
    m_state.hasFocus = focus;
}

void UICanvas::SetCursorPosition(const Vector2& cursorPosition) {
    m_state.cursorPosition = cursorPosition;
}

void UICanvas::SyncWithRenderer(Application::AppContext& ctx, const Application::FrameUpdateArgs&) {
    if (!ctx.renderer) {
        return;
    }

    const int width = ctx.renderer->GetWidth();
    const int height = ctx.renderer->GetHeight();

    if (width != m_state.windowWidth || height != m_state.windowHeight) {
        m_state.windowWidth = width;
        m_state.windowHeight = height;
    }

    m_state.scaleFactor = CalculateScale(m_state.windowWidth, m_state.windowHeight);
}

float UICanvas::CalculateScale(int32_t windowWidth, int32_t windowHeight) const {
    if (windowWidth <= 0 || windowHeight <= 0 || m_config.referenceWidth <= 0 || m_config.referenceHeight <= 0) {
        return 1.0f;
    }

    const float w = static_cast<float>(windowWidth);
    const float h = static_cast<float>(windowHeight);
    const float refW = static_cast<float>(m_config.referenceWidth);
    const float refH = static_cast<float>(m_config.referenceHeight);

    switch (m_config.scaleMode) {
    case UIScaleMode::Fixed:
        return 1.0f;
    case UIScaleMode::MatchWidth:
        return w / refW;
    case UIScaleMode::MatchHeight:
        return h / refH;
    case UIScaleMode::ScaleToFit:
    default:
        return std::min(w / refW, h / refH);
    }
}

} // namespace Render::UI


