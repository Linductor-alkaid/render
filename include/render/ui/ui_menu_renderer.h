#pragma once

#include "render/types.h"
#include "render/ui/ui_render_commands.h"

namespace Render::UI {

class UIMenu;
class UIMenuItem;
class UIPullDownMenu;
class UITheme;

/**
 * @brief 菜单渲染器
 * 
 * 负责将菜单组件转换为渲染命令
 * 支持主题系统和自定义样式
 */
class UIMenuRenderer {
public:
    UIMenuRenderer() = default;
    ~UIMenuRenderer() = default;

    /**
     * @brief 渲染菜单项
     * @param menuItem 菜单项
     * @param commands 渲染命令缓冲区
     * @param theme 当前主题
     * @param layerID 渲染层级
     */
    void RenderMenuItem(
        const UIMenuItem& menuItem,
        UIRenderCommandBuffer& commands,
        const UITheme& theme,
        int layerID
    );

    /**
     * @brief 渲染菜单背景和边框
     * @param menu 菜单
     * @param commands 渲染命令缓冲区
     * @param theme 当前主题
     * @param layerID 渲染层级
     */
    void RenderMenuBackground(
        const UIMenu& menu,
        UIRenderCommandBuffer& commands,
        const UITheme& theme,
        int layerID
    );

    /**
     * @brief 渲染下拉菜单
     * @param pulldown 下拉菜单
     * @param commands 渲染命令缓冲区
     * @param theme 当前主题
     * @param layerID 渲染层级
     */
    void RenderPullDownMenu(
        const UIPullDownMenu& pulldown,
        UIRenderCommandBuffer& commands,
        const UITheme& theme,
        int layerID
    );

private:
    void RenderMenuItemBackground(
        const Rect& rect,
        const Color& color,
        UIRenderCommandBuffer& commands,
        int layerID
    );

    void RenderMenuItemText(
        const std::string& text,
        const Vector2& position,
        const Color& color,
        float fontSize,
        UIRenderCommandBuffer& commands,
        int layerID
    );

    void RenderMenuItemIcon(
        const std::string& iconPath,
        const Rect& rect,
        const Color& color,
        UIRenderCommandBuffer& commands,
        int layerID
    );

    void RenderMenuItemCheckMark(
        const Rect& rect,
        const Color& color,
        UIRenderCommandBuffer& commands,
        int layerID
    );

    void RenderMenuItemShortcut(
        const std::string& shortcut,
        const Vector2& position,
        const Color& color,
        float fontSize,
        UIRenderCommandBuffer& commands,
        int layerID
    );

    void RenderMenuItemArrow(
        const Rect& rect,
        const Color& color,
        UIRenderCommandBuffer& commands,
        int layerID
    );

    void RenderMenuSeparator(
        const Rect& rect,
        const Color& color,
        UIRenderCommandBuffer& commands,
        int layerID
    );
};

} // namespace Render::UI

