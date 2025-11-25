# UIMenuItem API 参考

[返回 API 首页](README.md)

---

## 概述

`UIMenuItem` 是菜单项类，继承自 `UIWidget`，表示菜单中的单个项目。支持普通项、可选中项、分隔符和子菜单四种类型。

**头文件**: `render/ui/widgets/ui_menu_item.h`  
**命名空间**: `Render::UI`

**参考**: 基于 Blender 的 `UI_WTYPE_MENU_ITEM` 设计

### 🎨 核心特性

- **多种类型**: 普通、可选中、分隔符、子菜单
- **文本和图标**: 支持标签、图标、快捷键显示
- **工具提示**: 悬停提示信息
- **状态管理**: 选中/未选中、启用/禁用、悬停/按下
- **子菜单支持**: 可嵌套子菜单

---

## 类定义

```cpp
class UIMenuItem : public UIWidget {
public:
    using ClickHandler = std::function<void(UIMenuItem&)>;

    explicit UIMenuItem(std::string id, UIMenuItemType type = UIMenuItemType::Normal);

    // 基本属性
    void SetLabel(std::string label);
    [[nodiscard]] const std::string& GetLabel() const noexcept;

    void SetIcon(std::string iconPath);
    [[nodiscard]] const std::string& GetIcon() const noexcept;

    void SetShortcut(std::string shortcut);
    [[nodiscard]] const std::string& GetShortcut() const noexcept;

    void SetTooltip(std::string tooltip);
    [[nodiscard]] const std::string& GetTooltip() const noexcept;

    // 类型和状态
    [[nodiscard]] UIMenuItemType GetType() const noexcept;
    
    void SetCheckable(bool checkable);
    [[nodiscard]] bool IsCheckable() const noexcept;
    
    void SetChecked(bool checked);
    [[nodiscard]] bool IsChecked() const noexcept;

    void SetSeparator(bool separator);
    [[nodiscard]] bool IsSeparator() const noexcept;

    // 子菜单
    void SetSubMenu(std::shared_ptr<UIMenu> subMenu);
    [[nodiscard]] std::shared_ptr<UIMenu> GetSubMenu() const noexcept;
    [[nodiscard]] bool HasSubMenu() const noexcept;

    // 交互状态
    [[nodiscard]] bool IsHovered() const noexcept;
    [[nodiscard]] bool IsPressed() const noexcept;

    // 回调
    void SetOnClicked(ClickHandler handler);
    void SetOnCheckChanged(std::function<void(UIMenuItem&, bool)> handler);

    // 程序化触发
    void Click();
};
```

---

## 菜单项类型

```cpp
enum class UIMenuItemType : uint8_t {
    Normal,      // 普通菜单项
    Checkable,   // 可选中菜单项
    Separator,   // 分隔符
    SubMenu      // 子菜单
};
```

---

## 构造函数

### UIMenuItem

创建菜单项实例。

```cpp
explicit UIMenuItem(std::string id, UIMenuItemType type = UIMenuItemType::Normal);
```

**参数**:
- `id` - 菜单项的唯一标识符
- `type` - 菜单项类型

**示例**:
```cpp
// 普通菜单项
auto item = std::make_unique<UIMenuItem>("save", UIMenuItemType::Normal);

// 可选中菜单项
auto checkItem = std::make_unique<UIMenuItem>("grid", UIMenuItemType::Checkable);

// 分隔符
auto separator = std::make_unique<UIMenuItem>("sep1", UIMenuItemType::Separator);
```

---

## 基本属性

### SetLabel

设置菜单项文本。

```cpp
void SetLabel(std::string label);
```

**参数**:
- `label` - 显示文本

**示例**:
```cpp
item->SetLabel("Save File");
```

---

### GetLabel

获取菜单项文本。

```cpp
[[nodiscard]] const std::string& GetLabel() const noexcept;
```

**返回值**: 文本字符串引用

---

### SetIcon

设置菜单项图标。

```cpp
void SetIcon(std::string iconPath);
```

**参数**:
- `iconPath` - 图标路径

**注意**: 当前为预留接口，完整图标系统待实现

---

### SetShortcut

设置快捷键文本。

```cpp
void SetShortcut(std::string shortcut);
```

**参数**:
- `shortcut` - 快捷键文本（如 "Ctrl+S"）

**示例**:
```cpp
saveItem->SetShortcut("Ctrl+S");
```

---

### SetTooltip

设置工具提示文本。

```cpp
void SetTooltip(std::string tooltip);
```

**参数**:
- `tooltip` - 提示文本

**示例**:
```cpp
item->SetTooltip("Save the current document");
```

---

## 类型和状态

### GetType

获取菜单项类型。

```cpp
[[nodiscard]] UIMenuItemType GetType() const noexcept;
```

**返回值**: 菜单项类型枚举值

---

### SetCheckable

设置是否可选中。

```cpp
void SetCheckable(bool checkable);
```

**参数**:
- `checkable` - `true` 表示可选中

**说明**: 会将类型切换为 `Checkable` 或 `Normal`

---

### IsCheckable

检查是否为可选中类型。

```cpp
[[nodiscard]] bool IsCheckable() const noexcept;
```

**返回值**: 可选中返回 `true`

---

### SetChecked

设置选中状态（仅对可选中项有效）。

```cpp
void SetChecked(bool checked);
```

**参数**:
- `checked` - 选中状态

**说明**: 会触发 `OnCheckChanged` 回调

**示例**:
```cpp
gridItem->SetChecked(true);
```

---

### IsChecked

获取选中状态。

```cpp
[[nodiscard]] bool IsChecked() const noexcept;
```

**返回值**: 选中返回 `true`

---

### IsSeparator

检查是否为分隔符。

```cpp
[[nodiscard]] bool IsSeparator() const noexcept;
```

**返回值**: 分隔符返回 `true`

---

## 子菜单

### SetSubMenu

设置子菜单。

```cpp
void SetSubMenu(std::shared_ptr<UIMenu> subMenu);
```

**参数**:
- `subMenu` - 子菜单对象

**说明**: 设置后类型会自动变为 `SubMenu`

**示例**:
```cpp
auto exportMenu = std::make_shared<UIMenu>("export_menu");
exportMenu->AddMenuItem("png", "PNG Image");
exportMenu->AddMenuItem("jpg", "JPEG Image");

exportItem->SetSubMenu(exportMenu);
```

---

### GetSubMenu

获取子菜单。

```cpp
[[nodiscard]] std::shared_ptr<UIMenu> GetSubMenu() const noexcept;
```

**返回值**: 子菜单对象指针，无子菜单返回 `nullptr`

---

### HasSubMenu

检查是否有子菜单。

```cpp
[[nodiscard]] bool HasSubMenu() const noexcept;
```

**返回值**: 有子菜单返回 `true`

---

## 交互状态

### IsHovered

检查鼠标是否悬停。

```cpp
[[nodiscard]] bool IsHovered() const noexcept;
```

**返回值**: 悬停返回 `true`

---

### IsPressed

检查是否被按下。

```cpp
[[nodiscard]] bool IsPressed() const noexcept;
```

**返回值**: 按下返回 `true`

---

## 事件处理

### SetOnClicked

设置点击回调函数。

```cpp
void SetOnClicked(ClickHandler handler);
```

**参数**:
- `handler` - 点击回调函数 `void(UIMenuItem&)`

**示例**:
```cpp
saveItem->SetOnClicked([](UIMenuItem&) {
    SaveCurrentFile();
});
```

---

### SetOnCheckChanged

设置选中状态改变回调。

```cpp
void SetOnCheckChanged(std::function<void(UIMenuItem&, bool)> handler);
```

**参数**:
- `handler` - 回调函数 `void(UIMenuItem&, bool checked)`

**示例**:
```cpp
gridItem->SetOnCheckChanged([](UIMenuItem&, bool checked) {
    SetGridVisibility(checked);
});
```

---

### Click

程序化触发点击。

```cpp
void Click();
```

**说明**: 
- 用于键盘导航等场景
- 会触发 `OnClicked` 回调
- 对于可选中项会切换选中状态

---

## 使用示例

### 创建普通菜单项

```cpp
auto saveItem = std::make_unique<UIMenuItem>("save", UIMenuItemType::Normal);
saveItem->SetLabel("Save");
saveItem->SetShortcut("Ctrl+S");
saveItem->SetTooltip("Save the current document");
saveItem->SetOnClicked([](UIMenuItem&) {
    SaveDocument();
});
```

### 创建可选中菜单项

```cpp
auto gridItem = std::make_unique<UIMenuItem>("grid", UIMenuItemType::Checkable);
gridItem->SetLabel("Show Grid");
gridItem->SetChecked(true);
gridItem->SetOnCheckChanged([](UIMenuItem&, bool checked) {
    LOG_INFO("Grid visibility: {}", checked ? "On" : "Off");
    SetGridVisibility(checked);
});
```

### 创建带子菜单的菜单项

```cpp
// 创建子菜单
auto recentMenu = std::make_shared<UIMenu>("recent_menu");
for (const auto& file : recentFiles) {
    recentMenu->AddMenuItem("recent_" + file, file);
}

// 创建菜单项并关联子菜单
auto recentItem = std::make_unique<UIMenuItem>("recent", UIMenuItemType::SubMenu);
recentItem->SetLabel("Recent Files");
recentItem->SetSubMenu(recentMenu);
```

### 创建分隔符

```cpp
auto separator = std::make_unique<UIMenuItem>("sep", UIMenuItemType::Separator);
// 分隔符不需要其他属性
```

---

## 渲染特性

菜单项的渲染由 `UIMenuRenderer` 处理：

- **文本**: 左侧显示标签
- **快捷键**: 右侧显示快捷键文本
- **勾选标记**: 可选中项显示 ✓ 标记
- **子菜单箭头**: 有子菜单显示 ▶ 箭头
- **分隔线**: 分隔符显示水平线

样式由主题系统控制：
- 正常状态: `theme.menu.normal`
- 悬停状态: `theme.menu.hover`
- 按下状态: `theme.menu.pressed`
- 禁用状态: `theme.menu.disabled`

---

## 注意事项

1. **类型切换**: 调用 `SetCheckable` 或 `SetSubMenu` 会自动切换类型
2. **分隔符**: 分隔符默认禁用，不响应交互事件
3. **子菜单**: 点击带子菜单的项会打开子菜单而不触发点击回调
4. **快捷键**: 快捷键文本仅用于显示，实际快捷键需在输入系统中注册

---

## 参见

- [UIMenu](UIMenu.md) - 菜单容器类
- [UIPullDownMenu](UIPullDownMenu.md) - 下拉菜单
- [UIWidget](UIWidget.md) - 基类文档
- [UI菜单系统文档](../ui/UI_MENU_SYSTEM.md)

---

[返回 API 首页](README.md)

