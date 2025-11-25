# UITextField API 参考

[返回 API 首页](README.md)

---

## 概述

`UITextField` 是文本输入框控件类，继承自 `UIWidget`，提供单行文本输入和编辑功能。

**头文件**: `render/ui/widgets/ui_text_field.h`  
**命名空间**: `Render::UI`

### 🎨 核心特性

- **文本输入**: 支持键盘输入和退格
- **光标管理**: 文本光标显示和移动
- **文本选择**: 支持文本选择（规划中）
- **占位符**: 空文本时显示提示
- **只读模式**: 可设置为只读

---

## 类定义

```cpp
class UITextField : public UIWidget {
public:
    using TextChangedHandler = std::function<void(UITextField&, const std::string&)>;

    explicit UITextField(std::string id);

    // 文本管理
    void SetText(std::string text);
    [[nodiscard]] const std::string& GetText() const noexcept;

    void SetPlaceholder(std::string placeholder);
    [[nodiscard]] const std::string& GetPlaceholder() const noexcept;

    // 状态
    [[nodiscard]] bool IsFocused() const noexcept;
    [[nodiscard]] bool IsHovered() const noexcept;

    // 回调
    void SetOnTextChanged(TextChangedHandler handler);
};
```

---

## 构造函数

### UITextField

创建文本输入框实例。

```cpp
explicit UITextField(std::string id);
```

**参数**:
- `id` - 文本框的唯一标识符

**示例**:
```cpp
auto textField = std::make_unique<UITextField>("name_input");
```

---

## 文本管理

### SetText

设置文本内容。

```cpp
void SetText(std::string text);
```

**参数**:
- `text` - 文本内容

**示例**:
```cpp
textField->SetText("Hello World");
```

---

### GetText

获取文本内容。

```cpp
[[nodiscard]] const std::string& GetText() const noexcept;
```

**返回值**: 文本字符串引用

---

### SetPlaceholder

设置占位符文本。

```cpp
void SetPlaceholder(std::string placeholder);
```

**参数**:
- `placeholder` - 占位符文本

**示例**:
```cpp
textField->SetPlaceholder("Enter your name...");
```

---

### GetPlaceholder

获取占位符文本。

```cpp
[[nodiscard]] const std::string& GetPlaceholder() const noexcept;
```

**返回值**: 占位符字符串引用

---

## 状态查询

### IsFocused

检查是否获得焦点。

```cpp
[[nodiscard]] bool IsFocused() const noexcept;
```

**返回值**: 有焦点返回 `true`

---

### IsHovered

检查鼠标是否悬停。

```cpp
[[nodiscard]] bool IsHovered() const noexcept;
```

**返回值**: 悬停返回 `true`

---

## 事件处理

### SetOnTextChanged

设置文本改变回调。

```cpp
void SetOnTextChanged(TextChangedHandler handler);
```

**参数**:
- `handler` - 回调函数 `void(UITextField&, const std::string& newText)`

**示例**:
```cpp
textField->SetOnTextChanged([](UITextField& field, const std::string& text) {
    LOG_INFO("Text changed: {}", text);
    ValidateInput(text);
});
```

---

## 使用示例

### 创建基本文本输入框

```cpp
auto textField = std::make_unique<UITextField>("username");
textField->SetPlaceholder("Enter username...");
textField->SetPreferredSize({200.0f, 30.0f});

textField->SetOnTextChanged([](UITextField&, const std::string& text) {
    LOG_INFO("Username: {}", text);
});

container->AddChild(std::move(textField));
```

### 只读文本框（状态显示）

```cpp
auto statusField = std::make_unique<UITextField>("status");
statusField->SetText("Ready");
statusField->SetEnabled(false);  // 只读
textField->SetPreferredSize({300.0f, 25.0f});

container->AddChild(std::move(statusField));
```

### 表单输入

```cpp
// 用户名输入
auto nameField = std::make_unique<UITextField>("name");
nameField->SetPlaceholder("Name");
nameField->SetPreferredSize({250.0f, 30.0f});

// 邮箱输入
auto emailField = std::make_unique<UITextField>("email");
emailField->SetPlaceholder("Email");
emailField->SetPreferredSize({250.0f, 30.0f});

// 密码输入（注意：当前不支持密码模式，这是未来改进）
auto passwordField = std::make_unique<UITextField>("password");
passwordField->SetPlaceholder("Password");
passwordField->SetPreferredSize({250.0f, 30.0f});
```

---

## 输入处理

文本框自动处理以下输入：

- **键盘输入**: 可打印字符
- **退格键**: 删除字符
- **方向键**: 移动光标（规划中）
- **Ctrl+C/V**: 复制/粘贴（规划中）
- **点击**: 设置焦点和光标位置

---

## 主题样式

文本框会自动应用主题系统中定义的样式：

- **正常状态**: `theme.textField.normal` 颜色集
- **悬停状态**: `theme.textField.hover` 颜色集
- **焦点状态**: `theme.textField.active` 颜色集
- **禁用状态**: `theme.textField.disabled` 颜色集

---

## 注意事项

1. **焦点管理**: 点击文本框获得焦点，点击外部失去焦点
2. **文本光标**: 仅在有焦点时显示
3. **单行输入**: 当前仅支持单行文本
4. **UTF-8**: 支持UTF-8中文输入

---

## 未来改进

- ⏳ 文本选择和复制/粘贴
- ⏳ 光标位置移动（方向键、Home/End）
- ⏳ 密码输入模式
- ⏳ 输入验证和格式化
- ⏳ 多行文本支持

---

## 参见

- [UIWidget](UIWidget.md) - 基类文档
- [UIButton](UIButton.md) - 按钮控件
- [UITheme](UITheme.md) - 主题系统

---

[返回 API 首页](README.md)

