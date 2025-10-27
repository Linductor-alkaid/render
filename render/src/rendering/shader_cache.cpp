#include "render/shader_cache.h"
#include "render/logger.h"

namespace Render {

ShaderCache& ShaderCache::GetInstance() {
    static ShaderCache instance;
    return instance;
}

std::shared_ptr<Shader> ShaderCache::LoadShader(const std::string& name,
                                                 const std::string& vertexPath,
                                                 const std::string& fragmentPath,
                                                 const std::string& geometryPath) {
    // 检查是否已缓存
    auto it = m_shaders.find(name);
    if (it != m_shaders.end()) {
        LOG_INFO("Shader '" + name + "' found in cache (RefCount: " + 
                 std::to_string(it->second.use_count()) + ")");
        return it->second;
    }
    
    // 创建新着色器
    LOG_INFO("Loading new shader: " + name);
    auto shader = std::make_shared<Shader>();
    
    if (!shader->LoadFromFile(vertexPath, fragmentPath, geometryPath)) {
        LOG_ERROR("Failed to load shader: " + name);
        return nullptr;
    }
    
    shader->SetName(name);
    
    // 添加到缓存
    m_shaders[name] = shader;
    LOG_INFO("Shader '" + name + "' cached successfully");
    
    return shader;
}

std::shared_ptr<Shader> ShaderCache::LoadShaderFromSource(const std::string& name,
                                                           const std::string& vertexSource,
                                                           const std::string& fragmentSource,
                                                           const std::string& geometrySource) {
    // 检查是否已缓存
    auto it = m_shaders.find(name);
    if (it != m_shaders.end()) {
        LOG_INFO("Shader '" + name + "' found in cache (RefCount: " + 
                 std::to_string(it->second.use_count()) + ")");
        return it->second;
    }
    
    // 创建新着色器
    LOG_INFO("Loading new shader from source: " + name);
    auto shader = std::make_shared<Shader>();
    
    if (!shader->LoadFromSource(vertexSource, fragmentSource, geometrySource)) {
        LOG_ERROR("Failed to load shader from source: " + name);
        return nullptr;
    }
    
    shader->SetName(name);
    
    // 添加到缓存
    m_shaders[name] = shader;
    LOG_INFO("Shader '" + name + "' cached successfully");
    
    return shader;
}

std::shared_ptr<Shader> ShaderCache::GetShader(const std::string& name) {
    auto it = m_shaders.find(name);
    if (it != m_shaders.end()) {
        return it->second;
    }
    
    LOG_WARNING("Shader '" + name + "' not found in cache");
    return nullptr;
}

bool ShaderCache::ReloadShader(const std::string& name) {
    auto it = m_shaders.find(name);
    if (it == m_shaders.end()) {
        LOG_WARNING("Cannot reload shader '" + name + "': not found in cache");
        return false;
    }
    
    LOG_INFO("Reloading shader: " + name);
    return it->second->Reload();
}

void ShaderCache::ReloadAll() {
    LOG_INFO("Reloading all shaders (" + std::to_string(m_shaders.size()) + " shaders)...");
    
    size_t successCount = 0;
    size_t failCount = 0;
    
    for (auto& pair : m_shaders) {
        if (pair.second->Reload()) {
            successCount++;
        } else {
            failCount++;
            LOG_WARNING("Failed to reload shader: " + pair.first);
        }
    }
    
    LOG_INFO("Reload complete: " + std::to_string(successCount) + " succeeded, " + 
             std::to_string(failCount) + " failed");
}

void ShaderCache::RemoveShader(const std::string& name) {
    auto it = m_shaders.find(name);
    if (it != m_shaders.end()) {
        LOG_INFO("Removing shader from cache: " + name + 
                 " (RefCount before removal: " + std::to_string(it->second.use_count()) + ")");
        m_shaders.erase(it);
    }
}

void ShaderCache::Clear() {
    LOG_INFO("Clearing shader cache (" + std::to_string(m_shaders.size()) + " shaders)");
    m_shaders.clear();
}

long ShaderCache::GetReferenceCount(const std::string& name) const {
    auto it = m_shaders.find(name);
    if (it != m_shaders.end()) {
        return it->second.use_count();
    }
    return 0;
}

void ShaderCache::PrintStatistics() const {
    LOG_INFO("========================================");
    LOG_INFO("Shader Cache Statistics");
    LOG_INFO("========================================");
    LOG_INFO("Total shaders in cache: " + std::to_string(m_shaders.size()));
    
    if (!m_shaders.empty()) {
        LOG_INFO("Shader details:");
        for (const auto& pair : m_shaders) {
            LOG_INFO("  - " + pair.first + 
                     " (ID: " + std::to_string(pair.second->GetProgramID()) + 
                     ", RefCount: " + std::to_string(pair.second.use_count()) + ")");
        }
    }
    
    LOG_INFO("========================================");
}

size_t ShaderCache::PrecompileShaders(const std::vector<std::tuple<std::string, std::string, std::string, std::string>>& shaderList) {
    LOG_INFO("========================================");
    LOG_INFO("Precompiling " + std::to_string(shaderList.size()) + " shaders...");
    LOG_INFO("========================================");
    
    size_t successCount = 0;
    
    for (const auto& shaderDef : shaderList) {
        const std::string& name = std::get<0>(shaderDef);
        const std::string& vertPath = std::get<1>(shaderDef);
        const std::string& fragPath = std::get<2>(shaderDef);
        const std::string& geomPath = std::get<3>(shaderDef);
        
        auto shader = LoadShader(name, vertPath, fragPath, geomPath);
        if (shader) {
            successCount++;
        }
    }
    
    LOG_INFO("Precompilation complete: " + std::to_string(successCount) + "/" + 
             std::to_string(shaderList.size()) + " shaders loaded successfully");
    
    return successCount;
}

} // namespace Render

