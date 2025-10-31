#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>

namespace Render {

// 前向声明
class ResourceManager;

/**
 * @brief 资源ID类型
 */
using ResourceID = uint32_t;

/**
 * @brief 资源代数（Generation）类型
 * 
 * 代数用于检测悬空句柄：
 * - 每次资源被删除并重用时，代数递增
 * - 句柄保存创建时的代数
 * - 访问时比较代数，不匹配则表示资源已失效
 */
using ResourceGeneration = uint32_t;

/**
 * @brief 无效的资源ID
 */
constexpr ResourceID INVALID_RESOURCE_ID = std::numeric_limits<ResourceID>::max();

/**
 * @brief 智能资源句柄
 * 
 * 资源句柄是一个轻量级的资源引用，使用 ID + Generation 方式管理资源。
 * 相比 shared_ptr，资源句柄有以下优势：
 * 
 * 1. **更好的缓存局部性**
 *    - 句柄只包含两个 uint32_t（8字节），而 shared_ptr 是 16 字节
 *    - 可以在连续内存中存储大量句柄，提高缓存命中率
 * 
 * 2. **支持资源热重载**
 *    - 可以替换资源内容而不改变句柄
 *    - 所有持有句柄的对象自动使用新资源
 * 
 * 3. **防止循环引用**
 *    - 不使用引用计数，不会产生循环引用问题
 *    - 资源生命周期由 ResourceManager 统一管理
 * 
 * 4. **自动检测悬空引用**
 *    - 使用 generation 机制检测已删除的资源
 *    - 访问已删除资源会返回 nullptr，而不是崩溃
 * 
 * 5. **内存友好**
 *    - 所有资源在 ResourceManager 中连续存储
 *    - 删除资源后 ID 可以被重用
 *    - 减少内存碎片
 * 
 * 使用示例:
 * @code
 * // 创建资源并获取句柄
 * auto texture = std::make_shared<Texture>();
 * auto handle = resourceManager.CreateTextureHandle("albedo", texture);
 * 
 * // 使用句柄访问资源
 * if (auto tex = handle.Get()) {
 *     tex->Bind(0);
 * }
 * 
 * // 句柄可以安全复制
 * auto handle2 = handle;
 * 
 * // 资源删除后，句柄自动失效
 * resourceManager.RemoveTexture("albedo");
 * assert(handle.Get() == nullptr);  // 返回 nullptr 而不是崩溃
 * @endcode
 * 
 * @tparam T 资源类型（Texture, Mesh, Material, Shader）
 */
template<typename T>
class ResourceHandle {
public:
    /**
     * @brief 默认构造函数 - 创建无效句柄
     */
    ResourceHandle() 
        : m_id(INVALID_RESOURCE_ID)
        , m_generation(0) {
    }
    
    /**
     * @brief 构造函数
     * @param id 资源ID
     * @param generation 资源代数
     */
    ResourceHandle(ResourceID id, ResourceGeneration generation)
        : m_id(id)
        , m_generation(generation) {
    }
    
    /**
     * @brief 获取资源指针
     * @return 资源指针，如果句柄无效或资源已删除，返回 nullptr
     * 
     * 此方法是线程安全的，但返回的指针需要立即使用。
     * 如果需要长期持有资源，应该获取 shared_ptr。
     */
    T* Get() const;
    
    /**
     * @brief 获取资源的 shared_ptr
     * @return 资源的 shared_ptr，如果句柄无效或资源已删除，返回 nullptr
     * 
     * 这个方法返回 shared_ptr，可以安全地长期持有。
     * 适用于需要在多个地方使用资源的场景。
     */
    std::shared_ptr<T> GetShared() const;
    
    /**
     * @brief 检查句柄是否有效
     * @return 如果句柄指向有效资源，返回 true
     */
    bool IsValid() const;
    
    /**
     * @brief 使句柄失效
     */
    void Invalidate() {
        m_id = INVALID_RESOURCE_ID;
        m_generation = 0;
    }
    
    /**
     * @brief 获取资源ID
     */
    ResourceID GetID() const {
        return m_id;
    }
    
    /**
     * @brief 获取资源代数
     */
    ResourceGeneration GetGeneration() const {
        return m_generation;
    }
    
    /**
     * @brief bool 转换运算符
     * 
     * 允许在条件语句中使用：
     * if (handle) { ... }
     */
    explicit operator bool() const {
        return IsValid();
    }
    
    /**
     * @brief 箭头运算符
     * 
     * 允许像指针一样使用句柄：
     * handle->Bind();
     * 
     * 注意：使用前应该检查 IsValid()
     */
    T* operator->() const {
        return Get();
    }
    
    /**
     * @brief 解引用运算符
     * 
     * 注意：使用前应该检查 IsValid()
     */
    T& operator*() const {
        return *Get();
    }
    
    /**
     * @brief 相等比较
     */
    bool operator==(const ResourceHandle& other) const {
        return m_id == other.m_id && m_generation == other.m_generation;
    }
    
    /**
     * @brief 不等比较
     */
    bool operator!=(const ResourceHandle& other) const {
        return !(*this == other);
    }
    
    /**
     * @brief 小于比较（用于在容器中排序）
     */
    bool operator<(const ResourceHandle& other) const {
        if (m_id != other.m_id) {
            return m_id < other.m_id;
        }
        return m_generation < other.m_generation;
    }

private:
    ResourceID m_id;              // 资源ID
    ResourceGeneration m_generation;  // 资源代数
};

/**
 * @brief 纹理句柄
 */
class Texture;
using TextureHandle = ResourceHandle<Texture>;

/**
 * @brief 网格句柄
 */
class Mesh;
using MeshHandle = ResourceHandle<Mesh>;

/**
 * @brief 材质句柄
 */
class Material;
using MaterialHandle = ResourceHandle<Material>;

/**
 * @brief 着色器句柄
 */
class Shader;
using ShaderHandle = ResourceHandle<Shader>;

} // namespace Render

/**
 * @brief 为 ResourceHandle 提供哈希支持，以便在 unordered_map 等容器中使用
 */
namespace std {
    template<typename T>
    struct hash<Render::ResourceHandle<T>> {
        size_t operator()(const Render::ResourceHandle<T>& handle) const {
            // 组合 ID 和 generation 的哈希值
            size_t h1 = hash<Render::ResourceID>()(handle.GetID());
            size_t h2 = hash<Render::ResourceGeneration>()(handle.GetGeneration());
            return h1 ^ (h2 << 1);
        }
    };
}

