#pragma once

#include "render/resource_manager.h"
#include "render/material.h"
#include "render/shader.h"
#include "render/uniform_manager.h"

#include <string>
#include <vector>
#include <optional>
#include <functional>

namespace Render::Application {

/**
 * @brief 材质信息（用于工具链面板）
 */
struct MaterialInfo {
    std::string name;
    std::string shaderName;
    Color ambientColor;
    Color diffuseColor;
    Color specularColor;
    Color emissiveColor;
    float shininess = 0.0f;
    float metallic = 0.0f;
    float roughness = 0.0f;
    std::vector<std::string> textureNames;
    bool isValid = false;
};

/**
 * @brief Uniform信息（用于工具链面板）
 */
struct UniformInfo {
    std::string name;
    int location = -1;
    std::string typeName;  // 类型名称（如 "float", "vec3", "mat4" 等）
};

/**
 * @brief 着色器信息（用于工具链面板）
 */
struct ShaderInfo {
    std::string name;
    uint32_t programID = 0;
    std::string vertexPath;
    std::string fragmentPath;
    std::string geometryPath;
    std::vector<UniformInfo> uniforms;
    bool isValid = false;
};

/**
 * @brief 材质/Shader面板数据源接口
 * 
 * 为工具链（HUD、材质面板、Shader面板）提供材质和着色器的查询接口
 */
class MaterialShaderPanelDataSource {
public:
    MaterialShaderPanelDataSource(ResourceManager& resourceManager);
    ~MaterialShaderPanelDataSource() = default;

    // ========================================================================
    // 材质查询接口
    // ========================================================================

    /**
     * @brief 获取所有材质名称列表
     */
    std::vector<std::string> GetMaterialNames() const;

    /**
     * @brief 获取材质信息
     * @param name 材质名称
     * @return 材质信息，如果材质不存在返回 std::nullopt
     */
    std::optional<MaterialInfo> GetMaterialInfo(const std::string& name) const;

    /**
     * @brief 获取所有材质信息
     */
    std::vector<MaterialInfo> GetAllMaterialInfos() const;

    /**
     * @brief 遍历所有材质
     * @param callback 回调函数 (name, materialInfo)
     */
    void ForEachMaterial(std::function<void(const std::string&, const MaterialInfo&)> callback) const;

    // ========================================================================
    // 着色器查询接口
    // ========================================================================

    /**
     * @brief 获取所有着色器名称列表
     */
    std::vector<std::string> GetShaderNames() const;

    /**
     * @brief 获取着色器信息
     * @param name 着色器名称
     * @return 着色器信息，如果着色器不存在返回 std::nullopt
     */
    std::optional<ShaderInfo> GetShaderInfo(const std::string& name) const;

    /**
     * @brief 获取所有着色器信息
     */
    std::vector<ShaderInfo> GetAllShaderInfos() const;

    /**
     * @brief 遍历所有着色器
     * @param callback 回调函数 (name, shaderInfo)
     */
    void ForEachShader(std::function<void(const std::string&, const ShaderInfo&)> callback) const;

    // ========================================================================
    // 材质修改接口（供编辑器使用）
    // ========================================================================

    /**
     * @brief 更新材质属性
     * @param name 材质名称
     * @param updates 更新的属性映射 (propertyName -> value)
     * @return 是否成功
     * 
     * 支持更新的属性：
     * - "ambientColor": Color
     * - "diffuseColor": Color
     * - "specularColor": Color
     * - "emissiveColor": Color
     * - "shininess": float
     * - "metallic": float
     * - "roughness": float
     * 
     * 注意：此接口仅用于运行时修改，不会持久化到文件
     */
    bool UpdateMaterialProperty(const std::string& name, const std::string& propertyName, const std::string& value);

    /**
     * @brief 获取材质的着色器引用计数
     */
    long GetMaterialReferenceCount(const std::string& name) const;

private:
    ResourceManager& m_resourceManager;
};

} // namespace Render::Application

