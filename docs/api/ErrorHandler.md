# ErrorHandler API 参考

[返回 API 首页](README.md)

---

## 概述

`ErrorHandler` 提供统一的错误处理机制，支持错误分类、严重程度等级、源代码位置追踪、OpenGL 错误检查、回调系统和错误统计。

**头文件**: `render/error.h`  
**命名空间**: `Render`

### 主要特性

- ✅ **统一的错误码体系** - 50+ 错误码，按类别分类
- ✅ **多级严重程度** - Info、Warning、Error、Critical
- ✅ **自动位置追踪** - 使用 C++20 `std::source_location` 自动捕获错误位置
- ✅ **OpenGL 错误检查** - 自动检测和报告 OpenGL 错误
- ✅ **回调系统** - 支持多个自定义错误处理回调
- ✅ **错误统计** - 自动收集错误统计信息
- ✅ **完全线程安全** - 所有操作都是线程安全的
- ✅ **性能友好** - 正常情况下几乎无开销

---

## 错误严重程度

```cpp
enum class ErrorSeverity {
    Info,       // 信息 - 一般性信息
    Warning,    // 警告 - 可恢复的问题
    Error,      // 错误 - 可能可恢复的错误
    Critical    // 严重错误 - 不可恢复的错误
};
```

**级别说明**:
- `Info` - 用于调试信息，不影响功能
- `Warning` - 可恢复的问题，如使用默认值替代
- `Error` - 影响功能但可能可恢复的错误
- `Critical` - 不可恢复的严重错误，可能导致程序终止

---

## 错误类别

```cpp
enum class ErrorCategory {
    OpenGL = 1000,         // OpenGL 相关错误
    Resource = 2000,       // 资源管理错误
    Threading = 3000,      // 线程相关错误
    Rendering = 4000,      // 渲染相关错误
    IO = 5000,             // 文件 I/O 错误
    Initialization = 6000, // 初始化错误
    Generic = 9000         // 通用错误
};
```

---

## 错误码

### OpenGL 错误 (1000-1999)

| 错误码 | 名称 | 描述 |
|--------|------|------|
| 1000 | GLInvalidEnum | 无效的枚举值 |
| 1001 | GLInvalidValue | 无效的参数值 |
| 1002 | GLInvalidOperation | 无效的操作 |
| 1003 | GLOutOfMemory | OpenGL 内存不足 |
| 1004 | GLInvalidFramebufferOperation | 无效的帧缓冲操作 |
| 1005 | GLContextCreationFailed | 上下文创建失败 |
| 1006 | GLExtensionNotSupported | 扩展不支持 |

### 资源错误 (2000-2999)

| 错误码 | 名称 | 描述 |
|--------|------|------|
| 2000 | ResourceNotFound | 资源未找到 |
| 2001 | ResourceAlreadyExists | 资源已存在 |
| 2002 | ResourceLoadFailed | 资源加载失败 |
| 2003 | ResourceInvalidFormat | 无效的资源格式 |
| 2004 | ResourceCorrupted | 资源损坏 |
| 2005 | ResourceUploadFailed | 资源上传失败 |
| 2006 | ResourceInUse | 资源正在使用中 |

### 线程错误 (3000-3999)

| 错误码 | 名称 | 描述 |
|--------|------|------|
| 3000 | WrongThread | 错误的线程调用 |
| 3001 | DeadlockDetected | 检测到死锁 |
| 3002 | ThreadCreationFailed | 线程创建失败 |
| 3003 | ThreadSynchronizationFailed | 线程同步失败 |

### 渲染错误 (4000-4999)

| 错误码 | 名称 | 描述 |
|--------|------|------|
| 4000 | ShaderCompileFailed | 着色器编译失败 |
| 4001 | ShaderLinkFailed | 着色器链接失败 |
| 4002 | ShaderUniformNotFound | Uniform 未找到 |
| 4003 | TextureUploadFailed | 纹理上传失败 |
| 4004 | MeshUploadFailed | 网格上传失败 |
| 4005 | RenderTargetInvalid | 渲染目标无效 |

### I/O 错误 (5000-5999)

| 错误码 | 名称 | 描述 |
|--------|------|------|
| 5000 | FileNotFound | 文件未找到 |
| 5001 | FileOpenFailed | 文件打开失败 |
| 5002 | FileReadFailed | 文件读取失败 |
| 5003 | FileWriteFailed | 文件写入失败 |
| 5004 | PathInvalid | 路径无效 |

### 初始化错误 (6000-6999)

| 错误码 | 名称 | 描述 |
|--------|------|------|
| 6000 | InitializationFailed | 初始化失败 |
| 6001 | AlreadyInitialized | 已经初始化 |
| 6002 | NotInitialized | 未初始化 |
| 6003 | ConfigurationInvalid | 配置无效 |

### 通用错误 (9000-9999)

| 错误码 | 名称 | 描述 |
|--------|------|------|
| 9000 | NotImplemented | 功能未实现 |
| 9001 | InvalidArgument | 无效的参数 |
| 9002 | NullPointer | 空指针 |
| 9003 | OutOfRange | 超出范围 |
| 9004 | OutOfMemory | 内存不足 |
| 9005 | InvalidState | 无效的状态 |
| 9006 | OperationFailed | 操作失败 |
| 9999 | Unknown | 未知错误 |

---

## RenderError 类

错误异常类，继承自 `std::exception`。

```cpp
class RenderError : public std::exception {
public:
    RenderError(ErrorCode code, 
                const std::string& message,
                ErrorSeverity severity = ErrorSeverity::Error,
                std::source_location location = std::source_location::current());
    
    const char* what() const noexcept override;
    
    ErrorCode GetCode() const;
    ErrorCategory GetCategory() const;
    ErrorSeverity GetSeverity() const;
    const std::string& GetMessage() const;
    const std::string& GetFullMessage() const;
    const std::string& GetFile() const;
    int GetLine() const;
    const std::string& GetFunction() const;
};
```

### 构造函数

```cpp
RenderError(ErrorCode code, 
            const std::string& message,
            ErrorSeverity severity = ErrorSeverity::Error,
            std::source_location location = std::source_location::current());
```

**参数**:
- `code` - 错误码
- `message` - 错误消息
- `severity` - 严重程度（默认 Error）
- `location` - 源代码位置（自动捕获）

**线程安全**: ✅ 构造函数是线程安全的

---

### GetCode

获取错误码。

```cpp
ErrorCode GetCode() const;
```

**返回值**: 错误码

---

### GetCategory

获取错误类别。

```cpp
ErrorCategory GetCategory() const;
```

**返回值**: 错误类别

---

### GetSeverity

获取严重程度。

```cpp
ErrorSeverity GetSeverity() const;
```

**返回值**: 严重程度

---

### GetMessage

获取用户消息。

```cpp
const std::string& GetMessage() const;
```

**返回值**: 错误消息（不含位置信息）

---

### GetFullMessage

获取完整消息（包含位置信息）。

```cpp
const std::string& GetFullMessage() const;
```

**返回值**: 完整的错误消息

---

## ErrorHandler 类

全局错误处理器（单例）。

```cpp
class ErrorHandler {
public:
    static ErrorHandler& GetInstance();
    
    void Handle(const RenderError& error);
    bool CheckGLError(const std::source_location& location = 
                      std::source_location::current());
    void CheckGLErrorThrow(const std::source_location& location = 
                          std::source_location::current());
    
    size_t AddCallback(ErrorCallback callback);
    void RemoveCallback(size_t id);
    void ClearCallbacks();
    
    void SetEnabled(bool enable);
    bool IsEnabled() const;
    void SetGLErrorCheckEnabled(bool enable);
    bool IsGLErrorCheckEnabled() const;
    
    ErrorStats GetStats() const;
    void ResetStats();
    void SetMaxErrors(size_t maxErrors);
};
```

### GetInstance

获取单例实例。

```cpp
static ErrorHandler& GetInstance();
```

**返回值**: ErrorHandler 单例引用

**线程安全**: ✅ 线程安全的单例实现

**示例**:
```cpp
auto& handler = ErrorHandler::GetInstance();
```

---

### Handle

处理错误。

```cpp
void Handle(const RenderError& error);
```

**参数**:
- `error` - 错误对象

**功能**:
- 根据严重程度记录日志
- 调用所有注册的回调函数
- 更新错误统计

**线程安全**: ✅ 使用互斥锁保护

**示例**:
```cpp
RenderError error(ErrorCode::ResourceNotFound, "纹理未找到");
ErrorHandler::GetInstance().Handle(error);
```

---

### CheckGLError

检查 OpenGL 错误（只记录）。

```cpp
bool CheckGLError(const std::source_location& location = 
                  std::source_location::current());
```

**参数**:
- `location` - 调用位置（自动捕获）

**返回值**: 是否检测到错误

**功能**:
- 调用 `glGetError()` 检查错误
- 如果有错误，记录到日志
- **不抛出异常**

**线程安全**: ✅ 线程安全

**示例**:
```cpp
glBindTexture(GL_TEXTURE_2D, textureID);
CHECK_GL_ERROR();  // 只记录，不抛出异常
```

---

### CheckGLErrorThrow

检查 OpenGL 错误并抛出异常。

```cpp
void CheckGLErrorThrow(const std::source_location& location = 
                      std::source_location::current());
```

**参数**:
- `location` - 调用位置（自动捕获）

**功能**:
- 调用 `glGetError()` 检查错误
- 如果有错误，**抛出 RenderError 异常**

**线程安全**: ✅ 线程安全

**示例**:
```cpp
try {
    glTexImage2D(...);
    CHECK_GL_ERROR_THROW();  // 有错误会抛出异常
} catch (const RenderError& e) {
    // 处理错误
}
```

---

### AddCallback

添加错误回调。

```cpp
size_t AddCallback(ErrorCallback callback);
```

**参数**:
- `callback` - 回调函数，签名为 `void(const RenderError&)`

**返回值**: 回调 ID（用于移除）

**线程安全**: ✅ 使用互斥锁保护

**示例**:
```cpp
size_t id = ErrorHandler::GetInstance().AddCallback(
    [](const RenderError& error) {
        if (error.GetSeverity() == ErrorSeverity::Critical) {
            SaveCrashDump(error);
        }
    }
);
```

---

### RemoveCallback

移除错误回调。

```cpp
void RemoveCallback(size_t id);
```

**参数**:
- `id` - 回调 ID（由 AddCallback 返回）

**线程安全**: ✅ 使用互斥锁保护

---

### ClearCallbacks

清空所有回调。

```cpp
void ClearCallbacks();
```

**线程安全**: ✅ 使用互斥锁保护

---

### SetEnabled

启用/禁用错误处理。

```cpp
void SetEnabled(bool enable);
```

**参数**:
- `enable` - 是否启用

**线程安全**: ✅ 使用原子操作

---

### SetGLErrorCheckEnabled

启用/禁用 GL 错误检查。

```cpp
void SetGLErrorCheckEnabled(bool enable);
```

**参数**:
- `enable` - 是否启用

**线程安全**: ✅ 使用原子操作

**示例**:
```cpp
// 发布版本中可以禁用 GL 错误检查以提高性能
#ifdef NDEBUG
    ErrorHandler::GetInstance().SetGLErrorCheckEnabled(false);
#endif
```

---

### GetStats

获取错误统计。

```cpp
ErrorStats GetStats() const;
```

**返回值**: 错误统计结构

```cpp
struct ErrorStats {
    size_t infoCount;      // Info 级别错误数
    size_t warningCount;   // Warning 级别错误数
    size_t errorCount;     // Error 级别错误数
    size_t criticalCount;  // Critical 级别错误数
    size_t totalCount;     // 总错误数
};
```

**线程安全**: ✅ 使用原子操作

**示例**:
```cpp
auto stats = ErrorHandler::GetInstance().GetStats();
std::cout << "错误统计:\n";
std::cout << "  警告: " << stats.warningCount << "\n";
std::cout << "  错误: " << stats.errorCount << "\n";
std::cout << "  总计: " << stats.totalCount << "\n";
```

---

### ResetStats

重置错误统计。

```cpp
void ResetStats();
```

**线程安全**: ✅ 使用原子操作

---

### SetMaxErrors

设置最大错误数量。

```cpp
void SetMaxErrors(size_t maxErrors);
```

**参数**:
- `maxErrors` - 最大错误数量（0 = 无限制）

**功能**: 当错误数量超过此值时，停止处理新错误（防止无限递归）

**线程安全**: ✅ 使用原子操作

---

## 便捷宏

### RENDER_ERROR

创建 Error 级别的错误。

```cpp
#define RENDER_ERROR(code, msg) \
    Render::RenderError(code, msg, Render::ErrorSeverity::Error)
```

**示例**:
```cpp
throw RENDER_ERROR(ErrorCode::ResourceNotFound, "纹理未找到: albedo.png");
```

---

### RENDER_WARNING

创建 Warning 级别的错误。

```cpp
#define RENDER_WARNING(code, msg) \
    Render::RenderError(code, msg, Render::ErrorSeverity::Warning)
```

**示例**:
```cpp
throw RENDER_WARNING(ErrorCode::ResourceAlreadyExists, "资源已存在");
```

---

### RENDER_CRITICAL

创建 Critical 级别的错误。

```cpp
#define RENDER_CRITICAL(code, msg) \
    Render::RenderError(code, msg, Render::ErrorSeverity::Critical)
```

**示例**:
```cpp
throw RENDER_CRITICAL(ErrorCode::GLOutOfMemory, "OpenGL 内存不足");
```

---

### RENDER_ASSERT

断言宏。

```cpp
#define RENDER_ASSERT(condition, msg) \
    do { \
        if (!(condition)) { \
            throw RENDER_ERROR(Render::ErrorCode::InvalidArgument, \
                             std::string("断言失败: ") + #condition + " - " + (msg)); \
        } \
    } while(0)
```

**示例**:
```cpp
RENDER_ASSERT(texture != nullptr, "纹理指针不能为空");
RENDER_ASSERT(width > 0 && height > 0, "纹理尺寸必须大于 0");
```

---

### CHECK_GL_ERROR

检查 OpenGL 错误（只记录）。

```cpp
#define CHECK_GL_ERROR() \
    Render::ErrorHandler::GetInstance().CheckGLError()
```

**示例**:
```cpp
glBindTexture(GL_TEXTURE_2D, id);
CHECK_GL_ERROR();  // 只记录，不抛出异常
```

---

### CHECK_GL_ERROR_THROW

检查 OpenGL 错误（抛出异常）。

```cpp
#define CHECK_GL_ERROR_THROW() \
    Render::ErrorHandler::GetInstance().CheckGLErrorThrow()
```

**示例**:
```cpp
try {
    glTexImage2D(...);
    CHECK_GL_ERROR_THROW();  // 有错误会抛出异常
} catch (const RenderError& e) {
    // 处理错误
}
```

---

### HANDLE_ERROR

处理错误（不抛出异常）。

```cpp
#define HANDLE_ERROR(error) \
    Render::ErrorHandler::GetInstance().Handle(error)
```

**示例**:
```cpp
if (!LoadTexture(path)) {
    HANDLE_ERROR(RENDER_ERROR(ErrorCode::TextureUploadFailed, 
                              "纹理加载失败: " + path));
    return nullptr;
}
```

---

### RENDER_TRY / RENDER_CATCH

Try-Catch 辅助宏。

```cpp
#define RENDER_TRY try

#define RENDER_CATCH \
    catch (const Render::RenderError& e) { \
        Render::ErrorHandler::GetInstance().Handle(e); \
    }

#define RENDER_CATCH_ALL \
    catch (const Render::RenderError& e) { \
        Render::ErrorHandler::GetInstance().Handle(e); \
    } \
    catch (const std::exception& e) { \
        /* 处理标准异常 */ \
    } \
    catch (...) { \
        /* 处理未知异常 */ \
    }
```

**示例**:
```cpp
RENDER_TRY {
    LoadTexture("test.png");
    CompileShader("shader.glsl");
} 
RENDER_CATCH {
    // 错误已自动处理
    return false;
}
```

---

## 基本使用

### 抛出和捕获错误

```cpp
#include "render/error.h"

// 抛出错误
void LoadTexture(const std::string& path) {
    if (path.empty()) {
        throw RENDER_ERROR(ErrorCode::InvalidArgument, 
                         "纹理路径不能为空");
    }
    
    if (!FileExists(path)) {
        throw RENDER_ERROR(ErrorCode::FileNotFound, 
                         "纹理文件不存在: " + path);
    }
    
    // ... 加载纹理
}

// 捕获和处理错误
RENDER_TRY {
    LoadTexture("textures/albedo.png");
}
RENDER_CATCH {
    // 错误已自动记录
}
```

---

### 参数验证

```cpp
bool Texture::CreateFromData(const void* data, int width, int height) {
    // 参数验证
    if (width <= 0 || height <= 0) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::InvalidArgument, 
                                 "纹理尺寸无效: " + 
                                 std::to_string(width) + "x" + std::to_string(height)));
        return false;
    }
    
    if (width > 8192 || height > 8192) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::OutOfRange, 
                                   "纹理尺寸超过推荐限制"));
    }
    
    // ... 创建纹理
    return true;
}
```

---

### OpenGL 错误检查

```cpp
void UploadTexture(const void* data, int width, int height) {
    // 生成纹理
    GLuint textureID;
    glGenTextures(1, &textureID);
    CHECK_GL_ERROR();  // 只记录，不抛出
    
    // 绑定纹理
    glBindTexture(GL_TEXTURE_2D, textureID);
    CHECK_GL_ERROR();
    
    // 上传数据
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 
                 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    CHECK_GL_ERROR();
}
```

---

### 自定义错误回调

```cpp
class ErrorMonitor {
public:
    ErrorMonitor() {
        // 注册回调
        m_callbackId = ErrorHandler::GetInstance().AddCallback(
            [this](const RenderError& error) {
                OnError(error);
            }
        );
    }
    
    ~ErrorMonitor() {
        // 移除回调
        ErrorHandler::GetInstance().RemoveCallback(m_callbackId);
    }
    
private:
    void OnError(const RenderError& error) {
        // 记录到文件
        m_logFile << error.GetFullMessage() << "\n";
        
        // 如果是严重错误，保存现场
        if (error.GetSeverity() == ErrorSeverity::Critical) {
            SaveCrashDump(error);
        }
    }
    
    size_t m_callbackId;
    std::ofstream m_logFile;
};
```

---

### 错误统计

```cpp
void PrintErrorStatistics() {
    auto stats = ErrorHandler::GetInstance().GetStats();
    
    std::cout << "========================================\n";
    std::cout << "错误统计\n";
    std::cout << "========================================\n";
    std::cout << "信息:   " << stats.infoCount << "\n";
    std::cout << "警告:   " << stats.warningCount << "\n";
    std::cout << "错误:   " << stats.errorCount << "\n";
    std::cout << "严重:   " << stats.criticalCount << "\n";
    std::cout << "总计:   " << stats.totalCount << "\n";
    std::cout << "========================================\n";
}
```

---

## 高级用法

### 线程安全的资源加载

```cpp
class TextureLoader {
public:
    TexturePtr LoadTexture(const std::string& name, 
                          const std::string& filepath) {
        // 参数验证
        if (filepath.empty()) {
            HANDLE_ERROR(RENDER_ERROR(ErrorCode::InvalidArgument, 
                                     "文件路径为空"));
            return nullptr;
        }
        
        // 检查缓存
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_cache.find(name);
            if (it != m_cache.end()) {
                return it->second;
            }
        }
        
        // 加载纹理
        RENDER_TRY {
            auto texture = LoadTextureInternal(filepath);
            if (!texture) {
                throw RENDER_ERROR(ErrorCode::TextureUploadFailed, 
                                 "纹理加载失败: " + filepath);
            }
            
            // 添加到缓存
            std::lock_guard<std::mutex> lock(m_mutex);
            m_cache[name] = texture;
            return texture;
        }
        RENDER_CATCH {
            return nullptr;
        }
    }
    
private:
    std::mutex m_mutex;
    std::unordered_map<std::string, TexturePtr> m_cache;
};
```

---

### 错误恢复策略

```cpp
Camera::SetPerspective(float fov, float aspect, float near, float far) {
    // 参数验证和自动修正
    if (fov <= 0.0f || fov >= 180.0f) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::OutOfRange, 
                                   "FOV 超出范围，自动修正"));
        fov = std::clamp(fov, 1.0f, 179.0f);
    }
    
    if (aspect <= 0.0f) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument, 
                                   "宽高比无效，使用默认值"));
        aspect = 1.0f;
    }
    
    if (near <= 0.0f || far <= near) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument, 
                                   "裁剪面参数无效，自动修正"));
        near = std::max(0.01f, near);
        far = std::max(near + 1.0f, far);
    }
    
    // 设置投影
    m_fov = fov;
    m_aspect = aspect;
    m_near = near;
    m_far = far;
    UpdateProjectionMatrix();
}
```

---

## 性能考虑

### 正常情况（无错误）

- **开销**: 几乎为零
- 错误检查是简单的条件判断
- 现代 CPU 的分支预测可以优化

### 错误情况（有错误）

- **开销**: 错误记录和回调调用
- 日志记录的开销
- 回调函数的执行时间

### 优化建议

1. **发布版本中禁用详细检查**:
```cpp
#ifdef NDEBUG
    ErrorHandler::GetInstance().SetGLErrorCheckEnabled(false);
#endif
```

2. **使用 HANDLE_ERROR 而非抛出异常**:
```cpp
// ✅ 好 - 只记录，不中断流程
if (!LoadTexture(path)) {
    HANDLE_ERROR(RENDER_ERROR(ErrorCode::TextureUploadFailed, "..."));
    return GetDefaultTexture();
}

// ❌ 避免 - 异常开销较大
if (!LoadTexture(path)) {
    throw RENDER_ERROR(ErrorCode::TextureUploadFailed, "...");
}
```

3. **避免在热路径中频繁检查**:
```cpp
// ❌ 不好 - 每次迭代都检查
for (int i = 0; i < 1000000; ++i) {
    RENDER_ASSERT(data != nullptr, "...");
    ProcessData(data[i]);
}

// ✅ 好 - 只在外层检查一次
RENDER_ASSERT(data != nullptr, "...");
for (int i = 0; i < 1000000; ++i) {
    ProcessData(data[i]);
}
```

---

## 最佳实践

### 选择合适的严重程度

- **Info**: 调试信息，不影响功能
- **Warning**: 可恢复的问题，已自动处理
- **Error**: 影响功能但可能可恢复
- **Critical**: 不可恢复，需要终止

### 提供详细的错误消息

```cpp
// ❌ 不好
throw RENDER_ERROR(ErrorCode::ResourceNotFound, "未找到");

// ✅ 好
throw RENDER_ERROR(ErrorCode::ResourceNotFound, 
                  "Texture::LoadFromFile: 纹理文件未找到: " + 
                  filepath + " (当前目录: " + GetCurrentDir() + ")");
```

### 使用正确的错误码

```cpp
// 选择最具体的错误码
if (!file.exists()) {
    // ✅ 使用 FileNotFound 而不是 ResourceNotFound
    throw RENDER_ERROR(ErrorCode::FileNotFound, "...");
}
```

### 避免异常用于控制流

```cpp
// ❌ 不要
try {
    auto texture = GetTexture(name);
} catch (const RenderError&) {
    texture = CreateDefaultTexture();
}

// ✅ 使用返回值
auto texture = GetTexture(name);
if (!texture) {
    texture = CreateDefaultTexture();
}
```

---

## 线程安全

### 保证

- ✅ **ErrorHandler::GetInstance()** - 线程安全的单例
- ✅ **ErrorHandler::Handle()** - 使用互斥锁保护
- ✅ **ErrorHandler::AddCallback/RemoveCallback** - 使用互斥锁保护
- ✅ **错误统计** - 使用原子操作
- ✅ **配置方法** - 使用原子操作

### 使用建议

可以安全地在多个线程中：
- 抛出和捕获错误
- 调用 CHECK_GL_ERROR()
- 添加/移除回调
- 获取统计信息

---

## 相关 API

- **[Logger](Logger.md)** - 日志系统（ErrorHandler 会自动记录到 Logger）
- **[GLThreadChecker](GLThreadChecker.md)** - OpenGL 线程检查

---

**API 版本**: 1.0  
**最后更新**: 2025-10-31

