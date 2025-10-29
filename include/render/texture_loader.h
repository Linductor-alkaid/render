#pragma once

#include "render/texture.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <future>
#include <mutex>

namespace Render {

/**
 * @brief 异步纹理加载结果
 */
struct AsyncTextureResult {
    bool success;           ///< 是否加载成功
    TexturePtr texture;     ///< 纹理指针
    std::string error;      ///< 错误信息（如果失败）
};

/**
 * @brief 纹理加载器
 * 
 * 负责纹理的加载、缓存和资源管理
 * 支持同步和异步加载模式
 */
class TextureLoader {
public:
    /**
     * @brief 获取单例实例
     */
    static TextureLoader& GetInstance();
    
    /**
     * @brief 加载或获取纹理（同步）
     * @param name 纹理名称（用于缓存键）
     * @param filepath 纹理文件路径
     * @param generateMipmap 是否生成 Mipmap
     * @return 纹理指针，失败返回 nullptr
     */
    TexturePtr LoadTexture(const std::string& name,
                          const std::string& filepath,
                          bool generateMipmap = true);
    
    /**
     * @brief 从内存数据创建或获取纹理
     * @param name 纹理名称
     * @param data 纹理数据指针
     * @param width 纹理宽度
     * @param height 纹理高度
     * @param format 纹理格式
     * @param generateMipmap 是否生成 Mipmap
     * @return 纹理指针，失败返回 nullptr
     */
    TexturePtr CreateTexture(const std::string& name,
                            const void* data,
                            int width,
                            int height,
                            TextureFormat format = TextureFormat::RGBA,
                            bool generateMipmap = true);
    
    /**
     * @brief 异步加载纹理
     * @param name 纹理名称
     * @param filepath 纹理文件路径
     * @param generateMipmap 是否生成 Mipmap
     * @return 异步结果的 future 对象
     */
    std::future<AsyncTextureResult> LoadTextureAsync(const std::string& name,
                                                      const std::string& filepath,
                                                      bool generateMipmap = true);
    
    /**
     * @brief 获取已缓存的纹理
     * @param name 纹理名称
     * @return 纹理指针，未找到返回 nullptr
     */
    TexturePtr GetTexture(const std::string& name);
    
    /**
     * @brief 检查纹理是否已缓存
     * @param name 纹理名称
     * @return 已缓存返回 true
     */
    bool HasTexture(const std::string& name) const;
    
    /**
     * @brief 移除指定纹理
     * @param name 纹理名称
     * @return 成功返回 true
     */
    bool RemoveTexture(const std::string& name);
    
    /**
     * @brief 清空所有缓存
     */
    void Clear();
    
    /**
     * @brief 获取缓存中的纹理数量
     */
    size_t GetTextureCount() const;
    
    /**
     * @brief 获取纹理引用计数
     * @param name 纹理名称
     * @return 引用计数，未找到返回 0
     */
    long GetReferenceCount(const std::string& name) const;
    
    /**
     * @brief 打印缓存统计信息
     */
    void PrintStatistics() const;
    
    /**
     * @brief 预加载纹理列表
     * @param textureList 纹理定义列表 {name, filepath, generateMipmap}
     * @return 成功加载的纹理数量
     */
    size_t PreloadTextures(const std::vector<std::tuple<std::string, std::string, bool>>& textureList);
    
    /**
     * @brief 清理未使用的纹理（引用计数为1，仅被缓存持有）
     * @return 清理的纹理数量
     */
    size_t CleanupUnused();
    
    /**
     * @brief 获取总纹理内存使用量（估算）
     * @return 内存字节数
     */
    size_t GetTotalMemoryUsage() const;

private:
    TextureLoader() = default;
    ~TextureLoader() = default;
    
    // 禁止拷贝和赋值
    TextureLoader(const TextureLoader&) = delete;
    TextureLoader& operator=(const TextureLoader&) = delete;
    
    /**
     * @brief 内部加载纹理函数
     */
    TexturePtr LoadTextureInternal(const std::string& filepath, bool generateMipmap);
    
    std::unordered_map<std::string, TexturePtr> m_textures;  ///< 纹理缓存
    mutable std::mutex m_mutex;                              ///< 线程安全互斥锁
};

} // namespace Render

