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


