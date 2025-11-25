# UICheckBox API 参考

[返回 API 首页](README.md)

---

## 概述

`UICheckBox` 是复选框控件类，继承自 `UIWidget`，提供选中/未选中状态切换功能。

**头文件**: `render/ui/widgets/ui_checkbox.h`  
**命名空间**: `Render::UI`

**参考**: 基于 Blender 的 `UI_WTYPE_CHECKBOX` 设计

### 🎨 核心特性

- **选中状态**: 支持选中/未选中切换
- **文本标签**: 可选的标签文本
- **点击切换**: 点击自动切换状态
- **状态回调**: 状态改变时触发回调

---

## 类定义

```cpp
class UICheckBox : public UIWidget {
public:
    using CheckChangedHandler = std::function<void(UICheckBox&, bool)>;

    explicit UICheckBox(std::string id);

    void SetLabel(std::string label);
    [[nodiscard]] const std::string& GetLabel() const noexcept;

    void SetChecked(bool checked);
    [[nodiscard]] bool IsChecked() const noexcept;

    [[nodiscard]] bool IsHovered() const noexcept;

    void SetOnCheckChanged(CheckChangedHandler handler);
};
```

---

## 构造函数

### UICheckBox

创建复选框实例。

```cpp
explicit UICheckBox(std::string id);
```

**参数**:
- `id` - 复选框的唯一标识符

**示例**:
```cpp
auto checkbox = std::make_unique<UICheckBox>("agree_terms");
```

---

## 标签管理

### SetLabel

设置标签文本。

```cpp
void SetLabel(std::string label);
```

**参数**:
- `label` - 标签文本

**示例**:
```cpp
checkbox->SetLabel("I agree to the terms");
```

---

### GetLabel

获取标签文本。

```cpp
[[nodiscard]] const std::string& GetLabel() const noexcept;
```

**返回值**: 标签字符串引用

---

## 状态管理

### SetChecked

设置选中状态。

```cpp
void SetChecked(bool checked);
```

**参数**:
- `checked` - 选中状态

**说明**: 会触发 `OnCheckChanged` 回调

**示例**:
```cpp
checkbox->SetChecked(true);
```

---

### IsChecked

获取选中状态。

```cpp
[[nodiscard]] bool IsChecked() const noexcept;
```

**返回值**: 选中返回 `true`

---

### IsHovered

检查鼠标是否悬停。

```cpp
[[nodiscard]] bool IsHovered() const noexcept;
```

**返回值**: 悬停返回 `true`

---

## 事件处理

### SetOnCheckChanged

设置状态改变回调。

```cpp
void SetOnCheckChanged(CheckChangedHandler handler);
```

**参数**:
- `handler` - 回调函数 `void(UICheckBox&, bool checked)`

**示例**:
```cpp
checkbox->SetOnCheckChanged([](UICheckBox&, bool checked) {
    LOG_INFO("Checkbox state: {}", checked ? "Checked" : "Unchecked");
    UpdateSetting(checked);
});
```

---

## 使用示例

### 创建基本复选框

```cpp
auto checkbox = std::make_unique<UICheckBox>("show_grid");
checkbox->SetLabel("Show Grid");
checkbox->SetChecked(true);

checkbox->SetOnCheckChanged([](UICheckBox&, bool checked) {
    SetGridVisibility(checked);
});

container->AddChild(std::move(checkbox));
```

### 设置选项组

```cpp
// 创建多个复选框
auto vsyncBox = std::make_unique<UICheckBox>("vsync");
vsyncBox->SetLabel("VSync");
vsyncBox->SetChecked(true);
vsyncBox->SetOnCheckChanged([](UICheckBox&, bool checked) {
    SetVSync(checked);
});

auto fullscreenBox = std::make_unique<UICheckBox>("fullscreen");
fullscreenBox->SetLabel("Fullscreen");
fullscreenBox->SetChecked(false);
fullscreenBox->SetOnCheckChanged([](UICheckBox&, bool checked) {
    SetFullscreen(checked);
});

optionsPanel->AddChild(std::move(vsyncBox));
optionsPanel->AddChild(std::move(fullscreenBox));
```

### 同意条款复选框

```cpp
auto agreeBox = std::make_unique<UICheckBox>("agree");
agreeBox->SetLabel("I agree to the terms and conditions");
agreeBox->SetChecked(false);

auto* submitBtn = formPanel->FindById("submit_btn");
agreeBox->SetOnCheckChanged([submitBtn](UICheckBox&, bool checked) {
    // 只有同意条款才能提交
    if (submitBtn) {
        submitBtn->SetEnabled(checked);
    }
});
```

---

## 交互行为

- **点击**: 点击复选框或标签切换状态
- **空格键**: 焦点状态下按空格切换状态
- **禁用**: 禁用时不响应交互

---

## 渲染特性

复选框的渲染由主题系统控制：

- **方框**: 显示选中/未选中状态
- **勾选标记**: 选中时显示 ✓
- **标签文本**: 显示在方框右侧
- **悬停高亮**: 鼠标悬停时背景高亮

---

## 参见

- [UIRadioButton](UIRadioButton.md) - 单选按钮
- [UIToggle](UIToggle.md) - 开关控件
- [UIWidget](UIWidget.md) - 基类文档

---

[返回 API 首页](README.md)

