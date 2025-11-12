#pragma once

#include "render/render_state.h"
#include "render/types.h"

#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Render {

struct RenderLayerId {
    uint32_t value = std::numeric_limits<uint32_t>::max();

    constexpr RenderLayerId() = default;
    constexpr explicit RenderLayerId(uint32_t v) : value(v) {}

    [[nodiscard]] constexpr bool IsValid() const {
        return value != std::numeric_limits<uint32_t>::max();
    }

    static constexpr RenderLayerId Invalid() {
        return RenderLayerId(std::numeric_limits<uint32_t>::max());
    }

    constexpr explicit operator uint32_t() const { return value; }

    constexpr bool operator==(RenderLayerId other) const { return value == other.value; }
    constexpr bool operator!=(RenderLayerId other) const { return value != other.value; }
    constexpr bool operator<(RenderLayerId other) const { return value < other.value; }
};

struct RenderLayerIdHash {
    std::size_t operator()(RenderLayerId id) const noexcept {
        return std::hash<uint32_t>{}(id.value);
    }
};

struct RenderLayerViewport {
    int32_t x = 0;
    int32_t y = 0;
    int32_t width = 0;
    int32_t height = 0;

    [[nodiscard]] bool IsEmpty() const {
        return width <= 0 || height <= 0;
    }
};

enum class RenderLayerType {
    World,
    ScreenSpace,
    Overlay
};

enum class LayerSortPolicy {
    OpaqueMaterialFirst,
    TransparentDepth,
    ScreenSpaceStable
};

struct RenderStateOverrides {
    std::optional<bool> depthTest;
    std::optional<bool> depthWrite;
    std::optional<DepthFunc> depthFunc;
    std::optional<BlendMode> blendMode;
    std::optional<CullFace> cullFace;
    std::optional<bool> scissorTest;
};

struct RenderLayerDescriptor {
    RenderLayerId id{};
    std::string name;
    uint32_t priority = 0;
    RenderLayerType type = RenderLayerType::World;
    LayerSortPolicy sortPolicy = LayerSortPolicy::OpaqueMaterialFirst;
    RenderStateOverrides defaultState{};
    bool enableByDefault = true;
    int32_t defaultSortBias = 0;
    uint8_t maskIndex = 0; ///< 对应 CameraComponent::layerMask 的比特索引（0-31）
};

struct RenderLayerState {
    bool enabled = true;
    RenderStateOverrides overrides{};
    std::optional<RenderLayerViewport> viewport;
    std::optional<RenderLayerViewport> scissorRect;
};

struct RenderLayerRecord {
    RenderLayerDescriptor descriptor;
    RenderLayerState state;
};

class RenderLayerRegistry {
public:
    RenderLayerRegistry();
    ~RenderLayerRegistry() = default;

    void SetDefaultLayers(const std::vector<RenderLayerDescriptor>& descriptors);

    void RegisterLayer(const RenderLayerDescriptor& descriptor);
    void RegisterLayers(const std::vector<RenderLayerDescriptor>& descriptors);
    void RegisterLayers(std::initializer_list<RenderLayerDescriptor> descriptors);

    [[nodiscard]] bool HasLayer(RenderLayerId id) const;

    [[nodiscard]] std::optional<RenderLayerDescriptor> GetDescriptor(RenderLayerId id) const;
    [[nodiscard]] std::optional<RenderLayerState> GetState(RenderLayerId id) const;

    bool SetEnabled(RenderLayerId id, bool enabled);
    bool SetOverrides(RenderLayerId id, const RenderStateOverrides& overrides);
    bool SetViewport(RenderLayerId id, const std::optional<RenderLayerViewport>& viewport);
    bool SetScissorRect(RenderLayerId id, const std::optional<RenderLayerViewport>& scissorRect);

    [[nodiscard]] std::vector<RenderLayerRecord> ListLayers() const;

    void Clear();
    void ResetToDefaults();

private:
    struct LayerEntry {
        RenderLayerDescriptor descriptor;
        RenderLayerState state;
    };

    std::unordered_map<uint32_t, LayerEntry> m_layers;
    std::vector<RenderLayerDescriptor> m_defaultDescriptors;
    mutable std::shared_mutex m_mutex;

    void RegisterLayerLocked(const RenderLayerDescriptor& descriptor);
};

namespace RenderLayerDefaults {

std::vector<RenderLayerDescriptor> CreateDefaultDescriptors();

} // namespace RenderLayerDefaults

namespace Layers {

namespace World {
constexpr RenderLayerId Background{680};
constexpr RenderLayerId Midground{700};
constexpr RenderLayerId Foreground{720};
} // namespace World

namespace UI {
constexpr RenderLayerId Background{780};
constexpr RenderLayerId Panel{790};
constexpr RenderLayerId Default{800};
constexpr RenderLayerId Foreground{810};
constexpr RenderLayerId Overlay{900};
constexpr RenderLayerId Tooltip{910};
} // namespace UI

namespace HUD {
constexpr RenderLayerId Overlay{905};
} // namespace HUD

namespace Debug {
constexpr RenderLayerId Overlay{999};
} // namespace Debug

} // namespace Layers

} // namespace Render


