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

#include "render/sprite/sprite.h"
#include <unordered_map>

namespace Render {

class SpriteSheet {
public:
    SpriteSheet();

    void SetTexture(const Ref<Texture>& texture);
    [[nodiscard]] Ref<Texture> GetTexture() const;

    void AddFrame(const std::string& name, const SpriteFrame& frame);
    [[nodiscard]] bool HasFrame(const std::string& name) const;
    [[nodiscard]] const SpriteFrame& GetFrame(const std::string& name) const;

    [[nodiscard]] const std::unordered_map<std::string, SpriteFrame>& GetAllFrames() const;

private:
    Ref<Texture> m_texture;
    std::unordered_map<std::string, SpriteFrame> m_frames;
};

} // namespace Render


