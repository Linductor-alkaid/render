# Renderer API 参考

[返回 API 首页](README.md)

---

## 概述

`Renderer` 类是渲染引擎的核心类，提供高层渲染接口，管理渲染上下文、状态和统计信息。

**头文件**: `render/renderer.h`  
**命名空间**: `Render`

### 🔒 线程安全

**所有公共方法都是线程安全的**，可以从多个线程安全调用。内部使用互斥锁保护所有可变状态。

⚠️ **重要限制**：虽然 `Renderer` 类本身是线程安全的，但 OpenGL 调用必须在创建上下文的线程（通常是主线程）中执行。详见 [线程安全使用指南](#线程安全)

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
RenderStats GetStats() const;
```

**返回值**: 渲染统计结构的副本（线程安全）

**🔒 线程安全**: 返回副本而非引用，确保多线程访问安全

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
RenderStats stats = renderer->GetStats();
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

**🔒 线程安全**: 获取指针本身是线程安全的，但：
- ⚠️ OpenGL 调用必须在创建上下文的线程中执行
- ⚠️ 其他线程调用 `Shutdown()` 后指针可能失效

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

**🔒 线程安全**: `RenderState` 本身是线程安全的，可以从多个线程安全调用其方法

**示例**:
```cpp
RenderState* state = renderer->GetRenderState();
state->SetDepthTest(true);
state->SetBlendMode(BlendMode::Alpha);
```

---

### GetLayerRegistry

获取渲染层级注册表。

```cpp
RenderLayerRegistry& GetLayerRegistry();
const RenderLayerRegistry& GetLayerRegistry() const;
```

**返回值**: 渲染层级注册表引用，用于注册/查询层描述。

**🔒 线程安全**: `RenderLayerRegistry` 内部使用读写锁保护，可安全在多线程环境中注册或查询层级数据。

**示例**:
```cpp
auto& layers = renderer->GetLayerRegistry();
layers.SetDefaultLayers(RenderLayerDefaults::CreateDefaultDescriptors());
layers.ResetToDefaults();
```

---

### SetActiveLayerMask / GetActiveLayerMask

设置或读取当前相机的可见层级遮罩。`FlushRenderQueue()` 会基于 `RenderLayerDescriptor::maskIndex` 与该遮罩过滤层级。

```cpp
void SetActiveLayerMask(uint32_t mask);
uint32_t GetActiveLayerMask() const;
```

**参数说明**:
- `mask` — 32bit 位掩码，每一位对应一个层级的 `maskIndex`。默认值 `0xFFFFFFFF` 表示全部可见。

**示例**:
```cpp
// 由 CameraSystem / UniformSystem 设置主相机的 layerMask
renderer->SetActiveLayerMask(cameraComp.layerMask);

// 用户手动切换成仅渲染 UI 层
if (auto desc = renderer->GetLayerRegistry().GetDescriptor(Layers::UI::Default)) {
    uint32_t uiMask = 1u << desc->maskIndex;
    renderer->SetActiveLayerMask(uiMask);
    renderer->FlushRenderQueue(); // 仅渲染 UI 层的数据
}
```

完整演示：示例 `51_layer_mask_demo` 提供键盘切换（1=世界层、2=UI层、3=全部、U=切换 UI 可见性）的可视化对比，并在日志输出 `[LayerMaskDebug]`，可验证遮罩及层级渲染状态覆写是否正常。

> 自 2025-11-10 起，`FlushRenderQueue()` 会在批处理阶段为每个 `Renderable` 重新应用所属层的覆写；`SpriteBatcher` 绘制完 UI 层后也会恢复默认 `RenderState`，避免跨层状态污染。

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

## 线程安全

### 概述

从 v1.0 版本起，`Renderer` 类已全面实现线程安全，所有公共方法都可以从多个线程安全调用。

### 保证

✅ **线程安全保证**：
- 所有公共方法都使用互斥锁保护
- 初始化状态使用原子操作
- 可以从多个线程同时调用不同方法
- 不会出现数据竞争

### 限制

⚠️ **OpenGL 限制**：
- OpenGL 调用必须在创建上下文的线程（通常是主线程）中执行
- 这是 OpenGL 的固有限制，不是 `Renderer` 的限制

### 最佳实践

#### 单线程使用（推荐）

最简单和推荐的方式是在主线程中使用 Renderer：

```cpp
int main() {
    Renderer* renderer = Renderer::Create();
    renderer->Initialize("My App", 1280, 720);
    
    while (running) {
        renderer->BeginFrame();
        // 所有渲染代码在主线程
        renderer->Clear();
        // ... 绘制操作 ...
        renderer->EndFrame();
        renderer->Present();
    }
    
    Renderer::Destroy(renderer);
    return 0;
}
```

#### 多线程查询（安全）

可以从其他线程安全地查询统计信息：

```cpp
// 监控线程
void MonitorThread(Renderer* renderer) {
    while (running) {
        // ✅ 安全：查询统计信息
        RenderStats stats = renderer->GetStats();
        float fps = renderer->GetFPS();
        
        LOG_INFO("FPS: " + std::to_string(fps));
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// 主线程渲染
int main() {
    Renderer* renderer = Renderer::Create();
    renderer->Initialize("Multi-threaded App", 1280, 720);
    
    // 启动监控线程
    std::thread monitor(MonitorThread, renderer);
    
    // 主线程渲染循环
    while (running) {
        renderer->BeginFrame();
        renderer->Clear();
        // ... 渲染 ...
        renderer->EndFrame();
        renderer->Present();
    }
    
    running = false;
    monitor.join();
    Renderer::Destroy(renderer);
    return 0;
}
```

#### 多线程设置修改（谨慎）

可以从其他线程修改设置，但需谨慎：

```cpp
// 设置线程
void SettingsThread(Renderer* renderer) {
    while (running) {
        // ✅ 安全：修改渲染设置
        renderer->SetClearColor(r, g, b, 1.0f);
        
        // ✅ 安全：RenderState 本身是线程安全的
        auto* renderState = renderer->GetRenderState();
        renderState->SetDepthTest(true);
        renderState->SetBlendMode(BlendMode::Alpha);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
```

#### 禁止的操作 ❌

```cpp
// ❌ 错误：不要在非主线程中进行 OpenGL 调用
void WorkerThread(Renderer* renderer) {
    auto* context = renderer->GetContext();  // ✅ 获取指针安全
    
    // ❌ 错误：在非主线程中调用 OpenGL
    glDrawArrays(...);  // 会导致未定义行为
}
```

### 测试

项目包含专门的线程安全测试：

```bash
# 运行 Renderer 线程安全测试
./build/bin/Release/08_renderer_thread_safe_test.exe
```

测试覆盖：
- 多线程并发状态查询
- 多线程并发设置修改
- 渲染循环 + 并发操作
- 压力测试（20+ 个线程）

### 更多信息

详细的线程安全使用指南，请参阅：
- [Renderer 线程安全文档](../RENDERER_THREAD_SAFETY.md)
- [RenderState 线程安全](RenderState.md#线程安全)
- [线程安全总结](../THREAD_SAFETY_SUMMARY.md)

---

## 注意事项

1. **线程安全**: 所有方法都是线程安全的，但 OpenGL 调用必须在主线程中执行
2. **单例模式**: 虽然可以创建多个 `Renderer` 实例，但通常只需要一个
3. **初始化顺序**: 必须先调用 `Create()` 再调用 `Initialize()`
4. **清理顺序**: 确保在销毁渲染器前清理所有 OpenGL 资源
5. **帧循环**: `BeginFrame()` → 渲染 → `EndFrame()` → `Present()` 的顺序不能打乱
6. **性能**: 使用 `GetStats()` 监控渲染性能
7. **多线程**: 查询和设置可以多线程，但实际渲染应在主线程

---

## 相关文档

### API 文档
- [OpenGLContext API](OpenGLContext.md)
- [RenderState API](RenderState.md)

### 示例程序
- [基础窗口示例](../../examples/01_basic_window.cpp)
- [线程安全测试](../../examples/08_renderer_thread_safe_test.cpp)

### 线程安全
- [Renderer 线程安全指南](../RENDERER_THREAD_SAFETY.md)
- [RenderState 线程安全](../THREAD_SAFETY.md)
- [整体线程安全总结](../THREAD_SAFETY_SUMMARY.md)

---

[上一篇: API 首页](README.md) | [下一篇: Shader](Shader.md)

