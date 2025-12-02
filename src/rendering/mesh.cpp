#include "render/mesh.h"
#include "render/logger.h"
#include "render/error.h"
#include "render/gl_thread_checker.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <thread>

namespace Render {

// ============================================================================
// Mesh 实现
// ============================================================================

Mesh::Mesh()
    : m_VAO(0)
    , m_VBO(0)
    , m_EBO(0)
    , m_Uploaded(false)
    , m_uploadState(UploadState::NotUploaded)
{
}

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
    : m_Vertices(vertices)
    , m_Indices(indices)
    , m_VAO(0)
    , m_VBO(0)
    , m_EBO(0)
    , m_Uploaded(false)
    , m_uploadState(UploadState::NotUploaded)
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
    m_uploadState.store(other.m_uploadState.load(std::memory_order_acquire), 
                        std::memory_order_release);
    
    other.m_VAO = 0;
    other.m_VBO = 0;
    other.m_EBO = 0;
    other.m_Uploaded = false;
    other.m_uploadState.store(UploadState::NotUploaded, std::memory_order_release);
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
        m_uploadState.store(other.m_uploadState.load(std::memory_order_acquire), 
                            std::memory_order_release);
        
        other.m_VAO = 0;
        other.m_VBO = 0;
        other.m_EBO = 0;
        other.m_Uploaded = false;
        other.m_uploadState.store(UploadState::NotUploaded, std::memory_order_release);
    }
    return *this;
}

void Mesh::SetVertices(const std::vector<Vertex>& vertices) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Vertices = vertices;
    m_Uploaded = false;  // 需要重新上传
    m_uploadState.store(UploadState::NotUploaded, std::memory_order_release);
}

void Mesh::SetIndices(const std::vector<uint32_t>& indices) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Indices = indices;
    m_Uploaded = false;  // 需要重新上传
    m_uploadState.store(UploadState::NotUploaded, std::memory_order_release);
}

void Mesh::SetData(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Vertices = vertices;
    m_Indices = indices;
    m_Uploaded = false;  // 需要重新上传
    m_uploadState.store(UploadState::NotUploaded, std::memory_order_release);
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
    // ✅ 两阶段上传优化：大幅减少锁持有时间
    // 阶段1: 持锁复制数据（微秒级）
    // 阶段2: 无锁OpenGL调用（毫秒级）
    // 阶段3: 持锁更新状态（微秒级）
    
    // === 阶段1：快速复制数据（持锁，微秒级）===
    std::vector<Vertex> vertices_copy;
    std::vector<uint32_t> indices_copy;
    bool need_reupload = false;
    
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        
        if (m_Vertices.empty()) {
            HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidState, 
                                       "Mesh::Upload: No vertices to upload"));
            m_uploadState.store(UploadState::Failed, std::memory_order_release);
            return;
        }
        
        // 检查当前状态
        UploadState currentState = m_uploadState.load(std::memory_order_acquire);
        if (currentState == UploadState::Uploaded && m_VAO != 0 && m_VBO != 0) {
            Logger::GetInstance().Debug("Mesh::Upload: 网格已上传，跳过 (VAO:" + 
                                       std::to_string(m_VAO) + ")");
            return;
        }
        
        if (currentState == UploadState::Uploading) {
            Logger::GetInstance().Warning("Mesh::Upload: 正在上传中，跳过");
            return;
        }
        
        // 标记为正在上传（其他线程会看到此状态并等待）
        m_uploadState.store(UploadState::Uploading, std::memory_order_release);
        
        // 快速复制数据
        vertices_copy = m_Vertices;
        indices_copy = m_Indices;
        need_reupload = m_Uploaded;
    }  // 🔓 锁释放！其他线程现在可以访问Mesh对象
    
    // === 阶段2：OpenGL调用（无锁，毫秒级）===
    GLuint vao = 0, vbo = 0, ebo = 0;
    
    try {
        // 如果需要重新上传，先清理旧资源
        if (need_reupload) {
            Logger::GetInstance().Info("Mesh::Upload: 重新上传");
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
        }
        
        // 创建VAO
        GL_THREAD_CHECK();
        glGenVertexArrays(1, &vao);
        if (vao == 0) {
            throw std::runtime_error("Failed to generate VAO");
        }
        glBindVertexArray(vao);
        
        // 创建并填充VBO
        glGenBuffers(1, &vbo);
        if (vbo == 0) {
            throw std::runtime_error("Failed to generate VBO");
        }
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, 
                     vertices_copy.size() * sizeof(Vertex), 
                     vertices_copy.data(), 
                     GL_STATIC_DRAW);
        
        // 创建并填充EBO
        if (!indices_copy.empty()) {
            glGenBuffers(1, &ebo);
            if (ebo == 0) {
                throw std::runtime_error("Failed to generate EBO");
            }
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, 
                         indices_copy.size() * sizeof(uint32_t), 
                         indices_copy.data(), 
                         GL_STATIC_DRAW);
        }
        
        // 设置顶点属性
        SetupVertexAttributes();
        
        // 解绑
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        
        // === 阶段3：更新状态（持锁，微秒级）===
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_VAO = vao;
            m_VBO = vbo;
            m_EBO = ebo;
            m_Uploaded = true;
        }
        
        // 标记上传完成（原子操作，无锁）
        m_uploadState.store(UploadState::Uploaded, std::memory_order_release);
        
        Logger::GetInstance().Debug("Mesh uploaded: " + std::to_string(vertices_copy.size()) + 
                                   " vertices, " + std::to_string(indices_copy.size()) + " indices");
                                   
    } catch (const std::exception& e) {
        // 异常处理
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::Unknown, 
                                 "Mesh::Upload: Exception - " + std::string(e.what())));
        
        // 清理资源（无锁）
        if (vao != 0) glDeleteVertexArrays(1, &vao);
        if (vbo != 0) glDeleteBuffers(1, &vbo);
        if (ebo != 0) glDeleteBuffers(1, &ebo);
        
        // 标记失败
        m_uploadState.store(UploadState::Failed, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Uploaded = false;
        }
        
    } catch (...) {
        // 捕获所有异常
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::Unknown, 
                                 "Mesh::Upload: Unknown exception"));
        
        // 清理资源
        if (vao != 0) glDeleteVertexArrays(1, &vao);
        if (vbo != 0) glDeleteBuffers(1, &vbo);
        if (ebo != 0) glDeleteBuffers(1, &ebo);
        
        // 标记失败
        m_uploadState.store(UploadState::Failed, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Uploaded = false;
        }
    }
}

void Mesh::Draw(DrawMode mode) const {
    // ✅ 优化：等待上传完成（如果正在上传中）
    UploadState state = m_uploadState.load(std::memory_order_acquire);
    if (state == UploadState::Uploading) {
        // 等待上传完成（带超时）
        int retries = 0;
        const int MAX_RETRIES = 1000;  // 约1秒超时（假设每次yield约1ms）
        
        while (state == UploadState::Uploading && retries < MAX_RETRIES) {
            std::this_thread::yield();  // 让出CPU给上传线程
            state = m_uploadState.load(std::memory_order_acquire);
            retries++;
        }
        
        if (retries >= MAX_RETRIES) {
            HANDLE_ERROR(RENDER_ERROR(ErrorCode::ThreadSynchronizationFailed, 
                                     "Mesh::Draw: 等待上传超时（1秒）"));
            return;
        }
        
        if (retries > 0) {
            Logger::GetInstance().Debug("Mesh::Draw: 等待上传完成 (重试次数: " + 
                                       std::to_string(retries) + ")");
        }
    }
    
    std::lock_guard<std::mutex> lock(m_Mutex);
    
    // ✅ 增强：更详细的状态检查
    if (!m_Uploaded) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidState, 
                                   "Mesh::Draw: 网格数据尚未上传到 GPU，请先调用 Upload()"));
        return;
    }
    
    if (m_VAO == 0) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::InvalidState, 
                                 "Mesh::Draw: VAO 无效 (m_Uploaded=true 但 VAO=0)"));
        return;
    }
    
    if (m_Vertices.empty()) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidState, 
                                   "Mesh::Draw: 网格顶点数据为空"));
        return;
    }
    
    GL_THREAD_CHECK();
    glBindVertexArray(m_VAO);
    
    // ✅ 确保禁用实例化属性（location 6-11），避免LOD渲染器修改VAO后影响普通渲染
    // LOD渲染器会在VAO上启用这些属性，但普通渲染不应该使用它们
    for (int i = 6; i <= 11; ++i) {
        glDisableVertexAttribArray(i);
    }
    
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
    // ✅ 优化：等待上传完成（如果正在上传中）
    UploadState state = m_uploadState.load(std::memory_order_acquire);
    if (state == UploadState::Uploading) {
        // 等待上传完成（带超时）
        int retries = 0;
        const int MAX_RETRIES = 1000;
        
        while (state == UploadState::Uploading && retries < MAX_RETRIES) {
            std::this_thread::yield();
            state = m_uploadState.load(std::memory_order_acquire);
            retries++;
        }
        
        if (retries >= MAX_RETRIES) {
            HANDLE_ERROR(RENDER_ERROR(ErrorCode::ThreadSynchronizationFailed, 
                                     "Mesh::DrawInstanced: 等待上传超时"));
            return;
        }
    }
    
    std::lock_guard<std::mutex> lock(m_Mutex);
    
    // ✅ 增强：更详细的状态检查
    if (!m_Uploaded) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidState, 
                                   "Mesh::DrawInstanced: 网格数据尚未上传到 GPU，请先调用 Upload()"));
        return;
    }
    
    if (m_VAO == 0) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::InvalidState, 
                                 "Mesh::DrawInstanced: VAO 无效"));
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

uint32_t Mesh::GetVertexArrayID() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_VAO;
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
    m_uploadState.store(UploadState::NotUploaded, std::memory_order_release);
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

    if (m_Vertices.empty()) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidState,
                                   "Mesh::RecalculateTangents: Mesh has no vertices"));
        return;
    }

    const float EPSILON = 1e-6f;

    // 清零当前切线/副切线
    for (auto& vertex : m_Vertices) {
        vertex.tangent = Vector3::Zero();
        vertex.bitangent = Vector3::Zero();
    }

    auto accumulateTriangle = [&](uint32_t i0, uint32_t i1, uint32_t i2) {
        if (i0 >= m_Vertices.size() || i1 >= m_Vertices.size() || i2 >= m_Vertices.size()) {
            return;
        }

        const Vector3& p0 = m_Vertices[i0].position;
        const Vector3& p1 = m_Vertices[i1].position;
        const Vector3& p2 = m_Vertices[i2].position;

        const Vector2& uv0 = m_Vertices[i0].texCoord;
        const Vector2& uv1 = m_Vertices[i1].texCoord;
        const Vector2& uv2 = m_Vertices[i2].texCoord;

        Vector3 edge1 = p1 - p0;
        Vector3 edge2 = p2 - p0;

        Vector2 deltaUV1 = uv1 - uv0;
        Vector2 deltaUV2 = uv2 - uv0;

        float determinant = deltaUV1.x() * deltaUV2.y() - deltaUV2.x() * deltaUV1.y();
        if (std::abs(determinant) < EPSILON) {
            return;
        }

        float r = 1.0f / determinant;
        Vector3 tangent = (edge1 * deltaUV2.y() - edge2 * deltaUV1.y()) * r;
        Vector3 bitangent = (edge2 * deltaUV1.x() - edge1 * deltaUV2.x()) * r;

        m_Vertices[i0].tangent += tangent;
        m_Vertices[i1].tangent += tangent;
        m_Vertices[i2].tangent += tangent;

        m_Vertices[i0].bitangent += bitangent;
        m_Vertices[i1].bitangent += bitangent;
        m_Vertices[i2].bitangent += bitangent;
    };

    if (!m_Indices.empty()) {
        for (size_t i = 0; i + 2 < m_Indices.size(); i += 3) {
            accumulateTriangle(m_Indices[i], m_Indices[i + 1], m_Indices[i + 2]);
        }
    } else {
        if (m_Vertices.size() % 3 != 0) {
            HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidState,
                                       "Mesh::RecalculateTangents: Non-indexed mesh vertex count not divisible by 3"));
        }

        for (size_t i = 0; i + 2 < m_Vertices.size(); i += 3) {
            accumulateTriangle(static_cast<uint32_t>(i),
                               static_cast<uint32_t>(i + 1),
                               static_cast<uint32_t>(i + 2));
        }
    }

    // 正交化并归一化
    for (auto& vertex : m_Vertices) {
        Vector3 normal = vertex.normal;
        if (normal.squaredNorm() < EPSILON) {
            normal = Vector3::UnitY();
        } else {
            normal.normalize();
        }

        Vector3 tangent = vertex.tangent;
        if (tangent.squaredNorm() < EPSILON) {
            tangent = Vector3::UnitX();
        }

        tangent = (tangent - normal * normal.dot(tangent));
        float tangentLength = tangent.norm();
        if (tangentLength < EPSILON) {
            tangent = Vector3::UnitX();
        } else {
            tangent /= tangentLength;
        }

        Vector3 bitangent = vertex.bitangent;
        float handedness = 1.0f;
        if (bitangent.squaredNorm() >= EPSILON) {
            handedness = (normal.cross(tangent).dot(bitangent) < 0.0f) ? -1.0f : 1.0f;
        }
        bitangent = normal.cross(tangent) * handedness;

        vertex.normal = normal;
        vertex.tangent = tangent;
        vertex.bitangent = bitangent;
    }

    if (m_Uploaded) {
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, m_Vertices.size() * sizeof(Vertex), m_Vertices.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    Logger::GetInstance().Info("Mesh tangents recalculated");
}

void Mesh::SetupVertexAttributes() {
    // 顶点属性布局：
    // Location 0: Position (vec3) - 12 bytes
    // Location 1: TexCoord (vec2) - 8 bytes
    // Location 2: Normal (vec3) - 12 bytes
    // Location 3: Color (vec4) - 16 bytes
    // Location 4: Tangent (vec3) - 12 bytes
    // Location 5: Bitangent (vec3) - 12 bytes
    // Total: 72 bytes per vertex
    
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
    
    // Tangent
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, tangent));
    
    // Bitangent
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, bitangent));
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

