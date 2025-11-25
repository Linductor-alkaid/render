#include "render/ui/ui_theme.h"

#include <fstream>
#include <sstream>

#include "render/logger.h"
#include "render/json_serializer.h"
#include "render/ui/ui_theme_serialization.h"

namespace Render::UI {

// UITheme 实现

const UIThemeColorSet& UITheme::GetWidgetColorSet(
    const std::string& widgetType,
    bool isHovered,
    bool isPressed,
    bool isDisabled,
    bool isActive) const {
    
    const UIThemeWidgetColors* colors = nullptr;
    
    if (widgetType == "button") {
        colors = &button;
    } else if (widgetType == "textField") {
        colors = &textField;
    } else if (widgetType == "panel") {
        colors = &panel;
    } else if (widgetType == "menu") {
        colors = &menu;
    } else {
        // 默认使用按钮颜色
        colors = &button;
    }
    
    if (isDisabled) {
        return colors->disabled;
    }
    if (isPressed) {
        return colors->pressed;
    }
    if (isActive) {
        return colors->active;
    }
    if (isHovered) {
        return colors->hover;
    }
    return colors->normal;
}

UITheme UITheme::CreateDefault() {
    UITheme theme;
    
    // 按钮颜色（圆角控件默认填充颜色为灰白色）
    theme.button.normal.inner = Color(0.92f, 0.92f, 0.92f, 1.0f);  // 灰白色
    theme.button.normal.text = Color(0.2f, 0.2f, 0.2f, 1.0f);
    theme.button.hover.inner = Color(0.96f, 0.96f, 0.96f, 1.0f);  // 稍亮的灰白色
    theme.button.hover.text = Color(0.2f, 0.2f, 0.2f, 1.0f);
    theme.button.pressed.inner = Color(0.85f, 0.85f, 0.85f, 1.0f);  // 稍暗的灰白色
    theme.button.pressed.text = Color(0.15f, 0.15f, 0.15f, 1.0f);
    theme.button.disabled.inner = Color(0.7f, 0.7f, 0.7f, 1.0f);
    theme.button.disabled.text = Color(0.5f, 0.5f, 0.5f, 1.0f);
    
    // 文本输入框颜色（圆角控件默认填充颜色为灰白色）
    theme.textField.normal.inner = Color(0.92f, 0.92f, 0.92f, 1.0f);  // 灰白色
    theme.textField.normal.text = Color(0.2f, 0.2f, 0.2f, 1.0f);
    theme.textField.hover.inner = Color(0.95f, 0.95f, 0.95f, 1.0f);  // 稍亮的灰白色
    theme.textField.hover.text = Color(0.2f, 0.2f, 0.2f, 1.0f);
    theme.textField.active.inner = Color(0.98f, 0.98f, 0.98f, 1.0f);  // 更亮的灰白色
    theme.textField.active.text = Color(0.1f, 0.1f, 0.1f, 1.0f);
    theme.textField.disabled.inner = Color(0.9f, 0.9f, 0.9f, 1.0f);
    theme.textField.disabled.text = Color(0.6f, 0.6f, 0.6f, 1.0f);
    
    // 面板颜色（圆角控件默认填充颜色为灰白色）
    theme.panel.normal.inner = Color(0.92f, 0.92f, 0.92f, 1.0f);  // 灰白色
    theme.panel.normal.text = Color(0.2f, 0.2f, 0.2f, 1.0f);
    
    // 菜单颜色
    theme.menu.normal.inner = Color(0.98f, 0.98f, 0.98f, 1.0f);
    theme.menu.normal.text = Color(0.2f, 0.2f, 0.2f, 1.0f);
    theme.menu.hover.inner = Color(0.9f, 0.9f, 0.95f, 1.0f);
    theme.menu.hover.text = Color(0.1f, 0.1f, 0.1f, 1.0f);
    
    // 字体样式
    theme.widget.family = "NotoSansSC-Regular";
    theme.widget.size = 14.0f;
    theme.widgetLabel.family = "NotoSansSC-Regular";
    theme.widgetLabel.size = 14.0f;
    theme.menuFont.family = "NotoSansSC-Regular";
    theme.menuFont.size = 14.0f;
    
    // 尺寸配置
    theme.sizes.widgetUnit = 20.0f;
    theme.sizes.panelSpace = 8.0f;
    theme.sizes.buttonHeight = 40.0f;
    theme.sizes.textFieldHeight = 40.0f;
    theme.sizes.spacing = 8.0f;
    theme.sizes.padding = 8.0f;
    
    // 背景和边框
    theme.backgroundColor = Color(0.95f, 0.95f, 0.95f, 1.0f);
    theme.borderColor = Color(0.3f, 0.3f, 0.3f, 1.0f);
    
    return theme;
}

UITheme UITheme::CreateDark() {
    UITheme theme;
    
    // 按钮颜色（暗色主题）
    theme.button.normal.inner = Color(0.25f, 0.25f, 0.3f, 1.0f);
    theme.button.normal.text = Color(0.9f, 0.9f, 0.9f, 1.0f);
    theme.button.hover.inner = Color(0.35f, 0.35f, 0.4f, 1.0f);
    theme.button.hover.text = Color(1.0f, 1.0f, 1.0f, 1.0f);
    theme.button.pressed.inner = Color(0.15f, 0.15f, 0.2f, 1.0f);
    theme.button.pressed.text = Color(0.95f, 0.95f, 0.95f, 1.0f);
    theme.button.disabled.inner = Color(0.2f, 0.2f, 0.2f, 1.0f);
    theme.button.disabled.text = Color(0.5f, 0.5f, 0.5f, 1.0f);
    
    // 文本输入框颜色（暗色主题）
    theme.textField.normal.inner = Color(0.2f, 0.2f, 0.2f, 1.0f);
    theme.textField.normal.text = Color(0.9f, 0.9f, 0.9f, 1.0f);
    theme.textField.hover.inner = Color(0.25f, 0.25f, 0.25f, 1.0f);
    theme.textField.hover.text = Color(0.9f, 0.9f, 0.9f, 1.0f);
    theme.textField.active.inner = Color(0.15f, 0.2f, 0.25f, 1.0f);
    theme.textField.active.text = Color(1.0f, 1.0f, 1.0f, 1.0f);
    theme.textField.disabled.inner = Color(0.15f, 0.15f, 0.15f, 1.0f);
    theme.textField.disabled.text = Color(0.5f, 0.5f, 0.5f, 1.0f);
    
    // 面板颜色（暗色主题）
    theme.panel.normal.inner = Color(0.15f, 0.15f, 0.15f, 1.0f);
    theme.panel.normal.text = Color(0.9f, 0.9f, 0.9f, 1.0f);
    
    // 菜单颜色（暗色主题）
    theme.menu.normal.inner = Color(0.2f, 0.2f, 0.2f, 1.0f);
    theme.menu.normal.text = Color(0.9f, 0.9f, 0.9f, 1.0f);
    theme.menu.hover.inner = Color(0.3f, 0.3f, 0.35f, 1.0f);
    theme.menu.hover.text = Color(1.0f, 1.0f, 1.0f, 1.0f);
    
    // 字体样式（与默认主题相同）
    theme.widget.family = "NotoSansSC-Regular";
    theme.widget.size = 14.0f;
    theme.widgetLabel.family = "NotoSansSC-Regular";
    theme.widgetLabel.size = 14.0f;
    theme.menuFont.family = "NotoSansSC-Regular";
    theme.menuFont.size = 14.0f;
    
    // 尺寸配置（与默认主题相同）
    theme.sizes.widgetUnit = 20.0f;
    theme.sizes.panelSpace = 8.0f;
    theme.sizes.buttonHeight = 40.0f;
    theme.sizes.textFieldHeight = 40.0f;
    theme.sizes.spacing = 8.0f;
    theme.sizes.padding = 8.0f;
    
    // 背景和边框（暗色主题）
    theme.backgroundColor = Color(0.1f, 0.1f, 0.1f, 1.0f);
    theme.borderColor = Color(0.4f, 0.4f, 0.4f, 1.0f);
    
    return theme;
}

bool UITheme::LoadFromJSON(const std::string& jsonPath, UITheme& theme) {
    nlohmann::json j;
    if (!JsonSerializer::LoadFromFile(jsonPath, j)) {
        Logger::GetInstance().ErrorFormat("[UITheme] Failed to load JSON from '%s'", jsonPath.c_str());
        return false;
    }
    
    try {
        theme = j.get<UITheme>();
        Logger::GetInstance().InfoFormat("[UITheme] Successfully loaded theme from '%s'", jsonPath.c_str());
        return true;
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("[UITheme] Failed to parse theme from '%s': %s", 
                                         jsonPath.c_str(), e.what());
        return false;
    }
}

bool UITheme::SaveToJSON(const UITheme& theme, const std::string& jsonPath) {
    try {
        nlohmann::json j = theme;
        if (!JsonSerializer::SaveToFile(j, jsonPath)) {
            Logger::GetInstance().ErrorFormat("[UITheme] Failed to save JSON to '%s'", jsonPath.c_str());
            return false;
        }
        
        Logger::GetInstance().InfoFormat("[UITheme] Successfully saved theme to '%s'", jsonPath.c_str());
        return true;
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("[UITheme] Failed to serialize theme to '%s': %s", 
                                         jsonPath.c_str(), e.what());
        return false;
    }
}

// UIThemeManager 实现

UIThemeManager& UIThemeManager::GetInstance() {
    static UIThemeManager instance;
    return instance;
}

bool UIThemeManager::LoadTheme(const std::string& themeName, const std::string& themePath) {
    UITheme theme;
    if (!UITheme::LoadFromJSON(themePath, theme)) {
        Logger::GetInstance().ErrorFormat("[UIThemeManager] Failed to load theme '%s' from '%s'", 
                                         themeName.c_str(), themePath.c_str());
        return false;
    }
    
    m_themes[themeName] = std::move(theme);
    Logger::GetInstance().InfoFormat("[UIThemeManager] Loaded theme '%s' from '%s'", 
                                    themeName.c_str(), themePath.c_str());
    return true;
}

void UIThemeManager::SetCurrentTheme(const std::string& themeName) {
    if (m_themes.find(themeName) != m_themes.end() || themeName == "default") {
        m_currentThemeName = themeName;
        Logger::GetInstance().InfoFormat("[UIThemeManager] Switched to theme '%s'", themeName.c_str());
    } else {
        Logger::GetInstance().WarningFormat("[UIThemeManager] Theme '%s' not found, using default", themeName.c_str());
        m_currentThemeName = "default";
    }
}

const UITheme& UIThemeManager::GetCurrentTheme() const {
    if (m_currentThemeName == "default") {
        return m_defaultTheme;
    }
    
    auto it = m_themes.find(m_currentThemeName);
    if (it != m_themes.end()) {
        return it->second;
    }
    
    Logger::GetInstance().WarningFormat("[UIThemeManager] Theme '%s' not found, using default", 
                                        m_currentThemeName.c_str());
    return m_defaultTheme;
}

const UITheme& UIThemeManager::GetTheme(const std::string& themeName) const {
    if (themeName == "default") {
        return m_defaultTheme;
    }
    
    auto it = m_themes.find(themeName);
    if (it != m_themes.end()) {
        return it->second;
    }
    
    Logger::GetInstance().WarningFormat("[UIThemeManager] Theme '%s' not found, using default", themeName.c_str());
    return m_defaultTheme;
}

void UIThemeManager::RegisterBuiltinTheme(const std::string& name, const UITheme& theme) {
    m_themes[name] = theme;
    Logger::GetInstance().InfoFormat("[UIThemeManager] Registered builtin theme '%s'", name.c_str());
}

UITheme UIThemeManager::GetThemeForDPI(float dpiScale) const {
    const UITheme& baseTheme = GetCurrentTheme();
    UITheme scaledTheme = baseTheme;
    
    // 缩放字体大小
    scaledTheme.widget.size = baseTheme.widget.size * dpiScale;
    scaledTheme.widgetLabel.size = baseTheme.widgetLabel.size * dpiScale;
    scaledTheme.menuFont.size = baseTheme.menuFont.size * dpiScale;
    
    // 缩放尺寸配置
    scaledTheme.sizes.widgetUnit = baseTheme.sizes.widgetUnit * dpiScale;
    scaledTheme.sizes.panelSpace = baseTheme.sizes.panelSpace * dpiScale;
    scaledTheme.sizes.buttonHeight = baseTheme.sizes.buttonHeight * dpiScale;
    scaledTheme.sizes.textFieldHeight = baseTheme.sizes.textFieldHeight * dpiScale;
    scaledTheme.sizes.spacing = baseTheme.sizes.spacing * dpiScale;
    scaledTheme.sizes.padding = baseTheme.sizes.padding * dpiScale;
    
    return scaledTheme;
}

void UIThemeManager::InitializeDefaults() {
    // 注册默认主题
    m_themes["default"] = UITheme::CreateDefault();
    m_themes["dark"] = UITheme::CreateDark();
    m_currentThemeName = "default";
    
    Logger::GetInstance().Info("[UIThemeManager] Initialized default themes (default, dark)");
}

} // namespace Render::UI

