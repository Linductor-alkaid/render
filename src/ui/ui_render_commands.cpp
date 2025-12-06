/*
 * Copyright (c) 2025 Li Chaoyu
 * 
 * This file is part of Render.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * For commercial licensing, please contact: 2052046346@qq.com
 */
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


