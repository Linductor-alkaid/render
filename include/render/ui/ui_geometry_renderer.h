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

#include "render/types.h"
#include "render/ui/ui_render_commands.h"
#include <vector>
#include <memory>
#include <cstdint>

namespace Render {
class Renderer;
class Shader;
class Texture;
class Material;
class MeshRenderable;
class SpriteRenderable;
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

    /**
     * @brief 重置网格对象池索引（在每帧开始时调用）
     */
    void ResetMeshPool();

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
     * @brief 三角剖分多边形（Ear Clipping算法）
     */
    std::vector<uint32_t> TriangulatePolygon(const std::vector<Vector2>& vertices);

    /**
     * @brief 使用Mesh渲染填充的多边形
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

    /**
     * @brief 从对象池获取或创建 MeshRenderable 对象
     */
    Render::MeshRenderable* AcquireMeshRenderable();

    bool m_initialized = false;
    Ref<Render::Texture> m_solidTexture;
    Ref<Render::Material> m_solidMaterial;  // 用于填充多边形的材质
    bool m_loggedTextureError = false;
    
    // 对象池：存储 SpriteRenderable 对象，用于描边和线段渲染
    std::vector<std::unique_ptr<Render::SpriteRenderable>> m_spritePool;
    size_t m_spritePoolIndex = 0;  // 当前使用的对象索引
    
    // 对象池：存储 MeshRenderable 对象，用于填充多边形
    std::vector<std::unique_ptr<Render::MeshRenderable>> m_meshPool;
    size_t m_meshPoolIndex = 0;  // 当前使用的对象索引
};

} // namespace Render::UI

