# UIWidget API 参考

[返回 API 首页](README.md)

---

## 概述

`UIWidget` 是所有UI控件的基类，提供布局、事件处理、可见性和状态管理等基础功能。

**头文件**: `render/ui/ui_widget.h`  
**命名空间**: `Render::UI`

### 🎨 核心特性

- **层次结构**: 支持父子关系的树形结构
- **布局系统**: 支持Flex和Grid两种布局模式
- **事件处理**: 鼠标、键盘、焦点等交互事件
- **状态管理**: 可见性、启用/禁用、脏标记
- **样式属性**: 尺寸、内边距、对齐方式等

---

## 类定义

```cpp
class UIWidget {
public:
    explicit UIWidget(std::string id);
    virtual ~UIWidget();

    // 基本属性
    [[nodiscard]] const std::string& GetId() const noexcept;
    [[nodiscard]] UIWidget* GetParent() noexcept;
    [[nodiscard]] bool IsRoot() const noexcept;

    // 可见性
    void SetVisibility(UIVisibility visibility);
    [[nodiscard]] UIVisibility GetVisibility() const noexcept;
    [[nodiscard]] bool IsVisible() const noexcept;

    // 启用状态
    void SetEnabled(bool enabled);
    [[nodiscard]] bool IsEnabled() const noexcept;

    // 布局
    void SetLayoutRect(const Rect& rect) noexcept;
    [[nodiscard]] const Rect& GetLayoutRect() const noexcept;
    void SetPreferredSize(const Vector2& size) noexcept;
    [[nodiscard]] Vector2 GetPreferredSize() const noexcept;
    void SetMinSize(const Vector2& size) noexcept;
    [[nodiscard]] Vector2 GetMinSize() const noexcept;
    void SetPadding(const Vector4& padding) noexcept;
    [[nodiscard]] Vector4 GetPadding() const noexcept;

    // Flex 布局属性
    void SetLayoutDirection(UILayoutDirection direction) noexcept;
    void SetJustifyContent(UIFlexJustifyContent justifyContent) noexcept;
    void SetAlignItems(UIFlexAlignItems alignItems) noexcept;
    void SetAlignSelf(UIFlexAlignSelf alignSelf) noexcept;
    void SetFlexGrow(float flexGrow) noexcept;
    void SetFlexShrink(float flexShrink) noexcept;
    void SetSpacing(float spacing) noexcept;

    // Grid 布局属性
    void SetLayoutMode(UILayoutMode mode) noexcept;
    void SetGridColumns(int columns) noexcept;
    void SetGridRows(int rows) noexcept;
    void SetGridCellSpacing(const Vector2& spacing) noexcept;

    // 子节点管理
    UIWidget* AddChild(std::unique_ptr<UIWidget> child);
    std::unique_ptr<UIWidget> RemoveChild(std::string_view id);
    UIWidget* FindById(std::string_view id) noexcept;

    // 脏标记
    void MarkDirty(UIWidgetDirtyFlag flags = UIWidgetDirtyFlag::All) noexcept;
    void ClearDirty(UIWidgetDirtyFlag flags = UIWidgetDirtyFlag::All) noexcept;
    [[nodiscard]] UIWidgetDirtyFlag GetDirtyFlags() const noexcept;
};
```

---

## 构造和析构

### UIWidget

创建UI控件实例。

```cpp
explicit UIWidget(std::string id);
```

**参数**:
- `id` - 控件的唯一标识符

**示例**:
```cpp
auto widget = std::make_unique<UIWidget>("my_widget");
```

---

## 基本属性

### GetId

获取控件ID。

```cpp
[[nodiscard]] const std::string& GetId() const noexcept;
```

**返回值**: 控件ID字符串引用

---

### GetParent

获取父控件指针。

```cpp
[[nodiscard]] UIWidget* GetParent() noexcept;
```

**返回值**: 父控件指针，根节点返回 `nullptr`

---

### IsRoot

检查是否为根节点。

```cpp
[[nodiscard]] bool IsRoot() const noexcept;
```

**返回值**: 如果是根节点返回 `true`

---

## 可见性管理

### SetVisibility

设置控件可见性。

```cpp
void SetVisibility(UIVisibility visibility);
```

**参数**:
- `visibility` - 可见性状态
  - `UIVisibility::Visible` - 可见
  - `UIVisibility::Hidden` - 隐藏（占用空间）
  - `UIVisibility::Collapsed` - 折叠（不占用空间）

**示例**:
```cpp
widget->SetVisibility(UIVisibility::Hidden);
```

---

### IsVisible

检查控件是否可见。

```cpp
[[nodiscard]] bool IsVisible() const noexcept;
```

**返回值**: 可见返回 `true`

---

## 启用状态

### SetEnabled

设置控件启用状态。

```cpp
void SetEnabled(bool enabled);
```

**参数**:
- `enabled` - `true` 启用，`false` 禁用

**示例**:
```cpp
button->SetEnabled(false); // 禁用按钮
```

---

### IsEnabled

检查控件是否启用。

```cpp
[[nodiscard]] bool IsEnabled() const noexcept;
```

**返回值**: 启用返回 `true`

---

## 布局属性

### SetLayoutRect

设置控件的布局矩形（由布局系统调用）。

```cpp
void SetLayoutRect(const Rect& rect) noexcept;
```

**参数**:
- `rect` - 布局矩形 (x, y, width, height)

---

### GetLayoutRect

获取控件的布局矩形。

```cpp
[[nodiscard]] const Rect& GetLayoutRect() const noexcept;
```

**返回值**: 布局矩形引用

---

### SetPreferredSize

设置首选尺寸。

```cpp
void SetPreferredSize(const Vector2& size) noexcept;
```

**参数**:
- `size` - 首选尺寸 (width, height)

**示例**:
```cpp
widget->SetPreferredSize({200.0f, 50.0f});
```

---

### SetPadding

设置内边距。

```cpp
void SetPadding(const Vector4& padding) noexcept;
```

**参数**:
- `padding` - 内边距 (left, top, right, bottom)

**示例**:
```cpp
widget->SetPadding({10.0f, 10.0f, 10.0f, 10.0f});
```

---

## Flex 布局

### SetLayoutDirection

设置布局方向。

```cpp
void SetLayoutDirection(UILayoutDirection direction) noexcept;
```

**参数**:
- `direction` - 布局方向
  - `UILayoutDirection::Horizontal` - 水平
  - `UILayoutDirection::Vertical` - 垂直

**示例**:
```cpp
container->SetLayoutDirection(UILayoutDirection::Vertical);
```

---

### SetJustifyContent

设置主轴对齐方式。

```cpp
void SetJustifyContent(UIFlexJustifyContent justifyContent) noexcept;
```

**参数**:
- `justifyContent` - 对齐方式
  - `FlexStart` - 起始对齐
  - `FlexEnd` - 结束对齐
  - `Center` - 居中对齐
  - `SpaceBetween` - 两端对齐
  - `SpaceAround` - 周围间距
  - `SpaceEvenly` - 均匀分布

---

### SetAlignItems

设置交叉轴对齐方式。

```cpp
void SetAlignItems(UIFlexAlignItems alignItems) noexcept;
```

**参数**:
- `alignItems` - 对齐方式
  - `FlexStart` - 起始对齐
  - `FlexEnd` - 结束对齐
  - `Center` - 居中对齐
  - `Stretch` - 拉伸填充

---

### SetFlexGrow

设置伸展因子。

```cpp
void SetFlexGrow(float flexGrow) noexcept;
```

**参数**:
- `flexGrow` - 伸展因子，0表示不伸展

**示例**:
```cpp
widget->SetFlexGrow(1.0f); // 占用剩余空间
```

---

### SetSpacing

设置子元素间距。

```cpp
void SetSpacing(float spacing) noexcept;
```

**参数**:
- `spacing` - 间距（像素）

---

## Grid 布局

### SetLayoutMode

设置布局模式。

```cpp
void SetLayoutMode(UILayoutMode mode) noexcept;
```

**参数**:
- `mode` - 布局模式
  - `UILayoutMode::Flex` - Flex 布局
  - `UILayoutMode::Grid` - Grid 布局
  - `UILayoutMode::Absolute` - 绝对定位

**示例**:
```cpp
container->SetLayoutMode(UILayoutMode::Grid);
container->SetGridColumns(3);
```

---

### SetGridColumns

设置网格列数。

```cpp
void SetGridColumns(int columns) noexcept;
```

**参数**:
- `columns` - 列数

---

### SetGridRows

设置网格行数。

```cpp
void SetGridRows(int rows) noexcept;
```

**参数**:
- `rows` - 行数（0表示自动计算）

---

## 子节点管理

### AddChild

添加子控件。

```cpp
UIWidget* AddChild(std::unique_ptr<UIWidget> child);
```

**参数**:
- `child` - 子控件的智能指针

**返回值**: 添加的子控件的原始指针

**示例**:
```cpp
auto button = std::make_unique<UIButton>("btn1");
auto* btnPtr = container->AddChild(std::move(button));
```

---

### RemoveChild

移除子控件。

```cpp
std::unique_ptr<UIWidget> RemoveChild(std::string_view id);
```

**参数**:
- `id` - 子控件ID

**返回值**: 被移除的子控件的智能指针

---

### FindById

查找子控件（递归搜索）。

```cpp
UIWidget* FindById(std::string_view id) noexcept;
```

**参数**:
- `id` - 控件ID

**返回值**: 找到的控件指针，未找到返回 `nullptr`

---

## 脏标记

### MarkDirty

标记控件需要更新。

```cpp
void MarkDirty(UIWidgetDirtyFlag flags = UIWidgetDirtyFlag::All) noexcept;
```

**参数**:
- `flags` - 脏标记类型
  - `UIWidgetDirtyFlag::Layout` - 布局需要更新
  - `UIWidgetDirtyFlag::Visual` - 视觉需要更新
  - `UIWidgetDirtyFlag::Children` - 子节点需要更新
  - `UIWidgetDirtyFlag::All` - 全部更新

---

## 事件回调（Protected）

可在派生类中重写以处理事件：

```cpp
virtual void OnMouseEnter();
virtual void OnMouseLeave();
virtual void OnMouseMove(const Vector2& position, const Vector2& delta);
virtual void OnMouseButton(uint8_t button, bool pressed, const Vector2& position);
virtual void OnMouseClick(uint8_t button, const Vector2& position);
virtual void OnMouseWheel(const Vector2& offset);
virtual void OnKey(int scancode, bool pressed, bool repeat);
virtual void OnTextInput(const std::string& text);
virtual void OnFocusGained();
virtual void OnFocusLost();
```

---

## 使用示例

### 创建基本容器

```cpp
// 创建垂直容器
auto container = std::make_unique<UIWidget>("container");
container->SetLayoutDirection(UILayoutDirection::Vertical);
container->SetJustifyContent(UIFlexJustifyContent::FlexStart);
container->SetAlignItems(UIFlexAlignItems::Stretch);
container->SetSpacing(10.0f);
container->SetPadding({20.0f, 20.0f, 20.0f, 20.0f});

// 添加子控件
auto button1 = std::make_unique<UIButton>("btn1");
button1->SetLabel("Button 1");
container->AddChild(std::move(button1));

auto button2 = std::make_unique<UIButton>("btn2");
button2->SetLabel("Button 2");
container->AddChild(std::move(button2));
```

### 创建Grid布局

```cpp
auto grid = std::make_unique<UIWidget>("grid");
grid->SetLayoutMode(UILayoutMode::Grid);
grid->SetGridColumns(3);
grid->SetGridCellSpacing({5.0f, 5.0f});

for (int i = 0; i < 9; ++i) {
    auto cell = std::make_unique<UIWidget>("cell_" + std::to_string(i));
    cell->SetPreferredSize({100.0f, 100.0f});
    grid->AddChild(std::move(cell));
}
```

---

## 相关类型

### UIVisibility

```cpp
enum class UIVisibility : uint8_t {
    Visible,    // 可见
    Hidden,     // 隐藏（占用空间）
    Collapsed   // 折叠（不占用空间）
};
```

### UIWidgetDirtyFlag

```cpp
enum class UIWidgetDirtyFlag : uint32_t {
    None = 0,
    Layout = 1u << 0,
    Visual = 1u << 1,
    Children = 1u << 2,
    All = 0xFFFFFFFFu
};
```

---

## 参见

- [UIButton](UIButton.md) - 按钮控件
- [UITextField](UITextField.md) - 文本输入框
- [UIMenu](UIMenu.md) - 菜单系统
- [UI布局系统文档](../guides/UI_FRAMEWORK_FOUNDATION_PLAN.md)

---

[返回 API 首页](README.md)

