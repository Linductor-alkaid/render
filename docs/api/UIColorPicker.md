# UIColorPicker API 参考

[返回 API 首页](README.md)

---

## 概述

`UIColorPicker` 是颜色选择器控件类，继承自 `UIWidget`，提供RGB颜色选择和预览功能。

**头文件**: `render/ui/widgets/ui_color_picker.h`  
**命名空间**: `Render::UI`

**参考**: 基于 Blender 的 `UI_WTYPE_RGB_PICKER` 设计

### 🎨 核心特性

- **RGB颜色选择**: 支持红、绿、蓝通道独立调节
- **Alpha通道**: 支持透明度调节
- **颜色预览**: 实时预览当前颜色
- **滑块控制**: 使用滑块调节各通道
- **值改变回调**: 颜色改变时触发

---

## 类定义

```cpp
class UIColorPicker : public UIWidget {
public:
    using ColorChangedHandler = std::function<void(UIColorPicker&, const Color&)>;

    explicit UIColorPicker(std::string id);

    // 颜色管理
    void SetColor(const Color& color);
    [[nodiscard]] const Color& GetColor() const noexcept;

    // Alpha 通道
    void SetAlphaEnabled(bool enabled);
    [[nodiscard]] bool IsAlphaEnabled() const noexcept;

    // 回调
    void SetOnColorChanged(ColorChangedHandler handler);
};
```

---

## 构造函数

### UIColorPicker

创建颜色选择器实例。

```cpp
explicit UIColorPicker(std::string id);
```

**参数**:
- `id` - 颜色选择器的唯一标识符

**示例**:
```cpp
auto colorPicker = std::make_unique<UIColorPicker>("bg_color");
```

---

## 颜色管理

### SetColor

设置当前颜色。

```cpp
void SetColor(const Color& color);
```

**参数**:
- `color` - 颜色值（RGBA）

**示例**:
```cpp
colorPicker->SetColor(Color(1.0f, 0.5f, 0.0f, 1.0f));  // 橙色
```

---

### GetColor

获取当前颜色。

```cpp
[[nodiscard]] const Color& GetColor() const noexcept;
```

**返回值**: 颜色值引用

---

## Alpha 通道

### SetAlphaEnabled

启用/禁用Alpha通道。

```cpp
void SetAlphaEnabled(bool enabled);
```

**参数**:
- `enabled` - 是否启用Alpha通道

**示例**:
```cpp
colorPicker->SetAlphaEnabled(true);
```

---

### IsAlphaEnabled

检查Alpha通道是否启用。

```cpp
[[nodiscard]] bool IsAlphaEnabled() const noexcept;
```

**返回值**: 启用返回 `true`

---

## 事件处理

### SetOnColorChanged

设置颜色改变回调。

```cpp
void SetOnColorChanged(ColorChangedHandler handler);
```

**参数**:
- `handler` - 回调函数 `void(UIColorPicker&, const Color& newColor)`

**示例**:
```cpp
colorPicker->SetOnColorChanged([](UIColorPicker&, const Color& color) {
    SetBackgroundColor(color);
    LOG_INFO("Color: R={} G={} B={} A={}", color.r, color.g, color.b, color.a);
});
```

---

## 使用示例

### 背景颜色选择器

```cpp
auto bgColorPicker = std::make_unique<UIColorPicker>("bg_color");
bgColorPicker->SetColor(Color(0.1f, 0.1f, 0.1f, 1.0f));  // 深灰色
bgColorPicker->SetAlphaEnabled(false);  // 背景不需要透明度

bgColorPicker->SetOnColorChanged([](UIColorPicker&, const Color& color) {
    Renderer::GetInstance()->SetClearColor(color);
});

container->AddChild(std::move(bgColorPicker));
```

### 材质颜色编辑

```cpp
auto materialPanel = std::make_unique<UIWidget>("material_panel");
materialPanel->SetLayoutDirection(UILayoutDirection::Vertical);
materialPanel->SetSpacing(10.0f);

// 漫反射颜色
auto diffuseColor = std::make_unique<UIColorPicker>("diffuse");
diffuseColor->SetColor(Color::White());
diffuseColor->SetOnColorChanged([](UIColorPicker&, const Color& color) {
    material->SetDiffuseColor(color);
});

// 高光颜色
auto specularColor = std::make_unique<UIColorPicker>("specular");
specularColor->SetColor(Color::White());
specularColor->SetOnColorChanged([](UIColorPicker&, const Color& color) {
    material->SetSpecularColor(color);
});

// 自发光颜色
auto emissiveColor = std::make_unique<UIColorPicker>("emissive");
emissiveColor->SetColor(Color::Black());
emissiveColor->SetAlphaEnabled(false);
emissiveColor->SetOnColorChanged([](UIColorPicker&, const Color& color) {
    material->SetEmissiveColor(color);
});

materialPanel->AddChild(std::move(diffuseColor));
materialPanel->AddChild(std::move(specularColor));
materialPanel->AddChild(std::move(emissiveColor));
```

### 带透明度的颜色选择

```cpp
auto spriteColor = std::make_unique<UIColorPicker>("sprite_tint");
spriteColor->SetColor(Color::White());
spriteColor->SetAlphaEnabled(true);  // 启用Alpha

spriteColor->SetOnColorChanged([](UIColorPicker&, const Color& color) {
    sprite->SetTint(color);
});
```

---

## 组件布局

颜色选择器内部包含：

1. **R滑块**: 红色通道 (0.0 - 1.0)
2. **G滑块**: 绿色通道 (0.0 - 1.0)
3. **B滑块**: 蓝色通道 (0.0 - 1.0)
4. **A滑块**: Alpha通道 (0.0 - 1.0)（可选）
5. **颜色预览**: 显示当前颜色的方块

---

## 交互行为

- **滑块拖拽**: 拖拽各通道滑块调节颜色
- **实时预览**: 调节时实时更新预览
- **值限制**: 各通道值自动限制在 0.0 - 1.0 范围

---

## 未来改进

- ⏳ HSV颜色模式
- ⏳ 颜色选择器弹窗
- ⏳ 颜色历史记录
- ⏳ 十六进制颜色码输入
- ⏳ 吸管工具

---

## 参见

- [UISlider](UISlider.md) - 滑块控件
- [UIWidget](UIWidget.md) - 基类文档
- [颜色选择器文档](../ui/UI_COLOR_PICKER_USAGE.md)

---

[返回 API 首页](README.md)

