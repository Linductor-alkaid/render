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
#pragma once

#include "render/types.h"
#include "render/texture.h"
#include <memory>
#include <vector>
#include <string>

namespace Render {

struct SpriteFrame {
    Rect uv;
    Vector2 size;
    Vector2 pivot;
};

class Sprite {
public:
    Sprite();

    void SetTexture(const Ref<Texture>& texture);
    [[nodiscard]] Ref<Texture> GetTexture() const;

    void SetFrame(const SpriteFrame& frame);
    [[nodiscard]] const SpriteFrame& GetFrame() const;

    void SetTint(const Color& color);
    [[nodiscard]] Color GetTint() const;

    void SetFlip(bool flipX, bool flipY);
    [[nodiscard]] bool IsFlipX() const;
    [[nodiscard]] bool IsFlipY() const;

    void SetUserData(int userData);
    [[nodiscard]] int GetUserData() const;

private:
    Ref<Texture> m_texture;
    SpriteFrame m_frame{};
    Color m_tint{1.0f, 1.0f, 1.0f, 1.0f};
    bool m_flipX = false;
    bool m_flipY = false;
    int m_userData = 0;
};

} // namespace Render


