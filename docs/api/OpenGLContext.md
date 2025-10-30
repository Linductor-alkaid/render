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
    
    // 主循环
    bool running = true;
    while (running) {
        // 处理事件...
        
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

---

## 相关文档

- [Renderer API](Renderer.md)
- [GLThreadChecker API](GLThreadChecker.md) - OpenGL 线程安全检查 ⭐ **新增**
- [示例程序: 01_basic_window](../../examples/01_basic_window.cpp)
- [示例程序: 22_gl_thread_safety_test](../../examples/22_gl_thread_safety_test.cpp) - 线程安全测试 ⭐ **新增**

---

[上一篇: RenderState](RenderState.md) | [下一篇: GLThreadChecker](GLThreadChecker.md)

