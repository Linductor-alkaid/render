#pragma once

#include "render/lighting/light.h"
#include <atomic>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Render {
namespace Lighting {

/**
 * @brief 光源数量限制配置
 */
struct LightLimits {
    uint32_t maxDirectional = 2;
    uint32_t maxPoint = 4;
    uint32_t maxSpot = 2;
    uint32_t maxAmbient = 1;
};

/**
 * @brief 帧级光源快照
 */
struct LightingFrameSnapshot {
    std::vector<LightParameters> directionalLights;
    std::vector<LightParameters> pointLights;
    std::vector<LightParameters> spotLights;
    std::vector<LightParameters> ambientLights;

    uint32_t culledDirectional = 0;
    uint32_t culledPoint = 0;
    uint32_t culledSpot = 0;
    uint32_t culledAmbient = 0;
};

/**
 * @brief 光照管理器，负责注册、更新与帧级同步
 */
class LightManager {
public:
    LightManager();
    ~LightManager() = default;

    LightManager(const LightManager&) = delete;
    LightManager& operator=(const LightManager&) = delete;

    LightManager(LightManager&&) = delete;
    LightManager& operator=(LightManager&&) = delete;

    /**
     * @brief 注册光源，返回句柄
     */
    LightHandle RegisterLight(const LightParameters& params);

    /**
     * @brief 使用完整参数更新光源
     */
    bool UpdateLight(LightHandle handle, const LightParameters& params);

    /**
     * @brief 更新光源的启用状态
     */
    bool SetLightEnabled(LightHandle handle, bool enabled);

    /**
     * @brief 移除光源
     */
    bool RemoveLight(LightHandle handle);

    /**
     * @brief 获取光源参数副本
     */
    std::optional<LightParameters> GetLight(LightHandle handle) const;

    /**
     * @brief 对所有光源执行回调（读锁）
     */
    void ForEachLight(const std::function<void(const LightParameters&)>& visitor) const;

    /**
     * @brief 构建帧级快照并按优先级/距离裁剪
     * @param cameraPosition 世界空间相机位置
     */
    LightingFrameSnapshot BuildFrameSnapshot(const Vector3& cameraPosition) const;

    /**
     * @brief 设置光源数量限制
     */
    void SetLimits(const LightLimits& limits);

    /**
     * @brief 获取光源数量限制
     */
    LightLimits GetLimits() const;

    /**
     * @brief 清空所有光源
     */
    void Clear();

private:
    struct LightRecord {
        LightParameters parameters;
        uint64_t revision = 0;
    };

    static LightHandle ComposeHandle(LightType type, uint32_t index);
    static uint32_t ExtractIndex(LightHandle handle);
    static LightType ExtractType(LightHandle handle);

    LightHandle AllocateHandle(LightType type);

    mutable std::shared_mutex m_mutex;
    std::unordered_map<LightHandle, LightRecord> m_lights;
    std::atomic<uint32_t> m_identifierCounter;
    LightLimits m_limits;
};

} // namespace Lighting
} // namespace Render


