#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "render/application/app_module.h"

namespace Render {
class Renderer;
class RenderState;
class Text;
class TextRenderable;
class Font;
} // namespace Render

#include "render/types.h"
#include "render/text/text.h"
#include "render/text/font.h"

namespace Render::Application {

class DebugHUDModule final : public AppModule {
public:
    DebugHUDModule() = default;
    ~DebugHUDModule() override = default;

    std::string_view Name() const override { return "DebugHUDModule"; }
    ModuleDependencies Dependencies() const override { return {"CoreRenderModule"}; }
    int Priority(ModulePhase phase) const override;

    void OnRegister(ECS::World& world, AppContext& ctx) override;
    void OnUnregister(ECS::World& world, AppContext& ctx) override;

    void OnPostFrame(const FrameUpdateArgs& frame, AppContext& ctx) override;

private:
    void DrawHUD(const FrameUpdateArgs& frame, AppContext& ctx);
    void UpdateTextContent(const FrameUpdateArgs& frame, AppContext& ctx);
    void CreateTextObjects(AppContext& ctx);
    void DestroyTextObjects();

    bool m_registered = false;
    float m_accumulatedTime = 0.0f;
    uint32_t m_frameCounter = 0;
    float m_smoothedFPS = 0.0f;
    
    // 文本对象
    FontPtr m_font;
    std::vector<TextPtr> m_textObjects;
    std::vector<std::unique_ptr<TextRenderable>> m_textRenderables;
    
    // 统计信息缓存
    struct StatsCache {
        float fps = 0.0f;
        float frameTime = 0.0f;
        uint32_t drawCalls = 0;
        uint32_t triangles = 0;
        uint32_t vertices = 0;
        uint32_t batchCount = 0;
        uint32_t originalDrawCalls = 0;
        uint32_t batchedDrawCalls = 0;
        uint32_t instancedDrawCalls = 0;
        uint32_t instancedInstances = 0;
        size_t textureCount = 0;
        size_t meshCount = 0;
        size_t materialCount = 0;
        size_t shaderCount = 0;
        size_t textureMemory = 0;
        size_t meshMemory = 0;
        size_t totalMemory = 0;
    } m_statsCache;
    
    bool m_textObjectsCreated = false;
};

} // namespace Render::Application


