#include "render/mesh_loader.h"
#include "render/logger.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Render {

// ============================================================================
// MeshLoader 实现
// ============================================================================

Ref<Mesh> MeshLoader::CreatePlane(float width, float height, 
                                   uint32_t widthSegments, uint32_t heightSegments,
                                   const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    // 确保至少有 1 个分段
    widthSegments = std::max(1u, widthSegments);
    heightSegments = std::max(1u, heightSegments);
    
    float halfWidth = width * 0.5f;
    float halfHeight = height * 0.5f;
    
    // 生成顶点
    for (uint32_t y = 0; y <= heightSegments; ++y) {
        for (uint32_t x = 0; x <= widthSegments; ++x) {
            float u = static_cast<float>(x) / widthSegments;
            float v = static_cast<float>(y) / heightSegments;
            
            Vertex vertex;
            vertex.position = Vector3(
                (u - 0.5f) * width,
                0.0f,
                (v - 0.5f) * height
            );
            vertex.texCoord = Vector2(u, v);
            vertex.normal = Vector3(0.0f, 1.0f, 0.0f);  // 向上
            vertex.color = color;
            
            vertices.push_back(vertex);
        }
    }
    
    // 生成索引
    for (uint32_t y = 0; y < heightSegments; ++y) {
        for (uint32_t x = 0; x < widthSegments; ++x) {
            uint32_t i0 = y * (widthSegments + 1) + x;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + widthSegments + 1;
            uint32_t i3 = i2 + 1;
            
            // 第一个三角形
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);
            
            // 第二个三角形
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }
    
    auto mesh = CreateRef<Mesh>(vertices, indices);
    mesh->Upload();
    
    Logger::GetInstance().Info("Created plane mesh: " + std::to_string(vertices.size()) + " vertices");
    return mesh;
}

Ref<Mesh> MeshLoader::CreateCube(float width, float height, float depth, const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    float hw = width * 0.5f;
    float hh = height * 0.5f;
    float hd = depth * 0.5f;
    
    // 每个面 4 个顶点，6 个面共 24 个顶点
    // 这样每个面可以有独立的法线和纹理坐标
    
    // 前面 (+Z)
    vertices.push_back(Vertex(Vector3(-hw, -hh,  hd), Vector2(0, 1), Vector3(0, 0, 1), color));
    vertices.push_back(Vertex(Vector3( hw, -hh,  hd), Vector2(1, 1), Vector3(0, 0, 1), color));
    vertices.push_back(Vertex(Vector3( hw,  hh,  hd), Vector2(1, 0), Vector3(0, 0, 1), color));
    vertices.push_back(Vertex(Vector3(-hw,  hh,  hd), Vector2(0, 0), Vector3(0, 0, 1), color));
    
    // 后面 (-Z)
    vertices.push_back(Vertex(Vector3( hw, -hh, -hd), Vector2(0, 1), Vector3(0, 0, -1), color));
    vertices.push_back(Vertex(Vector3(-hw, -hh, -hd), Vector2(1, 1), Vector3(0, 0, -1), color));
    vertices.push_back(Vertex(Vector3(-hw,  hh, -hd), Vector2(1, 0), Vector3(0, 0, -1), color));
    vertices.push_back(Vertex(Vector3( hw,  hh, -hd), Vector2(0, 0), Vector3(0, 0, -1), color));
    
    // 右面 (+X)
    vertices.push_back(Vertex(Vector3( hw, -hh,  hd), Vector2(0, 1), Vector3(1, 0, 0), color));
    vertices.push_back(Vertex(Vector3( hw, -hh, -hd), Vector2(1, 1), Vector3(1, 0, 0), color));
    vertices.push_back(Vertex(Vector3( hw,  hh, -hd), Vector2(1, 0), Vector3(1, 0, 0), color));
    vertices.push_back(Vertex(Vector3( hw,  hh,  hd), Vector2(0, 0), Vector3(1, 0, 0), color));
    
    // 左面 (-X)
    vertices.push_back(Vertex(Vector3(-hw, -hh, -hd), Vector2(0, 1), Vector3(-1, 0, 0), color));
    vertices.push_back(Vertex(Vector3(-hw, -hh,  hd), Vector2(1, 1), Vector3(-1, 0, 0), color));
    vertices.push_back(Vertex(Vector3(-hw,  hh,  hd), Vector2(1, 0), Vector3(-1, 0, 0), color));
    vertices.push_back(Vertex(Vector3(-hw,  hh, -hd), Vector2(0, 0), Vector3(-1, 0, 0), color));
    
    // 上面 (+Y)
    vertices.push_back(Vertex(Vector3(-hw,  hh,  hd), Vector2(0, 1), Vector3(0, 1, 0), color));
    vertices.push_back(Vertex(Vector3( hw,  hh,  hd), Vector2(1, 1), Vector3(0, 1, 0), color));
    vertices.push_back(Vertex(Vector3( hw,  hh, -hd), Vector2(1, 0), Vector3(0, 1, 0), color));
    vertices.push_back(Vertex(Vector3(-hw,  hh, -hd), Vector2(0, 0), Vector3(0, 1, 0), color));
    
    // 下面 (-Y)
    vertices.push_back(Vertex(Vector3(-hw, -hh, -hd), Vector2(0, 1), Vector3(0, -1, 0), color));
    vertices.push_back(Vertex(Vector3( hw, -hh, -hd), Vector2(1, 1), Vector3(0, -1, 0), color));
    vertices.push_back(Vertex(Vector3( hw, -hh,  hd), Vector2(1, 0), Vector3(0, -1, 0), color));
    vertices.push_back(Vertex(Vector3(-hw, -hh,  hd), Vector2(0, 0), Vector3(0, -1, 0), color));
    
    // 索引（每个面 2 个三角形）
    for (uint32_t i = 0; i < 6; ++i) {
        uint32_t base = i * 4;
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }
    
    auto mesh = CreateRef<Mesh>(vertices, indices);
    mesh->Upload();
    
    Logger::GetInstance().Info("Created cube mesh: " + std::to_string(vertices.size()) + " vertices");
    return mesh;
}

Ref<Mesh> MeshLoader::CreateSphere(float radius, uint32_t segments, uint32_t rings, const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    segments = std::max(3u, segments);
    rings = std::max(2u, rings);
    
    // 生成顶点
    for (uint32_t ring = 0; ring <= rings; ++ring) {
        float v = static_cast<float>(ring) / rings;
        float phi = v * static_cast<float>(M_PI);  // 0 到 π
        
        for (uint32_t seg = 0; seg <= segments; ++seg) {
            float u = static_cast<float>(seg) / segments;
            float theta = u * 2.0f * static_cast<float>(M_PI);  // 0 到 2π
            
            // 球面坐标转换为笛卡尔坐标
            float x = radius * std::sin(phi) * std::cos(theta);
            float y = radius * std::cos(phi);
            float z = radius * std::sin(phi) * std::sin(theta);
            
            Vertex vertex;
            vertex.position = Vector3(x, y, z);
            vertex.texCoord = Vector2(u, v);
            vertex.normal = Vector3(x, y, z).normalized();  // 球体法线即为归一化位置向量
            vertex.color = color;
            
            vertices.push_back(vertex);
        }
    }
    
    // 生成索引（逆时针为正面，从外向内看）
    for (uint32_t ring = 0; ring < rings; ++ring) {
        for (uint32_t seg = 0; seg < segments; ++seg) {
            uint32_t i0 = ring * (segments + 1) + seg;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + segments + 1;
            uint32_t i3 = i2 + 1;
            
            // 第一个三角形
            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i2);
            
            // 第二个三角形
            indices.push_back(i2);
            indices.push_back(i1);
            indices.push_back(i3);
        }
    }
    
    auto mesh = CreateRef<Mesh>(vertices, indices);
    mesh->Upload();
    
    Logger::GetInstance().Info("Created sphere mesh: " + std::to_string(vertices.size()) + " vertices");
    return mesh;
}

Ref<Mesh> MeshLoader::CreateCylinder(float radiusTop, float radiusBottom, float height, 
                                      uint32_t segments, const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    segments = std::max(3u, segments);
    float halfHeight = height * 0.5f;
    
    // ========== 侧面 ==========
    // 为侧面生成顶点（每个高度层一圈顶点）
    for (uint32_t i = 0; i <= segments; ++i) {
        float u = static_cast<float>(i) / segments;
        float theta = u * 2.0f * static_cast<float>(M_PI);
        float cosTheta = std::cos(theta);
        float sinTheta = std::sin(theta);
        
        // 顶部顶点
        Vertex topVertex;
        topVertex.position = Vector3(radiusTop * cosTheta, halfHeight, radiusTop * sinTheta);
        topVertex.texCoord = Vector2(u, 0);
        topVertex.normal = Vector3(cosTheta, 0, sinTheta);
        topVertex.color = color;
        vertices.push_back(topVertex);
        
        // 底部顶点
        Vertex bottomVertex;
        bottomVertex.position = Vector3(radiusBottom * cosTheta, -halfHeight, radiusBottom * sinTheta);
        bottomVertex.texCoord = Vector2(u, 1);
        bottomVertex.normal = Vector3(cosTheta, 0, sinTheta);
        bottomVertex.color = color;
        vertices.push_back(bottomVertex);
    }
    
    // 侧面索引（从外部看，逆时针为正面）
    // 顶点布局：i0=top, i1=bottom, i2=next_top, i3=next_bottom
    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t i0 = i * 2;        // 当前圆周的顶部
        uint32_t i1 = i0 + 1;       // 当前圆周的底部
        uint32_t i2 = i0 + 2;       // 下一个圆周的顶部
        uint32_t i3 = i2 + 1;       // 下一个圆周的底部
        
        // 从外部看这个四边形，逆时针应该是：i0 → i2 → i1
        indices.push_back(i0);
        indices.push_back(i2);
        indices.push_back(i1);
        
        // 第二个三角形：i1 → i2 → i3
        indices.push_back(i1);
        indices.push_back(i2);
        indices.push_back(i3);
    }
    
    // ========== 顶面盖子 ==========
    uint32_t topCapStart = vertices.size();
    
    // 顶面中心点
    vertices.push_back(Vertex(Vector3(0, halfHeight, 0), Vector2(0.5f, 0.5f), Vector3(0, 1, 0), color));
    uint32_t topCenter = topCapStart;
    
    // 顶面圆周顶点
    for (uint32_t i = 0; i <= segments; ++i) {
        float u = static_cast<float>(i) / segments;
        float theta = u * 2.0f * static_cast<float>(M_PI);
        float cosTheta = std::cos(theta);
        float sinTheta = std::sin(theta);
        
        Vertex v;
        v.position = Vector3(radiusTop * cosTheta, halfHeight, radiusTop * sinTheta);
        v.texCoord = Vector2(cosTheta * 0.5f + 0.5f, sinTheta * 0.5f + 0.5f);
        v.normal = Vector3(0, 1, 0);
        v.color = color;
        vertices.push_back(v);
    }
    
    // 顶面索引（从上方看，顺时针，正面朝上）
    for (uint32_t i = 0; i < segments; ++i) {
        indices.push_back(topCenter);
        indices.push_back(topCapStart + 1 + i + 1);
        indices.push_back(topCapStart + 1 + i);
    }
    
    // ========== 底面盖子 ==========
    uint32_t bottomCapStart = vertices.size();
    
    // 底面中心点
    vertices.push_back(Vertex(Vector3(0, -halfHeight, 0), Vector2(0.5f, 0.5f), Vector3(0, -1, 0), color));
    uint32_t bottomCenter = bottomCapStart;
    
    // 底面圆周顶点
    for (uint32_t i = 0; i <= segments; ++i) {
        float u = static_cast<float>(i) / segments;
        float theta = u * 2.0f * static_cast<float>(M_PI);
        float cosTheta = std::cos(theta);
        float sinTheta = std::sin(theta);
        
        Vertex v;
        v.position = Vector3(radiusBottom * cosTheta, -halfHeight, radiusBottom * sinTheta);
        v.texCoord = Vector2(cosTheta * 0.5f + 0.5f, sinTheta * 0.5f + 0.5f);
        v.normal = Vector3(0, -1, 0);
        v.color = color;
        vertices.push_back(v);
    }
    
    // 底面索引（从下方看，顺时针，正面朝下；从上方看是逆时针）
    for (uint32_t i = 0; i < segments; ++i) {
        indices.push_back(bottomCenter);
        indices.push_back(bottomCapStart + 1 + i);
        indices.push_back(bottomCapStart + 1 + i + 1);
    }
    
    auto mesh = CreateRef<Mesh>(vertices, indices);
    mesh->Upload();
    
    Logger::GetInstance().Info("Created cylinder mesh: " + std::to_string(vertices.size()) + " vertices");
    return mesh;
}

Ref<Mesh> MeshLoader::CreateCone(float radius, float height, uint32_t segments, const Color& color) {
    // 圆锥就是顶部半径为 0 的圆柱
    return CreateCylinder(0.0f, radius, height, segments, color);
}

Ref<Mesh> MeshLoader::CreateTorus(float majorRadius, float minorRadius, 
                                   uint32_t majorSegments, uint32_t minorSegments,
                                   const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    majorSegments = std::max(3u, majorSegments);
    minorSegments = std::max(3u, minorSegments);
    
    // 生成顶点
    for (uint32_t i = 0; i <= majorSegments; ++i) {
        float u = static_cast<float>(i) / majorSegments;
        float theta = u * 2.0f * static_cast<float>(M_PI);
        float cosTheta = std::cos(theta);
        float sinTheta = std::sin(theta);
        
        for (uint32_t j = 0; j <= minorSegments; ++j) {
            float v = static_cast<float>(j) / minorSegments;
            float phi = v * 2.0f * static_cast<float>(M_PI);
            float cosPhi = std::cos(phi);
            float sinPhi = std::sin(phi);
            
            // 计算位置
            float x = (majorRadius + minorRadius * cosPhi) * cosTheta;
            float y = minorRadius * sinPhi;
            float z = (majorRadius + minorRadius * cosPhi) * sinTheta;
            
            // 计算法线
            Vector3 center(majorRadius * cosTheta, 0, majorRadius * sinTheta);
            Vector3 pos(x, y, z);
            Vector3 normal = (pos - center).normalized();
            
            Vertex vertex;
            vertex.position = pos;
            vertex.texCoord = Vector2(u, v);
            vertex.normal = normal;
            vertex.color = color;
            
            vertices.push_back(vertex);
        }
    }
    
    // 生成索引（逆时针为正面）
    for (uint32_t i = 0; i < majorSegments; ++i) {
        for (uint32_t j = 0; j < minorSegments; ++j) {
            uint32_t i0 = i * (minorSegments + 1) + j;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + minorSegments + 1;
            uint32_t i3 = i2 + 1;
            
            // 第一个三角形
            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i2);
            
            // 第二个三角形
            indices.push_back(i2);
            indices.push_back(i1);
            indices.push_back(i3);
        }
    }
    
    auto mesh = CreateRef<Mesh>(vertices, indices);
    mesh->Upload();
    
    Logger::GetInstance().Info("Created torus mesh: " + std::to_string(vertices.size()) + " vertices");
    return mesh;
}

Ref<Mesh> MeshLoader::CreateCapsule(float radius, float height, uint32_t segments, 
                                     uint32_t rings, const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    segments = std::max(3u, segments);
    rings = std::max(1u, rings);
    
    float halfHeight = height * 0.5f;
    
    // 顶部半球（从赤道到顶点）
    for (uint32_t ring = 0; ring <= rings; ++ring) {
        float v = static_cast<float>(ring) / rings;
        float phi = v * static_cast<float>(M_PI) * 0.5f;  // 0 到 π/2
        
        for (uint32_t seg = 0; seg <= segments; ++seg) {
            float u = static_cast<float>(seg) / segments;
            float theta = u * 2.0f * static_cast<float>(M_PI);
            
            float x = radius * std::cos(phi) * std::cos(theta);
            float y = radius * std::sin(phi) + halfHeight;
            float z = radius * std::cos(phi) * std::sin(theta);
            
            Vertex vertex;
            vertex.position = Vector3(x, y, z);
            vertex.texCoord = Vector2(u, v * 0.25f);
            vertex.normal = Vector3(x, y - halfHeight, z).normalized();
            vertex.color = color;
            
            vertices.push_back(vertex);
        }
    }
    
    // 中间圆柱部分（连接两个半球）
    uint32_t cylinderStart = vertices.size();
    for (uint32_t seg = 0; seg <= segments; ++seg) {
        float u = static_cast<float>(seg) / segments;
        float theta = u * 2.0f * static_cast<float>(M_PI);
        float cosTheta = std::cos(theta);
        float sinTheta = std::sin(theta);
        
        // 顶部圆周（在 +halfHeight）
        Vertex topVertex;
        topVertex.position = Vector3(radius * cosTheta, halfHeight, radius * sinTheta);
        topVertex.texCoord = Vector2(u, 0.25f);
        topVertex.normal = Vector3(cosTheta, 0, sinTheta);
        topVertex.color = color;
        vertices.push_back(topVertex);
        
        // 底部圆周（在 -halfHeight）
        Vertex bottomVertex;
        bottomVertex.position = Vector3(radius * cosTheta, -halfHeight, radius * sinTheta);
        bottomVertex.texCoord = Vector2(u, 0.75f);
        bottomVertex.normal = Vector3(cosTheta, 0, sinTheta);
        bottomVertex.color = color;
        vertices.push_back(bottomVertex);
    }
    
    // 底部半球（从赤道到底部顶点）
    uint32_t bottomStart = vertices.size();
    for (uint32_t ring = 0; ring <= rings; ++ring) {
        float v = static_cast<float>(ring) / rings;
        float phi = v * static_cast<float>(M_PI) * 0.5f;  // 0 到 π/2
        
        for (uint32_t seg = 0; seg <= segments; ++seg) {
            float u = static_cast<float>(seg) / segments;
            float theta = u * 2.0f * static_cast<float>(M_PI);
            
            float x = radius * std::cos(phi) * std::cos(theta);
            float y = -radius * std::sin(phi) - halfHeight;
            float z = radius * std::cos(phi) * std::sin(theta);
            
            Vertex vertex;
            vertex.position = Vector3(x, y, z);
            vertex.texCoord = Vector2(u, 0.75f + v * 0.25f);
            vertex.normal = Vector3(x, y + halfHeight, z).normalized();
            vertex.color = color;
            
            vertices.push_back(vertex);
        }
    }
    
    // 顶部半球索引（逆时针为正面，从外向内看）
    for (uint32_t ring = 0; ring < rings; ++ring) {
        for (uint32_t seg = 0; seg < segments; ++seg) {
            uint32_t i0 = ring * (segments + 1) + seg;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + segments + 1;
            uint32_t i3 = i2 + 1;
            
            // 第一个三角形（反转）
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);
            
            // 第二个三角形（反转）
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }
    
    // 中间圆柱部分索引（与圆柱侧面相同的卷绕顺序）
    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t i0 = cylinderStart + i * 2;       // 当前顶部
        uint32_t i1 = i0 + 1;                      // 当前底部
        uint32_t i2 = i0 + 2;                      // 下一个顶部
        uint32_t i3 = i2 + 1;                      // 下一个底部
        
        // 第一个三角形（反转）
        indices.push_back(i0);
        indices.push_back(i2);
        indices.push_back(i1);
        
        // 第二个三角形（反转）
        indices.push_back(i1);
        indices.push_back(i2);
        indices.push_back(i3);
    }
    
    // 底部半球索引（逆时针为正面）
    for (uint32_t ring = 0; ring < rings; ++ring) {
        for (uint32_t seg = 0; seg < segments; ++seg) {
            uint32_t i0 = bottomStart + ring * (segments + 1) + seg;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + segments + 1;
            uint32_t i3 = i2 + 1;
            
            // 第一个三角形
            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i2);
            
            // 第二个三角形
            indices.push_back(i2);
            indices.push_back(i1);
            indices.push_back(i3);
        }
    }
    
    auto mesh = CreateRef<Mesh>(vertices, indices);
    mesh->Upload();
    
    Logger::GetInstance().Info("Created capsule mesh: " + std::to_string(vertices.size()) + " vertices");
    return mesh;
}

Ref<Mesh> MeshLoader::CreateQuad(float width, float height, const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    float hw = width * 0.5f;
    float hh = height * 0.5f;
    
    // 四个顶点（XY 平面，法线向 +Z）
    vertices.push_back(Vertex(Vector3(-hw, -hh, 0), Vector2(0, 1), Vector3(0, 0, 1), color));
    vertices.push_back(Vertex(Vector3( hw, -hh, 0), Vector2(1, 1), Vector3(0, 0, 1), color));
    vertices.push_back(Vertex(Vector3( hw,  hh, 0), Vector2(1, 0), Vector3(0, 0, 1), color));
    vertices.push_back(Vertex(Vector3(-hw,  hh, 0), Vector2(0, 0), Vector3(0, 0, 1), color));
    
    // 两个三角形
    indices = { 0, 1, 2, 0, 2, 3 };
    
    auto mesh = CreateRef<Mesh>(vertices, indices);
    mesh->Upload();
    
    Logger::GetInstance().Info("Created quad mesh: 4 vertices");
    return mesh;
}

Ref<Mesh> MeshLoader::CreateTriangle(float size, const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    float h = size * 0.866f;  // sqrt(3) / 2
    float halfSize = size * 0.5f;
    
    // 等边三角形（XY 平面，法线向 +Z）
    vertices.push_back(Vertex(Vector3(0, h * 0.5f, 0), Vector2(0.5f, 0), Vector3(0, 0, 1), color));
    vertices.push_back(Vertex(Vector3(-halfSize, -h * 0.5f, 0), Vector2(0, 1), Vector3(0, 0, 1), color));
    vertices.push_back(Vertex(Vector3(halfSize, -h * 0.5f, 0), Vector2(1, 1), Vector3(0, 0, 1), color));
    
    indices = { 0, 1, 2 };
    
    auto mesh = CreateRef<Mesh>(vertices, indices);
    mesh->Upload();
    
    Logger::GetInstance().Info("Created triangle mesh: 3 vertices");
    return mesh;
}

Ref<Mesh> MeshLoader::CreateCircle(float radius, uint32_t segments, const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    segments = std::max(3u, segments);
    
    // 中心点
    vertices.push_back(Vertex(Vector3(0, 0, 0), Vector2(0.5f, 0.5f), Vector3(0, 0, 1), color));
    
    // 圆周顶点
    for (uint32_t i = 0; i <= segments; ++i) {
        float u = static_cast<float>(i) / segments;
        float theta = u * 2.0f * static_cast<float>(M_PI);
        float x = radius * std::cos(theta);
        float y = radius * std::sin(theta);
        
        Vertex vertex;
        vertex.position = Vector3(x, y, 0);
        vertex.texCoord = Vector2(x / radius * 0.5f + 0.5f, y / radius * 0.5f + 0.5f);
        vertex.normal = Vector3(0, 0, 1);
        vertex.color = color;
        
        vertices.push_back(vertex);
    }
    
    // 生成三角形扇形索引
    for (uint32_t i = 0; i < segments; ++i) {
        indices.push_back(0);
        indices.push_back(i + 1);
        indices.push_back(i + 2);
    }
    
    auto mesh = CreateRef<Mesh>(vertices, indices);
    mesh->Upload();
    
    Logger::GetInstance().Info("Created circle mesh: " + std::to_string(vertices.size()) + " vertices");
    return mesh;
}

} // namespace Render

