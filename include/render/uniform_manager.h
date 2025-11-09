#pragma once

#include "render/types.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace Render {

/**
 * @brief Uniform 变量管理器
 * 
 * 统一管理着色器的 uniform 变量，提供类型安全的接口
 * 并缓存 uniform 位置以提高性能
 */
class UniformManager {
public:
    UniformManager(uint32_t programID);
    ~UniformManager();
    
    /**
     * @brief 设置 int uniform
     */
    void SetInt(const std::string& name, int value);
    
    /**
     * @brief 设置 float uniform
     */
    void SetFloat(const std::string& name, float value);
    
    /**
     * @brief 设置 bool uniform
     */
    void SetBool(const std::string& name, bool value);
    
    /**
     * @brief 设置 Vector2 uniform
     */
    void SetVector2(const std::string& name, const Vector2& value);
    
    /**
     * @brief 设置 Vector3 uniform
     */
    void SetVector3(const std::string& name, const Vector3& value);
    
    /**
     * @brief 设置 Vector4 uniform
     */
    void SetVector4(const std::string& name, const Vector4& value);
    
    /**
     * @brief 设置 Matrix3 uniform
     */
    void SetMatrix3(const std::string& name, const Matrix3& value);
    
    /**
     * @brief 设置 Matrix4 uniform
     */
    void SetMatrix4(const std::string& name, const Matrix4& value);
    
    /**
     * @brief 设置 Color uniform
     */
    void SetColor(const std::string& name, const Color& value);
    
    /**
     * @brief 设置 int 数组
     */
    void SetIntArray(const std::string& name, const int* values, uint32_t count);
    
    /**
     * @brief 设置 float 数组
     */
    void SetFloatArray(const std::string& name, const float* values, uint32_t count);
    
    /**
     * @brief 设置 Vector3 数组
     */
    void SetVector3Array(const std::string& name, const Vector3* values, uint32_t count);
    
    /**
     * @brief 设置 Vector2 数组
     */
    void SetVector2Array(const std::string& name, const Vector2* values, uint32_t count);
    
    /**
     * @brief 设置 Matrix4 数组
     */
    void SetMatrix4Array(const std::string& name, const Matrix4* values, uint32_t count);

    /**
     * @brief 设置 Vector4 数组
     */
    void SetVector4Array(const std::string& name, const Vector4* values, uint32_t count);

    /**
     * @brief 设置颜色数组
     */
    void SetColorArray(const std::string& name, const Color* values, uint32_t count);
    
    /**
     * @brief 检查 uniform 是否存在
     */
    bool HasUniform(const std::string& name) const;

    /**
     * @brief 注册纹理采样器 uniform 并绑定到指定纹理单元
     * @param name uniform 名称
     * @param textureUnit 纹理单元（0-31）
     */
    void RegisterTextureUniform(const std::string& name, int textureUnit);

    /**
     * @brief 获取已注册的纹理单元
     * @param name uniform 名称
     * @param outTextureUnit 输出纹理单元
     * @return 如果 uniform 已注册，返回 true
     */
    bool TryGetTextureUnit(const std::string& name, int& outTextureUnit) const;
    
    /**
     * @brief 获取 uniform 位置
     */
    int GetUniformLocation(const std::string& name);
    
    /**
     * @brief 清除缓存
     */
    void ClearCache();
    
    /**
     * @brief 获取所有 uniform 名称
     */
    std::vector<std::string> GetAllUniformNames() const;
    
    /**
     * @brief 打印所有 uniform 信息（调试用）
     */
    void PrintUniformInfo() const;
    
private:
    uint32_t m_programID;
    
    // Uniform 位置缓存
    mutable std::unordered_map<std::string, int> m_uniformLocationCache;
    mutable std::mutex m_cacheMutex;  // 保护缓存的互斥锁
    mutable std::unordered_map<std::string, int> m_textureUnits; // 已注册的纹理单元
    
    /**
     * @brief 获取或查找 uniform 位置
     */
    int GetOrFindUniformLocation(const std::string& name);
};

} // namespace Render

