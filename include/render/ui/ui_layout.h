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

enum class UIFlexJustifyContent {
    FlexStart,    // 从起始位置对齐
    FlexEnd,      // 从结束位置对齐
    Center,       // 居中对齐
    SpaceBetween, // 两端对齐，中间均匀分布
    SpaceAround,  // 均匀分布，两端有间距
    SpaceEvenly   // 完全均匀分布
};

enum class UIFlexAlignItems {
    FlexStart,  // 从起始位置对齐
    FlexEnd,    // 从结束位置对齐
    Center,     // 居中对齐
    Stretch,    // 拉伸填充
    Baseline    // 基线对齐（暂不支持）
};

enum class UIFlexAlignSelf {
    Auto,       // 继承父容器的 alignItems
    FlexStart,  // 从起始位置对齐
    FlexEnd,    // 从结束位置对齐
    Center,     // 居中对齐
    Stretch,    // 拉伸填充
    Baseline    // 基线对齐（暂不支持）
};

struct UILayoutMetrics {
    Vector2 position = Vector2::Zero();
    Vector2 size = Vector2::Zero();
    Vector2 measuredSize = Vector2::Zero(); // 测量阶段计算出的尺寸
};

enum class UILayoutMode {
    Flex,      // Flex 布局
    Grid,      // Grid 布局
    Absolute   // 绝对布局（向后兼容）
};

struct UIGridProperties {
    int columns = 1;                    // 列数
    int rows = 0;                       // 行数（0 = 自动计算）
    Vector2 cellSpacing = Vector2::Zero(); // 单元格间距
    std::vector<float> columnWidths;    // 列宽（百分比或固定值，空 = 自动）
    std::vector<float> rowHeights;      // 行高（百分比或固定值，空 = 自动）
    
    // Grid 项目属性（每个子节点可以设置）
    int gridColumnStart = -1;           // 起始列（-1 = 自动）
    int gridColumnEnd = -1;             // 结束列（-1 = 自动，实际列数 = end - start）
    int gridRowStart = -1;              // 起始行（-1 = 自动）
    int gridRowEnd = -1;                // 结束行（-1 = 自动，实际行数 = end - start）
};

struct UILayoutProperties {
    UILayoutType type = UILayoutType::Container;
    UILayoutMode mode = UILayoutMode::Flex;  // 布局模式
    UILayoutDirection direction = UILayoutDirection::Vertical;
    Vector2 padding = Vector2::Zero();
    float spacing = 0.0f;
    Vector2 minSize = Vector2::Zero();
    Vector2 preferredSize = Vector2::Zero();
    float flexGrow = 0.0f;
    float flexShrink = 1.0f;
    bool autoSize = true;
    
    // Flex 布局属性
    UIFlexJustifyContent justifyContent = UIFlexJustifyContent::FlexStart;
    UIFlexAlignItems alignItems = UIFlexAlignItems::FlexStart;
    UIFlexAlignSelf alignSelf = UIFlexAlignSelf::Auto;
    
    // Grid 布局属性
    UIGridProperties grid;
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


