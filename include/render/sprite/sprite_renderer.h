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


