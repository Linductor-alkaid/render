#pragma once

#include "render/logger.h"
#include "render/ecs/entity.h"
#include "render/ecs/components.h"
#include <functional>
#include <string>

namespace Render {
namespace ECS {

class SpriteAnimationScriptRegistry {
public:
    using ScriptFunc = std::function<void(EntityID, const SpriteAnimationEvent&, SpriteAnimationComponent&)>;

    static bool Register(const std::string& name, ScriptFunc callback);
    static void Unregister(const std::string& name);
    static bool Invoke(const std::string& name,
                       EntityID entity,
                       const SpriteAnimationEvent& eventData,
                       SpriteAnimationComponent& component);
};

} // namespace ECS
} // namespace Render


