#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "render/types.h"

namespace Render::UI {

/**
 * @brief UI 主题颜色集合
 * 参考 Blender 的 uiWidgetColors 结构
 */
struct UIThemeColorSet {
    Color outline = Color(0.2f, 0.2f, 0.2f, 1.0f);          // 轮廓颜色
    Color inner = Color(0.9f, 0.9f, 0.9f, 1.0f);            // 内部颜色
    Color innerSelected = Color(0.7f, 0.7f, 0.9f, 1.0f);    // 选中内部颜色
    Color item = Color(0.95f, 0.95f, 0.95f, 1.0f);         // 项目颜色
    Color text = Color(0.2f, 0.2f, 0.2f, 1.0f);             // 文本颜色
    Color textSelected = Color(0.1f, 0.1f, 0.1f, 1.0f);   // 选中文本颜色
    Color shadow = Color(0.0f, 0.0f, 0.0f, 0.3f);           // 阴影颜色
};

/**
 * @brief 控件状态颜色集合
 */
struct UIThemeWidgetColors {
    UIThemeColorSet normal;      // 正常状态
    UIThemeColorSet hover;       // 悬停状态
    UIThemeColorSet pressed;      // 按下状态
    UIThemeColorSet disabled;     // 禁用状态
    UIThemeColorSet active;       // 激活状态（焦点）
};

/**
 * @brief 字体样式
 */
struct UIThemeFontStyle {
    std::string family = "NotoSansSC-Regular";  // 字体族
    float size = 14.0f;                         // 字体大小
    bool bold = false;                          // 粗体
    bool italic = false;                       // 斜体
};

/**
 * @brief 尺寸配置
 */
struct UIThemeSizes {
    float widgetUnit = 20.0f;        // 控件单位（像素）
    float panelSpace = 8.0f;          // 面板间距
    float buttonHeight = 40.0f;       // 按钮高度
    float textFieldHeight = 40.0f;   // 文本输入框高度
    float spacing = 8.0f;            // 默认间距
    float padding = 8.0f;            // 默认内边距
};

/**
 * @brief UI 主题
 * 参考 Blender 的 uiStyle 和 uiWidgetColors 结构
 */
class UITheme {
public:
    UITheme() = default;
    ~UITheme() = default;

    // 控件颜色
    UIThemeWidgetColors button;      // 按钮颜色
    UIThemeWidgetColors textField;   // 文本输入框颜色
    UIThemeWidgetColors panel;       // 面板颜色
    UIThemeWidgetColors menu;        // 菜单颜色

    // 字体样式
    UIThemeFontStyle widget;         // 控件字体
    UIThemeFontStyle widgetLabel;    // 标签字体
    UIThemeFontStyle menuFont;       // 菜单字体

    // 尺寸配置
    UIThemeSizes sizes;              // 尺寸配置

    // 背景颜色
    Color backgroundColor = Color(0.95f, 0.95f, 0.95f, 1.0f);  // 背景颜色
    Color borderColor = Color(0.3f, 0.3f, 0.3f, 1.0f);         // 边框颜色

    /**
     * @brief 获取控件颜色（根据状态）
     */
    [[nodiscard]] const UIThemeColorSet& GetWidgetColorSet(
        const std::string& widgetType,
        bool isHovered = false,
        bool isPressed = false,
        bool isDisabled = false,
        bool isActive = false) const;

    /**
     * @brief 创建默认主题
     */
    static UITheme CreateDefault();

    /**
     * @brief 创建暗色主题
     */
    static UITheme CreateDark();

    /**
     * @brief 从 JSON 加载主题
     */
    static bool LoadFromJSON(const std::string& jsonPath, UITheme& theme);

    /**
     * @brief 保存主题到 JSON
     */
    static bool SaveToJSON(const UITheme& theme, const std::string& jsonPath);
};

/**
 * @brief 主题管理器
 * 单例模式，管理主题的加载、切换和应用
 */
class UIThemeManager {
public:
    static UIThemeManager& GetInstance();

    /**
     * @brief 加载主题文件
     */
    bool LoadTheme(const std::string& themeName, const std::string& themePath);

    /**
     * @brief 设置当前主题
     */
    void SetCurrentTheme(const std::string& themeName);

    /**
     * @brief 获取当前主题
     */
    [[nodiscard]] const UITheme& GetCurrentTheme() const;

    /**
     * @brief 获取主题（如果不存在则返回默认主题）
     */
    [[nodiscard]] const UITheme& GetTheme(const std::string& themeName) const;

    /**
     * @brief 注册内置主题
     */
    void RegisterBuiltinTheme(const std::string& name, const UITheme& theme);

    /**
     * @brief 获取 DPI 缩放后的主题（用于高 DPI 显示）
     */
    [[nodiscard]] UITheme GetThemeForDPI(float dpiScale) const;

    /**
     * @brief 初始化默认主题
     */
    void InitializeDefaults();

private:
    UIThemeManager() = default;
    ~UIThemeManager() = default;

    UIThemeManager(const UIThemeManager&) = delete;
    UIThemeManager& operator=(const UIThemeManager&) = delete;

    std::unordered_map<std::string, UITheme> m_themes;
    std::string m_currentThemeName = "default";
    UITheme m_defaultTheme = UITheme::CreateDefault();
};

} // namespace Render::UI

