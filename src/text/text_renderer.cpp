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
#include "render/text/text_renderer.h"
#include "render/render_state.h"
#include "render/math_utils.h"
#include "render/render_layer.h"

namespace Render {

TextRenderer::TextRenderer(Renderer* renderer)
    : m_renderer(renderer)
    , m_renderablePool(32, 512) {  // 初始容量 32，最大容量 512
}

void TextRenderer::Begin() {
    m_instances.clear();
    // 归还上一批次的所有 renderables 到对象池
    m_renderablePool.Reset();
    m_activeRenderables.clear();
}

void TextRenderer::Draw(const TextPtr& text,
                        const Vector3& position,
                        float rotation,
                        const Vector2& scale) {
    if (!text) {
        return;
    }

    text->EnsureUpdated();
    Vector2 size = text->GetSize();

    m_instances.push_back(TextInstance{
        text,
        position,
        rotation,
        scale,
        size
    });
}

void TextRenderer::End() {
    if (!m_renderer || m_instances.empty()) {
        return;
    }

    float width = static_cast<float>(m_renderer->GetWidth());
    float height = static_cast<float>(m_renderer->GetHeight());
    if (width <= 0.0f) {
        width = 1.0f;
    }
    if (height <= 0.0f) {
        height = 1.0f;
    }

    TextRenderable::SetViewProjection(Matrix4::Identity(),
                                      MathUtils::Orthographic(0.0f, width, height, 0.0f, -1.0f, 1.0f));

    for (const auto& instance : m_instances) {
        // 从对象池获取 TextRenderable
        TextRenderable* renderable = m_renderablePool.Acquire();
        if (!renderable) {
            // 池已满，跳过此文本（或者可以记录警告）
            continue;
        }
        
        m_activeRenderables.push_back(renderable);
        renderable->ClearDepthHint();

        auto transform = renderable->GetTransform();
        if (!transform) {
            transform = CreateRef<Transform>();
            renderable->SetTransform(transform);
        }

        Vector2 scaledSize(instance.size.x() * instance.scale.x(),
                           instance.size.y() * instance.scale.y());

        TextAlignment alignment = TextAlignment::Left;
        if (instance.text) {
            alignment = instance.text->GetAlignment();
        }

        Vector3 anchor = instance.position;
        switch (alignment) {
            case TextAlignment::Center:
                anchor.x() -= scaledSize.x() * 0.5f;
                break;
            case TextAlignment::Right:
                anchor.x() -= scaledSize.x();
                break;
            case TextAlignment::Left:
            default:
                break;
        }

        Vector3 offset(scaledSize.x() * 0.5f, scaledSize.y() * 0.5f, 0.0f);
        transform->SetPosition(anchor + offset);
        transform->SetScale(Vector3(instance.scale.x(), instance.scale.y(), 1.0f));
        transform->SetRotation(MathUtils::FromEulerDegrees(0.0f, 0.0f, instance.rotation));

        renderable->SetText(instance.text);
        renderable->SetDepthHint((anchor + offset).z());
        renderable->SetVisible(true);
        renderable->SetLayerID(Layers::UI::Default.value);
        renderable->SubmitToRenderer(m_renderer);
    }

    m_renderer->FlushRenderQueue();
}

} // namespace Render


