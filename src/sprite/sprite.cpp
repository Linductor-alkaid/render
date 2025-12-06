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
#include "render/sprite/sprite.h"

namespace Render {

Sprite::Sprite()
    : m_frame{Rect(0, 0, 1, 1), Vector2(1.0f, 1.0f), Vector2(0.5f, 0.5f)} {
}

void Sprite::SetTexture(const Ref<Texture>& texture) {
    m_texture = texture;
}

Ref<Texture> Sprite::GetTexture() const {
    return m_texture;
}

void Sprite::SetFrame(const SpriteFrame& frame) {
    m_frame = frame;
}

const SpriteFrame& Sprite::GetFrame() const {
    return m_frame;
}

void Sprite::SetTint(const Color& color) {
    m_tint = color;
}

Color Sprite::GetTint() const {
    return m_tint;
}

void Sprite::SetFlip(bool flipX, bool flipY) {
    m_flipX = flipX;
    m_flipY = flipY;
}

bool Sprite::IsFlipX() const {
    return m_flipX;
}

bool Sprite::IsFlipY() const {
    return m_flipY;
}

void Sprite::SetUserData(int userData) {
    m_userData = userData;
}

int Sprite::GetUserData() const {
    return m_userData;
}

} // namespace Render


