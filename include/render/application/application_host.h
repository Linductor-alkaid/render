#pragma once

#include <memory>
#include <string_view>

#include "render/application/app_context.h"
#include "render/application/event_bus.h"
#include "render/application/module_registry.h"
#include "render/application/scene_manager.h"

namespace Render {

namespace ECS {
class World;
}

class Renderer;
class ResourceManager;
class UniformManager;
class AsyncResourceLoader;

namespace Application {

class ApplicationHost {
public:
    struct Config {
        Renderer* renderer = nullptr;
        UniformManager* uniformManager = nullptr;
        ResourceManager* resourceManager = nullptr;
        AsyncResourceLoader* asyncLoader = nullptr;
        std::shared_ptr<ECS::World> world;
        bool createWorldIfMissing = true;
    };

    ApplicationHost() = default;
    ~ApplicationHost();

    ApplicationHost(const ApplicationHost&) = delete;
    ApplicationHost& operator=(const ApplicationHost&) = delete;

    bool Initialize(const Config& config);
    void Shutdown();

    void RegisterSceneFactory(std::string sceneId, SceneFactory factory);
    bool PushScene(std::string_view sceneId, SceneEnterArgs args = {});
    bool ReplaceScene(std::string_view sceneId, SceneEnterArgs args = {});

    void UpdateFrame(const FrameUpdateArgs& args);
    void UpdateWorld(float deltaTime);

    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    AppContext& GetContext() noexcept { return m_context; }
    const AppContext& GetContext() const noexcept { return m_context; }

    ModuleRegistry& GetModuleRegistry() noexcept { return m_moduleRegistry; }
    SceneManager& GetSceneManager() noexcept { return m_sceneManager; }
    EventBus& GetEventBus() noexcept { return m_eventBus; }
    ECS::World& GetWorld() noexcept { return *m_world; }

private:
    bool CreateWorldIfNeeded(bool allowCreate);
    void ResetContext();

    bool m_initialized = false;
    bool m_ownsWorld = false;

    std::shared_ptr<ECS::World> m_world;
    AppContext m_context{};
    EventBus m_eventBus{};
    ModuleRegistry m_moduleRegistry{};
    SceneManager m_sceneManager{};
};

} // namespace Application
} // namespace Render


