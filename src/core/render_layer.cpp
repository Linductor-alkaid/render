#include "render/render_layer.h"

#include "render/logger.h"

#include <algorithm>

namespace Render {

RenderLayerRegistry::RenderLayerRegistry() = default;

void RenderLayerRegistry::SetDefaultLayers(const std::vector<RenderLayerDescriptor>& descriptors) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_defaultDescriptors = descriptors;
}

void RenderLayerRegistry::RegisterLayer(const RenderLayerDescriptor& descriptor) {
    if (!descriptor.id.IsValid()) {
        Logger::GetInstance().Warning("[RenderLayerRegistry] Attempted to register layer with invalid id");
        return;
    }

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    RegisterLayerLocked(descriptor);
}

void RenderLayerRegistry::RegisterLayers(const std::vector<RenderLayerDescriptor>& descriptors) {
    if (descriptors.empty()) {
        return;
    }

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    for (const auto& descriptor : descriptors) {
        if (!descriptor.id.IsValid()) {
            Logger::GetInstance().Warning("[RenderLayerRegistry] Skipped registering layer with invalid id");
            continue;
        }
        RegisterLayerLocked(descriptor);
    }
}

void RenderLayerRegistry::RegisterLayers(std::initializer_list<RenderLayerDescriptor> descriptors) {
    if (descriptors.size() == 0) {
        return;
    }
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    for (const auto& descriptor : descriptors) {
        if (!descriptor.id.IsValid()) {
            Logger::GetInstance().Warning("[RenderLayerRegistry] Skipped registering layer with invalid id");
            continue;
        }
        RegisterLayerLocked(descriptor);
    }
}

bool RenderLayerRegistry::HasLayer(RenderLayerId id) const {
    if (!id.IsValid()) {
        return false;
    }

    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_layers.find(id.value) != m_layers.end();
}

std::optional<RenderLayerDescriptor> RenderLayerRegistry::GetDescriptor(RenderLayerId id) const {
    if (!id.IsValid()) {
        return std::nullopt;
    }

    std::shared_lock<std::shared_mutex> lock(m_mutex);
    const auto it = m_layers.find(id.value);
    if (it == m_layers.end()) {
        return std::nullopt;
    }
    return it->second.descriptor;
}

std::optional<RenderLayerState> RenderLayerRegistry::GetState(RenderLayerId id) const {
    if (!id.IsValid()) {
        return std::nullopt;
    }

    std::shared_lock<std::shared_mutex> lock(m_mutex);
    const auto it = m_layers.find(id.value);
    if (it == m_layers.end()) {
        return std::nullopt;
    }
    return it->second.state;
}

bool RenderLayerRegistry::SetEnabled(RenderLayerId id, bool enabled) {
    if (!id.IsValid()) {
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    const auto it = m_layers.find(id.value);
    if (it == m_layers.end()) {
        return false;
    }
    it->second.state.enabled = enabled;
    return true;
}

bool RenderLayerRegistry::SetOverrides(RenderLayerId id, const RenderStateOverrides& overrides) {
    if (!id.IsValid()) {
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    const auto it = m_layers.find(id.value);
    if (it == m_layers.end()) {
        return false;
    }
    it->second.state.overrides = overrides;
    return true;
}

bool RenderLayerRegistry::SetViewport(RenderLayerId id, const std::optional<RenderLayerViewport>& viewport) {
    if (!id.IsValid()) {
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    const auto it = m_layers.find(id.value);
    if (it == m_layers.end()) {
        return false;
    }
    it->second.state.viewport = viewport;
    return true;
}

bool RenderLayerRegistry::SetScissorRect(RenderLayerId id, const std::optional<RenderLayerViewport>& scissorRect) {
    if (!id.IsValid()) {
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    const auto it = m_layers.find(id.value);
    if (it == m_layers.end()) {
        return false;
    }
    it->second.state.scissorRect = scissorRect;
    return true;
}

std::vector<RenderLayerRecord> RenderLayerRegistry::ListLayers() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    std::vector<RenderLayerRecord> records;
    records.reserve(m_layers.size());

    for (const auto& [_, entry] : m_layers) {
        records.push_back(RenderLayerRecord{entry.descriptor, entry.state});
    }

    std::sort(records.begin(), records.end(), [](const RenderLayerRecord& a, const RenderLayerRecord& b) {
        if (a.descriptor.priority != b.descriptor.priority) {
            return a.descriptor.priority < b.descriptor.priority;
        }
        return a.descriptor.id.value < b.descriptor.id.value;
    });

    return records;
}

void RenderLayerRegistry::Clear() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_layers.clear();
}

void RenderLayerRegistry::ResetToDefaults() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_layers.clear();
    for (const auto& descriptor : m_defaultDescriptors) {
        if (!descriptor.id.IsValid()) {
            continue;
        }
        RegisterLayerLocked(descriptor);
    }
}

namespace {

RenderLayerDescriptor NormalizeDescriptor(const RenderLayerDescriptor& descriptor) {
    RenderLayerDescriptor normalized = descriptor;
    if (normalized.maskIndex >= 32) {
        Logger::GetInstance().WarningFormat(
            "[RenderLayerRegistry] maskIndex %u exceeds 31, wrapping to %u",
            static_cast<uint32_t>(normalized.maskIndex),
            static_cast<uint32_t>(normalized.maskIndex % 32));
        normalized.maskIndex = static_cast<uint8_t>(normalized.maskIndex % 32);
    }
    return normalized;
}

} // namespace

void RenderLayerRegistry::RegisterLayerLocked(const RenderLayerDescriptor& descriptor) {
    RenderLayerDescriptor storedDescriptor = NormalizeDescriptor(descriptor);

    auto it = m_layers.find(storedDescriptor.id.value);
    if (it != m_layers.end()) {
        it->second.descriptor = storedDescriptor;
        it->second.state.overrides = storedDescriptor.defaultState;
        return;
    }

    LayerEntry entry{};
    entry.descriptor = storedDescriptor;
    entry.state.enabled = storedDescriptor.enableByDefault;
    entry.state.overrides = storedDescriptor.defaultState;
    m_layers.emplace(storedDescriptor.id.value, std::move(entry));
}

namespace RenderLayerDefaults {

namespace {

RenderStateOverrides MakeWorldDefaultOverrides() {
    RenderStateOverrides overrides{};
    overrides.depthTest = true;
    overrides.depthWrite = true;
    overrides.blendMode = BlendMode::None;
    overrides.cullFace = CullFace::Back;
    return overrides;
}

RenderStateOverrides MakeBackgroundOverrides() {
    RenderStateOverrides overrides{};
    overrides.depthTest = false;
    overrides.depthWrite = false;
    overrides.blendMode = BlendMode::None;
    return overrides;
}

RenderStateOverrides MakeScreenSpaceOverrides() {
    RenderStateOverrides overrides{};
    overrides.depthTest = false;
    overrides.depthWrite = false;
    overrides.blendMode = BlendMode::Alpha;
    overrides.cullFace = CullFace::None;
    return overrides;
}

} // namespace

std::vector<RenderLayerDescriptor> CreateDefaultDescriptors() {
    std::vector<RenderLayerDescriptor> descriptors;
    descriptors.reserve(11);

    descriptors.push_back(RenderLayerDescriptor{
        Layers::World::Background,
        "world.background",
        100,
        RenderLayerType::World,
        LayerSortPolicy::OpaqueMaterialFirst,
        MakeBackgroundOverrides(),
        true,
        -100,
        static_cast<uint8_t>(0)
    });

    descriptors.push_back(RenderLayerDescriptor{
        Layers::World::Midground,
        "world.midground",
        200,
        RenderLayerType::World,
        LayerSortPolicy::OpaqueMaterialFirst,
        MakeWorldDefaultOverrides(),
        true,
        0,
        static_cast<uint8_t>(1)
    });

    descriptors.push_back(RenderLayerDescriptor{
        Layers::World::Foreground,
        "world.foreground",
        300,
        RenderLayerType::World,
        LayerSortPolicy::TransparentDepth,
        MakeWorldDefaultOverrides(),
        true,
        100,
        static_cast<uint8_t>(2)
    });

    descriptors.push_back(RenderLayerDescriptor{
        Layers::UI::Background,
        "ui.background",
        700,
        RenderLayerType::ScreenSpace,
        LayerSortPolicy::ScreenSpaceStable,
        MakeScreenSpaceOverrides(),
        true,
        -200,
        static_cast<uint8_t>(3)
    });

    descriptors.push_back(RenderLayerDescriptor{
        Layers::UI::Panel,
        "ui.panel",
        720,
        RenderLayerType::ScreenSpace,
        LayerSortPolicy::ScreenSpaceStable,
        MakeScreenSpaceOverrides(),
        true,
        -50,
        static_cast<uint8_t>(4)
    });

    descriptors.push_back(RenderLayerDescriptor{
        Layers::UI::Default,
        "ui.default",
        740,
        RenderLayerType::ScreenSpace,
        LayerSortPolicy::ScreenSpaceStable,
        MakeScreenSpaceOverrides(),
        true,
        0,
        static_cast<uint8_t>(5)
    });

    descriptors.push_back(RenderLayerDescriptor{
        Layers::UI::Foreground,
        "ui.foreground",
        760,
        RenderLayerType::ScreenSpace,
        LayerSortPolicy::ScreenSpaceStable,
        MakeScreenSpaceOverrides(),
        true,
        50,
        static_cast<uint8_t>(6)
    });

    descriptors.push_back(RenderLayerDescriptor{
        Layers::UI::Overlay,
        "ui.overlay",
        800,
        RenderLayerType::Overlay,
        LayerSortPolicy::ScreenSpaceStable,
        MakeScreenSpaceOverrides(),
        true,
        0,
        static_cast<uint8_t>(7)
    });

    descriptors.push_back(RenderLayerDescriptor{
        Layers::HUD::Overlay,
        "hud.overlay",
        810,
        RenderLayerType::Overlay,
        LayerSortPolicy::ScreenSpaceStable,
        MakeScreenSpaceOverrides(),
        true,
        200,
        static_cast<uint8_t>(10)
    });

    descriptors.push_back(RenderLayerDescriptor{
        Layers::UI::Tooltip,
        "ui.tooltip",
        820,
        RenderLayerType::Overlay,
        LayerSortPolicy::ScreenSpaceStable,
        MakeScreenSpaceOverrides(),
        true,
        50,
        static_cast<uint8_t>(8)
    });

    descriptors.push_back(RenderLayerDescriptor{
        Layers::Debug::Overlay,
        "debug.overlay",
        900,
        RenderLayerType::Overlay,
        LayerSortPolicy::ScreenSpaceStable,
        MakeScreenSpaceOverrides(),
        true,
        0,
        static_cast<uint8_t>(9)
    });

    return descriptors;
}

} // namespace RenderLayerDefaults

} // namespace Render


