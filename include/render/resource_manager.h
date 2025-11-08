#pragma once

#include "types.h"
#include "texture.h"
#include "mesh.h"
#include "material.h"
#include "shader.h"
#include "resource_handle.h"
#include "resource_slot.h"
#include "resource_dependency.h"
#include "sprite/sprite_atlas.h"
#include <unordered_map>
#include <mutex>
#include <string>
#include <vector>
#include <functional>

namespace Render {

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
    size_t spriteAtlasCount = 0;
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
    // 精灵图集管理
    // ========================================================================
    
    /**
     * @brief 注册精灵图集
     */
    bool RegisterSpriteAtlas(const std::string& name, SpriteAtlasPtr atlas);
    
    /**
     * @brief 获取精灵图集
     */
    SpriteAtlasPtr GetSpriteAtlas(const std::string& name);
    
    /**
     * @brief 移除精灵图集
     */
    bool RemoveSpriteAtlas(const std::string& name);
    
    /**
     * @brief 检查精灵图集是否存在
     */
    bool HasSpriteAtlas(const std::string& name) const;
    
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
    
    /**
     * @brief 列出所有 SpriteAtlas 名称
     */
    std::vector<std::string> ListSpriteAtlases() const;
    
    // ========================================================================
    // 高级功能
    // ========================================================================
    
    /**
     * @brief 遍历所有纹理
     * @param callback 回调函数 (name, texture)
     * 
     * @note 线程安全说明：
     *  - ✅ 使用快照模式，回调中可以安全地调用ResourceManager的其他方法
     *  - ✅ 回调看到的是调用时刻的资源快照，不是实时数据
     *  - ✅ 回调执行期间不持有任何锁，不会阻塞其他线程
     * 
     * @warning 性能注意事项：
     *  - 此方法会创建资源列表的临时副本（只复制shared_ptr，不复制资源本身）
     *  - 对于大量资源，会有一定的内存开销
     * 
     * @example 正确用法（现在更简单了）
     * @code
     * // 现在可以直接在回调中修改资源管理器
     * manager.ForEachTexture([&](const std::string& name, Ref<Texture> tex) {
     *     if (ShouldRemove(tex)) {
     *         manager.RemoveTexture(name);  // ✅ 安全！不会死锁
     *     }
     * });
     * @endcode
     */
    void ForEachTexture(std::function<void(const std::string&, Ref<Texture>)> callback);
    
    /**
     * @brief 遍历所有网格
     * @param callback 回调函数 (name, mesh)
     * 
     * @note 使用快照模式，回调中可以安全地调用ResourceManager的其他方法。
     *       详见 ForEachTexture() 的文档说明。
     */
    void ForEachMesh(std::function<void(const std::string&, Ref<Mesh>)> callback);
    
    /**
     * @brief 遍历所有材质
     * @param callback 回调函数 (name, material)
     * 
     * @note 使用快照模式，回调中可以安全地调用ResourceManager的其他方法。
     *       详见 ForEachTexture() 的文档说明。
     */
    void ForEachMaterial(std::function<void(const std::string&, Ref<Material>)> callback);
    
    /**
     * @brief 遍历所有着色器
     * @param callback 回调函数 (name, shader)
     * 
     * @note 使用快照模式，回调中可以安全地调用ResourceManager的其他方法。
     *       详见 ForEachTexture() 的文档说明。
     */
    void ForEachShader(std::function<void(const std::string&, Ref<Shader>)> callback);
    
    // ========================================================================
    // 智能句柄系统（新增）
    // ========================================================================
    
    /**
     * @brief 创建纹理句柄
     * @param name 纹理名称
     * @param texture 纹理对象
     * @return 纹理句柄
     * 
     * 句柄系统提供以下优势：
     * - 更好的缓存局部性（句柄只有8字节）
     * - 支持资源热重载（保持句柄，替换资源）
     * - 防止循环引用（不使用引用计数）
     * - 自动检测悬空引用（代数机制）
     */
    TextureHandle CreateTextureHandle(const std::string& name, Ref<Texture> texture);
    
    /**
     * @brief 创建网格句柄
     */
    MeshHandle CreateMeshHandle(const std::string& name, Ref<Mesh> mesh);
    
    /**
     * @brief 创建材质句柄
     */
    MaterialHandle CreateMaterialHandle(const std::string& name, Ref<Material> material);
    
    /**
     * @brief 创建着色器句柄
     */
    ShaderHandle CreateShaderHandle(const std::string& name, Ref<Shader> shader);
    
    /**
     * @brief 通过句柄获取纹理指针
     * @param handle 纹理句柄
     * @return 纹理指针，如果句柄无效返回 nullptr
     */
    Texture* GetTextureByHandle(const TextureHandle& handle);
    
    /**
     * @brief 通过句柄获取纹理的 shared_ptr
     */
    Ref<Texture> GetTextureSharedByHandle(const TextureHandle& handle);
    
    /**
     * @brief 检查纹理句柄是否有效
     */
    bool IsTextureHandleValid(const TextureHandle& handle) const;
    
    /**
     * @brief 通过句柄获取网格指针
     */
    Mesh* GetMeshByHandle(const MeshHandle& handle);
    
    /**
     * @brief 通过句柄获取网格的 shared_ptr
     */
    Ref<Mesh> GetMeshSharedByHandle(const MeshHandle& handle);
    
    /**
     * @brief 检查网格句柄是否有效
     */
    bool IsMeshHandleValid(const MeshHandle& handle) const;
    
    /**
     * @brief 通过句柄获取材质指针
     */
    Material* GetMaterialByHandle(const MaterialHandle& handle);
    
    /**
     * @brief 通过句柄获取材质的 shared_ptr
     */
    Ref<Material> GetMaterialSharedByHandle(const MaterialHandle& handle);
    
    /**
     * @brief 检查材质句柄是否有效
     */
    bool IsMaterialHandleValid(const MaterialHandle& handle) const;
    
    /**
     * @brief 通过句柄获取着色器指针
     */
    Shader* GetShaderByHandle(const ShaderHandle& handle);
    
    /**
     * @brief 通过句柄获取着色器的 shared_ptr
     */
    Ref<Shader> GetShaderSharedByHandle(const ShaderHandle& handle);
    
    /**
     * @brief 检查着色器句柄是否有效
     */
    bool IsShaderHandleValid(const ShaderHandle& handle) const;
    
    /**
     * @brief 热重载纹理
     * @param handle 纹理句柄
     * @param newTexture 新纹理
     * @return 是否成功
     * 
     * 保持句柄不变，只替换纹理内容。
     * 所有持有该句柄的对象会自动使用新纹理。
     */
    bool ReloadTexture(const TextureHandle& handle, Ref<Texture> newTexture);
    
    /**
     * @brief 热重载网格
     */
    bool ReloadMesh(const MeshHandle& handle, Ref<Mesh> newMesh);
    
    /**
     * @brief 热重载材质
     */
    bool ReloadMaterial(const MaterialHandle& handle, Ref<Material> newMaterial);
    
    /**
     * @brief 热重载着色器
     */
    bool ReloadShader(const ShaderHandle& handle, Ref<Shader> newShader);
    
    /**
     * @brief 通过句柄移除纹理
     */
    bool RemoveTextureByHandle(const TextureHandle& handle);
    
    /**
     * @brief 通过句柄移除网格
     */
    bool RemoveMeshByHandle(const MeshHandle& handle);
    
    /**
     * @brief 通过句柄移除材质
     */
    bool RemoveMaterialByHandle(const MaterialHandle& handle);
    
    /**
     * @brief 通过句柄移除着色器
     */
    bool RemoveShaderByHandle(const ShaderHandle& handle);
    
    /**
     * @brief 获取句柄系统统计信息
     */
    struct HandleStats {
        size_t textureSlots = 0;
        size_t textureActiveSlots = 0;
        size_t textureFreeSlots = 0;
        
        size_t meshSlots = 0;
        size_t meshActiveSlots = 0;
        size_t meshFreeSlots = 0;
        
        size_t materialSlots = 0;
        size_t materialActiveSlots = 0;
        size_t materialFreeSlots = 0;
        
        size_t shaderSlots = 0;
        size_t shaderActiveSlots = 0;
        size_t shaderFreeSlots = 0;
    };
    
    HandleStats GetHandleStats() const;
    
    // ========================================================================
    // 依赖关系跟踪和循环检测（新增）
    // ========================================================================
    
    /**
     * @brief 获取依赖跟踪器
     * @return 依赖跟踪器引用
     */
    ResourceDependencyTracker& GetDependencyTracker() { return m_dependencyTracker; }
    const ResourceDependencyTracker& GetDependencyTracker() const { return m_dependencyTracker; }
    
    /**
     * @brief 更新资源的依赖关系
     * @param resourceName 资源名称
     * @param dependencies 依赖列表
     * 
     * 此方法应在资源加载或修改时调用，以保持依赖关系最新
     */
    void UpdateResourceDependencies(const std::string& resourceName,
                                    const std::vector<std::string>& dependencies);
    
    /**
     * @brief 检测是否存在循环引用
     * @return 所有发现的循环引用
     */
    std::vector<CircularReference> DetectCircularReferences();
    
    /**
     * @brief 执行完整的依赖分析
     * @return 分析结果
     */
    DependencyAnalysisResult AnalyzeDependencies();
    
    /**
     * @brief 打印依赖关系统计信息
     */
    void PrintDependencyStatistics() const;
    
    /**
     * @brief 生成依赖关系DOT图（用于Graphviz可视化）
     * @param outputPath 输出文件路径
     * @return 是否成功
     */
    bool ExportDependencyGraph(const std::string& outputPath);
    
private:
    ResourceManager() = default;
    ~ResourceManager() = default;
    
    // 资源存储（使用 ResourceEntry 封装）- 传统方式
    std::unordered_map<std::string, ResourceEntry<Texture>> m_textures;
    std::unordered_map<std::string, ResourceEntry<Mesh>> m_meshes;
    std::unordered_map<std::string, ResourceEntry<Material>> m_materials;
    std::unordered_map<std::string, ResourceEntry<Shader>> m_shaders;
    std::unordered_map<std::string, ResourceEntry<SpriteAtlas>> m_spriteAtlases;
    
    // 智能句柄系统 - 新方式
    ResourceSlotManager<Texture> m_textureSlots;
    ResourceSlotManager<Mesh> m_meshSlots;
    ResourceSlotManager<Material> m_materialSlots;
    ResourceSlotManager<Shader> m_shaderSlots;
    
    // 名称到句柄的映射（用于通过名称查找句柄）
    std::unordered_map<std::string, TextureHandle> m_textureHandles;
    std::unordered_map<std::string, MeshHandle> m_meshHandles;
    std::unordered_map<std::string, MaterialHandle> m_materialHandles;
    std::unordered_map<std::string, ShaderHandle> m_shaderHandles;
    
    // 帧计数器
    uint32_t m_currentFrame = 0;
    
    // 依赖关系跟踪器
    ResourceDependencyTracker m_dependencyTracker;
    
    // 线程安全
    mutable std::mutex m_mutex;
};

} // namespace Render

