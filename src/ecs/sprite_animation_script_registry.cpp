#include "render/ecs/sprite_animation_script_registry.h"
#include "render/logger.h"
#include <unordered_map>
#include <mutex>

namespace Render {
namespace ECS {

namespace {
std::unordered_map<std::string, SpriteAnimationScriptRegistry::ScriptFunc> g_scripts;
std::mutex g_mutex;
}

bool SpriteAnimationScriptRegistry::Register(const std::string& name, ScriptFunc callback) {
    if (name.empty()) {
        Logger::GetInstance().Warning("[SpriteAnimationScriptRegistry] Attempted to register script with empty name");
        return false;
    }
    if (!callback) {
        Logger::GetInstance().WarningFormat("[SpriteAnimationScriptRegistry] Script '%s' callback is null", name.c_str());
        return false;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    g_scripts[name] = std::move(callback);
    return true;
}

void SpriteAnimationScriptRegistry::Unregister(const std::string& name) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_scripts.erase(name);
}

bool SpriteAnimationScriptRegistry::Invoke(const std::string& name,
                                           EntityID entity,
                                           const SpriteAnimationEvent& eventData,
                                           SpriteAnimationComponent& component) {
    if (name.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_scripts.find(name);
    if (it == g_scripts.end()) {
        Logger::GetInstance().WarningFormat("[SpriteAnimationScriptRegistry] Script '%s' not found", name.c_str());
        return false;
    }

    it->second(entity, eventData, component);
    return true;
}

} // namespace ECS
} // namespace Render


