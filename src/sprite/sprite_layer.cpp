#include "render/sprite/sprite_layer.h"
#include "render/logger.h"
#include <algorithm>

namespace Render {

std::unordered_map<std::string, SpriteRenderLayer::LayerInfo> SpriteRenderLayer::s_layers;
std::mutex SpriteRenderLayer::s_mutex;
bool SpriteRenderLayer::s_defaultsInitialized = false;

std::string SpriteRenderLayer::NormalizeKey(const std::string& name) {
    std::string key = name;
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return key;
}

void SpriteRenderLayer::EnsureDefaults() {
    if (s_defaultsInitialized) {
        return;
    }

    s_layers.clear();
    s_layers.emplace(NormalizeKey("world.background"), LayerInfo{680, -100});
    s_layers.emplace(NormalizeKey("world.midground"), LayerInfo{700, 0});
    s_layers.emplace(NormalizeKey("world.foreground"), LayerInfo{720, 100});

    s_layers.emplace(NormalizeKey("ui.background"), LayerInfo{780, -200});
    s_layers.emplace(NormalizeKey("ui.panel"), LayerInfo{790, -50});
    s_layers.emplace(NormalizeKey("ui.default"), LayerInfo{800, 0});
    s_layers.emplace(NormalizeKey("ui.foreground"), LayerInfo{810, 50});
    s_layers.emplace(NormalizeKey("ui.overlay"), LayerInfo{900, 0});
    s_layers.emplace(NormalizeKey("ui.tooltip"), LayerInfo{910, 50});

    s_layers.emplace(NormalizeKey("debug.overlay"), LayerInfo{999, 0});

    s_defaultsInitialized = true;
}

void SpriteRenderLayer::RegisterLayer(const std::string& name, uint32_t layerID, int32_t sortBias) {
    if (name.empty()) {
        Logger::GetInstance().Warning("[SpriteRenderLayer] Attempted to register layer with empty name");
        return;
    }

    std::lock_guard<std::mutex> lock(s_mutex);
    EnsureDefaults();

    s_layers[NormalizeKey(name)] = LayerInfo{layerID, sortBias};
}

void SpriteRenderLayer::RegisterLayers(const std::vector<std::pair<std::string, LayerInfo>>& layers) {
    if (layers.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(s_mutex);
    EnsureDefaults();

    for (const auto& [name, info] : layers) {
        if (name.empty()) {
            Logger::GetInstance().Warning("[SpriteRenderLayer] Skipped registering layer with empty name");
            continue;
        }
        s_layers[NormalizeKey(name)] = info;
    }
}

std::optional<SpriteRenderLayer::LayerInfo> SpriteRenderLayer::GetLayer(const std::string& name) {
    if (name.empty()) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(s_mutex);
    EnsureDefaults();

    const auto it = s_layers.find(NormalizeKey(name));
    if (it == s_layers.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool SpriteRenderLayer::HasLayer(const std::string& name) {
    if (name.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(s_mutex);
    EnsureDefaults();
    return s_layers.find(NormalizeKey(name)) != s_layers.end();
}

bool SpriteRenderLayer::ApplyLayer(const std::string& name,
                                   ECS::SpriteRenderComponent& component,
                                   int32_t localOrder) {
    auto layerOpt = GetLayer(name);
    if (!layerOpt.has_value()) {
        Logger::GetInstance().WarningFormat(
            "[SpriteRenderLayer] Unknown layer '%s', keep current layerID=%u sortOrder=%d",
            name.c_str(),
            component.layerID,
            component.sortOrder);
        return false;
    }

    const auto& info = layerOpt.value();
    component.layerID = info.layerID;
    component.sortOrder = info.sortBias + localOrder;
    return true;
}

std::vector<std::pair<std::string, SpriteRenderLayer::LayerInfo>> SpriteRenderLayer::ListLayers() {
    std::lock_guard<std::mutex> lock(s_mutex);
    EnsureDefaults();

    std::vector<std::pair<std::string, LayerInfo>> result;
    result.reserve(s_layers.size());
    for (const auto& [name, info] : s_layers) {
        result.emplace_back(name, info);
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });
    return result;
}

void SpriteRenderLayer::ResetToDefaults() {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_defaultsInitialized = false;
    EnsureDefaults();
}

} // namespace Render


