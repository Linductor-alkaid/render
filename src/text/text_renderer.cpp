#include "render/text/text_renderer.h"
#include "render/render_state.h"
#include "render/math_utils.h"

namespace Render {

TextRenderer::TextRenderer(Renderer* renderer)
    : m_renderer(renderer) {
}

void TextRenderer::Begin() {
    m_instances.clear();
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

    if (m_renderables.size() < m_instances.size()) {
        size_t oldSize = m_renderables.size();
        m_renderables.resize(m_instances.size());
        for (size_t i = oldSize; i < m_renderables.size(); ++i) {
            m_renderables[i] = std::make_unique<TextRenderable>();
        }
    }

    for (size_t i = 0; i < m_instances.size(); ++i) {
        const auto& instance = m_instances[i];
        auto& renderablePtr = m_renderables[i];
        if (!renderablePtr) {
            renderablePtr = std::make_unique<TextRenderable>();
        }
        TextRenderable* renderable = renderablePtr.get();
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
        renderable->SetLayerID(800);
        renderable->SubmitToRenderer(m_renderer);
    }

    m_renderer->FlushRenderQueue();
}

} // namespace Render


