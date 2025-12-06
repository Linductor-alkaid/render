/*
 * Copyright (c) 2025 Li Chaoyu
 * 
 * This file is part of Render.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * For commercial licensing, please contact: 2052046346@qq.com
 */
#include "render/material.h"
#include "render/shader.h"
#include "render/texture.h"
#include "render/render_state.h"
#include "render/logger.h"
#include "render/error.h"
#include <utility>
#include <map>
#include <unordered_map>
#include <atomic>
#include <array>
#include <vector>
#include <algorithm>

namespace Render {

namespace {
constexpr size_t kMaxUniformVectorArraySize = 64;
}

std::atomic<uint32_t> Material::s_nextStableID{1};

// ============================================================================
// 构造和析构
// ============================================================================

Material::Material()
    : m_name("Unnamed Material")
    , m_shader(nullptr)
    , m_ambientColor(0.2f, 0.2f, 0.2f, 1.0f)
    , m_diffuseColor(0.8f, 0.8f, 0.8f, 1.0f)
    , m_specularColor(1.0f, 1.0f, 1.0f, 1.0f)
    , m_emissiveColor(0.0f, 0.0f, 0.0f, 1.0f)
    , m_shininess(32.0f)
    , m_opacity(1.0f)
    , m_metallic(0.0f)
    , m_roughness(0.5f)
    , m_blendMode(BlendMode::None)
    , m_cullFace(CullFace::Back)
    , m_depthTest(true)
    , m_depthWrite(true)
    , m_stableID(s_nextStableID.fetch_add(1u, std::memory_order_relaxed))
{
}

Material::~Material() {
    // 智能指针自动管理
}

Material::Material(Material&& other) noexcept {
    std::lock_guard<std::mutex> lock(other.m_mutex);
    m_name = std::move(other.m_name);
    m_shader = std::move(other.m_shader);
    m_ambientColor = other.m_ambientColor;
    m_diffuseColor = other.m_diffuseColor;
    m_specularColor = other.m_specularColor;
    m_emissiveColor = other.m_emissiveColor;
    m_shininess = other.m_shininess;
    m_opacity = other.m_opacity;
    m_metallic = other.m_metallic;
    m_roughness = other.m_roughness;
    m_textures = std::move(other.m_textures);
    m_intParams = std::move(other.m_intParams);
    m_floatParams = std::move(other.m_floatParams);
    m_vector2Params = std::move(other.m_vector2Params);
    m_vector3Params = std::move(other.m_vector3Params);
    m_vector4Params = std::move(other.m_vector4Params);
    m_matrix4Params = std::move(other.m_matrix4Params);
    m_vector2ArrayParams = std::move(other.m_vector2ArrayParams);
    m_colorArrayParams = std::move(other.m_colorArrayParams);
    m_blendMode = other.m_blendMode;
    m_cullFace = other.m_cullFace;
    m_depthTest = other.m_depthTest;
    m_depthWrite = other.m_depthWrite;
    m_stableID = other.m_stableID;
    other.m_stableID = 0;
    m_cacheDirty = true;
    m_cachedState.reset();
}

Material& Material::operator=(Material&& other) noexcept {
    if (this != &other) {
        std::scoped_lock lock(m_mutex, other.m_mutex);
        m_name = std::move(other.m_name);
        m_shader = std::move(other.m_shader);
        m_ambientColor = other.m_ambientColor;
        m_diffuseColor = other.m_diffuseColor;
        m_specularColor = other.m_specularColor;
        m_emissiveColor = other.m_emissiveColor;
        m_shininess = other.m_shininess;
        m_opacity = other.m_opacity;
        m_metallic = other.m_metallic;
        m_roughness = other.m_roughness;
        m_textures = std::move(other.m_textures);
        m_intParams = std::move(other.m_intParams);
        m_floatParams = std::move(other.m_floatParams);
        m_vector2Params = std::move(other.m_vector2Params);
        m_vector3Params = std::move(other.m_vector3Params);
        m_vector4Params = std::move(other.m_vector4Params);
        m_matrix4Params = std::move(other.m_matrix4Params);
        m_vector2ArrayParams = std::move(other.m_vector2ArrayParams);
        m_colorArrayParams = std::move(other.m_colorArrayParams);
        m_blendMode = other.m_blendMode;
        m_cullFace = other.m_cullFace;
        m_depthTest = other.m_depthTest;
        m_depthWrite = other.m_depthWrite;
        m_stableID = other.m_stableID;
        other.m_stableID = 0;
    }
    m_cacheDirty = true;
    m_cachedState.reset();
    return *this;
}

void Material::InvalidateCacheLocked() {
    m_cachedState.reset();
    m_cacheDirty = true;
}

std::shared_ptr<Material::CachedState> Material::EnsureCachedStateLocked() {
    if (!m_cacheDirty && m_cachedState) {
        return m_cachedState;
    }

    auto snapshot = std::make_shared<CachedState>();
    snapshot->shader = m_shader;
    snapshot->ambientColor = m_ambientColor;
    snapshot->diffuseColor = m_diffuseColor;
    snapshot->specularColor = m_specularColor;
    snapshot->emissiveColor = m_emissiveColor;
    snapshot->shininess = m_shininess;
    snapshot->opacity = m_opacity;
    snapshot->metallic = m_metallic;
    snapshot->roughness = m_roughness;
    snapshot->blendMode = m_blendMode;
    snapshot->cullFace = m_cullFace;
    snapshot->depthTest = m_depthTest;
    snapshot->depthWrite = m_depthWrite;
    snapshot->name = m_name;

    snapshot->textures.reserve(m_textures.size());
    for (const auto& entry : m_textures) {
        snapshot->textures.emplace_back(entry.first, entry.second);
    }

    snapshot->intParams.reserve(m_intParams.size());
    for (const auto& entry : m_intParams) {
        snapshot->intParams.emplace_back(entry.first, entry.second);
    }

    snapshot->floatParams.reserve(m_floatParams.size());
    for (const auto& entry : m_floatParams) {
        snapshot->floatParams.emplace_back(entry.first, entry.second);
    }

    snapshot->vector2Params.reserve(m_vector2Params.size());
    for (const auto& entry : m_vector2Params) {
        snapshot->vector2Params.emplace_back(entry.first, entry.second);
    }

    snapshot->vector3Params.reserve(m_vector3Params.size());
    for (const auto& entry : m_vector3Params) {
        snapshot->vector3Params.emplace_back(entry.first, entry.second);
    }

    snapshot->vector4Params.reserve(m_vector4Params.size());
    for (const auto& entry : m_vector4Params) {
        snapshot->vector4Params.emplace_back(entry.first, entry.second);
    }

    snapshot->matrix4Params.reserve(m_matrix4Params.size());
    for (const auto& entry : m_matrix4Params) {
        snapshot->matrix4Params.emplace_back(entry.first, entry.second);
    }

    snapshot->vector2ArrayParams.reserve(m_vector2ArrayParams.size());
    for (const auto& entry : m_vector2ArrayParams) {
        snapshot->vector2ArrayParams.emplace_back(entry.first, entry.second);
    }

    snapshot->colorArrayParams.reserve(m_colorArrayParams.size());
    for (const auto& entry : m_colorArrayParams) {
        snapshot->colorArrayParams.emplace_back(entry.first, entry.second);
    }

    m_cachedState = snapshot;
    m_cacheDirty = false;
    return m_cachedState;
}

// ============================================================================
// 名称管理
// ============================================================================

void Material::SetName(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_name = name;
    InvalidateCacheLocked();
}

std::string Material::GetName() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_name;
}

// ============================================================================
// 着色器管理
// ============================================================================

void Material::SetShader(std::shared_ptr<Shader> shader) {
    if (!shader) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::NullPointer, 
                                   "Material::SetShader: 尝试设置空着色器"));
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    m_shader = shader;
    InvalidateCacheLocked();
}

std::shared_ptr<Shader> Material::GetShader() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_shader;
}

// ============================================================================
// 材质属性 - 颜色
// ============================================================================

void Material::SetAmbientColor(const Color& color) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ambientColor = color;
    InvalidateCacheLocked();
}

Color Material::GetAmbientColor() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_ambientColor;
}

void Material::SetDiffuseColor(const Color& color) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_diffuseColor = color;
    InvalidateCacheLocked();
}

Color Material::GetDiffuseColor() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_diffuseColor;
}

void Material::SetSpecularColor(const Color& color) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_specularColor = color;
    InvalidateCacheLocked();
}

Color Material::GetSpecularColor() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_specularColor;
}

void Material::SetEmissiveColor(const Color& color) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_emissiveColor = color;
    InvalidateCacheLocked();
}

Color Material::GetEmissiveColor() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_emissiveColor;
}

// ============================================================================
// 材质属性 - 物理参数
// ============================================================================

void Material::SetShininess(float shininess) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_shininess = shininess;
    InvalidateCacheLocked();
}

float Material::GetShininess() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_shininess;
}

void Material::SetOpacity(float opacity) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_opacity = opacity;
    InvalidateCacheLocked();
}

float Material::GetOpacity() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_opacity;
}

void Material::SetMetallic(float metallic) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_metallic = metallic;
    InvalidateCacheLocked();
}

float Material::GetMetallic() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metallic;
}

void Material::SetRoughness(float roughness) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_roughness = roughness;
    InvalidateCacheLocked();
}

float Material::GetRoughness() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_roughness;
}

// ============================================================================
// 纹理管理
// ============================================================================

void Material::SetTexture(const std::string& name, std::shared_ptr<Texture> texture) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (texture && texture->IsValid()) {
        m_textures[name] = texture;
    } else {
        LOG_WARNING("Attempting to set invalid texture '" + name + "' to material '" + m_name + "'");
    }
    InvalidateCacheLocked();
}

std::shared_ptr<Texture> Material::GetTexture(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_textures.find(name);
    if (it != m_textures.end()) {
        return it->second;
    }
    return nullptr;
}

bool Material::HasTexture(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_textures.find(name) != m_textures.end();
}

void Material::RemoveTexture(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_textures.erase(name);
    InvalidateCacheLocked();
}

void Material::ClearTextures() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_textures.clear();
    InvalidateCacheLocked();
}

std::vector<std::string> Material::GetTextureNames() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    names.reserve(m_textures.size());
    for (const auto& pair : m_textures) {
        names.push_back(pair.first);
    }
    return names;
}

void Material::ForEachTexture(std::function<void(const std::string&, const Ref<Texture>&)> callback) const {
    if (!callback) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& pair : m_textures) {
        callback(pair.first, pair.second);
    }
}

// ============================================================================
// 自定义参数
// ============================================================================

void Material::SetInt(const std::string& name, int value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_intParams[name] = value;
    InvalidateCacheLocked();
}

void Material::SetFloat(const std::string& name, float value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_floatParams[name] = value;
    InvalidateCacheLocked();
}

void Material::SetVector2(const std::string& name, const Vector2& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_vector2Params[name] = value;
    InvalidateCacheLocked();
}

void Material::SetVector3(const std::string& name, const Vector3& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_vector3Params[name] = value;
    InvalidateCacheLocked();
}

void Material::SetVector4(const std::string& name, const Vector4& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_vector4Params[name] = value;
    InvalidateCacheLocked();
}

void Material::SetColor(const std::string& name, const Color& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_vector4Params[name] = value.ToVector4();
    InvalidateCacheLocked();
}

void Material::SetVector2Array(const std::string& name, const std::vector<Vector2>& values) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (values.empty()) {
        m_vector2ArrayParams.erase(name);
        InvalidateCacheLocked();
        return;
    }

    if (values.size() > kMaxUniformVectorArraySize) {
        m_vector2ArrayParams[name] = std::vector<Vector2>(values.begin(), values.begin() + kMaxUniformVectorArraySize);
        LOG_WARNING("Material::SetVector2Array: '" + name + "' 超出最大uniform数组长度 "
                    + std::to_string(kMaxUniformVectorArraySize) + ", 已截断。");
    } else {
        m_vector2ArrayParams[name] = values;
    }
    InvalidateCacheLocked();
}

void Material::SetColorArray(const std::string& name, const std::vector<Color>& values) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (values.empty()) {
        m_colorArrayParams.erase(name);
        InvalidateCacheLocked();
        return;
    }

    if (values.size() > kMaxUniformVectorArraySize) {
        m_colorArrayParams[name] = std::vector<Color>(values.begin(), values.begin() + kMaxUniformVectorArraySize);
        LOG_WARNING("Material::SetColorArray: '" + name + "' 超出最大uniform数组长度 "
                    + std::to_string(kMaxUniformVectorArraySize) + ", 已截断。");
    } else {
        m_colorArrayParams[name] = values;
    }
    InvalidateCacheLocked();
}

void Material::SetMatrix4(const std::string& name, const Matrix4& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_matrix4Params[name] = value;
    InvalidateCacheLocked();
}

// ============================================================================
// 应用和绑定
// ============================================================================

void Material::Bind(RenderState* renderState) {
    std::shared_ptr<CachedState> snapshot;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_shader) {
            HANDLE_ERROR(RENDER_WARNING(ErrorCode::NullPointer,
                                       "Material::Bind: 材质 '" + m_name + "' 没有着色器"));
            return;
        }

        if (!m_shader->IsValid()) {
            HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidState,
                                       "Material::Bind: 材质 '" + m_name + "' 的着色器无效"));
            return;
        }

        snapshot = EnsureCachedStateLocked();
    }

    if (!snapshot || !snapshot->shader) {
        LOG_ERROR("Material::Bind: 缺少有效的着色器快照");
        return;
    }

    if (!snapshot->shader->IsValid()) {
        LOG_ERROR("Shader is invalid for material '" + snapshot->name + "'");
        return;
    }

    auto shader = snapshot->shader;
    shader->Use();

    if (renderState) {
        renderState->SetBlendMode(snapshot->blendMode);
        renderState->SetCullFace(snapshot->cullFace);
        renderState->SetDepthTest(snapshot->depthTest);
        renderState->SetDepthWrite(snapshot->depthWrite);
    }

    auto* uniformMgr = shader->GetUniformManager();
    if (!uniformMgr) {
        LOG_ERROR("UniformManager is null for material '" + snapshot->name + "'");
        return;
    }

    try {
        if (uniformMgr->HasUniform("uAmbientColor")) {
            uniformMgr->SetColor("uAmbientColor", snapshot->ambientColor);
        }
        if (uniformMgr->HasUniform("uDiffuseColor")) {
            uniformMgr->SetColor("uDiffuseColor", snapshot->diffuseColor);
        }
        if (uniformMgr->HasUniform("uSpecularColor")) {
            uniformMgr->SetColor("uSpecularColor", snapshot->specularColor);
        }
        if (uniformMgr->HasUniform("uShininess")) {
            uniformMgr->SetFloat("uShininess", snapshot->shininess);
        }

        if (uniformMgr->HasUniform("material.ambient")) {
            uniformMgr->SetColor("material.ambient", snapshot->ambientColor);
        }
        if (uniformMgr->HasUniform("material.diffuse")) {
            uniformMgr->SetColor("material.diffuse", snapshot->diffuseColor);
        }
        if (uniformMgr->HasUniform("material.specular")) {
            uniformMgr->SetColor("material.specular", snapshot->specularColor);
        }
        if (uniformMgr->HasUniform("material.emissive")) {
            uniformMgr->SetColor("material.emissive", snapshot->emissiveColor);
        }
        if (uniformMgr->HasUniform("material.shininess")) {
            uniformMgr->SetFloat("material.shininess", snapshot->shininess);
        }
        if (uniformMgr->HasUniform("material.opacity")) {
            uniformMgr->SetFloat("material.opacity", snapshot->opacity);
        }
        if (uniformMgr->HasUniform("material.metallic")) {
            uniformMgr->SetFloat("material.metallic", snapshot->metallic);
        }
        if (uniformMgr->HasUniform("material.roughness")) {
            uniformMgr->SetFloat("material.roughness", snapshot->roughness);
        }

        if (uniformMgr->HasUniform("uColor")) {
            uniformMgr->SetColor("uColor", snapshot->diffuseColor);
        }

        if (uniformMgr->HasUniform("uUseTexture")) {
            const bool hasTexture = std::any_of(snapshot->textures.begin(), snapshot->textures.end(),
                [](const auto& pair) {
                    return pair.first == "diffuseMap" || pair.first == "uTexture0";
                });
            uniformMgr->SetBool("uUseTexture", hasTexture);
        }

        if (uniformMgr->HasUniform("uUseVertexColor")) {
            uniformMgr->SetBool("uUseVertexColor", true);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Exception setting material properties: " + std::string(e.what()));
        return;
    }

    try {
        std::array<bool, 32> usedUnits{};
        bool hasDiffuse = false;
        bool hasNormal = false;

        for (const auto& pair : snapshot->textures) {
            const std::string& texName = pair.first;
            const auto& texture = pair.second;

            if (!texture || !texture->IsValid()) {
                continue;
            }

            int bindUnit = -1;
            int cachedUnit = -1;

            if (uniformMgr->TryGetTextureUnit(texName, cachedUnit)) {
                bindUnit = std::clamp(cachedUnit, 0, 31);
                uniformMgr->RegisterTextureUniform(texName, bindUnit);
            } else {
                auto it = std::find(usedUnits.begin(), usedUnits.end(), false);
                if (it == usedUnits.end()) {
                    HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidState,
                        "Material::Bind: 可用纹理单元耗尽，无法绑定 '" + texName + "'"));
                    continue;
                }
                bindUnit = static_cast<int>(std::distance(usedUnits.begin(), it));
                uniformMgr->RegisterTextureUniform(texName, bindUnit);
            }

            if (bindUnit >= 0 && bindUnit < static_cast<int>(usedUnits.size())) {
                usedUnits[bindUnit] = true;
            }

            texture->Bind(bindUnit);

            if (texName == "diffuseMap") {
                hasDiffuse = true;
            } else if (texName == "normalMap") {
                hasNormal = true;
            }
        }

        if (uniformMgr->HasUniform("hasDiffuseMap")) {
            uniformMgr->SetBool("hasDiffuseMap", hasDiffuse);
        }
        if (uniformMgr->HasUniform("hasNormalMap")) {
            uniformMgr->SetBool("hasNormalMap", hasNormal);
        }

        for (const auto& pair : snapshot->intParams) {
            if (uniformMgr->HasUniform(pair.first)) {
                uniformMgr->SetInt(pair.first, pair.second);
            }
        }

        for (const auto& pair : snapshot->floatParams) {
            if (uniformMgr->HasUniform(pair.first)) {
                uniformMgr->SetFloat(pair.first, pair.second);
            }
        }

        for (const auto& pair : snapshot->vector2Params) {
            if (uniformMgr->HasUniform(pair.first)) {
                uniformMgr->SetVector2(pair.first, pair.second);
            }
        }

        for (const auto& pair : snapshot->vector3Params) {
            if (uniformMgr->HasUniform(pair.first)) {
                uniformMgr->SetVector3(pair.first, pair.second);
            }
        }

        for (const auto& pair : snapshot->vector4Params) {
            if (uniformMgr->HasUniform(pair.first)) {
                uniformMgr->SetVector4(pair.first, pair.second);
            }
        }

        for (const auto& pair : snapshot->matrix4Params) {
            if (uniformMgr->HasUniform(pair.first)) {
                uniformMgr->SetMatrix4(pair.first, pair.second);
            }
        }

        for (const auto& pair : snapshot->vector2ArrayParams) {
            if (!pair.second.empty() && uniformMgr->HasUniform(pair.first)) {
                uniformMgr->SetVector2Array(pair.first, pair.second.data(),
                    static_cast<uint32_t>(pair.second.size()));
            }
        }

        for (const auto& pair : snapshot->colorArrayParams) {
            if (!pair.second.empty() && uniformMgr->HasUniform(pair.first)) {
                uniformMgr->SetColorArray(pair.first, pair.second.data(),
                    static_cast<uint32_t>(pair.second.size()));
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Exception setting texture/custom parameters: " + std::string(e.what()));
    }
}

void Material::Unbind() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_shader) {
        m_shader->Unuse();
    }
}

void Material::ApplyRenderState(RenderState* renderState) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!renderState) {
        return;
    }
    
    renderState->SetBlendMode(m_blendMode);
    renderState->SetCullFace(m_cullFace);
    renderState->SetDepthTest(m_depthTest);
    renderState->SetDepthWrite(m_depthWrite);
}

bool Material::IsValid() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_shader && m_shader->IsValid();
}

// ============================================================================
// 渲染状态
// ============================================================================

void Material::SetBlendMode(BlendMode mode) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_blendMode = mode;
    InvalidateCacheLocked();
}

BlendMode Material::GetBlendMode() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_blendMode;
}

void Material::SetCullFace(CullFace mode) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cullFace = mode;
    InvalidateCacheLocked();
}

CullFace Material::GetCullFace() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cullFace;
}

void Material::SetDepthTest(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_depthTest = enable;
    InvalidateCacheLocked();
}

bool Material::GetDepthTest() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_depthTest;
}

void Material::SetDepthWrite(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_depthWrite = enable;
    InvalidateCacheLocked();
}

bool Material::GetDepthWrite() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_depthWrite;
}

} // namespace Render

