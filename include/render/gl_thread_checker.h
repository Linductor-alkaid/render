#pragma once

#include <thread>
#include <atomic>
#include <string>

namespace Render {

/**
 * @brief OpenGL 线程安全检查器
 * 
 * 用于确保所有 OpenGL 调用都在创建上下文的线程中执行。
 * 这是一个单例类，在整个应用程序中共享。
 * 
 * 使用方法：
 * 1. 在创建 OpenGL 上下文后调用 RegisterGLThread()
 * 2. 在任何 OpenGL 调用前使用 GL_THREAD_CHECK() 宏
 * 3. 在 Shutdown 时调用 UnregisterGLThread()
 * 
 * @note 这个类是线程安全的
 */
class GLThreadChecker {
public:
    /**
     * @brief 获取单例实例
     */
    static GLThreadChecker& GetInstance();
    
    /**
     * @brief 注册 OpenGL 上下文线程
     * 
     * 应该在 OpenGL 上下文创建成功后立即调用。
     * 如果已经注册过，会记录警告但不会失败。
     */
    void RegisterGLThread();
    
    /**
     * @brief 注销 OpenGL 上下文线程
     * 
     * 应该在 OpenGL 上下文销毁时调用。
     */
    void UnregisterGLThread();
    
    /**
     * @brief 检查当前线程是否是 OpenGL 线程
     * 
     * @return 如果当前线程是注册的 OpenGL 线程返回 true
     */
    bool IsGLThread() const;
    
    /**
     * @brief 验证当前线程是否是 OpenGL 线程
     * 
     * 如果不是 OpenGL 线程，会记录错误日志。
     * 
     * @param file 调用文件名 (__FILE__)
     * @param line 调用行号 (__LINE__)
     * @param function 调用函数名 (可选)
     * @return 如果当前线程是 OpenGL 线程返回 true
     */
    bool ValidateGLThread(const char* file, int line, const char* function = nullptr) const;
    
    /**
     * @brief 检查是否已经注册了 OpenGL 线程
     */
    bool IsRegistered() const { return m_registered.load(); }
    
    /**
     * @brief 获取 OpenGL 线程 ID（用于调试）
     */
    std::thread::id GetGLThreadId() const { return m_glThreadId; }
    
    /**
     * @brief 设置是否在检测到错误时终止程序（默认为 true）
     * 
     * @param terminate 如果为 true，检测到线程错误时会调用 std::terminate()
     */
    void SetTerminateOnError(bool terminate) { m_terminateOnError.store(terminate); }
    
    /**
     * @brief 获取是否在检测到错误时终止程序
     */
    bool GetTerminateOnError() const { return m_terminateOnError.load(); }
    
private:
    GLThreadChecker();
    ~GLThreadChecker();
    
    // 禁止拷贝和赋值
    GLThreadChecker(const GLThreadChecker&) = delete;
    GLThreadChecker& operator=(const GLThreadChecker&) = delete;
    
    std::thread::id m_glThreadId;                // OpenGL 线程 ID
    std::atomic<bool> m_registered;              // 是否已注册
    std::atomic<bool> m_terminateOnError;        // 错误时是否终止程序
};

} // namespace Render

/**
 * @brief OpenGL 线程检查宏
 * 
 * 在任何 OpenGL 调用前使用此宏，例如：
 * 
 * @code
 * GL_THREAD_CHECK();
 * glClear(GL_COLOR_BUFFER_BIT);
 * @endcode
 * 
 * 在调试模式下，这个宏会进行完整的线程检查。
 * 在发布模式下，可以通过定义 GL_DISABLE_THREAD_CHECK 来禁用检查以提高性能。
 */
#if defined(_DEBUG) || !defined(GL_DISABLE_THREAD_CHECK)
    #define GL_THREAD_CHECK() \
        do { \
            Render::GLThreadChecker::GetInstance().ValidateGLThread(__FILE__, __LINE__, __FUNCTION__); \
        } while(0)
#else
    #define GL_THREAD_CHECK() ((void)0)
#endif

/**
 * @brief 注册 OpenGL 线程宏（在上下文初始化后调用）
 */
#define GL_THREAD_REGISTER() \
    Render::GLThreadChecker::GetInstance().RegisterGLThread()

/**
 * @brief 注销 OpenGL 线程宏（在上下文销毁前调用）
 */
#define GL_THREAD_UNREGISTER() \
    Render::GLThreadChecker::GetInstance().UnregisterGLThread()

