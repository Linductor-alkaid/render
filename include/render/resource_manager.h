#pragma once

#include "types.h"
#include "texture.h"
#include "mesh.h"
#include "material.h"
#include "shader.h"
#include <unordered_map>
#include <mutex>
#include <string>
#include <vector>
#include <functional>

namespace Render {

/**
 * @brief 资源类型枚举
 */
enum class ResourceType {
    Texture,
    Mesh,
    Material,
    Shader
};

/**
 * @brief 资源条目
 * 
 * 封装资源引用和访问信息，用于更安全的资源生命周期管理
 */
template<typename T>
struct ResourceEntry {
    std::shared_ptr<T> resource;        // 资源引用
    bool markedForDeletion = false;     // 删除标记（用于两阶段清理）
    uint32_t lastAccessFrame = 0;       // 最后访问帧号
    
    ResourceEntry() = default;
    ResourceEntry(std::shared_ptr<T> res, uint32_t frame)
        : resource(std::move(res)), lastAccessFrame(frame) {}
};

/**
 * @brief 资源统计信息
 */
struct ResourceStats {
    size_t textureCount = 0;
    size_t meshCount = 0;
    size_t materialCount = 0;
    size_t shaderCount = 0;
    size_t totalCount = 0;
    
    size_t textureMemory = 0;    // 纹理内存（字节）
    size_t meshMemory = 0;        // 网格内存（字节）
    size_t totalMemory = 0;       // 总内存（字节）
};

/**
 * @brief 资源管理器
 * 
 * 统一管理所有渲染资源（纹理、网格、材质、着色器）
 * 提供注册、获取、释放、统计等功能
 * 
 * 特性：
 * - 单例模式
 * - 线程安全
 * - 引用计数管理
 * - 自动清理未使用资源
 * - 资源统计和监控
 */
class ResourceManager {
public:
    /**
     * @brief 获取单例实例
     */
    static ResourceManager& GetInstance();
    
    // 禁止拷贝和移动
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) = delete;
    ResourceManager& operator=(ResourceManager&&) = delete;
    
    // ========================================================================
    // 纹理管理
    // ========================================================================
    
    /**
     * @brief 注册纹理资源
     * @param name 纹理名称（唯一标识）
     * @param texture 纹理对象
     * @return 是否注册成功（名称冲突会失败）
     */
    bool RegisterTexture(const std::string& name, Ref<Texture> texture);
    
    /**
     * @brief 获取纹理资源
     * @param name 纹理名称
     * @return 纹理对象，不存在返回 nullptr
     */
    Ref<Texture> GetTexture(const std::string& name);
    
    /**
     * @brief 移除纹理资源
     * @param name 纹理名称
     * @return 是否成功移除
     */
    bool RemoveTexture(const std::string& name);
    
    /**
     * @brief 检查纹理是否存在
     */
    bool HasTexture(const std::string& name) const;
    
    // ========================================================================
    // 网格管理
    // ========================================================================
    
    /**
     * @brief 注册网格资源
     */
    bool RegisterMesh(const std::string& name, Ref<Mesh> mesh);
    
    /**
     * @brief 获取网格资源
     */
    Ref<Mesh> GetMesh(const std::string& name);
    
    /**
     * @brief 移除网格资源
     */
    bool RemoveMesh(const std::string& name);
    
    /**
     * @brief 检查网格是否存在
     */
    bool HasMesh(const std::string& name) const;
    
    // ========================================================================
    // 材质管理
    // ========================================================================
    
    /**
     * @brief 注册材质资源
     */
    bool RegisterMaterial(const std::string& name, Ref<Material> material);
    
    /**
     * @brief 获取材质资源
     */
    Ref<Material> GetMaterial(const std::string& name);
    
    /**
     * @brief 移除材质资源
     */
    bool RemoveMaterial(const std::string& name);
    
    /**
     * @brief 检查材质是否存在
     */
    bool HasMaterial(const std::string& name) const;
    
    // ========================================================================
    // 着色器管理
    // ========================================================================
    
    /**
     * @brief 注册着色器资源
     */
    bool RegisterShader(const std::string& name, Ref<Shader> shader);
    
    /**
     * @brief 获取着色器资源
     */
    Ref<Shader> GetShader(const std::string& name);
    
    /**
     * @brief 移除着色器资源
     */
    bool RemoveShader(const std::string& name);
    
    /**
     * @brief 检查着色器是否存在
     */
    bool HasShader(const std::string& name) const;
    
    // ========================================================================
    // 批量操作
    // ========================================================================
    
    /**
     * @brief 清空所有资源
     */
    void Clear();
    
    /**
     * @brief 清空指定类型的资源
     * @param type 资源类型
     */
    void ClearType(ResourceType type);
    
    /**
     * @brief 清理未使用的资源（引用计数为1，仅被管理器持有）
     * @param unusedFrames 资源多少帧未使用后清理（默认60帧）
     * @return 清理的资源数量
     * 
     * 注意：使用两阶段清理策略，避免竞态条件
     */
    size_t CleanupUnused(uint32_t unusedFrames = 60);
    
    /**
     * @brief 清理指定类型的未使用资源
     * @param type 资源类型
     * @param unusedFrames 资源多少帧未使用后清理（默认60帧）
     */
    size_t CleanupUnusedType(ResourceType type, uint32_t unusedFrames = 60);
    
    /**
     * @brief 开始新的一帧
     * 
     * 应在每帧开始时调用，用于跟踪资源访问
     */
    void BeginFrame();
    
    // ========================================================================
    // 统计和监控
    // ========================================================================
    
    /**
     * @brief 获取资源统计信息
     */
    ResourceStats GetStats() const;
    
    /**
     * @brief 获取指定资源的引用计数
     * @param type 资源类型
     * @param name 资源名称
     * @return 引用计数，资源不存在返回 0
     */
    long GetReferenceCount(ResourceType type, const std::string& name) const;
    
    /**
     * @brief 打印资源统计信息
     */
    void PrintStatistics() const;
    
    /**
     * @brief 列出所有纹理名称
     */
    std::vector<std::string> ListTextures() const;
    
    /**
     * @brief 列出所有网格名称
     */
    std::vector<std::string> ListMeshes() const;
    
    /**
     * @brief 列出所有材质名称
     */
    std::vector<std::string> ListMaterials() const;
    
    /**
     * @brief 列出所有着色器名称
     */
    std::vector<std::string> ListShaders() const;
    
    // ========================================================================
    // 高级功能
    // ========================================================================
    
    /**
     * @brief 遍历所有纹理
     * @param callback 回调函数 (name, texture)
     */
    void ForEachTexture(std::function<void(const std::string&, Ref<Texture>)> callback);
    
    /**
     * @brief 遍历所有网格
     */
    void ForEachMesh(std::function<void(const std::string&, Ref<Mesh>)> callback);
    
    /**
     * @brief 遍历所有材质
     */
    void ForEachMaterial(std::function<void(const std::string&, Ref<Material>)> callback);
    
    /**
     * @brief 遍历所有着色器
     */
    void ForEachShader(std::function<void(const std::string&, Ref<Shader>)> callback);
    
private:
    ResourceManager() = default;
    ~ResourceManager() = default;
    
    // 资源存储（使用 ResourceEntry 封装）
    std::unordered_map<std::string, ResourceEntry<Texture>> m_textures;
    std::unordered_map<std::string, ResourceEntry<Mesh>> m_meshes;
    std::unordered_map<std::string, ResourceEntry<Material>> m_materials;
    std::unordered_map<std::string, ResourceEntry<Shader>> m_shaders;
    
    // 帧计数器
    uint32_t m_currentFrame = 0;
    
    // 线程安全
    mutable std::mutex m_mutex;
};

} // namespace Render

