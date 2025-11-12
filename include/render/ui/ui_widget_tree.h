#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "render/ui/ui_widget.h"

namespace Render::UI {

class UIWidgetTree {
public:
    UIWidgetTree();
    ~UIWidgetTree();

    UIWidgetTree(const UIWidgetTree&) = delete;
    UIWidgetTree& operator=(const UIWidgetTree&) = delete;

    void SetRoot(UIWidgetPtr root);

    [[nodiscard]] UIWidget* GetRoot() noexcept { return m_root.get(); }
    [[nodiscard]] const UIWidget* GetRoot() const noexcept { return m_root.get(); }

    [[nodiscard]] bool IsEmpty() const noexcept { return m_root == nullptr; }

    UIWidget* Find(std::string_view id) noexcept;
    const UIWidget* Find(std::string_view id) const noexcept;

    void Traverse(const std::function<void(UIWidget&)>& visitor);
    void Traverse(const std::function<void(const UIWidget&)>& visitor) const;

private:
    void SetRootInternal(UIWidgetPtr root);

    UIWidgetPtr m_root;
};

} // namespace Render::UI


