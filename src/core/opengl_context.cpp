#include "render/opengl_context.h"
#include "render/logger.h"
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
    if (m_initialized) {
        LOG_WARNING("OpenGLContext already initialized");
        return true;
    }
    
    LOG_INFO("Initializing OpenGL Context...");
    
    // 初始化 SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_ERROR(std::string("Failed to initialize SDL: ") + SDL_GetError());
        return false;
    }
    LOG_INFO("SDL initialized successfully");
    
    m_width = width;
    m_height = height;
    
    // 创建窗口
    if (!CreateWindow(title, width, height)) {
        LOG_ERROR("Failed to create window");
        return false;
    }
    
    // 创建 OpenGL 上下文
    if (!CreateGLContext(config)) {
        LOG_ERROR("Failed to create OpenGL context");
        return false;
    }
    
    // 初始化 GLAD
    if (!InitializeGLAD()) {
        LOG_ERROR("Failed to initialize GLAD");
        return false;
    }
    
    // 输出 OpenGL 信息
    LogGLInfo();
    
    // 设置视口
    glViewport(0, 0, width, height);
    
    // 启用深度测试
    glEnable(GL_DEPTH_TEST);
    
    // 启用面剔除
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    m_initialized = true;
    LOG_INFO("OpenGL Context initialized successfully");
    
    return true;
}

void OpenGLContext::Shutdown() {
    if (!m_initialized) {
        return;
    }
    
    LOG_INFO("Shutting down OpenGL Context...");
    
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
        glViewport(0, 0, width, height);
        LOG_INFO("Window resized to " + std::to_string(width) + "x" + std::to_string(height));
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
    const GLubyte* version = glGetString(GL_VERSION);
    return version ? std::string(reinterpret_cast<const char*>(version)) : "Unknown";
}

std::string OpenGLContext::GetGPUInfo() const {
    const GLubyte* renderer = glGetString(GL_RENDERER);
    return renderer ? std::string(reinterpret_cast<const char*>(renderer)) : "Unknown";
}

bool OpenGLContext::IsExtensionSupported(const std::string& extension) const {
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

} // namespace Render

