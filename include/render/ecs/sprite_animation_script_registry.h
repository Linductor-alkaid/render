/*
 * Copyright (c) 2025 Li Chaoyu
 * 
 * This file is part of Render.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * For commercial licensing, please contact: 2052046346@qq.com
 */
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


