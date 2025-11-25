# UIButton API 参考

[返回 API 首页](README.md)

---

## 概述

`UIButton` 是按钮控件类，继承自 `UIWidget`，提供点击交互和标签显示功能。

**头文件**: `render/ui/widgets/ui_button.h`  
**命名空间**: `Render::UI`

### 🎨 核心特性

- **文本标签**: 显示按钮文本
- **交互状态**: 支持悬停、按下状态
- **点击事件**: 回调函数支持
- **主题样式**: 自动应用主题颜色

---

## 类定义

```cpp
class UIButton : public UIWidget {
public:
    using ClickHandler = std::function<void(UIButton&)>;

    explicit UIButton(std::string id);

    void SetLabel(std::string label);
    [[nodiscard]] const std::string& GetLabel() const noexcept;

    void SetOnClicked(ClickHandler handler);

    [[nodiscard]] bool IsHovered() const noexcept;
    [[nodiscard]] bool IsPressed() const noexcept;
};
```

---

## 构造函数

### UIButton

创建按钮实例。

```cpp
explicit UIButton(std::string id);
```

**参数**:
- `id` - 按钮的唯一标识符

**示例**:
```cpp
auto button = std::make_unique<UIButton>("my_button");
```

---

## 标签管理

### SetLabel

设置按钮文本。

```cpp
void SetLabel(std::string label);
```

**参数**:
- `label` - 按钮文本

**示例**:
```cpp
button->SetLabel("Click Me");
```

---

### GetLabel

获取按钮文本。

```cpp
[[nodiscard]] const std::string& GetLabel() const noexcept;
```

**返回值**: 按钮文本字符串引用

---

## 事件处理

### SetOnClicked

设置点击回调函数。

```cpp
void SetOnClicked(ClickHandler handler);
```

**参数**:
- `handler` - 点击回调函数 `void(UIButton&)`

**示例**:
```cpp
button->SetOnClicked([](UIButton& btn) {
    LOG_INFO("Button {} clicked", btn.GetId());
});
```

---

## 状态查询

### IsHovered

检查鼠标是否悬停在按钮上。

```cpp
[[nodiscard]] bool IsHovered() const noexcept;
```

**返回值**: 悬停返回 `true`

---

### IsPressed

检查按钮是否被按下。

```cpp
[[nodiscard]] bool IsPressed() const noexcept;
```

**返回值**: 按下返回 `true`

---

## 使用示例

### 基本按钮

```cpp
// 创建按钮
auto button = std::make_unique<UIButton>("submit_btn");
button->SetLabel("Submit");
button->SetPreferredSize({120.0f, 40.0f});

// 设置点击事件
button->SetOnClicked([](UIButton&) {
    LOG_INFO("Form submitted");
});

// 添加到容器
container->AddChild(std::move(button));
```

### 带状态检查的按钮

```cpp
button->SetOnClicked([](UIButton& btn) {
    if (btn.IsEnabled()) {
        // 处理点击
        ProcessClick();
        
        // 临时禁用
        btn.SetEnabled(false);
    }
});
```

### 动态更新标签

```cpp
int clickCount = 0;
button->SetOnClicked([&clickCount](UIButton& btn) {
    clickCount++;
    btn.SetLabel("Clicked: " + std::to_string(clickCount));
});
```

---

## 继承的属性

从 `UIWidget` 继承的所有属性和方法均可使用：

- 布局属性（尺寸、对齐、间距等）
- 可见性和启用状态
- 子节点管理（虽然按钮通常不包含子节点）

**示例**:
```cpp
button->SetPreferredSize({100.0f, 30.0f});
button->SetEnabled(false);
button->SetVisibility(UIVisibility::Hidden);
```

---

## 主题样式

按钮会自动应用主题系统中定义的样式：

- **正常状态**: `theme.button.normal` 颜色集
- **悬停状态**: `theme.button.hover` 颜色集
- **按下状态**: `theme.button.pressed` 颜色集
- **禁用状态**: `theme.button.disabled` 颜色集

可通过 `UIThemeManager` 切换主题来改变按钮外观。

---

## 注意事项

1. **线程安全**: UI操作应在主线程进行
2. **回调执行**: 点击回调在主线程的事件循环中执行
3. **状态管理**: 悬停和按下状态由输入系统自动管理
4. **标签更新**: 修改标签会触发布局更新

---

## 参见

- [UIWidget](UIWidget.md) - 基类文档
- [UIToggle](UIToggle.md) - 开关控件
- [UIMenu](UIMenu.md) - 菜单系统
- [UITheme](UITheme.md) - 主题系统

---

[返回 API 首页](README.md)

