# UIToggle API 参考

[返回 API 首页](README.md)

---

## 概述

`UIToggle` 是开关控件类，继承自 `UIWidget`，提供布尔值开关功能，类似移动应用中的Toggle Switch。

**头文件**: `render/ui/widgets/ui_toggle.h`  
**命名空间**: `Render::UI`

### 🎨 核心特性

- **开关状态**: On/Off 二态切换
- **文本标签**: 可选的标签文本
- **动画过渡**: 平滑的状态切换动画（规划中）
- **状态回调**: 状态改变时触发

---

## 类定义

```cpp
class UIToggle : public UIWidget {
public:
    using ToggleChangedHandler = std::function<void(UIToggle&, bool)>;

    explicit UIToggle(std::string id);

    void SetLabel(std::string label);
    [[nodiscard]] const std::string& GetLabel() const noexcept;

    void SetOn(bool on);
    [[nodiscard]] bool IsOn() const noexcept;

    [[nodiscard]] bool IsHovered() const noexcept;

    void SetOnToggleChanged(ToggleChangedHandler handler);
};
```

---

## 构造函数

### UIToggle

创建开关实例。

```cpp
explicit UIToggle(std::string id);
```

**参数**:
- `id` - 开关的唯一标识符

**示例**:
```cpp
auto toggle = std::make_unique<UIToggle>("vsync_toggle");
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
toggle->SetLabel("Enable VSync");
```

---

## 状态管理

### SetOn

设置开关状态。

```cpp
void SetOn(bool on);
```

**参数**:
- `on` - 开关状态

**说明**: 会触发 `OnToggleChanged` 回调

**示例**:
```cpp
toggle->SetOn(true);
```

---

### IsOn

获取开关状态。

```cpp
[[nodiscard]] bool IsOn() const noexcept;
```

**返回值**: 打开返回 `true`，关闭返回 `false`

---

## 事件处理

### SetOnToggleChanged

设置状态改变回调。

```cpp
void SetOnToggleChanged(ToggleChangedHandler handler);
```

**参数**:
- `handler` - 回调函数 `void(UIToggle&, bool isOn)`

**示例**:
```cpp
toggle->SetOnToggleChanged([](UIToggle&, bool isOn) {
    SetVSync(isOn);
    LOG_INFO("VSync: {}", isOn ? "On" : "Off");
});
```

---

## 使用示例

### 创建基本开关

```cpp
auto vsyncToggle = std::make_unique<UIToggle>("vsync");
vsyncToggle->SetLabel("VSync");
vsyncToggle->SetOn(true);

vsyncToggle->SetOnToggleChanged([](UIToggle&, bool isOn) {
    Renderer::GetInstance()->SetVSync(isOn);
});

container->AddChild(std::move(vsyncToggle));
```

### 设置面板

```cpp
auto settingsPanel = std::make_unique<UIWidget>("settings");
settingsPanel->SetLayoutDirection(UILayoutDirection::Vertical);
settingsPanel->SetSpacing(10.0f);

// 音频开关
auto audioToggle = std::make_unique<UIToggle>("audio");
audioToggle->SetLabel("Enable Audio");
audioToggle->SetOn(true);
settingsPanel->AddChild(std::move(audioToggle));

// 全屏开关
auto fullscreenToggle = std::make_unique<UIToggle>("fullscreen");
fullscreenToggle->SetLabel("Fullscreen");
fullscreenToggle->SetOn(false);
settingsPanel->AddChild(std::move(fullscreenToggle));

// 垂直同步
auto vsyncToggle = std::make_unique<UIToggle>("vsync");
vsyncToggle->SetLabel("VSync");
vsyncToggle->SetOn(true);
settingsPanel->AddChild(std::move(vsyncToggle));
```

### 条件切换

```cpp
auto debugToggle = std::make_unique<UIToggle>("debug_mode");
debugToggle->SetLabel("Debug Mode");
debugToggle->SetOn(false);

debugToggle->SetOnToggleChanged([](UIToggle&, bool isOn) {
    SetDebugMode(isOn);
    
    if (isOn) {
        ShowDebugPanel();
    } else {
        HideDebugPanel();
    }
});
```

---

## 与 UICheckBox 的区别

| 特性 | UIToggle | UICheckBox |
|------|----------|------------|
| 外观 | 滑动开关 | 方框+勾选 |
| 用途 | 开关设置 | 选项勾选 |
| 动画 | 平滑过渡 | 即时切换 |
| 典型场景 | 功能开关 | 多选选项 |

---

## 渲染特性

- **轨道**: 背景轨道，颜色表示状态
- **滑块**: 可拖拽的圆形或矩形滑块
- **动画**: 状态切换时的平滑动画（规划中）
- **标签**: 显示在开关右侧

---

## 参见

- [UICheckBox](UICheckBox.md) - 复选框控件
- [UISlider](UISlider.md) - 滑块控件
- [UIWidget](UIWidget.md) - 基类文档

---

[返回 API 首页](README.md)

