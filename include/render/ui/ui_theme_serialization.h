#pragma once

#include "render/ui/ui_theme.h"
#include "render/json_serializer.h"
#include <nlohmann/json.hpp>

/**
 * @file ui_theme_serialization.h
 * @brief UI主题的JSON序列化支持
 * 
 * 该文件为UI主题相关的结构体提供JSON序列化和反序列化功能。
 * 将序列化代码从主题类中分离出来，保持代码清晰和模块化。
 */

namespace Render::UI {

// ============================================================================
// UIThemeColorSet 序列化
// ============================================================================

inline void to_json(nlohmann::json& j, const UIThemeColorSet& colorSet) {
    j = {
        {"outline", colorSet.outline},
        {"inner", colorSet.inner},
        {"innerSelected", colorSet.innerSelected},
        {"item", colorSet.item},
        {"text", colorSet.text},
        {"textSelected", colorSet.textSelected},
        {"shadow", colorSet.shadow}
    };
}

inline void from_json(const nlohmann::json& j, UIThemeColorSet& colorSet) {
    if (j.contains("outline")) j.at("outline").get_to(colorSet.outline);
    if (j.contains("inner")) j.at("inner").get_to(colorSet.inner);
    if (j.contains("innerSelected")) j.at("innerSelected").get_to(colorSet.innerSelected);
    if (j.contains("item")) j.at("item").get_to(colorSet.item);
    if (j.contains("text")) j.at("text").get_to(colorSet.text);
    if (j.contains("textSelected")) j.at("textSelected").get_to(colorSet.textSelected);
    if (j.contains("shadow")) j.at("shadow").get_to(colorSet.shadow);
}

// ============================================================================
// UIThemeWidgetColors 序列化
// ============================================================================

inline void to_json(nlohmann::json& j, const UIThemeWidgetColors& widgetColors) {
    j = {
        {"normal", widgetColors.normal},
        {"hover", widgetColors.hover},
        {"pressed", widgetColors.pressed},
        {"disabled", widgetColors.disabled},
        {"active", widgetColors.active}
    };
}

inline void from_json(const nlohmann::json& j, UIThemeWidgetColors& widgetColors) {
    if (j.contains("normal")) j.at("normal").get_to(widgetColors.normal);
    if (j.contains("hover")) j.at("hover").get_to(widgetColors.hover);
    if (j.contains("pressed")) j.at("pressed").get_to(widgetColors.pressed);
    if (j.contains("disabled")) j.at("disabled").get_to(widgetColors.disabled);
    if (j.contains("active")) j.at("active").get_to(widgetColors.active);
}

// ============================================================================
// UIThemeFontStyle 序列化
// ============================================================================

inline void to_json(nlohmann::json& j, const UIThemeFontStyle& fontStyle) {
    j = {
        {"family", fontStyle.family},
        {"size", fontStyle.size},
        {"bold", fontStyle.bold},
        {"italic", fontStyle.italic}
    };
}

inline void from_json(const nlohmann::json& j, UIThemeFontStyle& fontStyle) {
    if (j.contains("family")) j.at("family").get_to(fontStyle.family);
    if (j.contains("size")) j.at("size").get_to(fontStyle.size);
    if (j.contains("bold")) j.at("bold").get_to(fontStyle.bold);
    if (j.contains("italic")) j.at("italic").get_to(fontStyle.italic);
}

// ============================================================================
// UIThemeSizes 序列化
// ============================================================================

inline void to_json(nlohmann::json& j, const UIThemeSizes& sizes) {
    j = {
        {"widgetUnit", sizes.widgetUnit},
        {"panelSpace", sizes.panelSpace},
        {"buttonHeight", sizes.buttonHeight},
        {"textFieldHeight", sizes.textFieldHeight},
        {"spacing", sizes.spacing},
        {"padding", sizes.padding}
    };
}

inline void from_json(const nlohmann::json& j, UIThemeSizes& sizes) {
    if (j.contains("widgetUnit")) j.at("widgetUnit").get_to(sizes.widgetUnit);
    if (j.contains("panelSpace")) j.at("panelSpace").get_to(sizes.panelSpace);
    if (j.contains("buttonHeight")) j.at("buttonHeight").get_to(sizes.buttonHeight);
    if (j.contains("textFieldHeight")) j.at("textFieldHeight").get_to(sizes.textFieldHeight);
    if (j.contains("spacing")) j.at("spacing").get_to(sizes.spacing);
    if (j.contains("padding")) j.at("padding").get_to(sizes.padding);
}

// ============================================================================
// UITheme 序列化
// ============================================================================

inline void to_json(nlohmann::json& j, const UITheme& theme) {
    j = {
        {"version", "1.0"},
        {"colors", {
            {"button", theme.button},
            {"textField", theme.textField},
            {"panel", theme.panel},
            {"menu", theme.menu}
        }},
        {"fonts", {
            {"widget", theme.widget},
            {"widgetLabel", theme.widgetLabel},
            {"menuFont", theme.menuFont}
        }},
        {"sizes", theme.sizes},
        {"backgroundColor", theme.backgroundColor},
        {"borderColor", theme.borderColor}
    };
}

inline void from_json(const nlohmann::json& j, UITheme& theme) {
    // 检查版本（可选）
    if (j.contains("version")) {
        std::string version = j.at("version").get<std::string>();
        // 未来可以根据版本号进行不同的解析
    }
    
    // 加载颜色
    if (j.contains("colors")) {
        const auto& colors = j.at("colors");
        if (colors.contains("button")) colors.at("button").get_to(theme.button);
        if (colors.contains("textField")) colors.at("textField").get_to(theme.textField);
        if (colors.contains("panel")) colors.at("panel").get_to(theme.panel);
        if (colors.contains("menu")) colors.at("menu").get_to(theme.menu);
    }
    
    // 加载字体
    if (j.contains("fonts")) {
        const auto& fonts = j.at("fonts");
        if (fonts.contains("widget")) fonts.at("widget").get_to(theme.widget);
        if (fonts.contains("widgetLabel")) fonts.at("widgetLabel").get_to(theme.widgetLabel);
        if (fonts.contains("menuFont")) fonts.at("menuFont").get_to(theme.menuFont);
    }
    
    // 加载尺寸
    if (j.contains("sizes")) {
        j.at("sizes").get_to(theme.sizes);
    }
    
    // 加载背景和边框颜色
    if (j.contains("backgroundColor")) j.at("backgroundColor").get_to(theme.backgroundColor);
    if (j.contains("borderColor")) j.at("borderColor").get_to(theme.borderColor);
}

} // namespace Render::UI


