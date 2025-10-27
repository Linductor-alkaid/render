#pragma once

#include "types.h"
#include <glad/glad.h>
#include <vector>
#include <memory>

namespace Render {

/**
 * @brief 顶点数据结构
 * 
 * 包含完整的顶点属性：位置、纹理坐标、法线和颜色
 */
struct Vertex {
    Vector3 position;   // 位置
    Vector2 texCoord;   // 纹理坐标
    Vector3 normal;     // 法线
    Color color;        // 顶点颜色
    
    /**
     * @brief 默认构造函数
     */
    Vertex() 
        : position(Vector3::Zero())
        , texCoord(Vector2::Zero())
        , normal(Vector3::UnitY())
        , color(Color::White()) {}
    
    /**
     * @brief 构造函数（仅位置）
     */
    Vertex(const Vector3& pos) 
        : position(pos)
        , texCoord(Vector2::Zero())
        , normal(Vector3::UnitY())
        , color(Color::White()) {}
    
    /**
     * @brief 完整构造函数
     */
    Vertex(const Vector3& pos, const Vector2& uv, const Vector3& norm, const Color& col)
        : position(pos)
        , texCoord(uv)
        , normal(norm)
        , color(col) {}
};

/**
 * @brief 网格绘制模式
 */
enum class DrawMode {
    Triangles,      // 三角形列表
    TriangleStrip,  // 三角形带
    TriangleFan,    // 三角形扇
    Lines,          // 线段列表
    LineStrip,      // 线段带
    LineLoop,       // 线段环
    Points          // 点
};

/**
 * @brief 网格类
 * 
 * 管理顶点数据、索引数据和 OpenGL 缓冲区对象（VAO/VBO/EBO）
 * 提供便捷的网格创建、更新和渲染接口
 */
class Mesh {
public:
    /**
     * @brief 默认构造函数
     */
    Mesh();
    
    /**
     * @brief 构造函数（带数据）
     * @param vertices 顶点数据
     * @param indices 索引数据
     */
    Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    
    /**
     * @brief 析构函数
     */
    ~Mesh();
    
    // 禁用拷贝
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    
    // 允许移动
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;
    
    /**
     * @brief 设置顶点数据
     * @param vertices 顶点数组
     */
    void SetVertices(const std::vector<Vertex>& vertices);
    
    /**
     * @brief 设置索引数据
     * @param indices 索引数组
     */
    void SetIndices(const std::vector<uint32_t>& indices);
    
    /**
     * @brief 设置顶点和索引数据
     * @param vertices 顶点数组
     * @param indices 索引数组
     */
    void SetData(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    
    /**
     * @brief 更新顶点数据（部分更新）
     * @param vertices 新的顶点数据
     * @param offset 起始偏移量（顶点索引）
     */
    void UpdateVertices(const std::vector<Vertex>& vertices, size_t offset = 0);
    
    /**
     * @brief 创建 GPU 缓冲区（VAO/VBO/EBO）
     * 
     * 必须在设置数据后调用此方法才能渲染
     */
    void Upload();
    
    /**
     * @brief 绘制网格
     * @param mode 绘制模式（默认为三角形）
     */
    void Draw(DrawMode mode = DrawMode::Triangles) const;
    
    /**
     * @brief 绘制网格实例
     * @param instanceCount 实例数量
     * @param mode 绘制模式（默认为三角形）
     */
    void DrawInstanced(uint32_t instanceCount, DrawMode mode = DrawMode::Triangles) const;
    
    /**
     * @brief 释放 GPU 资源
     */
    void Clear();
    
    /**
     * @brief 获取顶点数据
     */
    const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
    
    /**
     * @brief 获取索引数据
     */
    const std::vector<uint32_t>& GetIndices() const { return m_Indices; }
    
    /**
     * @brief 获取顶点数量
     */
    size_t GetVertexCount() const { return m_Vertices.size(); }
    
    /**
     * @brief 获取索引数量
     */
    size_t GetIndexCount() const { return m_Indices.size(); }
    
    /**
     * @brief 获取三角形数量
     */
    size_t GetTriangleCount() const { return m_Indices.size() / 3; }
    
    /**
     * @brief 是否已上传到 GPU
     */
    bool IsUploaded() const { return m_VAO != 0; }
    
    /**
     * @brief 计算包围盒
     */
    AABB CalculateBounds() const;
    
    /**
     * @brief 重新计算法线（基于三角形）
     */
    void RecalculateNormals();
    
    /**
     * @brief 重新计算切线（用于法线贴图）
     * @note 目前版本暂不支持切线，预留接口
     */
    void RecalculateTangents();

private:
    /**
     * @brief 设置顶点属性指针
     */
    void SetupVertexAttributes();
    
    /**
     * @brief DrawMode 转换为 OpenGL 绘制模式
     */
    GLenum ConvertDrawMode(DrawMode mode) const;

private:
    std::vector<Vertex> m_Vertices;     // 顶点数据
    std::vector<uint32_t> m_Indices;    // 索引数据
    
    GLuint m_VAO;   // 顶点数组对象
    GLuint m_VBO;   // 顶点缓冲对象
    GLuint m_EBO;   // 元素缓冲对象（索引）
    
    bool m_Uploaded;    // 是否已上传到 GPU
};

} // namespace Render

