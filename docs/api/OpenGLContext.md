# OpenGLContext API 参考

[返回 API 首页](README.md)

---

## 概述

`OpenGLContext` 管理 OpenGL 上下文、窗口创建和扩展检测。

**头文件**: `render/opengl_context.h`  
**命名空间**: `Render`

---

## 配置结构

### OpenGLConfig

OpenGL 上下文配置。

```cpp
struct OpenGLConfig {
    int majorVersion = 4;        // OpenGL 主版本号
    int minorVersion = 5;        // OpenGL 次版本号
    bool coreProfile = true;     // 使用核心模式
    bool debugContext = true;    // 启用调试上下文
    int depthBits = 24;          // 深度缓冲位数
    int stencilBits = 8;         // 模板缓冲位数
    int msaaSamples = 4;         // MSAA 采样数
    bool doubleBuffer = true;    // 双缓冲
};
```

**示例**:
```cpp
OpenGLConfig config;
config.majorVersion = 4;
config.minorVersion = 5;
config.msaaSamples = 8;  // 8x MSAA
```

---

## 初始化和清理

### Initialize

初始化 OpenGL 上下文。

```cpp
bool Initialize(const std::string& title, 
               int width, 
               int height, 
               const OpenGLConfig& config = OpenGLConfig());
```

**参数**:
- `title` - 窗口标题
- `width` - 窗口宽度
- `height` - 窗口高度
- `config` - OpenGL 配置

**返回值**: 成功返回 `true`

**示例**:
```cpp
OpenGLContext context;
OpenGLConfig config;
config.msaaSamples = 8;

if (!context.Initialize("My App", 1920, 1080, config)) {
    LOG_ERROR("Failed to initialize OpenGL context");
    return -1;
}
```

---

### Shutdown

关闭并清理上下文。

```cpp
void Shutdown();
```

---

## 窗口管理

### SwapBuffers

交换前后缓冲区（呈现）。

```cpp
void SwapBuffers();
```

**示例**:
```cpp
// 渲染...
context.SwapBuffers();
```

---

### SetVSync

设置垂直同步。

```cpp
void SetVSync(bool enable);
```

**示例**:
```cpp
context.SetVSync(true);  // 启用 VSync
```

---

### SetWindowTitle

设置窗口标题。

```cpp
void SetWindowTitle(const std::string& title);
```

---

### SetWindowSize

设置窗口大小。

```cpp
void SetWindowSize(int width, int height);
```

**功能**:
- 调整窗口尺寸
- 更新视口大小
- **自动触发所有已注册的窗口大小变化回调** ⭐ **新增**

**示例**:
```cpp
// 注册回调（例如更新相机宽高比）
context.AddResizeCallback([&camera](int w, int h) {
    camera.SetAspectRatio(static_cast<float>(w) / h);
});

// 改变窗口大小，相机会自动更新
context.SetWindowSize(1280, 720);
```

**参考**: [窗口大小变化回调](#窗口大小变化回调)

---

### SetFullscreen

设置全屏模式。

```cpp
void SetFullscreen(bool fullscreen);
```

**示例**:
```cpp
context.SetFullscreen(true);  // 全屏
```

---

## 窗口大小变化回调

⭐ **新增功能 (2025-10-31)**: OpenGLContext 现在支持观察者模式，可以注册回调以响应窗口大小变化。

### WindowResizeCallback

窗口大小变化回调函数类型。

```cpp
using WindowResizeCallback = std::function<void(int width, int height)>;
```

**参数**:
- `width` - 新的窗口宽度
- `height` - 新的窗口高度

---

### AddResizeCallback

添加窗口大小变化回调。

```cpp
void AddResizeCallback(WindowResizeCallback callback);
```

**功能**: 当窗口大小改变时（通过 `SetWindowSize` 调用），所有已注册的回调将被调用。

**线程安全**: 是

**示例**:
```cpp
// 示例 1: 更新相机宽高比
context.AddResizeCallback([&camera](int width, int height) {
    float aspectRatio = static_cast<float>(width) / height;
    camera.SetAspectRatio(aspectRatio);
    LOG_INFO("Camera aspect ratio updated: " + std::to_string(aspectRatio));
});

// 示例 2: 更新渲染目标
context.AddResizeCallback([&renderTarget](int width, int height) {
    renderTarget.Resize(width, height);
});

// 示例 3: 多个回调
context.AddResizeCallback([](int w, int h) {
    LOG_INFO("Window resized to " + std::to_string(w) + "x" + std::to_string(h));
});
```

**注意事项**:
1. 回调会在 `SetWindowSize` 方法内部调用
2. 回调执行时已经持有内部互斥锁，避免在回调中执行耗时操作
3. 如果回调抛出异常，会被捕获并记录，不会影响其他回调的执行

---

### ClearResizeCallbacks

清除所有窗口大小变化回调。

```cpp
void ClearResizeCallbacks();
```

**示例**:
```cpp
// 清除所有回调
context.ClearResizeCallbacks();
```

---

### 完整使用示例

```cpp
#include "render/opengl_context.h"
#include "render/camera.h"

// 创建上下文和相机
OpenGLContext context;
Camera camera;

// 初始化
context.Initialize("My App", 1920, 1080);
camera.SetPerspective(45.0f, 1920.0f / 1080.0f, 0.1f, 1000.0f);

// 注册回调：当窗口大小改变时自动更新相机
context.AddResizeCallback([&camera](int width, int height) {
    camera.SetAspectRatio(static_cast<float>(width) / height);
});

// 现在改变窗口大小，相机会自动更新
context.SetWindowSize(1280, 720);  // 相机宽高比自动更新为 1280/720

// 也可以清除回调
context.ClearResizeCallbacks();
```

---

## 查询方法

### GetWidth / GetHeight

获取窗口尺寸。

```cpp
int GetWidth() const;
int GetHeight() const;
```

---

### GetWindow

获取 SDL 窗口指针。

```cpp
SDL_Window* GetWindow() const;
```

---

### GetGLContext

获取 OpenGL 上下文。

```cpp
SDL_GLContext GetGLContext() const;
```

---

### GetGLVersion

获取 OpenGL 版本字符串。

```cpp
std::string GetGLVersion() const;
```

**示例**:
```cpp
std::string version = context.GetGLVersion();
LOG_INFO("OpenGL Version: " + version);
// 输出: "4.5.0 NVIDIA 531.18"
```

---

### GetGPUInfo

获取 GPU 信息。

```cpp
std::string GetGPUInfo() const;
```

**示例**:
```cpp
std::string gpu = context.GetGPUInfo();
LOG_INFO("GPU: " + gpu);
// 输出: "NVIDIA GeForce RTX 3080"
```

---

### IsExtensionSupported

检查 OpenGL 扩展是否支持。

```cpp
bool IsExtensionSupported(const std::string& extension) const;
```

**示例**:
```cpp
if (context.IsExtensionSupported("GL_ARB_direct_state_access")) {
    LOG_INFO("DSA supported");
}
```

---

### IsInitialized

检查是否已初始化。

```cpp
bool IsInitialized() const;
```

---

## 完整示例

```cpp
#include "render/opengl_context.h"
#include "render/camera.h"
#include "render/logger.h"

int main() {
    // 创建上下文
    OpenGLContext context;
    
    // 配置
    OpenGLConfig config;
    config.majorVersion = 4;
    config.minorVersion = 5;
    config.msaaSamples = 8;
    config.debugContext = true;
    
    // 初始化
    if (!context.Initialize("OpenGL App", 1920, 1080, config)) {
        LOG_ERROR("Failed to initialize OpenGL context");
        return -1;
    }
    
    // 打印信息
    LOG_INFO("OpenGL Version: " + context.GetGLVersion());
    LOG_INFO("GPU: " + context.GetGPUInfo());
    
    // 启用 VSync
    context.SetVSync(true);
    
    // 创建相机
    Camera camera;
    camera.SetPerspective(45.0f, 1920.0f / 1080.0f, 0.1f, 1000.0f);
    
    // ⭐ 新增：注册窗口大小变化回调
    context.AddResizeCallback([&camera](int width, int height) {
        camera.SetAspectRatio(static_cast<float>(width) / height);
        LOG_INFO("Window resized, camera aspect ratio updated");
    });
    
    // 主循环
    bool running = true;
    while (running) {
        // 处理事件...
        // 如果用户调整窗口大小，相机会自动更新
        
        // 渲染...
        
        // 呈现
        context.SwapBuffers();
    }
    
    // 清理
    context.Shutdown();
    
    return 0;
}
```

---

## 注意事项

1. **OpenGL 4.5+**: 引擎要求 OpenGL 4.5 或更高版本
2. **调试上下文**: 开发时启用，可获取详细错误信息
3. **VSync**: 启用可避免画面撕裂，但限制帧率
4. **MSAA**: 采样数越高，抗锯齿效果越好，但性能开销越大
5. **窗口大小变化回调** ⭐ **新增**: 
   - 使用回调机制响应窗口大小变化
   - 避免在回调中执行耗时操作
   - 回调是线程安全的
   - 适用场景：更新相机宽高比、调整渲染目标大小等

---

## 相关文档

- [Renderer API](Renderer.md)
- [GLThreadChecker API](GLThreadChecker.md) - OpenGL 线程安全检查 ⭐ **新增**
- [示例程序: 01_basic_window](../../examples/01_basic_window.cpp)
- [示例程序: 22_gl_thread_safety_test](../../examples/22_gl_thread_safety_test.cpp) - 线程安全测试 ⭐ **新增**

---

[上一篇: RenderState](RenderState.md) | [下一篇: GLThreadChecker](GLThreadChecker.md)

