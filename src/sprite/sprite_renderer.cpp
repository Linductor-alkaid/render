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
#include "render/sprite/sprite_renderer.h"
#include "render/render_state.h"
#include "render/transform.h"
#include "render/math_utils.h"
#include "render/render_layer.h"

namespace Render {

SpriteRenderer::SpriteRenderer(Renderer* renderer)
    : m_renderer(renderer)
    , m_renderablePool(32, 512) {  // 初始容量 32，最大容量 512
}

void SpriteRenderer::Begin() {
    m_instances.clear();
    // 归还上一批次的所有 renderables 到对象池
    m_renderablePool.Reset();
    m_activeRenderables.clear();
}

void SpriteRenderer::Draw(const Sprite& sprite, const Vector3& position, float rotation, const Vector2& scale) {
    SpriteInstance instance{};
    instance.sprite = sprite;
    instance.position = position;
    instance.rotation = rotation;
    instance.scale = scale;
    m_instances.push_back(instance);
}

void SpriteRenderer::End() {
    if (!m_renderer || m_instances.empty()) {
        return;
    }

    auto renderState = m_renderer->GetRenderState();
    float width = static_cast<float>(m_renderer->GetWidth());
    float height = static_cast<float>(m_renderer->GetHeight());
    if (width <= 0.0f) {
        width = 1.0f;
    }
    if (height <= 0.0f) {
        height = 1.0f;
    }
    SpriteRenderable::SetViewProjection(Matrix4::Identity(), MathUtils::Orthographic(0.0f, width, height, 0.0f, -1.0f, 1.0f));

    for (const auto& instance : m_instances) {
        const Sprite& sprite = instance.sprite;
        const SpriteFrame& frame = sprite.GetFrame();

        // 从对象池获取 SpriteRenderable
        SpriteRenderable* renderable = m_renderablePool.Acquire();
        if (!renderable) {
            // 池已满，跳过此 sprite（或者可以记录警告）
            continue;
        }
        
        m_activeRenderables.push_back(renderable);

        renderable->SetTexture(sprite.GetTexture());
        renderable->SetSourceRect(frame.uv);
        renderable->SetSize(Vector2(frame.size.x() * instance.scale.x(), frame.size.y() * instance.scale.y()));
        renderable->SetTintColor(sprite.GetTint());

        auto transform = std::make_shared<Transform>();
        transform->SetPosition(instance.position);
        transform->SetScale(Vector3(instance.scale.x(), instance.scale.y(), 1.0f));
        transform->SetRotation(MathUtils::FromEulerDegrees(0.0f, 0.0f, instance.rotation));
        renderable->SetTransform(transform);
        
        renderable->SetVisible(true);
        renderable->SetLayerID(Layers::UI::Default.value);

        renderable->Render(renderState.get());
    }
}

} // namespace Render


