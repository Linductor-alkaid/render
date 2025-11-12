#include "render/ui/ui_render_commands.h"

namespace Render::UI {

void UIRenderCommandBuffer::Clear() {
    m_commands.clear();
}

void UIRenderCommandBuffer::AddSprite(const UISpriteCommand& cmd) {
    UIRenderCommand entry;
    entry.type = UIRenderCommandType::Sprite;
    entry.sprite = cmd;
    m_commands.emplace_back(std::move(entry));
}

void UIRenderCommandBuffer::AddText(const UITextCommand& cmd) {
    UIRenderCommand entry;
    entry.type = UIRenderCommandType::Text;
    entry.text = cmd;
    m_commands.emplace_back(std::move(entry));
}

void UIRenderCommandBuffer::AddDebugRect(const UIDebugRectCommand& cmd) {
    UIRenderCommand entry;
    entry.type = UIRenderCommandType::DebugRect;
    entry.debugRect = cmd;
    m_commands.emplace_back(std::move(entry));
}

} // namespace Render::UI


