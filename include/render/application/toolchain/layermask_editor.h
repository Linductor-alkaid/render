#pragma once

#include "render/render_layer.h"

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace Render::Application {

/**
 * @brief LayerMask信息（用于LayerMask编辑器）
 */
struct LayerMaskInfo {
    uint32_t layerMask = 0xFFFFFFFF;  // 32位掩码
    std::string name;                 // 掩码名称（可选）
    std::vector<RenderLayerId> enabledLayers;  // 启用的层级ID列表
};

/**
 * @brief LayerMask编辑器数据源接口
 * 
 * 为工具链提供LayerMask的查询和编辑功能
 */
class LayerMaskEditorDataSource {
public:
    LayerMaskEditorDataSource(RenderLayerRegistry& registry);
    ~LayerMaskEditorDataSource() = default;

    // ========================================================================
    // LayerMask查询接口
    // ========================================================================

    /**
     * @brief 将LayerMask转换为启用的层级ID列表
     * @param layerMask 32位掩码
     * @return 启用的层级ID列表
     */
    std::vector<RenderLayerId> LayerMaskToLayers(uint32_t layerMask) const;

    /**
     * @brief 将层级ID列表转换为LayerMask
     * @param layers 层级ID列表
     * @return 32位掩码
     */
    uint32_t LayersToLayerMask(const std::vector<RenderLayerId>& layers) const;

    /**
     * @brief 检查层级是否在LayerMask中
     * @param layerMask 32位掩码
     * @param layerId 层级ID
     * @return 是否在掩码中
     */
    bool IsLayerInMask(uint32_t layerMask, RenderLayerId layerId) const;

    /**
     * @brief 设置层级在LayerMask中的状态
     * @param layerMask 原始掩码（引用）
     * @param layerId 层级ID
     * @param enabled 是否启用
     * @return 新的掩码值
     */
    uint32_t SetLayerInMask(uint32_t layerMask, RenderLayerId layerId, bool enabled) const;

    /**
     * @brief 获取所有已注册的层级信息（用于编辑器显示）
     */
    std::vector<RenderLayerRecord> GetAllLayers() const;

    /**
     * @brief 根据LayerMask获取层级信息
     * @param layerMask 32位掩码
     * @return LayerMask信息
     */
    LayerMaskInfo GetLayerMaskInfo(uint32_t layerMask, const std::string& name = "") const;

    /**
     * @brief 创建空的LayerMask（所有层级都禁用）
     */
    uint32_t CreateEmptyMask() const;

    /**
     * @brief 创建包含所有层级的LayerMask
     */
    uint32_t CreateFullMask() const;

    /**
     * @brief 验证LayerMask是否有效
     * @param layerMask 32位掩码
     * @return 是否有效（至少包含一个已注册的层级）
     */
    bool ValidateLayerMask(uint32_t layerMask) const;

private:
    RenderLayerRegistry& m_registry;

    /**
     * @brief 根据层级ID获取对应的掩码位索引
     * @param layerId 层级ID
     * @return 位索引（0-31），如果层级不存在返回-1
     */
    int GetLayerMaskIndex(RenderLayerId layerId) const;
};

} // namespace Render::Application

