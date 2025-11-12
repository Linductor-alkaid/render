#include "render/ui/ui_layout.h"

#include <algorithm>
#include <vector>

#include "render/ui/ui_widget.h"
#include "render/ui/ui_widget_tree.h"

namespace Render::UI {

UILayoutNode::UILayoutNode(UIWidget* widget)
    : m_widget(widget) {}

UILayoutNode* UILayoutNode::AddChild(std::unique_ptr<UILayoutNode> child) {
    if (!child) {
        return nullptr;
    }
    auto* raw = child.get();
    m_children.push_back(std::move(child));
    return raw;
}

void UILayoutNode::ClearChildren() {
    m_children.clear();
}

namespace {

std::unique_ptr<UILayoutNode> BuildNode(UIWidget& widget) {
    auto node = std::make_unique<UILayoutNode>(&widget);
    widget.ForEachChild([&](UIWidget& child) {
        auto childNode = BuildNode(child);
        node->AddChild(std::move(childNode));
    });
    return node;
}

void ApplyAbsoluteLayout(UILayoutNode& node,
                         const Vector2& position,
                         const Vector2& size) {
    UIWidget* widget = node.GetWidget();
    if (widget) {
        Rect rect(position.x(), position.y(), size.x(), size.y());
        widget->SetLayoutRect(rect);

        node.Metrics().position = position;
        node.Metrics().size = size;
    }

    Vector4 padding = Vector4::Zero();
    if (widget) {
        padding = widget->GetPadding();
    }

    float contentX = position.x() + padding.x();
    float contentY = position.y() + padding.y();
    float contentWidth = std::max(size.x() - padding.x() - padding.z(), 0.0f);
    float contentHeight = std::max(size.y() - padding.y() - padding.w(), 0.0f);

    constexpr float kDefaultSpacing = 16.0f;
    std::vector<UILayoutNode*> childNodes;
    std::vector<Vector2> childSizes;
    float totalHeight = 0.0f;

    node.ForEachChild([&](UILayoutNode& childNode) {
        UIWidget* childWidget = childNode.GetWidget();
        Vector2 preferred = childWidget ? childWidget->GetPreferredSize() : Vector2::Zero();
        Vector2 minSize = childWidget ? childWidget->GetMinSize() : Vector2::Zero();

        Vector2 childSize = preferred;
        if (childSize.x() <= 0.0f) {
            childSize.x() = contentWidth;
        }
        if (childSize.y() <= 0.0f) {
            childSize.y() = minSize.y();
        }
        if (childSize.y() <= 0.0f) {
            childSize.y() = 48.0f;
        }

        childSize.x() = std::max(childSize.x(), minSize.x());

        childNodes.push_back(&childNode);
        childSizes.push_back(childSize);
        totalHeight += childSize.y();
    });

    if (!childNodes.empty()) {
        totalHeight += kDefaultSpacing * static_cast<float>(childNodes.size() - 1);
    }

    bool parentIsRoot = widget ? widget->GetParent() == nullptr : false;
    float currentY = contentY;
    if (parentIsRoot) {
        float centeredOffset = (contentHeight - totalHeight) * 0.5f;
        if (centeredOffset > 0.0f) {
            currentY += centeredOffset;
        }
    }

    for (size_t i = 0; i < childNodes.size(); ++i) {
        UILayoutNode* childNode = childNodes[i];
        const Vector2& childSize = childSizes[i];

        float childX = contentX;
        if (childSize.x() < contentWidth) {
            childX = contentX + (contentWidth - childSize.x()) * 0.5f;
        }

        float maxY = contentY + contentHeight - childSize.y();
        float childY = std::min(currentY, maxY);
        Vector2 childPosition(childX, childY);

        ApplyAbsoluteLayout(*childNode, childPosition, childSize);
        currentY = childY + childSize.y();
        if (i + 1 < childNodes.size()) {
            currentY += kDefaultSpacing;
        }
    }
}

} // namespace

void UILayoutEngine::SyncTree(UIWidgetTree& widgetTree,
                              const Vector2& canvasSize,
                              UILayoutContext& context) {
    UIWidget* rootWidget = widgetTree.GetRoot();
    if (!rootWidget) {
        context.Clear();
        return;
    }

    context.root = BuildNode(*rootWidget);
    ApplyAbsoluteLayout(*context.root, Vector2::Zero(), canvasSize);
}

} // namespace Render::UI


