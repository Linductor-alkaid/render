# Renderer API 参考

[返回 API 首页](README.md)

---

## 概述

`Renderer` 类是渲染引擎的核心类，提供高层渲染接口，管理渲染上下文、状态和统计信息。

**头文件**: `render/renderer.h`  
**命名空间**: `Render`

---

## 类定义

```cpp
class Renderer {
public:
    static Renderer* Create();
    static void Destroy(Renderer* renderer);
    
    bool Initialize(const std::string& title = "RenderEngine", 
                   int width = 1920, 
                   int height = 1080);
    void Shutdown();
    
    void BeginFrame();
    void EndFrame();
    void Present();
    void Clear(bool colorBuffer = true, bool depthBuffer = true, bool stencilBuffer = false);
    
    // ... 更多方法见下文
};
```

---

## 静态方法

### Create

创建渲染器实例。

```cpp
static Renderer* Create();
```

**返回值**: 渲染器指针，失败返回 `nullptr`

**示例**:
```cpp
Renderer* renderer = Renderer::Create();
if (!renderer) {
    LOG_ERROR("Failed to create renderer");
    return -1;
}
```

---

### Destroy

销毁渲染器实例。

```cpp
static void Destroy(Renderer* renderer);
```

**参数**:
- `renderer` - 要销毁的渲染器指针

**示例**:
```cpp
Renderer::Destroy(renderer);
renderer = nullptr;
```

---

## 初始化和清理

### Initialize

初始化渲染器，创建窗口和 OpenGL 上下文。

```cpp
bool Initialize(const std::string& title = "RenderEngine", 
               int width = 1920, 
               int height = 1080);
```

**参数**:
- `title` - 窗口标题
- `width` - 窗口宽度（像素）
- `height` - 窗口高度（像素）

**返回值**: 成功返回 `true`，失败返回 `false`

**示例**:
```cpp
if (!renderer->Initialize("My Game", 1280, 720)) {
    LOG_ERROR("Failed to initialize renderer");
    return -1;
}
```

---

### Shutdown

关闭渲染器，释放所有资源。

```cpp
void Shutdown();
```

**示例**:
```cpp
renderer->Shutdown();
```

---

## 渲染循环

### BeginFrame

开始新的一帧渲染。

```cpp
void BeginFrame();
```

**说明**: 更新时间统计，准备新的渲染帧。

**示例**:
```cpp
while (running) {
    renderer->BeginFrame();
    
    // 渲染代码...
    
    renderer->EndFrame();
    renderer->Present();
}
```

---

### EndFrame

结束当前帧渲染。

```cpp
void EndFrame();
```

**说明**: 更新帧统计信息（FPS、帧时间等）。

---

### Present

呈现渲染结果到屏幕。

```cpp
void Present();
```

**说明**: 交换前后缓冲区，显示渲染内容。

---

### Clear

清空缓冲区。

```cpp
void Clear(bool colorBuffer = true, 
          bool depthBuffer = true, 
          bool stencilBuffer = false);
```

**参数**:
- `colorBuffer` - 是否清空颜色缓冲区
- `depthBuffer` - 是否清空深度缓冲区
- `stencilBuffer` - 是否清空模板缓冲区

**示例**:
```cpp
// 清空颜色和深度缓冲区
renderer->Clear();

// 只清空颜色缓冲区
renderer->Clear(true, false, false);
```

---

## 渲染设置

### SetClearColor

设置清屏颜色。

```cpp
// 方式 1: 使用 Color 对象
void SetClearColor(const Color& color);

// 方式 2: 使用分量
void SetClearColor(float r, float g, float b, float a = 1.0f);
```

**参数**:
- `color` - 颜色对象
- `r, g, b, a` - 红、绿、蓝、透明度分量（0.0~1.0）

**示例**:
```cpp
// 使用 Color 对象
renderer->SetClearColor(Color(0.1f, 0.1f, 0.15f, 1.0f));

// 使用分量
renderer->SetClearColor(0.1f, 0.1f, 0.15f, 1.0f);

// 动态改变清屏颜色
float time = SDL_GetTicks() / 1000.0f;
float r = 0.5f + 0.5f * std::sin(time);
renderer->SetClearColor(r, 0.2f, 0.2f, 1.0f);
```

---

## 窗口管理

### SetWindowTitle

设置窗口标题。

```cpp
void SetWindowTitle(const std::string& title);
```

**参数**:
- `title` - 窗口标题

**示例**:
```cpp
// 显示 FPS
std::string title = "My Game | FPS: " + std::to_string(static_cast<int>(renderer->GetFPS()));
renderer->SetWindowTitle(title);
```

---

### SetWindowSize

设置窗口大小。

```cpp
void SetWindowSize(int width, int height);
```

**参数**:
- `width` - 宽度（像素）
- `height` - 高度（像素）

**示例**:
```cpp
renderer->SetWindowSize(1920, 1080);
```

---

### SetVSync

设置垂直同步。

```cpp
void SetVSync(bool enable);
```

**参数**:
- `enable` - `true` 启用 VSync，`false` 禁用

**示例**:
```cpp
renderer->SetVSync(true);  // 启用 VSync，限制帧率
```

---

### SetFullscreen

设置全屏模式。

```cpp
void SetFullscreen(bool fullscreen);
```

**参数**:
- `fullscreen` - `true` 全屏，`false` 窗口模式

**示例**:
```cpp
// 切换全屏
bool isFullscreen = false;
if (keyPressed(KEY_F11)) {
    isFullscreen = !isFullscreen;
    renderer->SetFullscreen(isFullscreen);
}
```

---

## 查询方法

### GetWidth

获取窗口宽度。

```cpp
int GetWidth() const;
```

**返回值**: 窗口宽度（像素）

---

### GetHeight

获取窗口高度。

```cpp
int GetHeight() const;
```

**返回值**: 窗口高度（像素）

**示例**:
```cpp
float aspectRatio = static_cast<float>(renderer->GetWidth()) / renderer->GetHeight();
```

---

### GetDeltaTime

获取帧时间间隔。

```cpp
float GetDeltaTime() const;
```

**返回值**: 自上一帧以来的时间（秒）

**示例**:
```cpp
float deltaTime = renderer->GetDeltaTime();
rotation += rotationSpeed * deltaTime;  // 时间相关的旋转
```

---

### GetFPS

获取当前帧率。

```cpp
float GetFPS() const;
```

**返回值**: 当前 FPS

**示例**:
```cpp
float fps = renderer->GetFPS();
LOG_INFO("Current FPS: " + std::to_string(fps));
```

---

### GetStats

获取渲染统计信息。

```cpp
const RenderStats& GetStats() const;
```

**返回值**: 渲染统计结构

**RenderStats 结构**:
```cpp
struct RenderStats {
    uint32_t drawCalls;    // 绘制调用次数
    uint32_t triangles;    // 三角形数量
    uint32_t vertices;     // 顶点数量
    float frameTime;       // 帧时间（毫秒）
    float fps;             // 帧率
};
```

**示例**:
```cpp
const RenderStats& stats = renderer->GetStats();
LOG_INFO("Draw Calls: " + std::to_string(stats.drawCalls));
LOG_INFO("Triangles: " + std::to_string(stats.triangles));
```

---

### GetContext

获取 OpenGL 上下文。

```cpp
OpenGLContext* GetContext();
```

**返回值**: OpenGL 上下文指针

**示例**:
```cpp
OpenGLContext* context = renderer->GetContext();
std::string glVersion = context->GetGLVersion();
```

---

### GetRenderState

获取渲染状态管理器。

```cpp
RenderState* GetRenderState();
```

**返回值**: 渲染状态管理器指针

**示例**:
```cpp
RenderState* state = renderer->GetRenderState();
state->SetDepthTest(true);
state->SetBlendMode(BlendMode::Alpha);
```

---

### IsInitialized

检查是否已初始化。

```cpp
bool IsInitialized() const;
```

**返回值**: 已初始化返回 `true`

---

## 完整示例

### 基础渲染循环

```cpp
#include "render/renderer.h"
#include "render/logger.h"
#include <SDL3/SDL.h>

int main() {
    // 初始化日志
    Logger::GetInstance().SetLogToConsole(true);
    Logger::GetInstance().SetLogToFile(true);
    
    // 创建渲染器
    Renderer* renderer = Renderer::Create();
    if (!renderer) {
        LOG_ERROR("Failed to create renderer");
        return -1;
    }
    
    // 初始化
    if (!renderer->Initialize("My Application", 1280, 720)) {
        LOG_ERROR("Failed to initialize renderer");
        Renderer::Destroy(renderer);
        return -1;
    }
    
    // 设置 VSync
    renderer->SetVSync(true);
    
    // 主循环
    bool running = true;
    while (running) {
        // 事件处理
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }
        
        // 开始帧
        renderer->BeginFrame();
        
        // 设置清屏颜色
        renderer->SetClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        renderer->Clear();
        
        // 这里添加渲染代码...
        
        // 结束帧
        renderer->EndFrame();
        renderer->Present();
        
        // 更新标题显示 FPS
        static float titleUpdateTimer = 0.0f;
        titleUpdateTimer += renderer->GetDeltaTime();
        if (titleUpdateTimer >= 1.0f) {
            std::string title = "My Application | FPS: " + 
                              std::to_string(static_cast<int>(renderer->GetFPS()));
            renderer->SetWindowTitle(title);
            titleUpdateTimer = 0.0f;
        }
    }
    
    // 清理
    Renderer::Destroy(renderer);
    
    return 0;
}
```

---

### 使用渲染状态

```cpp
// 获取渲染状态管理器
RenderState* state = renderer->GetRenderState();

// 启用深度测试
state->SetDepthTest(true);
state->SetDepthFunc(DepthFunc::Less);

// 启用混合
state->SetBlendMode(BlendMode::Alpha);

// 启用面剔除
state->SetCullFace(CullFace::Back);

// 设置视口
state->SetViewport(0, 0, renderer->GetWidth(), renderer->GetHeight());
```

---

## 注意事项

1. **单例模式**: 虽然可以创建多个 `Renderer` 实例，但通常只需要一个
2. **初始化顺序**: 必须先调用 `Create()` 再调用 `Initialize()`
3. **清理顺序**: 确保在销毁渲染器前清理所有 OpenGL 资源
4. **帧循环**: `BeginFrame()` → 渲染 → `EndFrame()` → `Present()` 的顺序不能打乱
5. **性能**: 使用 `GetStats()` 监控渲染性能

---

## 相关文档

- [OpenGLContext API](OpenGLContext.md)
- [RenderState API](RenderState.md)
- [示例程序: 01_basic_window](../../examples/01_basic_window.cpp)

---

[上一篇: API 首页](README.md) | [下一篇: OpenGLContext](OpenGLContext.md)

