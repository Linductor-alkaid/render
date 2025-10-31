#include "render/shader_cache.h"
#include "render/logger.h"
#include "render/error.h"

namespace Render {

ShaderCache& ShaderCache::GetInstance() {
    static ShaderCache instance;
    return instance;
}

std::shared_ptr<Shader> ShaderCache::LoadShader(const std::string& name,
                                                 const std::string& vertexPath,
                                                 const std::string& fragmentPath,
                                                 const std::string& geometryPath) {
    // 先尝试读锁检查是否已缓存
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        auto it = m_shaders.find(name);
        if (it != m_shaders.end()) {
            LOG_INFO("Shader '" + name + "' found in cache (RefCount: " + 
                     std::to_string(it->second.use_count()) + ")");
            return it->second;
        }
    }
    
    // 创建新着色器（不需要锁，因为还没有加入缓存）
    LOG_INFO("Loading new shader: " + name);
    auto shader = std::make_shared<Shader>();
    
    if (!shader->LoadFromFile(vertexPath, fragmentPath, geometryPath)) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::ShaderCompileFailed, 
                                 "ShaderCache: 着色器加载失败: " + name));
        return nullptr;
    }
    
    shader->SetName(name);
    
    // 使用写锁添加到缓存
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        // 双重检查，防止其他线程已经加载了同名着色器
        auto it = m_shaders.find(name);
        if (it != m_shaders.end()) {
            LOG_INFO("Shader '" + name + "' was loaded by another thread");
            return it->second;
        }
        m_shaders[name] = shader;
        LOG_INFO("Shader '" + name + "' cached successfully");
    }
    
    return shader;
}

std::shared_ptr<Shader> ShaderCache::LoadShaderFromSource(const std::string& name,
                                                           const std::string& vertexSource,
                                                           const std::string& fragmentSource,
                                                           const std::string& geometrySource) {
    // 先尝试读锁检查是否已缓存
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        auto it = m_shaders.find(name);
        if (it != m_shaders.end()) {
            LOG_INFO("Shader '" + name + "' found in cache (RefCount: " + 
                     std::to_string(it->second.use_count()) + ")");
            return it->second;
        }
    }
    
    // 创建新着色器（不需要锁，因为还没有加入缓存）
    LOG_INFO("Loading new shader from source: " + name);
    auto shader = std::make_shared<Shader>();
    
    if (!shader->LoadFromSource(vertexSource, fragmentSource, geometrySource)) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::ShaderCompileFailed, 
                                 "ShaderCache: 从源码加载着色器失败: " + name));
        return nullptr;
    }
    
    shader->SetName(name);
    
    // 使用写锁添加到缓存
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        // 双重检查，防止其他线程已经加载了同名着色器
        auto it = m_shaders.find(name);
        if (it != m_shaders.end()) {
            LOG_INFO("Shader '" + name + "' was loaded by another thread");
            return it->second;
        }
        m_shaders[name] = shader;
        LOG_INFO("Shader '" + name + "' cached successfully");
    }
    
    return shader;
}

std::shared_ptr<Shader> ShaderCache::GetShader(const std::string& name) {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_shaders.find(name);
    if (it != m_shaders.end()) {
        return it->second;
    }
    
    HANDLE_ERROR(RENDER_WARNING(ErrorCode::ResourceNotFound, 
                                "ShaderCache: 着色器未找到: " + name));
    return nullptr;
}

bool ShaderCache::ReloadShader(const std::string& name) {
    std::shared_ptr<Shader> shader;
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        auto it = m_shaders.find(name);
        if (it == m_shaders.end()) {
            LOG_WARNING("Cannot reload shader '" + name + "': not found in cache");
            return false;
        }
        shader = it->second;
    }
    
    // 在锁外调用 Reload，因为 Reload 内部有自己的同步机制
    LOG_INFO("Reloading shader: " + name);
    return shader->Reload();
}

void ShaderCache::ReloadAll() {
    // 先复制一份着色器列表，避免长时间持有锁
    std::vector<std::pair<std::string, std::shared_ptr<Shader>>> shadersCopy;
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        LOG_INFO("Reloading all shaders (" + std::to_string(m_shaders.size()) + " shaders)...");
        shadersCopy.reserve(m_shaders.size());
        for (const auto& pair : m_shaders) {
            shadersCopy.push_back(pair);
        }
    }
    
    size_t successCount = 0;
    size_t failCount = 0;
    
    for (auto& pair : shadersCopy) {
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
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_shaders.find(name);
    if (it != m_shaders.end()) {
        LOG_INFO("Removing shader from cache: " + name + 
                 " (RefCount before removal: " + std::to_string(it->second.use_count()) + ")");
        m_shaders.erase(it);
    }
}

void ShaderCache::Clear() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    LOG_INFO("Clearing shader cache (" + std::to_string(m_shaders.size()) + " shaders)");
    m_shaders.clear();
}

long ShaderCache::GetReferenceCount(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_shaders.find(name);
    if (it != m_shaders.end()) {
        return it->second.use_count();
    }
    return 0;
}

void ShaderCache::PrintStatistics() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
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
    
    // LoadShader 内部已经有锁保护，这里不需要额外加锁
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

