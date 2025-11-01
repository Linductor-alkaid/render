#include "render/mesh.h"
#include "render/logger.h"
#include "render/error.h"
#include "render/gl_thread_checker.h"
#include <algorithm>
#include <unordered_map>

namespace Render {

// ============================================================================
// Mesh 实现
// ============================================================================

Mesh::Mesh()
    : m_VAO(0)
    , m_VBO(0)
    , m_EBO(0)
    , m_Uploaded(false)
{
}

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
    : m_Vertices(vertices)
    , m_Indices(indices)
    , m_VAO(0)
    , m_VBO(0)
    , m_EBO(0)
    , m_Uploaded(false)
{
}

Mesh::~Mesh() {
    Clear();
}

Mesh::Mesh(Mesh&& other) noexcept {
    std::lock_guard<std::mutex> lock(other.m_Mutex);
    
    m_Vertices = std::move(other.m_Vertices);
    m_Indices = std::move(other.m_Indices);
    m_VAO = other.m_VAO;
    m_VBO = other.m_VBO;
    m_EBO = other.m_EBO;
    m_Uploaded = other.m_Uploaded;
    
    other.m_VAO = 0;
    other.m_VBO = 0;
    other.m_EBO = 0;
    other.m_Uploaded = false;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        // 使用 scoped_lock 同时锁定两个互斥锁，避免死锁
        std::scoped_lock lock(m_Mutex, other.m_Mutex);
        
        // 释放当前网格资源（内部实现，已持有锁）
        if (m_VAO != 0) {
            GL_THREAD_CHECK();
            glDeleteVertexArrays(1, &m_VAO);
            m_VAO = 0;
        }
        if (m_VBO != 0) {
            GL_THREAD_CHECK();
            glDeleteBuffers(1, &m_VBO);
            m_VBO = 0;
        }
        if (m_EBO != 0) {
            GL_THREAD_CHECK();
            glDeleteBuffers(1, &m_EBO);
            m_EBO = 0;
        }
        
        m_Vertices = std::move(other.m_Vertices);
        m_Indices = std::move(other.m_Indices);
        m_VAO = other.m_VAO;
        m_VBO = other.m_VBO;
        m_EBO = other.m_EBO;
        m_Uploaded = other.m_Uploaded;
        
        other.m_VAO = 0;
        other.m_VBO = 0;
        other.m_EBO = 0;
        other.m_Uploaded = false;
    }
    return *this;
}

void Mesh::SetVertices(const std::vector<Vertex>& vertices) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Vertices = vertices;
    m_Uploaded = false;  // 需要重新上传
}

void Mesh::SetIndices(const std::vector<uint32_t>& indices) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Indices = indices;
    m_Uploaded = false;  // 需要重新上传
}

void Mesh::SetData(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Vertices = vertices;
    m_Indices = indices;
    m_Uploaded = false;  // 需要重新上传
}

void Mesh::UpdateVertices(const std::vector<Vertex>& vertices, size_t offset) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    
    // 检查网格是否已上传
    if (!m_Uploaded) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidState, 
                                   "Mesh::UpdateVertices: Mesh not uploaded yet"));
        return;
    }
    
    // 检查输入数据是否为空
    if (vertices.empty()) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument, 
                                   "Mesh::UpdateVertices: Empty vertex data provided"));
        return;
    }
    
    // 检查 offset 是否越界
    if (offset >= m_Vertices.size()) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::OutOfRange, 
                                 "Mesh::UpdateVertices: Offset " + std::to_string(offset) + 
                                 " exceeds vertex count " + std::to_string(m_Vertices.size())));
        return;
    }
    
    // 检查 offset + size 是否越界
    if (offset + vertices.size() > m_Vertices.size()) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::OutOfRange, 
                                 "Mesh::UpdateVertices: Offset " + std::to_string(offset) + 
                                 " + size " + std::to_string(vertices.size()) + 
                                 " exceeds vertex count " + std::to_string(m_Vertices.size())));
        return;
    }
    
    // 更新 CPU 端数据
    std::copy(vertices.begin(), vertices.end(), m_Vertices.begin() + offset);
    
    // 更新 GPU 端数据
    GL_THREAD_CHECK();
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 
                    offset * sizeof(Vertex), 
                    vertices.size() * sizeof(Vertex), 
                    vertices.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Mesh::Upload() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    
    if (m_Vertices.empty()) {
        Logger::GetInstance().Warning("Mesh::Upload - No vertices to upload");
        return;
    }
    
    // 如果已经上传过，先清理旧资源（内部实现，已持有锁）
    if (m_Uploaded) {
        if (m_VAO != 0) {
            GL_THREAD_CHECK();
            glDeleteVertexArrays(1, &m_VAO);
            m_VAO = 0;
        }
        if (m_VBO != 0) {
            GL_THREAD_CHECK();
            glDeleteBuffers(1, &m_VBO);
            m_VBO = 0;
        }
        if (m_EBO != 0) {
            GL_THREAD_CHECK();
            glDeleteBuffers(1, &m_EBO);
            m_EBO = 0;
        }
        m_Uploaded = false;
    }
    
    // 创建 VAO
    GL_THREAD_CHECK();
    glGenVertexArrays(1, &m_VAO);
    glBindVertexArray(m_VAO);
    
    // 创建并填充 VBO
    glGenBuffers(1, &m_VBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, 
                 m_Vertices.size() * sizeof(Vertex), 
                 m_Vertices.data(), 
                 GL_STATIC_DRAW);
    
    // 创建并填充 EBO（如果有索引）
    if (!m_Indices.empty()) {
        glGenBuffers(1, &m_EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, 
                     m_Indices.size() * sizeof(uint32_t), 
                     m_Indices.data(), 
                     GL_STATIC_DRAW);
    }
    
    // 设置顶点属性
    SetupVertexAttributes();
    
    // 解绑
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    
    m_Uploaded = true;
    
    Logger::GetInstance().Info("Mesh uploaded: " + std::to_string(m_Vertices.size()) + " vertices, " + 
                               std::to_string(m_Indices.size()) + " indices");
}

void Mesh::Draw(DrawMode mode) const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    
    if (!m_Uploaded) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidState, 
                                   "Mesh::Draw: 网格数据尚未上传到 GPU"));
        return;
    }
    
    if (m_Vertices.empty()) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidState, 
                                   "Mesh::Draw: 网格顶点数据为空"));
        return;
    }
    
    GL_THREAD_CHECK();
    glBindVertexArray(m_VAO);
    
    GLenum glMode = ConvertDrawMode(mode);
    
    if (!m_Indices.empty()) {
        // 使用索引绘制
        glDrawElements(glMode, static_cast<GLsizei>(m_Indices.size()), GL_UNSIGNED_INT, 0);
    } else {
        // 直接绘制顶点
        glDrawArrays(glMode, 0, static_cast<GLsizei>(m_Vertices.size()));
    }
    
    glBindVertexArray(0);
}

void Mesh::DrawInstanced(uint32_t instanceCount, DrawMode mode) const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    
    if (!m_Uploaded) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidState, 
                                   "Mesh::DrawInstanced: 网格数据尚未上传到 GPU"));
        return;
    }
    
    if (instanceCount == 0) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument, 
                                   "Mesh::DrawInstanced: 实例数量为 0"));
        return;
    }
    
    GL_THREAD_CHECK();
    glBindVertexArray(m_VAO);
    
    GLenum glMode = ConvertDrawMode(mode);
    
    if (!m_Indices.empty()) {
        // 使用索引绘制实例
        glDrawElementsInstanced(glMode, static_cast<GLsizei>(m_Indices.size()), 
                                GL_UNSIGNED_INT, 0, instanceCount);
    } else {
        // 直接绘制顶点实例
        glDrawArraysInstanced(glMode, 0, static_cast<GLsizei>(m_Vertices.size()), instanceCount);
    }
    
    glBindVertexArray(0);
}

void Mesh::Clear() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    
    if (m_VAO != 0) {
        GL_THREAD_CHECK();
        glDeleteVertexArrays(1, &m_VAO);
        m_VAO = 0;
    }
    
    if (m_VBO != 0) {
        GL_THREAD_CHECK();
        glDeleteBuffers(1, &m_VBO);
        m_VBO = 0;
    }
    
    if (m_EBO != 0) {
        GL_THREAD_CHECK();
        glDeleteBuffers(1, &m_EBO);
        m_EBO = 0;
    }
    
    m_Uploaded = false;
}

AABB Mesh::CalculateBounds() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    
    // 返回空的包围盒并记录警告
    if (m_Vertices.empty()) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidState, 
                                   "Mesh::CalculateBounds: Mesh has no vertices"));
        return AABB();
    }
    
    // 在同一个临界区内完成所有操作
    AABB bounds;
    bounds.min = m_Vertices[0].position;
    bounds.max = m_Vertices[0].position;
    
    // 从索引 1 开始遍历，避免重复处理第一个顶点
    for (size_t i = 1; i < m_Vertices.size(); ++i) {
        const auto& pos = m_Vertices[i].position;
        bounds.min = bounds.min.cwiseMin(pos);
        bounds.max = bounds.max.cwiseMax(pos);
    }
    
    return bounds;
}

void Mesh::RecalculateNormals() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    
    if (m_Indices.size() < 3) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidState, 
                                   "Mesh::RecalculateNormals: Not enough indices for triangles"));
        return;
    }
    
    // 先将所有法线清零
    for (auto& vertex : m_Vertices) {
        vertex.normal = Vector3::Zero();
    }
    
    // 统计无效三角形数量
    size_t invalidTriangles = 0;
    
    // 遍历每个三角形，计算面法线并累加到顶点
    for (size_t i = 0; i < m_Indices.size(); i += 3) {
        uint32_t i0 = m_Indices[i];
        uint32_t i1 = m_Indices[i + 1];
        uint32_t i2 = m_Indices[i + 2];
        
        // 添加越界检查和警告
        if (i0 >= m_Vertices.size() || i1 >= m_Vertices.size() || i2 >= m_Vertices.size()) {
            if (invalidTriangles == 0) {
                // 只在首次发现无效三角形时记录详细信息
                HANDLE_ERROR(RENDER_WARNING(ErrorCode::OutOfRange, 
                                           "Mesh::RecalculateNormals: Invalid triangle indices at offset " + 
                                           std::to_string(i) + " [" + std::to_string(i0) + ", " + 
                                           std::to_string(i1) + ", " + std::to_string(i2) + "], " +
                                           "vertex count: " + std::to_string(m_Vertices.size())));
            }
            invalidTriangles++;
            continue;
        }
        
        Vector3& p0 = m_Vertices[i0].position;
        Vector3& p1 = m_Vertices[i1].position;
        Vector3& p2 = m_Vertices[i2].position;
        
        // 计算边向量
        Vector3 edge1 = p1 - p0;
        Vector3 edge2 = p2 - p0;
        
        // 计算面法线
        Vector3 normal = edge1.cross(edge2);
        
        // 累加到各顶点（面积加权）
        m_Vertices[i0].normal += normal;
        m_Vertices[i1].normal += normal;
        m_Vertices[i2].normal += normal;
    }
    
    // 如果有多个无效三角形，报告总数
    if (invalidTriangles > 1) {
        Logger::GetInstance().Warning("Mesh::RecalculateNormals: Skipped " + 
                                      std::to_string(invalidTriangles) + " invalid triangles");
    }
    
    // 归一化所有法线
    for (auto& vertex : m_Vertices) {
        float length = vertex.normal.norm();
        if (length > 1e-6f) {
            vertex.normal.normalize();
        } else {
            vertex.normal = Vector3::UnitY();  // 默认向上
        }
    }
    
    // 如果已上传，需要更新 GPU 数据
    if (m_Uploaded) {
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, m_Vertices.size() * sizeof(Vertex), m_Vertices.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    
    Logger::GetInstance().Info("Mesh normals recalculated");
}

void Mesh::RecalculateTangents() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    // TODO: 实现切线计算（用于法线贴图）
    Logger::GetInstance().Warning("Mesh::RecalculateTangents - Not implemented yet");
}

void Mesh::SetupVertexAttributes() {
    // 顶点属性布局：
    // Location 0: Position (vec3) - 12 bytes
    // Location 1: TexCoord (vec2) - 8 bytes
    // Location 2: Normal (vec3) - 12 bytes
    // Location 3: Color (vec4) - 16 bytes
    // Total: 48 bytes per vertex
    
    // Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 
                          (void*)offsetof(Vertex, position));
    
    // TexCoord
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), 
                          (void*)offsetof(Vertex, texCoord));
    
    // Normal
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 
                          (void*)offsetof(Vertex, normal));
    
    // Color
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), 
                          (void*)offsetof(Vertex, color));
}

GLenum Mesh::ConvertDrawMode(DrawMode mode) const {
    switch (mode) {
        case DrawMode::Triangles:       return GL_TRIANGLES;
        case DrawMode::TriangleStrip:   return GL_TRIANGLE_STRIP;
        case DrawMode::TriangleFan:     return GL_TRIANGLE_FAN;
        case DrawMode::Lines:           return GL_LINES;
        case DrawMode::LineStrip:       return GL_LINE_STRIP;
        case DrawMode::LineLoop:        return GL_LINE_LOOP;
        case DrawMode::Points:          return GL_POINTS;
        default:                        return GL_TRIANGLES;
    }
}

size_t Mesh::GetMemoryUsage() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    
    // 顶点数据内存
    size_t vertexMemory = m_Vertices.size() * sizeof(Vertex);
    
    // 索引数据内存
    size_t indexMemory = m_Indices.size() * sizeof(uint32_t);
    
    // 总内存 = 顶点内存 + 索引内存
    return vertexMemory + indexMemory;
}

} // namespace Render

