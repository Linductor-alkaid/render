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
#include "render/mesh.h"
#include "render/material.h"
#include "render/shader_cache.h"

namespace Render::UI {

UIGeometryRenderer::UIGeometryRenderer() = default;

UIGeometryRenderer::~UIGeometryRenderer() {
    Shutdown();
    // 清空对象池
    m_meshPool.clear();
    m_meshPoolIndex = 0;
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

    // 创建基础材质（用于填充多边形）
    m_solidMaterial = CreateRef<Render::Material>();
    m_solidMaterial->SetName("UIGeometrySolidMaterial");
    
    // 尝试获取基础着色器，如果没有则使用默认
    auto basicShader = ShaderCache::GetInstance().GetShader("basic");
    if (!basicShader) {
        // 如果基础着色器不存在，尝试加载
        basicShader = ShaderCache::GetInstance().LoadShader("basic", "shaders/basic.vert", "shaders/basic.frag");
    }
    if (basicShader) {
        m_solidMaterial->SetShader(basicShader);
    }

    m_initialized = true;
    return true;
}

void UIGeometryRenderer::Shutdown() {
    if (!m_initialized) {
        return;
    }

    m_solidTexture.reset();
    m_solidMaterial.reset();
    // 清空对象池
    m_meshPool.clear();
    m_meshPoolIndex = 0;
    m_spritePool.clear();
    m_spritePoolIndex = 0;
    m_initialized = false;
}

void UIGeometryRenderer::ResetMeshPool() {
    // 重置索引，准备下一帧使用
    m_meshPoolIndex = 0;
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
    
    const float pi = 3.14159265359f;
    const float angleStep = pi * 0.5f / static_cast<float>(segments);
    
    // 四个圆角中心
    Vector2 topLeftCenter(x + radius, y + radius);
    Vector2 topRightCenter(x + w - radius, y + radius);
    Vector2 bottomRightCenter(x + w - radius, y + h - radius);
    Vector2 bottomLeftCenter(x + radius, y + h - radius);
    
    // 路径顺序：从左上角开始，逆时针方向
    // 注意：在UI坐标系中，Y轴向下为正，但三角函数使用标准数学坐标系（Y向上）
    // 标准坐标系：0°=右，90°=上，180°=左，270°=下
    // 对于UI坐标系，我们需要调整角度：UI的"上"对应标准坐标系的270°，UI的"下"对应标准坐标系的90°
    
    // 辅助函数：生成圆弧顶点
    auto AddArc = [&](const Vector2& center, float startAngle, float endAngle, int segs) {
        for (int i = 0; i <= segs; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(segs);
            float angle = startAngle + t * (endAngle - startAngle);
            vertices.emplace_back(center.x() + radius * std::cos(angle), 
                                 center.y() + radius * std::sin(angle));
        }
    };
    
    // 1. 左上角圆弧（从顶部到左侧）
    // 标准坐标系：从270°（下，对应UI的顶部）到180°（左）
    if (radius < w * 0.5f) {
        vertices.emplace_back(x, y + radius);  // 顶部直线边的左端点
    }
    AddArc(topLeftCenter, pi * 1.5f, pi, segments);
    
    // 2. 左侧直线边（从上到下）
    if (radius < h * 0.5f) {
        vertices.emplace_back(x + radius, y + h - radius);
    }
    
    // 3. 左下角圆弧（从左侧到底部）
    // 标准坐标系：从180°（左）到90°（上，对应UI的底部）
    AddArc(bottomLeftCenter, pi, pi * 0.5f, segments);
    
    // 4. 底部直线边（从左到右）
    if (radius < w * 0.5f) {
        vertices.emplace_back(x + w - radius, y + h - radius);
    }
    
    // 5. 右下角圆弧（从底部到右侧）
    // 标准坐标系：从90°（上，对应UI的底部）到0°（右）
    AddArc(bottomRightCenter, pi * 0.5f, 0.0f, segments);
    
    // 6. 右侧直线边（从下到上）
    if (radius < h * 0.5f) {
        vertices.emplace_back(x + w - radius, y + radius);
    }
    
    // 7. 右上角圆弧（从右侧到顶部）
    // 标准坐标系：从0°（右）到270°（下，对应UI的顶部）
    // 注意：从0到270需要逆时针，即从0到-90（或从0到270，但需要确保方向）
    // 使用负角度：从0到-π/2，然后标准化
    for (int i = 0; i <= segments; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(segments);
        float angle = -t * pi * 0.5f;  // 从0到-π/2
        if (angle < 0) {
            angle += 2.0f * pi;  // 标准化到0-2π：从0到3π/2
        }
        vertices.emplace_back(topRightCenter.x() + radius * std::cos(angle), 
                             topRightCenter.y() + radius * std::sin(angle));
    }
    
    return vertices;
}

// Ear Clipping 三角剖分算法
std::vector<uint32_t> UIGeometryRenderer::TriangulatePolygon(const std::vector<Vector2>& vertices) {
    std::vector<uint32_t> indices;
    
    if (vertices.size() < 3) {
        return indices;
    }
    
    // 创建顶点索引列表
    std::vector<uint32_t> vertexIndices;
    vertexIndices.reserve(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        vertexIndices.push_back(static_cast<uint32_t>(i));
    }
    
    // 计算多边形方向（顺时针或逆时针）
    float signedArea = 0.0f;
    for (size_t i = 0; i < vertices.size(); ++i) {
        size_t next = (i + 1) % vertices.size();
        signedArea += (vertices[next].x() - vertices[i].x()) * (vertices[next].y() + vertices[i].y());
    }
    bool isClockwise = signedArea > 0.0f;
    
    // 辅助函数：计算三角形面积（带符号）
    auto TriangleArea = [&](uint32_t i0, uint32_t i1, uint32_t i2) -> float {
        const Vector2& v0 = vertices[i0];
        const Vector2& v1 = vertices[i1];
        const Vector2& v2 = vertices[i2];
        return (v1.x() - v0.x()) * (v2.y() - v0.y()) - (v2.x() - v0.x()) * (v1.y() - v0.y());
    };
    
    // 辅助函数：检查点是否在三角形内部
    auto PointInTriangle = [&](const Vector2& p, uint32_t i0, uint32_t i1, uint32_t i2) -> bool {
        const Vector2& v0 = vertices[i0];
        const Vector2& v1 = vertices[i1];
        const Vector2& v2 = vertices[i2];
        
        float d1 = (p.x() - v1.x()) * (v0.y() - v1.y()) - (v0.x() - v1.x()) * (p.y() - v1.y());
        float d2 = (p.x() - v2.x()) * (v1.y() - v2.y()) - (v1.x() - v2.x()) * (p.y() - v2.y());
        float d3 = (p.x() - v0.x()) * (v2.y() - v0.y()) - (v2.x() - v0.x()) * (p.y() - v0.y());
        
        bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
        bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
        
        return !(hasNeg && hasPos);
    };
    
    // Ear Clipping 主循环
    while (vertexIndices.size() > 3) {
        bool earFound = false;
        
        for (size_t i = 0; i < vertexIndices.size(); ++i) {
            uint32_t prevIdx = vertexIndices[(i + vertexIndices.size() - 1) % vertexIndices.size()];
            uint32_t currIdx = vertexIndices[i];
            uint32_t nextIdx = vertexIndices[(i + 1) % vertexIndices.size()];
            
            // 检查三角形方向是否正确
            float area = TriangleArea(prevIdx, currIdx, nextIdx);
            if ((isClockwise && area < 0.0f) || (!isClockwise && area > 0.0f)) {
                continue;  // 不是凸顶点
            }
            
            // 检查是否有其他顶点在三角形内部
            bool isEar = true;
            for (size_t j = 0; j < vertexIndices.size(); ++j) {
                uint32_t testIdx = vertexIndices[j];
                if (testIdx == prevIdx || testIdx == currIdx || testIdx == nextIdx) {
                    continue;
                }
                
                if (PointInTriangle(vertices[testIdx], prevIdx, currIdx, nextIdx)) {
                    isEar = false;
                    break;
                }
            }
            
            if (isEar) {
                // 找到耳朵，添加三角形索引
                indices.push_back(prevIdx);
                indices.push_back(currIdx);
                indices.push_back(nextIdx);
                
                // 移除当前顶点
                vertexIndices.erase(vertexIndices.begin() + i);
                earFound = true;
                break;
            }
        }
        
        if (!earFound) {
            // 如果找不到耳朵，可能是退化情况，使用扇形剖分
            for (size_t i = 1; i < vertexIndices.size() - 1; ++i) {
                indices.push_back(vertexIndices[0]);
                indices.push_back(vertexIndices[i]);
                indices.push_back(vertexIndices[i + 1]);
            }
            break;
        }
    }
    
    // 添加最后一个三角形
    if (vertexIndices.size() == 3) {
        indices.push_back(vertexIndices[0]);
        indices.push_back(vertexIndices[1]);
        indices.push_back(vertexIndices[2]);
    }
    
    return indices;
}

Render::MeshRenderable* UIGeometryRenderer::AcquireMeshRenderable() {
    // 如果对象池不够大，扩展它
    if (m_meshPoolIndex >= m_meshPool.size()) {
        m_meshPool.emplace_back(std::make_unique<Render::MeshRenderable>());
    }
    
    // 返回当前索引的对象，并递增索引
    return m_meshPool[m_meshPoolIndex++].get();
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

    // 为了确保线段之间没有间隙，稍微延长线段
    const float overlapExtension = width * 0.5f;
    Vector2 extendedStart = start - dir * overlapExtension;
    Vector2 extendedEnd = end + dir * overlapExtension;
    float extendedLength = length + 2.0f * overlapExtension;

    // 使用延长后的线段来渲染
    Vector2 center = (extendedStart + extendedEnd) * 0.5f;
    float angle = std::atan2(dir.y(), dir.x());

    auto transform = CreateRef<Transform>();
    transform->SetPosition(Vector3(center.x(), center.y(), -depth * 0.001f));
    transform->SetRotationEuler(Vector3(0.0f, 0.0f, angle));

    auto* sprite = AcquireSpriteRenderable();
    sprite->SetTransform(transform);
    sprite->SetLayerID(layerID);
    sprite->SetRenderPriority(static_cast<int32_t>(-depth * 1000.0f));
    sprite->SetTexture(m_solidTexture);
    sprite->SetSourceRect(Rect(0.0f, 0.0f, 1.0f, 1.0f));
    sprite->SetSize(Vector2(extendedLength, width));
    sprite->SetTintColor(color);
    sprite->SetViewProjectionOverride(view, projection);
    sprite->SubmitToRenderer(renderer);
}

void UIGeometryRenderer::RenderFilledPolygon(const std::vector<Vector2>& vertices, const Color& color, float depth, int layerID, const Matrix4& view, const Matrix4& projection, Render::Renderer* renderer) {
    if (vertices.size() < 3 || !m_solidMaterial || !renderer) {
        return;
    }

    // 使用三角剖分算法生成三角形索引
    std::vector<uint32_t> indices = TriangulatePolygon(vertices);
    if (indices.empty()) {
        return;
    }
    
    // 创建顶点数据（转换为3D顶点，z坐标为深度）
    std::vector<Render::Vertex> meshVertices;
    meshVertices.reserve(vertices.size());
    for (const auto& v : vertices) {
        Render::Vertex vertex;
        vertex.position = Vector3(v.x(), v.y(), -depth * 0.001f);
        vertex.texCoord = Vector2(0.0f, 0.0f);
        vertex.normal = Vector3(0.0f, 0.0f, 1.0f);
        vertex.color = color;
        meshVertices.push_back(vertex);
    }
    
    // 创建网格
    auto mesh = CreateRef<Render::Mesh>(meshVertices, indices);
    mesh->Upload();
    
    // 设置材质颜色和视图/投影矩阵
    // 关键修复：通过 Material 的 Uniform 设置视图和投影矩阵，确保使用UI的正交投影
    m_solidMaterial->SetDiffuseColor(color);
    m_solidMaterial->SetMatrix4("uView", view);
    m_solidMaterial->SetMatrix4("uProjection", projection);
    
    // 创建 MeshRenderable 并提交
    auto* meshRenderable = AcquireMeshRenderable();
    meshRenderable->SetMesh(mesh);
    meshRenderable->SetMaterial(m_solidMaterial);
    
    // 设置变换（单位矩阵，因为顶点已经包含位置信息）
    auto transform = CreateRef<Transform>();
    transform->SetPosition(Vector3::Zero());
    meshRenderable->SetTransform(transform);
    
    meshRenderable->SetLayerID(layerID);
    meshRenderable->SetRenderPriority(static_cast<int32_t>(-depth * 1000.0f));
    
    meshRenderable->SubmitToRenderer(renderer);
}

void UIGeometryRenderer::RenderStrokedPolygon(const std::vector<Vector2>& vertices, const Color& color, float strokeWidth, float depth, int layerID, const Matrix4& view, const Matrix4& projection, Render::Renderer* renderer) {
    if (vertices.size() < 2 || !m_solidTexture || !m_solidTexture->IsValid() || !renderer) {
        return;
    }

    // 移除重复的相邻顶点
    std::vector<Vector2> cleanedVertices;
    cleanedVertices.reserve(vertices.size());
    const float epsilon = 0.001f;
    
    for (size_t i = 0; i < vertices.size(); ++i) {
        size_t prev = (i + vertices.size() - 1) % vertices.size();
        Vector2 diff = vertices[i] - vertices[prev];
        if (diff.norm() > epsilon) {
            cleanedVertices.push_back(vertices[i]);
        }
    }
    
    if (cleanedVertices.size() < 2) {
        cleanedVertices = vertices;
    }
    
    // 渲染每条边，确保线段之间没有间隙
    for (size_t i = 0; i < cleanedVertices.size(); ++i) {
        size_t next = (i + 1) % cleanedVertices.size();
        RenderLineWithSprite(cleanedVertices[i], cleanedVertices[next], strokeWidth, color, depth, layerID, view, projection, renderer);
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

    if (!m_solidMaterial || !renderer) {
        return;
    }

    // 填充：直接使用 Mesh 渲染矩形（更高效）
    if (cmd.filled) {
        std::vector<Vector2> vertices = {
            Vector2(cmd.rect.x, cmd.rect.y),
            Vector2(cmd.rect.x + cmd.rect.width, cmd.rect.y),
            Vector2(cmd.rect.x + cmd.rect.width, cmd.rect.y + cmd.rect.height),
            Vector2(cmd.rect.x, cmd.rect.y + cmd.rect.height)
        };
        RenderFilledPolygon(vertices, cmd.fillColor, cmd.depth, cmd.layerID, view, projection, renderer);
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

    if (!m_solidMaterial || !renderer) {
        return;
    }

    // 填充：使用多边形渲染
    if (cmd.filled) {
        auto vertices = GenerateCircle(cmd.center, cmd.radius, cmd.segments);
        RenderFilledPolygon(vertices, cmd.fillColor, cmd.depth, cmd.layerID, view, projection, renderer);
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

    if (!m_solidMaterial || !renderer) {
        return;
    }

    // 填充
    if (cmd.filled) {
        // 如果圆角半径很小，直接使用矩形填充（更高效）
        const float maxRadius = std::min(cmd.rect.width, cmd.rect.height) * 0.5f;
        const float radius = std::min(cmd.cornerRadius, maxRadius);
        
        if (radius < 0.1f) {
            UIRectangleCommand rectCmd;
            rectCmd.rect = cmd.rect;
            rectCmd.fillColor = cmd.fillColor;
            rectCmd.strokeColor = Color(0, 0, 0, 0);
            rectCmd.strokeWidth = 0.0f;
            rectCmd.filled = true;
            rectCmd.stroked = false;
            rectCmd.layerID = cmd.layerID;
            rectCmd.depth = cmd.depth;
            RenderRectangle(rectCmd, view, projection, renderer);
        } else {
            // 使用多边形填充
            auto vertices = GenerateRoundedRectangle(cmd.rect, cmd.cornerRadius, cmd.segments);
            RenderFilledPolygon(vertices, cmd.fillColor, cmd.depth, cmd.layerID, view, projection, renderer);
        }
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
}