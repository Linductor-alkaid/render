# RenderState API 参考

[返回 API 首页](README.md)

---

## 概述

`RenderState` 管理 OpenGL 渲染状态，缓存状态以减少不必要的状态切换，提高渲染性能。

**头文件**: `render/render_state.h`  
**命名空间**: `Render`

---

## 枚举类型

### BlendMode

混合模式枚举。

```cpp
enum class BlendMode {
    None,       // 无混合
    Alpha,      // Alpha 混合（透明）
    Additive,   // 加法混合（发光效果）
    Multiply,   // 乘法混合
    Custom      // 自定义混合
};
```

---

### DepthFunc

深度测试函数枚举。

```cpp
enum class DepthFunc {
    Never,          // 永不通过
    Less,           // 小于（默认）
    Equal,          // 等于
    LessEqual,      // 小于或等于
    Greater,        // 大于
    NotEqual,       // 不等于
    GreaterEqual,   // 大于或等于
    Always          // 总是通过
};
```

---

### CullFace

面剔除模式枚举。

```cpp
enum class CullFace {
    None,           // 无剔除
    Front,          // 剔除正面
    Back,           // 剔除背面（默认）
    FrontAndBack    // 剔除所有面
};
```

---

## 深度测试

### SetDepthTest

启用/禁用深度测试。

```cpp
void SetDepthTest(bool enable);
```

**示例**:
```cpp
// 3D 场景启用深度测试
renderState->SetDepthTest(true);

// 2D UI 禁用深度测试
renderState->SetDepthTest(false);
```

---

### SetDepthFunc

设置深度比较函数。

```cpp
void SetDepthFunc(DepthFunc func);
```

**示例**:
```cpp
// 标准 3D 渲染
renderState->SetDepthFunc(DepthFunc::Less);

// 天空盒渲染（总是通过）
renderState->SetDepthFunc(DepthFunc::Always);
```

---

### SetDepthWrite

启用/禁用深度写入。

```cpp
void SetDepthWrite(bool enable);
```

**示例**:
```cpp
// 渲染半透明物体前禁用深度写入
renderState->SetDepthWrite(false);
renderState->SetBlendMode(BlendMode::Alpha);
// 渲染半透明物体...

// 恢复深度写入
renderState->SetDepthWrite(true);
```

---

## 混合模式

### SetBlendMode

设置混合模式。

```cpp
void SetBlendMode(BlendMode mode);
```

**示例**:
```cpp
// Alpha 混合（透明度）
renderState->SetBlendMode(BlendMode::Alpha);

// 加法混合（发光效果）
renderState->SetBlendMode(BlendMode::Additive);

// 禁用混合
renderState->SetBlendMode(BlendMode::None);
```

---

### SetBlendFunc

设置自定义混合函数。

```cpp
void SetBlendFunc(uint32_t srcFactor, uint32_t dstFactor);
```

**参数**: OpenGL 混合因子（GL_ONE, GL_SRC_ALPHA 等）

**示例**:
```cpp
#include <glad/glad.h>

// 自定义混合
renderState->SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
```

---

## 面剔除

### SetCullFace

设置面剔除模式。

```cpp
void SetCullFace(CullFace mode);
```

**示例**:
```cpp
// 剔除背面（标准）
renderState->SetCullFace(CullFace::Back);

// 双面渲染
renderState->SetCullFace(CullFace::None);

// 内部渲染（如天空盒）
renderState->SetCullFace(CullFace::Front);
```

---

## 视口和裁剪

### SetViewport

设置渲染视口。

```cpp
void SetViewport(int x, int y, int width, int height);
```

**示例**:
```cpp
// 全屏视口
renderState->SetViewport(0, 0, windowWidth, windowHeight);

// 分屏渲染（左半屏）
renderState->SetViewport(0, 0, windowWidth / 2, windowHeight);
```

---

### SetScissorTest

启用/禁用裁剪测试。

```cpp
void SetScissorTest(bool enable);
```

---

### SetScissorRect

设置裁剪区域。

```cpp
void SetScissorRect(int x, int y, int width, int height);
```

**示例**:
```cpp
// 启用裁剪
renderState->SetScissorTest(true);
renderState->SetScissorRect(100, 100, 200, 200);
// 只在 (100,100) 到 (300,300) 区域渲染

// 禁用裁剪
renderState->SetScissorTest(false);
```

---

## 清屏

### SetClearColor

设置清屏颜色。

```cpp
void SetClearColor(const Color& color);
```

**示例**:
```cpp
renderState->SetClearColor(Color(0.1f, 0.1f, 0.15f, 1.0f));
```

---

### Clear

清空缓冲区。

```cpp
void Clear(bool colorBuffer = true, 
          bool depthBuffer = true, 
          bool stencilBuffer = false);
```

**示例**:
```cpp
// 清空所有
renderState->Clear();

// 只清空深度
renderState->Clear(false, true, false);
```

---

## 状态管理

### Reset

重置所有状态为默认值。

```cpp
void Reset();
```

**示例**:
```cpp
// 场景切换时重置状态
renderState->Reset();
```

---

### GetBlendMode / GetCullFace

获取当前状态。

```cpp
BlendMode GetBlendMode() const;
CullFace GetCullFace() const;
```

---

## 完整示例

### 3D 场景渲染

```cpp
RenderState* state = renderer->GetRenderState();

// 不透明物体
state->SetDepthTest(true);
state->SetDepthWrite(true);
state->SetDepthFunc(DepthFunc::Less);
state->SetBlendMode(BlendMode::None);
state->SetCullFace(CullFace::Back);

// 渲染不透明物体...

// 半透明物体
state->SetDepthWrite(false);  // 禁用深度写入
state->SetBlendMode(BlendMode::Alpha);

// 渲染半透明物体...

// 恢复状态
state->SetDepthWrite(true);
state->SetBlendMode(BlendMode::None);
```

---

### UI 渲染

```cpp
// 保存 3D 状态
auto oldBlend = state->GetBlendMode();
auto oldCull = state->GetCullFace();

// UI 状态
state->SetDepthTest(false);
state->SetBlendMode(BlendMode::Alpha);
state->SetCullFace(CullFace::None);

// 渲染 UI...

// 恢复状态
state->SetDepthTest(true);
state->SetBlendMode(oldBlend);
state->SetCullFace(oldCull);
```

---

## 性能提示

1. **状态缓存**: `RenderState` 自动缓存，避免重复设置
2. **批处理**: 按状态分组渲染对象
3. **状态排序**: 减少状态切换次数

---

[上一篇: UniformManager](UniformManager.md) | [下一篇: OpenGLContext](OpenGLContext.md)

