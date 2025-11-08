#pragma once

#include "render/types.h"
#include "render/ecs/components.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <optional>
#include <vector>

namespace Render {

/**
 * @brief 精灵渲染层级管理器
 *
 * 通过命名层映射到具体的 layerID 与 sortBias，简化 UI/世界层级配置。
 * 提供线程安全的注册、查询与批量遍历接口，内部自带一组默认层。
 */
class SpriteRenderLayer {
public:
    struct LayerInfo {
        uint32_t layerID = 800;   ///< 渲染层 ID
        int32_t sortBias = 0;     ///< 默认排序偏移
    };

    /**
     * @brief 注册或覆盖一条渲染层
     * @param name      层名称（大小写不敏感）
     * @param layerID   渲染器使用的层 ID
     * @param sortBias  默认排序偏移量
     */
    static void RegisterLayer(const std::string& name, uint32_t layerID, int32_t sortBias = 0);

    /**
     * @brief 批量注册层信息
     */
    static void RegisterLayers(const std::vector<std::pair<std::string, LayerInfo>>& layers);

    /**
     * @brief 查询层信息
     * @param name 层名称
     * @return 若存在返回 LayerInfo，否则 std::nullopt
     */
    static std::optional<LayerInfo> GetLayer(const std::string& name);

    /**
     * @brief 是否存在指定层
     */
    static bool HasLayer(const std::string& name);

    /**
     * @brief 根据层名称设置组件 layerID 和 sortOrder
     * @param name        层名称
     * @param component   目标 SpriteRenderComponent
     * @param localOrder  附加排序偏移
     * @return 是否成功应用（若层不存在则返回 false）
     */
    static bool ApplyLayer(const std::string& name, ECS::SpriteRenderComponent& component, int32_t localOrder = 0);

    /**
     * @brief 列出所有已注册层
     */
    static std::vector<std::pair<std::string, LayerInfo>> ListLayers();

    /**
     * @brief 清空用户自定义层（保留默认层级）
     */
    static void ResetToDefaults();

private:
    static std::string NormalizeKey(const std::string& name);
    static void EnsureDefaults();

    static std::unordered_map<std::string, LayerInfo> s_layers;
    static std::mutex s_mutex;
    static bool s_defaultsInitialized;
};

} // namespace Render


