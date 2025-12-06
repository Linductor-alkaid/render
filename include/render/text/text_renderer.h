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

#include "render/text/text.h"
#include "render/renderable.h"
#include "render/renderer.h"
#include "render/object_pool.h"
#include <vector>
#include <memory>

namespace Render {

/**
 * @brief 即时模式文本渲染器
 *
 * 与 SpriteRenderer 类似，提供 Begin/Draw/End 接口以便快速绘制 UI 文本。
 * 
 * 优化：
 * - 使用对象池减少 TextRenderable 的分配/释放开销
 * - 支持批量处理和复用
 */
class TextRenderer {
public:
    explicit TextRenderer(Renderer* renderer);

    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    /**
     * @brief 开始一批文本绘制
     */
    void Begin();

    /**
     * @brief 绘制文本
     * @param text 文本对象（共享）
     * @param position 世界或屏幕空间位置
     * @param rotation Z 轴旋转角度（度）
     * @param scale 额外缩放
     */
    void Draw(const TextPtr& text,
              const Vector3& position,
              float rotation = 0.0f,
              const Vector2& scale = Vector2(1.0f, 1.0f));

    /**
     * @brief 提交并渲染所有文本
     */
    void End();
    
    /**
     * @brief 获取对象池统计信息
     */
    size_t GetPoolSize() const { return m_renderablePool.GetPoolSize(); }
    size_t GetActiveRenderables() const { return m_renderablePool.GetActiveCount(); }

private:
    struct TextInstance {
        TextPtr text;
        Vector3 position;
        float rotation;
        Vector2 scale;
        Vector2 size;
    };

    Renderer* m_renderer;
    std::vector<TextInstance> m_instances;
    ObjectPool<TextRenderable> m_renderablePool;  // 对象池替代 unique_ptr vector
    std::vector<TextRenderable*> m_activeRenderables;  // 当前批次活跃的 renderables
};

} // namespace Render


