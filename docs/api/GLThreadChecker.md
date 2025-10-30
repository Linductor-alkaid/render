# GLThreadChecker API 参考

[返回 API 首页](README.md)

---

## 概述

`GLThreadChecker` 是一个单例类，用于确保所有 OpenGL 调用都在创建上下文的线程中执行。OpenGL 要求所有的 API 调用必须在创建上下文的线程中进行，违反这一规则会导致未定义行为或程序崩溃。

**头文件**: `render/gl_thread_checker.h`  
**命名空间**: `Render`  
**类型**: 单例类

---

## 核心功能

- ✅ **自动检测线程错误** - 在任何 OpenGL 调用前自动检查线程
- ✅ **详细错误信息** - 提供文件名、行号、函数名和线程 ID
- ✅ **灵活配置** - 可选择记录日志或终止程序
- ✅ **编译时可禁用** - Release 模式下可禁用以提高性能
- ✅ **零侵入集成** - 通过宏自动集成到现有代码

---

## 主要方法

### GetInstance

获取单例实例。

```cpp
static GLThreadChecker& GetInstance();
```

**返回值**: `GLThreadChecker` 单例引用

**示例**:
```cpp
auto& checker = GLThreadChecker::GetInstance();
```

---

### RegisterGLThread

注册 OpenGL 上下文线程。应该在 OpenGL 上下文创建成功后立即调用。

```cpp
void RegisterGLThread();
```

**注意**: 
- 通常由 `OpenGLContext::Initialize()` 自动调用
- 如果在不同线程重复注册会触发错误

**示例**:
```cpp
// 通常不需要手动调用，OpenGLContext 会自动处理
GL_THREAD_REGISTER();
```

---

### UnregisterGLThread

注销 OpenGL 上下文线程。应该在 OpenGL 上下文销毁时调用。

```cpp
void UnregisterGLThread();
```

**注意**: 
- 通常由 `OpenGLContext::Shutdown()` 自动调用
- 必须在注册的线程中调用

**示例**:
```cpp
// 通常不需要手动调用，OpenGLContext 会自动处理
GL_THREAD_UNREGISTER();
```

---

### IsGLThread

检查当前线程是否是注册的 OpenGL 线程。

```cpp
bool IsGLThread() const;
```

**返回值**: 如果当前线程是 OpenGL 线程返回 `true`，否则返回 `false`

**示例**:
```cpp
if (GLThreadChecker::GetInstance().IsGLThread()) {
    // 可以安全地调用 OpenGL
    glClear(GL_COLOR_BUFFER_BIT);
}
```

---

### ValidateGLThread

验证当前线程是否是 OpenGL 线程，如果不是则记录错误。

```cpp
bool ValidateGLThread(const char* file, int line, const char* function = nullptr) const;
```

**参数**:
- `file` - 调用文件名（通常使用 `__FILE__`）
- `line` - 调用行号（通常使用 `__LINE__`）
- `function` - 调用函数名（可选，通常使用 `__FUNCTION__`）

**返回值**: 如果是正确的线程返回 `true`，否则返回 `false`

**注意**: 通常不直接调用，而是使用 `GL_THREAD_CHECK()` 宏

---

### SetTerminateOnError / GetTerminateOnError

设置或获取错误时是否终止程序的选项。

```cpp
void SetTerminateOnError(bool terminate);
bool GetTerminateOnError() const;
```

**参数**:
- `terminate` - 如果为 `true`，检测到线程错误时会调用 `std::terminate()`

**默认值**: `true`（在调试模式下建议保持启用）

**示例**:
```cpp
// 在测试时临时禁用终止，以便观察错误日志
auto& checker = GLThreadChecker::GetInstance();
checker.SetTerminateOnError(false);

// 进行测试...

// 恢复设置
checker.SetTerminateOnError(true);
```

---

### IsRegistered

检查是否已经注册了 OpenGL 线程。

```cpp
bool IsRegistered() const;
```

**返回值**: 如果已注册返回 `true`

---

### GetGLThreadId

获取注册的 OpenGL 线程 ID（用于调试）。

```cpp
std::thread::id GetGLThreadId() const;
```

**返回值**: OpenGL 线程的 `std::thread::id`

---

## 便捷宏

### GL_THREAD_CHECK

在任何 OpenGL 调用前使用此宏进行线程检查。

```cpp
GL_THREAD_CHECK();
```

**示例**:
```cpp
void MyRenderFunction() {
    GL_THREAD_CHECK();  // 检查线程
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
```

**编译选项**:
- **Debug 模式**: 默认启用，进行完整的线程检查
- **Release 模式**: 可通过定义 `GL_DISABLE_THREAD_CHECK` 来禁用检查以提高性能

```cpp
// CMakeLists.txt 中禁用 Release 模式的线程检查
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(MyTarget PRIVATE GL_DISABLE_THREAD_CHECK)
endif()
```

---

### GL_THREAD_REGISTER

注册 OpenGL 线程的宏（在上下文初始化后调用）。

```cpp
GL_THREAD_REGISTER();
```

**注意**: 通常不需要手动调用，`OpenGLContext::Initialize()` 会自动处理

---

### GL_THREAD_UNREGISTER

注销 OpenGL 线程的宏（在上下文销毁前调用）。

```cpp
GL_THREAD_UNREGISTER();
```

**注意**: 通常不需要手动调用，`OpenGLContext::Shutdown()` 会自动处理

---

## 使用示例

### 基本使用（自动）

最简单的方式是使用框架的自动管理：

```cpp
#include "render/renderer.h"

int main() {
    // 创建渲染器
    auto renderer = Renderer::Create();
    
    // 初始化（自动注册 OpenGL 线程）
    renderer->Initialize("My App", 1920, 1080);
    
    // 所有 OpenGL 调用会自动检查线程
    renderer->BeginFrame();
    renderer->Clear();
    renderer->EndFrame();
    renderer->Present();
    
    // 清理（自动注销 OpenGL 线程）
    Renderer::Destroy(renderer);
    
    return 0;
}
```

---

### 手动添加线程检查

在自定义的 OpenGL 调用中添加检查：

```cpp
void MyCustomRenderFunction() {
    GL_THREAD_CHECK();  // 添加线程检查
    
    // OpenGL 调用
    glBindFramebuffer(GL_FRAMEBUFFER, myFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
```

---

### 多线程场景

正确的多线程使用方式：

```cpp
#include "render/renderer.h"
#include <thread>

// ❌ 错误：在其他线程调用 OpenGL
void WrongThreadFunction(Renderer* renderer) {
    // 这会触发错误！
    renderer->Clear();  // GL_THREAD_CHECK() 会检测到错误
}

// ✅ 正确：通过消息队列通知主线程
struct RenderCommand {
    // 渲染命令数据
};

std::queue<RenderCommand> renderQueue;
std::mutex queueMutex;

void WorkerThread() {
    // 在工作线程中准备数据
    RenderCommand cmd;
    // ... 准备命令 ...
    
    // 将命令加入队列
    std::lock_guard<std::mutex> lock(queueMutex);
    renderQueue.push(cmd);
}

void MainRenderLoop(Renderer* renderer) {
    // 在主线程（OpenGL 线程）中执行渲染
    while (running) {
        // 处理渲染命令队列
        std::lock_guard<std::mutex> lock(queueMutex);
        while (!renderQueue.empty()) {
            auto cmd = renderQueue.front();
            renderQueue.pop();
            
            // 在 OpenGL 线程中执行
            ExecuteRenderCommand(cmd);  // 这里可以安全调用 OpenGL
        }
        
        renderer->BeginFrame();
        renderer->Clear();
        // ... 渲染 ...
        renderer->EndFrame();
        renderer->Present();
    }
}
```

---

### 调试模式和发布模式

```cpp
// 在调试模式下进行严格检查
#ifdef _DEBUG
    auto& checker = GLThreadChecker::GetInstance();
    checker.SetTerminateOnError(true);  // 遇到错误立即终止
#else
    // 在发布模式下可以只记录日志
    auto& checker = GLThreadChecker::GetInstance();
    checker.SetTerminateOnError(false);  // 只记录日志，不终止
#endif
```

---

### 测试线程安全性

```cpp
#include "render/gl_thread_checker.h"
#include <thread>

void TestThreadSafety() {
    auto& checker = GLThreadChecker::GetInstance();
    
    // 临时禁用终止以便观察错误
    bool originalSetting = checker.GetTerminateOnError();
    checker.SetTerminateOnError(false);
    
    // 创建子线程尝试调用 OpenGL（会触发错误）
    std::thread testThread([&]() {
        if (!checker.IsGLThread()) {
            LOG_WARNING("Current thread is NOT the OpenGL thread");
        }
        
        // 这会触发错误日志
        GL_THREAD_CHECK();
    });
    
    testThread.join();
    
    // 恢复原始设置
    checker.SetTerminateOnError(originalSetting);
}
```

---

## 错误消息示例

当在错误的线程中调用 OpenGL 时，会产生如下错误消息：

```
[ERROR] OpenGL call from wrong thread!
  Expected thread ID: 2336
  Current thread ID:  12812
  Location: G:\myproject\render\src\core\opengl_context.cpp:166 in GetGLVersion()
```

---

## 性能考虑

### Debug 模式
- **完整检查**: 每次 OpenGL 调用都会检查线程
- **开销**: 极小（仅一次线程 ID 比较）
- **建议**: 始终启用以发现潜在问题

### Release 模式
- **可选禁用**: 定义 `GL_DISABLE_THREAD_CHECK` 可完全禁用检查
- **开销**: 禁用后为零开销（宏展开为空）
- **建议**: 
  - 开发阶段：保持启用
  - 性能关键场景：可以禁用
  - 如果不确定：建议保持启用（开销极小）

---

## 注意事项

### ⚠️ 重要规则

1. **单一 OpenGL 线程**: OpenGL 上下文只能在一个线程中使用
2. **自动管理**: 使用 `Renderer` 或 `OpenGLContext` 时会自动处理注册
3. **不要手动注册**: 除非你明确知道自己在做什么
4. **错误即 Bug**: 如果触发线程检查错误，说明代码存在严重的线程安全问题

### ⚠️ 限制

1. **不支持多上下文**: 当前实现假设只有一个 OpenGL 上下文
2. **不支持上下文共享**: 如果使用共享上下文，需要扩展实现
3. **不检查 SDL 调用**: 只检查 OpenGL API，不检查 SDL 的窗口操作

### ⚠️ 最佳实践

1. ✅ **主线程渲染**: 始终在主线程中进行 OpenGL 调用
2. ✅ **数据准备**: 在工作线程中准备数据，在主线程中上传
3. ✅ **命令模式**: 使用命令队列在线程间传递渲染指令
4. ✅ **启用检查**: 至少在开发阶段启用线程检查
5. ❌ **避免跨线程**: 不要在多个线程间传递 OpenGL 对象句柄

---

## 集成位置

框架已在以下位置集成了线程检查：

### 核心模块
- ✅ `OpenGLContext` - 所有窗口和上下文操作
- ✅ `RenderState` - 所有状态管理和 OpenGL 状态设置
- ✅ `Renderer` - 渲染循环管理

### 资源模块
- ✅ `Shader` - 着色器编译、链接和使用
- ✅ `Texture` - 纹理创建和操作
- ✅ `Mesh` - 网格上传和绘制
- ✅ `UniformManager` - Uniform 变量设置

### 自定义代码
如果你添加了自定义的 OpenGL 调用，请确保添加 `GL_THREAD_CHECK()`：

```cpp
void MyCustomGLFunction() {
    GL_THREAD_CHECK();  // 添加这一行
    
    // 你的 OpenGL 调用
    glBindTexture(GL_TEXTURE_2D, textureId);
    // ...
}
```

---

## 相关文档

- [Renderer API](Renderer.md) - 主渲染器（已集成线程检查）
- [OpenGLContext API](OpenGLContext.md) - OpenGL 上下文管理（自动注册）
- [示例程序: 22_gl_thread_safety_test](../../examples/22_gl_thread_safety_test.cpp) - 完整的测试示例

---

## 常见问题

### Q: 为什么需要线程检查？
A: OpenGL 是线程不安全的，所有调用必须在创建上下文的线程中执行。违反这一规则会导致崩溃或未定义行为。

### Q: 性能影响有多大？
A: 极小。每次检查只是一次线程 ID 比较（几纳秒级别）。如果仍担心性能，可以在 Release 模式下禁用。

### Q: 可以在多个线程使用 OpenGL 吗？
A: 不推荐。虽然可以创建多个上下文并在不同线程使用，但会带来复杂的同步问题。建议使用命令模式。

### Q: 检查失败会怎样？
A: 默认情况下会记录详细的错误日志并终止程序（在 Debug 模式下）。可以配置为仅记录日志。

### Q: 如何在自定义代码中添加检查？
A: 只需在任何 OpenGL 调用前添加 `GL_THREAD_CHECK();` 宏即可。

---

[上一篇: OpenGLContext](OpenGLContext.md) | [下一篇: Logger](Logger.md)

