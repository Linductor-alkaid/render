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

// 前向声明
void MeasureFlexLayout(UILayoutNode& node, const Vector2& availableSize);
void ArrangeFlexLayout(UILayoutNode& node, const Vector2& position, const Vector2& size);
void MeasureGridLayout(UILayoutNode& node, const Vector2& availableSize);
void ArrangeGridLayout(UILayoutNode& node, const Vector2& position, const Vector2& size);
void MeasureAbsoluteLayout(UILayoutNode& node, const Vector2& availableSize);
void ArrangeAbsoluteLayout(UILayoutNode& node, const Vector2& position, const Vector2& size);

std::unique_ptr<UILayoutNode> BuildNode(UIWidget& widget) {
    auto node = std::make_unique<UILayoutNode>(&widget);
    
    // 从 Widget 同步布局属性
    UILayoutProperties& props = node->Properties();
    Vector4 padding = widget.GetPadding();
    props.padding = Vector2(padding.x(), padding.y());
    props.minSize = widget.GetMinSize();
    props.preferredSize = widget.GetPreferredSize();
    props.mode = widget.GetLayoutMode();
    props.direction = widget.GetLayoutDirection();
    props.justifyContent = widget.GetJustifyContent();
    props.alignItems = widget.GetAlignItems();
    props.alignSelf = widget.GetAlignSelf();
    props.flexGrow = widget.GetFlexGrow();
    props.flexShrink = widget.GetFlexShrink();
    props.spacing = widget.GetSpacing();
    
    // Grid 布局属性
    props.grid.columns = widget.GetGridColumns();
    props.grid.rows = widget.GetGridRows();
    props.grid.cellSpacing = widget.GetGridCellSpacing();
    props.grid.columnWidths = widget.GetGridColumnWidths();
    props.grid.rowHeights = widget.GetGridRowHeights();
    
    // Grid 项目属性（子节点使用）
    auto [colStart, colEnd] = widget.GetGridColumnSpan();
    auto [rowStart, rowEnd] = widget.GetGridRowSpan();
    props.grid.gridColumnStart = colStart;
    props.grid.gridColumnEnd = colEnd;
    props.grid.gridRowStart = rowStart;
    props.grid.gridRowEnd = rowEnd;
    
    widget.ForEachChild([&](UIWidget& child) {
        auto childNode = BuildNode(child);
        node->AddChild(std::move(childNode));
    });
    return node;
}

// 辅助函数：将 UIFlexAlignSelf 转换为 UIFlexAlignItems
static UIFlexAlignItems ResolveAlignSelf(const UILayoutProperties& childProps, 
                                         const UILayoutProperties& parentProps) {
    if (childProps.alignSelf != UIFlexAlignSelf::Auto) {
        switch (childProps.alignSelf) {
        case UIFlexAlignSelf::FlexStart:
            return UIFlexAlignItems::FlexStart;
        case UIFlexAlignSelf::FlexEnd:
            return UIFlexAlignItems::FlexEnd;
        case UIFlexAlignSelf::Center:
            return UIFlexAlignItems::Center;
        case UIFlexAlignSelf::Stretch:
            return UIFlexAlignItems::Stretch;
        case UIFlexAlignSelf::Baseline:
            return UIFlexAlignItems::Baseline;
        case UIFlexAlignSelf::Auto:
            break; // 不应该到达这里
        }
    }
    return parentProps.alignItems;
}

// Flex 布局测量阶段（参考 Blender 的 estimate_impl）
// 只计算"理想"尺寸，不处理 flexGrow/flexShrink，不修改子节点尺寸
void MeasureFlexLayout(UILayoutNode& node, const Vector2& availableSize) {
    const UILayoutProperties& props = node.Properties();
    UIWidget* widget = node.GetWidget();
    
    if (!widget) {
        node.Metrics().measuredSize = Vector2::Zero();
        return;
    }
    
    Vector4 padding = widget->GetPadding();
    float paddingX = padding.x() + padding.z();
    float paddingY = padding.y() + padding.w();
    
    Vector2 contentSize = availableSize;
    contentSize.x() = std::max(0.0f, contentSize.x() - paddingX);
    contentSize.y() = std::max(0.0f, contentSize.y() - paddingY);
    
    bool isHorizontal = props.direction == UILayoutDirection::Horizontal;
    float mainAxisSize = isHorizontal ? contentSize.x() : contentSize.y();
    float crossAxisSize = isHorizontal ? contentSize.y() : contentSize.x();
    
    float totalMainSize = 0.0f;
    float maxCrossSize = 0.0f;
    int visibleChildCount = 0;
    
    // 测量所有子节点（参考 Blender：只计算尺寸，不处理空间分配）
    node.ForEachChild([&](UILayoutNode& childNode) {
        UIWidget* childWidget = childNode.GetWidget();
        if (!childWidget || !childWidget->IsVisible()) {
            return;
        }
        
        const UILayoutProperties& childProps = childNode.Properties();
        
        // 计算子节点可用空间（对于测量阶段，使用无限空间或 preferredSize）
        Vector2 childAvailable = isHorizontal 
            ? Vector2(mainAxisSize > 0.0f ? mainAxisSize : std::numeric_limits<float>::max(), 
                      crossAxisSize > 0.0f ? crossAxisSize : std::numeric_limits<float>::max())
            : Vector2(crossAxisSize > 0.0f ? crossAxisSize : std::numeric_limits<float>::max(),
                      mainAxisSize > 0.0f ? mainAxisSize : std::numeric_limits<float>::max());
        
        // 如果有 preferredSize，使用它作为约束
        if (childProps.preferredSize.x() > 0.0f) {
            childAvailable.x() = std::min(childAvailable.x(), childProps.preferredSize.x());
        }
        if (childProps.preferredSize.y() > 0.0f) {
            childAvailable.y() = std::min(childAvailable.y(), childProps.preferredSize.y());
        }
        
        // 递归测量子节点（根据子节点的布局模式）
        if (childProps.mode == UILayoutMode::Grid) {
            MeasureGridLayout(childNode, childAvailable);
        } else if (childProps.mode == UILayoutMode::Flex) {
            MeasureFlexLayout(childNode, childAvailable);
        } else {
            // 绝对布局：使用 preferredSize 或 minSize
            Vector2 childSize = childProps.preferredSize;
            if (childSize.x() <= 0.0f) {
                childSize.x() = std::max(childProps.minSize.x(), isHorizontal ? 100.0f : 0.0f);
            }
            if (childSize.y() <= 0.0f) {
                childSize.y() = std::max(childProps.minSize.y(), isHorizontal ? 0.0f : 40.0f);
            }
            childNode.Metrics().measuredSize = childSize;
        }
        
        Vector2 childMeasured = childNode.Metrics().measuredSize;
        Vector2 childPreferred = childProps.preferredSize;
        Vector2 childMin = childProps.minSize;
        
        // 计算子节点在主轴上的理想尺寸（基于 measuredSize、preferredSize、minSize）
        float childMainSize = isHorizontal ? childMeasured.x() : childMeasured.y();
        if (childMainSize <= 0.0f) {
            // 使用 preferredSize
            if (isHorizontal && childPreferred.x() > 0.0f) {
                childMainSize = childPreferred.x();
            } else if (!isHorizontal && childPreferred.y() > 0.0f) {
                childMainSize = childPreferred.y();
            }
        }
        if (childMainSize <= 0.0f) {
            // 使用 minSize 或默认值
            float childMinMain = isHorizontal ? childMin.x() : childMin.y();
            childMainSize = childMinMain > 0.0f ? childMinMain : (isHorizontal ? 100.0f : 40.0f);
        }
        
        // 计算子节点在交叉轴上的理想尺寸
        float childCrossSize = isHorizontal ? childMeasured.y() : childMeasured.x();
        if (childCrossSize <= 0.0f) {
            if (isHorizontal && childPreferred.y() > 0.0f) {
                childCrossSize = childPreferred.y();
            } else if (!isHorizontal && childPreferred.x() > 0.0f) {
                childCrossSize = childPreferred.x();
            }
        }
        if (childCrossSize <= 0.0f) {
            float childMinCross = isHorizontal ? childMin.y() : childMin.x();
            childCrossSize = childMinCross > 0.0f ? childMinCross : (isHorizontal ? 40.0f : 100.0f);
        }
        
        // 注意：测量阶段不处理 Stretch，这应该在排列阶段处理
        // 但我们需要记录最大交叉轴尺寸
        
        totalMainSize += childMainSize;
        maxCrossSize = std::max(maxCrossSize, childCrossSize);
        visibleChildCount++;
    });
    
    // 添加间距
    float spacing = props.spacing > 0.0f ? props.spacing : 0.0f;
    if (visibleChildCount > 1 && spacing > 0.0f) {
        totalMainSize += spacing * static_cast<float>(visibleChildCount - 1);
    }
    
    // 计算容器理想尺寸（不考虑 flexGrow/flexShrink）
    float containerMainSize = totalMainSize;
    float containerCrossSize = maxCrossSize;
    
    // 应用 minSize 约束
    if (props.minSize.x() > 0.0f) {
        if (isHorizontal) {
            containerMainSize = std::max(containerMainSize, props.minSize.x());
        } else {
            containerCrossSize = std::max(containerCrossSize, props.minSize.x());
        }
    }
    if (props.minSize.y() > 0.0f) {
        if (isHorizontal) {
            containerCrossSize = std::max(containerCrossSize, props.minSize.y());
        } else {
            containerMainSize = std::max(containerMainSize, props.minSize.y());
        }
    }
    
    // 应用 preferredSize 约束
    if (props.preferredSize.x() > 0.0f) {
        if (isHorizontal) {
            containerMainSize = std::max(containerMainSize, props.preferredSize.x() - paddingX);
        } else {
            containerCrossSize = std::max(containerCrossSize, props.preferredSize.x() - paddingX);
        }
    }
    if (props.preferredSize.y() > 0.0f) {
        if (isHorizontal) {
            containerCrossSize = std::max(containerCrossSize, props.preferredSize.y() - paddingY);
        } else {
            containerMainSize = std::max(containerMainSize, props.preferredSize.y() - paddingY);
        }
    }
    
    // 设置容器测量尺寸（不包含 padding，padding 在排列阶段处理）
    Vector2 measuredSize = isHorizontal 
        ? Vector2(containerMainSize, containerCrossSize)
        : Vector2(containerCrossSize, containerMainSize);
    
    node.Metrics().measuredSize = measuredSize;
}

// Flex 布局排列阶段（参考 Blender 的 resolve_impl）
// 处理 flexGrow/flexShrink，计算最终位置和尺寸
void ArrangeFlexLayout(UILayoutNode& node, const Vector2& position, const Vector2& size) {
    const UILayoutProperties& props = node.Properties();
    UIWidget* widget = node.GetWidget();
    
    if (!widget) {
        return;
    }
    
    Rect rect(position.x(), position.y(), size.x(), size.y());
    widget->SetLayoutRect(rect);
    node.Metrics().position = position;
    node.Metrics().size = size;
    
    Vector4 padding = widget->GetPadding();
    float paddingLeft = padding.x();
    float paddingTop = padding.y();
    float paddingRight = padding.z();
    float paddingBottom = padding.w();
    
    float contentX = position.x() + paddingLeft;
    float contentY = position.y() + paddingTop;
    float contentWidth = std::max(0.0f, size.x() - paddingLeft - paddingRight);
    float contentHeight = std::max(0.0f, size.y() - paddingTop - paddingBottom);
    
    bool isHorizontal = props.direction == UILayoutDirection::Horizontal;
    float mainAxisSize = isHorizontal ? contentWidth : contentHeight;
    float crossAxisSize = isHorizontal ? contentHeight : contentWidth;
    
    // 收集可见子节点及其理想尺寸
    struct ChildInfo {
        UILayoutNode* node;
        float idealMainSize;      // 理想主轴尺寸（来自测量阶段）
        float idealCrossSize;     // 理想交叉轴尺寸
        float finalMainSize;      // 最终主轴尺寸（考虑 flexGrow/flexShrink）
        float finalCrossSize;     // 最终交叉轴尺寸（考虑 Stretch）
        UIFlexAlignItems align;   // 解析后的对齐方式
    };
    
    std::vector<ChildInfo> children;
    float totalIdealMainSize = 0.0f;
    float totalFlexGrow = 0.0f;
    float totalFlexShrink = 0.0f;
    
    node.ForEachChild([&](UILayoutNode& childNode) {
        UIWidget* childWidget = childNode.GetWidget();
        if (!childWidget || !childWidget->IsVisible()) {
            return;
        }
        
        const UILayoutProperties& childProps = childNode.Properties();
        Vector2 childMeasured = childNode.Metrics().measuredSize;
        
        float idealMainSize = isHorizontal ? childMeasured.x() : childMeasured.y();
        float idealCrossSize = isHorizontal ? childMeasured.y() : childMeasured.x();
        
        // 解析对齐方式
        UIFlexAlignItems align = ResolveAlignSelf(childProps, props);
        
        ChildInfo info;
        info.node = &childNode;
        info.idealMainSize = idealMainSize;
        info.idealCrossSize = idealCrossSize;
        info.finalMainSize = idealMainSize;  // 初始值，将在后面调整
        info.finalCrossSize = idealCrossSize; // 初始值，将在后面调整
        info.align = align;
        
        totalIdealMainSize += idealMainSize;
        if (childProps.flexGrow > 0.0f) {
            totalFlexGrow += childProps.flexGrow;
        }
        if (childProps.flexShrink > 0.0f) {
            totalFlexShrink += childProps.flexShrink;
        }
        
        children.push_back(info);
    });
    
    // 添加间距
    float spacing = props.spacing > 0.0f ? props.spacing : 0.0f;
    float totalSpacing = 0.0f;
    if (children.size() > 1 && spacing > 0.0f) {
        totalSpacing = spacing * static_cast<float>(children.size() - 1);
    }
    
    float totalNeededMainSize = totalIdealMainSize + totalSpacing;
    
    // 处理 flexGrow：如果可用空间大于所需空间，分配额外空间
    if (totalNeededMainSize < mainAxisSize && totalFlexGrow > 0.0f) {
        float extraSpace = mainAxisSize - totalNeededMainSize;
        for (auto& info : children) {
            const UILayoutProperties& childProps = info.node->Properties();
            if (childProps.flexGrow > 0.0f) {
                float growAmount = (childProps.flexGrow / totalFlexGrow) * extraSpace;
                info.finalMainSize = info.idealMainSize + growAmount;
            }
        }
        totalNeededMainSize = mainAxisSize;
    }
    
    // 处理 flexShrink：如果所需空间大于可用空间，收缩子节点
    if (totalNeededMainSize > mainAxisSize && totalFlexShrink > 0.0f) {
        float shrinkAmount = totalNeededMainSize - mainAxisSize;
        for (auto& info : children) {
            const UILayoutProperties& childProps = info.node->Properties();
            if (childProps.flexShrink > 0.0f) {
                float shrink = (childProps.flexShrink / totalFlexShrink) * shrinkAmount;
                float minMainSize = isHorizontal 
                    ? childProps.minSize.x() 
                    : childProps.minSize.y();
                info.finalMainSize = std::max(minMainSize, info.idealMainSize - shrink);
            }
        }
        totalNeededMainSize = mainAxisSize;
    }
    
    // 处理交叉轴对齐（Stretch）
    for (auto& info : children) {
        if (info.align == UIFlexAlignItems::Stretch) {
            info.finalCrossSize = crossAxisSize;
        }
    }
    
    // 计算主轴起始位置（根据 justifyContent）
    float mainStart = 0.0f;
    float actualTotalMainSize = 0.0f;
    for (const auto& info : children) {
        actualTotalMainSize += info.finalMainSize;
    }
    actualTotalMainSize += totalSpacing;
    float extraSpace = mainAxisSize - actualTotalMainSize;
    
    float adjustedSpacing = spacing;
    switch (props.justifyContent) {
    case UIFlexJustifyContent::FlexStart:
        mainStart = 0.0f;
        break;
    case UIFlexJustifyContent::FlexEnd:
        mainStart = extraSpace;
        break;
    case UIFlexJustifyContent::Center:
        mainStart = extraSpace * 0.5f;
        break;
    case UIFlexJustifyContent::SpaceBetween:
        if (children.size() > 1) {
            adjustedSpacing = extraSpace / static_cast<float>(children.size() - 1);
            mainStart = 0.0f;
        }
        break;
    case UIFlexJustifyContent::SpaceAround:
        if (!children.empty()) {
            adjustedSpacing = extraSpace / static_cast<float>(children.size() * 2);
            mainStart = adjustedSpacing;
        }
        break;
    case UIFlexJustifyContent::SpaceEvenly:
        if (!children.empty()) {
            adjustedSpacing = extraSpace / static_cast<float>(children.size() + 1);
            mainStart = adjustedSpacing;
        }
        break;
    }
    
    // 排列子节点
    float currentMain = mainStart;
    for (size_t i = 0; i < children.size(); ++i) {
        const auto& info = children[i];
        
        // 计算交叉轴位置
        float crossStart = 0.0f;
        float crossExtra = crossAxisSize - info.finalCrossSize;
        
        switch (info.align) {
        case UIFlexAlignItems::FlexStart:
            crossStart = 0.0f;
            break;
        case UIFlexAlignItems::FlexEnd:
            crossStart = crossExtra;
            break;
        case UIFlexAlignItems::Center:
            crossStart = crossExtra * 0.5f;
            break;
        case UIFlexAlignItems::Stretch:
            crossStart = 0.0f;
            // finalCrossSize 已经在上面设置为 crossAxisSize
            break;
        case UIFlexAlignItems::Baseline:
            crossStart = 0.0f; // 暂不支持基线对齐
            break;
        }
        
        // 计算子节点位置和尺寸
        Vector2 childPosition;
        Vector2 childSize;
        if (isHorizontal) {
            childPosition = Vector2(contentX + currentMain, contentY + crossStart);
            childSize = Vector2(info.finalMainSize, info.finalCrossSize);
        } else {
            childPosition = Vector2(contentX + crossStart, contentY + currentMain);
            childSize = Vector2(info.finalCrossSize, info.finalMainSize);
        }
        
        // 递归排列子节点（根据子节点的布局模式）
        const UILayoutProperties& childProps = info.node->Properties();
        if (childProps.mode == UILayoutMode::Grid) {
            ArrangeGridLayout(*info.node, childPosition, childSize);
        } else if (childProps.mode == UILayoutMode::Flex) {
            ArrangeFlexLayout(*info.node, childPosition, childSize);
        } else {
            // 绝对布局：递归排列
            ArrangeAbsoluteLayout(*info.node, childPosition, childSize);
        }
        
        currentMain += info.finalMainSize;
        if (i + 1 < children.size()) {
            currentMain += adjustedSpacing;
        }
    }
}

// 绝对布局测量阶段（参考 Blender 的 LayoutAbsolute::estimate_impl）
// 对于简单的栈式布局，计算所有子节点的理想尺寸总和
void MeasureAbsoluteLayout(UILayoutNode& node, const Vector2& availableSize) {
    const UILayoutProperties& props = node.Properties();
    UIWidget* widget = node.GetWidget();
    
    if (!widget) {
        node.Metrics().measuredSize = Vector2::Zero();
        return;
    }
    
    Vector4 padding = widget->GetPadding();
    float paddingX = padding.x() + padding.z();
    float paddingY = padding.y() + padding.w();
    
    Vector2 contentSize = availableSize;
    contentSize.x() = std::max(0.0f, contentSize.x() - paddingX);
    contentSize.y() = std::max(0.0f, contentSize.y() - paddingY);
    
    // 对于绝对布局，我们使用简单的垂直栈式布局（向后兼容）
    // 测量所有子节点
    float maxWidth = 0.0f;
    float totalHeight = 0.0f;
    int visibleChildCount = 0;
    constexpr float kDefaultSpacing = 16.0f;
    
    node.ForEachChild([&](UILayoutNode& childNode) {
        UIWidget* childWidget = childNode.GetWidget();
        if (!childWidget || !childWidget->IsVisible()) {
            return;
        }
        
        const UILayoutProperties& childProps = childNode.Properties();
        
        // 计算子节点可用空间
        Vector2 childAvailable = contentSize;
        
        // 如果有 preferredSize，使用它作为约束
        if (childProps.preferredSize.x() > 0.0f) {
            childAvailable.x() = std::min(childAvailable.x(), childProps.preferredSize.x());
        }
        if (childProps.preferredSize.y() > 0.0f) {
            childAvailable.y() = std::min(childAvailable.y(), childProps.preferredSize.y());
        }
        
        // 递归测量子节点（根据子节点的布局模式）
        if (childProps.mode == UILayoutMode::Grid) {
            MeasureGridLayout(childNode, childAvailable);
        } else if (childProps.mode == UILayoutMode::Flex) {
            MeasureFlexLayout(childNode, childAvailable);
        } else {
            // 绝对布局：使用 preferredSize 或 minSize
            Vector2 childSize = childProps.preferredSize;
            if (childSize.x() <= 0.0f) {
                childSize.x() = std::max(childProps.minSize.x(), contentSize.x() > 0.0f ? contentSize.x() : 100.0f);
            }
            if (childSize.y() <= 0.0f) {
                childSize.y() = std::max(childProps.minSize.y(), 40.0f);
            }
            childNode.Metrics().measuredSize = childSize;
        }
        
        Vector2 childMeasured = childNode.Metrics().measuredSize;
        Vector2 childPreferred = childProps.preferredSize;
        Vector2 childMin = childProps.minSize;
        
        // 计算子节点理想尺寸
        float childWidth = childMeasured.x();
        if (childWidth <= 0.0f) {
            childWidth = childPreferred.x() > 0.0f ? childPreferred.x() : (childMin.x() > 0.0f ? childMin.x() : contentSize.x() > 0.0f ? contentSize.x() : 100.0f);
        }
        
        float childHeight = childMeasured.y();
        if (childHeight <= 0.0f) {
            childHeight = childPreferred.y() > 0.0f ? childPreferred.y() : (childMin.y() > 0.0f ? childMin.y() : 40.0f);
        }
        
        maxWidth = std::max(maxWidth, childWidth);
        totalHeight += childHeight;
        visibleChildCount++;
    });
    
    // 添加间距
    if (visibleChildCount > 1) {
        totalHeight += kDefaultSpacing * static_cast<float>(visibleChildCount - 1);
    }
    
    // 计算容器理想尺寸
    float containerWidth = maxWidth;
    float containerHeight = totalHeight;
    
    // 应用 minSize 约束
    if (props.minSize.x() > 0.0f) {
        containerWidth = std::max(containerWidth, props.minSize.x());
    }
    if (props.minSize.y() > 0.0f) {
        containerHeight = std::max(containerHeight, props.minSize.y());
    }
    
    // 应用 preferredSize 约束
    if (props.preferredSize.x() > 0.0f) {
        containerWidth = std::max(containerWidth, props.preferredSize.x() - paddingX);
    }
    if (props.preferredSize.y() > 0.0f) {
        containerHeight = std::max(containerHeight, props.preferredSize.y() - paddingY);
    }
    
    // 设置容器测量尺寸
    Vector2 measuredSize(containerWidth, containerHeight);
    node.Metrics().measuredSize = measuredSize;
}

// 绝对布局排列阶段（参考 Blender 的 LayoutAbsolute::resolve_impl）
// 对于简单的栈式布局，垂直排列子节点，水平居中
void ArrangeAbsoluteLayout(UILayoutNode& node, const Vector2& position, const Vector2& size) {
    const UILayoutProperties& props = node.Properties();
    UIWidget* widget = node.GetWidget();
    
    if (!widget) {
        return;
    }
    
    Rect rect(position.x(), position.y(), size.x(), size.y());
    widget->SetLayoutRect(rect);
    node.Metrics().position = position;
    node.Metrics().size = size;
    
    Vector4 padding = widget->GetPadding();
    float paddingLeft = padding.x();
    float paddingTop = padding.y();
    float paddingRight = padding.z();
    float paddingBottom = padding.w();
    
    float contentX = position.x() + paddingLeft;
    float contentY = position.y() + paddingTop;
    float contentWidth = std::max(0.0f, size.x() - paddingLeft - paddingRight);
    float contentHeight = std::max(0.0f, size.y() - paddingTop - paddingBottom);
    
    // 收集可见子节点及其理想尺寸
    struct ChildInfo {
        UILayoutNode* node;
        Vector2 idealSize;
        Vector2 finalSize;
    };
    
    std::vector<ChildInfo> children;
    float totalIdealHeight = 0.0f;
    constexpr float kDefaultSpacing = 16.0f;
    
    node.ForEachChild([&](UILayoutNode& childNode) {
        UIWidget* childWidget = childNode.GetWidget();
        if (!childWidget || !childWidget->IsVisible()) {
            return;
        }
        
        Vector2 childMeasured = childNode.Metrics().measuredSize;
        const UILayoutProperties& childProps = childNode.Properties();
        Vector2 childPreferred = childProps.preferredSize;
        Vector2 childMin = childProps.minSize;
        
        // 计算子节点理想尺寸
        float childWidth = childMeasured.x();
        if (childWidth <= 0.0f) {
            childWidth = childPreferred.x() > 0.0f ? childPreferred.x() : (childMin.x() > 0.0f ? childMin.x() : contentWidth > 0.0f ? contentWidth : 100.0f);
        }
        
        float childHeight = childMeasured.y();
        if (childHeight <= 0.0f) {
            childHeight = childPreferred.y() > 0.0f ? childPreferred.y() : (childMin.y() > 0.0f ? childMin.y() : 40.0f);
        }
        
        ChildInfo info;
        info.node = &childNode;
        info.idealSize = Vector2(childWidth, childHeight);
        info.finalSize = info.idealSize; // 绝对布局不调整尺寸
        
        totalIdealHeight += childHeight;
        children.push_back(info);
    });
    
    // 添加间距
    float totalSpacing = 0.0f;
    if (children.size() > 1) {
        totalSpacing = kDefaultSpacing * static_cast<float>(children.size() - 1);
    }
    
    float totalNeededHeight = totalIdealHeight + totalSpacing;
    
    // 计算起始 Y 位置（如果是根节点，居中显示）
    bool parentIsRoot = widget->GetParent() == nullptr;
    float startY = contentY;
    if (parentIsRoot && totalNeededHeight < contentHeight) {
        float centeredOffset = (contentHeight - totalNeededHeight) * 0.5f;
        if (centeredOffset > 0.0f) {
            startY += centeredOffset;
        }
    }
    
    // 排列子节点
    float currentY = startY;
    for (size_t i = 0; i < children.size(); ++i) {
        const auto& info = children[i];
        
        // 计算子节点位置（水平居中）
        float childX = contentX;
        if (info.finalSize.x() < contentWidth) {
            childX = contentX + (contentWidth - info.finalSize.x()) * 0.5f;
        }
        
        // 确保不超出内容区域
        float maxY = contentY + contentHeight - info.finalSize.y();
        float childY = std::min(currentY, maxY);
        
        Vector2 childPosition(childX, childY);
        
        // 递归排列子节点（根据子节点的布局模式）
        const UILayoutProperties& childProps = info.node->Properties();
        if (childProps.mode == UILayoutMode::Grid) {
            ArrangeGridLayout(*info.node, childPosition, info.finalSize);
        } else if (childProps.mode == UILayoutMode::Flex) {
            ArrangeFlexLayout(*info.node, childPosition, info.finalSize);
        } else {
            // 绝对布局：递归调用
            ArrangeAbsoluteLayout(*info.node, childPosition, info.finalSize);
        }
        
        currentY = childY + info.finalSize.y();
        if (i + 1 < children.size()) {
            currentY += kDefaultSpacing;
        }
    }
}

// Grid 布局测量阶段
void MeasureGridLayout(UILayoutNode& node, const Vector2& availableSize) {
    const UILayoutProperties& props = node.Properties();
    UIWidget* widget = node.GetWidget();
    
    if (!widget) {
        node.Metrics().measuredSize = Vector2::Zero();
        return;
    }
    
    Vector4 padding = widget->GetPadding();
    float paddingX = padding.x() + padding.z();
    float paddingY = padding.y() + padding.w();
    
    Vector2 contentSize = availableSize;
    contentSize.x() = std::max(0.0f, contentSize.x() - paddingX);
    contentSize.y() = std::max(0.0f, contentSize.y() - paddingY);
    
    const UIGridProperties& grid = props.grid;
    int columns = grid.columns;
    
    // 收集所有可见子节点
    std::vector<UILayoutNode*> children;
    node.ForEachChild([&](UILayoutNode& childNode) {
        UIWidget* childWidget = childNode.GetWidget();
        if (!childWidget || !childWidget->IsVisible()) {
            return;
        }
        children.push_back(&childNode);
    });
    
    if (children.empty()) {
        node.Metrics().measuredSize = Vector2(paddingX, paddingY);
        return;
    }
    
    // 计算行数（如果未指定）
    int rows = grid.rows;
    if (rows <= 0) {
        rows = (static_cast<int>(children.size()) + columns - 1) / columns;
    }
    
    // 计算列宽
    std::vector<float> columnWidths(columns, 0.0f);
    float totalColumnWidth = 0.0f;
    
    if (!grid.columnWidths.empty()) {
        // 使用指定的列宽
        for (int i = 0; i < columns && i < static_cast<int>(grid.columnWidths.size()); ++i) {
            float width = grid.columnWidths[i];
            if (width > 0.0f && width < 1.0f) {
                // 百分比
                columnWidths[i] = contentSize.x() * width;
            } else {
                // 固定值
                columnWidths[i] = width;
            }
            totalColumnWidth += columnWidths[i];
        }
    }
    
    // 如果列宽未完全指定，平均分配剩余空间
    if (totalColumnWidth < contentSize.x()) {
        float remainingWidth = contentSize.x() - totalColumnWidth;
        int unspecifiedColumns = 0;
        for (int i = 0; i < columns; ++i) {
            if (i >= static_cast<int>(grid.columnWidths.size()) || grid.columnWidths[i] <= 0.0f) {
                unspecifiedColumns++;
            }
        }
        if (unspecifiedColumns > 0) {
            float avgWidth = remainingWidth / static_cast<float>(unspecifiedColumns);
            for (int i = 0; i < columns; ++i) {
                if (i >= static_cast<int>(grid.columnWidths.size()) || grid.columnWidths[i] <= 0.0f) {
                    columnWidths[i] = avgWidth;
                }
            }
        }
    }
    
    // 计算行高
    std::vector<float> rowHeights(rows, 0.0f);
    float totalRowHeight = 0.0f;
    
    if (!grid.rowHeights.empty()) {
        // 使用指定的行高
        for (int i = 0; i < rows && i < static_cast<int>(grid.rowHeights.size()); ++i) {
            float height = grid.rowHeights[i];
            if (height > 0.0f && height < 1.0f) {
                // 百分比
                rowHeights[i] = contentSize.y() * height;
            } else {
                // 固定值
                rowHeights[i] = height;
            }
            totalRowHeight += rowHeights[i];
        }
    }
    
    // 如果行高未完全指定，平均分配剩余空间
    if (totalRowHeight < contentSize.y()) {
        float remainingHeight = contentSize.y() - totalRowHeight;
        int unspecifiedRows = 0;
        for (int i = 0; i < rows; ++i) {
            if (i >= static_cast<int>(grid.rowHeights.size()) || grid.rowHeights[i] <= 0.0f) {
                unspecifiedRows++;
            }
        }
        if (unspecifiedRows > 0) {
            float avgHeight = remainingHeight / static_cast<float>(unspecifiedRows);
            for (int i = 0; i < rows; ++i) {
                if (i >= static_cast<int>(grid.rowHeights.size()) || grid.rowHeights[i] <= 0.0f) {
                    rowHeights[i] = avgHeight;
                }
            }
        }
    }
    
    // 测量每个子节点
    for (size_t i = 0; i < children.size(); ++i) {
        UILayoutNode* childNode = children[i];
        const UIGridProperties& childGrid = childNode->Properties().grid;
        
        // 计算子节点占用的列和行
        int colStart = childGrid.gridColumnStart >= 0 ? childGrid.gridColumnStart : (static_cast<int>(i) % columns);
        int colEnd = childGrid.gridColumnEnd >= 0 ? childGrid.gridColumnEnd : (colStart + 1);
        int rowStart = childGrid.gridRowStart >= 0 ? childGrid.gridRowStart : (static_cast<int>(i) / columns);
        int rowEnd = childGrid.gridRowEnd >= 0 ? childGrid.gridRowEnd : (rowStart + 1);
        
        // 计算子节点可用空间
        float cellWidth = 0.0f;
        float cellHeight = 0.0f;
        
        for (int col = colStart; col < colEnd && col < columns; ++col) {
            cellWidth += columnWidths[col];
            if (col < colEnd - 1) {
                cellWidth += grid.cellSpacing.x();
            }
        }
        
        for (int row = rowStart; row < rowEnd && row < rows; ++row) {
            cellHeight += rowHeights[row];
            if (row < rowEnd - 1) {
                cellHeight += grid.cellSpacing.y();
            }
        }
        
        // 递归测量子节点（根据子节点的布局模式）
        const UILayoutProperties& childProps = childNode->Properties();
        if (childProps.mode == UILayoutMode::Grid) {
            MeasureGridLayout(*childNode, Vector2(cellWidth, cellHeight));
        } else if (childProps.mode == UILayoutMode::Flex) {
            MeasureFlexLayout(*childNode, Vector2(cellWidth, cellHeight));
        } else {
            // 绝对布局：递归测量
            MeasureAbsoluteLayout(*childNode, Vector2(cellWidth, cellHeight));
        }
    }
    
    // 计算容器总尺寸
    float totalWidth = 0.0f;
    for (int i = 0; i < columns; ++i) {
        totalWidth += columnWidths[i];
    }
    totalWidth += grid.cellSpacing.x() * static_cast<float>(std::max(0, columns - 1));
    
    float totalHeight = 0.0f;
    for (int i = 0; i < rows; ++i) {
        totalHeight += rowHeights[i];
    }
    totalHeight += grid.cellSpacing.y() * static_cast<float>(std::max(0, rows - 1));
    
    Vector2 measuredSize(totalWidth + paddingX, totalHeight + paddingY);
    if (props.preferredSize.x() > 0.0f) {
        measuredSize.x() = std::max(measuredSize.x(), props.preferredSize.x());
    }
    if (props.preferredSize.y() > 0.0f) {
        measuredSize.y() = std::max(measuredSize.y(), props.preferredSize.y());
    }
    
    node.Metrics().measuredSize = measuredSize;
}

// Grid 布局排列阶段
void ArrangeGridLayout(UILayoutNode& node, const Vector2& position, const Vector2& size) {
    const UILayoutProperties& props = node.Properties();
    UIWidget* widget = node.GetWidget();
    
    if (!widget) {
        return;
    }
    
    Rect rect(position.x(), position.y(), size.x(), size.y());
    widget->SetLayoutRect(rect);
    node.Metrics().position = position;
    node.Metrics().size = size;
    
    Vector4 padding = widget->GetPadding();
    float paddingLeft = padding.x();
    float paddingTop = padding.y();
    float paddingRight = padding.z();
    float paddingBottom = padding.w();
    
    float contentX = position.x() + paddingLeft;
    float contentY = position.y() + paddingTop;
    float contentWidth = std::max(0.0f, size.x() - paddingLeft - paddingRight);
    float contentHeight = std::max(0.0f, size.y() - paddingTop - paddingBottom);
    
    const UIGridProperties& grid = props.grid;
    int columns = grid.columns;
    
    // 收集所有可见子节点
    std::vector<UILayoutNode*> children;
    node.ForEachChild([&](UILayoutNode& childNode) {
        UIWidget* childWidget = childNode.GetWidget();
        if (!childWidget || !childWidget->IsVisible()) {
            return;
        }
        children.push_back(&childNode);
    });
    
    if (children.empty()) {
        return;
    }
    
    // 计算行数
    int rows = grid.rows;
    if (rows <= 0) {
        rows = (static_cast<int>(children.size()) + columns - 1) / columns;
    }
    
    // 计算列宽（与测量阶段相同）
    std::vector<float> columnWidths(columns, 0.0f);
    float totalColumnWidth = 0.0f;
    
    if (!grid.columnWidths.empty()) {
        for (int i = 0; i < columns && i < static_cast<int>(grid.columnWidths.size()); ++i) {
            float width = grid.columnWidths[i];
            if (width > 0.0f && width < 1.0f) {
                columnWidths[i] = contentWidth * width;
            } else {
                columnWidths[i] = width;
            }
            totalColumnWidth += columnWidths[i];
        }
    }
    
    if (totalColumnWidth < contentWidth) {
        float remainingWidth = contentWidth - totalColumnWidth;
        int unspecifiedColumns = 0;
        for (int i = 0; i < columns; ++i) {
            if (i >= static_cast<int>(grid.columnWidths.size()) || grid.columnWidths[i] <= 0.0f) {
                unspecifiedColumns++;
            }
        }
        if (unspecifiedColumns > 0) {
            float avgWidth = remainingWidth / static_cast<float>(unspecifiedColumns);
            for (int i = 0; i < columns; ++i) {
                if (i >= static_cast<int>(grid.columnWidths.size()) || grid.columnWidths[i] <= 0.0f) {
                    columnWidths[i] = avgWidth;
                }
            }
        }
    }
    
    // 计算行高（与测量阶段相同）
    std::vector<float> rowHeights(rows, 0.0f);
    float totalRowHeight = 0.0f;
    
    if (!grid.rowHeights.empty()) {
        for (int i = 0; i < rows && i < static_cast<int>(grid.rowHeights.size()); ++i) {
            float height = grid.rowHeights[i];
            if (height > 0.0f && height < 1.0f) {
                rowHeights[i] = contentHeight * height;
            } else {
                rowHeights[i] = height;
            }
            totalRowHeight += rowHeights[i];
        }
    }
    
    if (totalRowHeight < contentHeight) {
        float remainingHeight = contentHeight - totalRowHeight;
        int unspecifiedRows = 0;
        for (int i = 0; i < rows; ++i) {
            if (i >= static_cast<int>(grid.rowHeights.size()) || grid.rowHeights[i] <= 0.0f) {
                unspecifiedRows++;
            }
        }
        if (unspecifiedRows > 0) {
            float avgHeight = remainingHeight / static_cast<float>(unspecifiedRows);
            for (int i = 0; i < rows; ++i) {
                if (i >= static_cast<int>(grid.rowHeights.size()) || grid.rowHeights[i] <= 0.0f) {
                    rowHeights[i] = avgHeight;
                }
            }
        }
    }
    
    // 计算列和行的起始位置
    std::vector<float> columnPositions(columns + 1, 0.0f);
    for (int i = 0; i < columns; ++i) {
        columnPositions[i + 1] = columnPositions[i] + columnWidths[i];
        if (i < columns - 1) {
            columnPositions[i + 1] += grid.cellSpacing.x();
        }
    }
    
    std::vector<float> rowPositions(rows + 1, 0.0f);
    for (int i = 0; i < rows; ++i) {
        rowPositions[i + 1] = rowPositions[i] + rowHeights[i];
        if (i < rows - 1) {
            rowPositions[i + 1] += grid.cellSpacing.y();
        }
    }
    
    // 排列每个子节点
    for (size_t i = 0; i < children.size(); ++i) {
        UILayoutNode* childNode = children[i];
        const UIGridProperties& childGrid = childNode->Properties().grid;
        
        // 计算子节点占用的列和行
        int colStart = childGrid.gridColumnStart >= 0 ? childGrid.gridColumnStart : (static_cast<int>(i) % columns);
        int colEnd = childGrid.gridColumnEnd >= 0 ? childGrid.gridColumnEnd : (colStart + 1);
        int rowStart = childGrid.gridRowStart >= 0 ? childGrid.gridRowStart : (static_cast<int>(i) / columns);
        int rowEnd = childGrid.gridRowEnd >= 0 ? childGrid.gridRowEnd : (rowStart + 1);
        
        // 确保索引在有效范围内
        colStart = std::max(0, std::min(colStart, columns - 1));
        colEnd = std::max(colStart + 1, std::min(colEnd, columns));
        rowStart = std::max(0, std::min(rowStart, rows - 1));
        rowEnd = std::max(rowStart + 1, std::min(rowEnd, rows));
        
        // 计算子节点位置和尺寸
        float childX = contentX + columnPositions[colStart];
        float childY = contentY + rowPositions[rowStart];
        float childWidth = columnPositions[colEnd] - columnPositions[colStart];
        float childHeight = rowPositions[rowEnd] - rowPositions[rowStart];
        
        // 减去间距
        if (colEnd > colStart + 1) {
            childWidth -= grid.cellSpacing.x() * static_cast<float>(colEnd - colStart - 1);
        }
        if (rowEnd > rowStart + 1) {
            childHeight -= grid.cellSpacing.y() * static_cast<float>(rowEnd - rowStart - 1);
        }
        
        Vector2 childPosition(childX, childY);
        Vector2 childSize(childWidth, childHeight);
        
        // 递归排列子节点（根据子节点的布局模式）
        const UILayoutProperties& childProps = childNode->Properties();
        if (childProps.mode == UILayoutMode::Grid) {
            ArrangeGridLayout(*childNode, childPosition, childSize);
        } else if (childProps.mode == UILayoutMode::Flex) {
            ArrangeFlexLayout(*childNode, childPosition, childSize);
        } else {
            // 绝对布局：递归排列
            ArrangeAbsoluteLayout(*childNode, childPosition, childSize);
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
    
    // 根据布局模式选择不同的布局算法
    const UILayoutProperties& rootProps = context.root->Properties();
    
    if (rootProps.mode == UILayoutMode::Grid) {
        // Grid 布局：测量阶段
        MeasureGridLayout(*context.root, canvasSize);
        
        // Grid 布局：排列阶段
        Vector2 rootSize = context.root->Metrics().measuredSize;
        if (rootWidget->GetPreferredSize().x() > 0.0f) {
            rootSize.x() = std::max(rootSize.x(), rootWidget->GetPreferredSize().x());
        }
        if (rootWidget->GetPreferredSize().y() > 0.0f) {
            rootSize.y() = std::max(rootSize.y(), rootWidget->GetPreferredSize().y());
        }
        rootSize = canvasSize;
        
        ArrangeGridLayout(*context.root, Vector2::Zero(), rootSize);
    } else if (rootProps.mode == UILayoutMode::Flex) {
        // Flex 布局：测量阶段
        MeasureFlexLayout(*context.root, canvasSize);
        
        // Flex 布局：排列阶段
        Vector2 rootSize = context.root->Metrics().measuredSize;
        if (rootWidget->GetPreferredSize().x() > 0.0f) {
            rootSize.x() = std::max(rootSize.x(), rootWidget->GetPreferredSize().x());
        }
        if (rootWidget->GetPreferredSize().y() > 0.0f) {
            rootSize.y() = std::max(rootSize.y(), rootWidget->GetPreferredSize().y());
        }
        rootSize = canvasSize;
        
        ArrangeFlexLayout(*context.root, Vector2::Zero(), rootSize);
    } else {
        // 绝对布局：测量阶段
        MeasureAbsoluteLayout(*context.root, canvasSize);
        
        // 绝对布局：排列阶段
        Vector2 rootSize = context.root->Metrics().measuredSize;
        if (rootWidget->GetPreferredSize().x() > 0.0f) {
            rootSize.x() = std::max(rootSize.x(), rootWidget->GetPreferredSize().x());
        }
        if (rootWidget->GetPreferredSize().y() > 0.0f) {
            rootSize.y() = std::max(rootSize.y(), rootWidget->GetPreferredSize().y());
        }
        rootSize = canvasSize;
        
        ArrangeAbsoluteLayout(*context.root, Vector2::Zero(), rootSize);
    }
}

} // namespace Render::UI


