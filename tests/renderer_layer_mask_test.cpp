#include <render/renderer.h>
#include <render/renderable.h>
#include <render/logger.h>
#include <render/render_layer.h>

#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Render;

class TestRenderable : public Renderable {
public:
    TestRenderable()
        : Renderable(RenderableType::Custom) {}

    void Render(RenderState* /*renderState*/) override {}

    void SubmitToRenderer(Renderer* renderer) override {
        if (renderer) {
            renderer->SubmitRenderable(this);
        }
    }

    [[nodiscard]] AABB GetBoundingBox() const override {
        return AABB(Vector3::Zero(), Vector3::Zero());
    }
};

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    Logger::GetInstance().SetLogToConsole(false);
    Logger::GetInstance().SetLogToFile(false);

    Renderer renderer;

    auto& layerRegistry = renderer.GetLayerRegistry();
    auto worldDescOpt = layerRegistry.GetDescriptor(Layers::World::Midground);
    auto uiDescOpt = layerRegistry.GetDescriptor(Layers::UI::Default);

    if (!worldDescOpt.has_value() || !uiDescOpt.has_value()) {
        std::cerr << "[renderer_layer_mask_test] Missing default layer descriptors" << std::endl;
        return 1;
    }

    const uint32_t worldMask = 1u << worldDescOpt->maskIndex;
    const uint32_t uiMask = 1u << uiDescOpt->maskIndex;

    renderer.SetActiveLayerMask(0xFFFFFFFFu);

    TestRenderable worldRenderable;
    worldRenderable.SetLayerID(Layers::World::Midground.value);

    TestRenderable uiRenderable;
    uiRenderable.SetLayerID(Layers::UI::Default.value);

    worldRenderable.SubmitToRenderer(&renderer);
    uiRenderable.SubmitToRenderer(&renderer);

    auto requireSize = [&](uint32_t mask, size_t expected, const char* description) -> bool {
        renderer.SetActiveLayerMask(mask);
        size_t actual = renderer.GetRenderQueueSize();
        if (actual != expected) {
            std::cerr << "[renderer_layer_mask_test] " << description
                      << " expected=" << expected << " actual=" << actual << std::endl;
            return false;
        }
        return true;
    };

    if (!requireSize(0xFFFFFFFFu, 2, "All layers visible")) {
        return 1;
    }
    if (!requireSize(worldMask, 1, "World layer only")) {
        return 1;
    }
    if (!requireSize(uiMask, 1, "UI layer only")) {
        return 1;
    }
    if (!requireSize(worldMask | uiMask, 2, "World + UI layers")) {
        return 1;
    }
    if (!requireSize(0u, 0, "No layers visible")) {
        return 1;
    }

    renderer.ClearRenderQueue();
    std::cout << "[renderer_layer_mask_test] Passed layer mask filtering checks." << std::endl;
    return 0;
}


