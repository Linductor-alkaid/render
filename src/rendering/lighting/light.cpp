#include "render/lighting/light.h"

#include <mutex>
#include <shared_mutex>
#include <utility>

namespace Render {
namespace Lighting {

Light::Light(const LightParameters& params)
    : m_params(params) {
}

Light::Light(Light&& other) noexcept {
    std::unique_lock lock(other.m_mutex);
    m_params = other.m_params;
}

Light& Light::operator=(Light&& other) noexcept {
    if (this != &other) {
        std::scoped_lock guard(m_mutex, other.m_mutex);
        m_params = other.m_params;
    }
    return *this;
}

void Light::SetParameters(const LightParameters& params) {
    std::unique_lock lock(m_mutex);
    m_params = params;
}

LightParameters Light::GetParameters() const {
    std::shared_lock lock(m_mutex);
    return m_params;
}

void Light::SetEnabled(bool enabled) {
    std::unique_lock lock(m_mutex);
    m_params.common.enabled = enabled;
}

bool Light::IsEnabled() const {
    std::shared_lock lock(m_mutex);
    return m_params.common.enabled;
}

void Light::SetPriority(int32_t priority) {
    std::unique_lock lock(m_mutex);
    m_params.common.priority = priority;
}

int32_t Light::GetPriority() const {
    std::shared_lock lock(m_mutex);
    return m_params.common.priority;
}

void Light::SetLayerID(uint32_t layerID) {
    std::unique_lock lock(m_mutex);
    m_params.common.layerID = layerID;
}

uint32_t Light::GetLayerID() const {
    std::shared_lock lock(m_mutex);
    return m_params.common.layerID;
}

} // namespace Lighting
} // namespace Render


