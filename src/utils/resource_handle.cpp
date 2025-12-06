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
#include "render/resource_handle.h"
#include "render/resource_manager.h"
#include "render/texture.h"
#include "render/mesh.h"
#include "render/material.h"
#include "render/shader.h"

namespace Render {

// ============================================================================
// TextureHandle 实现
// ============================================================================

template<>
Texture* ResourceHandle<Texture>::Get() const {
    if (m_id == INVALID_RESOURCE_ID) {
        return nullptr;
    }
    return ResourceManager::GetInstance().GetTextureByHandle(*this);
}

template<>
std::shared_ptr<Texture> ResourceHandle<Texture>::GetShared() const {
    if (m_id == INVALID_RESOURCE_ID) {
        return nullptr;
    }
    return ResourceManager::GetInstance().GetTextureSharedByHandle(*this);
}

template<>
bool ResourceHandle<Texture>::IsValid() const {
    if (m_id == INVALID_RESOURCE_ID) {
        return false;
    }
    return ResourceManager::GetInstance().IsTextureHandleValid(*this);
}

// ============================================================================
// MeshHandle 实现
// ============================================================================

template<>
Mesh* ResourceHandle<Mesh>::Get() const {
    if (m_id == INVALID_RESOURCE_ID) {
        return nullptr;
    }
    return ResourceManager::GetInstance().GetMeshByHandle(*this);
}

template<>
std::shared_ptr<Mesh> ResourceHandle<Mesh>::GetShared() const {
    if (m_id == INVALID_RESOURCE_ID) {
        return nullptr;
    }
    return ResourceManager::GetInstance().GetMeshSharedByHandle(*this);
}

template<>
bool ResourceHandle<Mesh>::IsValid() const {
    if (m_id == INVALID_RESOURCE_ID) {
        return false;
    }
    return ResourceManager::GetInstance().IsMeshHandleValid(*this);
}

// ============================================================================
// MaterialHandle 实现
// ============================================================================

template<>
Material* ResourceHandle<Material>::Get() const {
    if (m_id == INVALID_RESOURCE_ID) {
        return nullptr;
    }
    return ResourceManager::GetInstance().GetMaterialByHandle(*this);
}

template<>
std::shared_ptr<Material> ResourceHandle<Material>::GetShared() const {
    if (m_id == INVALID_RESOURCE_ID) {
        return nullptr;
    }
    return ResourceManager::GetInstance().GetMaterialSharedByHandle(*this);
}

template<>
bool ResourceHandle<Material>::IsValid() const {
    if (m_id == INVALID_RESOURCE_ID) {
        return false;
    }
    return ResourceManager::GetInstance().IsMaterialHandleValid(*this);
}

// ============================================================================
// ShaderHandle 实现
// ============================================================================

template<>
Shader* ResourceHandle<Shader>::Get() const {
    if (m_id == INVALID_RESOURCE_ID) {
        return nullptr;
    }
    return ResourceManager::GetInstance().GetShaderByHandle(*this);
}

template<>
std::shared_ptr<Shader> ResourceHandle<Shader>::GetShared() const {
    if (m_id == INVALID_RESOURCE_ID) {
        return nullptr;
    }
    return ResourceManager::GetInstance().GetShaderSharedByHandle(*this);
}

template<>
bool ResourceHandle<Shader>::IsValid() const {
    if (m_id == INVALID_RESOURCE_ID) {
        return false;
    }
    return ResourceManager::GetInstance().IsShaderHandleValid(*this);
}

} // namespace Render

