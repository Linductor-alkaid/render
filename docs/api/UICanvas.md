# UICanvas API 参考

[返回 API 首页](README.md)

---

## 概述

`UICanvas` 是UI画布类，负责管理UI系统的全局状态和缩放，协调UI与渲染器之间的交互。

**头文件**: `render/ui/uicanvas.h`  
**命名空间**: `Render::UI`

### 🎨 核心特性

- **分辨率缩放**: 支持多种缩放模式
- **DPI适配**: 自动适配高DPI显示
- **状态管理**: 管理窗口尺寸、焦点、光标位置
- **帧同步**: 与渲染器同步更新

---

## 类定义

```cpp
class UICanvas {
public:
    UICanvas() = default;
    ~UICanvas() = default;

    void Initialize(Application::AppContext& ctx);
    void Shutdown(Application::AppContext& ctx);

    void BeginFrame(const Application::FrameUpdateArgs& frame, Application::AppContext& ctx);
    void EndFrame(const Application::FrameUpdateArgs& frame, Application::AppContext& ctx);

    void SetScaleMode(UIScaleMode mode);
    void SetReferenceResolution(int32_t width, int32_t height);
    void SetReferenceDpi(float dpi);

    void SetFocus(bool focus);
    void SetCursorPosition(const Vector2& cursorPosition);

    [[nodiscard]] const UICanvasConfig& GetConfig() const;
    [[nodiscard]] const UICanvasState& GetState() const;
};
```

---

## 数据结构

### UIScaleMode

```cpp
enum class UIScaleMode {
    Fixed,          // 固定分辨率（不缩放）
    ScaleToFit,     // 缩放以适应窗口（保持比例）
    MatchWidth,     // 匹配宽度
    MatchHeight     // 匹配高度
};
```

---

### UICanvasConfig

```cpp
struct UICanvasConfig {
    int32_t referenceWidth = 1920;      // 参考宽度
    int32_t referenceHeight = 1080;     // 参考高度
    float referenceDpi = 96.0f;         // 参考DPI
    UIScaleMode scaleMode = UIScaleMode::ScaleToFit;

    void SetReferenceResolution(int32_t width, int32_t height);
};
```

---

### UICanvasState

```cpp
struct UICanvasState {
    int32_t windowWidth = 0;            // 窗口宽度
    int32_t windowHeight = 0;           // 窗口高度
    float dpiScale = 1.0f;              // DPI缩放
    float scaleFactor = 1.0f;           // 缩放因子
    bool hasFocus = true;               // 窗口焦点
    Vector2 cursorPosition;             // 光标位置
    float absoluteTime = 0.0f;          // 绝对时间
    float deltaTime = 0.0f;             // 帧间隔

    [[nodiscard]] Vector2 WindowSize() const;
};
```

---

## 初始化

### Initialize

初始化UI画布。

```cpp
void Initialize(Application::AppContext& ctx);
```

**参数**:
- `ctx` - 应用上下文

**说明**: 通常在模块的 `OnAttach` 中调用

---

### Shutdown

关闭UI画布。

```cpp
void Shutdown(Application::AppContext& ctx);
```

**参数**:
- `ctx` - 应用上下文

---

## 帧管理

### BeginFrame

开始UI帧。

```cpp
void BeginFrame(const Application::FrameUpdateArgs& frame, Application::AppContext& ctx);
```

**参数**:
- `frame` - 帧更新参数
- `ctx` - 应用上下文

**说明**: 在每帧开始时调用，更新状态

---

### EndFrame

结束UI帧。

```cpp
void EndFrame(const Application::FrameUpdateArgs& frame, Application::AppContext& ctx);
```

**参数**:
- `frame` - 帧更新参数
- `ctx` - 应用上下文

---

## 配置

### SetScaleMode

设置缩放模式。

```cpp
void SetScaleMode(UIScaleMode mode);
```

**参数**:
- `mode` - 缩放模式（见 [UIScaleMode](#uiscalemode)）

**示例**:
```cpp
canvas->SetScaleMode(UIScaleMode::ScaleToFit);
```

---

### SetReferenceResolution

设置参考分辨率。

```cpp
void SetReferenceResolution(int32_t width, int32_t height);
```

**参数**:
- `width` - 参考宽度
- `height` - 参考高度

**示例**:
```cpp
canvas->SetReferenceResolution(1920, 1080);
```

---

### SetReferenceDpi

设置参考DPI。

```cpp
void SetReferenceDpi(float dpi);
```

**参数**:
- `dpi` - 参考DPI值（通常为96.0）

---

## 状态管理

### SetFocus

设置窗口焦点状态。

```cpp
void SetFocus(bool focus);
```

**参数**:
- `focus` - 焦点状态

---

### SetCursorPosition

设置光标位置。

```cpp
void SetCursorPosition(const Vector2& cursorPosition);
```

**参数**:
- `cursorPosition` - 光标位置（窗口坐标）

---

## 使用示例

### 基本初始化

```cpp
auto canvas = std::make_unique<UICanvas>();
canvas->Initialize(ctx);
canvas->SetReferenceResolution(1920, 1080);
canvas->SetScaleMode(UIScaleMode::ScaleToFit);
```

### 在应用模块中使用

```cpp
class MyUIModule : public ApplicationModule {
public:
    void OnAttach(AppContext& ctx) override {
        m_canvas = std::make_unique<UICanvas>();
        m_canvas->Initialize(ctx);
        m_canvas->SetReferenceResolution(1920, 1080);
    }

    void OnDetach(AppContext& ctx) override {
        if (m_canvas) {
            m_canvas->Shutdown(ctx);
            m_canvas.reset();
        }
    }

    void OnBeginFrame(const FrameUpdateArgs& frame, AppContext& ctx) override {
        if (m_canvas) {
            m_canvas->BeginFrame(frame, ctx);
        }
    }

    void OnEndFrame(const FrameUpdateArgs& frame, AppContext& ctx) override {
        if (m_canvas) {
            m_canvas->EndFrame(frame, ctx);
        }
    }

private:
    std::unique_ptr<UICanvas> m_canvas;
};
```

### 响应式UI

```cpp
// 设置为匹配窗口宽度
canvas->SetScaleMode(UIScaleMode::MatchWidth);
canvas->SetReferenceResolution(1920, 1080);

// UI宽度会随窗口宽度缩放
// 高度保持比例自动调整
```

### 高DPI支持

```cpp
// 设置参考DPI
canvas->SetReferenceDpi(96.0f);

// 获取当前DPI缩放
const auto& state = canvas->GetState();
float dpiScale = state.dpiScale;

// 应用到主题
auto scaledTheme = UIThemeManager::GetInstance().GetThemeForDPI(dpiScale);
```

---

## 缩放模式详解

### Fixed（固定）

```cpp
canvas->SetScaleMode(UIScaleMode::Fixed);
```

UI使用固定分辨率，不随窗口大小改变。

### ScaleToFit（自适应）

```cpp
canvas->SetScaleMode(UIScaleMode::ScaleToFit);
```

UI缩放以适应窗口，保持参考分辨率的宽高比。

### MatchWidth（匹配宽度）

```cpp
canvas->SetScaleMode(UIScaleMode::MatchWidth);
```

UI宽度匹配窗口宽度，高度按比例调整。

### MatchHeight（匹配高度）

```cpp
canvas->SetScaleMode(UIScaleMode::MatchHeight);
```

UI高度匹配窗口高度，宽度按比例调整。

---

## 获取状态信息

```cpp
const auto& state = canvas->GetState();

int windowWidth = state.windowWidth;
int windowHeight = state.windowHeight;
float dpiScale = state.dpiScale;
float scaleFactor = state.scaleFactor;
Vector2 cursorPos = state.cursorPosition;
float deltaTime = state.deltaTime;
```

---

## 注意事项

1. **初始化顺序**: 应在创建UI控件前初始化
2. **帧管理**: BeginFrame 和 EndFrame 必须成对调用
3. **线程安全**: 所有方法应在主线程调用
4. **上下文依赖**: 需要有效的 `AppContext`

---

## 参见

- [UIWidget](UIWidget.md) - 控件基类
- [UITheme](UITheme.md) - 主题系统
- [UI框架文档](../guides/UI_FRAMEWORK_FOUNDATION_PLAN.md)

---

[返回 API 首页](README.md)

