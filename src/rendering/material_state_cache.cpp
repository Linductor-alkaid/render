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
#include "render/material_state_cache.h"
#include "render/material.h"
#include "render/render_state.h"

namespace Render {

MaterialStateCache& MaterialStateCache::Get() {
    thread_local MaterialStateCache cache;
    return cache;
}

void MaterialStateCache::Reset() {
    m_lastMaterial = nullptr;
    m_lastRenderState = nullptr;
}

bool MaterialStateCache::ShouldBind(const Material* material, RenderState* renderState) const {
    return material != m_lastMaterial || renderState != m_lastRenderState;
}

void MaterialStateCache::OnBind(const Material* material, RenderState* renderState) {
    m_lastMaterial = material;
    m_lastRenderState = renderState;
}

} // namespace Render


