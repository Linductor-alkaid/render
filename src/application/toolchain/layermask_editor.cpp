#include "render/application/toolchain/layermask_editor.h"

#include "render/render_layer.h"
#include "render/logger.h"

#include <algorithm>
#include <bitset>

namespace Render::Application {

LayerMaskEditorDataSource::LayerMaskEditorDataSource(RenderLayerRegistry& registry)
    : m_registry(registry) {
}

std::vector<RenderLayerId> LayerMaskEditorDataSource::LayerMaskToLayers(uint32_t layerMask) const {
    std::vector<RenderLayerId> layers;
    
    auto allLayers = m_registry.ListLayers();
    for (const auto& record : allLayers) {
        if (!record.descriptor.id.IsValid()) {
            continue;
        }

        int maskIndex = GetLayerMaskIndex(record.descriptor.id);
        if (maskIndex < 0 || maskIndex >= 32) {
            continue;
        }

        // 检查对应的位是否设置
        if ((layerMask & (1u << maskIndex)) != 0) {
            layers.push_back(record.descriptor.id);
        }
    }

    return layers;
}

uint32_t LayerMaskEditorDataSource::LayersToLayerMask(const std::vector<RenderLayerId>& layers) const {
    uint32_t mask = 0;

    for (auto layerId : layers) {
        if (!layerId.IsValid()) {
            continue;
        }

        int maskIndex = GetLayerMaskIndex(layerId);
        if (maskIndex >= 0 && maskIndex < 32) {
            mask |= (1u << maskIndex);
        }
    }

    return mask;
}

bool LayerMaskEditorDataSource::IsLayerInMask(uint32_t layerMask, RenderLayerId layerId) const {
    if (!layerId.IsValid()) {
        return false;
    }

    int maskIndex = GetLayerMaskIndex(layerId);
    if (maskIndex < 0 || maskIndex >= 32) {
        return false;
    }

    return (layerMask & (1u << maskIndex)) != 0;
}

uint32_t LayerMaskEditorDataSource::SetLayerInMask(uint32_t layerMask, RenderLayerId layerId, bool enabled) const {
    if (!layerId.IsValid()) {
        return layerMask;
    }

    int maskIndex = GetLayerMaskIndex(layerId);
    if (maskIndex < 0 || maskIndex >= 32) {
        return layerMask;
    }

    if (enabled) {
        layerMask |= (1u << maskIndex);
    } else {
        layerMask &= ~(1u << maskIndex);
    }

    return layerMask;
}

std::vector<RenderLayerRecord> LayerMaskEditorDataSource::GetAllLayers() const {
    return m_registry.ListLayers();
}

LayerMaskInfo LayerMaskEditorDataSource::GetLayerMaskInfo(uint32_t layerMask, const std::string& name) const {
    LayerMaskInfo info;
    info.layerMask = layerMask;
    info.name = name;
    info.enabledLayers = LayerMaskToLayers(layerMask);
    return info;
}

uint32_t LayerMaskEditorDataSource::CreateEmptyMask() const {
    return 0;
}

uint32_t LayerMaskEditorDataSource::CreateFullMask() const {
    uint32_t mask = 0;
    auto allLayers = m_registry.ListLayers();
    
    for (const auto& record : allLayers) {
        if (!record.descriptor.id.IsValid()) {
            continue;
        }

        int maskIndex = GetLayerMaskIndex(record.descriptor.id);
        if (maskIndex >= 0 && maskIndex < 32) {
            mask |= (1u << maskIndex);
        }
    }

    return mask;
}

bool LayerMaskEditorDataSource::ValidateLayerMask(uint32_t layerMask) const {
    // 如果掩码为0，表示所有层级都禁用，这是有效的
    if (layerMask == 0) {
        return true;
    }

    // 检查掩码中是否有至少一个已注册的层级
    auto enabledLayers = LayerMaskToLayers(layerMask);
    return !enabledLayers.empty();
}

int LayerMaskEditorDataSource::GetLayerMaskIndex(RenderLayerId layerId) const {
    auto descriptor = m_registry.GetDescriptor(layerId);
    if (!descriptor.has_value()) {
        return -1;
    }

    // 使用描述符中的maskIndex字段
    // maskIndex对应CameraComponent::layerMask的比特索引（0-31）
    return descriptor->maskIndex;
}

} // namespace Render::Application

