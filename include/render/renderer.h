#pragma once

#include "render/types.h"
#include "render/opengl_context.h"
#include "render/render_state.h"
#include <memory>
#include <string>
#include <mutex>
#include <atomic>
#include <vector>

namespace Render {

// 前向声明
class Renderable;

/**
 * @brief 渲染统计信息
 */
struct RenderStats {
    uint32_t drawCalls = 0;
    uint32_t triangles = 0;
    uint32_t vertices = 0;
    float frameTime = 0.0f;
    float fps = 0.0f;
    
    void Reset() {
        drawCalls = 0;
        triangles = 0;
        vertices = 0;
    }
};

/**
 * @brief 主渲染器类
 * 
 * 提供高层渲染接口，管理渲染上下文和状态
 * 
 * 线程安全：
 * - 所有公共方法都是线程安全的
 * - 使用互斥锁保护内部状态
 * - 注意：OpenGL 调用需要在创建上下文的线程中执行
 *   （通常是主线程）
 */
class Renderer {
public:
    /**
     * @brief 创建渲染器实例
     */
    static Renderer* Create();
    
    /**
     * @brief 销毁渲染器实例
     */
    static void Destroy(Renderer* renderer);
    
    Renderer();
    ~Renderer();
    
    /**
     * @brief 初始化渲染器
     * @param title 窗口标题
     * @param width 窗口宽度
     * @param height 窗口高度
     * @return 成功返回 true，失败返回 false
     */
    bool Initialize(const std::string& title = "RenderEngine", 
                   int width = 1920, 
                   int height = 1080);
    
    /**
     * @brief 关闭渲染器
     */
    void Shutdown();
    
    /**
     * @brief 开始新的一帧
     */
    void BeginFrame();
    
    /**
     * @brief 结束当前帧
     */
    void EndFrame();
    
    /**
     * @brief 呈现渲染结果
     */
    void Present();
    
    /**
     * @brief 清空缓冲区
     */
    void Clear(bool colorBuffer = true, bool depthBuffer = true, bool stencilBuffer = false);
    
    /**
     * @brief 设置清屏颜色
     */
    void SetClearColor(const Color& color);
    
    /**
     * @brief 设置清屏颜色（分量）
     */
    void SetClearColor(float r, float g, float b, float a = 1.0f);
    
    /**
     * @brief 设置窗口标题
     */
    void SetWindowTitle(const std::string& title);
    
    /**
     * @brief 设置窗口大小
     */
    void SetWindowSize(int width, int height);
    
    /**
     * @brief 设置 VSync
     */
    void SetVSync(bool enable);
    
    /**
     * @brief 设置全屏模式
     */
    void SetFullscreen(bool fullscreen);
    
    /**
     * @brief 获取窗口宽度
     */
    int GetWidth() const;
    
    /**
     * @brief 获取窗口高度
     */
    int GetHeight() const;
    
    /**
     * @brief 获取帧时间（秒）
     */
    float GetDeltaTime() const { 
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_deltaTime; 
    }
    
    /**
     * @brief 获取 FPS
     */
    float GetFPS() const { 
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stats.fps; 
    }
    
    /**
     * @brief 获取渲染统计信息（返回副本以保证线程安全）
     */
    RenderStats GetStats() const { 
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stats; 
    }
    
    /**
     * @brief 获取 OpenGL 上下文
     * 返回 shared_ptr，避免悬空指针
     */
    std::shared_ptr<OpenGLContext> GetContext() const { 
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_context; 
    }
    
    /**
     * @brief 获取渲染状态管理器
     */
    std::shared_ptr<RenderState> GetRenderState() const { 
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_renderState; 
    }
    
    /**
     * @brief 检查是否已初始化
     */
    bool IsInitialized() const { return m_initialized; }
    
    // ========================================================================
    // Renderable 支持（ECS 集成）
    // ========================================================================
    
    /**
     * @brief 提交 Renderable 对象到渲染队列
     * @param renderable Renderable 对象指针
     * 
     * 注意：Renderable 对象必须在 FlushRenderQueue 调用前保持有效
     */
    void SubmitRenderable(Renderable* renderable);
    
    /**
     * @brief 渲染所有提交的 Renderable 对象
     * 
     * 会按以下顺序排序：
     * 1. 按层级 (layerID) 排序
     * 2. 按材质排序（减少状态切换）
     * 3. 按渲染优先级排序
     */
    void FlushRenderQueue();
    
    /**
     * @brief 清空渲染队列
     */
    void ClearRenderQueue();
    
    /**
     * @brief 获取渲染队列中的对象数量
     */
    [[nodiscard]] size_t GetRenderQueueSize() const;
    
private:
    void UpdateStats();
    
    std::shared_ptr<OpenGLContext> m_context;
    std::shared_ptr<RenderState> m_renderState;
    
    std::atomic<bool> m_initialized;
    RenderStats m_stats;
    
    // 时间统计
    float m_deltaTime;
    float m_lastFrameTime;
    float m_fpsUpdateTimer;
    uint32_t m_frameCount;
    
    // 渲染队列（ECS 集成）
    std::vector<Renderable*> m_renderQueue;
    
    // 辅助函数
    void SortRenderQueue();
    
    // 线程安全
    mutable std::mutex m_mutex;  // 保护所有可变状态
};

} // namespace Render

