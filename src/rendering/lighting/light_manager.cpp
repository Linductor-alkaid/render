#include "render/lighting/light_manager.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <shared_mutex>

namespace Render {
namespace Lighting {

namespace {

constexpr uint32_t kTypeShift = 32U;
constexpr uint64_t kIndexMask = 0x00000000FFFFFFFFULL;
constexpr float kPi = 3.14159265358979323846f;

inline float DegToRadians(float degrees) {
    return degrees * kPi / 180.0f;
}

} // namespace

LightManager::LightManager()
    : m_identifierCounter(1U) {
}

LightHandle LightManager::RegisterLight(const LightParameters& params) {
    if (params.type == LightType::Unknown) {
        return InvalidLightHandle;
    }

    auto handle = AllocateHandle(params.type);
    if (handle == InvalidLightHandle) {
        return InvalidLightHandle;
    }

    LightParameters stored = params;
    stored.type = params.type;

    std::unique_lock lock(m_mutex);
    auto [it, inserted] = m_lights.emplace(handle, LightRecord{stored, 1ULL});
    if (!inserted) {
        return InvalidLightHandle;
    }
    return handle;
}

bool LightManager::UpdateLight(LightHandle handle, const LightParameters& params) {
    if (handle == InvalidLightHandle) {
        return false;
    }

    auto expectedType = ExtractType(handle);
    LightParameters newParams = params;
    if (newParams.type == LightType::Unknown) {
        newParams.type = expectedType;
    }

    if (newParams.type != expectedType) {
        return false;
    }

    std::unique_lock lock(m_mutex);
    auto it = m_lights.find(handle);
    if (it == m_lights.end()) {
        return false;
    }

    it->second.parameters = newParams;
    ++it->second.revision;
    return true;
}

bool LightManager::SetLightEnabled(LightHandle handle, bool enabled) {
    if (handle == InvalidLightHandle) {
        return false;
    }

    std::unique_lock lock(m_mutex);
    auto it = m_lights.find(handle);
    if (it == m_lights.end()) {
        return false;
    }

    it->second.parameters.common.enabled = enabled;
    ++it->second.revision;
    return true;
}

bool LightManager::RemoveLight(LightHandle handle) {
    if (handle == InvalidLightHandle) {
        return false;
    }

    std::unique_lock lock(m_mutex);
    return m_lights.erase(handle) > 0;
}

std::optional<LightParameters> LightManager::GetLight(LightHandle handle) const {
    if (handle == InvalidLightHandle) {
        return std::nullopt;
    }

    std::shared_lock lock(m_mutex);
    auto it = m_lights.find(handle);
    if (it == m_lights.end()) {
        return std::nullopt;
    }
    return it->second.parameters;
}

void LightManager::ForEachLight(const std::function<void(const LightParameters&)>& visitor) const {
    std::shared_lock lock(m_mutex);
    for (const auto& [handle, record] : m_lights) {
        (void)handle;
        visitor(record.parameters);
    }
}

LightingFrameSnapshot LightManager::BuildFrameSnapshot(const Vector3& cameraPosition) const {
    LightingFrameSnapshot snapshot;

    std::shared_lock lock(m_mutex);
    snapshot.directionalLights.reserve(m_lights.size());
    snapshot.pointLights.reserve(m_lights.size());
    snapshot.spotLights.reserve(m_lights.size());
    snapshot.ambientLights.reserve(m_lights.size());

    for (const auto& [handle, record] : m_lights) {
        (void)handle;
        const auto& params = record.parameters;
        if (!params.common.enabled) {
            continue;
        }

        switch (params.type) {
            case LightType::Directional:
                snapshot.directionalLights.push_back(params);
                break;
            case LightType::Point:
                snapshot.pointLights.push_back(params);
                break;
            case LightType::Spot:
                snapshot.spotLights.push_back(params);
                break;
            case LightType::Ambient:
                snapshot.ambientLights.push_back(params);
                break;
            default:
                break;
        }
    }

    auto priorityComparator = [](const LightParameters& lhs, const LightParameters& rhs) {
        if (lhs.common.priority != rhs.common.priority) {
            return lhs.common.priority > rhs.common.priority;
        }
        if (lhs.common.intensity != rhs.common.intensity) {
            return lhs.common.intensity > rhs.common.intensity;
        }
        return lhs.common.layerID < rhs.common.layerID;
    };

    std::sort(snapshot.directionalLights.begin(), snapshot.directionalLights.end(), priorityComparator);
    std::sort(snapshot.ambientLights.begin(), snapshot.ambientLights.end(), priorityComparator);

    auto distanceAwareComparator = [&](const LightParameters& lhs, const LightParameters& rhs) {
        if (lhs.common.priority != rhs.common.priority) {
            return lhs.common.priority > rhs.common.priority;
        }
        float lhsDistanceSquared = 0.0f;
        float rhsDistanceSquared = 0.0f;
        if (lhs.type == LightType::Point) {
            lhsDistanceSquared = (lhs.point.position - cameraPosition).squaredNorm();
        } else if (lhs.type == LightType::Spot) {
            lhsDistanceSquared = (lhs.spot.position - cameraPosition).squaredNorm();
        }

        if (rhs.type == LightType::Point) {
            rhsDistanceSquared = (rhs.point.position - cameraPosition).squaredNorm();
        } else if (rhs.type == LightType::Spot) {
            rhsDistanceSquared = (rhs.spot.position - cameraPosition).squaredNorm();
        }

        if (lhsDistanceSquared != rhsDistanceSquared) {
            return lhsDistanceSquared < rhsDistanceSquared;
        }

        return lhs.common.intensity > rhs.common.intensity;
    };

    std::sort(snapshot.pointLights.begin(), snapshot.pointLights.end(), distanceAwareComparator);
    std::sort(snapshot.spotLights.begin(), snapshot.spotLights.end(), distanceAwareComparator);

    auto applyLimit = [](std::vector<LightParameters>& lights, uint32_t limit, uint32_t& culledCounter) {
        if (limit == 0) {
            culledCounter = static_cast<uint32_t>(lights.size());
            lights.clear();
            return;
        }
        if (lights.size() > limit) {
            culledCounter = static_cast<uint32_t>(lights.size() - limit);
            lights.resize(limit);
        } else {
            culledCounter = 0;
        }
    };

    applyLimit(snapshot.directionalLights, m_limits.maxDirectional, snapshot.culledDirectional);
    applyLimit(snapshot.pointLights, m_limits.maxPoint, snapshot.culledPoint);
    applyLimit(snapshot.spotLights, m_limits.maxSpot, snapshot.culledSpot);
    applyLimit(snapshot.ambientLights, m_limits.maxAmbient, snapshot.culledAmbient);

    // 预计算聚光灯的余弦用于 shader
    for (auto& spot : snapshot.spotLights) {
        spot.spot.innerCutoff = std::cos(DegToRadians(spot.spot.innerCutoff));
        spot.spot.outerCutoff = std::cos(DegToRadians(spot.spot.outerCutoff));
    }

    return snapshot;
}

void LightManager::SetLimits(const LightLimits& limits) {
    std::unique_lock lock(m_mutex);
    m_limits = limits;
}

LightLimits LightManager::GetLimits() const {
    std::shared_lock lock(m_mutex);
    return m_limits;
}

void LightManager::Clear() {
    std::unique_lock lock(m_mutex);
    m_lights.clear();
}

LightHandle LightManager::ComposeHandle(LightType type, uint32_t index) {
    if (type == LightType::Unknown || index == 0) {
        return InvalidLightHandle;
    }
    return (static_cast<uint64_t>(static_cast<uint32_t>(type)) << kTypeShift) |
           static_cast<uint64_t>(index);
}

uint32_t LightManager::ExtractIndex(LightHandle handle) {
    return static_cast<uint32_t>(handle & kIndexMask);
}

LightType LightManager::ExtractType(LightHandle handle) {
    return static_cast<LightType>(static_cast<uint32_t>(handle >> kTypeShift));
}

LightHandle LightManager::AllocateHandle(LightType type) {
    uint32_t index = m_identifierCounter.fetch_add(1U, std::memory_order_relaxed);
    if (index == 0U) { // 避免 0 句柄
        index = m_identifierCounter.fetch_add(1U, std::memory_order_relaxed);
        if (index == 0U) {
            return InvalidLightHandle;
        }
    }
    return ComposeHandle(type, index);
}

} // namespace Lighting
} // namespace Render


