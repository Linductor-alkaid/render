#pragma once

#include <cstdint>

#include "render/types.h"

namespace Render {
class Renderer;
}

namespace Render::Application {
struct FrameUpdateArgs;
struct AppContext;
} // namespace Render::Application

namespace Render::UI {

enum class UIScaleMode {
    Fixed,
    ScaleToFit,
    MatchWidth,
    MatchHeight
};

struct UICanvasConfig {
    int32_t referenceWidth = 1920;
    int32_t referenceHeight = 1080;
    float referenceDpi = 96.0f;
    UIScaleMode scaleMode = UIScaleMode::ScaleToFit;

    void SetReferenceResolution(int32_t width, int32_t height) {
        referenceWidth = width > 0 ? width : 1920;
        referenceHeight = height > 0 ? height : 1080;
    }
};

struct UICanvasState {
    int32_t windowWidth = 0;
    int32_t windowHeight = 0;
    float dpiScale = 1.0f;
    float scaleFactor = 1.0f;
    bool hasFocus = true;
    Vector2 cursorPosition = Vector2::Zero();
    float absoluteTime = 0.0f;
    float deltaTime = 0.0f;

    [[nodiscard]] Vector2 WindowSize() const {
        return Vector2(static_cast<float>(windowWidth), static_cast<float>(windowHeight));
    }
};

class UICanvas {
public:
    UICanvas() = default;
    ~UICanvas() = default;

    void Initialize(Application::AppContext& ctx);
    void Shutdown(Application::AppContext& ctx);

    void BeginFrame(const Application::FrameUpdateArgs& frame, Application::AppContext& ctx);
    void EndFrame(const Application::FrameUpdateArgs& frame, Application::AppContext& ctx);

    void SetScaleMode(UIScaleMode mode);
    void SetReferenceResolution(int32_t width, int32_t height);
    void SetReferenceDpi(float dpi);

    void SetFocus(bool focus);
    void SetCursorPosition(const Vector2& cursorPosition);

    [[nodiscard]] const UICanvasConfig& GetConfig() const { return m_config; }
    [[nodiscard]] const UICanvasState& GetState() const { return m_state; }

private:
    void SyncWithRenderer(Application::AppContext& ctx, const Application::FrameUpdateArgs& frame);
    float CalculateScale(int32_t windowWidth, int32_t windowHeight) const;

    bool m_initialized = false;
    UICanvasConfig m_config{};
    UICanvasState m_state{};
};

} // namespace Render::UI


