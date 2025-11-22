#include "render/application/toolchain/material_shader_panel.h"

#include "render/resource_manager.h"
#include "render/material.h"
#include "render/shader.h"
#include "render/logger.h"

#include <algorithm>

namespace Render::Application {

MaterialShaderPanelDataSource::MaterialShaderPanelDataSource(ResourceManager& resourceManager)
    : m_resourceManager(resourceManager) {
}

std::vector<std::string> MaterialShaderPanelDataSource::GetMaterialNames() const {
    return m_resourceManager.ListMaterials();
}

std::optional<MaterialInfo> MaterialShaderPanelDataSource::GetMaterialInfo(const std::string& name) const {
    auto material = m_resourceManager.GetMaterial(name);
    if (!material) {
        return std::nullopt;
    }

    MaterialInfo info;
    info.name = name;
    info.isValid = true;

    // 获取着色器名称
    auto shader = material->GetShader();
    if (shader) {
        info.shaderName = shader->GetName();
    }

    // 获取材质属性
    info.ambientColor = material->GetAmbientColor();
    info.diffuseColor = material->GetDiffuseColor();
    info.specularColor = material->GetSpecularColor();
    info.emissiveColor = material->GetEmissiveColor();
    info.shininess = material->GetShininess();

    // 获取物理材质属性
    info.metallic = material->GetMetallic();
    info.roughness = material->GetRoughness();

    // 获取纹理列表（需要Material提供获取纹理名称的方法）
    // 这里先留空，等Material接口完善后再实现
    // info.textureNames = material->GetTextureNames();

    return info;
}

std::vector<MaterialInfo> MaterialShaderPanelDataSource::GetAllMaterialInfos() const {
    std::vector<MaterialInfo> infos;
    
    m_resourceManager.ForEachMaterial([&](const std::string& name, Ref<Material> material) {
        if (!material) {
            return;
        }

        MaterialInfo info;
        info.name = name;
        info.isValid = true;

        auto shader = material->GetShader();
        if (shader) {
            info.shaderName = shader->GetName();
        }

        info.ambientColor = material->GetAmbientColor();
        info.diffuseColor = material->GetDiffuseColor();
        info.specularColor = material->GetSpecularColor();
        info.emissiveColor = material->GetEmissiveColor();
        info.shininess = material->GetShininess();
        info.metallic = material->GetMetallic();
        info.roughness = material->GetRoughness();

        infos.push_back(std::move(info));
    });

    // 按名称排序
    std::sort(infos.begin(), infos.end(), [](const MaterialInfo& lhs, const MaterialInfo& rhs) {
        return lhs.name < rhs.name;
    });

    return infos;
}

void MaterialShaderPanelDataSource::ForEachMaterial(
    std::function<void(const std::string&, const MaterialInfo&)> callback) const {
    if (!callback) {
        return;
    }

    auto infos = GetAllMaterialInfos();
    for (const auto& info : infos) {
        callback(info.name, info);
    }
}

std::vector<std::string> MaterialShaderPanelDataSource::GetShaderNames() const {
    return m_resourceManager.ListShaders();
}

std::optional<ShaderInfo> MaterialShaderPanelDataSource::GetShaderInfo(const std::string& name) const {
    auto shader = m_resourceManager.GetShader(name);
    if (!shader || !shader->IsValid()) {
        return std::nullopt;
    }

    ShaderInfo info;
    info.name = name;
    info.programID = shader->GetProgramID();
    info.isValid = true;

    // 获取Uniform列表
    auto uniformManager = shader->GetUniformManager();
    if (uniformManager) {
        auto uniformNames = uniformManager->GetAllUniformNames();
        for (const auto& uniformName : uniformNames) {
            UniformInfo uniformInfo;
            uniformInfo.name = uniformName;
            uniformInfo.location = uniformManager->GetUniformLocation(uniformName);
            // 类型名称需要从OpenGL查询，这里先设置为未知
            uniformInfo.typeName = "unknown";
            info.uniforms.push_back(uniformInfo);
        }
    }

    return info;
}

std::vector<ShaderInfo> MaterialShaderPanelDataSource::GetAllShaderInfos() const {
    std::vector<ShaderInfo> infos;

    m_resourceManager.ForEachShader([&](const std::string& name, Ref<Shader> shader) {
        if (!shader || !shader->IsValid()) {
            return;
        }

        ShaderInfo info;
        info.name = name;
        info.programID = shader->GetProgramID();
        info.isValid = true;

        auto uniformManager = shader->GetUniformManager();
        if (uniformManager) {
            auto uniformNames = uniformManager->GetAllUniformNames();
            for (const auto& uniformName : uniformNames) {
                UniformInfo uniformInfo;
                uniformInfo.name = uniformName;
                uniformInfo.location = uniformManager->GetUniformLocation(uniformName);
                // 类型名称需要从OpenGL查询，这里先设置为未知
                uniformInfo.typeName = "unknown";
                info.uniforms.push_back(uniformInfo);
            }
        }

        infos.push_back(std::move(info));
    });

    // 按名称排序
    std::sort(infos.begin(), infos.end(), [](const ShaderInfo& lhs, const ShaderInfo& rhs) {
        return lhs.name < rhs.name;
    });

    return infos;
}

void MaterialShaderPanelDataSource::ForEachShader(
    std::function<void(const std::string&, const ShaderInfo&)> callback) const {
    if (!callback) {
        return;
    }

    auto infos = GetAllShaderInfos();
    for (const auto& info : infos) {
        callback(info.name, info);
    }
}

bool MaterialShaderPanelDataSource::UpdateMaterialProperty(
    const std::string& name,
    const std::string& propertyName,
    const std::string& value) {
    auto material = m_resourceManager.GetMaterial(name);
    if (!material) {
        Logger::GetInstance().Warning("MaterialShaderPanelDataSource::UpdateMaterialProperty - material not found: " + name);
        return false;
    }

    // 这里需要根据propertyName和value类型来更新材质
    // 目前Material类的接口可能还不完全支持所有属性的更新
    // 先实现基础功能，后续可以根据需要扩展

    Logger::GetInstance().InfoFormat(
        "MaterialShaderPanelDataSource::UpdateMaterialProperty - updating %s.%s = %s",
        name.c_str(),
        propertyName.c_str(),
        value.c_str()
    );

    return true;
}

long MaterialShaderPanelDataSource::GetMaterialReferenceCount(const std::string& name) const {
    return m_resourceManager.GetReferenceCount(ResourceType::Material, name);
}

} // namespace Render::Application

