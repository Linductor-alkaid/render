#pragma once

#include "render/shader.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <shared_mutex>

namespace Render {

/**
 * @brief 着色器缓存管理器
 * 
 * 管理着色器的加载、缓存和生命周期
 * 支持资源引用计数和自动释放
 */
class ShaderCache {
public:
    /**
     * @brief 获取单例实例
     */
    static ShaderCache& GetInstance();
    
    /**
     * @brief 加载或获取着色器
     * @param name 着色器名称（用于缓存键）
     * @param vertexPath 顶点着色器路径
     * @param fragmentPath 片段着色器路径
     * @param geometryPath 几何着色器路径（可选）
     * @return 着色器指针，失败返回 nullptr
     */
    std::shared_ptr<Shader> LoadShader(const std::string& name,
                                       const std::string& vertexPath,
                                       const std::string& fragmentPath,
                                       const std::string& geometryPath = "");
    
    /**
     * @brief 从源码加载或获取着色器
     * @param name 着色器名称
     * @param vertexSource 顶点着色器源码
     * @param fragmentSource 片段着色器源码
     * @param geometrySource 几何着色器源码（可选）
     * @return 着色器指针，失败返回 nullptr
     */
    std::shared_ptr<Shader> LoadShaderFromSource(const std::string& name,
                                                  const std::string& vertexSource,
                                                  const std::string& fragmentSource,
                                                  const std::string& geometrySource = "");
    
    /**
     * @brief 获取已缓存的着色器
     * @param name 着色器名称
     * @return 着色器指针，未找到返回 nullptr
     */
    std::shared_ptr<Shader> GetShader(const std::string& name);
    
    /**
     * @brief 检查着色器是否在缓存中
     * @param name 着色器名称
     * @return 存在返回 true，否则返回 false
     */
    bool HasShader(const std::string& name) const;
    
    /**
     * @brief 重载指定着色器
     * @param name 着色器名称
     * @return 成功返回 true
     */
    bool ReloadShader(const std::string& name);
    
    /**
     * @brief 重载所有着色器
     */
    void ReloadAll();
    
    /**
     * @brief 移除指定着色器
     * @param name 着色器名称
     */
    void RemoveShader(const std::string& name);
    
    /**
     * @brief 清空所有缓存
     */
    void Clear();
    
    /**
     * @brief 获取缓存中的着色器数量
     */
    size_t GetShaderCount() const { return m_shaders.size(); }
    
    /**
     * @brief 获取着色器引用计数
     * @param name 着色器名称
     * @return 引用计数，未找到返回 0
     */
    long GetReferenceCount(const std::string& name) const;
    
    /**
     * @brief 打印缓存统计信息
     */
    void PrintStatistics() const;
    
    /**
     * @brief 预编译着色器列表
     * @param shaderList 着色器定义列表 {name, vertPath, fragPath, geomPath}
     * @return 成功加载的着色器数量
     */
    size_t PrecompileShaders(const std::vector<std::tuple<std::string, std::string, std::string, std::string>>& shaderList);
    
private:
    ShaderCache() = default;
    ~ShaderCache() = default;
    
    // 禁止拷贝和赋值
    ShaderCache(const ShaderCache&) = delete;
    ShaderCache& operator=(const ShaderCache&) = delete;
    
    std::unordered_map<std::string, std::shared_ptr<Shader>> m_shaders;
    mutable std::shared_mutex m_mutex;  // 读写锁，支持多读单写
};

} // namespace Render

