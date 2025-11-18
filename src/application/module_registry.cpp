#include "render/application/module_registry.h"

#include <algorithm>

#include "render/ecs/world.h"
#include "render/logger.h"

namespace Render::Application {

ModuleRegistry::ModuleRegistry() = default;

ModuleRegistry::~ModuleRegistry() {
    Shutdown();
}

void ModuleRegistry::Initialize(ECS::World* world, AppContext* ctx) {
    m_world = world;
    m_context = ctx;
}

void ModuleRegistry::Shutdown() {
    for (auto& [name, record] : m_modules) {
        if (record.registered && record.module && m_world && m_context) {
            record.module->OnUnregister(*m_world, *m_context);
        }
    }
    m_modules.clear();
    m_sortedPreFrame.clear();
    m_sortedPostFrame.clear();
    m_world = nullptr;
    m_context = nullptr;
}

bool ModuleRegistry::RegisterModule(std::unique_ptr<AppModule> module, bool activateImmediately) {
    if (!module) {
        return false;
    }

    std::string name(module->Name());
    if (m_modules.contains(name)) {
        Logger::GetInstance().Warning("ModuleRegistry::RegisterModule - module already exists: " + name);
        return false;
    }

    if (!CanRegister(*module)) {
        Logger::GetInstance().Warning("ModuleRegistry::RegisterModule - missing dependencies for module: " + name);
        return false;
    }

    ModuleRecord record;
    record.module = std::move(module);
    record.active = false;
    record.registered = false;

    auto [it, inserted] = m_modules.emplace(name, std::move(record));
    if (!inserted) {
        return false;
    }

    if (activateImmediately) {
        ActivateModule(name);
    }
    return true;
}

void ModuleRegistry::UnregisterModule(std::string_view name) {
    auto it = m_modules.find(std::string(name));
    if (it == m_modules.end()) {
        return;
    }

    auto& record = it->second;
    if (record.registered && record.module && m_world && m_context) {
        record.module->OnUnregister(*m_world, *m_context);
    }
    m_modules.erase(it);
    SortForPhase(ModulePhase::PreFrame);
    SortForPhase(ModulePhase::PostFrame);
}

bool ModuleRegistry::ActivateModule(std::string_view name) {
    auto it = m_modules.find(std::string(name));
    if (it == m_modules.end() || !m_world || !m_context) {
        return false;
    }

    auto& record = it->second;
    if (!record.registered) {
        record.module->OnRegister(*m_world, *m_context);
        record.registered = true;
    }
    record.active = true;
    SortForPhase(ModulePhase::PreFrame);
    SortForPhase(ModulePhase::PostFrame);
    return true;
}

void ModuleRegistry::DeactivateModule(std::string_view name) {
    auto it = m_modules.find(std::string(name));
    if (it == m_modules.end()) {
        return;
    }

    auto& record = it->second;
    if (record.active && record.registered && record.module && m_world && m_context) {
        record.module->OnUnregister(*m_world, *m_context);
        record.registered = false;
    }
    record.active = false;
    SortForPhase(ModulePhase::PreFrame);
    SortForPhase(ModulePhase::PostFrame);
}

void ModuleRegistry::ForEachModule(const std::function<void(const AppModule&)>& visitor) const {
    for (const auto& [name, record] : m_modules) {
        if (record.module) {
            visitor(*record.module);
        }
    }
}

AppModule* ModuleRegistry::GetModule(std::string_view name) {
    auto it = m_modules.find(std::string(name));
    if (it == m_modules.end()) {
        return nullptr;
    }
    return it->second.module.get();
}

const AppModule* ModuleRegistry::GetModule(std::string_view name) const {
    auto it = m_modules.find(std::string(name));
    if (it == m_modules.end()) {
        return nullptr;
    }
    return it->second.module.get();
}

void ModuleRegistry::InvokePhase(ModulePhase phase, const FrameUpdateArgs& frameArgs) {
    Logger::GetInstance().DebugFormat("[ModuleRegistry] InvokePhase %d begin", static_cast<int>(phase));
    const auto& sorted = (phase == ModulePhase::PreFrame) ? m_sortedPreFrame : m_sortedPostFrame;
    for (auto* module : sorted) {
        if (!module) {
            continue;
        }
        Logger::GetInstance().DebugFormat("[ModuleRegistry] Invoke module %s phase %d",
                                          std::string(module->Name()).c_str(),
                                          static_cast<int>(phase));
        if (phase == ModulePhase::PreFrame) {
            module->OnPreFrame(frameArgs, *m_context);
        } else if (phase == ModulePhase::PostFrame) {
            module->OnPostFrame(frameArgs, *m_context);
        }
    }
    Logger::GetInstance().DebugFormat("[ModuleRegistry] InvokePhase %d end", static_cast<int>(phase));
}

bool ModuleRegistry::CanRegister(const AppModule& module) const {
    if (!m_world || !m_context) {
        Logger::GetInstance().Error("ModuleRegistry::CanRegister - registry not initialized with world/context.");
        return false;
    }

    std::vector<std::string> missing;
    if (!ResolveDependencies(module, missing)) {
        std::string message = "ModuleRegistry::CanRegister - unresolved dependencies for module '" +
                              std::string(module.Name()) + "': ";
        for (size_t i = 0; i < missing.size(); ++i) {
            message += missing[i];
            if (i + 1 < missing.size()) {
                message += ", ";
            }
        }
        Logger::GetInstance().Warning(message);
        return false;
    }

    return true;
}

bool ModuleRegistry::ResolveDependencies(const AppModule& module, std::vector<std::string>& missing) const {
    std::unordered_set<std::string> visited;
    std::function<bool(const std::string&)> visit = [&](const std::string& name) -> bool {
        if (visited.contains(name)) {
            return true;
        }
        visited.insert(name);

        auto it = m_modules.find(name);
        if (it == m_modules.end()) {
            missing.push_back(name);
            return false;
        }
        if (!it->second.module) {
            missing.push_back(name);
            return false;
        }

        for (const auto& dep : it->second.module->Dependencies()) {
            if (!visit(dep)) {
                return false;
            }
        }
        return true;
    };

    bool allResolved = true;
    for (const auto& dependency : module.Dependencies()) {
        if (!visit(dependency)) {
            allResolved = false;
        }
    }
    return allResolved && missing.empty();
}

void ModuleRegistry::SortForPhase(ModulePhase phase) {
    std::vector<AppModule*> activeModules;
    activeModules.reserve(m_modules.size());

    for (auto& [name, record] : m_modules) {
        if (record.active && record.module) {
            activeModules.push_back(record.module.get());
        }
    }

    std::sort(activeModules.begin(), activeModules.end(), [phase](const AppModule* lhs, const AppModule* rhs) {
        return lhs->Priority(phase) > rhs->Priority(phase);
    });

    if (phase == ModulePhase::PreFrame) {
        m_sortedPreFrame = activeModules;
    } else {
        m_sortedPostFrame = activeModules;
    }
}

} // namespace Render::Application


