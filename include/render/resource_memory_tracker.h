#pragma once

#include "render/types.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>

namespace Render {

// 前向声明
class Texture;
class Mesh;
class Shader;

/**
 * @brief 资源内存统计信息
 */
struct ResourceMemoryStats {
    size_t textureMemory = 0;      ///< 纹理占用内存（字节）
    size_t meshMemory = 0;         ///< 网格占用内存（字节）
    size_t shaderMemory = 0;       ///< 着色器占用内存（字节）
    size_t bufferMemory = 0;       ///< GPU 缓冲占用内存（字节）
    size_t totalMemory = 0;        ///< 总内存占用（字节）
    
    uint32_t textureCount = 0;     ///< 纹理数量
    uint32_t meshCount = 0;        ///< 网格数量
    uint32_t shaderCount = 0;      ///< 着色器数量
    uint32_t bufferCount = 0;      ///< GPU 缓冲数量
    
    void Reset() {
        textureMemory = 0;
        meshMemory = 0;
        shaderMemory = 0;
        bufferMemory = 0;
        totalMemory = 0;
        textureCount = 0;
        meshCount = 0;
        shaderCount = 0;
        bufferCount = 0;
    }
    
    void UpdateTotal() {
        totalMemory = textureMemory + meshMemory + shaderMemory + bufferMemory;
    }
};

/**
 * @brief 单个资源信息
 */
struct ResourceInfo {
    std::string name;              ///< 资源名称/路径
    size_t memorySize = 0;         ///< 内存占用（字节）
    uint32_t width = 0;            ///< 宽度（纹理）
    uint32_t height = 0;           ///< 高度（纹理）
    uint32_t vertexCount = 0;      ///< 顶点数（网格）
    uint32_t indexCount = 0;       ///< 索引数（网格）
};

/**
 * @brief 资源内存追踪器
 * 
 * 追踪和统计所有资源的内存使用情况，提供：
 * - 实时内存统计
 * - 详细资源列表
 * - 内存泄漏检测
 * - 导出功能（JSON/CSV）
 * 
 * 线程安全：所有公共方法都是线程安全的
 * 
 * 使用方式：
 * @code
 * auto& tracker = ResourceMemoryTracker::GetInstance();
 * 
 * // 注册资源
 * tracker.RegisterTexture(texture);
 * tracker.RegisterMesh(mesh);
 * 
 * // 获取统计
 * auto stats = tracker.GetStats();
 * printf("Total memory: %.2f MB\n", stats.totalMemory / (1024.0f * 1024.0f));
 * 
 * // 注销资源
 * tracker.UnregisterTexture(texture);
 * @endcode
 */
class ResourceMemoryTracker {
public:
    /**
     * @brief 获取单例实例
     */
    static ResourceMemoryTracker& GetInstance();
    
    // 禁止拷贝和移动
    ResourceMemoryTracker(const ResourceMemoryTracker&) = delete;
    ResourceMemoryTracker& operator=(const ResourceMemoryTracker&) = delete;
    ResourceMemoryTracker(ResourceMemoryTracker&&) = delete;
    ResourceMemoryTracker& operator=(ResourceMemoryTracker&&) = delete;
    
    // ========================================================================
    // 纹理追踪
    // ========================================================================
    
    /**
     * @brief 注册纹理
     * @param texture 纹理指针
     */
    void RegisterTexture(Texture* texture);
    
    /**
     * @brief 注销纹理
     * @param texture 纹理指针
     */
    void UnregisterTexture(Texture* texture);
    
    /**
     * @brief 获取所有纹理信息
     * @return 纹理信息列表
     */
    std::vector<ResourceInfo> GetTextureInfoList() const;
    
    // ========================================================================
    // 网格追踪
    // ========================================================================
    
    /**
     * @brief 注册网格
     * @param mesh 网格指针
     */
    void RegisterMesh(Mesh* mesh);
    
    /**
     * @brief 注销网格
     * @param mesh 网格指针
     */
    void UnregisterMesh(Mesh* mesh);
    
    /**
     * @brief 获取所有网格信息
     * @return 网格信息列表
     */
    std::vector<ResourceInfo> GetMeshInfoList() const;
    
    // ========================================================================
    // 着色器追踪
    // ========================================================================
    
    /**
     * @brief 注册着色器
     * @param shader 着色器指针
     */
    void RegisterShader(Shader* shader);
    
    /**
     * @brief 注销着色器
     * @param shader 着色器指针
     */
    void UnregisterShader(Shader* shader);
    
    /**
     * @brief 获取所有着色器信息
     * @return 着色器信息列表
     */
    std::vector<ResourceInfo> GetShaderInfoList() const;
    
    // ========================================================================
    // GPU 缓冲追踪
    // ========================================================================
    
    /**
     * @brief 注册 GPU 缓冲
     * @param bufferId OpenGL 缓冲 ID
     * @param size 缓冲大小（字节）
     * @param name 缓冲名称（用于调试）
     */
    void RegisterBuffer(uint32_t bufferId, size_t size, const std::string& name = "");
    
    /**
     * @brief 注销 GPU 缓冲
     * @param bufferId OpenGL 缓冲 ID
     */
    void UnregisterBuffer(uint32_t bufferId);
    
    /**
     * @brief 获取所有缓冲信息
     * @return 缓冲信息列表
     */
    std::vector<ResourceInfo> GetBufferInfoList() const;
    
    // ========================================================================
    // 统计信息
    // ========================================================================
    
    /**
     * @brief 获取内存统计信息
     * @return 统计信息（返回副本以保证线程安全）
     */
    ResourceMemoryStats GetStats() const;
    
    /**
     * @brief 生成详细报告
     * @param outputPath 输出文件路径（JSON 格式）
     * @return 成功返回 true
     */
    bool GenerateReport(const std::string& outputPath) const;
    
    /**
     * @brief 重置所有统计信息
     * 
     * 注意：这会清除所有已注册的资源记录
     */
    void Reset();
    
    /**
     * @brief 检测可能的内存泄漏
     * @return 可能泄漏的资源列表
     */
    std::vector<std::string> DetectLeaks() const;

private:
    ResourceMemoryTracker() = default;
    ~ResourceMemoryTracker() = default;
    
    struct TextureEntry {
        Texture* texture = nullptr;
        size_t memorySize = 0;
        std::string name;
        uint32_t width = 0;
        uint32_t height = 0;
    };
    
    struct MeshEntry {
        Mesh* mesh = nullptr;
        size_t memorySize = 0;
        std::string name;
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
    };
    
    struct ShaderEntry {
        Shader* shader = nullptr;
        size_t memorySize = 0;
        std::string name;
    };
    
    struct BufferEntry {
        uint32_t bufferId = 0;
        size_t memorySize = 0;
        std::string name;
    };
    
    std::unordered_map<Texture*, TextureEntry> m_textures;
    std::unordered_map<Mesh*, MeshEntry> m_meshes;
    std::unordered_map<Shader*, ShaderEntry> m_shaders;
    std::unordered_map<uint32_t, BufferEntry> m_buffers;
    
    mutable std::shared_mutex m_mutex;  ///< 读写锁，支持多读单写
    
    // 辅助函数
    size_t CalculateTextureMemory(Texture* texture) const;
    size_t CalculateMeshMemory(Mesh* mesh) const;
    size_t CalculateShaderMemory(Shader* shader) const;
};

} // namespace Render

