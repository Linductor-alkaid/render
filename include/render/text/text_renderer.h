#pragma once

#include "render/text/text.h"
#include "render/renderable.h"
#include "render/renderer.h"
#include <vector>
#include <memory>

namespace Render {

/**
 * @brief 即时模式文本渲染器
 *
 * 与 SpriteRenderer 类似，提供 Begin/Draw/End 接口以便快速绘制 UI 文本。
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
    std::vector<std::unique_ptr<TextRenderable>> m_renderables;
};

} // namespace Render


