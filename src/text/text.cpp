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
#include "render/text/text.h"
#include "render/logger.h"

namespace Render {

Text::Text()
    : m_font(nullptr)
    , m_color(Color::White())
    , m_wrapWidth(0)
    , m_dirty(true)
    , m_textureSize(Vector2::Zero())
    , m_alignment(TextAlignment::Left) {
}

Text::Text(const FontPtr& font)
    : Text() {
    m_font = font;
}

void Text::SetFont(const FontPtr& font) {
    std::unique_lock lock(m_mutex);
    if (m_font != font) {
        m_font = font;
        m_dirty = true;
    }
}

FontPtr Text::GetFont() const {
    std::shared_lock lock(m_mutex);
    return m_font;
}

void Text::SetString(const std::string& text) {
    std::unique_lock lock(m_mutex);
    if (m_text != text) {
        m_text = text;
        m_dirty = true;
    }
}

const std::string& Text::GetString() const {
    std::shared_lock lock(m_mutex);
    return m_text;
}

void Text::SetColor(const Color& color) {
    std::unique_lock lock(m_mutex);
    m_color = color;
}

Color Text::GetColor() const {
    std::shared_lock lock(m_mutex);
    return m_color;
}

void Text::SetWrapWidth(int wrapWidth) {
    std::unique_lock lock(m_mutex);
    if (m_wrapWidth != wrapWidth) {
        m_wrapWidth = wrapWidth;
        m_dirty = true;
    }
}

int Text::GetWrapWidth() const {
    std::shared_lock lock(m_mutex);
    return m_wrapWidth;
}

void Text::SetAlignment(TextAlignment alignment) {
    std::unique_lock lock(m_mutex);
    if (m_alignment != alignment) {
        m_alignment = alignment;
    }
}

TextAlignment Text::GetAlignment() const {
    std::shared_lock lock(m_mutex);
    return m_alignment;
}

bool Text::EnsureUpdated() const {
    std::unique_lock lock(m_mutex);
    if (!m_dirty) {
        return m_texture != nullptr || m_text.empty();
    }
    return UpdateTexture();
}

Ref<Texture> Text::GetTexture() const {
    EnsureUpdated();
    std::shared_lock lock(m_mutex);
    return m_texture;
}

Vector2 Text::GetSize() const {
    EnsureUpdated();
    std::shared_lock lock(m_mutex);
    return m_textureSize;
}

void Text::MarkDirty() const {
    std::unique_lock lock(m_mutex);
    m_dirty = true;
}

bool Text::UpdateTexture() const {
    if (!m_font || !m_font->IsValid()) {
        m_texture.reset();
        m_textureSize = Vector2::Zero();
        m_dirty = false;
        return false;
    }

    RasterizedText rasterized = m_font->RenderText(m_text, m_wrapWidth);
    m_texture = rasterized.texture;
    m_textureSize = rasterized.size;
    m_dirty = false;

    return m_texture != nullptr || m_text.empty();
}

} // namespace Render


