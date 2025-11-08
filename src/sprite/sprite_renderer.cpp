#include "render/sprite/sprite_renderer.h"
#include "render/render_state.h"
#include "render/transform.h"
#include "render/math_utils.h"

namespace Render {

SpriteRenderer::SpriteRenderer(Renderer* renderer)
    : m_renderer(renderer) {
}

void SpriteRenderer::Begin() {
    m_instances.clear();
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

        m_renderable.SetTexture(sprite.GetTexture());
        m_renderable.SetSourceRect(frame.uv);
        m_renderable.SetSize(Vector2(frame.size.x() * instance.scale.x(), frame.size.y() * instance.scale.y()));
        m_renderable.SetTintColor(sprite.GetTint());

        auto transform = std::make_shared<Transform>();
        transform->SetPosition(instance.position);
        transform->SetScale(Vector3(instance.scale.x(), instance.scale.y(), 1.0f));
        transform->SetRotation(MathUtils::FromEulerDegrees(0.0f, 0.0f, instance.rotation));
        m_renderable.SetTransform(transform);

        m_renderable.Render(renderState.get());
    }
}

} // namespace Render


