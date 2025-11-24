#include "render/ui/ui_widget.h"

#include <algorithm>
#include <utility>

namespace Render::UI {

UIWidget::UIWidget(std::string id)
    : m_id(std::move(id)) {
    if (m_id.empty()) {
        m_id = "ui.widget";
    }
}

UIWidget::~UIWidget() = default;

void UIWidget::SetVisibility(UIVisibility visibility) {
    if (m_visibility == visibility) {
        return;
    }

    const auto old = m_visibility;
    m_visibility = visibility;
    MarkDirty(UIWidgetDirtyFlag::Visual);
    OnVisibilityChanged(old, visibility);
}

void UIWidget::SetEnabled(bool enabled) {
    if (m_enabled == enabled) {
        return;
    }

    m_enabled = enabled;
    OnEnableChanged(enabled);
}

void UIWidget::MarkDirty(UIWidgetDirtyFlag flags) noexcept {
    m_dirtyFlags |= flags;
    if (m_parent) {
        m_parent->MarkDirty(UIWidgetDirtyFlag::Children);
    }
}

void UIWidget::ClearDirty(UIWidgetDirtyFlag flags) noexcept {
    if (flags == UIWidgetDirtyFlag::All) {
        m_dirtyFlags = UIWidgetDirtyFlag::None;
        return;
    }
    const auto mask = static_cast<uint32_t>(~static_cast<uint32_t>(flags));
    m_dirtyFlags = static_cast<UIWidgetDirtyFlag>(static_cast<uint32_t>(m_dirtyFlags) & mask);
}

void UIWidget::SetLayoutRect(const Rect& rect) noexcept {
    if (m_layoutRect.x == rect.x &&
        m_layoutRect.y == rect.y &&
        m_layoutRect.width == rect.width &&
        m_layoutRect.height == rect.height) {
        return;
    }

    m_layoutRect = rect;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIWidget::SetPreferredSize(const Vector2& size) noexcept {
    Vector2 clamped = size;
    clamped.x() = std::max(0.0f, clamped.x());
    clamped.y() = std::max(0.0f, clamped.y());
    if ((m_preferredSize - clamped).isZero(1e-3f)) {
        return;
    }
    m_preferredSize = clamped;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIWidget::SetMinSize(const Vector2& size) noexcept {
    Vector2 clamped = size;
    clamped.x() = std::max(0.0f, clamped.x());
    clamped.y() = std::max(0.0f, clamped.y());
    if ((m_minSize - clamped).isZero(1e-3f)) {
        return;
    }
    m_minSize = clamped;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIWidget::SetPadding(const Vector4& padding) noexcept {
    Vector4 clamped = padding;
    clamped.x() = std::max(0.0f, clamped.x());
    clamped.y() = std::max(0.0f, clamped.y());
    clamped.z() = std::max(0.0f, clamped.z());
    clamped.w() = std::max(0.0f, clamped.w());
    if ((m_padding - clamped).isZero(1e-3f)) {
        return;
    }
    m_padding = clamped;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIWidget::SetLayoutDirection(UILayoutDirection direction) noexcept {
    if (m_layoutDirection == direction) {
        return;
    }
    m_layoutDirection = direction;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIWidget::SetJustifyContent(UIFlexJustifyContent justifyContent) noexcept {
    if (m_justifyContent == justifyContent) {
        return;
    }
    m_justifyContent = justifyContent;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIWidget::SetAlignItems(UIFlexAlignItems alignItems) noexcept {
    if (m_alignItems == alignItems) {
        return;
    }
    m_alignItems = alignItems;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIWidget::SetAlignSelf(UIFlexAlignSelf alignSelf) noexcept {
    if (m_alignSelf == alignSelf) {
        return;
    }
    m_alignSelf = alignSelf;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIWidget::SetFlexGrow(float flexGrow) noexcept {
    flexGrow = std::max(0.0f, flexGrow);
    if (std::abs(m_flexGrow - flexGrow) < 1e-3f) {
        return;
    }
    m_flexGrow = flexGrow;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIWidget::SetFlexShrink(float flexShrink) noexcept {
    flexShrink = std::max(0.0f, flexShrink);
    if (std::abs(m_flexShrink - flexShrink) < 1e-3f) {
        return;
    }
    m_flexShrink = flexShrink;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIWidget::SetSpacing(float spacing) noexcept {
    spacing = std::max(0.0f, spacing);
    if (std::abs(m_spacing - spacing) < 1e-3f) {
        return;
    }
    m_spacing = spacing;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIWidget::SetLayoutMode(UILayoutMode mode) noexcept {
    if (m_layoutMode == mode) {
        return;
    }
    m_layoutMode = mode;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIWidget::SetGridColumns(int columns) noexcept {
    if (m_gridColumns == columns) {
        return;
    }
    m_gridColumns = std::max(1, columns);
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIWidget::SetGridRows(int rows) noexcept {
    if (m_gridRows == rows) {
        return;
    }
    m_gridRows = std::max(0, rows);
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIWidget::SetGridCellSpacing(const Vector2& spacing) noexcept {
    if ((m_gridCellSpacing - spacing).isZero(1e-3f)) {
        return;
    }
    m_gridCellSpacing = spacing;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIWidget::SetGridColumnWidths(const std::vector<float>& widths) noexcept {
    m_gridColumnWidths = widths;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIWidget::SetGridRowHeights(const std::vector<float>& heights) noexcept {
    m_gridRowHeights = heights;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIWidget::SetGridColumnSpan(int start, int end) noexcept {
    m_gridColumnStart = start;
    m_gridColumnEnd = end;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

void UIWidget::SetGridRowSpan(int start, int end) noexcept {
    m_gridRowStart = start;
    m_gridRowEnd = end;
    MarkDirty(UIWidgetDirtyFlag::Layout);
}

UIWidget* UIWidget::AddChild(std::unique_ptr<UIWidget> child) {
    if (!child) {
        return nullptr;
    }
    child->m_parent = this;
    auto* raw = child.get();
    m_children.push_back(std::move(child));
    MarkDirty(UIWidgetDirtyFlag::Children);
    OnChildAdded(*raw);
    return raw;
}

std::unique_ptr<UIWidget> UIWidget::RemoveChild(std::string_view id) {
    auto it = std::find_if(m_children.begin(), m_children.end(),
                           [&](const std::unique_ptr<UIWidget>& widget) {
                               return widget && widget->GetId() == id;
                           });
    if (it == m_children.end()) {
        return nullptr;
    }

    auto removed = std::move(*it);
    m_children.erase(it);
    if (removed) {
        removed->m_parent = nullptr;
        OnChildRemoved(*removed);
    }
    MarkDirty(UIWidgetDirtyFlag::Children);
    return removed;
}

UIWidget* UIWidget::FindById(std::string_view id) noexcept {
    if (m_id == id) {
        return this;
    }
    return FindByIdRecursive(id);
}

const UIWidget* UIWidget::FindById(std::string_view id) const noexcept {
    if (m_id == id) {
        return this;
    }
    return FindByIdRecursive(id);
}

UIWidget* UIWidget::FindByIdRecursive(std::string_view id) noexcept {
    for (auto& child : m_children) {
        if (!child) {
            continue;
        }
        if (child->m_id == id) {
            return child.get();
        }
        if (auto* nested = child->FindByIdRecursive(id)) {
            return nested;
        }
    }
    return nullptr;
}

const UIWidget* UIWidget::FindByIdRecursive(std::string_view id) const noexcept {
    for (const auto& child : m_children) {
        if (!child) {
            continue;
        }
        if (child->m_id == id) {
            return child.get();
        }
        if (const auto* nested = child->FindByIdRecursive(id)) {
            return nested;
        }
    }
    return nullptr;
}

void UIWidget::OnChildAdded(UIWidget& child) {
    (void)child;
}

void UIWidget::OnChildRemoved(UIWidget& child) {
    (void)child;
}

void UIWidget::OnVisibilityChanged(UIVisibility, UIVisibility) {}

void UIWidget::OnEnableChanged(bool) {}

void UIWidget::OnFocusGained() {}

void UIWidget::OnFocusLost() {}

void UIWidget::OnMouseEnter() {}

void UIWidget::OnMouseLeave() {}

void UIWidget::OnMouseMove(const Vector2&, const Vector2&) {}

void UIWidget::OnMouseButton(uint8_t, bool, const Vector2&) {}

void UIWidget::OnMouseWheel(const Vector2&) {}

void UIWidget::OnKey(int, bool, bool) {}

void UIWidget::OnTextInput(const std::string&) {}

void UIWidget::OnMouseClick(uint8_t, const Vector2&) {}

} // namespace Render::UI


