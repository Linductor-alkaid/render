# UIMenu API 参考

[返回 API 首页](README.md)

---

## 概述

`UIMenu` 是菜单容器类，继承自 `UIWidget`，用于管理和显示菜单项列表。支持嵌套子菜单、键盘导航和滚动。

**头文件**: `render/ui/widgets/ui_menu.h`  
**命名空间**: `Render::UI`

### 🎨 核心特性

- **菜单项管理**: 添加、删除、查找菜单项
- **多种菜单项类型**: 普通、可选中、分隔符、子菜单
- **键盘导航**: 方向键、回车、ESC键支持
- **滚动支持**: 长菜单自动启用滚动
- **垂直布局**: 自动垂直排列菜单项

---

## 类定义

```cpp
class UIMenu : public UIWidget {
public:
    explicit UIMenu(std::string id);

    // 菜单项管理
    UIMenuItem* AddMenuItem(std::string id, const std::string& label);
    UIMenuItem* AddCheckableItem(std::string id, const std::string& label, bool checked = false);
    UIMenuItem* AddSeparator(std::string id = "");
    UIMenuItem* AddSubMenuItem(std::string id, const std::string& label, std::shared_ptr<UIMenu> subMenu);
    
    void RemoveMenuItem(const std::string& id);
    void ClearMenuItems();

    [[nodiscard]] UIMenuItem* GetMenuItem(const std::string& id) const;
    [[nodiscard]] const std::vector<UIMenuItem*>& GetMenuItems() const noexcept;

    // 菜单显示控制
    void Open();
    void Close();
    [[nodiscard]] bool IsOpen() const noexcept;

    // 菜单尺寸
    void SetMinWidth(float width);
    [[nodiscard]] float GetMinWidth() const noexcept;

    void SetMaxHeight(float height);
    [[nodiscard]] float GetMaxHeight() const noexcept;

    // 滚动支持
    [[nodiscard]] bool NeedsScroll() const noexcept;
    void SetScrollOffset(float offset);
    [[nodiscard]] float GetScrollOffset() const noexcept;

    // 回调
    void SetOnOpened(std::function<void(UIMenu&)> handler);
    void SetOnClosed(std::function<void(UIMenu&)> handler);
};
```

---

## 构造函数

### UIMenu

创建菜单实例。

```cpp
explicit UIMenu(std::string id);
```

**参数**:
- `id` - 菜单的唯一标识符

**示例**:
```cpp
auto menu = std::make_shared<UIMenu>("file_menu");
```

---

## 菜单项管理

### AddMenuItem

添加普通菜单项。

```cpp
UIMenuItem* AddMenuItem(std::string id, const std::string& label);
```

**参数**:
- `id` - 菜单项ID
- `label` - 显示文本

**返回值**: 添加的菜单项指针

**示例**:
```cpp
auto newItem = menu->AddMenuItem("file_new", "New");
newItem->SetShortcut("Ctrl+N");
newItem->SetOnClicked([](UIMenuItem&) {
    CreateNewFile();
});
```

---

### AddCheckableItem

添加可选中菜单项。

```cpp
UIMenuItem* AddCheckableItem(std::string id, const std::string& label, bool checked = false);
```

**参数**:
- `id` - 菜单项ID
- `label` - 显示文本
- `checked` - 初始选中状态

**返回值**: 添加的菜单项指针

**示例**:
```cpp
auto gridItem = menu->AddCheckableItem("view_grid", "Show Grid", true);
gridItem->SetOnCheckChanged([](UIMenuItem&, bool checked) {
    SetGridVisibility(checked);
});
```

---

### AddSeparator

添加分隔符。

```cpp
UIMenuItem* AddSeparator(std::string id = "");
```

**参数**:
- `id` - 分隔符ID（可选，自动生成）

**返回值**: 添加的分隔符指针

**示例**:
```cpp
menu->AddSeparator();
```

---

### AddSubMenuItem

添加子菜单项。

```cpp
UIMenuItem* AddSubMenuItem(std::string id, const std::string& label, std::shared_ptr<UIMenu> subMenu);
```

**参数**:
- `id` - 菜单项ID
- `label` - 显示文本
- `subMenu` - 子菜单对象

**返回值**: 添加的菜单项指针

**示例**:
```cpp
auto recentMenu = std::make_shared<UIMenu>("recent_menu");
recentMenu->AddMenuItem("recent_1", "file1.txt");
recentMenu->AddMenuItem("recent_2", "file2.txt");

menu->AddSubMenuItem("recent", "Recent Files", recentMenu);
```

---

### RemoveMenuItem

移除菜单项。

```cpp
void RemoveMenuItem(const std::string& id);
```

**参数**:
- `id` - 要移除的菜单项ID

---

### ClearMenuItems

清空所有菜单项。

```cpp
void ClearMenuItems();
```

---

### GetMenuItem

获取菜单项。

```cpp
[[nodiscard]] UIMenuItem* GetMenuItem(const std::string& id) const;
```

**参数**:
- `id` - 菜单项ID

**返回值**: 菜单项指针，未找到返回 `nullptr`

---

### GetMenuItems

获取所有菜单项列表。

```cpp
[[nodiscard]] const std::vector<UIMenuItem*>& GetMenuItems() const noexcept;
```

**返回值**: 菜单项指针向量的常引用

---

## 显示控制

### Open

打开菜单。

```cpp
void Open();
```

**说明**: 将菜单设置为可见并触发 `OnOpened` 回调

---

### Close

关闭菜单。

```cpp
void Close();
```

**说明**: 隐藏菜单并关闭所有子菜单，触发 `OnClosed` 回调

---

### IsOpen

检查菜单是否打开。

```cpp
[[nodiscard]] bool IsOpen() const noexcept;
```

**返回值**: 打开返回 `true`

---

## 尺寸设置

### SetMinWidth

设置最小宽度。

```cpp
void SetMinWidth(float width);
```

**参数**:
- `width` - 最小宽度（像素）

**示例**:
```cpp
menu->SetMinWidth(200.0f);
```

---

### SetMaxHeight

设置最大高度。

```cpp
void SetMaxHeight(float height);
```

**参数**:
- `height` - 最大高度（像素）

**说明**: 超过此高度会启用滚动

---

## 滚动支持

### NeedsScroll

检查是否需要滚动。

```cpp
[[nodiscard]] bool NeedsScroll() const noexcept;
```

**返回值**: 菜单内容超过最大高度时返回 `true`

---

### SetScrollOffset

设置滚动偏移。

```cpp
void SetScrollOffset(float offset);
```

**参数**:
- `offset` - 滚动偏移量（像素）

---

## 回调函数

### SetOnOpened

设置菜单打开回调。

```cpp
void SetOnOpened(std::function<void(UIMenu&)> handler);
```

**参数**:
- `handler` - 打开回调函数

---

### SetOnClosed

设置菜单关闭回调。

```cpp
void SetOnClosed(std::function<void(UIMenu&)> handler);
```

**参数**:
- `handler` - 关闭回调函数

---

## 键盘导航

菜单自动支持以下键盘操作：

- **↑ / ↓**: 在菜单项间导航
- **Enter**: 激活当前选中的菜单项
- **→**: 打开子菜单
- **← / ESC**: 关闭菜单

---

## 使用示例

### 创建基本菜单

```cpp
auto menu = std::make_shared<UIMenu>("file_menu");
menu->SetMinWidth(200.0f);
menu->SetMaxHeight(400.0f);

// 添加普通菜单项
auto newItem = menu->AddMenuItem("new", "New");
newItem->SetShortcut("Ctrl+N");
newItem->SetOnClicked([](UIMenuItem&) {
    CreateNewDocument();
});

auto openItem = menu->AddMenuItem("open", "Open...");
openItem->SetShortcut("Ctrl+O");

// 添加分隔符
menu->AddSeparator();

// 添加保存菜单项
auto saveItem = menu->AddMenuItem("save", "Save");
saveItem->SetShortcut("Ctrl+S");
```

### 创建带子菜单的菜单

```cpp
// 创建主菜单
auto mainMenu = std::make_shared<UIMenu>("main_menu");

// 创建子菜单
auto exportMenu = std::make_shared<UIMenu>("export_menu");
exportMenu->AddMenuItem("export_png", "PNG Image");
exportMenu->AddMenuItem("export_jpg", "JPEG Image");
exportMenu->AddMenuItem("export_pdf", "PDF Document");

// 添加子菜单项
mainMenu->AddSubMenuItem("export", "Export As", exportMenu);
```

### 创建带可选中项的视图菜单

```cpp
auto viewMenu = std::make_shared<UIMenu>("view_menu");

auto gridItem = viewMenu->AddCheckableItem("grid", "Show Grid", true);
gridItem->SetOnCheckChanged([](UIMenuItem&, bool checked) {
    SetGridVisibility(checked);
});

auto axisItem = viewMenu->AddCheckableItem("axis", "Show Axis", true);
axisItem->SetOnCheckChanged([](UIMenuItem&, bool checked) {
    SetAxisVisibility(checked);
});

viewMenu->AddSeparator();

auto wireframeItem = viewMenu->AddCheckableItem("wireframe", "Wireframe Mode", false);
```

### 动态更新菜单

```cpp
// 清空并重新填充菜单
menu->ClearMenuItems();

for (const auto& file : recentFiles) {
    auto item = menu->AddMenuItem("recent_" + file, file);
    item->SetOnClicked([file](UIMenuItem&) {
        OpenFile(file);
    });
}
```

---

## 参见

- [UIMenuItem](UIMenuItem.md) - 菜单项类
- [UIPullDownMenu](UIPullDownMenu.md) - 下拉菜单
- [UIWidget](UIWidget.md) - 基类文档
- [UI菜单系统文档](../ui/UI_MENU_SYSTEM.md)

---

[返回 API 首页](README.md)

