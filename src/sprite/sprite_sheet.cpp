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
#include "render/sprite/sprite_sheet.h"
#include "render/error.h"

namespace Render {

SpriteSheet::SpriteSheet()
    : m_texture(nullptr) {
}

void SpriteSheet::SetTexture(const Ref<Texture>& texture) {
    m_texture = texture;
}

Ref<Texture> SpriteSheet::GetTexture() const {
    return m_texture;
}

void SpriteSheet::AddFrame(const std::string& name, const SpriteFrame& frame) {
    m_frames[name] = frame;
}

bool SpriteSheet::HasFrame(const std::string& name) const {
    return m_frames.find(name) != m_frames.end();
}

const SpriteFrame& SpriteSheet::GetFrame(const std::string& name) const {
    auto it = m_frames.find(name);
    if (it == m_frames.end()) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::ResourceNotFound, "SpriteSheet: frame not found: " + name));
        static SpriteFrame empty{Rect(0, 0, 1, 1), Vector2(1.0f, 1.0f), Vector2(0.5f, 0.5f)};
        return empty;
    }
    return it->second;
}

const std::unordered_map<std::string, SpriteFrame>& SpriteSheet::GetAllFrames() const {
    return m_frames;
}

} // namespace Render


