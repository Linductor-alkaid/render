# UI 菜单系统文档

[返回文档首页](../README.md)

---

## 概述

本文档介绍项目的 UI 菜单系统实现，该系统参考 Blender 的菜单架构设计，提供了完整的菜单功能支持。

**实现时间**: 2025-11-25  
**参考来源**: Blender UI 菜单系统 (`interface_widgets.cc`, `interface_region_menu_pie.cc`)

---

## 核心组件

### 1. UIMenuItem（菜单项）

菜单项是菜单中的基本单元，支持多种类型和状态。

#### 菜单项类型

```cpp
enum class UIMenuItemType {
    Normal,      // 普通菜单项
    Checkable,   // 可选中菜单项
    Separator,   // 分隔符
    SubMenu      // 子菜单
};
```

#### 关键特性

- **文本和标签**: 显示菜单项的名称
- **图标支持**: 可选的图标显示（预留接口）
- **快捷键显示**: 在菜单项右侧显示快捷键
- **工具提示**: 悬停时显示提示信息
- **状态管理**: 支持选中/未选中、启用/禁用、悬停/按下状态
- **子菜单**: 支持嵌套子菜单

#### 使用示例

```cpp
// 创建普通菜单项
auto menuItem = std::make_unique<UIMenuItem>("item1", UIMenuItemType::Normal);
menuItem->SetLabel("Open File");
menuItem->SetShortcut("Ctrl+O");
menuItem->SetOnClicked([](UIMenuItem&) {
    LOG_INFO("Open File clicked");
});

// 创建可选中菜单项
auto checkItem = std::make_unique<UIMenuItem>("check1", UIMenuItemType::Checkable);
checkItem->SetLabel("Show Grid");
checkItem->SetChecked(true);
checkItem->SetOnCheckChanged([](UIMenuItem&, bool checked) {
    LOG_INFO("Grid: {}", checked ? "On" : "Off");
});
```

### 2. UIMenu（菜单容器）

UIMenu 是菜单项的容器，负责管理和布局菜单项。

#### 关键特性

- **菜单项管理**: 添加、删除、查找菜单项
- **布局支持**: 自动垂直布局菜单项
- **滚动支持**: 当菜单项过多时自动启用滚动
- **键盘导航**: 支持方向键和回车键导航
- **子菜单**: 支持嵌套子菜单结构

#### 使用示例

```cpp
auto menu = std::make_shared<UIMenu>("file_menu");
menu->SetMinWidth(200.0f);
menu->SetMaxHeight(400.0f);

// 添加普通菜单项
auto newItem = menu->AddMenuItem("new", "New");
newItem->SetShortcut("Ctrl+N");

// 添加可选中项
auto gridItem = menu->AddCheckableItem("grid", "Show Grid", true);

// 添加分隔符
menu->AddSeparator();

// 添加子菜单
auto recentMenu = std::make_shared<UIMenu>("recent_menu");
recentMenu->AddMenuItem("recent1", "file1.txt");
recentMenu->AddMenuItem("recent2", "file2.txt");
menu->AddSubMenuItem("recent", "Recent Files", recentMenu);
```

### 3. UIPullDownMenu（下拉菜单）

UIPullDownMenu 是带触发按钮的下拉菜单组件。

#### 菜单定位

```cpp
enum class UIMenuPlacement {
    Below,      // 在触发器下方
    Above,      // 在触发器上方
    Left,       // 在触发器左侧
    Right,      // 在触发器右侧
    Auto        // 自动选择（根据空间）
};
```

#### 使用示例

```cpp
auto pulldown = std::make_unique<UIPullDownMenu>("file_pulldown");
pulldown->SetLabel("File");
pulldown->SetPlacement(UIMenuPlacement::Below);

auto menu = std::make_shared<UIMenu>("file_menu");
// ... 添加菜单项 ...

pulldown->SetMenu(menu);

// 菜单打开/关闭回调
pulldown->SetOnMenuOpened([](UIPullDownMenu&) {
    LOG_INFO("Menu opened");
});
```

---

## 菜单渲染系统

### UIMenuRenderer

菜单渲染器负责将菜单组件转换为渲染命令。

#### 渲染功能

- **菜单背景**: 圆角矩形背景和边框
- **菜单项背景**: 根据状态变化的背景色
- **文本渲染**: 菜单项文本和快捷键
- **图标渲染**: 菜单项图标（预留接口）
- **勾选标记**: 可选中项的勾选标记
- **子菜单箭头**: 指示有子菜单的箭头
- **分隔符**: 水平分隔线

#### 主题集成

菜单系统完全集成到 UITheme 主题系统，支持：

- 菜单背景色和边框色
- 不同状态的菜单项颜色（正常、悬停、按下、禁用）
- 字体大小和样式
- 圆角半径和边框宽度

---

## 输入处理

### 鼠标交互

- **点击**: 触发菜单项动作或打开子菜单
- **悬停**: 高亮菜单项
- **滚轮**: 滚动长菜单

### 键盘导航

- **方向键上/下**: 在菜单项间导航
- **回车键**: 激活当前选中的菜单项
- **方向键右**: 打开子菜单
- **方向键左/ESC**: 关闭菜单

---

## 示例代码

完整的菜单系统示例位于 `examples/61_ui_menu_example.cpp`，演示了：

1. 文件菜单（File Menu）
   - 新建、打开、保存等常规操作
   - 最近文件子菜单
   - 快捷键显示

2. 编辑菜单（Edit Menu）
   - 撤销/重做
   - 复制/粘贴操作

3. 视图菜单（View Menu）
   - 可选中的视图选项
   - 相机子菜单

---

## 集成到项目

### 1. 添加菜单到 UI

```cpp
// 创建下拉菜单
auto pulldown = std::make_unique<UIPullDownMenu>("my_menu");
pulldown->SetLabel("Menu");

auto menu = std::make_shared<UIMenu>("menu_content");
// 添加菜单项...
pulldown->SetMenu(menu);

// 添加到父容器
parentWidget->AddChild(std::move(pulldown));
```

### 2. 处理菜单事件

```cpp
menuItem->SetOnClicked([this](UIMenuItem& item) {
    // 处理点击事件
    HandleMenuAction(item.GetId());
});
```

### 3. 动态更新菜单

```cpp
// 添加菜单项
menu->AddMenuItem("new_item", "New Item");

// 删除菜单项
menu->RemoveMenuItem("old_item");

// 清空菜单
menu->ClearMenuItems();
```

---

## 技术特点

### 1. 参考 Blender 设计

- **布局系统**: 使用 Flex 布局自动排列菜单项
- **状态管理**: 完整的交互状态支持
- **主题系统**: 可配置的外观和样式

### 2. 性能优化

- **命令缓冲**: 使用渲染命令缓冲减少 API 调用
- **懒加载**: 菜单仅在打开时才渲染
- **虚拟化**: 支持长菜单的滚动

### 3. 可扩展性

- **插件化设计**: 易于添加新的菜单项类型
- **自定义渲染**: 可重写渲染方法
- **事件系统**: 灵活的回调机制

---

## 已知限制与未来改进

### 当前限制

1. **图标支持**: 图标渲染接口已预留，但尚未完整实现
2. **菜单定位**: 自动定位（Auto placement）目前默认为下方，需要实现智能定位
3. **动画效果**: 菜单打开/关闭暂无动画过渡
4. **径向菜单**: Pie Menu（径向菜单）尚未实现

### 计划改进

1. **完整图标系统**: 支持自定义图标纹理
2. **智能定位**: 根据屏幕边界自动调整菜单位置
3. **动画过渡**: 添加淡入淡出和滑动动画
4. **径向菜单**: 实现 Blender 风格的 Pie Menu
5. **上下文菜单**: 右键菜单支持
6. **菜单栏**: 顶部菜单栏组件

---

## 相关文档

- [UI_SYSTEM_BLENDER_REFERENCE_UPDATE_PLAN.md](../application/UI_SYSTEM_BLENDER_REFERENCE_UPDATE_PLAN.md) - UI 系统整体规划
- [UI_FRAMEWORK_FOUNDATION_PLAN.md](../guides/UI_FRAMEWORK_FOUNDATION_PLAN.md) - UI 框架基础
- [UI_COLOR_PICKER_USAGE.md](UI_COLOR_PICKER_USAGE.md) - 颜色选择器文档

---

[返回文档首页](../README.md)

