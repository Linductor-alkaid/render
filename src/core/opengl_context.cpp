#include "render/opengl_context.h"
#include "render/logger.h"
#include "render/error.h"
#include <glad/glad.h>

namespace Render {

OpenGLContext::OpenGLContext()
    : m_window(nullptr)
    , m_glContext(nullptr)
    , m_width(0)
    , m_height(0)
    , m_initialized(false)
    , m_vsyncEnabled(false) {
}

OpenGLContext::~OpenGLContext() {
    Shutdown();
}

bool OpenGLContext::Initialize(const std::string& title, 
                               int width, 
                               int height, 
                               const OpenGLConfig& config) {
    RENDER_TRY {
        if (m_initialized) {
            throw RENDER_WARNING(ErrorCode::AlreadyInitialized, 
                               "OpenGLContext: 上下文已经初始化");
        }
        
        LOG_INFO("Initializing OpenGL Context...");
        
        // 初始化 SDL
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            throw RENDER_ERROR(ErrorCode::InitializationFailed, 
                             "OpenGLContext: SDL 初始化失败: " + std::string(SDL_GetError()));
        }
        LOG_INFO("SDL initialized successfully");
        
        m_width = width;
        m_height = height;
        
        // 创建窗口
        if (!CreateWindow(title, width, height)) {
            throw RENDER_ERROR(ErrorCode::InitializationFailed, 
                             "OpenGLContext: 窗口创建失败");
        }
        
        // 创建 OpenGL 上下文
        if (!CreateGLContext(config)) {
            throw RENDER_ERROR(ErrorCode::GLContextCreationFailed, 
                             "OpenGLContext: OpenGL 上下文创建失败");
        }
        
        // 初始化 GLAD
        if (!InitializeGLAD()) {
            throw RENDER_ERROR(ErrorCode::InitializationFailed, 
                             "OpenGLContext: GLAD 初始化失败");
        }
        
        // 输出 OpenGL 信息
        LogGLInfo();
        
        // 注册 OpenGL 线程 - 必须在所有 OpenGL 调用之前
        GL_THREAD_REGISTER();
        LOG_INFO("OpenGL thread registered for thread safety checks");
        
        // 设置视口
        GL_THREAD_CHECK();
        glViewport(0, 0, width, height);
        
        // 启用深度测试
        GL_THREAD_CHECK();
        glEnable(GL_DEPTH_TEST);
        
        // 启用面剔除
        GL_THREAD_CHECK();
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        
        m_initialized = true;
        LOG_INFO("OpenGL Context initialized successfully");
        
        return true;
    }
    RENDER_CATCH {
        return false;
    }
}

void OpenGLContext::Shutdown() {
    if (!m_initialized) {
        return;
    }
    
    LOG_INFO("Shutting down OpenGL Context...");
    
    // 注销 OpenGL 线程
    GL_THREAD_UNREGISTER();
    
    if (m_glContext) {
        SDL_GL_DestroyContext(m_glContext);
        m_glContext = nullptr;
    }
    
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    
    SDL_Quit();
    
    m_initialized = false;
    LOG_INFO("OpenGL Context shut down successfully");
}

void OpenGLContext::SwapBuffers() {
    GL_THREAD_CHECK();
    if (m_window) {
        SDL_GL_SwapWindow(m_window);
    }
}

void OpenGLContext::SetVSync(bool enable) {
    if (enable) {
        if (SDL_GL_SetSwapInterval(1)) {
            m_vsyncEnabled = true;
            LOG_INFO("VSync enabled");
        } else {
            LOG_WARNING("Failed to enable VSync");
        }
    } else {
        if (SDL_GL_SetSwapInterval(0)) {
            m_vsyncEnabled = false;
            LOG_INFO("VSync disabled");
        } else {
            LOG_WARNING("Failed to disable VSync");
        }
    }
}

void OpenGLContext::SetWindowTitle(const std::string& title) {
    if (m_window) {
        SDL_SetWindowTitle(m_window, title.c_str());
    }
}

void OpenGLContext::SetWindowSize(int width, int height) {
    if (m_window) {
        SDL_SetWindowSize(m_window, width, height);
        m_width = width;
        m_height = height;
        GL_THREAD_CHECK();
        glViewport(0, 0, width, height);
        LOG_INFO("Window resized to " + std::to_string(width) + "x" + std::to_string(height));
        
        // 通知所有已注册的观察者
        NotifyResizeCallbacks(width, height);
    }
}

void OpenGLContext::SetFullscreen(bool fullscreen) {
    if (m_window) {
        bool success = SDL_SetWindowFullscreen(m_window, fullscreen);
        if (success) {
            LOG_INFO(fullscreen ? "Fullscreen enabled" : "Fullscreen disabled");
        } else {
            LOG_WARNING("Failed to change fullscreen mode");
        }
    }
}

std::string OpenGLContext::GetGLVersion() const {
    GL_THREAD_CHECK();
    const GLubyte* version = glGetString(GL_VERSION);
    return version ? std::string(reinterpret_cast<const char*>(version)) : "Unknown";
}

std::string OpenGLContext::GetGPUInfo() const {
    GL_THREAD_CHECK();
    const GLubyte* renderer = glGetString(GL_RENDERER);
    return renderer ? std::string(reinterpret_cast<const char*>(renderer)) : "Unknown";
}

bool OpenGLContext::IsExtensionSupported(const std::string& extension) const {
    GL_THREAD_CHECK();
    GLint numExtensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
    
    for (GLint i = 0; i < numExtensions; ++i) {
        const GLubyte* ext = glGetStringi(GL_EXTENSIONS, i);
        if (ext && extension == reinterpret_cast<const char*>(ext)) {
            return true;
        }
    }
    
    return false;
}

bool OpenGLContext::CreateWindow(const std::string& title, int width, int height) {
    // 设置 SDL OpenGL 属性
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    
    #ifdef _DEBUG
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
    #endif
    
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
    
    // 创建窗口
    m_window = SDL_CreateWindow(
        title.c_str(),
        width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
    
    if (!m_window) {
        LOG_ERROR(std::string("Failed to create window: ") + SDL_GetError());
        return false;
    }
    
    LOG_INFO("Window created: " + std::to_string(width) + "x" + std::to_string(height));
    return true;
}

bool OpenGLContext::CreateGLContext(const OpenGLConfig& config) {
    m_glContext = SDL_GL_CreateContext(m_window);
    if (!m_glContext) {
        LOG_ERROR(std::string("Failed to create OpenGL context: ") + SDL_GetError());
        return false;
    }
    
    if (!SDL_GL_MakeCurrent(m_window, m_glContext)) {
        LOG_ERROR(std::string("Failed to make context current: ") + SDL_GetError());
        return false;
    }
    
    LOG_INFO("OpenGL context created");
    return true;
}

bool OpenGLContext::InitializeGLAD() {
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        LOG_ERROR("Failed to initialize GLAD");
        return false;
    }
    
    LOG_INFO("GLAD initialized");
    return true;
}

void OpenGLContext::LogGLInfo() {
    // 注意：这个函数在 GL_THREAD_REGISTER 之前调用，所以不能使用 GL_THREAD_CHECK
    // GetGLVersion 和 GetGPUInfo 内部有自己的检查，但此时会跳过（因为还未注册）
    LOG_INFO("OpenGL Information:");
    LOG_INFO("  Version: " + GetGLVersion());
    LOG_INFO("  Vendor: " + std::string(reinterpret_cast<const char*>(glGetString(GL_VENDOR))));
    LOG_INFO("  Renderer: " + GetGPUInfo());
    LOG_INFO("  GLSL Version: " + std::string(reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION))));
    
    GLint majorVersion, minorVersion;
    glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
    glGetIntegerv(GL_MINOR_VERSION, &minorVersion);
    LOG_INFO("  Context Version: " + std::to_string(majorVersion) + "." + std::to_string(minorVersion));
}

void OpenGLContext::AddResizeCallback(WindowResizeCallback callback) {
    if (callback) {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        m_resizeCallbacks.push_back(std::move(callback));
        LOG_DEBUG("OpenGLContext: 添加了窗口大小变化回调，当前回调数量: " + 
                  std::to_string(m_resizeCallbacks.size()));
    }
}

void OpenGLContext::ClearResizeCallbacks() {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    size_t count = m_resizeCallbacks.size();
    m_resizeCallbacks.clear();
    LOG_DEBUG("OpenGLContext: 清除了 " + std::to_string(count) + " 个窗口大小变化回调");
}

void OpenGLContext::NotifyResizeCallbacks(int width, int height) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    
    if (m_resizeCallbacks.empty()) {
        return;
    }
    
    LOG_DEBUG("OpenGLContext: 通知 " + std::to_string(m_resizeCallbacks.size()) + 
              " 个回调窗口大小变化: " + std::to_string(width) + "x" + std::to_string(height));
    
    for (const auto& callback : m_resizeCallbacks) {
        if (callback) {
            try {
                callback(width, height);
            }
            catch (const std::exception& e) {
                LOG_ERROR("OpenGLContext: 窗口大小变化回调执行失败: " + std::string(e.what()));
            }
            catch (...) {
                LOG_ERROR("OpenGLContext: 窗口大小变化回调执行失败: 未知异常");
            }
        }
    }
}

} // namespace Render

