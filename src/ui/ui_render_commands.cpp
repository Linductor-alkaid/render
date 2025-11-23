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

void UIRenderCommandBuffer::AddLine(const UILineCommand& cmd) {
    UIRenderCommand entry;
    entry.type = UIRenderCommandType::Line;
    entry.line = cmd;
    m_commands.emplace_back(std::move(entry));
}

void UIRenderCommandBuffer::AddBezierCurve(const UIBezierCurveCommand& cmd) {
    UIRenderCommand entry;
    entry.type = UIRenderCommandType::BezierCurve;
    entry.bezierCurve = cmd;
    m_commands.emplace_back(std::move(entry));
}

void UIRenderCommandBuffer::AddRectangle(const UIRectangleCommand& cmd) {
    UIRenderCommand entry;
    entry.type = UIRenderCommandType::Rectangle;
    entry.rectangle = cmd;
    m_commands.emplace_back(std::move(entry));
}

void UIRenderCommandBuffer::AddCircle(const UICircleCommand& cmd) {
    UIRenderCommand entry;
    entry.type = UIRenderCommandType::Circle;
    entry.circle = cmd;
    m_commands.emplace_back(std::move(entry));
}

void UIRenderCommandBuffer::AddRoundedRectangle(const UIRoundedRectangleCommand& cmd) {
    UIRenderCommand entry;
    entry.type = UIRenderCommandType::RoundedRectangle;
    entry.roundedRectangle = cmd;
    m_commands.emplace_back(std::move(entry));
}

void UIRenderCommandBuffer::AddPolygon(const UIPolygonCommand& cmd) {
    UIRenderCommand entry;
    entry.type = UIRenderCommandType::Polygon;
    entry.polygon = cmd;
    m_commands.emplace_back(std::move(entry));
}

} // namespace Render::UI


