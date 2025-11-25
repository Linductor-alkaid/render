# UISlider API 参考

[返回 API 首页](README.md)

---

## 概述

`UISlider` 是滑块控件类，继承自 `UIWidget`，提供数值拖拽调整功能。

**头文件**: `render/ui/widgets/ui_slider.h`  
**命名空间**: `Render::UI`

**参考**: 基于 Blender 的 `UI_WTYPE_SLIDER` 设计

### 🎨 核心特性

- **数值范围**: 可配置最小值、最大值、步长
- **方向支持**: 水平或垂直滑块
- **拖拽交互**: 鼠标拖拽改变数值
- **数值显示**: 可选的数值文本显示
- **值改变回调**: 数值改变时触发

---

## 类定义

```cpp
class UISlider : public UIWidget {
public:
    using ValueChangedHandler = std::function<void(UISlider&, float)>;

    explicit UISlider(std::string id);

    // 数值范围
    void SetRange(float minValue, float maxValue);
    void SetMinValue(float minValue);
    void SetMaxValue(float maxValue);
    [[nodiscard]] float GetMinValue() const noexcept;
    [[nodiscard]] float GetMaxValue() const noexcept;

    // 当前值
    void SetValue(float value);
    [[nodiscard]] float GetValue() const noexcept;

    // 步长
    void SetStep(float step);
    [[nodiscard]] float GetStep() const noexcept;

    // 方向
    void SetHorizontal(bool horizontal);
    [[nodiscard]] bool IsHorizontal() const noexcept;

    // 显示选项
    void SetShowValue(bool show);
    [[nodiscard]] bool IsShowingValue() const noexcept;

    // 状态
    [[nodiscard]] bool IsHovered() const noexcept;
    [[nodiscard]] bool IsDragging() const noexcept;

    // 回调
    void SetOnValueChanged(ValueChangedHandler handler);
};
```

---

## 构造函数

### UISlider

创建滑块实例。

```cpp
explicit UISlider(std::string id);
```

**参数**:
- `id` - 滑块的唯一标识符

**示例**:
```cpp
auto slider = std::make_unique<UISlider>("volume_slider");
```

---

## 数值范围

### SetRange

设置最小值和最大值。

```cpp
void SetRange(float minValue, float maxValue);
```

**参数**:
- `minValue` - 最小值
- `maxValue` - 最大值

**示例**:
```cpp
slider->SetRange(0.0f, 100.0f);
```

---

### SetValue

设置当前值。

```cpp
void SetValue(float value);
```

**参数**:
- `value` - 当前值（会自动限制在范围内）

**说明**: 会触发 `OnValueChanged` 回调

**示例**:
```cpp
slider->SetValue(50.0f);
```

---

### GetValue

获取当前值。

```cpp
[[nodiscard]] float GetValue() const noexcept;
```

**返回值**: 当前值

---

### SetStep

设置步长。

```cpp
void SetStep(float step);
```

**参数**:
- `step` - 步长（0表示连续）

**示例**:
```cpp
slider->SetStep(1.0f);  // 整数值
```

---

## 方向设置

### SetHorizontal

设置是否为水平滑块。

```cpp
void SetHorizontal(bool horizontal);
```

**参数**:
- `horizontal` - `true` 为水平，`false` 为垂直

**示例**:
```cpp
slider->SetHorizontal(true);  // 水平滑块
```

---

## 显示选项

### SetShowValue

设置是否显示数值。

```cpp
void SetShowValue(bool show);
```

**参数**:
- `show` - 是否显示数值文本

**示例**:
```cpp
slider->SetShowValue(true);
```

---

## 状态查询

### IsDragging

检查是否正在拖拽。

```cpp
[[nodiscard]] bool IsDragging() const noexcept;
```

**返回值**: 拖拽中返回 `true`

---

## 事件处理

### SetOnValueChanged

设置值改变回调。

```cpp
void SetOnValueChanged(ValueChangedHandler handler);
```

**参数**:
- `handler` - 回调函数 `void(UISlider&, float newValue)`

**示例**:
```cpp
slider->SetOnValueChanged([](UISlider&, float value) {
    LOG_INFO("Slider value: {}", value);
    SetVolume(value / 100.0f);
});
```

---

## 使用示例

### 音量滑块

```cpp
auto volumeSlider = std::make_unique<UISlider>("volume");
volumeSlider->SetRange(0.0f, 100.0f);
volumeSlider->SetValue(75.0f);
volumeSlider->SetStep(1.0f);
volumeSlider->SetHorizontal(true);
volumeSlider->SetShowValue(true);
volumeSlider->SetPreferredSize({200.0f, 30.0f});

volumeSlider->SetOnValueChanged([](UISlider&, float value) {
    AudioSystem::SetMasterVolume(value / 100.0f);
});

container->AddChild(std::move(volumeSlider));
```

### 亮度调节

```cpp
auto brightnessSlider = std::make_unique<UISlider>("brightness");
brightnessSlider->SetRange(-1.0f, 1.0f);
brightnessSlider->SetValue(0.0f);
brightnessSlider->SetStep(0.1f);
brightnessSlider->SetHorizontal(true);

brightnessSlider->SetOnValueChanged([](UISlider&, float value) {
    SetBrightness(value);
});
```

### 垂直滑块（进度条）

```cpp
auto progressSlider = std::make_unique<UISlider>("progress");
progressSlider->SetRange(0.0f, 1.0f);
progressSlider->SetValue(0.0f);
progressSlider->SetHorizontal(false);  // 垂直
progressSlider->SetPreferredSize({30.0f, 200.0f});
progressSlider->SetEnabled(false);  // 只读进度显示

// 更新进度
void UpdateProgress(float progress) {
    progressSlider->SetValue(progress);
}
```

### 整数滑块

```cpp
auto levelSlider = std::make_unique<UISlider>("level");
levelSlider->SetRange(1.0f, 10.0f);
levelSlider->SetValue(5.0f);
levelSlider->SetStep(1.0f);  // 整数步长
levelSlider->SetShowValue(true);

levelSlider->SetOnValueChanged([](UISlider&, float value) {
    int level = static_cast<int>(value);
    SetDifficultyLevel(level);
});
```

---

## 交互行为

- **点击**: 点击轨道跳转到该位置
- **拖拽**: 拖拽滑块改变数值
- **步长**: 有步长时值会对齐到最近的步长点
- **范围限制**: 值自动限制在最小值和最大值之间

---

## 渲染特性

- **轨道**: 滑块的背景轨道
- **滑块**: 可拖拽的滑块按钮
- **数值文本**: 可选的数值显示
- **主题样式**: 自动应用主题颜色

---

## 参见

- [UIWidget](UIWidget.md) - 基类文档
- [UIButton](UIButton.md) - 按钮控件
- [UIToggle](UIToggle.md) - 开关控件

---

[返回 API 首页](README.md)

