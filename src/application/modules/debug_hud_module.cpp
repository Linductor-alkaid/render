#include "render/application/modules/debug_hud_module.h"

#include "render/application/app_context.h"
#include "render/application/event_bus.h"
#include "render/logger.h"
#include "render/renderer.h"
#include "render/render_state.h"
#include "render/renderable.h"
#include "render/text/text.h"
#include "render/text/font.h"
#include "render/resource_manager.h"
#include "render/transform.h"
#include "render/types.h"
#include "render/math_utils.h"
#include "render/material_sort_key.h"
#include "render/render_layer.h"
#include <sstream>
#include <iomanip>

namespace Render::Application {

int DebugHUDModule::Priority(ModulePhase phase) const {
    switch (phase) {
        case ModulePhase::PostFrame:
            return -50;
        default:
            return 0;
    }
}

void DebugHUDModule::OnRegister(ECS::World&, AppContext& ctx) {
    m_registered = true;
    m_accumulatedTime = 0.0f;
    m_frameCounter = 0;
    m_smoothedFPS = 0.0f;
    m_textObjectsCreated = false;
    
    // 确保使用默认UI层，并配置禁用深度测试和深度写入
    // 注意：不要修改默认层的maskIndex，这会导致相机layerMask过滤问题
    if (ctx.renderer) {
        auto& layerRegistry = ctx.renderer->GetLayerRegistry();
        RenderLayerId uiLayerId = Layers::UI::Default;  // 使用默认UI层ID
        
        // 只更新渲染状态覆写，不修改层的其他属性（特别是maskIndex）
        RenderStateOverrides uiOverrides;
        uiOverrides.depthTest = false;   // UI层禁用深度测试
        uiOverrides.depthWrite = false;  // UI层禁用深度写入
        uiOverrides.blendMode = BlendMode::Alpha;  // UI层使用Alpha混合
        layerRegistry.SetOverrides(uiLayerId, uiOverrides);
        Logger::GetInstance().Info("[DebugHUDModule] Configured default UI layer (800) to disable depth test");
    }
    
    // 尝试加载字体
    if (ctx.resourceManager) {
        // 尝试从资源管理器获取默认字体
        m_font = ctx.resourceManager->GetFont("default");
        if (!m_font) {
            // 如果没有默认字体，尝试加载系统字体
            m_font = std::make_shared<Font>();
            // 尝试加载项目中的字体文件（按优先级顺序）
            const std::vector<std::string> fontPaths = {
                "assets/fonts/NotoSansSC-Regular.ttf",  // 项目字体
                "assets/fonts/default.ttf",
                "assets/fonts/arial.ttf",
                "fonts/default.ttf",
                "fonts/NotoSansSC-Regular.ttf"
            };
            bool fontLoaded = false;
            for (const auto& path : fontPaths) {
                if (m_font->LoadFromFile(path, 16.0f)) {
                    fontLoaded = true;
                    ctx.resourceManager->RegisterFont("debug_hud_font", m_font);
                    Logger::GetInstance().InfoFormat("[DebugHUDModule] Loaded font from: %s", path.c_str());
                    break;
                }
            }
            if (!fontLoaded) {
                Logger::GetInstance().Warning("[DebugHUDModule] Failed to load font, HUD will use text rendering fallback");
                m_font = nullptr;
            }
        }
    }
    
    Logger::GetInstance().Info("[DebugHUDModule] Registered");
}

void DebugHUDModule::OnUnregister(ECS::World&, AppContext&) {
    if (!m_registered) {
        return;
    }
    m_registered = false;
    DestroyTextObjects();
    m_font = nullptr;
    Logger::GetInstance().Info("[DebugHUDModule] Unregistered");
}

void DebugHUDModule::OnPostFrame(const FrameUpdateArgs& frame, AppContext& ctx) {
    if (!m_registered || !ctx.renderer) {
        return;
    }

    m_accumulatedTime += frame.deltaTime;
    ++m_frameCounter;
    if (m_accumulatedTime >= 0.5f) {
        const float fps = static_cast<float>(m_frameCounter) / m_accumulatedTime;
        const float smoothing = 0.1f;
        m_smoothedFPS = (1.0f - smoothing) * m_smoothedFPS + smoothing * fps;
        m_accumulatedTime = 0.0f;
        m_frameCounter = 0;
    }

    // 创建文本对象（如果还没有创建）
    if (!m_textObjectsCreated && m_font) {
        CreateTextObjects(ctx);
    }

    DrawHUD(frame, ctx);
}

void DebugHUDModule::CreateTextObjects(AppContext& ctx) {
    if (!m_font || !ctx.renderer) {
        return;
    }

    // 创建多个文本对象用于显示不同的统计信息
    const int lineCount = 15; // 显示15行信息
    m_textObjects.clear();
    m_textObjects.reserve(lineCount);
    m_textRenderables.clear();
    m_textRenderables.reserve(lineCount);

    // 预先计算屏幕尺寸和位置参数
    const float screenWidth = static_cast<float>(ctx.renderer->GetWidth());
    const float screenHeight = static_cast<float>(ctx.renderer->GetHeight());
    const float lineHeight = 24.0f;  // 行高

    for (size_t i = 0; i < lineCount; ++i) {
        // 创建Text对象
        auto text = std::make_shared<Text>(m_font);
        if (text) {
            text->SetColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
            text->SetAlignment(TextAlignment::Left);  // 左对齐
            text->SetString(""); // 初始为空，后续更新
        }
        m_textObjects.push_back(text);

        // 创建TextRenderable
        auto renderable = std::make_unique<TextRenderable>();
        renderable->SetText(text);
        renderable->SetLayerID(Layers::UI::Default.value); // 使用默认UI层
        renderable->SetRenderPriority(static_cast<int32_t>(i));

        // 计算左下角位置（从左下角向上排列，留出边距避免文本超出窗口）
        // 注意：TextRenderable的基准点是文本中心，所以需要调整位置
        // 对于左对齐文本：如果文本中心在X位置，文本左边缘在 X - textWidth/2
        // 如果我们想让文本左边缘在leftMargin，那么中心应该在 leftMargin + textWidth/2
        // 但由于文本在创建时为空，我们使用一个合理的估算值，并在UpdateTextContent时重新计算
        const float leftMargin = 20.0f;   // 左边距
        const float bottomMargin = 20.0f; // 下边距
        
        // 从下往上排列：最下面一行是i=lineCount-1，最上面一行是i=0
        // 计算每行文本的左上角Y位置
        const float anchorY = screenHeight - bottomMargin - static_cast<float>(lineCount - 1 - i) * lineHeight;
        
        // 使用一个合理的估算文本宽度（后续会在UpdateTextContent时根据实际文本尺寸调整）
        const float estimatedTextWidth = 400.0f; // 估算文本宽度
        const float centerX = leftMargin + estimatedTextWidth * 0.5f; // 中心X = 左边距 + 文本宽度的一半
        const float centerY = anchorY + lineHeight * 0.5f; // 中心Y = 左上角Y + 行高的一半
        
        auto transform = std::make_shared<Transform>();
        transform->SetPosition(Vector3(centerX, centerY, 0.0f));
        renderable->SetTransform(transform);

        // 设置屏幕空间视图/投影矩阵（原点在左上角，Y向下）
        Matrix4 view = Matrix4::Identity();
        Matrix4 projection = MathUtils::Orthographic(0.0f, screenWidth,
                                                      screenHeight, 0.0f, -1.0f, 1.0f);
        renderable->SetViewProjectionOverride(view, projection);

        // 为UI文本设置材质排序键，禁用深度测试和深度写入，避免遮挡3D场景
        MaterialSortKey uiKey;
        uiKey.depthTest = false;   // UI层禁用深度测试
        uiKey.depthWrite = false; // UI层禁用深度写入
        uiKey.pipelineFlags = MaterialPipelineFlags_ScreenSpace; // 标记为屏幕空间
        renderable->SetMaterialSortKey(uiKey);

        m_textRenderables.push_back(std::move(renderable));
    }

    m_textObjectsCreated = true;
    Logger::GetInstance().Info("[DebugHUDModule] Text objects created");
}

void DebugHUDModule::DestroyTextObjects() {
    m_textRenderables.clear();
    m_textObjects.clear();
    m_textObjectsCreated = false;
}

void DebugHUDModule::UpdateTextContent(const FrameUpdateArgs&, AppContext& ctx) {
    if (!m_textObjectsCreated || m_textObjects.empty()) {
        return;
    }

    // 获取渲染统计信息
    // 注意：PostFrame在UpdateWorld和FlushRenderQueue之前调用，所以这里读取的是上一帧的统计信息
    // 这是合理的，因为HUD显示的是上一帧的渲染结果
    auto renderStats = ctx.renderer->GetStats();
    
    // 获取资源统计信息
    ResourceStats resourceStats{};
    if (ctx.resourceManager) {
        resourceStats = ctx.resourceManager->GetStats();
    }

    // 更新统计缓存
    m_statsCache.fps = m_smoothedFPS;
    m_statsCache.frameTime = renderStats.frameTime;
    m_statsCache.drawCalls = renderStats.drawCalls;
    m_statsCache.triangles = renderStats.triangles;
    m_statsCache.vertices = renderStats.vertices;
    m_statsCache.batchCount = renderStats.batchCount;
    m_statsCache.originalDrawCalls = renderStats.originalDrawCalls;
    m_statsCache.batchedDrawCalls = renderStats.batchedDrawCalls;
    m_statsCache.instancedDrawCalls = renderStats.instancedDrawCalls;
    m_statsCache.instancedInstances = renderStats.instancedInstances;
    m_statsCache.textureCount = resourceStats.textureCount;
    m_statsCache.meshCount = resourceStats.meshCount;
    m_statsCache.materialCount = resourceStats.materialCount;
    m_statsCache.shaderCount = resourceStats.shaderCount;
    m_statsCache.textureMemory = resourceStats.textureMemory;
    m_statsCache.meshMemory = resourceStats.meshMemory;
    m_statsCache.totalMemory = resourceStats.totalMemory;

    // 格式化并更新文本内容
    std::vector<std::string> lines;
    
    // 性能统计
    lines.push_back("=== Performance ===");
    lines.push_back(std::string("FPS: ") + std::to_string(static_cast<int>(m_statsCache.fps)));
    lines.push_back(std::string("Frame Time: ") + std::to_string(m_statsCache.frameTime) + " ms");
    
    // 渲染统计
    lines.push_back("=== Rendering ===");
    lines.push_back(std::string("Draw Calls: ") + std::to_string(m_statsCache.drawCalls));
    lines.push_back(std::string("  Original: ") + std::to_string(m_statsCache.originalDrawCalls));
    lines.push_back(std::string("  Batched: ") + std::to_string(m_statsCache.batchedDrawCalls));
    lines.push_back(std::string("  Instanced: ") + std::to_string(m_statsCache.instancedDrawCalls) + 
                    " (" + std::to_string(m_statsCache.instancedInstances) + " instances)");
    lines.push_back(std::string("Batches: ") + std::to_string(m_statsCache.batchCount));
    lines.push_back(std::string("Triangles: ") + std::to_string(m_statsCache.triangles));
    lines.push_back(std::string("Vertices: ") + std::to_string(m_statsCache.vertices));
    
    // 资源统计
    lines.push_back("=== Resources ===");
    lines.push_back(std::string("Textures: ") + std::to_string(m_statsCache.textureCount));
    lines.push_back(std::string("Meshes: ") + std::to_string(m_statsCache.meshCount));
    lines.push_back(std::string("Materials: ") + std::to_string(m_statsCache.materialCount));
    lines.push_back(std::string("Shaders: ") + std::to_string(m_statsCache.shaderCount));
    
    // 内存统计
    std::stringstream memStream;
    memStream << std::fixed << std::setprecision(2);
    float totalMemMB = static_cast<float>(m_statsCache.totalMemory) / (1024.0f * 1024.0f);
    float textureMemMB = static_cast<float>(m_statsCache.textureMemory) / (1024.0f * 1024.0f);
    float meshMemMB = static_cast<float>(m_statsCache.meshMemory) / (1024.0f * 1024.0f);
    lines.push_back(std::string("Memory: ") + std::to_string(totalMemMB) + " MB");
    lines.push_back(std::string("  Textures: ") + std::to_string(textureMemMB) + " MB");
    lines.push_back(std::string("  Meshes: ") + std::to_string(meshMemMB) + " MB");

    // 更新文本对象并重新计算位置（确保左对齐和文本不超出窗口）
    const float screenWidth = static_cast<float>(ctx.renderer->GetWidth());
    const float screenHeight = static_cast<float>(ctx.renderer->GetHeight());
    const float leftMargin = 20.0f;  // 左边距
    const float lineHeight = 24.0f;
    const int lineCount = static_cast<int>(m_textObjects.size());
    
    // 计算所有文本的矩形框（用于边界检查）
    float maxTextWidth = 0.0f;
    std::vector<Vector2> textSizes;
    textSizes.reserve(lines.size());
    
    for (size_t i = 0; i < m_textObjects.size() && i < lines.size(); ++i) {
        if (m_textObjects[i]) {
            m_textObjects[i]->SetString(lines[i]);
            m_textObjects[i]->MarkDirty();
            
            // 获取文本实际尺寸
            m_textObjects[i]->EnsureUpdated();
            Vector2 textSize = m_textObjects[i]->GetSize();
            textSizes.push_back(textSize);
            maxTextWidth = std::max(maxTextWidth, textSize.x());
        } else {
            textSizes.push_back(Vector2(0.0f, lineHeight));
        }
    }
    
    // 重新计算并更新每个文本的位置（确保左对齐和文本不超出窗口）
    // 目标：让文本底部边缘距离窗口底部的距离等于左边距
    for (size_t i = 0; i < m_textRenderables.size() && i < textSizes.size(); ++i) {
        auto& renderable = m_textRenderables[i];
        if (!renderable) {
            continue;
        }
        
        Vector2 textSize = textSizes[i];
        if (textSize.y() <= 0.0f) {
            textSize.y() = lineHeight; // 使用行高作为默认高度
        }
        
        // 计算文本左上角位置（左对齐）
        // 从下往上排列：最下面一行是i=lineCount-1，最上面一行是i=0
        // 最下面一行文本的底部边缘距离窗口底部的距离应该等于leftMargin
        // 文本底部Y = centerY + textSize.y() * 0.5f
        // 窗口底部距离 = screenHeight - (centerY + textSize.y() * 0.5f) = leftMargin
        // 所以：centerY = screenHeight - leftMargin - textSize.y() * 0.5f
        // 对于其他行，需要加上行高偏移
        const float anchorX = leftMargin;
        const int lineIndex = static_cast<int>(lineCount - 1 - static_cast<int>(i)); // 从下往上，0是最下面一行
        const float bottomEdgeY = screenHeight - leftMargin; // 最下面一行文本的底部边缘Y位置
        const float anchorY = bottomEdgeY - textSize.y() - static_cast<float>(lineIndex) * lineHeight;
        
        // TextRenderable的基准点是中心，所以需要加上文本尺寸的一半
        const float centerX = anchorX + textSize.x() * 0.5f; // 中心X = 左边距 + 文本宽度的一半
        const float centerY = anchorY + textSize.y() * 0.5f; // 中心Y = 左上角Y + 文本高度的一半
        
        // 确保文本不超出窗口边界
        const float minX = textSize.x() * 0.5f;
        const float minY = textSize.y() * 0.5f;
        const float maxX = screenWidth - textSize.x() * 0.5f;
        const float maxY = screenHeight - textSize.y() * 0.5f;
        
        const float clampedX = std::max(minX, std::min(centerX, maxX));
        const float clampedY = std::max(minY, std::min(centerY, maxY));
        
        // 更新transform位置
        auto transform = renderable->GetTransform();
        if (transform) {
            transform->SetPosition(Vector3(clampedX, clampedY, 0.0f));
        }
    }
}

void DebugHUDModule::DrawHUD(const FrameUpdateArgs& frame, AppContext& ctx) {
    if (!m_textObjectsCreated || !ctx.renderer) {
        // 如果没有文本对象，回退到Logger输出
        auto stats = ctx.renderer->GetStats();
        Logger::GetInstance().DebugFormat("[DebugHUD] FPS: %.2f frameTime: %.2fms drawCalls: %u batches: %u",
                                          m_smoothedFPS,
                                          stats.frameTime,
                                          stats.drawCalls,
                                          stats.batchCount);
        return;
    }

    // 更新文本内容
    UpdateTextContent(frame, ctx);

    // 提交所有文本渲染对象
    for (auto& renderable : m_textRenderables) {
        if (renderable && renderable->IsVisible()) {
            renderable->SubmitToRenderer(ctx.renderer);
        }
    }
}

} // namespace Render::Application


