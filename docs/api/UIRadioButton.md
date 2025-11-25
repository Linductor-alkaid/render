# UIRadioButton API 参考

[返回 API 首页](README.md)

---

## 概述

`UIRadioButton` 是单选按钮控件类，继承自 `UIWidget`，提供单选组功能，确保同组中只有一个选项被选中。

**头文件**: `render/ui/widgets/ui_radio_button.h`  
**命名空间**: `Render::UI`

**参考**: 基于 Blender 的 `UI_WTYPE_RADIO` 设计

### 🎨 核心特性

- **单选组**: 同组只能选中一个
- **文本标签**: 显示选项文本
- **自动切换**: 点击自动切换组内选中项
- **选中回调**: 选中状态改变时触发

---

## 类定义

```cpp
class UIRadioButton : public UIWidget {
public:
    using SelectChangedHandler = std::function<void(UIRadioButton&, bool)>;

    explicit UIRadioButton(std::string id, std::string groupId = "");

    void SetLabel(std::string label);
    [[nodiscard]] const std::string& GetLabel() const noexcept;

    void SetGroupId(std::string groupId);
    [[nodiscard]] const std::string& GetGroupId() const noexcept;

    void SetSelected(bool selected);
    [[nodiscard]] bool IsSelected() const noexcept;

    [[nodiscard]] bool IsHovered() const noexcept;

    void SetOnSelectChanged(SelectChangedHandler handler);
};
```

---

## 构造函数

### UIRadioButton

创建单选按钮实例。

```cpp
explicit UIRadioButton(std::string id, std::string groupId = "");
```

**参数**:
- `id` - 单选按钮的唯一标识符
- `groupId` - 所属单选组ID（可选）

**示例**:
```cpp
auto radio1 = std::make_unique<UIRadioButton>("option1", "render_mode");
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
radio->SetLabel("Wireframe");
```

---

## 组管理

### SetGroupId

设置单选组ID。

```cpp
void SetGroupId(std::string groupId);
```

**参数**:
- `groupId` - 组ID

**说明**: 同一组的单选按钮只能有一个被选中

---

### GetGroupId

获取单选组ID。

```cpp
[[nodiscard]] const std::string& GetGroupId() const noexcept;
```

**返回值**: 组ID字符串引用

---

## 状态管理

### SetSelected

设置选中状态。

```cpp
void SetSelected(bool selected);
```

**参数**:
- `selected` - 选中状态

**说明**: 
- 设置为选中会自动取消同组其他按钮的选中状态
- 会触发 `OnSelectChanged` 回调

**示例**:
```cpp
radio->SetSelected(true);
```

---

### IsSelected

获取选中状态。

```cpp
[[nodiscard]] bool IsSelected() const noexcept;
```

**返回值**: 选中返回 `true`

---

## 事件处理

### SetOnSelectChanged

设置选中状态改变回调。

```cpp
void SetOnSelectChanged(SelectChangedHandler handler);
```

**参数**:
- `handler` - 回调函数 `void(UIRadioButton&, bool selected)`

**示例**:
```cpp
radio->SetOnSelectChanged([](UIRadioButton& btn, bool selected) {
    if (selected) {
        LOG_INFO("Selected: {}", btn.GetLabel());
    }
});
```

---

## 使用示例

### 创建单选组

```cpp
// 创建渲染模式选择组
auto solidRadio = std::make_unique<UIRadioButton>("solid", "render_mode");
solidRadio->SetLabel("Solid");
solidRadio->SetSelected(true);  // 默认选中
solidRadio->SetOnSelectChanged([](UIRadioButton&, bool selected) {
    if (selected) SetRenderMode(RenderMode::Solid);
});

auto wireframeRadio = std::make_unique<UIRadioButton>("wireframe", "render_mode");
wireframeRadio->SetLabel("Wireframe");
wireframeRadio->SetOnSelectChanged([](UIRadioButton&, bool selected) {
    if (selected) SetRenderMode(RenderMode::Wireframe);
});

auto shadedRadio = std::make_unique<UIRadioButton>("shaded", "render_mode");
shadedRadio->SetLabel("Shaded");
shadedRadio->SetOnSelectChanged([](UIRadioButton&, bool selected) {
    if (selected) SetRenderMode(RenderMode::Shaded);
});

modePanel->AddChild(std::move(solidRadio));
modePanel->AddChild(std::move(wireframeRadio));
modePanel->AddChild(std::move(shadedRadio));
```

### 质量设置

```cpp
auto lowRadio = std::make_unique<UIRadioButton>("quality_low", "quality");
lowRadio->SetLabel("Low");

auto mediumRadio = std::make_unique<UIRadioButton>("quality_medium", "quality");
mediumRadio->SetLabel("Medium");
mediumRadio->SetSelected(true);

auto highRadio = std::make_unique<UIRadioButton>("quality_high", "quality");
highRadio->SetLabel("High");

// 统一的回调处理
auto qualityHandler = [](UIRadioButton& btn, bool selected) {
    if (selected) {
        if (btn.GetId() == "quality_low") SetQuality(Quality::Low);
        else if (btn.GetId() == "quality_medium") SetQuality(Quality::Medium);
        else if (btn.GetId() == "quality_high") SetQuality(Quality::High);
    }
};

lowRadio->SetOnSelectChanged(qualityHandler);
mediumRadio->SetOnSelectChanged(qualityHandler);
highRadio->SetOnSelectChanged(qualityHandler);
```

---

## 单选组行为

1. **互斥选择**: 选中一个会自动取消同组其他按钮
2. **至少一个**: 同组中至少有一个被选中（通过初始设置保证）
3. **组识别**: 通过 `groupId` 识别同组按钮
4. **跨容器**: 同组按钮可以在不同容器中

---

## 注意事项

1. **组ID**: 必须设置相同的 `groupId` 才能形成单选组
2. **初始选中**: 创建时应选中一个默认选项
3. **回调检查**: 回调中应检查 `selected` 参数，避免重复处理

---

## 参见

- [UICheckBox](UICheckBox.md) - 复选框控件
- [UIToggle](UIToggle.md) - 开关控件
- [UIWidget](UIWidget.md) - 基类文档

---

[返回 API 首页](README.md)

