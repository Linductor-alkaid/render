#include "render/ui/ui_geometry_renderer.h"

#include <cmath>
#include <algorithm>
#include <memory>

#include "render/logger.h"
#include "render/renderer.h"
#include "render/texture.h"
#include "render/transform.h"
#include "render/math_utils.h"
#include "render/renderable.h"

namespace Render::UI {

UIGeometryRenderer::UIGeometryRenderer() = default;

UIGeometryRenderer::~UIGeometryRenderer() {
    Shutdown();
    // 清空对象池
    m_spritePool.clear();
    m_spritePoolIndex = 0;
}

bool UIGeometryRenderer::Initialize() {
    if (m_initialized) {
        return true;
    }

    // 创建纯色纹理用于渲染几何图形
    m_solidTexture = CreateRef<Render::Texture>();
    const uint32_t pixel = 0xFFFFFFFFu;
    if (!m_solidTexture->CreateFromData(&pixel, 1, 1, Render::TextureFormat::RGBA, false)) {
        if (!m_loggedTextureError) {
            Logger::GetInstance().Warning("[UIGeometryRenderer] Failed to create solid texture for geometry rendering.");
            m_loggedTextureError = true;
        }
        return false;
    }

    m_initialized = true;
    return true;
}

void UIGeometryRenderer::Shutdown() {
    if (!m_initialized) {
        return;
    }

    m_solidTexture.reset();
    // 清空对象池
    m_spritePool.clear();
    m_spritePoolIndex = 0;
    m_initialized = false;
}

void UIGeometryRenderer::ResetSpritePool() {
    // 重置索引，准备下一帧使用
    m_spritePoolIndex = 0;
}

std::vector<Vector2> UIGeometryRenderer::GenerateBezierCurve(const Vector2& p0, const Vector2& p1, const Vector2& p2, const Vector2& p3, int segments) {
    std::vector<Vector2> vertices;
    vertices.reserve(segments + 1);

    for (int i = 0; i <= segments; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(segments);
        float u = 1.0f - t;
        float tt = t * t;
        float uu = u * u;
        float uuu = uu * u;
        float ttt = tt * t;

        Vector2 point = uuu * p0 + 3.0f * uu * t * p1 + 3.0f * u * tt * p2 + ttt * p3;
        vertices.push_back(point);
    }

    return vertices;
}

std::vector<Vector2> UIGeometryRenderer::GenerateCircle(const Vector2& center, float radius, int segments) {
    std::vector<Vector2> vertices;
    vertices.reserve(segments + 1);

    const float angleStep = 2.0f * 3.14159265359f / static_cast<float>(segments);
    for (int i = 0; i <= segments; ++i) {
        float angle = static_cast<float>(i) * angleStep;
        Vector2 point(center.x() + radius * std::cos(angle), center.y() + radius * std::sin(angle));
        vertices.push_back(point);
    }

    return vertices;
}

std::vector<Vector2> UIGeometryRenderer::GenerateRoundedRectangle(const Rect& rect, float cornerRadius, int segments) {
    std::vector<Vector2> vertices;
    
    const float x = rect.x;
    const float y = rect.y;
    const float w = rect.width;
    const float h = rect.height;
    
    // 限制圆角半径不超过矩形尺寸的一半
    const float maxRadius = std::min(w, h) * 0.5f;
    const float radius = std::min(cornerRadius, maxRadius);
    
    const float angleStep = 3.14159265359f * 0.5f / static_cast<float>(segments);
    
    // 四个圆角中心
    Vector2 topLeft(x + radius, y + radius);
    Vector2 topRight(x + w - radius, y + radius);
    Vector2 bottomRight(x + w - radius, y + h - radius);
    Vector2 bottomLeft(x + radius, y + h - radius);
    
    // 左上角
    for (int i = 0; i <= segments; ++i) {
        float angle = 3.14159265359f + static_cast<float>(i) * angleStep;
        vertices.emplace_back(topLeft.x() + radius * std::cos(angle), topLeft.y() + radius * std::sin(angle));
    }
    
    // 右上角
    for (int i = 0; i <= segments; ++i) {
        float angle = 1.5f * 3.14159265359f + static_cast<float>(i) * angleStep;
        vertices.emplace_back(topRight.x() + radius * std::cos(angle), topRight.y() + radius * std::sin(angle));
    }
    
    // 右下角
    for (int i = 0; i <= segments; ++i) {
        float angle = static_cast<float>(i) * angleStep;
        vertices.emplace_back(bottomRight.x() + radius * std::cos(angle), bottomRight.y() + radius * std::sin(angle));
    }
    
    // 左下角
    for (int i = 0; i <= segments; ++i) {
        float angle = 0.5f * 3.14159265359f + static_cast<float>(i) * angleStep;
        vertices.emplace_back(bottomLeft.x() + radius * std::cos(angle), bottomLeft.y() + radius * std::sin(angle));
    }
    
    return vertices;
}

Render::SpriteRenderable* UIGeometryRenderer::AcquireSpriteRenderable() {
    // 如果对象池不够大，扩展它
    if (m_spritePoolIndex >= m_spritePool.size()) {
        m_spritePool.emplace_back(std::make_unique<Render::SpriteRenderable>());
    }
    
    // 返回当前索引的对象，并递增索引
    return m_spritePool[m_spritePoolIndex++].get();
}

void UIGeometryRenderer::RenderLineWithSprite(const Vector2& start, const Vector2& end, float width, const Color& color, float depth, int layerID, const Matrix4& view, const Matrix4& projection, Render::Renderer* renderer) {
    if (!m_solidTexture || !m_solidTexture->IsValid() || !renderer) {
        return;
    }

    Vector2 dir = end - start;
    float length = dir.norm();
    if (length < 0.001f) {
        return;
    }

    dir.normalize();
    Vector2 perp(-dir.y(), dir.x());  // 垂直向量

    // 计算矩形的四个顶点
    Vector2 halfWidth = perp * (width * 0.5f);
    Vector2 p0 = start + halfWidth;
    Vector2 p1 = start - halfWidth;
    Vector2 p2 = end - halfWidth;
    Vector2 p3 = end + halfWidth;

    // 使用两个三角形渲染矩形
    // 这里简化处理，使用一个大的Sprite来渲染
    Vector2 center = (start + end) * 0.5f;
    float angle = std::atan2(dir.y(), dir.x());

    auto transform = CreateRef<Transform>();
    transform->SetPosition(Vector3(center.x(), center.y(), -depth * 0.001f));
    transform->SetRotationEuler(Vector3(0.0f, 0.0f, angle));

    // 从对象池获取 SpriteRenderable 对象，确保在 FlushRenderQueue 处理前保持有效
    auto* sprite = AcquireSpriteRenderable();
    sprite->SetTransform(transform);
    sprite->SetLayerID(layerID);
    sprite->SetTexture(m_solidTexture);
    sprite->SetSourceRect(Rect(0.0f, 0.0f, 1.0f, 1.0f));
    sprite->SetSize(Vector2(length, width));
    sprite->SetTintColor(color);
    sprite->SetViewProjectionOverride(view, projection);
    sprite->SubmitToRenderer(renderer);
}

void UIGeometryRenderer::RenderFilledPolygon(const std::vector<Vector2>& vertices, const Color& color, float depth, int layerID, const Matrix4& view, const Matrix4& projection, Render::Renderer* renderer) {
    if (vertices.size() < 3 || !m_solidTexture || !m_solidTexture->IsValid() || !renderer) {
        return;
    }

    // 计算多边形边界框
    float minX = vertices[0].x();
    float maxX = vertices[0].x();
    float minY = vertices[0].y();
    float maxY = vertices[0].y();

    for (const auto& v : vertices) {
        minX = std::min(minX, v.x());
        maxX = std::max(maxX, v.x());
        minY = std::min(minY, v.y());
        maxY = std::max(maxY, v.y());
    }

    // 使用边界框渲染（简化实现，实际应该使用三角剖分）
    // 这里使用一个大的Sprite来近似渲染
    Vector2 center((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);
    Vector2 size(maxX - minX, maxY - minY);

    auto transform = CreateRef<Transform>();
    transform->SetPosition(Vector3(center.x(), center.y(), -depth * 0.001f));

    // 从对象池获取 SpriteRenderable 对象，确保在 FlushRenderQueue 处理前保持有效
    auto* sprite = AcquireSpriteRenderable();
    sprite->SetTransform(transform);
    sprite->SetLayerID(layerID);
    sprite->SetTexture(m_solidTexture);
    sprite->SetSourceRect(Rect(0.0f, 0.0f, 1.0f, 1.0f));
    sprite->SetSize(size);
    sprite->SetTintColor(color);
    sprite->SetViewProjectionOverride(view, projection);
    sprite->SubmitToRenderer(renderer);
}

void UIGeometryRenderer::RenderStrokedPolygon(const std::vector<Vector2>& vertices, const Color& color, float strokeWidth, float depth, int layerID, const Matrix4& view, const Matrix4& projection, Render::Renderer* renderer) {
    if (vertices.size() < 2 || !m_solidTexture || !m_solidTexture->IsValid() || !renderer) {
        return;
    }

    // 渲染每条边
    for (size_t i = 0; i < vertices.size(); ++i) {
        size_t next = (i + 1) % vertices.size();
        RenderLineWithSprite(vertices[i], vertices[next], strokeWidth, color, depth, layerID, view, projection, renderer);
    }
}

void UIGeometryRenderer::RenderLine(const UILineCommand& cmd, const Matrix4& view, const Matrix4& projection, Render::Renderer* renderer) {
    if (!m_initialized) {
        Initialize();
    }
    RenderLineWithSprite(cmd.start, cmd.end, cmd.width, cmd.color, cmd.depth, cmd.layerID, view, projection, renderer);
}

void UIGeometryRenderer::RenderBezierCurve(const UIBezierCurveCommand& cmd, const Matrix4& view, const Matrix4& projection, Render::Renderer* renderer) {
    if (!m_initialized) {
        Initialize();
    }

    auto vertices = GenerateBezierCurve(cmd.p0, cmd.p1, cmd.p2, cmd.p3, cmd.segments);
    if (vertices.size() < 2) {
        return;
    }

    // 渲染为连接的线段
    for (size_t i = 0; i < vertices.size() - 1; ++i) {
        RenderLineWithSprite(vertices[i], vertices[i + 1], cmd.width, cmd.color, cmd.depth, cmd.layerID, view, projection, renderer);
    }
}

void UIGeometryRenderer::RenderRectangle(const UIRectangleCommand& cmd, const Matrix4& view, const Matrix4& projection, Render::Renderer* renderer) {
    if (!m_initialized) {
        Initialize();
    }

    if (!m_solidTexture || !m_solidTexture->IsValid() || !renderer) {
        return;
    }

    // 填充
    if (cmd.filled) {
        auto transform = CreateRef<Transform>();
        transform->SetPosition(Vector3(cmd.rect.x + cmd.rect.width * 0.5f, cmd.rect.y + cmd.rect.height * 0.5f, -cmd.depth * 0.001f));

        // 从对象池获取 SpriteRenderable 对象，确保在 FlushRenderQueue 处理前保持有效
        auto* sprite = AcquireSpriteRenderable();
        sprite->SetTransform(transform);
        sprite->SetLayerID(cmd.layerID);
        sprite->SetTexture(m_solidTexture);
        sprite->SetSourceRect(Rect(0.0f, 0.0f, 1.0f, 1.0f));
        sprite->SetSize(Vector2(cmd.rect.width, cmd.rect.height));
        sprite->SetTintColor(cmd.fillColor);
        sprite->SetViewProjectionOverride(view, projection);
        sprite->SubmitToRenderer(renderer);
    }

    // 描边
    if (cmd.stroked && cmd.strokeWidth > 0.0f) {
        std::vector<Vector2> vertices = {
            Vector2(cmd.rect.x, cmd.rect.y),
            Vector2(cmd.rect.x + cmd.rect.width, cmd.rect.y),
            Vector2(cmd.rect.x + cmd.rect.width, cmd.rect.y + cmd.rect.height),
            Vector2(cmd.rect.x, cmd.rect.y + cmd.rect.height)
        };
        RenderStrokedPolygon(vertices, cmd.strokeColor, cmd.strokeWidth, cmd.depth, cmd.layerID, view, projection, renderer);
    }
}

void UIGeometryRenderer::RenderCircle(const UICircleCommand& cmd, const Matrix4& view, const Matrix4& projection, Render::Renderer* renderer) {
    if (!m_initialized) {
        Initialize();
    }

    if (!m_solidTexture || !m_solidTexture->IsValid() || !renderer) {
        return;
    }

    // 填充
    if (cmd.filled) {
        auto transform = CreateRef<Transform>();
        transform->SetPosition(Vector3(cmd.center.x(), cmd.center.y(), -cmd.depth * 0.001f));

        // 从对象池获取 SpriteRenderable 对象，确保在 FlushRenderQueue 处理前保持有效
        auto* sprite = AcquireSpriteRenderable();
        sprite->SetTransform(transform);
        sprite->SetLayerID(cmd.layerID);
        sprite->SetTexture(m_solidTexture);
        sprite->SetSourceRect(Rect(0.0f, 0.0f, 1.0f, 1.0f));
        sprite->SetSize(Vector2(cmd.radius * 2.0f, cmd.radius * 2.0f));
        sprite->SetTintColor(cmd.fillColor);
        sprite->SetViewProjectionOverride(view, projection);
        sprite->SubmitToRenderer(renderer);
    }

    // 描边
    if (cmd.stroked && cmd.strokeWidth > 0.0f) {
        auto vertices = GenerateCircle(cmd.center, cmd.radius, cmd.segments);
        RenderStrokedPolygon(vertices, cmd.strokeColor, cmd.strokeWidth, cmd.depth, cmd.layerID, view, projection, renderer);
    }
}

void UIGeometryRenderer::RenderRoundedRectangle(const UIRoundedRectangleCommand& cmd, const Matrix4& view, const Matrix4& projection, Render::Renderer* renderer) {
    if (!m_initialized) {
        Initialize();
    }

    if (!m_solidTexture || !m_solidTexture->IsValid() || !renderer) {
        return;
    }

    // 填充
    if (cmd.filled) {
        auto vertices = GenerateRoundedRectangle(cmd.rect, cmd.cornerRadius, cmd.segments);
        RenderFilledPolygon(vertices, cmd.fillColor, cmd.depth, cmd.layerID, view, projection, renderer);
    }

    // 描边
    if (cmd.stroked && cmd.strokeWidth > 0.0f) {
        auto vertices = GenerateRoundedRectangle(cmd.rect, cmd.cornerRadius, cmd.segments);
        RenderStrokedPolygon(vertices, cmd.strokeColor, cmd.strokeWidth, cmd.depth, cmd.layerID, view, projection, renderer);
    }
}

void UIGeometryRenderer::RenderPolygon(const UIPolygonCommand& cmd, const Matrix4& view, const Matrix4& projection, Render::Renderer* renderer) {
    if (!m_initialized) {
        Initialize();
    }

    if (cmd.vertices.size() < 3) {
        return;
    }

    // 填充
    if (cmd.filled) {
        RenderFilledPolygon(cmd.vertices, cmd.fillColor, cmd.depth, cmd.layerID, view, projection, renderer);
    }

    // 描边
    if (cmd.stroked && cmd.strokeWidth > 0.0f) {
        RenderStrokedPolygon(cmd.vertices, cmd.strokeColor, cmd.strokeWidth, cmd.depth, cmd.layerID, view, projection, renderer);
    }
}

} // namespace Render::UI

