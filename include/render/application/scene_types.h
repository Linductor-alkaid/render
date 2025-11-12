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


