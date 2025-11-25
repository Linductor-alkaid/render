#include "render/ui/ui_menu_renderer.h"
#include "render/ui/widgets/ui_menu.h"
#include "render/ui/widgets/ui_menu_item.h"
#include "render/ui/widgets/ui_pulldown_menu.h"
#include "render/ui/ui_theme.h"

namespace Render::UI {

void UIMenuRenderer::RenderMenuItem(
    const UIMenuItem& menuItem,
    UIRenderCommandBuffer& commands,
    const UITheme& theme,
    int layerID
) {
    const Rect rect = menuItem.GetLayoutRect();
    
    // 分隔符特殊处理
    if (menuItem.IsSeparator()) {
        RenderMenuSeparator(rect, theme.GetMenuSeparatorColor(), commands, layerID);
        return;
    }
    
    // 确定菜单项状态和颜色
    Color bgColor;
    Color textColor;
    
    if (!menuItem.IsEnabled()) {
        bgColor = theme.GetMenuItemDisabledBackgroundColor();
        textColor = theme.GetMenuItemDisabledTextColor();
    } else if (menuItem.IsPressed()) {
        bgColor = theme.GetMenuItemPressedBackgroundColor();
        textColor = theme.GetMenuItemPressedTextColor();
    } else if (menuItem.IsHovered()) {
        bgColor = theme.GetMenuItemHoverBackgroundColor();
        textColor = theme.GetMenuItemHoverTextColor();
    } else {
        bgColor = theme.GetMenuItemNormalBackgroundColor();
        textColor = theme.GetMenuItemNormalTextColor();
    }
    
    // 渲染背景
    RenderMenuItemBackground(rect, bgColor, commands, layerID);
    
    // 计算布局
    const float iconSize = 16.0f;
    const float padding = 8.0f;
    const float checkMarkWidth = 20.0f;
    float currentX = rect.x + padding;
    
    // 渲染勾选标记区域
    if (menuItem.IsCheckable()) {
        if (menuItem.IsChecked()) {
            Rect checkRect{currentX, rect.y + (rect.height - iconSize) * 0.5f, iconSize, iconSize};
            RenderMenuItemCheckMark(checkRect, textColor, commands, layerID);
        }
    }
    currentX += checkMarkWidth;
    
    // 渲染图标
    if (!menuItem.GetIcon().empty()) {
        Rect iconRect{currentX, rect.y + (rect.height - iconSize) * 0.5f, iconSize, iconSize};
        RenderMenuItemIcon(menuItem.GetIcon(), iconRect, textColor, commands, layerID);
        currentX += iconSize + padding;
    }
    
    // 渲染文本
    const float fontSize = theme.GetMenuItemFontSize();
    Vector2 textPos{currentX, rect.y + rect.height * 0.5f};
    RenderMenuItemText(menuItem.GetLabel(), textPos, textColor, fontSize, commands, layerID);
    
    // 渲染快捷键
    if (!menuItem.GetShortcut().empty()) {
        Vector2 shortcutPos{rect.x + rect.width - padding * 2.0f, rect.y + rect.height * 0.5f};
        RenderMenuItemShortcut(menuItem.GetShortcut(), shortcutPos, textColor, fontSize * 0.9f, commands, layerID);
    }
    
    // 渲染子菜单箭头
    if (menuItem.HasSubMenu()) {
        Rect arrowRect{rect.x + rect.width - padding - iconSize, rect.y + (rect.height - iconSize) * 0.5f, iconSize, iconSize};
        RenderMenuItemArrow(arrowRect, textColor, commands, layerID);
    }
}

void UIMenuRenderer::RenderMenuBackground(
    const UIMenu& menu,
    UIRenderCommandBuffer& commands,
    const UITheme& theme,
    int layerID
) {
    const Rect rect = menu.GetLayoutRect();
    
    // 渲染菜单背景（使用命令结构体）
    UIRoundedRectangleCommand cmd;
    cmd.rect = rect;
    cmd.cornerRadius = theme.GetMenuCornerRadius();
    cmd.fillColor = theme.GetMenuBackgroundColor();
    cmd.strokeColor = theme.GetMenuBorderColor();
    cmd.strokeWidth = theme.GetMenuBorderWidth();
    cmd.filled = true;
    cmd.stroked = true;
    cmd.layerID = layerID;
    cmd.depth = 0.0f;
    commands.AddRoundedRectangle(cmd);
    
    // 如果需要滚动，绘制滚动指示器
    if (menu.NeedsScroll()) {
        // TODO: 实现滚动指示器
    }
}

void UIMenuRenderer::RenderPullDownMenu(
    const UIPullDownMenu& /*pulldown*/,
    UIRenderCommandBuffer& /*commands*/,
    const UITheme& /*theme*/,
    int /*layerID*/
) {
    // 下拉菜单的渲染由其子组件（按钮和菜单）处理
    // 这里可以添加额外的装饰，如连接线等
}

void UIMenuRenderer::RenderMenuItemBackground(
    const Rect& rect,
    const Color& color,
    UIRenderCommandBuffer& commands,
    int layerID
) {
    UIRectangleCommand cmd;
    cmd.rect = rect;
    cmd.fillColor = color;
    cmd.strokeColor = Color::Clear();
    cmd.strokeWidth = 0.0f;
    cmd.filled = true;
    cmd.stroked = false;
    cmd.layerID = layerID;
    cmd.depth = 0.0f;
    commands.AddRectangle(cmd);
}

void UIMenuRenderer::RenderMenuItemText(
    const std::string& text,
    const Vector2& position,
    const Color& color,
    float fontSize,
    UIRenderCommandBuffer& commands,
    int layerID
) {
    UITextCommand cmd;
    cmd.text = text;
    cmd.transform = std::make_shared<Transform>();
    cmd.transform->SetPosition(Vector3(position.x(), position.y(), 0.0f));
    cmd.font = nullptr;  // 使用默认字体
    cmd.fontSize = fontSize;
    cmd.color = color;
    cmd.layerID = static_cast<uint32_t>(layerID);
    cmd.depth = 0.0f;
    commands.AddText(cmd);
}

void UIMenuRenderer::RenderMenuItemIcon(
    const std::string& iconPath,
    const Rect& rect,
    const Color& color,
    UIRenderCommandBuffer& commands,
    int layerID
) {
    // TODO: 实现图标渲染
    // 当前简单实现：使用纹理或预定义的图形
    (void)iconPath;  // 暂时未使用
    UIRectangleCommand cmd;
    cmd.rect = rect;
    cmd.fillColor = Color::Clear();
    cmd.strokeColor = color;
    cmd.strokeWidth = 1.0f;
    cmd.filled = false;
    cmd.stroked = true;
    cmd.layerID = layerID;
    cmd.depth = 0.0f;
    commands.AddRectangle(cmd);
}

void UIMenuRenderer::RenderMenuItemCheckMark(
    const Rect& rect,
    const Color& color,
    UIRenderCommandBuffer& commands,
    int layerID
) {
    // 绘制勾选标记（简单的勾号）
    const float centerX = rect.x + rect.width * 0.5f;
    const float centerY = rect.y + rect.height * 0.5f;
    const float size = rect.width * 0.6f;
    
    // 使用两条线段绘制勾号
    Vector2 p1{centerX - size * 0.3f, centerY};
    Vector2 p2{centerX - size * 0.1f, centerY + size * 0.3f};
    Vector2 p3{centerX + size * 0.4f, centerY - size * 0.4f};
    
    UILineCommand cmd1;
    cmd1.start = p1;
    cmd1.end = p2;
    cmd1.width = 2.0f;
    cmd1.color = color;
    cmd1.layerID = layerID;
    cmd1.depth = 0.0f;
    commands.AddLine(cmd1);
    
    UILineCommand cmd2;
    cmd2.start = p2;
    cmd2.end = p3;
    cmd2.width = 2.0f;
    cmd2.color = color;
    cmd2.layerID = layerID;
    cmd2.depth = 0.0f;
    commands.AddLine(cmd2);
}

void UIMenuRenderer::RenderMenuItemShortcut(
    const std::string& shortcut,
    const Vector2& position,
    const Color& color,
    float fontSize,
    UIRenderCommandBuffer& commands,
    int layerID
) {
    UITextCommand cmd;
    cmd.text = shortcut;
    cmd.transform = std::make_shared<Transform>();
    cmd.transform->SetPosition(Vector3(position.x(), position.y(), 0.0f));
    cmd.font = nullptr;  // 使用默认字体
    cmd.fontSize = fontSize;
    cmd.color = color;
    cmd.layerID = static_cast<uint32_t>(layerID);
    cmd.depth = 0.0f;
    commands.AddText(cmd);
}

void UIMenuRenderer::RenderMenuItemArrow(
    const Rect& rect,
    const Color& color,
    UIRenderCommandBuffer& commands,
    int layerID
) {
    // 绘制右箭头（指示有子菜单）
    const float centerX = rect.x + rect.width * 0.5f;
    const float centerY = rect.y + rect.height * 0.5f;
    const float size = rect.width * 0.5f;
    
    // 使用多边形绘制三角形箭头
    std::vector<Vector2> vertices = {
        {centerX - size * 0.3f, centerY - size * 0.5f},
        {centerX + size * 0.3f, centerY},
        {centerX - size * 0.3f, centerY + size * 0.5f}
    };
    
    UIPolygonCommand cmd;
    cmd.vertices = vertices;
    cmd.fillColor = color;
    cmd.strokeColor = Color::Clear();
    cmd.strokeWidth = 0.0f;
    cmd.filled = true;
    cmd.stroked = false;
    cmd.layerID = layerID;
    cmd.depth = 0.0f;
    commands.AddPolygon(cmd);
}

void UIMenuRenderer::RenderMenuSeparator(
    const Rect& rect,
    const Color& color,
    UIRenderCommandBuffer& commands,
    int layerID
) {
    // 绘制水平分隔线
    const float padding = 8.0f;
    const float y = rect.y + rect.height * 0.5f;
    
    UILineCommand cmd;
    cmd.start = Vector2{rect.x + padding, y};
    cmd.end = Vector2{rect.x + rect.width - padding, y};
    cmd.width = 1.0f;
    cmd.color = color;
    cmd.layerID = layerID;
    cmd.depth = 0.0f;
    commands.AddLine(cmd);
}

} // namespace Render::UI

