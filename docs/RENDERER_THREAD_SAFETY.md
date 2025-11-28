# Renderer 线程安全文档

本文档说明 `Renderer` 类的线程安全实现和使用指南。

## 概述

`Renderer` 类是渲染引擎的主要接口，提供高层渲染功能。从版本更新后，该类已全面实现线程安全，支持在多线程环境中安全使用。

## 线程安全策略

### 1. 互斥锁保护

`Renderer` 类使用 `std::mutex` 保护所有可变状态：

```cpp
class Renderer {
private:
    // ...成员变量...
    mutable std::mutex m_mutex;  // 保护所有可变状态
};
```

### 2. 原子操作

初始化标志使用原子类型，确保无锁访问：

```cpp
std::atomic<bool> m_initialized;
```

### 3. 受保护的操作

以下操作都有互斥锁保护：

#### 初始化和清理
- `Initialize()` - 初始化渲染器
- `Shutdown()` - 关闭渲染器

#### 帧管理
- `BeginFrame()` - 开始新的一帧
- `EndFrame()` - 结束当前帧
- `Present()` - 呈现渲染结果

#### 状态查询（线程安全）
- `GetDeltaTime()` - 获取帧时间
- `GetFPS()` - 获取 FPS
- `GetStats()` - 获取渲染统计（返回副本）
- `GetWidth()` / `GetHeight()` - 获取窗口尺寸

#### 设置修改
- `SetWindowTitle()` - 设置窗口标题
- `SetWindowSize()` - 设置窗口大小
- `SetVSync()` - 设置垂直同步
- `SetFullscreen()` - 设置全屏模式

#### 上下文访问
- `GetContext()` - 获取 OpenGL 上下文（有锁保护）
- `GetRenderState()` - 获取渲染状态管理器（有锁保护）

### 4. 无需额外保护的操作

以下操作委托给已经线程安全的 `RenderState` 类：

- `Clear()` - 清空缓冲区
- `SetClearColor()` - 设置清屏颜色

`RenderState` 类内部使用 `std::shared_mutex` 实现读写锁，支持多读单写。

## 使用指南

### 单线程使用（推荐）

最常见和推荐的使用方式是在主线程中使用 Renderer：

```cpp
int main() {
    Renderer* renderer = Renderer::Create();
    renderer->Initialize("My App", 1920, 1080);
    
    while (running) {
        renderer->BeginFrame();
        
        // 渲染代码
        renderer->Clear();
        // ... 绘制操作 ...
        
        renderer->EndFrame();
        renderer->Present();
    }
    
    Renderer::Destroy(renderer);
    return 0;
}
```

### 多线程使用

虽然 Renderer 是线程安全的，但需要注意以下几点：

#### 1. 查询统计信息（安全）

```cpp
// 工作线程可以安全地查询统计信息
void MonitorThread(Renderer* renderer) {
    while (running) {
        RenderStats stats = renderer->GetStats();
        float fps = renderer->GetFPS();
        
        // 记录或显示统计信息
        LogStats(stats, fps);
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
```

#### 2. 修改设置（安全但需谨慎）

```cpp
// 其他线程可以修改渲染设置
void SettingsThread(Renderer* renderer) {
    // 修改窗口设置
    renderer->SetVSync(true);
    renderer->SetClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    
    // 修改渲染状态
    auto* renderState = renderer->GetRenderState();
    renderState->SetDepthTest(true);
    renderState->SetBlendMode(BlendMode::Alpha);
}
```

#### 3. OpenGL 上下文限制 ⚠️

**重要：** OpenGL 调用必须在创建上下文的线程中执行（通常是主线程）：

```cpp
// ✅ 正确：在主线程中
void MainThread(Renderer* renderer) {
    renderer->BeginFrame();
    renderer->Clear();          // OpenGL 调用
    
    // 绘制操作...
    glDrawArrays(...);          // 直接 OpenGL 调用
    
    renderer->EndFrame();
    renderer->Present();
}

// ❌ 错误：不要在其他线程中进行 OpenGL 调用
void WorkerThread(Renderer* renderer) {
    // 获取上下文指针是安全的
    auto* context = renderer->GetContext();
    
    // 但不要在此线程调用 OpenGL 函数！
    // glDrawArrays(...);  // 会导致未定义行为
}
```

### 完整的多线程示例

```cpp
#include "render/renderer.h"
#include <thread>
#include <atomic>

std::atomic<bool> g_running(true);

// 主渲染线程
void RenderThread(Renderer* renderer) {
    while (g_running) {
        renderer->BeginFrame();
        renderer->Clear();
        
        // 所有 OpenGL 调用都在这里
        // ... 绘制代码 ...
        
        renderer->EndFrame();
        renderer->Present();
    }
}

// 监控线程
void MonitorThread(Renderer* renderer) {
    while (g_running) {
        // 安全地读取统计信息
        RenderStats stats = renderer->GetStats();
        float fps = renderer->GetFPS();
        
        std::cout << "FPS: " << fps << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// 设置线程
void SettingsThread(Renderer* renderer) {
    while (g_running) {
        // 安全地修改设置
        auto* renderState = renderer->GetRenderState();
        // 根据某些条件修改状态...
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main() {
    Renderer* renderer = Renderer::Create();
    renderer->Initialize("Multi-threaded App", 1920, 1080);
    
    // 启动工作线程
    std::thread renderThread(RenderThread, renderer);
    std::thread monitorThread(MonitorThread, renderer);
    std::thread settingsThread(SettingsThread, renderer);
    
    // 主线程做其他工作...
    
    // 等待结束
    renderThread.join();
    monitorThread.join();
    settingsThread.join();
    
    Renderer::Destroy(renderer);
    return 0;
}
```

## 性能考虑

### 锁的开销

- 互斥锁操作有一定开销，但相对于渲染操作通常可以忽略
- 对于高频率调用（如每帧调用），建议在同一线程中批量操作
- 避免在渲染循环中频繁从其他线程访问 Renderer

### 最佳实践

1. **主线程渲染**：所有 OpenGL 相关操作应在主线程（或创建上下文的线程）中执行
2. **批量更新**：如果需要从其他线程更新多个设置，考虑批量更新以减少锁竞争
3. **读取优化**：`GetStats()` 返回副本而非引用，确保线程安全但会有额外的复制开销
4. **避免死锁**：不要在持有 Renderer 锁的同时尝试获取其他锁

## 线程安全保证

### 保证

- ✅ 所有公共方法都是线程安全的
- ✅ 可以从多个线程同时调用不同的方法
- ✅ 内部状态受到保护，不会出现数据竞争
- ✅ `RenderState` 使用读写锁，支持高效的并发读取

### 不保证

- ❌ OpenGL 上下文的线程安全（这是 OpenGL 本身的限制）
- ❌ 跨线程的操作顺序（除非额外同步）
- ❌ 从 `GetContext()` 返回的指针在其他线程调用 `Shutdown()` 后仍然有效

## 测试

项目包含专门的线程安全测试程序：

```bash
# 运行 Renderer 线程安全测试
./build/bin/Release/08_renderer_thread_safe_test.exe
```

测试内容包括：
1. 多线程并发状态查询
2. 多线程并发设置修改
3. 渲染循环 + 并发查询和设置
4. 压力测试（20+ 个线程同时访问）

## 相关文档

- [RenderState 线程安全](THREAD_SAFETY.md) - RenderState 类的线程安全文档
- [RenderState 线程安全](THREAD_SAFETY.md) - RenderState 线程安全架构
- [API 文档](api/Renderer.md) - Renderer 类的 API 参考

## 更新历史

- **2025-10-28**：为 Renderer 类添加完整的线程安全支持
  - 添加互斥锁保护所有可变状态
  - 将 `m_initialized` 改为原子类型
  - 所有公共方法添加适当的锁保护
  - 创建专门的线程安全测试程序

