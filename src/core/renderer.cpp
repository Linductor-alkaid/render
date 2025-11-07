#include "render/renderer.h"
#include "render/renderable.h"
#include "render/logger.h"
#include "render/error.h"
#include "render/resource_manager.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdint>

namespace Render {

namespace {

BatchableItem CreateBatchableItem(Renderable* renderable) {
    BatchableItem item{};
    item.renderable = renderable;

    if (!renderable) {
        return item;
    }

    item.key.layerID = renderable->GetLayerID();
    item.key.renderableType = renderable->GetType();

    switch (renderable->GetType()) {
        case RenderableType::Mesh: {
            item.type = BatchItemType::Mesh;

            auto* meshRenderable = static_cast<MeshRenderable*>(renderable);
            auto mesh = meshRenderable->GetMesh();
            auto material = meshRenderable->GetMaterial();

            if (!mesh || !material) {
                item.batchable = false;
                return item;
            }

            auto shader = material->GetShader();
            if (!shader) {
                item.batchable = false;
                return item;
            }

            const bool hasIndices = mesh->GetIndexCount() > 0;

            item.meshData.mesh = mesh;
            item.meshData.material = material;
            item.meshData.materialOverride = meshRenderable->GetMaterialOverride();
            item.meshData.hasMaterialOverride = item.meshData.materialOverride.HasAnyOverride();
            item.meshData.castShadows = meshRenderable->GetCastShadows();
            item.meshData.receiveShadows = meshRenderable->GetReceiveShadows();
            item.meshData.modelMatrix = renderable->GetWorldMatrix();

            item.key.materialHandle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(material.get()));
            item.key.shaderHandle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(shader.get()));
            item.key.blendMode = material->GetBlendMode();
            item.key.cullFace = material->GetCullFace();
            item.key.depthTest = material->GetDepthTest();
            item.key.depthWrite = material->GetDepthWrite();
            item.key.castShadows = item.meshData.castShadows;
            item.key.receiveShadows = item.meshData.receiveShadows;

            bool isTransparent = false;
            const auto blendMode = material->GetBlendMode();
            if (blendMode == BlendMode::Alpha || blendMode == BlendMode::Additive) {
                isTransparent = true;
            }

            if (material->GetOpacity() < 1.0f) {
                isTransparent = true;
            }

            if (item.meshData.materialOverride.opacity.has_value() &&
                item.meshData.materialOverride.opacity.value() < 1.0f) {
                isTransparent = true;
            }

            item.isTransparent = isTransparent;
            item.batchable = hasIndices && !item.isTransparent && !item.meshData.hasMaterialOverride;
            item.instanceEligible = hasIndices && !item.meshData.hasMaterialOverride && !item.isTransparent;
            return item;
        }
        default:
            item.batchable = false;
            return item;
    }
}

} // namespace

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
    , m_frameCount(0)
    , m_batchingMode(BatchingMode::Disabled) {
    
    // 注意：构造阶段尚未被其他线程访问
    m_context = std::make_shared<OpenGLContext>();
    m_renderState = std::make_shared<RenderState>();
    m_batchManager.SetResourceManager(&ResourceManager::GetInstance());
}

Renderer::~Renderer() {
    Shutdown();
}

bool Renderer::Initialize(const std::string& title, int width, int height) {
    RENDER_TRY {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_initialized) {
            throw RENDER_WARNING(ErrorCode::AlreadyInitialized, 
                               "Renderer: 渲染器已经初始化");
        }
        
        LOG_INFO("========================================");
        LOG_INFO("Initializing RenderEngine...");
        LOG_INFO("========================================");
        
        // 初始化 OpenGL 上下文
        if (!m_context->Initialize(title, width, height)) {
            throw RENDER_ERROR(ErrorCode::InitializationFailed, 
                             "Renderer: OpenGL 上下文初始化失败");
        }
        
        // 重置渲染状态
        m_renderState->Reset();
        m_batchManager.SetMode(m_batchingMode);
        
        m_lastFrameTime = static_cast<float>(SDL_GetTicks()) * 0.001f;
        m_initialized = true;
        
        LOG_INFO("========================================");
        LOG_INFO("RenderEngine initialized successfully!");
        LOG_INFO("========================================");
        
        return true;
    }
    RENDER_CATCH {
        return false;
    }
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
    m_batchManager.Reset();
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

// ========================================================================
// Renderable 支持（ECS 集成）
// ========================================================================

void Renderer::SubmitRenderable(Renderable* renderable) {
    if (!renderable) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    m_renderQueue.push_back(renderable);
}

void Renderer::FlushRenderQueue() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_renderQueue.empty()) {
        return;
    }
    
    // 排序渲染队列
    SortRenderQueue();

    m_stats.originalDrawCalls = static_cast<uint32_t>(m_renderQueue.size());

    m_batchManager.SetMode(m_batchingMode);
    m_batchManager.Reset();

    for (auto* renderable : m_renderQueue) {
        if (!renderable || !renderable->IsVisible()) {
            continue;
        }

        m_batchManager.AddItem(CreateBatchableItem(renderable));
    }

    auto flushResult = m_batchManager.Flush(m_renderState.get());
    m_stats.drawCalls += flushResult.drawCalls;
    m_stats.batchCount += flushResult.batchCount;
    m_stats.batchedDrawCalls += flushResult.batchedDrawCalls;
    m_stats.fallbackDrawCalls += flushResult.fallbackDrawCalls;
    m_stats.batchedTriangles += flushResult.batchedTriangles;
    m_stats.batchedVertices += flushResult.batchedVertices;
    m_stats.fallbackBatches += flushResult.fallbackBatches;
    m_stats.instancedInstances += flushResult.instancedInstances;
    m_stats.workerProcessed += flushResult.workerProcessed;
    m_stats.workerMaxQueueDepth = std::max(m_stats.workerMaxQueueDepth, flushResult.workerMaxQueueDepth);
    m_stats.workerWaitTimeMs += flushResult.workerWaitTimeMs;

    if (m_batchingMode == BatchingMode::GpuInstancing) {
        m_stats.instancedDrawCalls += flushResult.instancedDrawCalls;
    }

    if (flushResult.batchCount > 0 || flushResult.fallbackBatches > 0) {
        static uint32_t s_batchFlushLogCounter = 0;
        constexpr uint32_t kBatchLogInterval = 120;
        ++s_batchFlushLogCounter;

        const bool hasFallback = (flushResult.fallbackDrawCalls > 0 || flushResult.fallbackBatches > 0);
        const bool intervalReached = (s_batchFlushLogCounter >= kBatchLogInterval);
        const bool shouldLog = hasFallback || intervalReached;

        if (shouldLog) {
            Logger::GetInstance().DebugFormat(
                "[Renderer] Batch flush: batches=%u, batchedDraw=%u, instancedDraw=%u, instances=%u, fallbackDraw=%u, fallbackBatches=%u, triangles=%u, vertices=%u, workerProcessed=%u, workerMaxQueue=%u, workerWaitMs=%.3f",
                flushResult.batchCount,
                flushResult.batchedDrawCalls,
                flushResult.instancedDrawCalls,
                flushResult.instancedInstances,
                flushResult.fallbackDrawCalls,
                flushResult.fallbackBatches,
                flushResult.batchedTriangles,
                flushResult.batchedVertices,
                flushResult.workerProcessed,
                flushResult.workerMaxQueueDepth,
                flushResult.workerWaitTimeMs
            );

            if (intervalReached) {
                s_batchFlushLogCounter = 0;
            }
        }
    }

    // 清空队列
    m_renderQueue.clear();
}

void Renderer::ClearRenderQueue() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_renderQueue.clear();
    m_batchManager.Reset();
}

size_t Renderer::GetRenderQueueSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_renderQueue.size();
}

void Renderer::SetBatchingMode(BatchingMode mode) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_batchingMode = mode;
    m_batchManager.SetMode(mode);
}

BatchingMode Renderer::GetBatchingMode() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_batchingMode;
}

void Renderer::SortRenderQueue() {
    // 按以下优先级排序：
    // 1. 层级 (layerID) - 低层级先渲染
    // 2. 渲染优先级 (renderPriority) - 低优先级先渲染
    // 3. 类型 (为了批处理)
    
    std::sort(m_renderQueue.begin(), m_renderQueue.end(),
        [](const Renderable* a, const Renderable* b) {
            // 先按层级排序
            if (a->GetLayerID() != b->GetLayerID()) {
                return a->GetLayerID() < b->GetLayerID();
            }
            
            // 再按渲染优先级排序
            if (a->GetRenderPriority() != b->GetRenderPriority()) {
                return a->GetRenderPriority() < b->GetRenderPriority();
            }
            
            // 最后按类型排序（相同类型一起渲染，提高批处理效率）
            return static_cast<int>(a->GetType()) < static_cast<int>(b->GetType());
        });
}

} // namespace Render

