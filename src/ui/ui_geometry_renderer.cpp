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
    m_transformPool.clear();
    m_transformPoolIndex = 0;
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

    // 创建基础材质
    m_solidMaterial = CreateRef<Render::Material>();
    m_solidMaterial->SetName("UIGeometrySolidMaterial");
    
    auto basicShader = ShaderCache::GetInstance().GetShader("basic");
    if (!basicShader) {
        basicShader = ShaderCache::GetInstance().LoadShader("basic", "shaders/basic.vert", "shaders/basic.frag");
    }
    if (!basicShader) {
        Logger::GetInstance().Error("[UIGeometryRenderer] Failed to load basic shader! Geometry rendering will not work.");
        m_solidMaterial.reset();
        return false;
    }
    
    m_solidMaterial->SetShader(basicShader);
    
    // 验证shader是否有必要的uniform
    auto* uniformMgr = basicShader->GetUniformManager();
    if (uniformMgr) {
        bool hasColor = uniformMgr->HasUniform("uColor");
        bool hasView = uniformMgr->HasUniform("uView");
        bool hasProjection = uniformMgr->HasUniform("uProjection");
        
        if (!hasColor) {
            Logger::GetInstance().Warning("[UIGeometryRenderer] Basic shader missing 'uColor' uniform. Color rendering may not work.");
        }
        if (!hasView || !hasProjection) {
            Logger::GetInstance().Warning("[UIGeometryRenderer] Basic shader missing view/projection uniforms. Geometry rendering may not work.");
        }
    }

    m_initialized = true;
    Logger::GetInstance().Info("[UIGeometryRenderer] Initialized successfully with basic shader.");
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
    m_transformPool.clear();
    m_transformPoolIndex = 0;
    m_initialized = false;
}

void UIGeometryRenderer::ResetMeshPool() {
    m_meshPoolIndex = 0;
}

void UIGeometryRenderer::ResetSpritePool() {
    m_spritePoolIndex = 0;
}

void UIGeometryRenderer::ResetTransformPool() {
    m_transformPoolIndex = 0;
}

Ref<Render::Transform> UIGeometryRenderer::AcquireTransform() {
    if (m_transformPoolIndex >= m_transformPool.size()) {
        m_transformPool.emplace_back(CreateRef<Render::Transform>());
    }
    auto transform = m_transformPool[m_transformPoolIndex++];
    // 重置 Transform 状态，确保干净的状态
    transform->SetPosition(Vector3::Zero());
    transform->SetRotation(Quaternion::Identity());
    transform->SetScale(Vector3::Ones());
    return transform;
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
    // 生成segments个顶点（不包括重复的最后一个点），形成闭合的多边形
    vertices.reserve(segments);

    const float angleStep = 2.0f * 3.14159265359f / static_cast<float>(segments);
    for (int i = 0; i < segments; ++i) {
        float angle = static_cast<float>(i) * angleStep;
        Vector2 point(center.x() + radius * std::cos(angle), center.y() + radius * std::sin(angle));
        vertices.push_back(point);
    }
    // 注意：不需要添加最后一个点（角度=2π），因为三角剖分算法会自动处理闭合多边形

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
    
    // 四个圆角中心
    Vector2 topLeftCenter(x + radius, y + radius);
    Vector2 topRightCenter(x + w - radius, y + radius);
    Vector2 bottomRightCenter(x + w - radius, y + h - radius);
    Vector2 bottomLeftCenter(x + radius, y + h - radius);
    
    // 辅助函数：生成圆弧顶点
    auto AddArc = [&](const Vector2& center, float startAngle, float endAngle, int segs) {
        for (int i = 0; i <= segs; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(segs);
            float angle = startAngle + t * (endAngle - startAngle);
            vertices.emplace_back(center.x() + radius * std::cos(angle), 
                                 center.y() + radius * std::sin(angle));
        }
    };
    
    // 从左上角开始，逆时针方向生成顶点
    AddArc(topLeftCenter, pi * 1.5f, pi, segments);
    AddArc(bottomLeftCenter, pi, pi * 0.5f, segments);
    AddArc(bottomRightCenter, pi * 0.5f, 0.0f, segments);
    AddArc(topRightCenter, 0.0f, -pi * 0.5f, segments);
    
    return vertices;
}

// Ear Clipping 三角剖分算法
std::vector<uint32_t> UIGeometryRenderer::TriangulatePolygon(const std::vector<Vector2>& vertices) {
    std::vector<uint32_t> indices;
    
    if (vertices.size() < 3) {
        return indices;
    }
    
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
            
            float area = TriangleArea(prevIdx, currIdx, nextIdx);
            if ((isClockwise && area < 0.0f) || (!isClockwise && area > 0.0f)) {
                continue;
            }
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
                indices.push_back(prevIdx);
                indices.push_back(currIdx);
                indices.push_back(nextIdx);
                vertexIndices.erase(vertexIndices.begin() + i);
                earFound = true;
                break;
            }
        }
        
        if (!earFound) {
            // 退化情况，使用扇形剖分
            for (size_t i = 1; i < vertexIndices.size() - 1; ++i) {
                indices.push_back(vertexIndices[0]);
                indices.push_back(vertexIndices[i]);
                indices.push_back(vertexIndices[i + 1]);
            }
            break;
        }
    }
    
    if (vertexIndices.size() == 3) {
        indices.push_back(vertexIndices[0]);
        indices.push_back(vertexIndices[1]);
        indices.push_back(vertexIndices[2]);
    }
    
    return indices;
}

Render::MeshRenderable* UIGeometryRenderer::AcquireMeshRenderable() {
    if (m_meshPoolIndex >= m_meshPool.size()) {
        m_meshPool.emplace_back(std::make_unique<Render::MeshRenderable>());
    }
    return m_meshPool[m_meshPoolIndex++].get();
}

Render::SpriteRenderable* UIGeometryRenderer::AcquireSpriteRenderable() {
    if (m_spritePoolIndex >= m_spritePool.size()) {
        m_spritePool.emplace_back(std::make_unique<Render::SpriteRenderable>());
    }
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

    // 延长线段以避免间隙
    const float overlapExtension = width * 0.5f;
    Vector2 extendedStart = start - dir * overlapExtension;
    Vector2 extendedEnd = end + dir * overlapExtension;
    float extendedLength = length + 2.0f * overlapExtension;

    Vector2 center = (extendedStart + extendedEnd) * 0.5f;
    float angle = std::atan2(dir.y(), dir.x());

    // 使用对象池获取 Transform，避免频繁创建销毁导致的生命周期问题
    auto transform = AcquireTransform();
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

    std::vector<uint32_t> indices = TriangulatePolygon(vertices);
    if (indices.empty()) {
        return;
    }
    
    std::vector<Render::Vertex> meshVertices;
    meshVertices.reserve(vertices.size());
    for (const auto& v : vertices) {
        Render::Vertex vertex;
        vertex.position = Vector3(v.x(), v.y(), -depth * 0.001f);
        vertex.texCoord = Vector2(0.0f, 0.0f);
        vertex.normal = Vector3(0.0f, 0.0f, 1.0f);
        // 参考实现：顶点颜色直接使用传入的颜色
        // Material的ApplyProperties会自动设置uUseVertexColor=true
        // 为了正确显示颜色，需要将Material的diffuseColor设置为白色
        // 这样最终颜色 = uColor(白色) * VertexColor(color) = color
        vertex.color = color;
        meshVertices.push_back(vertex);
    }
    
    auto mesh = CreateRef<Render::Mesh>(meshVertices, indices);
    mesh->Upload();
    
    // 参考实现：顶点颜色使用传入的color，Material的diffuseColor设置为白色
    // Material的ApplyProperties会自动设置uUseVertexColor=true
    // 最终颜色 = uColor(白色) * VertexColor(color) = color，颜色正确显示
    // 注意：由于m_solidMaterial是共享的，如果同一帧渲染多个不同颜色的图形，
    // 后面的颜色会覆盖前面的。但由于渲染是提交到队列的，每个MeshRenderable
    // 在渲染时会使用Material的当前状态，所以需要确保在提交前设置好所有属性。
    m_solidMaterial->SetDiffuseColor(Color::White());
    m_solidMaterial->SetMatrix4("uView", view);
    m_solidMaterial->SetMatrix4("uProjection", projection);
    
    // 调试日志（仅在Debug模式下启用，避免性能影响）
    #ifdef _DEBUG
    static int debugLogCount = 0;
    if (debugLogCount < 10) {  // 只记录前10次
        Logger::GetInstance().DebugFormat(
            "[UIGeometryRenderer] RenderFilledPolygon: color=(%.3f,%.3f,%.3f,%.3f), vertices=%zu",
            color.r, color.g, color.b, color.a, vertices.size());
        debugLogCount++;
    }
    #endif
    
    auto* meshRenderable = AcquireMeshRenderable();
    meshRenderable->SetMesh(mesh);
    meshRenderable->SetMaterial(m_solidMaterial);
    
    // 使用对象池获取 Transform，避免频繁创建销毁导致的生命周期问题
    auto transform = AcquireTransform();
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

    if (cmd.filled) {
        std::vector<Vector2> vertices = {
            Vector2(cmd.rect.x, cmd.rect.y),
            Vector2(cmd.rect.x + cmd.rect.width, cmd.rect.y),
            Vector2(cmd.rect.x + cmd.rect.width, cmd.rect.y + cmd.rect.height),
            Vector2(cmd.rect.x, cmd.rect.y + cmd.rect.height)
        };
        RenderFilledPolygon(vertices, cmd.fillColor, cmd.depth, cmd.layerID, view, projection, renderer);
    }

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

    if (cmd.filled) {
        auto vertices = GenerateCircle(cmd.center, cmd.radius, cmd.segments);
        if (vertices.size() < 3) {
            Logger::GetInstance().WarningFormat(
                "[UIGeometryRenderer] RenderCircle: Not enough vertices (%zu) for filled circle, segments=%d",
                vertices.size(), cmd.segments);
            return;
        }
        
        // 调试日志：检查填充圆形的渲染
        static int debugCircleCount = 0;
        if (debugCircleCount < 10) {
            Logger::GetInstance().InfoFormat(
                "[UIGeometryRenderer] RenderCircle filled: center=(%.1f,%.1f), radius=%.2f, "
                "color=(%.2f,%.2f,%.2f,%.2f), depth=%.3f, vertices=%zu",
                cmd.center.x(), cmd.center.y(), cmd.radius,
                cmd.fillColor.r, cmd.fillColor.g, cmd.fillColor.b, cmd.fillColor.a,
                cmd.depth, vertices.size());
            debugCircleCount++;
        }
        
        RenderFilledPolygon(vertices, cmd.fillColor, cmd.depth, cmd.layerID, view, projection, renderer);
    }
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

    if (cmd.filled) {
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
            auto vertices = GenerateRoundedRectangle(cmd.rect, cmd.cornerRadius, cmd.segments);
            RenderFilledPolygon(vertices, cmd.fillColor, cmd.depth, cmd.layerID, view, projection, renderer);
        }
    }
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

    if (cmd.filled) {
        RenderFilledPolygon(cmd.vertices, cmd.fillColor, cmd.depth, cmd.layerID, view, projection, renderer);
    }

    if (cmd.stroked && cmd.strokeWidth > 0.0f) {
        RenderStrokedPolygon(cmd.vertices, cmd.strokeColor, cmd.strokeWidth, cmd.depth, cmd.layerID, view, projection, renderer);
    }
}

} // namespace Render::UI
