#pragma once

#include "types.h"
#include <glad/glad.h>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

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
 * @brief 网格上传状态
 * 
 * 用于两阶段上传优化，减少锁竞争
 */
enum class UploadState {
    NotUploaded,    // 未上传
    Uploading,      // 正在上传（中间状态）
    Uploaded,       // 已上传
    Failed          // 上传失败
};

/**
 * @brief 网格类
 * 
 * 管理顶点数据、索引数据和 OpenGL 缓冲区对象（VAO/VBO/EBO）
 * 提供便捷的网格创建、更新和渲染接口
 * 
 * 线程安全：
 * - 所有公共方法都是线程安全的
 * - 使用互斥锁保护所有成员变量的访问
 * - 注意：OpenGL 调用必须在创建上下文的线程中执行
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
     * @brief 获取顶点数据（已弃用，请使用 AccessVertices 或 LockVertices）
     * @deprecated 返回副本较慢，推荐使用访问器方法
     */
    [[deprecated("Use AccessVertices() or LockVertices() instead")]]
    std::vector<Vertex> GetVertices() const { 
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Vertices;  // 返回副本以保证线程安全
    }
    
    /**
     * @brief 获取索引数据（已弃用，请使用 AccessIndices 或 LockIndices）
     * @deprecated 返回副本较慢，推荐使用访问器方法
     */
    [[deprecated("Use AccessIndices() or LockIndices() instead")]]
    std::vector<uint32_t> GetIndices() const { 
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Indices;  // 返回副本以保证线程安全
    }
    
    /**
     * @brief 通过回调访问顶点数据（方法1：推荐）
     * 
     * 使用回调方式在锁保护下访问顶点数据，避免数据竞争
     * 
     * @param func 回调函数，接受 const std::vector<Vertex>& 参数
     * 
     * @example
     * mesh->AccessVertices([](const std::vector<Vertex>& vertices) {
     *     for (const auto& v : vertices) {
     *         // 处理顶点数据
     *     }
     * });
     */
    template<typename Func>
    void AccessVertices(Func&& func) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        func(m_Vertices);
    }
    
    /**
     * @brief 通过回调访问索引数据（方法1：推荐）
     * 
     * 使用回调方式在锁保护下访问索引数据，避免数据竞争
     * 
     * @param func 回调函数，接受 const std::vector<uint32_t>& 参数
     * 
     * @example
     * mesh->AccessIndices([](const std::vector<uint32_t>& indices) {
     *     for (const auto& idx : indices) {
     *         // 处理索引数据
     *     }
     * });
     */
    template<typename Func>
    void AccessIndices(Func&& func) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        func(m_Indices);
    }
    
    /**
     * @brief RAII 顶点数据守卫（方法2）
     * 
     * 在对象生命周期内持有锁，允许安全访问顶点数据
     */
    class VertexGuard {
    public:
        VertexGuard(const Mesh& mesh) 
            : m_mesh(mesh), m_lock(mesh.m_Mutex) {}
        
        const std::vector<Vertex>& Get() const { 
            return m_mesh.m_Vertices; 
        }
        
    private:
        const Mesh& m_mesh;
        std::lock_guard<std::mutex> m_lock;
    };
    
    /**
     * @brief RAII 索引数据守卫（方法2）
     * 
     * 在对象生命周期内持有锁，允许安全访问索引数据
     */
    class IndexGuard {
    public:
        IndexGuard(const Mesh& mesh) 
            : m_mesh(mesh), m_lock(mesh.m_Mutex) {}
        
        const std::vector<uint32_t>& Get() const { 
            return m_mesh.m_Indices; 
        }
        
    private:
        const Mesh& m_mesh;
        std::lock_guard<std::mutex> m_lock;
    };
    
    /**
     * @brief 获取顶点数据的锁保护访问（方法2）
     * 
     * 返回 RAII 守卫对象，在其生命周期内持有锁
     * 
     * @return VertexGuard 守卫对象
     * 
     * @example
     * {
     *     auto guard = mesh->LockVertices();
     *     const auto& vertices = guard.Get();
     *     for (const auto& v : vertices) {
     *         // 处理顶点数据
     *     }
     * }  // 作用域结束，自动解锁
     */
    VertexGuard LockVertices() const {
        return VertexGuard(*this);
    }
    
    /**
     * @brief 获取索引数据的锁保护访问（方法2）
     * 
     * 返回 RAII 守卫对象，在其生命周期内持有锁
     * 
     * @return IndexGuard 守卫对象
     * 
     * @example
     * {
     *     auto guard = mesh->LockIndices();
     *     const auto& indices = guard.Get();
     *     for (const auto& idx : indices) {
     *         // 处理索引数据
     *     }
     * }  // 作用域结束，自动解锁
     */
    IndexGuard LockIndices() const {
        return IndexGuard(*this);
    }
    
    /**
     * @brief 获取顶点数量
     */
    size_t GetVertexCount() const { 
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Vertices.size(); 
    }
    
    /**
     * @brief 获取索引数量
     */
    size_t GetIndexCount() const { 
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Indices.size(); 
    }
    
    /**
     * @brief 获取三角形数量
     */
    size_t GetTriangleCount() const { 
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Indices.size() / 3; 
    }
    
    /**
     * @brief 是否已上传到 GPU
     */
    bool IsUploaded() const { 
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_VAO != 0; 
    }
    
    /**
     * @brief 获取上传状态（线程安全，无锁）
     * @return 当前上传状态
     */
    UploadState GetUploadState() const {
        return m_uploadState.load(std::memory_order_acquire);
    }
    
    /**
     * @brief 是否正在上传中（线程安全，无锁）
     * @return true 表示正在上传
     */
    bool IsUploading() const {
        return m_uploadState.load(std::memory_order_acquire) == UploadState::Uploading;
    }
    
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

    /**
     * @brief 获取网格内存使用量（字节）
     */
    size_t GetMemoryUsage() const;

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
    
    bool m_Uploaded;    // 是否已上传到 GPU（向后兼容）
    std::atomic<UploadState> m_uploadState;  // 上传状态（用于两阶段上传优化）
    
    mutable std::mutex m_Mutex;  // 互斥锁，保护所有成员变量
};

} // namespace Render

