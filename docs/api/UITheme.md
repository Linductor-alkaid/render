# UITheme API 参考

[返回 API 首页](README.md)

---

## 概述

`UITheme` 和 `UIThemeManager` 提供完整的UI主题系统，管理颜色、字体和尺寸配置。

**头文件**: `render/ui/ui_theme.h`  
**命名空间**: `Render::UI`

**参考**: 基于 Blender 的 `uiStyle` 和 `uiWidgetColors` 设计

### 🎨 核心特性

- **多主题支持**: 内置默认主题和暗色主题
- **状态颜色**: 为不同控件状态定义颜色
- **字体管理**: 字体族、大小、样式配置
- **DPI 适配**: 自动缩放以适应高DPI显示
- **运行时切换**: 动态切换主题无需重启

---

## UITheme 类

### 类定义

```cpp
class UITheme {
public:
    UITheme() = default;

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
    Color backgroundColor;
    Color borderColor;

    // 工具方法
    [[nodiscard]] const UIThemeColorSet& GetWidgetColorSet(
        const std::string& widgetType,
        bool isHovered = false,
        bool isPressed = false,
        bool isDisabled = false,
        bool isActive = false) const;

    // 菜单颜色辅助方法
    [[nodiscard]] Color GetMenuBackgroundColor() const;
    [[nodiscard]] Color GetMenuBorderColor() const;
    [[nodiscard]] Color GetMenuItemNormalBackgroundColor() const;
    [[nodiscard]] Color GetMenuItemHoverBackgroundColor() const;
    // ... 更多菜单颜色方法

    // 静态工厂方法
    static UITheme CreateDefault();
    static UITheme CreateDark();

    // 序列化
    static bool LoadFromJSON(const std::string& jsonPath, UITheme& theme);
    static bool SaveToJSON(const UITheme& theme, const std::string& jsonPath);
};
```

---

## 数据结构

### UIThemeColorSet

单个颜色集合。

```cpp
struct UIThemeColorSet {
    Color outline;          // 轮廓颜色
    Color inner;            // 内部颜色
    Color innerSelected;    // 选中内部颜色
    Color item;             // 项目颜色
    Color text;             // 文本颜色
    Color textSelected;     // 选中文本颜色
    Color shadow;           // 阴影颜色
};
```

---

### UIThemeWidgetColors

控件状态颜色集合。

```cpp
struct UIThemeWidgetColors {
    UIThemeColorSet normal;      // 正常状态
    UIThemeColorSet hover;       // 悬停状态
    UIThemeColorSet pressed;     // 按下状态
    UIThemeColorSet disabled;    // 禁用状态
    UIThemeColorSet active;      // 激活状态（焦点）
};
```

---

### UIThemeFontStyle

字体样式。

```cpp
struct UIThemeFontStyle {
    std::string family = "NotoSansSC-Regular";  // 字体族
    float size = 14.0f;                         // 字体大小
    bool bold = false;                          // 粗体
    bool italic = false;                        // 斜体
};
```

---

### UIThemeSizes

尺寸配置。

```cpp
struct UIThemeSizes {
    float widgetUnit = 20.0f;        // 控件单位（像素）
    float panelSpace = 8.0f;         // 面板间距
    float buttonHeight = 40.0f;      // 按钮高度
    float textFieldHeight = 40.0f;   // 文本输入框高度
    float spacing = 8.0f;            // 默认间距
    float padding = 8.0f;            // 默认内边距
};
```

---

## UIThemeManager 类

### 类定义

```cpp
class UIThemeManager {
public:
    static UIThemeManager& GetInstance();

    bool LoadTheme(const std::string& themeName, const std::string& themePath);
    void SetCurrentTheme(const std::string& themeName);
    [[nodiscard]] const UITheme& GetCurrentTheme() const;
    [[nodiscard]] const UITheme& GetTheme(const std::string& themeName) const;
    void RegisterBuiltinTheme(const std::string& name, const UITheme& theme);
    [[nodiscard]] UITheme GetThemeForDPI(float dpiScale) const;
    void InitializeDefaults();
};
```

---

## 方法详解

### GetInstance

获取单例实例。

```cpp
static UIThemeManager& GetInstance();
```

**返回值**: 主题管理器引用

**示例**:
```cpp
auto& themeManager = UIThemeManager::GetInstance();
```

---

### LoadTheme

从JSON文件加载主题。

```cpp
bool LoadTheme(const std::string& themeName, const std::string& themePath);
```

**参数**:
- `themeName` - 主题名称
- `themePath` - JSON文件路径

**返回值**: 成功返回 `true`

**示例**:
```cpp
themeManager.LoadTheme("custom", "themes/custom.json");
```

---

### SetCurrentTheme

设置当前主题。

```cpp
void SetCurrentTheme(const std::string& themeName);
```

**参数**:
- `themeName` - 主题名称

**示例**:
```cpp
themeManager.SetCurrentTheme("dark");
```

---

### GetCurrentTheme

获取当前主题。

```cpp
[[nodiscard]] const UITheme& GetCurrentTheme() const;
```

**返回值**: 当前主题对象引用

**示例**:
```cpp
const auto& theme = themeManager.GetCurrentTheme();
Color btnColor = theme.button.normal.inner;
```

---

### GetThemeForDPI

获取DPI缩放后的主题。

```cpp
[[nodiscard]] UITheme GetThemeForDPI(float dpiScale) const;
```

**参数**:
- `dpiScale` - DPI缩放因子（如 1.5 表示 150%）

**返回值**: 缩放后的主题对象

**示例**:
```cpp
// 为4K显示器获取2倍缩放主题
UITheme scaledTheme = themeManager.GetThemeForDPI(2.0f);
```

---

### InitializeDefaults

初始化默认主题。

```cpp
void InitializeDefaults();
```

**说明**: 
- 注册 "default" 和 "dark" 两个内置主题
- 设置 "default" 为当前主题
- 通常在程序启动时自动调用

---

## 主题JSON格式

### 示例主题文件

```json
{
  "name": "Dark",
  "version": "1.0",
  "colors": {
    "button": {
      "normal": {
        "outline": [64, 64, 64, 255],
        "inner": [32, 32, 32, 255],
        "text": [240, 240, 240, 255]
      },
      "hover": {
        "outline": [96, 96, 96, 255],
        "inner": [48, 48, 48, 255],
        "text": [255, 255, 255, 255]
      },
      "pressed": {
        "outline": [80, 80, 80, 255],
        "inner": [40, 40, 40, 255],
        "text": [200, 200, 200, 255]
      },
      "disabled": {
        "outline": [48, 48, 48, 255],
        "inner": [24, 24, 24, 255],
        "text": [128, 128, 128, 255]
      }
    }
  },
  "fonts": {
    "widget": {
      "family": "NotoSansSC-Regular",
      "size": 14.0,
      "bold": false
    },
    "menu": {
      "family": "NotoSansSC-Regular",
      "size": 13.0,
      "bold": false
    }
  },
  "sizes": {
    "widgetUnit": 20.0,
    "panelSpace": 8.0,
    "buttonHeight": 32.0,
    "textFieldHeight": 32.0,
    "spacing": 6.0,
    "padding": 8.0
  }
}
```

---

## 使用示例

### 切换主题

```cpp
auto& themeManager = UIThemeManager::GetInstance();

// 切换到暗色主题
themeManager.SetCurrentTheme("dark");

// 切换回默认主题
themeManager.SetCurrentTheme("default");
```

### 加载自定义主题

```cpp
auto& themeManager = UIThemeManager::GetInstance();

// 加载自定义主题
if (themeManager.LoadTheme("custom", "themes/my_theme.json")) {
    themeManager.SetCurrentTheme("custom");
    LOG_INFO("Custom theme loaded");
} else {
    LOG_ERROR("Failed to load custom theme");
}
```

### 创建程序化主题

```cpp
UITheme customTheme;

// 设置按钮颜色
customTheme.button.normal.inner = Color(0.2f, 0.5f, 0.8f, 1.0f);
customTheme.button.normal.text = Color::White();
customTheme.button.hover.inner = Color(0.3f, 0.6f, 0.9f, 1.0f);

// 设置字体
customTheme.widget.size = 16.0f;
customTheme.widget.bold = true;

// 注册主题
UIThemeManager::GetInstance().RegisterBuiltinTheme("custom", customTheme);
UIThemeManager::GetInstance().SetCurrentTheme("custom");
```

### 高DPI支持

```cpp
auto& themeManager = UIThemeManager::GetInstance();

// 检测系统DPI
float systemDPI = GetSystemDPI();
float dpiScale = systemDPI / 96.0f;

// 获取缩放主题
UITheme scaledTheme = themeManager.GetThemeForDPI(dpiScale);

// 使用缩放后的字体大小
float fontSize = scaledTheme.widget.size;
```

---

## 获取控件颜色

### GetWidgetColorSet

根据控件类型和状态获取颜色集。

```cpp
const UIThemeColorSet& colorSet = theme.GetWidgetColorSet(
    "button",           // 控件类型
    isHovered,          // 是否悬停
    isPressed,          // 是否按下
    isDisabled,         // 是否禁用
    isActive            // 是否激活
);

Color bgColor = colorSet.inner;
Color textColor = colorSet.text;
```

---

## 内置主题

### Default（默认浅色主题）

```cpp
UITheme theme = UITheme::CreateDefault();
```

- 浅色背景
- 深色文本
- 适合日间使用

### Dark（暗色主题）

```cpp
UITheme theme = UITheme::CreateDark();
```

- 深色背景
- 浅色文本
- 适合夜间使用

---

## 最佳实践

### 1. 初始化

```cpp
// 程序启动时初始化
UIThemeManager::GetInstance().InitializeDefaults();
```

### 2. 获取颜色

```cpp
// 总是通过 GetCurrentTheme 获取
const auto& theme = UIThemeManager::GetInstance().GetCurrentTheme();
Color btnColor = theme.button.normal.inner;
```

### 3. 动态切换

```cpp
// 提供主题切换UI
void OnThemeSelected(const std::string& themeName) {
    UIThemeManager::GetInstance().SetCurrentTheme(themeName);
    // UI会在下一帧自动使用新主题
}
```

---

## 注意事项

1. **单例模式**: `UIThemeManager` 是全局单例
2. **自动初始化**: `UIRuntimeModule` 会自动调用 `InitializeDefaults()`
3. **运行时切换**: 切换主题后UI会在下一帧应用新样式
4. **DPI缩放**: 缩放仅影响字体大小和控件尺寸，不影响颜色

---

## 参见

- [UIWidget](UIWidget.md) - 控件基类
- [UIButton](UIButton.md) - 按钮控件
- [UI主题系统文档](../ui/UI_MENU_SYSTEM.md)

---

[返回 API 首页](README.md)

