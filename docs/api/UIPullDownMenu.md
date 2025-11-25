# UIPullDownMenu API 参考

[返回 API 首页](README.md)

---

## 概述

`UIPullDownMenu` 是下拉菜单类，继承自 `UIWidget`，结合触发按钮和菜单内容，提供完整的下拉菜单功能。

**头文件**: `render/ui/widgets/ui_pulldown_menu.h`  
**命名空间**: `Render::UI`

**参考**: 基于 Blender 的 `UI_WTYPE_PULLDOWN` 设计

### 🎨 核心特性

- **触发按钮**: 内置按钮控件
- **菜单定位**: 支持上、下、左、右、自动定位
- **事件管理**: 打开/关闭回调
- **自动布局**: 菜单位置自动计算

---

## 类定义

```cpp
class UIPullDownMenu : public UIWidget {
public:
    explicit UIPullDownMenu(std::string id);

    // 触发按钮
    void SetLabel(const std::string& label);
    [[nodiscard]] const std::string& GetLabel() const;

    void SetIcon(const std::string& iconPath);
    [[nodiscard]] const std::string& GetIcon() const;

    [[nodiscard]] UIButton* GetButton() const noexcept;

    // 菜单内容
    void SetMenu(std::shared_ptr<UIMenu> menu);
    [[nodiscard]] std::shared_ptr<UIMenu> GetMenu() const noexcept;

    // 定位设置
    void SetPlacement(UIMenuPlacement placement);
    [[nodiscard]] UIMenuPlacement GetPlacement() const noexcept;

    // 菜单状态
    void OpenMenu();
    void CloseMenu();
    [[nodiscard]] bool IsMenuOpen() const;

    // 回调
    void SetOnMenuOpened(std::function<void(UIPullDownMenu&)> handler);
    void SetOnMenuClosed(std::function<void(UIPullDownMenu&)> handler);
};
```

---

## 菜单定位

```cpp
enum class UIMenuPlacement : uint8_t {
    Below,      // 在触发器下方
    Above,      // 在触发器上方
    Left,       // 在触发器左侧
    Right,      // 在触发器右侧
    Auto        // 自动选择（根据空间）
};
```

---

## 构造函数

### UIPullDownMenu

创建下拉菜单实例。

```cpp
explicit UIPullDownMenu(std::string id);
```

**参数**:
- `id` - 下拉菜单的唯一标识符

**说明**: 自动创建内置按钮

**示例**:
```cpp
auto pulldown = std::make_unique<UIPullDownMenu>("file_pulldown");
```

---

## 触发按钮

### SetLabel

设置按钮文本。

```cpp
void SetLabel(const std::string& label);
```

**参数**:
- `label` - 按钮文本

**示例**:
```cpp
pulldown->SetLabel("File");
```

---

### GetLabel

获取按钮文本。

```cpp
[[nodiscard]] const std::string& GetLabel() const;
```

**返回值**: 按钮文本字符串引用

---

### SetIcon

设置按钮图标（预留接口）。

```cpp
void SetIcon(const std::string& iconPath);
```

**参数**:
- `iconPath` - 图标路径

---

### GetButton

获取内置按钮指针。

```cpp
[[nodiscard]] UIButton* GetButton() const noexcept;
```

**返回值**: 按钮指针

**示例**:
```cpp
auto* button = pulldown->GetButton();
button->SetPreferredSize({100.0f, 30.0f});
```

---

## 菜单内容

### SetMenu

设置菜单内容。

```cpp
void SetMenu(std::shared_ptr<UIMenu> menu);
```

**参数**:
- `menu` - 菜单对象

**示例**:
```cpp
auto menu = std::make_shared<UIMenu>("file_menu");
menu->AddMenuItem("new", "New");
menu->AddMenuItem("open", "Open...");
menu->AddSeparator();
menu->AddMenuItem("save", "Save");

pulldown->SetMenu(menu);
```

---

### GetMenu

获取菜单对象。

```cpp
[[nodiscard]] std::shared_ptr<UIMenu> GetMenu() const noexcept;
```

**返回值**: 菜单对象指针

---

## 定位设置

### SetPlacement

设置菜单定位方式。

```cpp
void SetPlacement(UIMenuPlacement placement);
```

**参数**:
- `placement` - 定位方式（见 [UIMenuPlacement](#菜单定位)）

**示例**:
```cpp
pulldown->SetPlacement(UIMenuPlacement::Below);
```

---

### GetPlacement

获取菜单定位方式。

```cpp
[[nodiscard]] UIMenuPlacement GetPlacement() const noexcept;
```

**返回值**: 定位方式枚举值

---

## 菜单状态

### OpenMenu

打开菜单。

```cpp
void OpenMenu();
```

**说明**: 
- 更新菜单位置
- 显示菜单
- 触发 `OnMenuOpened` 回调

---

### CloseMenu

关闭菜单。

```cpp
void CloseMenu();
```

**说明**: 触发 `OnMenuClosed` 回调

---

### IsMenuOpen

检查菜单是否打开。

```cpp
[[nodiscard]] bool IsMenuOpen() const;
```

**返回值**: 打开返回 `true`

---

## 事件回调

### SetOnMenuOpened

设置菜单打开回调。

```cpp
void SetOnMenuOpened(std::function<void(UIPullDownMenu&)> handler);
```

**参数**:
- `handler` - 打开回调函数

**示例**:
```cpp
pulldown->SetOnMenuOpened([](UIPullDownMenu& pd) {
    LOG_INFO("Menu {} opened", pd.GetId());
});
```

---

### SetOnMenuClosed

设置菜单关闭回调。

```cpp
void SetOnMenuClosed(std::function<void(UIPullDownMenu&)> handler);
```

**参数**:
- `handler` - 关闭回调函数

---

## 使用示例

### 创建基本下拉菜单

```cpp
// 创建下拉菜单
auto pulldown = std::make_unique<UIPullDownMenu>("file_pulldown");
pulldown->SetLabel("File");
pulldown->SetPlacement(UIMenuPlacement::Below);

// 创建菜单内容
auto menu = std::make_shared<UIMenu>("file_menu");
menu->SetMinWidth(200.0f);

auto newItem = menu->AddMenuItem("new", "New");
newItem->SetShortcut("Ctrl+N");
newItem->SetOnClicked([](UIMenuItem&) {
    CreateNewFile();
});

auto openItem = menu->AddMenuItem("open", "Open...");
openItem->SetShortcut("Ctrl+O");

menu->AddSeparator();

auto saveItem = menu->AddMenuItem("save", "Save");
saveItem->SetShortcut("Ctrl+S");

// 关联菜单
pulldown->SetMenu(menu);

// 添加到容器
container->AddChild(std::move(pulldown));
```

### 创建菜单栏

```cpp
// 创建水平容器作为菜单栏
auto menuBar = std::make_unique<UIWidget>("menu_bar");
menuBar->SetLayoutDirection(UILayoutDirection::Horizontal);
menuBar->SetSpacing(5.0f);

// 文件菜单
auto fileMenu = CreateFileMenu();
menuBar->AddChild(std::move(fileMenu));

// 编辑菜单
auto editMenu = CreateEditMenu();
menuBar->AddChild(std::move(editMenu));

// 视图菜单
auto viewMenu = CreateViewMenu();
menuBar->AddChild(std::move(viewMenu));
```

### 带事件处理的下拉菜单

```cpp
auto pulldown = std::make_unique<UIPullDownMenu>("tools_pulldown");
pulldown->SetLabel("Tools");

pulldown->SetOnMenuOpened([](UIPullDownMenu&) {
    LOG_INFO("Tools menu opened");
    // 可以在这里动态更新菜单内容
});

pulldown->SetOnMenuClosed([](UIPullDownMenu&) {
    LOG_INFO("Tools menu closed");
});

auto menu = std::make_shared<UIMenu>("tools_menu");
// ... 添加菜单项 ...
pulldown->SetMenu(menu);
```

### 自定义按钮样式

```cpp
auto pulldown = std::make_unique<UIPullDownMenu>("custom_pulldown");
pulldown->SetLabel("Options");

// 获取并自定义按钮
auto* button = pulldown->GetButton();
button->SetPreferredSize({150.0f, 35.0f});
// 按钮的其他属性通过主题系统控制
```

---

## 菜单定位详解

### Below（下方）

```cpp
pulldown->SetPlacement(UIMenuPlacement::Below);
```

菜单显示在按钮正下方，左边对齐。菜单宽度至少与按钮相同。

### Above（上方）

```cpp
pulldown->SetPlacement(UIMenuPlacement::Above);
```

菜单显示在按钮正上方。

### Left（左侧）

```cpp
pulldown->SetPlacement(UIMenuPlacement::Left);
```

菜单显示在按钮左侧。

### Right（右侧）

```cpp
pulldown->SetPlacement(UIMenuPlacement::Right);
```

菜单显示在按钮右侧。

### Auto（自动）

```cpp
pulldown->SetPlacement(UIMenuPlacement::Auto);
```

根据可用空间自动选择最佳位置（当前默认为下方，未来会实现智能定位）。

---

## 注意事项

1. **按钮点击**: 点击按钮会自动打开/关闭菜单
2. **菜单位置**: 菜单位置在打开时自动计算
3. **子菜单**: 菜单项的子菜单会独立打开
4. **焦点管理**: 点击菜单外区域会自动关闭菜单（需输入系统支持）

---

## 未来改进

- ✅ 智能定位（Auto模式）
- ⏳ 动画过渡效果
- ⏳ 键盘快捷键导航
- ⏳ 触屏手势支持

---

## 参见

- [UIMenu](UIMenu.md) - 菜单类
- [UIMenuItem](UIMenuItem.md) - 菜单项类
- [UIButton](UIButton.md) - 按钮类
- [UIWidget](UIWidget.md) - 基类文档
- [UI菜单系统文档](../ui/UI_MENU_SYSTEM.md)

---

[返回 API 首页](README.md)

