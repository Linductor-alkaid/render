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


