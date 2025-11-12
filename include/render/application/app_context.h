#pragma once

#include <cstdint>
#include <string>

namespace Render {

namespace ECS {
class World;
}

class Renderer;
class UniformManager;
class ResourceManager;
class AsyncResourceLoader;

namespace UI {
class UIInputRouter;
}

namespace Application {

class EventBus;

struct FrameUpdateArgs {
    float deltaTime = 0.0f;
    double absoluteTime = 0.0;
    uint64_t frameIndex = 0;
};

struct AppContext {
    Renderer* renderer = nullptr;
    UniformManager* uniformManager = nullptr;
    ResourceManager* resourceManager = nullptr;
    AsyncResourceLoader* asyncLoader = nullptr;
    EventBus* globalEventBus = nullptr;
    ECS::World* world = nullptr;
    UI::UIInputRouter* uiInputRouter = nullptr;

    FrameUpdateArgs lastFrame{};

    [[nodiscard]] bool IsValid() const noexcept;
    void ValidateOrThrow(const std::string& source) const;
};

} // namespace Application
} // namespace Render

