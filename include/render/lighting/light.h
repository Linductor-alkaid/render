#pragma once

#include "render/types.h"
#include <cstdint>
#include <optional>
#include <shared_mutex>

namespace Render {
namespace Lighting {

/**
 * @brief 光源类型枚举
 */
enum class LightType : uint32_t {
    Unknown = 0,
    Directional = 1,
    Point = 2,
    Spot = 3,
    Ambient = 4
};

/**
 * @brief 无效光源句柄
 */
constexpr uint64_t InvalidLightHandle = 0ULL;

/**
 * @brief 光源句柄类型（高32位为类型，低32位为自增索引）
 */
using LightHandle = uint64_t;

/**
 * @brief 光源公共参数
 */
struct LightCommonProperties {
    Color color = Color::White();
    float intensity = 1.0f;
    bool castsShadows = false;
    bool enabled = true;
    int32_t priority = 0;
    uint32_t layerID = 300U; // 默认 WORLD_GEOMETRY
    float fadeDistance = 0.0f; // >0 时按距离渐隐
    float shadowBias = 0.001f;
};

/**
 * @brief 方向光参数
 */
struct DirectionalLightProperties {
    Vector3 direction = Vector3(0.0f, -1.0f, 0.0f); // 世界空间方向
};

/**
 * @brief 点光源参数
 */
struct PointLightProperties {
    Vector3 position = Vector3::Zero();
    Vector3 attenuation = Vector3(1.0f, 0.0f, 0.0f); // 常数、线性、二次衰减
    float range = 10.0f;
};

/**
 * @brief 聚光灯参数
 */
struct SpotLightProperties {
    Vector3 position = Vector3::Zero();
    Vector3 direction = Vector3(0.0f, -1.0f, 0.0f);
    Vector3 attenuation = Vector3(1.0f, 0.0f, 0.0f);
    float range = 15.0f;
    float innerCutoff = 20.0f;  // 角度（度）
    float outerCutoff = 25.0f;  // 角度（度）
};

/**
 * @brief 环境光参数
 */
struct AmbientLightProperties {
    float ambience = 1.0f; // 用于全局强度缩放
};

/**
 * @brief 光源参数总表
 */
struct LightParameters {
    LightType type = LightType::Unknown;
    LightCommonProperties common{};
    DirectionalLightProperties directional{};
    PointLightProperties point{};
    SpotLightProperties spot{};
    AmbientLightProperties ambient{};

    [[nodiscard]] bool IsEnabled() const { return common.enabled; }
};

/**
 * @brief 轻量级光源实例（线程安全）
 */
class Light {
public:
    explicit Light(const LightParameters& params = {});
    ~Light() = default;

    Light(const Light&) = delete;
    Light& operator=(const Light&) = delete;

    Light(Light&& other) noexcept;
    Light& operator=(Light&& other) noexcept;

    void SetParameters(const LightParameters& params);
    [[nodiscard]] LightParameters GetParameters() const;

    void SetEnabled(bool enabled);
    [[nodiscard]] bool IsEnabled() const;

    void SetPriority(int32_t priority);
    [[nodiscard]] int32_t GetPriority() const;

    void SetLayerID(uint32_t layerID);
    [[nodiscard]] uint32_t GetLayerID() const;

private:
    mutable std::shared_mutex m_mutex;
    LightParameters m_params;
};

} // namespace Lighting
} // namespace Render


