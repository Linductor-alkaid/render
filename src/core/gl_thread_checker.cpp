#include "render/gl_thread_checker.h"
#include "render/logger.h"
#include "render/error.h"
#include <sstream>
#include <iomanip>

namespace Render {

GLThreadChecker::GLThreadChecker()
    : m_glThreadId()
    , m_registered(false)
    , m_terminateOnError(true) {
}

GLThreadChecker::~GLThreadChecker() {
}

GLThreadChecker& GLThreadChecker::GetInstance() {
    static GLThreadChecker instance;
    return instance;
}

void GLThreadChecker::RegisterGLThread() {
    std::thread::id currentThreadId = std::this_thread::get_id();
    
    if (m_registered.load()) {
        // 检查是否是同一个线程
        if (m_glThreadId == currentThreadId) {
            LOG_WARNING("OpenGL thread already registered for this thread");
            return;
        } else {
            // 不同线程尝试注册，这通常是一个错误
            std::ostringstream oss;
            oss << "GLThreadChecker: 尝试从不同线程注册 OpenGL 线程! "
                << "已注册线程 ID: " << m_glThreadId 
                << ", 当前线程 ID: " << currentThreadId;
            
            HANDLE_ERROR(RENDER_CRITICAL(ErrorCode::ThreadCreationFailed, oss.str()));
            
            if (m_terminateOnError.load()) {
                LOG_ERROR("CRITICAL: 由于 OpenGL 线程注册冲突而终止");
                std::terminate();
            }
            return;
        }
    }
    
    m_glThreadId = currentThreadId;
    m_registered.store(true);
    
    std::ostringstream oss;
    oss << "OpenGL thread registered. Thread ID: " << currentThreadId;
    LOG_INFO(oss.str());
}

void GLThreadChecker::UnregisterGLThread() {
    if (!m_registered.load()) {
        LOG_WARNING("Attempting to unregister OpenGL thread, but no thread is registered");
        return;
    }
    
    std::thread::id currentThreadId = std::this_thread::get_id();
    
    if (m_glThreadId != currentThreadId) {
        std::ostringstream oss;
        oss << "GLThreadChecker: 尝试从错误线程注销 OpenGL 线程! "
            << "已注册线程 ID: " << m_glThreadId 
            << ", 当前线程 ID: " << currentThreadId;
        
        HANDLE_ERROR(RENDER_CRITICAL(ErrorCode::ThreadSynchronizationFailed, oss.str()));
        
        if (m_terminateOnError.load()) {
            LOG_ERROR("CRITICAL: 由于从错误线程注销 OpenGL 线程而终止");
            std::terminate();
        }
        return;
    }
    
    m_registered.store(false);
    LOG_INFO("OpenGL thread unregistered");
}

bool GLThreadChecker::IsGLThread() const {
    if (!m_registered.load()) {
        return false;
    }
    
    return m_glThreadId == std::this_thread::get_id();
}

bool GLThreadChecker::ValidateGLThread(const char* file, int line, const char* function) const {
    // 如果还没有注册，只记录警告但不认为是错误
    if (!m_registered.load()) {
        LOG_WARNING("OpenGL thread not registered yet, skipping thread validation");
        return true;
    }
    
    std::thread::id currentThreadId = std::this_thread::get_id();
    
    if (m_glThreadId != currentThreadId) {
        // 构建错误消息
        std::ostringstream oss;
        oss << "GLThreadChecker: OpenGL 调用来自错误线程!\n"
            << "  期望线程 ID: " << m_glThreadId << "\n"
            << "  当前线程 ID: " << currentThreadId << "\n"
            << "  位置: " << file << ":" << line;
        
        if (function) {
            oss << " in " << function << "()";
        }
        
        HANDLE_ERROR(RENDER_CRITICAL(ErrorCode::WrongThread, oss.str()));
        
        // 如果设置了终止选项，立即终止程序
        if (m_terminateOnError.load()) {
            LOG_ERROR("CRITICAL: 由于 OpenGL 线程违规而终止");
            std::terminate();
        }
        
        return false;
    }
    
    return true;
}

} // namespace Render

