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

