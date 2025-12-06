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

#include <cstdint>
#include "render/math_utils.h"

namespace Render {
namespace SpriteUI {

/**
 * @brief 九宫格拉伸配置
 */
struct NineSliceSettings {
    Vector4 borderPixels{0.0f, 0.0f, 0.0f, 0.0f}; ///< left, right, top, bottom 像素宽度
    uint8_t fillMode = 0;                          ///< 0 = 拉伸, 1 = 平铺（保留扩展位）

    inline bool IsEnabled() const {
        return borderPixels.x() > 0.0f || borderPixels.y() > 0.0f ||
               borderPixels.z() > 0.0f || borderPixels.w() > 0.0f;
    }
};

enum class SpriteFlipFlags : uint8_t {
    None      = 0,
    FlipX     = 1 << 0,
    FlipY     = 1 << 1,
};

inline SpriteFlipFlags operator|(SpriteFlipFlags a, SpriteFlipFlags b) {
    return static_cast<SpriteFlipFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline SpriteFlipFlags& operator|=(SpriteFlipFlags& a, SpriteFlipFlags b) {
    a = a | b;
    return a;
}

inline bool HasFlag(SpriteFlipFlags flags, SpriteFlipFlags flag) {
    return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(flag)) != 0;
}

} // namespace SpriteUI
} // namespace Render


