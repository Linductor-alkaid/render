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
    DebugRect
};

struct UISpriteCommand {
    Ref<Transform> transform;
    Ref<Texture> texture;
    Rect sourceRect;
    Vector2 size{1.0f, 1.0f};
    Color tint{1.0f, 1.0f, 1.0f, 1.0f};
    uint32_t layerID = 800;
    float depth = 0.0f;
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

struct UIRenderCommand {
    UIRenderCommandType type = UIRenderCommandType::Sprite;
    UISpriteCommand sprite;
    UITextCommand text;
    UIDebugRectCommand debugRect;
};

class UIRenderCommandBuffer {
public:
    void Clear();
    void AddSprite(const UISpriteCommand& cmd);
    void AddText(const UITextCommand& cmd);
    void AddDebugRect(const UIDebugRectCommand& cmd);

    [[nodiscard]] const std::vector<UIRenderCommand>& GetCommands() const { return m_commands; }

private:
    std::vector<UIRenderCommand> m_commands;
};

} // namespace Render::UI


