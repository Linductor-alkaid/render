#pragma once

#include <string>
#include <exception>
#include <functional>
#include <source_location>
#include <atomic>
#include <mutex>
#include <vector>
#include <memory>

namespace Render {

/**
 * @brief 错误严重程度
 */
enum class ErrorSeverity {
    Info,       // 信息
    Warning,    // 警告（可恢复）
    Error,      // 错误（可能可恢复）
    Critical    // 严重错误（不可恢复）
};

/**
 * @brief 错误类别
 */
enum class ErrorCategory {
    // OpenGL 错误 (1000-1999)
    OpenGL = 1000,
    
    // 资源错误 (2000-2999)
    Resource = 2000,
    
    // 线程错误 (3000-3999)
    Threading = 3000,
    
    // 渲染错误 (4000-4999)
    Rendering = 4000,
    
    // IO 错误 (5000-5999)
    IO = 5000,
    
    // 初始化错误 (6000-6999)
    Initialization = 6000,
    
    // 通用错误 (9000-9999)
    Generic = 9000
};

/**
 * @brief 错误码
 */
enum class ErrorCode {
    Success = 0,
    
    // OpenGL 错误 (1000-1999)
    GLInvalidEnum = 1000,
    GLInvalidValue,
    GLInvalidOperation,
    GLOutOfMemory,
    GLInvalidFramebufferOperation,
    GLContextCreationFailed,
    GLExtensionNotSupported,
    
    // 资源错误 (2000-2999)
    ResourceNotFound = 2000,
    ResourceAlreadyExists,
    ResourceLoadFailed,
    ResourceInvalidFormat,
    ResourceCorrupted,
    ResourceUploadFailed,
    ResourceInUse,
    
    // 线程错误 (3000-3999)
    WrongThread = 3000,
    DeadlockDetected,
    ThreadCreationFailed,
    ThreadSynchronizationFailed,
    
    // 渲染错误 (4000-4999)
    ShaderCompileFailed = 4000,
    ShaderLinkFailed,
    ShaderUniformNotFound,
    TextureUploadFailed,
    MeshUploadFailed,
    RenderTargetInvalid,
    
    // IO 错误 (5000-5999)
    FileNotFound = 5000,
    FileOpenFailed,
    FileReadFailed,
    FileWriteFailed,
    PathInvalid,
    
    // 初始化错误 (6000-6999)
    InitializationFailed = 6000,
    AlreadyInitialized,
    NotInitialized,
    ConfigurationInvalid,
    
    // 通用错误 (9000-9999)
    NotImplemented = 9000,
    InvalidArgument,
    NullPointer,
    OutOfRange,
    OutOfMemory,
    InvalidState,
    OperationFailed,
    Unknown = 9999
};

/**
 * @brief 渲染错误基类
 * 
 * 包含错误码、消息、严重程度和源位置信息
 * 支持异常抛出和错误处理
 */
class RenderError : public std::exception {
public:
    /**
     * @brief 构造渲染错误
     * @param code 错误码
     * @param message 错误消息
     * @param severity 严重程度（默认为 Error）
     * @param location 源代码位置（自动捕获）
     */
    RenderError(ErrorCode code, 
                const std::string& message,
                ErrorSeverity severity = ErrorSeverity::Error,
                std::source_location location = std::source_location::current());
    
    /**
     * @brief 获取格式化的错误消息
     */
    const char* what() const noexcept override {
        return m_fullMessage.c_str();
    }
    
    /**
     * @brief 获取错误码
     */
    ErrorCode GetCode() const { return m_code; }
    
    /**
     * @brief 获取错误类别
     */
    ErrorCategory GetCategory() const { return m_category; }
    
    /**
     * @brief 获取严重程度
     */
    ErrorSeverity GetSeverity() const { return m_severity; }
    
    /**
     * @brief 获取用户消息
     */
    const std::string& GetMessage() const { return m_message; }
    
    /**
     * @brief 获取完整消息（包含位置信息）
     */
    const std::string& GetFullMessage() const { return m_fullMessage; }
    
    /**
     * @brief 获取源文件名
     */
    const std::string& GetFile() const { return m_file; }
    
    /**
     * @brief 获取源文件行号
     */
    int GetLine() const { return m_line; }
    
    /**
     * @brief 获取函数名
     */
    const std::string& GetFunction() const { return m_function; }
    
private:
    ErrorCode m_code;
    ErrorCategory m_category;
    ErrorSeverity m_severity;
    std::string m_message;       // 用户消息
    std::string m_fullMessage;   // 完整消息（包含位置信息）
    std::string m_file;
    std::string m_function;
    int m_line;
    
    /**
     * @brief 从错误码获取类别
     */
    static ErrorCategory GetCategoryFromCode(ErrorCode code);
    
    /**
     * @brief 格式化完整错误消息
     */
    static std::string FormatMessage(ErrorCode code, 
                                    const std::string& message, 
                                    ErrorSeverity severity,
                                    const std::string& file,
                                    const std::string& function,
                                    int line);
};

/**
 * @brief 错误处理回调函数类型
 */
using ErrorCallback = std::function<void(const RenderError&)>;

/**
 * @brief 错误处理器（单例）
 * 
 * 提供全局错误处理机制，包括：
 * - 错误回调系统
 * - OpenGL 错误检查
 * - 错误统计
 * - 错误日志
 */
class ErrorHandler {
public:
    /**
     * @brief 获取单例实例
     */
    static ErrorHandler& GetInstance();
    
    // 禁止拷贝和移动
    ErrorHandler(const ErrorHandler&) = delete;
    ErrorHandler& operator=(const ErrorHandler&) = delete;
    ErrorHandler(ErrorHandler&&) = delete;
    ErrorHandler& operator=(ErrorHandler&&) = delete;
    
    /**
     * @brief 处理错误
     * 
     * 根据严重程度记录日志，并调用回调函数
     * @param error 错误对象
     */
    void Handle(const RenderError& error);
    
    /**
     * @brief 检查 OpenGL 错误（只记录，不抛出异常）
     * 
     * 检查是否有 OpenGL 错误，如果有则记录但不抛出异常
     * @param location 调用位置（自动捕获）
     * @return 是否检测到错误
     */
    bool CheckGLError(const std::source_location& location = 
                      std::source_location::current());
    
    /**
     * @brief 检查 OpenGL 错误并抛出异常
     * 
     * 检查是否有 OpenGL 错误，如果有则抛出 RenderError 异常
     * @param location 调用位置（自动捕获）
     */
    void CheckGLErrorThrow(const std::source_location& location = 
                          std::source_location::current());
    
    /**
     * @brief 添加错误回调
     * 
     * 可以添加多个回调，按添加顺序调用
     * @param callback 回调函数
     * @return 回调ID（用于移除）
     */
    size_t AddCallback(ErrorCallback callback);
    
    /**
     * @brief 移除错误回调
     * @param id 回调ID
     */
    void RemoveCallback(size_t id);
    
    /**
     * @brief 清空所有回调
     */
    void ClearCallbacks();
    
    /**
     * @brief 启用/禁用错误处理
     * @param enable 是否启用
     */
    void SetEnabled(bool enable) {
        m_enabled.store(enable, std::memory_order_release);
    }
    
    /**
     * @brief 检查错误处理是否启用
     */
    bool IsEnabled() const {
        return m_enabled.load(std::memory_order_acquire);
    }
    
    /**
     * @brief 启用/禁用 GL 错误检查
     * @param enable 是否启用
     */
    void SetGLErrorCheckEnabled(bool enable) {
        m_glCheckEnabled.store(enable, std::memory_order_release);
    }
    
    /**
     * @brief 检查 GL 错误检查是否启用
     */
    bool IsGLErrorCheckEnabled() const {
        return m_glCheckEnabled.load(std::memory_order_acquire);
    }
    
    /**
     * @brief 获取错误统计
     * @return 各种严重程度的错误数量
     */
    struct ErrorStats {
        size_t infoCount = 0;
        size_t warningCount = 0;
        size_t errorCount = 0;
        size_t criticalCount = 0;
        size_t totalCount = 0;
    };
    
    ErrorStats GetStats() const;
    
    /**
     * @brief 重置错误统计
     */
    void ResetStats();
    
    /**
     * @brief 设置最大错误数量
     * 
     * 当错误数量超过此值时，停止处理新错误（防止无限递归）
     * @param maxErrors 最大错误数量（0 = 无限制）
     */
    void SetMaxErrors(size_t maxErrors) {
        m_maxErrors.store(maxErrors, std::memory_order_release);
    }
    
private:
    ErrorHandler();
    ~ErrorHandler() = default;
    
    // 回调管理
    struct CallbackEntry {
        size_t id;
        ErrorCallback callback;
    };
    std::vector<CallbackEntry> m_callbacks;
    size_t m_nextCallbackId = 1;
    mutable std::mutex m_callbackMutex;
    
    // 错误统计
    std::atomic<size_t> m_infoCount{0};
    std::atomic<size_t> m_warningCount{0};
    std::atomic<size_t> m_errorCount{0};
    std::atomic<size_t> m_criticalCount{0};
    std::atomic<size_t> m_totalCount{0};
    
    // 配置
    std::atomic<bool> m_enabled{true};
    std::atomic<bool> m_glCheckEnabled{true};
    std::atomic<size_t> m_maxErrors{1000};
    
    /**
     * @brief 更新统计信息
     */
    void UpdateStats(ErrorSeverity severity);
};

// ============================================================================
// 便捷宏
// ============================================================================

/**
 * @brief 创建并抛出渲染错误
 * 
 * 使用方法：
 * throw RENDER_ERROR(ErrorCode::ResourceNotFound, "无法找到纹理");
 */
#define RENDER_ERROR(code, msg) \
    Render::RenderError(code, msg, Render::ErrorSeverity::Error)

/**
 * @brief 创建并抛出警告级别的错误
 */
#define RENDER_WARNING(code, msg) \
    Render::RenderError(code, msg, Render::ErrorSeverity::Warning)

/**
 * @brief 创建并抛出严重错误
 */
#define RENDER_CRITICAL(code, msg) \
    Render::RenderError(code, msg, Render::ErrorSeverity::Critical)

/**
 * @brief 断言宏（条件不满足时抛出异常）
 * 
 * 使用方法：
 * RENDER_ASSERT(texture != nullptr, "纹理指针不能为空");
 */
#define RENDER_ASSERT(condition, msg) \
    do { \
        if (!(condition)) { \
            throw RENDER_ERROR(Render::ErrorCode::InvalidArgument, \
                             std::string("断言失败: ") + #condition + " - " + (msg)); \
        } \
    } while(0)

/**
 * @brief 检查 OpenGL 错误（只记录）
 * 
 * 如果有错误，只记录日志，不抛出异常
 * 使用方法：
 * glBindTexture(GL_TEXTURE_2D, id);
 * CHECK_GL_ERROR();
 */
#define CHECK_GL_ERROR() \
    Render::ErrorHandler::GetInstance().CheckGLError()

/**
 * @brief 检查 OpenGL 错误（抛出异常）
 * 
 * 如果有错误，会抛出异常
 * 使用方法：
 * glBindTexture(GL_TEXTURE_2D, id);
 * CHECK_GL_ERROR_THROW();
 */
#define CHECK_GL_ERROR_THROW() \
    Render::ErrorHandler::GetInstance().CheckGLErrorThrow()

/**
 * @brief 处理错误（不抛出异常，只记录）
 * 
 * 使用方法：
 * HANDLE_ERROR(RenderError(ErrorCode::ResourceNotFound, "纹理不存在"));
 */
#define HANDLE_ERROR(error) \
    Render::ErrorHandler::GetInstance().Handle(error)

/**
 * @brief Try-Catch 辅助宏
 * 
 * 自动捕获 RenderError 并处理
 * 使用方法：
 * RENDER_TRY {
 *     // 可能抛出异常的代码
 * } RENDER_CATCH {
 *     // 异常已自动处理，这里可以添加额外逻辑
 *     return false;
 * }
 */
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
        Render::ErrorHandler::GetInstance().Handle( \
            Render::RenderError(Render::ErrorCode::Unknown, \
                              std::string("标准异常: ") + e.what())); \
    } \
    catch (...) { \
        Render::ErrorHandler::GetInstance().Handle( \
            Render::RenderError(Render::ErrorCode::Unknown, "未知异常")); \
    }

// ============================================================================
// 辅助函数
// ============================================================================

/**
 * @brief 获取错误码的名称
 */
const char* ErrorCodeToString(ErrorCode code);

/**
 * @brief 获取错误严重程度的名称
 */
const char* ErrorSeverityToString(ErrorSeverity severity);

/**
 * @brief 获取错误类别的名称
 */
const char* ErrorCategoryToString(ErrorCategory category);

} // namespace Render

