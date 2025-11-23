#pragma once

#include "render/types.h"
#include "render/ui/ui_render_commands.h"
#include <vector>
#include <memory>

namespace Render {
class Renderer;
class Shader;
class Texture;
}

namespace Render::UI {

/**
 * @brief UI 几何图形渲染器
 * 负责渲染直线、曲线、矩形、圆形、圆角矩形、多边形等几何图形
 */
class UIGeometryRenderer {
public:
    UIGeometryRenderer();
    ~UIGeometryRenderer();

    /**
     * @brief 初始化渲染器
     */
    bool Initialize();

    /**
     * @brief 关闭渲染器
     */
    void Shutdown();

    /**
     * @brief 渲染直线
     */
    void RenderLine(const UILineCommand& cmd, const Matrix4& view, const Matrix4& projection, Render::Renderer* renderer);

    /**
     * @brief 渲染贝塞尔曲线
     */
    void RenderBezierCurve(const UIBezierCurveCommand& cmd, const Matrix4& view, const Matrix4& projection, Render::Renderer* renderer);

    /**
     * @brief 渲染矩形
     */
    void RenderRectangle(const UIRectangleCommand& cmd, const Matrix4& view, const Matrix4& projection, Render::Renderer* renderer);

    /**
     * @brief 渲染圆形
     */
    void RenderCircle(const UICircleCommand& cmd, const Matrix4& view, const Matrix4& projection, Render::Renderer* renderer);

    /**
     * @brief 渲染圆角矩形
     */
    void RenderRoundedRectangle(const UIRoundedRectangleCommand& cmd, const Matrix4& view, const Matrix4& projection, Render::Renderer* renderer);

    /**
     * @brief 渲染多边形
     */
    void RenderPolygon(const UIPolygonCommand& cmd, const Matrix4& view, const Matrix4& projection, Render::Renderer* renderer);

    /**
     * @brief 重置对象池索引（在每帧开始时调用）
     */
    void ResetSpritePool();

private:
    /**
     * @brief 生成贝塞尔曲线顶点
     */
    std::vector<Vector2> GenerateBezierCurve(const Vector2& p0, const Vector2& p1, const Vector2& p2, const Vector2& p3, int segments);

    /**
     * @brief 生成圆形顶点
     */
    std::vector<Vector2> GenerateCircle(const Vector2& center, float radius, int segments);

    /**
     * @brief 生成圆角矩形顶点
     */
    std::vector<Vector2> GenerateRoundedRectangle(const Rect& rect, float cornerRadius, int segments);

    /**
     * @brief 使用Sprite渲染填充的多边形
     */
    void RenderFilledPolygon(const std::vector<Vector2>& vertices, const Color& color, float depth, int layerID, const Matrix4& view, const Matrix4& projection, Render::Renderer* renderer);

    /**
     * @brief 使用Sprite渲染描边的多边形
     */
    void RenderStrokedPolygon(const std::vector<Vector2>& vertices, const Color& color, float strokeWidth, float depth, int layerID, const Matrix4& view, const Matrix4& projection, Render::Renderer* renderer);

    /**
     * @brief 使用Sprite渲染直线
     */
    void RenderLineWithSprite(const Vector2& start, const Vector2& end, float width, const Color& color, float depth, int layerID, const Matrix4& view, const Matrix4& projection, Render::Renderer* renderer);

    /**
     * @brief 从对象池获取或创建 SpriteRenderable 对象
     */
    Render::SpriteRenderable* AcquireSpriteRenderable();

    bool m_initialized = false;
    Ref<Render::Texture> m_solidTexture;
    bool m_loggedTextureError = false;
    
    // 对象池：存储 SpriteRenderable 对象，确保它们在 FlushRenderQueue 处理前保持有效
    // 注意：这些对象会在每次渲染时重用，避免频繁分配/释放
    std::vector<std::unique_ptr<Render::SpriteRenderable>> m_spritePool;
    size_t m_spritePoolIndex = 0;  // 当前使用的对象索引
};

} // namespace Render::UI

