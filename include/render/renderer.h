/*
 * Copyright (c) 2025 Li Chaoyu
 * 
 * This file is part of Render.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * For commercial licensing, please contact: 2052046346@qq.com
 */
#pragma once

#include "render/types.h"
#include "render/opengl_context.h"
#include "render/render_state.h"
#include "render/lighting/light_manager.h"
#include "render/render_layer.h"
#include "render/render_batching.h"
#include <memory>
#include <string>
#include <mutex>
#include <atomic>
#include <vector>
#include <unordered_map>

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
    uint32_t batchCount = 0;
    uint32_t originalDrawCalls = 0;
    uint32_t instancedDrawCalls = 0;
    uint32_t instancedInstances = 0;
    uint32_t batchedDrawCalls = 0;
    uint32_t fallbackDrawCalls = 0;
    uint32_t batchedTriangles = 0;
    uint32_t batchedVertices = 0;
    uint32_t fallbackBatches = 0;
    uint32_t workerProcessed = 0;
    uint32_t workerMaxQueueDepth = 0;
    float workerWaitTimeMs = 0.0f;
    uint32_t materialSwitchesOriginal = 0;
    uint32_t materialSwitchesSorted = 0;
    uint32_t materialSortKeyReady = 0;
    uint32_t materialSortKeyMissing = 0;
    
    void Reset() {
        drawCalls = 0;
        triangles = 0;
        vertices = 0;
        batchCount = 0;
        originalDrawCalls = 0;
        instancedDrawCalls = 0;
        instancedInstances = 0;
        batchedDrawCalls = 0;
        fallbackDrawCalls = 0;
        batchedTriangles = 0;
        batchedVertices = 0;
        fallbackBatches = 0;
        workerProcessed = 0;
        workerMaxQueueDepth = 0;
        workerWaitTimeMs = 0.0f;
        materialSwitchesOriginal = 0;
        materialSwitchesSorted = 0;
        materialSortKeyReady = 0;
        materialSortKeyMissing = 0;
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
     * 注意：返回的是上一帧的统计数据，因为HUD在PostFrame阶段读取，
     * 而PostFrame在FlushRenderQueue之前调用，所以这里返回上一帧的数据是合理的
     */
    RenderStats GetStats() const { 
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_lastFrameStats; 
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
     * @brief 获取渲染层级注册表
     */
    [[nodiscard]] RenderLayerRegistry& GetLayerRegistry() { return m_layerRegistry; }

    /**
     * @brief 获取渲染层级注册表（常量）
     */
    [[nodiscard]] const RenderLayerRegistry& GetLayerRegistry() const { return m_layerRegistry; }

    /**
     * @brief 获取光照管理器
     */
    Lighting::LightManager& GetLightManager() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_lightManager;
    }

    /**
     * @brief 获取光照管理器（常量）
     */
    const Lighting::LightManager& GetLightManager() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_lightManager;
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

    /**
     * @brief 设置批处理模式
     */
    void SetBatchingMode(BatchingMode mode);

    /**
     * @brief 获取当前批处理模式
     */
    [[nodiscard]] BatchingMode GetBatchingMode() const;
    
    /**
     * @brief 设置当前相机可见层级遮罩
     */
    void SetActiveLayerMask(uint32_t mask);

    /**
     * @brief 获取当前相机可见层级遮罩
     */
    [[nodiscard]] uint32_t GetActiveLayerMask() const;
    
    // ========================================================================
    // LOD 实例化渲染支持（阶段2.3：与批处理系统集成）
    // ========================================================================
    
    /**
     * @brief 设置 LOD 实例化渲染模式
     * 
     * 如果启用，MeshRenderSystem 将优先使用 LOD 实例化渲染
     * 如果禁用或不可用，将回退到普通批处理模式
     * 
     * @param enabled 是否启用 LOD 实例化渲染
     * 
     * @note 阶段2.3：与批处理系统集成
     * @note LOD 实例化渲染与 BatchingMode::GpuInstancing 兼容
     * @note 如果 BatchingMode 为 Disabled 或 CpuMerge，LOD 实例化仍可使用
     */
    void SetLODInstancingEnabled(bool enabled);
    
    /**
     * @brief 获取 LOD 实例化渲染是否启用
     * @return 如果启用返回 true
     */
    [[nodiscard]] bool IsLODInstancingEnabled() const;
    
    /**
     * @brief LOD 实例化渲染统计信息
     * 
     * 用于性能分析和调试
     */
    struct LODInstancingStats {
        size_t lodGroupCount = 0;        ///< LOD 组数量
        size_t totalInstances = 0;       ///< 总实例数
        size_t drawCalls = 0;            ///< Draw Call 数量
        size_t lod0Instances = 0;        ///< LOD0 实例数
        size_t lod1Instances = 0;        ///< LOD1 实例数
        size_t lod2Instances = 0;        ///< LOD2 实例数
        size_t lod3Instances = 0;        ///< LOD3 实例数
        size_t culledCount = 0;          ///< 剔除数量
        
        void Reset() {
            lodGroupCount = 0;
            totalInstances = 0;
            drawCalls = 0;
            lod0Instances = 0;
            lod1Instances = 0;
            lod2Instances = 0;
            lod3Instances = 0;
            culledCount = 0;
        }
    };
    
    /**
     * @brief 获取 LOD 实例化渲染统计信息
     * @return 统计信息（返回副本以保证线程安全）
     */
    [[nodiscard]] LODInstancingStats GetLODInstancingStats() const;
    
    /**
     * @brief 更新 LOD 实例化渲染统计信息（由 MeshRenderSystem 调用）
     * @param stats 新的统计信息
     * 
     * @note 此方法由 MeshRenderSystem 在每帧结束时调用
     */
    void UpdateLODInstancingStats(const LODInstancingStats& stats);
    
    /**
     * @brief 检查 LOD 实例化渲染是否可用
     * 
     * 检查条件：
     * 1. LOD 实例化渲染已启用
     * 2. 批处理模式兼容（如果 BatchingMode 为 GpuInstancing，则兼容）
     * 
     * @return 如果可用返回 true
     */
    [[nodiscard]] bool IsLODInstancingAvailable() const;
    
private:
    void UpdateStats();
    
    std::shared_ptr<OpenGLContext> m_context;
    std::shared_ptr<RenderState> m_renderState;
    
    std::atomic<bool> m_initialized;
    RenderStats m_stats;
    RenderStats m_lastFrameStats;  // 上一帧的统计数据，供HUD显示
    
    // 时间统计
    float m_deltaTime;
    float m_lastFrameTime;
    float m_fpsUpdateTimer;
    uint32_t m_frameCount;
    
    struct LayerItem {
        Renderable* renderable = nullptr;
        size_t submissionIndex = 0;
    };

    struct LayerBucket {
        RenderLayerId id{};
        uint32_t priority = 0;
        LayerSortPolicy sortPolicy = LayerSortPolicy::OpaqueMaterialFirst;
        uint8_t maskIndex = 0;
        std::vector<LayerItem> items;
    };
    
    // 辅助函数
    void SortLayerItems(std::vector<LayerItem>& items, const RenderLayerDescriptor& descriptor);
    void ApplyLayerOverrides(const RenderLayerDescriptor& descriptor, const RenderLayerState& state);
    [[nodiscard]] size_t CountPendingRenderables() const;

    BatchManager m_batchManager;
    BatchingMode m_batchingMode;
    Lighting::LightManager m_lightManager;
    RenderLayerRegistry m_layerRegistry;
    std::unordered_map<uint32_t, size_t> m_layerBucketLookup;
    std::vector<LayerBucket> m_layerBuckets;
    size_t m_submissionCounter = 0;
    std::atomic<uint32_t> m_activeLayerMask;
    
    // LOD 实例化渲染（阶段2.3）
    std::atomic<bool> m_lodInstancingEnabled;  ///< 是否启用 LOD 实例化渲染
    LODInstancingStats m_lodInstancingStats;    ///< LOD 实例化渲染统计信息
    
    // 延迟清除缓冲区标志：用于解决UI状态更新时的频闪问题
    // 当UI状态更新时，延迟清除可以确保在UI命令提交后才清除缓冲区
    bool m_needsClear = false;
    
    // 线程安全
    mutable std::mutex m_mutex;  // 保护所有可变状态
};

} // namespace Render

