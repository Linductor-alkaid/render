#pragma once

#include <string>
#include <vector>

#include "render/renderable.h"
#include "render/transform.h"
#include "render/types.h"
#include "render/texture.h"

namespace Render {
class Font;
}

namespace Render::UI {

enum class UIRenderCommandType {
    Sprite,
    Text,
    DebugRect,
    Line,
    BezierCurve,
    Rectangle,
    Circle,
    RoundedRectangle,
    Polygon
};

struct UISpriteCommand {
    Ref<Transform> transform;
    Ref<Texture> texture;
    Rect sourceRect;
    Vector2 size{1.0f, 1.0f};
    Color tint{1.0f, 1.0f, 1.0f, 1.0f};
    uint32_t layerID = 800;
    float depth = 0.0f;
    // 标志位：明确标识这是光标命令还是图集命令
    // true = 光标命令（使用1x1纯色纹理），false = 图集命令（使用图集纹理）
    bool isCursor = false;
};

struct UITextCommand {
    Ref<Transform> transform;
    std::string text;
    Ref<Render::Font> font;
    float fontSize = 18.0f;
    Color color{1.0f, 1.0f, 1.0f, 1.0f};
    Vector2 offset = Vector2::Zero();
    uint32_t layerID = 800;
    float depth = 0.0f;
};

struct UIDebugRectCommand {
    Rect rect;
    Color color{0.0f, 1.0f, 0.0f, 1.0f};
    float thickness = 1.0f;
    float depth = 0.0f;
    uint32_t layerID = 0;
};

struct UILineCommand {
    Vector2 start;
    Vector2 end;
    float width = 1.0f;
    Color color{1.0f, 1.0f, 1.0f, 1.0f};
    int layerID = 800;
    float depth = 0.0f;
};

struct UIBezierCurveCommand {
    Vector2 p0, p1, p2, p3;  // 控制点
    int segments = 32;        // 分段数
    float width = 1.0f;
    Color color{1.0f, 1.0f, 1.0f, 1.0f};
    int layerID = 800;
    float depth = 0.0f;
};

struct UIRectangleCommand {
    Rect rect;
    Color fillColor{1.0f, 1.0f, 1.0f, 1.0f};
    Color strokeColor{0.0f, 0.0f, 0.0f, 1.0f};
    float strokeWidth = 0.0f;
    bool filled = true;
    bool stroked = false;
    int layerID = 800;
    float depth = 0.0f;
};

struct UICircleCommand {
    Vector2 center;
    float radius = 10.0f;
    Color fillColor{1.0f, 1.0f, 1.0f, 1.0f};
    Color strokeColor{0.0f, 0.0f, 0.0f, 1.0f};
    float strokeWidth = 0.0f;
    bool filled = true;
    bool stroked = false;
    int segments = 32;        // 圆形分段数
    int layerID = 800;
    float depth = 0.0f;
};

struct UIRoundedRectangleCommand {
    Rect rect;
    float cornerRadius = 5.0f;
    Color fillColor{1.0f, 1.0f, 1.0f, 1.0f};
    Color strokeColor{0.0f, 0.0f, 0.0f, 1.0f};
    float strokeWidth = 0.0f;
    bool filled = true;
    bool stroked = false;
    int segments = 8;        // 每个圆角的分段数
    int layerID = 800;
    float depth = 0.0f;
};

struct UIPolygonCommand {
    std::vector<Vector2> vertices;
    Color fillColor{1.0f, 1.0f, 1.0f, 1.0f};
    Color strokeColor{0.0f, 0.0f, 0.0f, 1.0f};
    float strokeWidth = 0.0f;
    bool filled = true;
    bool stroked = false;
    int layerID = 800;
    float depth = 0.0f;
};

struct UIRenderCommand {
    UIRenderCommandType type = UIRenderCommandType::Sprite;
    UISpriteCommand sprite;
    UITextCommand text;
    UIDebugRectCommand debugRect;
    UILineCommand line;
    UIBezierCurveCommand bezierCurve;
    UIRectangleCommand rectangle;
    UICircleCommand circle;
    UIRoundedRectangleCommand roundedRectangle;
    UIPolygonCommand polygon;
};

class UIRenderCommandBuffer {
public:
    void Clear();
    void AddSprite(const UISpriteCommand& cmd);
    void AddText(const UITextCommand& cmd);
    void AddDebugRect(const UIDebugRectCommand& cmd);
    void AddLine(const UILineCommand& cmd);
    void AddBezierCurve(const UIBezierCurveCommand& cmd);
    void AddRectangle(const UIRectangleCommand& cmd);
    void AddCircle(const UICircleCommand& cmd);
    void AddRoundedRectangle(const UIRoundedRectangleCommand& cmd);
    void AddPolygon(const UIPolygonCommand& cmd);

    [[nodiscard]] const std::vector<UIRenderCommand>& GetCommands() const { return m_commands; }

private:
    std::vector<UIRenderCommand> m_commands;
};

} // namespace Render::UI


