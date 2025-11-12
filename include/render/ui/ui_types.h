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


