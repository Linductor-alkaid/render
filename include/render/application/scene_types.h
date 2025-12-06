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
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Render {

namespace Application {

struct FrameUpdateArgs;

enum class SceneFlags : uint32_t {
    None = 0,
    UpdateWhenBackground = 1 << 0,
    RenderWhenBackground = 1 << 1,
};

constexpr SceneFlags operator|(SceneFlags lhs, SceneFlags rhs) noexcept {
    return static_cast<SceneFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr SceneFlags operator&(SceneFlags lhs, SceneFlags rhs) noexcept {
    return static_cast<SceneFlags>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

constexpr SceneFlags& operator|=(SceneFlags& lhs, SceneFlags rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

constexpr SceneFlags& operator&=(SceneFlags& lhs, SceneFlags rhs) noexcept {
    lhs = lhs & rhs;
    return lhs;
}

struct SceneSnapshot {
    std::string sceneId;
    std::unordered_map<std::string, std::string> state;
};

enum class ResourceScope {
    Scene,
    Shared
};

struct ResourceRequest {
    std::string identifier;
    std::string type;
    ResourceScope scope = ResourceScope::Scene;
    bool optional = false;
};

struct SceneResourceManifest {
    std::vector<ResourceRequest> required;
    std::vector<ResourceRequest> optional;

    void Merge(const SceneResourceManifest& other);
};

struct SceneEnterArgs {
    std::optional<SceneSnapshot> previousSnapshot;
    std::unordered_map<std::string, std::string> parameters;
    float preloadProgress = 1.0f;
};

struct SceneExitArgs {
    SceneFlags flags = SceneFlags::None;
};

} // namespace Application
} // namespace Render


