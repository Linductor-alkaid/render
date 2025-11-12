#pragma once

#include <memory>
#include <vector>

#include "render/types.h"

namespace Render::UI {

class UIWidget;
class UIWidgetTree;

enum class UILayoutType {
    Container,
    Leaf
};

enum class UILayoutDirection {
    Vertical,
    Horizontal
};

struct UILayoutMetrics {
    Vector2 position = Vector2::Zero();
    Vector2 size = Vector2::Zero();
};

struct UILayoutProperties {
    UILayoutType type = UILayoutType::Container;
    UILayoutDirection direction = UILayoutDirection::Vertical;
    Vector2 padding = Vector2::Zero();
    float spacing = 0.0f;
    Vector2 minSize = Vector2::Zero();
    Vector2 preferredSize = Vector2::Zero();
    float flexGrow = 0.0f;
    float flexShrink = 1.0f;
    bool autoSize = true;
};

class UILayoutNode {
public:
    explicit UILayoutNode(UIWidget* widget);

    UILayoutNode(const UILayoutNode&) = delete;
    UILayoutNode& operator=(const UILayoutNode&) = delete;

    UIWidget* GetWidget() noexcept { return m_widget; }
    const UIWidget* GetWidget() const noexcept { return m_widget; }

    UILayoutProperties& Properties() noexcept { return m_properties; }
    const UILayoutProperties& Properties() const noexcept { return m_properties; }

    UILayoutMetrics& Metrics() noexcept { return m_metrics; }
    const UILayoutMetrics& Metrics() const noexcept { return m_metrics; }

    UILayoutNode* AddChild(std::unique_ptr<UILayoutNode> child);
    void ClearChildren();

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

private:
    UIWidget* m_widget = nullptr;
    UILayoutProperties m_properties{};
    UILayoutMetrics m_metrics{};
    std::vector<std::unique_ptr<UILayoutNode>> m_children;
};

struct UILayoutContext {
    std::unique_ptr<UILayoutNode> root;

    void Clear() { root.reset(); }
};

class UILayoutEngine {
public:
    static void SyncTree(UIWidgetTree& widgetTree, const Vector2& canvasSize, UILayoutContext& context);
};

} // namespace Render::UI


