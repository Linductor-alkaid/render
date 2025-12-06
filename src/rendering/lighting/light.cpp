/*
 * Copyright (c) 2025 Li Chaoyu
 * 
 * This file is part of Render.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * For commercial licensing, please contact: 2052046346@qq.com
 */
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


