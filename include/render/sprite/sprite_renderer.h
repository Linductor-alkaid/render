#pragma once

#include "render/sprite/sprite.h"
#include "render/renderable.h"
#include "render/renderer.h"
#include "render/object_pool.h"
#include <vector>

namespace Render {

class SpriteRenderer {
public:
    explicit SpriteRenderer(Renderer* renderer);

    void Begin();
    void Draw(const Sprite& sprite, const Vector3& position, float rotation = 0.0f, const Vector2& scale = Vector2(1.0f, 1.0f));
    void End();
    
    /**
     * @brief 获取对象池统计信息
     */
    size_t GetPoolSize() const { return m_renderablePool.GetPoolSize(); }
    size_t GetActiveRenderables() const { return m_renderablePool.GetActiveCount(); }

private:
    struct SpriteInstance {
        Sprite sprite;
        Vector3 position;
        float rotation;
        Vector2 scale;
    };

    Renderer* m_renderer;
    std::vector<SpriteInstance> m_instances;
    ObjectPool<SpriteRenderable> m_renderablePool;  // 对象池替代单个 renderable
    std::vector<SpriteRenderable*> m_activeRenderables;  // 当前批次活跃的 renderables
};

} // namespace Render


