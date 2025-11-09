#include "render/renderer.h"
#include "render/renderable.h"
#include "render/logger.h"
#include "render/error.h"
#include "render/resource_manager.h"
#include "render/sprite/sprite_batcher.h"
#include "render/material_sort_key.h"
#include "render/text/text.h"
#include "render/material_state_cache.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <cmath>

namespace Render {

namespace {

uint32_t HashCombine(uint32_t seed, uint32_t value) {
    seed ^= value + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    return seed;
}

uint32_t HashFloat(float value) {
    return static_cast<uint32_t>(std::hash<float>{}(value));
}

uint32_t HashColor(const Color& color) {
    uint32_t seed = 0;
    seed = HashCombine(seed, HashFloat(color.r));
    seed = HashCombine(seed, HashFloat(color.g));
    seed = HashCombine(seed, HashFloat(color.b));
    seed = HashCombine(seed, HashFloat(color.a));
    return seed;
}

uint32_t HashPointer(const void* ptr) {
    const uintptr_t value = reinterpret_cast<uintptr_t>(ptr);
    uint32_t seed = 0;
    seed = HashCombine(seed, static_cast<uint32_t>(value & 0xFFFFFFFFu));
    seed = HashCombine(seed, static_cast<uint32_t>((value >> 32) & 0xFFFFFFFFu));
    return seed;
}

MaterialSortKey BuildMeshRenderableSortKey(MeshRenderable* meshRenderable) {
    MaterialSortKey key{};
    if (!meshRenderable) {
        return key;
    }

    Ref<Material> material = meshRenderable->GetMaterial();
    const MaterialOverride overrideData = meshRenderable->GetMaterialOverride();
    const uint32_t overrideHash = overrideData.ComputeHash();

    uint32_t pipelineFlags = MaterialPipelineFlags_None;
    if (meshRenderable->GetCastShadows()) {
        pipelineFlags |= MaterialPipelineFlags_CastShadow;
    }
    if (meshRenderable->GetReceiveShadows()) {
        pipelineFlags |= MaterialPipelineFlags_ReceiveShadow;
    }

    key = BuildMaterialSortKey(material.get(), overrideHash, pipelineFlags);
    return key;
}

MaterialSortKey BuildModelRenderableSortKey(ModelRenderable* modelRenderable) {
    MaterialSortKey key{};
    if (!modelRenderable) {
        return key;
    }

    ModelPtr model = modelRenderable->GetModel();
    if (!model) {
        return key;
    }

    uint32_t overrideHash = 0;
    uint32_t pipelineFlags = MaterialPipelineFlags_None;
    Material* primaryMaterial = nullptr;
    bool anyBlend = false;
    BlendMode resolvedBlend = BlendMode::None;

    bool transparent = false;

    model->AccessParts([&](const std::vector<ModelPart>& parts) {
        for (const auto& part : parts) {
            if (part.castShadows) {
                pipelineFlags |= MaterialPipelineFlags_CastShadow;
            }
            if (part.receiveShadows) {
                pipelineFlags |= MaterialPipelineFlags_ReceiveShadow;
            }

            if (!part.material) {
                continue;
            }

            if (!primaryMaterial) {
                primaryMaterial = part.material.get();
            }

            overrideHash = HashCombine(overrideHash, HashPointer(part.material.get()));
            overrideHash = HashCombine(overrideHash, HashFloat(part.material->GetOpacity()));

            if (auto shader = part.material->GetShader()) {
                overrideHash = HashCombine(overrideHash, HashPointer(shader.get()));
            }

            const BlendMode blend = part.material->GetBlendMode();
            if (!anyBlend) {
                resolvedBlend = blend;
                anyBlend = true;
            } else if (resolvedBlend != blend) {
                resolvedBlend = BlendMode::Custom;
            }

            if (blend == BlendMode::Alpha || blend == BlendMode::Additive || part.material->GetOpacity() < 1.0f) {
                transparent = true;
            }
        }
    });

    key = BuildMaterialSortKey(primaryMaterial, overrideHash, pipelineFlags);
    key.materialID = overrideHash;
    if (anyBlend) {
        key.blendMode = resolvedBlend;
    }

    modelRenderable->SetTransparentHint(transparent);

    return key;
}

MaterialSortKey BuildSpriteRenderableSortKey(SpriteRenderable* spriteRenderable) {
    MaterialSortKey key{};
    if (!spriteRenderable) {
        return key;
    }

    Ref<Texture> texture = spriteRenderable->GetTexture();
    const Color tint = spriteRenderable->GetTintColor();

    uint32_t overrideHash = 0;
    if (texture) {
        overrideHash = HashCombine(overrideHash, HashPointer(texture.get()));
    }
    overrideHash = HashCombine(overrideHash, HashColor(tint));

    uint32_t pipelineFlags = MaterialPipelineFlags_None;
    if (spriteRenderable->GetLayerID() >= 800u) {
        pipelineFlags |= MaterialPipelineFlags_ScreenSpace;
    }

    key = BuildMaterialSortKey(nullptr, overrideHash, pipelineFlags);
    key.blendMode = BlendMode::Alpha;
    key.cullFace = CullFace::None;
    key.depthTest = false;
    key.depthWrite = false;

    if (texture) {
        key.materialID = HashPointer(texture.get());
    }

    Ref<Mesh> quadMesh;
    Ref<Shader> shader;
    if (SpriteRenderable::AcquireSharedResources(quadMesh, shader) && shader) {
        key.shaderID = shader->GetProgramID();
    }

    return key;
}

MaterialSortKey BuildTextRenderableSortKey(TextRenderable* textRenderable) {
    MaterialSortKey key{};
    if (!textRenderable) {
        return key;
    }

    Ref<Text> text = textRenderable->GetText();
    if (!text) {
        return key;
    }

    text->EnsureUpdated();
    Ref<Texture> texture = text->GetTexture();
    const Color color = text->GetColor();

    uint32_t overrideHash = HashColor(color);
    if (texture) {
        overrideHash = HashCombine(overrideHash, HashPointer(texture.get()));
    }

    uint32_t pipelineFlags = MaterialPipelineFlags_ScreenSpace;

    key = BuildMaterialSortKey(nullptr, overrideHash, pipelineFlags);
    key.blendMode = BlendMode::Alpha;
    key.cullFace = CullFace::None;
    key.depthTest = false;
    key.depthWrite = false;

    if (texture) {
        key.materialID = HashPointer(texture.get());
    }

    return key;
}

void EnsureMaterialSortKey(Renderable* renderable) {
    if (!renderable) {
        return;
    }

    if (renderable->HasMaterialSortKey() && !renderable->IsMaterialSortKeyDirty()) {
        return;
    }

    MaterialSortKey key{};
    bool computed = false;

    switch (renderable->GetType()) {
        case RenderableType::Mesh: {
            auto* meshRenderable = static_cast<MeshRenderable*>(renderable);
            key = BuildMeshRenderableSortKey(meshRenderable);
            computed = true;
            break;
        }
        case RenderableType::Model: {
            auto* modelRenderable = static_cast<ModelRenderable*>(renderable);
            key = BuildModelRenderableSortKey(modelRenderable);
            computed = true;
            break;
        }
        case RenderableType::Sprite: {
            auto* spriteRenderable = static_cast<SpriteRenderable*>(renderable);
            key = BuildSpriteRenderableSortKey(spriteRenderable);
            computed = true;
            break;
        }
        case RenderableType::Text: {
            auto* textRenderable = static_cast<TextRenderable*>(renderable);
            key = BuildTextRenderableSortKey(textRenderable);
            computed = true;
            break;
        }
        default:
            break;
    }

    if (computed) {
        renderable->SetMaterialSortKey(key);
    }
}

MaterialSortKey BuildFallbackMaterialKey(const Renderable* renderable, uint32_t salt);

BatchableItem CreateBatchableItem(Renderable* renderable) {
    BatchableItem item{};
    item.renderable = renderable;

    if (!renderable) {
        return item;
    }

    item.key.layerID = renderable->GetLayerID();
    item.key.renderableType = renderable->GetType();
    if (renderable->HasMaterialSortKey() && !renderable->IsMaterialSortKeyDirty()) {
        item.key.materialKey = renderable->GetMaterialSortKey();
    } else {
        item.key.materialKey = BuildFallbackMaterialKey(renderable, 0u);
    }

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
            item.key.textureHandle = 0;
            item.key.blendMode = material->GetBlendMode();
            item.key.cullFace = material->GetCullFace();
            item.key.depthTest = material->GetDepthTest();
            item.key.depthWrite = material->GetDepthWrite();
            item.key.castShadows = item.meshData.castShadows;
            item.key.receiveShadows = item.meshData.receiveShadows;
            item.key.viewHash = 0;
            item.key.projectionHash = 0;
            item.key.screenSpace = false;

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
        case RenderableType::Model: {
            item.key.renderableType = RenderableType::Model;
            item.type = BatchItemType::Unsupported;
            item.batchable = false;
            item.isTransparent = renderable->GetTransparentHint();
            return item;
        }
        case RenderableType::Sprite: {
            item.type = BatchItemType::Sprite;
            auto* spriteRenderable = static_cast<SpriteBatchRenderable*>(renderable);
            auto* batcher = spriteRenderable->GetBatcher();
            if (!batcher) {
                item.batchable = false;
                return item;
            }

            SpriteBatcher::SpriteBatchInfo info{};
            if (!batcher->GetBatchInfo(spriteRenderable->GetBatchIndex(), info)) {
                item.batchable = false;
                return item;
            }

            Ref<Mesh> quadMesh;
            Ref<Shader> spriteShader;
            if (!SpriteRenderable::AcquireSharedResources(quadMesh, spriteShader) || !spriteShader || !quadMesh) {
                item.batchable = false;
                return item;
            }

            item.key.shaderHandle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(spriteShader.get()));
            item.key.meshHandle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(quadMesh.get()));
            item.key.textureHandle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(info.texture.get()));
            item.key.renderableType = RenderableType::Sprite;
            item.key.layerID = info.layer;
            item.key.blendMode = info.blendMode;
            item.key.cullFace = CullFace::None;
            item.key.depthTest = false;
            item.key.depthWrite = false;
            item.key.castShadows = false;
            item.key.receiveShadows = false;
            item.key.viewHash = info.viewHash;
            item.key.projectionHash = info.projectionHash;
            item.key.screenSpace = info.screenSpace;
            item.spriteData.batcher = batcher;
            item.spriteData.batchIndex = spriteRenderable->GetBatchIndex();
            item.spriteData.instanceCount = info.instanceCount;
            item.spriteData.blendMode = info.blendMode;
            item.spriteData.screenSpace = info.screenSpace;
            item.spriteData.texture = info.texture;

            item.batchable = info.instanceCount > 0;
            item.isTransparent = (info.blendMode == BlendMode::Alpha || info.blendMode == BlendMode::Additive);
            item.instanceEligible = item.batchable;
            return item;
        }
        case RenderableType::Text: {
            item.type = BatchItemType::Text;
            auto* textRenderable = static_cast<TextRenderable*>(renderable);

            TextRenderBatchData batchData;
            if (!textRenderable->GatherBatchData(batchData)) {
                item.batchable = false;
                return item;
            }

            item.textData = batchData;

            if (textRenderable->HasMaterialSortKey() && !textRenderable->IsMaterialSortKeyDirty()) {
                item.key.materialKey = textRenderable->GetMaterialSortKey();
            }

            item.key.renderableType = RenderableType::Text;
            item.key.layerID = renderable->GetLayerID();
            item.key.blendMode = BlendMode::Alpha;
            item.key.cullFace = CullFace::None;
            item.key.depthTest = false;
            item.key.depthWrite = false;
            item.key.castShadows = false;
            item.key.receiveShadows = false;
            item.key.shaderHandle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(batchData.shader.get()));
            item.key.meshHandle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(batchData.mesh.get()));
            item.key.textureHandle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(batchData.texture.get()));
            item.key.viewHash = batchData.viewHash;
            item.key.projectionHash = batchData.projectionHash;
            item.key.screenSpace = batchData.screenSpace;
            item.key.materialHandle = item.key.textureHandle;

            item.batchable = (batchData.texture != nullptr);
            item.isTransparent = true;
            item.instanceEligible = false;
            return item;
        }
        default:
            item.type = BatchItemType::Unsupported;
            item.batchable = false;
            item.isTransparent = renderable->GetTransparentHint();
            return item;
    }
}

struct MaterialSwitchMetrics {
    uint32_t switches = 0;
    uint32_t keyReady = 0;
    uint32_t keyMissing = 0;
};

MaterialSortKey BuildFallbackMaterialKey(const Renderable* renderable, uint32_t salt) {
    MaterialSortKey key{};
    const uintptr_t ptrValue = reinterpret_cast<uintptr_t>(renderable);
    key.materialID = static_cast<uint32_t>(ptrValue & 0xFFFFFFFFu);
    key.shaderID = static_cast<uint32_t>((ptrValue >> 32) & 0xFFFFFFFFu);
    key.overrideHash = key.materialID ^ key.shaderID ^ salt;
    key.pipelineFlags = salt;
    return key;
}

MaterialSwitchMetrics ComputeMaterialSwitchMetrics(const std::vector<Renderable*>& queue) {
    MaterialSwitchMetrics metrics{};
    MaterialSortKey previousKey{};
    bool hasPrevious = false;
    uint32_t fallbackSalt = 1;

    for (const auto* renderable : queue) {
        if (!renderable || !renderable->IsVisible()) {
            continue;
        }

        MaterialSortKey key{};
        bool hasKey = renderable->HasMaterialSortKey() && !renderable->IsMaterialSortKeyDirty();
        if (hasKey) {
            key = renderable->GetMaterialSortKey();
            metrics.keyReady++;
        } else {
            metrics.keyMissing++;
            key = BuildFallbackMaterialKey(renderable, fallbackSalt++);
        }

        if (hasPrevious && !(key == previousKey)) {
            metrics.switches++;
        }

        previousKey = key;
        hasPrevious = true;
    }

    return metrics;
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
    MaterialStateCache::Get().Reset();
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
    
    EnsureMaterialSortKey(renderable);

    std::lock_guard<std::mutex> lock(m_mutex);
    m_renderQueue.push_back(renderable);
}

void Renderer::FlushRenderQueue() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_renderQueue.empty()) {
        return;
    }

    const auto originalSwitchMetrics = ComputeMaterialSwitchMetrics(m_renderQueue);
    
    // 排序渲染队列
    SortRenderQueue();

    const auto sortedSwitchMetrics = ComputeMaterialSwitchMetrics(m_renderQueue);

    m_stats.materialSwitchesOriginal = originalSwitchMetrics.switches;
    m_stats.materialSwitchesSorted = sortedSwitchMetrics.switches;
    m_stats.materialSortKeyReady = std::max(originalSwitchMetrics.keyReady, sortedSwitchMetrics.keyReady);
    m_stats.materialSortKeyMissing = std::max(originalSwitchMetrics.keyMissing, sortedSwitchMetrics.keyMissing);
    
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
                "[Renderer] Batch flush: batches=%u, batchedDraw=%u, instancedDraw=%u, instances=%u, fallbackDraw=%u, fallbackBatches=%u, triangles=%u, vertices=%u, workerProcessed=%u, workerMaxQueue=%u, workerWaitMs=%.3f, matSwitchBefore=%u, matSwitchAfter=%u, matKeysReady=%u, matKeysMissing=%u",
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
                flushResult.workerWaitTimeMs,
                m_stats.materialSwitchesOriginal,
                m_stats.materialSwitchesSorted,
                m_stats.materialSortKeyReady,
                m_stats.materialSortKeyMissing
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
    auto isOpaque = [](Renderable* renderable) {
        return renderable && !renderable->GetTransparentHint();
    };

    auto partitionIt = std::stable_partition(m_renderQueue.begin(), m_renderQueue.end(), isOpaque);

    auto resolveKey = [](const Renderable* renderable) {
        if (!renderable) {
            return MaterialSortKey{};
        }
        if (renderable->HasMaterialSortKey() && !renderable->IsMaterialSortKeyDirty()) {
            return renderable->GetMaterialSortKey();
        }
        return BuildFallbackMaterialKey(renderable, 0u);
    };

    std::stable_sort(m_renderQueue.begin(), partitionIt,
        [&](Renderable* a, Renderable* b) {
            if (a == b) {
                return false;
            }
            if (!a) {
                return false;
            }
            if (!b) {
                return true;
            }

            const uint32_t layerA = a->GetLayerID();
            const uint32_t layerB = b->GetLayerID();
            if (layerA != layerB) {
                return layerA < layerB;
            }

            const MaterialSortKey keyA = resolveKey(a);
            const MaterialSortKey keyB = resolveKey(b);
            if (keyA != keyB) {
                return MaterialSortKeyLess{}(keyA, keyB);
            }

            const int32_t priorityA = a->GetRenderPriority();
            const int32_t priorityB = b->GetRenderPriority();
            if (priorityA != priorityB) {
                return priorityA < priorityB;
            }

            return static_cast<int>(a->GetType()) < static_cast<int>(b->GetType());
        });

    if (partitionIt == m_renderQueue.end()) {
        return;
    }

    struct TransparentEntry {
        Renderable* renderable = nullptr;
        MaterialSortKey materialKey{};
        float depth = 0.0f;
        size_t originalIndex = 0;
        uint32_t layer = 0;
        int32_t priority = 0;
    };

    auto computeDepthHint = [](Renderable* renderable) -> float {
        if (!renderable) {
            return 0.0f;
        }
        if (renderable->HasDepthHint()) {
            return renderable->GetDepthHint();
        }
        Matrix4 world = renderable->GetWorldMatrix();
        Vector3 position = world.block<3,1>(0, 3);
        return position.squaredNorm();
    };

    std::vector<TransparentEntry> transparentEntries;
    transparentEntries.reserve(static_cast<size_t>(std::distance(partitionIt, m_renderQueue.end())));

    size_t originalIndex = 0;
    for (auto it = partitionIt; it != m_renderQueue.end(); ++it, ++originalIndex) {
        Renderable* renderable = *it;
        TransparentEntry entry{};
        entry.renderable = renderable;
        entry.originalIndex = originalIndex;
        entry.layer = renderable ? renderable->GetLayerID() : 0u;
        entry.priority = renderable ? renderable->GetRenderPriority() : 0;
        entry.materialKey = resolveKey(renderable);
        entry.depth = renderable ? computeDepthHint(renderable) : 0.0f;
        transparentEntries.push_back(entry);
    }

    if (transparentEntries.empty()) {
        return;
    }

    auto nearlyEqual = [](float a, float b) {
        return std::fabs(a - b) <= 1e-6f * std::max(1.0f, std::max(std::fabs(a), std::fabs(b)));
    };

    std::stable_sort(transparentEntries.begin(), transparentEntries.end(),
        [&](const TransparentEntry& a, const TransparentEntry& b) {
            if (a.renderable == b.renderable) {
                return false;
            }

            if (a.layer != b.layer) {
                return a.layer < b.layer;
            }

            if (!nearlyEqual(a.depth, b.depth)) {
                return a.depth > b.depth;
            }

            if (a.materialKey != b.materialKey) {
                return MaterialSortKeyLess{}(a.materialKey, b.materialKey);
            }

            if (a.priority != b.priority) {
                return a.priority < b.priority;
            }

            return a.originalIndex < b.originalIndex;
        });

    auto assignIt = partitionIt;
    for (const auto& entry : transparentEntries) {
        *assignIt = entry.renderable;
        ++assignIt;
    }
}

} // namespace Render

