#include "render/renderer.h"
#include "render/logger.h"
#include <SDL3/SDL.h>

namespace Render {

Renderer* Renderer::Create() {
    return new Renderer();
}

void Renderer::Destroy(Renderer* renderer) {
    if (renderer) {
        renderer->Shutdown();
        delete renderer;
    }
}

Renderer::Renderer()
    : m_initialized(false)
    , m_deltaTime(0.0f)
    , m_lastFrameTime(0.0f)
    , m_fpsUpdateTimer(0.0f)
    , m_frameCount(0) {
    
    // 注意：unique_ptr的构造不需要锁保护，此时对象还未被其他线程访问
    m_context = std::make_unique<OpenGLContext>();
    m_renderState = std::make_unique<RenderState>();
}

Renderer::~Renderer() {
    Shutdown();
}

bool Renderer::Initialize(const std::string& title, int width, int height) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_initialized) {
        LOG_WARNING("Renderer already initialized");
        return true;
    }
    
    LOG_INFO("========================================");
    LOG_INFO("Initializing RenderEngine...");
    LOG_INFO("========================================");
    
    // 初始化 OpenGL 上下文
    if (!m_context->Initialize(title, width, height)) {
        LOG_ERROR("Failed to initialize OpenGL context");
        return false;
    }
    
    // 重置渲染状态
    m_renderState->Reset();
    
    m_lastFrameTime = static_cast<float>(SDL_GetTicks()) * 0.001f;
    m_initialized = true;
    
    LOG_INFO("========================================");
    LOG_INFO("RenderEngine initialized successfully!");
    LOG_INFO("========================================");
    
    return true;
}

void Renderer::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) {
        return;
    }
    
    LOG_INFO("Shutting down RenderEngine...");
    
    m_context->Shutdown();
    
    m_initialized = false;
    LOG_INFO("RenderEngine shut down successfully");
}

void Renderer::BeginFrame() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 更新时间
    float currentTime = static_cast<float>(SDL_GetTicks()) * 0.001f;
    m_deltaTime = currentTime - m_lastFrameTime;
    m_lastFrameTime = currentTime;
    
    // 重置帧统计
    m_stats.Reset();
}

void Renderer::EndFrame() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 更新统计信息
    UpdateStats();
    m_frameCount++;
}

void Renderer::Present() {
    // SwapBuffers 是线程安全的（由SDL/OpenGL处理）
    // 但为了保证调用顺序的一致性，仍然加锁
    std::lock_guard<std::mutex> lock(m_mutex);
    m_context->SwapBuffers();
}

void Renderer::Clear(bool colorBuffer, bool depthBuffer, bool stencilBuffer) {
    // RenderState 已经是线程安全的，不需要额外加锁
    m_renderState->Clear(colorBuffer, depthBuffer, stencilBuffer);
}

void Renderer::SetClearColor(const Color& color) {
    // RenderState 已经是线程安全的，不需要额外加锁
    m_renderState->SetClearColor(color);
}

void Renderer::SetClearColor(float r, float g, float b, float a) {
    // RenderState 已经是线程安全的，不需要额外加锁
    m_renderState->SetClearColor(Color(r, g, b, a));
}

void Renderer::SetWindowTitle(const std::string& title) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_context->SetWindowTitle(title);
}

void Renderer::SetWindowSize(int width, int height) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_context->SetWindowSize(width, height);
}

void Renderer::SetVSync(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_context->SetVSync(enable);
}

void Renderer::SetFullscreen(bool fullscreen) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_context->SetFullscreen(fullscreen);
}

int Renderer::GetWidth() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_context->GetWidth();
}

int Renderer::GetHeight() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_context->GetHeight();
}

void Renderer::UpdateStats() {
    m_stats.frameTime = m_deltaTime * 1000.0f; // 转换为毫秒
    
    // 每秒更新一次 FPS
    m_fpsUpdateTimer += m_deltaTime;
    if (m_fpsUpdateTimer >= 1.0f) {
        m_stats.fps = m_frameCount / m_fpsUpdateTimer;
        m_fpsUpdateTimer = 0.0f;
        m_frameCount = 0;
    }
}

} // namespace Render

