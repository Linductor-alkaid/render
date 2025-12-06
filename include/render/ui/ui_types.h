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

namespace Render::UI {

enum class UIWidgetDirtyFlag : uint32_t {
    None = 0,
    Layout = 1u << 0,
    Visual = 1u << 1,
    Children = 1u << 2,
    All = 0xFFFFFFFFu
};

constexpr UIWidgetDirtyFlag operator|(UIWidgetDirtyFlag lhs, UIWidgetDirtyFlag rhs) noexcept {
    return static_cast<UIWidgetDirtyFlag>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr UIWidgetDirtyFlag operator&(UIWidgetDirtyFlag lhs, UIWidgetDirtyFlag rhs) noexcept {
    return static_cast<UIWidgetDirtyFlag>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

constexpr UIWidgetDirtyFlag& operator|=(UIWidgetDirtyFlag& lhs, UIWidgetDirtyFlag rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

constexpr UIWidgetDirtyFlag& operator&=(UIWidgetDirtyFlag& lhs, UIWidgetDirtyFlag rhs) noexcept {
    lhs = lhs & rhs;
    return lhs;
}

enum class UIVisibility : uint8_t {
    Visible,
    Hidden,
    Collapsed
};

} // namespace Render::UI


