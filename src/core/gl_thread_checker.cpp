#include "render/gl_thread_checker.h"
#include "render/logger.h"
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
            oss << "Attempting to register OpenGL thread from different thread! "
                << "Registered thread ID: " << m_glThreadId 
                << ", Current thread ID: " << currentThreadId;
            LOG_ERROR(oss.str());
            
            if (m_terminateOnError.load()) {
                LOG_ERROR("CRITICAL: Terminating due to OpenGL thread registration conflict");
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
        oss << "Attempting to unregister OpenGL thread from wrong thread! "
            << "Registered thread ID: " << m_glThreadId 
            << ", Current thread ID: " << currentThreadId;
        LOG_ERROR(oss.str());
        
        if (m_terminateOnError.load()) {
            LOG_ERROR("CRITICAL: Terminating due to OpenGL thread unregistration from wrong thread");
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
        oss << "OpenGL call from wrong thread!\n"
            << "  Expected thread ID: " << m_glThreadId << "\n"
            << "  Current thread ID:  " << currentThreadId << "\n"
            << "  Location: " << file << ":" << line;
        
        if (function) {
            oss << " in " << function << "()";
        }
        
        LOG_ERROR(oss.str());
        
        // 如果设置了终止选项，立即终止程序
        if (m_terminateOnError.load()) {
            LOG_ERROR("CRITICAL: Terminating due to OpenGL thread violation");
            std::terminate();
        }
        
        return false;
    }
    
    return true;
}

} // namespace Render

