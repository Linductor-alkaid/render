#include "render/material.h"
#include "render/shader.h"
#include "render/texture.h"
#include "render/render_state.h"
#include "render/logger.h"
#include "render/error.h"
#include <utility>
#include <map>
#include <unordered_map>

namespace Render {

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
    m_blendMode = other.m_blendMode;
    m_cullFace = other.m_cullFace;
    m_depthTest = other.m_depthTest;
    m_depthWrite = other.m_depthWrite;
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
        m_blendMode = other.m_blendMode;
        m_cullFace = other.m_cullFace;
        m_depthTest = other.m_depthTest;
        m_depthWrite = other.m_depthWrite;
    }
    return *this;
}

// ============================================================================
// 名称管理
// ============================================================================

void Material::SetName(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_name = name;
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
}

Color Material::GetAmbientColor() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_ambientColor;
}

void Material::SetDiffuseColor(const Color& color) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_diffuseColor = color;
}

Color Material::GetDiffuseColor() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_diffuseColor;
}

void Material::SetSpecularColor(const Color& color) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_specularColor = color;
}

Color Material::GetSpecularColor() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_specularColor;
}

void Material::SetEmissiveColor(const Color& color) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_emissiveColor = color;
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
}

float Material::GetShininess() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_shininess;
}

void Material::SetOpacity(float opacity) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_opacity = opacity;
}

float Material::GetOpacity() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_opacity;
}

void Material::SetMetallic(float metallic) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_metallic = metallic;
}

float Material::GetMetallic() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metallic;
}

void Material::SetRoughness(float roughness) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_roughness = roughness;
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
}

void Material::ClearTextures() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_textures.clear();
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
}

void Material::SetFloat(const std::string& name, float value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_floatParams[name] = value;
}

void Material::SetVector2(const std::string& name, const Vector2& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_vector2Params[name] = value;
}

void Material::SetVector3(const std::string& name, const Vector3& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_vector3Params[name] = value;
}

void Material::SetVector4(const std::string& name, const Vector4& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_vector4Params[name] = value;
}

void Material::SetColor(const std::string& name, const Color& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_vector4Params[name] = value.ToVector4();
}

void Material::SetMatrix4(const std::string& name, const Matrix4& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_matrix4Params[name] = value;
}

// ============================================================================
// 应用和绑定
// ============================================================================

void Material::Bind(RenderState* renderState) {
    // 在锁内快速验证和拷贝数据
    Ref<Shader> shader;
    Color ambientColor, diffuseColor, specularColor, emissiveColor;
    float shininess, opacity, metallic, roughness;
    std::unordered_map<std::string, Ref<Texture>> textures;
    std::unordered_map<std::string, int> intParams;
    std::unordered_map<std::string, float> floatParams;
    std::unordered_map<std::string, Vector2> vector2Params;
    std::unordered_map<std::string, Vector3> vector3Params;
    std::unordered_map<std::string, Vector4> vector4Params;
    std::unordered_map<std::string, Matrix4> matrix4Params;
    std::string name;
    
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
        
        // 拷贝所有需要的数据
        shader = m_shader;
        ambientColor = m_ambientColor;
        diffuseColor = m_diffuseColor;
        specularColor = m_specularColor;
        emissiveColor = m_emissiveColor;
        shininess = m_shininess;
        opacity = m_opacity;
        metallic = m_metallic;
        roughness = m_roughness;
        textures = m_textures;
        intParams = m_intParams;
        floatParams = m_floatParams;
        vector2Params = m_vector2Params;
        vector3Params = m_vector3Params;
        vector4Params = m_vector4Params;
        matrix4Params = m_matrix4Params;
        name = m_name;
    }
    
    // 在锁外执行OpenGL调用
    // 1. 激活着色器
    shader->Use();
    
    // 2. 获取 UniformManager
    auto* uniformMgr = shader->GetUniformManager();
    if (!uniformMgr) {
        LOG_ERROR("UniformManager is null for material '" + name + "'");
        return;
    }
    
    // 3. 设置材质颜色属性
    if (uniformMgr->HasUniform("material.ambient")) {
        uniformMgr->SetColor("material.ambient", ambientColor);
    }
    if (uniformMgr->HasUniform("material.diffuse")) {
        uniformMgr->SetColor("material.diffuse", diffuseColor);
    }
    if (uniformMgr->HasUniform("material.specular")) {
        uniformMgr->SetColor("material.specular", specularColor);
    }
    if (uniformMgr->HasUniform("material.emissive")) {
        uniformMgr->SetColor("material.emissive", emissiveColor);
    }
    
    // 4.输送材质物理属性
    if (uniformMgr->HasUniform("material.shininess")) {
        uniformMgr->SetFloat("material.shininess", shininess);
    }
    if (uniformMgr->HasUniform("material.opacity")) {
        uniformMgr->SetFloat("material.opacity", opacity);
    }
    if (uniformMgr->HasUniform("material.metallic")) {
        uniformMgr->SetFloat("material.metallic", metallic);
    }
    if (uniformMgr->HasUniform("material.roughness")) {
        uniformMgr->SetFloat("material.roughness", roughness);
    }
    
    // 5. 绑定纹理
    int textureUnit = 0;
    bool hasDiffuse = false;
    
    for (const auto& pair : textures) {
        const std::string& texName = pair.first;
        auto texture = pair.second;
        
        if (texture && texture->IsValid()) {
            // 绑定纹理到纹理单元
            texture->Bind(textureUnit);
            
            // 设置纹理单元 uniform
            if (uniformMgr->HasUniform(texName)) {
                uniformMgr->SetInt(texName, textureUnit);
            }
            
            // 标记纹理类型
            if (texName == "diffuseMap") {
                hasDiffuse = true;
            }
            
            textureUnit++;
        }
    }
    
    // 设置纹理存在标志
    if (uniformMgr->HasUniform("hasDiffuseMap")) {
        uniformMgr->SetBool("hasDiffuseMap", hasDiffuse);
    }
    
    // 6. 设置自定义整型参数
    for (const auto& pair : intParams) {
        if (uniformMgr->HasUniform(pair.first)) {
            uniformMgr->SetInt(pair.first, pair.second);
        }
    }
    
    // 7. 设置自定义浮点参数
    for (const auto& pair : floatParams) {
        if (uniformMgr->HasUniform(pair.first)) {
            uniformMgr->SetFloat(pair.first, pair.second);
        }
    }
    
    // 8. 设置自定义向量2参数
    for (const auto& pair : vector2Params) {
        if (uniformMgr->HasUniform(pair.first)) {
            uniformMgr->SetVector2(pair.first, pair.second);
        }
    }
    
    // 9. 设置自定义向量3参数
    for (const auto& pair : vector3Params) {
        if (uniformMgr->HasUniform(pair.first)) {
            uniformMgr->SetVector3(pair.first, pair.second);
        }
    }
    
    // 10. 设置自定义向量4参数
    for (const auto& pair : vector4Params) {
        if (uniformMgr->HasUniform(pair.first)) {
            uniformMgr->SetVector4(pair.first, pair.second);
        }
    }
    
    // 11. 设置自定义矩阵4参数
    for (const auto& pair : matrix4Params) {
        if (uniformMgr->HasUniform(pair.first)) {
            uniformMgr->SetMatrix4(pair.first, pair.second);
        }
    }
    
    // 12. 应用渲染状态
    if (renderState) {
        ApplyRenderState(renderState);
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
}

BlendMode Material::GetBlendMode() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_blendMode;
}

void Material::SetCullFace(CullFace mode) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cullFace = mode;
}

CullFace Material::GetCullFace() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cullFace;
}

void Material::SetDepthTest(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_depthTest = enable;
}

bool Material::GetDepthTest() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_depthTest;
}

void Material::SetDepthWrite(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_depthWrite = enable;
}

bool Material::GetDepthWrite() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_depthWrite;
}

} // namespace Render

