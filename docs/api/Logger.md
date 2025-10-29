# Logger API 参考

[返回 API 首页](README.md)

---

## 概述

`Logger` 提供日志记录功能，支持文件和控制台输出，自动时间戳命名。

**头文件**: `render/logger.h`  
**命名空间**: `Render`

---

## 日志级别

```cpp
enum class LogLevel {
    Debug,      // 调试信息
    Info,       // 一般信息
    Warning,    // 警告
    Error       // 错误
};
```

---

## 单例访问

```cpp
Logger& logger = Logger::GetInstance();
```

---

## 配置方法

### SetLogLevel

设置日志级别。

```cpp
void SetLogLevel(LogLevel level);
```

**示例**:
```cpp
logger.SetLogLevel(LogLevel::Info);  // 只记录 Info 及以上
```

---

### SetLogToConsole / SetLogToFile

启用/禁用输出目标。

```cpp
void SetLogToConsole(bool enable);
void SetLogToFile(bool enable);
```

**示例**:
```cpp
logger.SetLogToConsole(true);
logger.SetLogToFile(true);
```

---

## 日志宏

### LOG_DEBUG / LOG_INFO / LOG_WARNING / LOG_ERROR

记录日志。

```cpp
LOG_DEBUG("Debug message");
LOG_INFO("Initialization complete");
LOG_WARNING("Texture not found, using default");
LOG_ERROR("Failed to load shader");
```

---

### 获取日志文件路径

```cpp
std::string logFile = logger.GetCurrentLogFile();
LOG_INFO("Log file: " + logFile);
```

---

## 示例

```cpp
// 初始化
Logger::GetInstance().SetLogToConsole(true);
Logger::GetInstance().SetLogToFile(true);
Logger::GetInstance().SetLogLevel(LogLevel::Info);

LOG_INFO("Application started");
LOG_DEBUG("This won't show (level is Info)");
LOG_WARNING("Low memory");
LOG_ERROR("Critical error");
```

---

[上一篇: OpenGLContext](OpenGLContext.md) | [下一篇: FileUtils](FileUtils.md)

