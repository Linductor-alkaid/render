#include "render/error.h"
#include "render/logger.h"
#include <glad/glad.h>
#include <sstream>
#include <algorithm>

namespace Render {

// ============================================================================
// RenderError 实现
// ============================================================================

RenderError::RenderError(ErrorCode code, 
                         const std::string& message,
                         ErrorSeverity severity,
                         std::source_location location)
    : m_code(code)
    , m_category(GetCategoryFromCode(code))
    , m_severity(severity)
    , m_message(message)
    , m_file(location.file_name())
    , m_function(location.function_name())
    , m_line(static_cast<int>(location.line()))
{
    m_fullMessage = FormatMessage(code, message, severity, m_file, m_function, m_line);
}

ErrorCategory RenderError::GetCategoryFromCode(ErrorCode code) {
    int codeValue = static_cast<int>(code);
    
    if (codeValue >= 1000 && codeValue < 2000) {
        return ErrorCategory::OpenGL;
    } else if (codeValue >= 2000 && codeValue < 3000) {
        return ErrorCategory::Resource;
    } else if (codeValue >= 3000 && codeValue < 4000) {
        return ErrorCategory::Threading;
    } else if (codeValue >= 4000 && codeValue < 5000) {
        return ErrorCategory::Rendering;
    } else if (codeValue >= 5000 && codeValue < 6000) {
        return ErrorCategory::IO;
    } else if (codeValue >= 6000 && codeValue < 7000) {
        return ErrorCategory::Initialization;
    } else if (codeValue >= 7000 && codeValue < 8000) {
        return ErrorCategory::Transform;
    } else {
        return ErrorCategory::Generic;
    }
}

std::string RenderError::FormatMessage(ErrorCode code, 
                                      const std::string& message, 
                                      ErrorSeverity severity,
                                      const std::string& file,
                                      const std::string& function,
                                      int line)
{
    std::ostringstream oss;
    
    // [严重程度] [类别] (错误码): 消息
    oss << "[" << ErrorSeverityToString(severity) << "] ";
    oss << "[" << ErrorCategoryToString(GetCategoryFromCode(code)) << "] ";
    oss << "(" << static_cast<int>(code) << "): ";
    oss << message;
    
    // 添加位置信息
    oss << " [" << file << ":" << line << " in " << function << "]";
    
    return oss.str();
}

// ============================================================================
// ErrorHandler 实现
// ============================================================================

ErrorHandler& ErrorHandler::GetInstance() {
    static ErrorHandler instance;
    return instance;
}

ErrorHandler::ErrorHandler() {
    // 默认添加一个日志回调
    AddCallback([](const RenderError& error) {
        auto& logger = Logger::GetInstance();
        
        switch (error.GetSeverity()) {
            case ErrorSeverity::Info:
                logger.Info(error.GetFullMessage());
                break;
            case ErrorSeverity::Warning:
                logger.Warning(error.GetFullMessage());
                break;
            case ErrorSeverity::Error:
                logger.Error(error.GetFullMessage());
                break;
            case ErrorSeverity::Critical:
                logger.Error("[CRITICAL] " + error.GetFullMessage());
                break;
        }
    });
}

void ErrorHandler::Handle(const RenderError& error) {
    // 检查是否启用
    if (!m_enabled.load(std::memory_order_acquire)) {
        return;
    }
    
    // 检查是否超过最大错误数
    size_t maxErrors = m_maxErrors.load(std::memory_order_acquire);
    if (maxErrors > 0 && m_totalCount.load(std::memory_order_acquire) >= maxErrors) {
        return;
    }
    
    // 更新统计
    UpdateStats(error.GetSeverity());
    
    // 调用所有回调
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    for (const auto& entry : m_callbacks) {
        try {
            if (entry.callback) {
                entry.callback(error);
            }
        } catch (...) {
            // 回调不应该抛出异常，但为了安全起见捕获
            // 避免无限递归，这里不再调用 Handle
        }
    }
}

bool ErrorHandler::CheckGLError(const std::source_location& location) {
    // 检查是否启用
    if (!m_glCheckEnabled.load(std::memory_order_acquire)) {
        return false;
    }
    
    GLenum error = glGetError();
    if (error == GL_NO_ERROR) {
        return false;
    }
    
    // 构建错误消息
    std::string message;
    ErrorCode code = ErrorCode::Unknown;
    
    switch (error) {
        case GL_INVALID_ENUM:
            message = "OpenGL: Invalid Enum";
            code = ErrorCode::GLInvalidEnum;
            break;
        case GL_INVALID_VALUE:
            message = "OpenGL: Invalid Value";
            code = ErrorCode::GLInvalidValue;
            break;
        case GL_INVALID_OPERATION:
            message = "OpenGL: Invalid Operation";
            code = ErrorCode::GLInvalidOperation;
            break;
        case GL_OUT_OF_MEMORY:
            message = "OpenGL: Out of Memory";
            code = ErrorCode::GLOutOfMemory;
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            message = "OpenGL: Invalid Framebuffer Operation";
            code = ErrorCode::GLInvalidFramebufferOperation;
            break;
        default:
            message = "OpenGL: Unknown Error (0x" + 
                     std::to_string(error) + ")";
            code = ErrorCode::Unknown;
            break;
    }
    
    // 创建并处理错误（只记录，不抛出）
    RenderError renderError(code, message, ErrorSeverity::Error, location);
    Handle(renderError);
    
    return true;
}

void ErrorHandler::CheckGLErrorThrow(const std::source_location& location) {
    // 检查是否启用
    if (!m_glCheckEnabled.load(std::memory_order_acquire)) {
        return;
    }
    
    GLenum error = glGetError();
    if (error == GL_NO_ERROR) {
        return;
    }
    
    // 构建错误消息
    std::string message;
    ErrorCode code = ErrorCode::Unknown;
    
    switch (error) {
        case GL_INVALID_ENUM:
            message = "OpenGL: Invalid Enum";
            code = ErrorCode::GLInvalidEnum;
            break;
        case GL_INVALID_VALUE:
            message = "OpenGL: Invalid Value";
            code = ErrorCode::GLInvalidValue;
            break;
        case GL_INVALID_OPERATION:
            message = "OpenGL: Invalid Operation";
            code = ErrorCode::GLInvalidOperation;
            break;
        case GL_OUT_OF_MEMORY:
            message = "OpenGL: Out of Memory";
            code = ErrorCode::GLOutOfMemory;
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            message = "OpenGL: Invalid Framebuffer Operation";
            code = ErrorCode::GLInvalidFramebufferOperation;
            break;
        default:
            message = "OpenGL: Unknown Error (0x" + 
                     std::to_string(error) + ")";
            code = ErrorCode::Unknown;
            break;
    }
    
    // 创建并抛出异常
    throw RenderError(code, message, ErrorSeverity::Error, location);
}

size_t ErrorHandler::AddCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    
    size_t id = m_nextCallbackId++;
    m_callbacks.push_back({id, std::move(callback)});
    
    return id;
}

void ErrorHandler::RemoveCallback(size_t id) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    
    m_callbacks.erase(
        std::remove_if(m_callbacks.begin(), m_callbacks.end(),
            [id](const CallbackEntry& entry) { return entry.id == id; }),
        m_callbacks.end()
    );
}

void ErrorHandler::ClearCallbacks() {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_callbacks.clear();
}

ErrorHandler::ErrorStats ErrorHandler::GetStats() const {
    ErrorStats stats;
    stats.infoCount = m_infoCount.load(std::memory_order_acquire);
    stats.warningCount = m_warningCount.load(std::memory_order_acquire);
    stats.errorCount = m_errorCount.load(std::memory_order_acquire);
    stats.criticalCount = m_criticalCount.load(std::memory_order_acquire);
    stats.totalCount = m_totalCount.load(std::memory_order_acquire);
    return stats;
}

void ErrorHandler::ResetStats() {
    m_infoCount.store(0, std::memory_order_release);
    m_warningCount.store(0, std::memory_order_release);
    m_errorCount.store(0, std::memory_order_release);
    m_criticalCount.store(0, std::memory_order_release);
    m_totalCount.store(0, std::memory_order_release);
}

void ErrorHandler::UpdateStats(ErrorSeverity severity) {
    m_totalCount.fetch_add(1, std::memory_order_acq_rel);
    
    switch (severity) {
        case ErrorSeverity::Info:
            m_infoCount.fetch_add(1, std::memory_order_acq_rel);
            break;
        case ErrorSeverity::Warning:
            m_warningCount.fetch_add(1, std::memory_order_acq_rel);
            break;
        case ErrorSeverity::Error:
            m_errorCount.fetch_add(1, std::memory_order_acq_rel);
            break;
        case ErrorSeverity::Critical:
            m_criticalCount.fetch_add(1, std::memory_order_acq_rel);
            break;
    }
}

// ============================================================================
// 辅助函数实现
// ============================================================================

const char* ErrorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::Success: return "Success";
        
        // OpenGL 错误
        case ErrorCode::GLInvalidEnum: return "GLInvalidEnum";
        case ErrorCode::GLInvalidValue: return "GLInvalidValue";
        case ErrorCode::GLInvalidOperation: return "GLInvalidOperation";
        case ErrorCode::GLOutOfMemory: return "GLOutOfMemory";
        case ErrorCode::GLInvalidFramebufferOperation: return "GLInvalidFramebufferOperation";
        case ErrorCode::GLContextCreationFailed: return "GLContextCreationFailed";
        case ErrorCode::GLExtensionNotSupported: return "GLExtensionNotSupported";
        
        // 资源错误
        case ErrorCode::ResourceNotFound: return "ResourceNotFound";
        case ErrorCode::ResourceAlreadyExists: return "ResourceAlreadyExists";
        case ErrorCode::ResourceLoadFailed: return "ResourceLoadFailed";
        case ErrorCode::ResourceInvalidFormat: return "ResourceInvalidFormat";
        case ErrorCode::ResourceCorrupted: return "ResourceCorrupted";
        case ErrorCode::ResourceUploadFailed: return "ResourceUploadFailed";
        case ErrorCode::ResourceInUse: return "ResourceInUse";
        
        // 线程错误
        case ErrorCode::WrongThread: return "WrongThread";
        case ErrorCode::DeadlockDetected: return "DeadlockDetected";
        case ErrorCode::ThreadCreationFailed: return "ThreadCreationFailed";
        case ErrorCode::ThreadSynchronizationFailed: return "ThreadSynchronizationFailed";
        
        // 渲染错误
        case ErrorCode::ShaderCompileFailed: return "ShaderCompileFailed";
        case ErrorCode::ShaderLinkFailed: return "ShaderLinkFailed";
        case ErrorCode::ShaderUniformNotFound: return "ShaderUniformNotFound";
        case ErrorCode::TextureUploadFailed: return "TextureUploadFailed";
        case ErrorCode::MeshUploadFailed: return "MeshUploadFailed";
        case ErrorCode::RenderTargetInvalid: return "RenderTargetInvalid";
        
        // IO 错误
        case ErrorCode::FileNotFound: return "FileNotFound";
        case ErrorCode::FileOpenFailed: return "FileOpenFailed";
        case ErrorCode::FileReadFailed: return "FileReadFailed";
        case ErrorCode::FileWriteFailed: return "FileWriteFailed";
        case ErrorCode::PathInvalid: return "PathInvalid";
        
        // 初始化错误
        case ErrorCode::InitializationFailed: return "InitializationFailed";
        case ErrorCode::AlreadyInitialized: return "AlreadyInitialized";
        case ErrorCode::NotInitialized: return "NotInitialized";
        case ErrorCode::ConfigurationInvalid: return "ConfigurationInvalid";
        
        // Transform 错误
        case ErrorCode::TransformCircularReference: return "TransformCircularReference";
        case ErrorCode::TransformSelfReference: return "TransformSelfReference";
        case ErrorCode::TransformHierarchyTooDeep: return "TransformHierarchyTooDeep";
        case ErrorCode::TransformParentDestroyed: return "TransformParentDestroyed";
        case ErrorCode::TransformObjectDestroyed: return "TransformObjectDestroyed";
        case ErrorCode::TransformInvalidPosition: return "TransformInvalidPosition";
        case ErrorCode::TransformInvalidRotation: return "TransformInvalidRotation";
        case ErrorCode::TransformInvalidScale: return "TransformInvalidScale";
        case ErrorCode::TransformInvalidMatrix: return "TransformInvalidMatrix";
        case ErrorCode::TransformLockTimeout: return "TransformLockTimeout";
        
        // 通用错误
        case ErrorCode::NotImplemented: return "NotImplemented";
        case ErrorCode::InvalidArgument: return "InvalidArgument";
        case ErrorCode::NullPointer: return "NullPointer";
        case ErrorCode::OutOfRange: return "OutOfRange";
        case ErrorCode::OutOfMemory: return "OutOfMemory";
        case ErrorCode::InvalidState: return "InvalidState";
        case ErrorCode::OperationFailed: return "OperationFailed";
        case ErrorCode::Unknown: return "Unknown";
        
        default: return "UnknownErrorCode";
    }
}

const char* ErrorSeverityToString(ErrorSeverity severity) {
    switch (severity) {
        case ErrorSeverity::Info: return "Info";
        case ErrorSeverity::Warning: return "Warning";
        case ErrorSeverity::Error: return "Error";
        case ErrorSeverity::Critical: return "Critical";
        default: return "Unknown";
    }
}

const char* ErrorCategoryToString(ErrorCategory category) {
    switch (category) {
        case ErrorCategory::OpenGL: return "OpenGL";
        case ErrorCategory::Resource: return "Resource";
        case ErrorCategory::Threading: return "Threading";
        case ErrorCategory::Rendering: return "Rendering";
        case ErrorCategory::IO: return "IO";
        case ErrorCategory::Initialization: return "Initialization";
        case ErrorCategory::Transform: return "Transform";
        case ErrorCategory::Generic: return "Generic";
        default: return "Unknown";
    }
}

} // namespace Render

