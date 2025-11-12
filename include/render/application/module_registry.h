#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "render/application/app_context.h"
#include "render/application/app_module.h"

namespace Render {

namespace ECS {
class World;
}

namespace Application {

class ModuleRegistry {
public:
    ModuleRegistry();
    ~ModuleRegistry();

    ModuleRegistry(const ModuleRegistry&) = delete;
    ModuleRegistry& operator=(const ModuleRegistry&) = delete;

    void Initialize(ECS::World* world, AppContext* ctx);
    void Shutdown();

    bool RegisterModule(std::unique_ptr<AppModule> module, bool activateImmediately = true);
    void UnregisterModule(std::string_view name);

    bool ActivateModule(std::string_view name);
    void DeactivateModule(std::string_view name);

    void ForEachModule(const std::function<void(const AppModule&)>& visitor) const;
    AppModule* GetModule(std::string_view name);
    const AppModule* GetModule(std::string_view name) const;

    void InvokePhase(ModulePhase phase, const FrameUpdateArgs& frameArgs);

private:
    struct ModuleRecord {
        std::unique_ptr<AppModule> module;
        bool active = false;
        bool registered = false;
    };

    bool CanRegister(const AppModule& module) const;
    bool ResolveDependencies(const AppModule& module, std::vector<std::string>& missing) const;
    void SortForPhase(ModulePhase phase);

    ECS::World* m_world = nullptr;
    AppContext* m_context = nullptr;

    std::unordered_map<std::string, ModuleRecord> m_modules;
    std::vector<AppModule*> m_sortedPreFrame;
    std::vector<AppModule*> m_sortedPostFrame;
};

} // namespace Application
} // namespace Render


