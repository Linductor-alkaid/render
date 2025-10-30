# Logger API 参考

[返回 API 首页](README.md)

---

## 概述

`Logger` 提供线程安全的日志记录功能，支持控制台和文件输出、格式化日志、彩色输出、线程ID显示、日志回调等高级特性。

**头文件**: `render/logger.h`  
**命名空间**: `Render`

### 主要特性

- ✅ **完全线程安全** - 使用原子操作和互斥锁保证多线程环境下的安全性
- ✅ **异步日志队列** - 高性能异步写入，性能提升近10倍（默认启用）
- ✅ **格式化日志** - 支持类似 printf 的格式化输出
- ✅ **彩色输出** - 控制台输出支持 ANSI 颜色代码
- ✅ **线程ID显示** - 可选显示日志来源线程
- ✅ **源文件位置** - 可选显示源文件名和行号
- ✅ **日志回调** - 支持自定义回调函数处理日志
- ✅ **文件轮转** - 支持按文件大小自动轮转，无日志丢失
- ✅ **向后兼容** - 保留所有旧版API

---

## 日志级别

```cpp
enum class LogLevel {
    Debug,      // 调试信息（青色）
    Info,       // 一般信息（绿色）
    Warning,    // 警告（黄色）
    Error       // 错误（红色）
};
```

---

## 单例访问

```cpp
Logger& logger = Logger::GetInstance();
```

---

## 基本配置

### SetLogLevel

设置日志级别，低于此级别的日志将被忽略。

```cpp
void SetLogLevel(LogLevel level);
```

**示例**:
```cpp
logger.SetLogLevel(LogLevel::Info);  // 只记录 Info 及以上
```

**线程安全**: ✅ 使用原子操作，无需加锁

---

### SetLogToConsole

启用/禁用控制台输出。

```cpp
void SetLogToConsole(bool enable);
```

**示例**:
```cpp
logger.SetLogToConsole(true);  // 启用控制台输出
```

**线程安全**: ✅ 使用原子操作

---

### SetLogToFile

启用/禁用文件输出。

```cpp
void SetLogToFile(bool enable, const std::string& filename = "");
```

**参数**:
- `enable`: 是否启用文件输出
- `filename`: 日志文件名（可选），留空则自动生成时间戳命名

**示例**:
```cpp
// 自动命名（例如：logs/render_20231030_143025.log）
logger.SetLogToFile(true);

// 指定文件名
logger.SetLogToFile(true, "myapp.log");
```

**线程安全**: ✅ 使用互斥锁保护

---

### SetLogDirectory

设置日志文件目录。

```cpp
void SetLogDirectory(const std::string& directory);
```

**示例**:
```cpp
logger.SetLogDirectory("logs");  // 默认值
```

**线程安全**: ✅ 使用互斥锁保护

---

## 高级配置

### SetColorOutput

启用/禁用控制台彩色输出。

```cpp
void SetColorOutput(bool enable);
```

**示例**:
```cpp
logger.SetColorOutput(true);  // 默认启用
```

**颜色方案**:
- DEBUG: 青色
- INFO: 绿色
- WARNING: 黄色
- ERROR: 红色

**线程安全**: ✅ 使用原子操作

---

### SetShowThreadId

启用/禁用线程ID显示。

```cpp
void SetShowThreadId(bool enable);
```

**示例**:
```cpp
logger.SetShowThreadId(true);  // 显示线程ID
// 输出: [2023-10-30 14:30:25.123] [TID:12345] [INFO] 消息
```

**线程安全**: ✅ 使用原子操作

---

### SetMaxFileSize

设置日志文件最大大小，超过后自动轮转。

```cpp
void SetMaxFileSize(size_t maxSize);
```

**参数**:
- `maxSize`: 最大字节数，0 表示不限制（默认）

**示例**:
```cpp
logger.SetMaxFileSize(10 * 1024 * 1024);  // 10MB
```

**注意**: 轮转时会创建新的时间戳命名的日志文件

**线程安全**: ✅ 使用原子操作

---

### SetLogCallback

设置日志回调函数。

```cpp
void SetLogCallback(LogCallback callback);

// 回调函数类型
using LogCallback = std::function<void(LogLevel level, const std::string& message)>;
```

**示例**:
```cpp
logger.SetLogCallback([](LogLevel level, const std::string& message) {
    if (level == LogLevel::Error) {
        // 发送错误告警
        SendAlert(message);
    }
    // 可以发送到远程服务器、数据库等
});

// 取消回调
logger.SetLogCallback(nullptr);
```

**线程安全**: ✅ 使用互斥锁保护，回调异常不影响日志系统

---

### SetAsyncLogging

启用/禁用异步日志模式（默认启用）。

```cpp
void SetAsyncLogging(bool enable);
```

**参数**:
- `enable`: true 启用异步（默认），false 禁用（同步模式）

**示例**:
```cpp
// 启用异步（默认）- 高性能，低延迟
logger.SetAsyncLogging(true);

// 禁用异步 - 适用于需要立即写入的场景
logger.SetAsyncLogging(false);
```

**工作原理**:
- **异步模式**: 日志消息放入队列，后台线程批量写入
- **同步模式**: 日志消息立即写入（阻塞调用线程）

**性能对比**:
- 异步模式: ~347,000 条/秒
- 同步模式: ~37,000 条/秒
- **性能提升**: **9.37倍**

**注意**: 更改此设置时会自动刷新队列

**线程安全**: ✅ 使用原子操作

---

### Flush

刷新日志队列，确保所有日志都已写入。

```cpp
void Flush();
```

**使用场景**:
- 程序即将退出
- 需要确保日志已写入
- 性能测试前后

**示例**:
```cpp
LOG_INFO("Critical operation completed");
logger.Flush();  // 确保日志已写入

// 性能测试
auto start = std::chrono::high_resolution_clock::now();
// ... 执行操作 ...
logger.Flush();  // 刷新队列
auto end = std::chrono::high_resolution_clock::now();
```

**注意**: 
- 仅在异步模式下有效
- 会阻塞直到队列为空
- 同步模式下此方法立即返回

**线程安全**: ✅ 完全线程安全

---

### GetQueueSize

获取当前队列中的日志数量。

```cpp
size_t GetQueueSize() const;
```

**返回值**: 队列中待处理的日志消息数量

**示例**:
```cpp
size_t pending = logger.GetQueueSize();
if (pending > 1000) {
    LOG_WARNING_F("日志队列堆积: %zu 条消息", pending);
}
```

**用途**:
- 监控日志系统负载
- 调试性能问题
- 判断是否需要调整日志级别

**线程安全**: ✅ 使用互斥锁保护

---

## 基本日志方法

### Log / Debug / Info / Warning / Error

记录日志消息。

```cpp
void Log(LogLevel level, const std::string& message);
void Debug(const std::string& message);
void Info(const std::string& message);
void Warning(const std::string& message);
void Error(const std::string& message);
```

**示例**:
```cpp
logger.Debug("Rendering frame " + std::to_string(frameCount));
logger.Info("Texture loaded successfully");
logger.Warning("Low memory: " + std::to_string(availMem) + " MB");
logger.Error("Failed to initialize OpenGL context");
```

**宏版本**（推荐）:
```cpp
LOG_DEBUG("Debug message");
LOG_INFO("Initialization complete");
LOG_WARNING("Texture not found, using default");
LOG_ERROR("Failed to load shader");
```

**线程安全**: ✅ 完全线程安全

---

## 格式化日志方法

### LogFormat / DebugFormat / InfoFormat / WarningFormat / ErrorFormat

支持类似 printf 的格式化输出。

```cpp
void LogFormat(LogLevel level, const char* format, ...);
void DebugFormat(const char* format, ...);
void InfoFormat(const char* format, ...);
void WarningFormat(const char* format, ...);
void ErrorFormat(const char* format, ...);
```

**示例**:
```cpp
int x = 42;
float y = 3.14159f;
logger.InfoFormat("Position: x=%d, y=%.2f", x, y);
logger.WarningFormat("Memory usage: %.1f%%", 75.5);
logger.ErrorFormat("Failed to load '%s', code: %d", filename.c_str(), errorCode);
```

**宏版本**（推荐）:
```cpp
LOG_DEBUG_F("Rendering frame #%d", frameCount);
LOG_INFO_F("Loaded %d textures in %.2f ms", count, time);
LOG_WARNING_F("Shader compilation took %d ms", duration);
LOG_ERROR_F("Out of memory: need %zu bytes", size);
```

**支持的格式说明符**: 
- 整数: `%d`, `%i`, `%u`, `%x`, `%X`
- 浮点: `%f`, `%F`, `%e`, `%E`, `%g`, `%G`
- 字符串: `%s`
- 字符: `%c`
- 指针: `%p`

**线程安全**: ✅ 完全线程安全

---

## 带位置信息的日志

### LogWithLocation

记录日志并包含源文件位置信息。

```cpp
void LogWithLocation(LogLevel level, const char* file, int line, 
                     const std::string& message);
```

**宏版本**（推荐）:
```cpp
LOG_DEBUG_LOC("Debug message with location");
LOG_INFO_LOC("Info message with location");
LOG_WARNING_LOC("Warning message with location");
LOG_ERROR_LOC("Error message with location");
```

**输出示例**:
```
[2023-10-30 14:30:25.123] [DEBUG] [main.cpp:42] Debug message with location
```

**线程安全**: ✅ 完全线程安全

---

## 查询方法

### GetCurrentLogFile

获取当前日志文件路径。

```cpp
std::string GetCurrentLogFile() const;
```

**示例**:
```cpp
std::string logFile = logger.GetCurrentLogFile();
LOG_INFO("Current log file: " + logFile);
```

**线程安全**: ✅ 使用互斥锁保护

---

## 完整示例

### 基本使用

```cpp
#include "render/logger.h"

int main() {
    auto& logger = Render::Logger::GetInstance();
    
    // 基本配置
    logger.SetLogLevel(Render::LogLevel::Debug);
    logger.SetLogToConsole(true);
    logger.SetLogToFile(true);
    
    // 基本日志
    LOG_INFO("Application started");
    LOG_DEBUG("Debug mode enabled");
    LOG_WARNING("This is a warning");
    LOG_ERROR("This is an error");
    
    return 0;
}
```

---

### 格式化日志

```cpp
int width = 1920, height = 1080;
float fps = 60.5f;

LOG_INFO_F("Window size: %dx%d", width, height);
LOG_DEBUG_F("Rendering at %.1f FPS", fps);

// 性能测试
auto start = std::chrono::high_resolution_clock::now();
// ... 执行操作 ...
auto end = std::chrono::high_resolution_clock::now();
auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
LOG_INFO_F("Operation completed in %lld ms", ms.count());
```

---

### 高级配置

```cpp
auto& logger = Render::Logger::GetInstance();

// 启用所有功能
logger.SetAsyncLogging(true);          // 异步日志（默认启用）
logger.SetColorOutput(true);           // 彩色输出
logger.SetShowThreadId(true);          // 显示线程ID
logger.SetMaxFileSize(10 * 1024 * 1024); // 10MB轮转

// 设置回调
logger.SetLogCallback([](Render::LogLevel level, const std::string& msg) {
    if (level == Render::LogLevel::Error) {
        // 发送错误通知
        NotifyErrorMonitoring(msg);
    }
});
```

---

### 异步日志示例

```cpp
auto& logger = Render::Logger::GetInstance();

// 配置异步模式（默认已启用）
logger.SetAsyncLogging(true);
logger.SetLogToFile(true);

// 高速写入日志
for (int i = 0; i < 10000; i++) {
    LOG_INFO_F("Processing item #%d", i);
}

// 确保所有日志已写入
logger.Flush();

// 检查队列状态
size_t pending = logger.GetQueueSize();
LOG_INFO_F("Pending logs: %zu", pending);
```

---

### 同步模式示例

某些场景需要立即写入（如崩溃前）：

```cpp
auto& logger = Render::Logger::GetInstance();

// 切换到同步模式
logger.SetAsyncLogging(false);

// 关键日志会立即写入
LOG_ERROR("Critical error occurred!");
// 此时日志已经写入磁盘

// 恢复异步模式
logger.SetAsyncLogging(true);
```

---

### 多线程使用

```cpp
#include <thread>

void WorkerThread(int id) {
    for (int i = 0; i < 100; i++) {
        LOG_INFO_F("Thread %d: Processing item %d", id, i);
    }
}

int main() {
    auto& logger = Render::Logger::GetInstance();
    logger.SetShowThreadId(true);  // 启用线程ID显示
    
    // 创建多个线程
    std::thread t1(WorkerThread, 1);
    std::thread t2(WorkerThread, 2);
    std::thread t3(WorkerThread, 3);
    
    t1.join();
    t2.join();
    t3.join();
    
    return 0;
}
```

---

### 调试模式

```cpp
#ifdef _DEBUG
    LOG_DEBUG_LOC("Variable value: " + std::to_string(value));
    LOG_DEBUG_F("Array[%d] = %f", index, array[index]);
#endif

// 带位置信息的错误
if (texture == nullptr) {
    LOG_ERROR_LOC("Failed to load texture");
}
```

---

## 性能特性

### 早期级别检查

```cpp
// 如果日志级别低于设定值，直接返回，无需加锁
if (level < m_logLevel.load(std::memory_order_acquire)) {
    return;
}
```

### 无锁配置读取

大部分配置使用 `std::atomic`，读取时无需加锁：
- `SetLogLevel` / 级别检查
- `SetLogToConsole` / 控制台输出判断
- `SetLogToFile` / 文件输出判断
- `SetColorOutput` / 颜色控制
- `SetShowThreadId` / 线程ID显示
- `SetMaxFileSize` / 文件大小检查

### 性能数据

在测试环境下（Intel i7, Windows 10, SSD）：

**异步模式（默认）**:
- 写入速度: **347,078 条/秒**
- 单条延迟: **2.9 μs**
- 多线程: 20线程×1000条 = 420ms
- 队列开销: 极低（~1-2μs）

**同步模式**:
- 写入速度: 36,998 条/秒
- 单条延迟: 27 μs
- 多线程: 存在锁竞争

**性能提升**: 异步模式比同步模式快 **9.37倍** (837%)

---

## 线程安全保证

### 避免死锁

1. **单一锁策略**: 整个 Logger 类只使用一个互斥锁
2. **早期检查**: 级别检查在加锁前完成
3. **锁的作用域**: 使用 `std::lock_guard` 确保自动释放
4. **原子配置**: 配置变量使用 `std::atomic`，读取无需加锁

### 线程安全的API

以下API完全线程安全：
- ✅ 所有日志输出方法（`Log`, `Debug`, `Info`, etc.）
- ✅ 所有格式化方法（`LogFormat`, `DebugFormat`, etc.）
- ✅ 所有配置方法（`SetLogLevel`, `SetLogToFile`, etc.）
- ✅ 查询方法（`GetCurrentLogFile`）
- ✅ 日志回调（异常隔离）

### 时间函数线程安全

使用线程安全的时间转换函数：
- Windows: `localtime_s`
- POSIX: `localtime_r`

---

## 最佳实践

### 1. 使用宏而非直接调用

```cpp
// 推荐
LOG_INFO("Message");
LOG_INFO_F("Value: %d", value);

// 不推荐
logger.Info("Message");
logger.InfoFormat("Value: %d", value);
```

**原因**: 宏更简洁，且可以在未来添加调试信息（如 `__FILE__`, `__LINE__`）

---

### 2. 合理设置日志级别

```cpp
// 开发环境
#ifdef _DEBUG
    logger.SetLogLevel(LogLevel::Debug);
#else
    logger.SetLogLevel(LogLevel::Info);
#endif
```

---

### 3. 使用格式化日志

```cpp
// 推荐：使用格式化
LOG_INFO_F("Loaded %d/%d textures", loaded, total);

// 不推荐：字符串拼接
LOG_INFO("Loaded " + std::to_string(loaded) + "/" + std::to_string(total) + " textures");
```

**原因**: 格式化更高效，代码更清晰

---

### 4. 错误处理中使用位置信息

```cpp
if (error) {
    LOG_ERROR_LOC("Detailed error description");
    return false;
}
```

---

### 5. 性能敏感代码中使用条件日志

```cpp
#ifdef ENABLE_VERBOSE_LOGGING
    LOG_DEBUG_F("Particle %d position: (%.2f, %.2f, %.2f)", 
                 id, pos.x, pos.y, pos.z);
#endif
```

---

### 6. 文件轮转配置

```cpp
// 生产环境：限制单个日志文件大小
logger.SetMaxFileSize(50 * 1024 * 1024);  // 50MB

// 开发环境：不限制
logger.SetMaxFileSize(0);
```

---

### 7. 异步日志最佳实践

```cpp
// 程序初始化时（异步模式默认启用）
logger.SetAsyncLogging(true);
logger.SetLogToFile(true);

// 正常使用，无需考虑性能
LOG_INFO_F("High frequency log: %d", value);

// 程序退出前刷新队列
void Shutdown() {
    LOG_INFO("Shutting down...");
    logger.Flush();  // 确保日志写入
}
```

---

### 8. 性能监控

```cpp
// 监控日志队列堆积
void MonitorLogQueue() {
    size_t queueSize = logger.GetQueueSize();
    if (queueSize > 5000) {
        // 队列堆积，考虑降低日志级别
        LOG_WARNING_F("Log queue backlog: %zu messages", queueSize);
    }
}
```

---

## 注意事项

1. **格式化字符串安全**: 确保格式说明符与参数类型匹配
2. **回调异常**: 回调函数中的异常会被捕获，不影响日志系统
3. **文件轮转**: 轮转在后台线程执行，不阻塞主线程，无日志丢失
4. **UTF-8编码**: 日志文件使用UTF-8编码（带BOM）
5. **Windows控制台**: 自动启用ANSI颜色支持（Windows 10+）
6. **异步队列**: 默认启用异步模式，程序退出前建议调用 `Flush()`
7. **队列容量**: 理论上无限，但极端情况下可能消耗内存

---

## 常见问题

### Q: 如何在多线程环境中使用？

A: Logger 完全线程安全，直接使用即可，无需额外同步。

---

### Q: 格式化日志的缓冲区大小？

A: 默认512字节，超过则动态分配。支持任意长度的日志消息。

---

### Q: 如何禁用某个级别的日志？

A: 使用 `SetLogLevel` 设置最低级别：
```cpp
logger.SetLogLevel(LogLevel::Warning);  // 禁用 Debug 和 Info
```

---

### Q: 日志文件什么时候创建？

A: 调用 `SetLogToFile(true)` 时立即创建。

---

### Q: 如何查看日志文件编码？

A: 日志文件使用 UTF-8 编码，文件开头有 BOM（`EF BB BF`）。

---

### Q: 异步模式会丢失日志吗？

A: 不会。程序正常退出时析构函数会自动刷新队列。建议在关键点手动调用 `Flush()`。

---

### Q: 如何选择同步还是异步模式？

A: 
- **异步模式（推荐）**: 适用于大部分场景，高性能，低延迟
- **同步模式**: 适用于需要立即写入的场景（如崩溃处理）

---

### Q: 异步模式下日志顺序会乱吗？

A: 不会。异步模式保证日志按照调用顺序写入（FIFO队列）。

---

### Q: 队列堆积怎么办？

A: 
1. 降低日志级别（减少日志量）
2. 增加文件写入频率
3. 检查是否有大量重复日志
4. 使用 `GetQueueSize()` 监控

---

[上一篇: GLThreadChecker](GLThreadChecker.md) | [下一篇: FileUtils](FileUtils.md)

