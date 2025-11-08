#pragma once

#include "render/sprite/sprite.h"
#include "render/renderable.h"
#include "render/renderer.h"
#include <vector>

namespace Render {

class SpriteRenderer {
public:
    explicit SpriteRenderer(Renderer* renderer);

    void Begin();
    void Draw(const Sprite& sprite, const Vector3& position, float rotation = 0.0f, const Vector2& scale = Vector2(1.0f, 1.0f));
    void End();

private:
    struct SpriteInstance {
        Sprite sprite;
        Vector3 position;
        float rotation;
        Vector2 scale;
    };

    Renderer* m_renderer;
    std::vector<SpriteInstance> m_instances;
    SpriteRenderable m_renderable;
};

} // namespace Render


