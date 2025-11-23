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

    // 使用三角扇（triangle fan）来渲染多边形
    // 以第一个顶点为中心，连接相邻的顶点形成三角形
    // 这样可以正确渲染圆角矩形等复杂形状，而不是使用边界框近似
    
    // 计算多边形的中心点（用于三角扇）
    Vector2 center(0.0f, 0.0f);
    for (const auto& v : vertices) {
        center += v;
    }
    center /= static_cast<float>(vertices.size());
    
    // 使用多个小矩形来近似每个三角形
    // 为了简化，我们使用扫描线算法：将多边形分解为水平条带
    // 但更简单的方法是使用多个小的矩形 Sprite 来覆盖多边形区域
    
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

    // 使用扫描线方法：将多边形分解为多个水平条带
    // 每个条带是一个矩形，只渲染在多边形内部的像素
    // 为了确保没有间隙，我们使用较小的步长，并确保相邻扫描线之间没有间隙
    const float scanLineStep = 0.25f;  // 使用更小的步长，确保覆盖所有区域，包括圆角过渡区域
    
    // 确保完全覆盖整个高度范围，从 minY 到 maxY
    // 使用连续的扫描线，确保相邻扫描线之间没有间隙
    float currentY = minY;
    
    while (currentY < maxY) {
        // 计算当前扫描线的顶部和底部
        float scanLineTop = currentY;
        float scanLineBottom = std::min(currentY + scanLineStep, maxY);
        float height = scanLineBottom - scanLineTop;
        float scanLineCenter = (scanLineTop + scanLineBottom) * 0.5f;
        
        // 确保高度至少为 0.01，避免渲染问题
        if (height < 0.01f) {
            currentY = maxY;  // 跳过这一条，直接结束
            break;
        }
        
        // 计算这条扫描线与多边形的所有交点
        // 使用扫描线的中心位置来计算交点，这样更准确
        std::vector<float> intersections;
        
        for (size_t j = 0; j < vertices.size(); ++j) {
            size_t next = (j + 1) % vertices.size();
            const Vector2& v1 = vertices[j];
            const Vector2& v2 = vertices[next];
            
            // 检查线段是否与扫描线相交
            float yMin = std::min(v1.y(), v2.y());
            float yMax = std::max(v1.y(), v2.y());
            
            // 检查扫描线的范围是否与线段相交
            // 使用更宽松的边界检查，确保边界上的点也能被包含
            if (scanLineBottom >= yMin && scanLineTop <= yMax) {
                // 如果线段是水平的，检查是否在扫描线范围内
                if (std::abs(v2.y() - v1.y()) < 0.0001f) {
                    // 水平线段：如果y坐标在扫描线范围内，添加两个端点
                    if (v1.y() >= scanLineTop && v1.y() <= scanLineBottom) {
                        intersections.push_back(std::min(v1.x(), v2.x()));
                        intersections.push_back(std::max(v1.x(), v2.x()));
                    }
                } else {
                    // 非水平线段：计算交点
                    // 使用扫描线的中心位置来计算交点
                    float t = (scanLineCenter - v1.y()) / (v2.y() - v1.y());
                    // 确保交点在线段范围内（使用更宽松的边界）
                    if (t >= -0.001f && t <= 1.001f) {
                        float x = v1.x() + t * (v2.x() - v1.x());
                        intersections.push_back(x);
                    }
                }
            }
        }
        
        // 对交点进行排序并去重（避免重复的交点）
        std::sort(intersections.begin(), intersections.end());
        intersections.erase(std::unique(intersections.begin(), intersections.end(), 
            [](float a, float b) { return std::abs(a - b) < 0.001f; }), intersections.end());
        
        // 成对处理交点，每对之间的区域属于多边形内部
        // 注意：交点数量应该是偶数（因为多边形是封闭的）
        // 如果交点数量是奇数，可能是边界情况，我们忽略最后一个交点
        size_t pairCount = intersections.size() / 2;
        for (size_t k = 0; k < pairCount; ++k) {
            float x1 = intersections[k * 2];
            float x2 = intersections[k * 2 + 1];
            float width = x2 - x1;
            
            if (width > 0.0f && height > 0.0f) {
                // 创建一个小矩形 Sprite 来填充这个区域
                // Sprite 的中心位置应该是扫描线的中心
                auto transform = CreateRef<Transform>();
                transform->SetPosition(Vector3(x1 + width * 0.5f, scanLineCenter, -depth * 0.001f));
                
                auto* sprite = AcquireSpriteRenderable();
                sprite->SetTransform(transform);
                sprite->SetLayerID(layerID);
                sprite->SetTexture(m_solidTexture);
                sprite->SetSourceRect(Rect(0.0f, 0.0f, 1.0f, 1.0f));
                sprite->SetSize(Vector2(width, height));
                sprite->SetTintColor(color);
                sprite->SetViewProjectionOverride(view, projection);
                sprite->SubmitToRenderer(renderer);
            }
        }
        
        // 移动到下一条扫描线（确保没有间隙）
        currentY = scanLineBottom;
    }
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

