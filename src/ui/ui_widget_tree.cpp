#include "render/ui/ui_widget_tree.h"

#include <queue>
#include <utility>

namespace Render::UI {

UIWidgetTree::UIWidgetTree() = default;
UIWidgetTree::~UIWidgetTree() = default;

void UIWidgetTree::SetRoot(UIWidgetPtr root) {
    SetRootInternal(std::move(root));
}

void UIWidgetTree::SetRootInternal(UIWidgetPtr root) {
    if (root) {
        root->m_parent = nullptr;
    }
    m_root = std::move(root);
}

UIWidget* UIWidgetTree::Find(std::string_view id) noexcept {
    if (!m_root) {
        return nullptr;
    }
    return m_root->FindById(id);
}

const UIWidget* UIWidgetTree::Find(std::string_view id) const noexcept {
    if (!m_root) {
        return nullptr;
    }
    return m_root->FindById(id);
}

void UIWidgetTree::Traverse(const std::function<void(UIWidget&)>& visitor) {
    if (!m_root) {
        return;
    }

    std::queue<UIWidget*> queue;
    queue.push(m_root.get());

    while (!queue.empty()) {
        UIWidget* current = queue.front();
        queue.pop();

        visitor(*current);

        current->ForEachChild([&](UIWidget& child) {
            queue.push(&child);
        });
    }
}

void UIWidgetTree::Traverse(const std::function<void(const UIWidget&)>& visitor) const {
    if (!m_root) {
        return;
    }

    std::queue<const UIWidget*> queue;
    queue.push(m_root.get());

    while (!queue.empty()) {
        const UIWidget* current = queue.front();
        queue.pop();

        visitor(*current);

        current->ForEachChild([&](const UIWidget& child) {
            queue.push(&child);
        });
    }
}

} // namespace Render::UI


