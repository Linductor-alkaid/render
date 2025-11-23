#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "render/types.h"
#include "render/ui/ui_types.h"
#include "render/ui/ui_layout.h"

namespace Render::UI {

class UIWidget {
public:
    explicit UIWidget(std::string id);
    virtual ~UIWidget();

    UIWidget(const UIWidget&) = delete;
    UIWidget& operator=(const UIWidget&) = delete;

    [[nodiscard]] const std::string& GetId() const noexcept { return m_id; }
    [[nodiscard]] UIWidget* GetParent() noexcept { return m_parent; }
    [[nodiscard]] const UIWidget* GetParent() const noexcept { return m_parent; }

    [[nodiscard]] bool IsRoot() const noexcept { return m_parent == nullptr; }

    void SetVisibility(UIVisibility visibility);
    [[nodiscard]] UIVisibility GetVisibility() const noexcept { return m_visibility; }
    [[nodiscard]] bool IsVisible() const noexcept { return m_visibility == UIVisibility::Visible; }

    void SetEnabled(bool enabled);
    [[nodiscard]] bool IsEnabled() const noexcept { return m_enabled; }

    void MarkDirty(UIWidgetDirtyFlag flags = UIWidgetDirtyFlag::All) noexcept;
    [[nodiscard]] UIWidgetDirtyFlag GetDirtyFlags() const noexcept { return m_dirtyFlags; }
    void ClearDirty(UIWidgetDirtyFlag flags = UIWidgetDirtyFlag::All) noexcept;

    void SetLayoutRect(const Rect& rect) noexcept;
    [[nodiscard]] const Rect& GetLayoutRect() const noexcept { return m_layoutRect; }
    [[nodiscard]] Vector2 GetPreferredSize() const noexcept { return m_preferredSize; }
    void SetPreferredSize(const Vector2& size) noexcept;
    [[nodiscard]] Vector2 GetMinSize() const noexcept { return m_minSize; }
    void SetMinSize(const Vector2& size) noexcept;
    [[nodiscard]] Vector4 GetPadding() const noexcept { return m_padding; }
    void SetPadding(const Vector4& padding) noexcept;

    // Flex 布局属性
    void SetLayoutDirection(UILayoutDirection direction) noexcept;
    [[nodiscard]] UILayoutDirection GetLayoutDirection() const noexcept { return m_layoutDirection; }
    
    void SetJustifyContent(UIFlexJustifyContent justifyContent) noexcept;
    [[nodiscard]] UIFlexJustifyContent GetJustifyContent() const noexcept { return m_justifyContent; }
    
    void SetAlignItems(UIFlexAlignItems alignItems) noexcept;
    [[nodiscard]] UIFlexAlignItems GetAlignItems() const noexcept { return m_alignItems; }
    
    void SetAlignSelf(UIFlexAlignSelf alignSelf) noexcept;
    [[nodiscard]] UIFlexAlignSelf GetAlignSelf() const noexcept { return m_alignSelf; }
    
    void SetFlexGrow(float flexGrow) noexcept;
    [[nodiscard]] float GetFlexGrow() const noexcept { return m_flexGrow; }
    
    void SetFlexShrink(float flexShrink) noexcept;
    [[nodiscard]] float GetFlexShrink() const noexcept { return m_flexShrink; }
    
    void SetSpacing(float spacing) noexcept;
    [[nodiscard]] float GetSpacing() const noexcept { return m_spacing; }
    
    // Grid 布局属性
    void SetLayoutMode(UILayoutMode mode) noexcept;
    [[nodiscard]] UILayoutMode GetLayoutMode() const noexcept { return m_layoutMode; }
    
    void SetGridColumns(int columns) noexcept;
    [[nodiscard]] int GetGridColumns() const noexcept { return m_gridColumns; }
    
    void SetGridRows(int rows) noexcept;
    [[nodiscard]] int GetGridRows() const noexcept { return m_gridRows; }
    
    void SetGridCellSpacing(const Vector2& spacing) noexcept;
    [[nodiscard]] const Vector2& GetGridCellSpacing() const noexcept { return m_gridCellSpacing; }
    
    void SetGridColumnWidths(const std::vector<float>& widths) noexcept;
    [[nodiscard]] const std::vector<float>& GetGridColumnWidths() const noexcept { return m_gridColumnWidths; }
    
    void SetGridRowHeights(const std::vector<float>& heights) noexcept;
    [[nodiscard]] const std::vector<float>& GetGridRowHeights() const noexcept { return m_gridRowHeights; }
    
    // Grid 项目属性（子节点使用）
    void SetGridColumnSpan(int start, int end) noexcept;
    [[nodiscard]] std::pair<int, int> GetGridColumnSpan() const noexcept { return {m_gridColumnStart, m_gridColumnEnd}; }
    
    void SetGridRowSpan(int start, int end) noexcept;
    [[nodiscard]] std::pair<int, int> GetGridRowSpan() const noexcept { return {m_gridRowStart, m_gridRowEnd}; }

    UIWidget* AddChild(std::unique_ptr<UIWidget> child);
    std::unique_ptr<UIWidget> RemoveChild(std::string_view id);

    UIWidget* FindById(std::string_view id) noexcept;
    const UIWidget* FindById(std::string_view id) const noexcept;

    template <typename Visitor>
    void ForEachChild(Visitor&& visitor) {
        for (auto& child : m_children) {
            visitor(*child);
        }
    }

    template <typename Visitor>
    void ForEachChild(Visitor&& visitor) const {
        for (const auto& child : m_children) {
            visitor(*child);
        }
    }

protected:
    virtual void OnChildAdded(UIWidget& child);
    virtual void OnChildRemoved(UIWidget& child);

    virtual void OnVisibilityChanged(UIVisibility oldVisibility, UIVisibility newVisibility);
    virtual void OnEnableChanged(bool enabled);

    // 输入与焦点回调（默认仅记录日志，可在派生类中重写）
    virtual void OnFocusGained();
    virtual void OnFocusLost();
    virtual void OnMouseEnter();
    virtual void OnMouseLeave();
    virtual void OnMouseButton(uint8_t button, bool pressed, const Vector2& position);
    virtual void OnMouseWheel(const Vector2& offset);
    virtual void OnKey(int scancode, bool pressed, bool repeat);
    virtual void OnTextInput(const std::string& text);
    virtual void OnMouseClick(uint8_t button, const Vector2& position);

private:
    UIWidget* m_parent = nullptr;
    std::string m_id;
    std::vector<std::unique_ptr<UIWidget>> m_children;
    Rect m_layoutRect{};
    Vector2 m_preferredSize{0.0f, 0.0f};
    Vector2 m_minSize{0.0f, 0.0f};
    Vector4 m_padding{0.0f, 0.0f, 0.0f, 0.0f};
    UIVisibility m_visibility = UIVisibility::Visible;
    bool m_enabled = true;
    UIWidgetDirtyFlag m_dirtyFlags = UIWidgetDirtyFlag::All;
    
    // 布局模式
    UILayoutMode m_layoutMode = UILayoutMode::Flex;
    
    // Flex 布局属性
    UILayoutDirection m_layoutDirection = UILayoutDirection::Vertical;
    UIFlexJustifyContent m_justifyContent = UIFlexJustifyContent::FlexStart;
    UIFlexAlignItems m_alignItems = UIFlexAlignItems::FlexStart;
    UIFlexAlignSelf m_alignSelf = UIFlexAlignSelf::Auto;
    float m_flexGrow = 0.0f;
    float m_flexShrink = 1.0f;
    float m_spacing = 0.0f;
    
    // Grid 布局属性
    int m_gridColumns = 1;
    int m_gridRows = 0;
    Vector2 m_gridCellSpacing = Vector2::Zero();
    std::vector<float> m_gridColumnWidths;
    std::vector<float> m_gridRowHeights;
    
    // Grid 项目属性（子节点使用）
    int m_gridColumnStart = -1;
    int m_gridColumnEnd = -1;
    int m_gridRowStart = -1;
    int m_gridRowEnd = -1;

    UIWidget* FindByIdRecursive(std::string_view id) noexcept;
    const UIWidget* FindByIdRecursive(std::string_view id) const noexcept;

    friend class UIWidgetTree;
    friend class UIInputRouter;
};

using UIWidgetPtr = std::unique_ptr<UIWidget>;

} // namespace Render::UI


