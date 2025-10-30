#include "render/resource_manager.h"
#include "render/logger.h"
#include <algorithm>

namespace Render {

ResourceManager& ResourceManager::GetInstance() {
    static ResourceManager instance;
    return instance;
}

void ResourceManager::BeginFrame() {
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_currentFrame;
}

// ============================================================================
// 纹理管理
// ============================================================================

bool ResourceManager::RegisterTexture(const std::string& name, Ref<Texture> texture) {
    if (!texture) {
        Logger::GetInstance().Error("ResourceManager: 尝试注册空纹理: " + name);
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_textures.find(name) != m_textures.end()) {
        Logger::GetInstance().Warning("ResourceManager: 纹理已存在: " + name);
        return false;
    }
    
    m_textures[name] = ResourceEntry<Texture>(texture, m_currentFrame);
    Logger::GetInstance().Debug("ResourceManager: 注册纹理: " + name);
    return true;
}

Ref<Texture> ResourceManager::GetTexture(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_textures.find(name);
    if (it != m_textures.end()) {
        it->second.lastAccessFrame = m_currentFrame;  // 更新访问帧
        return it->second.resource;
    }
    
    return nullptr;
}

bool ResourceManager::RemoveTexture(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_textures.find(name);
    if (it != m_textures.end()) {
        m_textures.erase(it);
        Logger::GetInstance().Debug("ResourceManager: 移除纹理: " + name);
        return true;
    }
    
    return false;
}

bool ResourceManager::HasTexture(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_textures.find(name) != m_textures.end();
}

// ============================================================================
// 网格管理
// ============================================================================

bool ResourceManager::RegisterMesh(const std::string& name, Ref<Mesh> mesh) {
    if (!mesh) {
        Logger::GetInstance().Error("ResourceManager: 尝试注册空网格: " + name);
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_meshes.find(name) != m_meshes.end()) {
        Logger::GetInstance().Warning("ResourceManager: 网格已存在: " + name);
        return false;
    }
    
    m_meshes[name] = ResourceEntry<Mesh>(mesh, m_currentFrame);
    Logger::GetInstance().Debug("ResourceManager: 注册网格: " + name);
    return true;
}

Ref<Mesh> ResourceManager::GetMesh(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_meshes.find(name);
    if (it != m_meshes.end()) {
        it->second.lastAccessFrame = m_currentFrame;  // 更新访问帧
        return it->second.resource;
    }
    
    return nullptr;
}

bool ResourceManager::RemoveMesh(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_meshes.find(name);
    if (it != m_meshes.end()) {
        m_meshes.erase(it);
        Logger::GetInstance().Debug("ResourceManager: 移除网格: " + name);
        return true;
    }
    
    return false;
}

bool ResourceManager::HasMesh(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_meshes.find(name) != m_meshes.end();
}

// ============================================================================
// 材质管理
// ============================================================================

bool ResourceManager::RegisterMaterial(const std::string& name, Ref<Material> material) {
    if (!material) {
        Logger::GetInstance().Error("ResourceManager: 尝试注册空材质: " + name);
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_materials.find(name) != m_materials.end()) {
        Logger::GetInstance().Warning("ResourceManager: 材质已存在: " + name);
        return false;
    }
    
    m_materials[name] = ResourceEntry<Material>(material, m_currentFrame);
    Logger::GetInstance().Debug("ResourceManager: 注册材质: " + name);
    return true;
}

Ref<Material> ResourceManager::GetMaterial(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_materials.find(name);
    if (it != m_materials.end()) {
        it->second.lastAccessFrame = m_currentFrame;  // 更新访问帧
        return it->second.resource;
    }
    
    return nullptr;
}

bool ResourceManager::RemoveMaterial(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_materials.find(name);
    if (it != m_materials.end()) {
        m_materials.erase(it);
        Logger::GetInstance().Debug("ResourceManager: 移除材质: " + name);
        return true;
    }
    
    return false;
}

bool ResourceManager::HasMaterial(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_materials.find(name) != m_materials.end();
}

// ============================================================================
// 着色器管理
// ============================================================================

bool ResourceManager::RegisterShader(const std::string& name, Ref<Shader> shader) {
    if (!shader) {
        Logger::GetInstance().Error("ResourceManager: 尝试注册空着色器: " + name);
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_shaders.find(name) != m_shaders.end()) {
        Logger::GetInstance().Warning("ResourceManager: 着色器已存在: " + name);
        return false;
    }
    
    m_shaders[name] = ResourceEntry<Shader>(shader, m_currentFrame);
    Logger::GetInstance().Debug("ResourceManager: 注册着色器: " + name);
    return true;
}

Ref<Shader> ResourceManager::GetShader(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_shaders.find(name);
    if (it != m_shaders.end()) {
        it->second.lastAccessFrame = m_currentFrame;  // 更新访问帧
        return it->second.resource;
    }
    
    return nullptr;
}

bool ResourceManager::RemoveShader(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_shaders.find(name);
    if (it != m_shaders.end()) {
        m_shaders.erase(it);
        Logger::GetInstance().Debug("ResourceManager: 移除着色器: " + name);
        return true;
    }
    
    return false;
}

bool ResourceManager::HasShader(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_shaders.find(name) != m_shaders.end();
}

// ============================================================================
// 批量操作
// ============================================================================

void ResourceManager::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_textures.clear();
    m_meshes.clear();
    m_materials.clear();
    m_shaders.clear();
    
    Logger::GetInstance().Info("ResourceManager: 清空所有资源");
}

void ResourceManager::ClearType(ResourceType type) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    switch (type) {
        case ResourceType::Texture:
            m_textures.clear();
            Logger::GetInstance().Info("ResourceManager: 清空所有纹理");
            break;
        case ResourceType::Mesh:
            m_meshes.clear();
            Logger::GetInstance().Info("ResourceManager: 清空所有网格");
            break;
        case ResourceType::Material:
            m_materials.clear();
            Logger::GetInstance().Info("ResourceManager: 清空所有材质");
            break;
        case ResourceType::Shader:
            m_shaders.clear();
            Logger::GetInstance().Info("ResourceManager: 清空所有着色器");
            break;
    }
}

size_t ResourceManager::CleanupUnused(uint32_t unusedFrames) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t cleanedCount = 0;
    
    // 阶段1: 标记待删除的资源
    std::vector<std::string> texturesToDelete;
    std::vector<std::string> meshesToDelete;
    std::vector<std::string> materialsToDelete;
    std::vector<std::string> shadersToDelete;
    
    // 标记纹理
    for (auto& [name, entry] : m_textures) {
        bool unused = (m_currentFrame - entry.lastAccessFrame) > unusedFrames;
        bool onlyManagerRef = entry.resource.use_count() == 1;
        
        if (unused && onlyManagerRef) {
            texturesToDelete.push_back(name);
            entry.markedForDeletion = true;
        }
    }
    
    // 标记网格
    for (auto& [name, entry] : m_meshes) {
        bool unused = (m_currentFrame - entry.lastAccessFrame) > unusedFrames;
        bool onlyManagerRef = entry.resource.use_count() == 1;
        
        if (unused && onlyManagerRef) {
            meshesToDelete.push_back(name);
            entry.markedForDeletion = true;
        }
    }
    
    // 标记材质
    for (auto& [name, entry] : m_materials) {
        bool unused = (m_currentFrame - entry.lastAccessFrame) > unusedFrames;
        bool onlyManagerRef = entry.resource.use_count() == 1;
        
        if (unused && onlyManagerRef) {
            materialsToDelete.push_back(name);
            entry.markedForDeletion = true;
        }
    }
    
    // 标记着色器
    for (auto& [name, entry] : m_shaders) {
        bool unused = (m_currentFrame - entry.lastAccessFrame) > unusedFrames;
        bool onlyManagerRef = entry.resource.use_count() == 1;
        
        if (unused && onlyManagerRef) {
            shadersToDelete.push_back(name);
            entry.markedForDeletion = true;
        }
    }
    
    // 阶段2: 再次检查并删除
    // 删除纹理
    for (const auto& name : texturesToDelete) {
        auto it = m_textures.find(name);
        if (it != m_textures.end() && it->second.markedForDeletion && it->second.resource.use_count() == 1) {
            Logger::GetInstance().Debug("ResourceManager: 清理未使用纹理: " + name + 
                " (已 " + std::to_string(m_currentFrame - it->second.lastAccessFrame) + " 帧未使用)");
            m_textures.erase(it);
            ++cleanedCount;
        }
    }
    
    // 删除网格
    for (const auto& name : meshesToDelete) {
        auto it = m_meshes.find(name);
        if (it != m_meshes.end() && it->second.markedForDeletion && it->second.resource.use_count() == 1) {
            Logger::GetInstance().Debug("ResourceManager: 清理未使用网格: " + name + 
                " (已 " + std::to_string(m_currentFrame - it->second.lastAccessFrame) + " 帧未使用)");
            m_meshes.erase(it);
            ++cleanedCount;
        }
    }
    
    // 删除材质
    for (const auto& name : materialsToDelete) {
        auto it = m_materials.find(name);
        if (it != m_materials.end() && it->second.markedForDeletion && it->second.resource.use_count() == 1) {
            Logger::GetInstance().Debug("ResourceManager: 清理未使用材质: " + name + 
                " (已 " + std::to_string(m_currentFrame - it->second.lastAccessFrame) + " 帧未使用)");
            m_materials.erase(it);
            ++cleanedCount;
        }
    }
    
    // 删除着色器
    for (const auto& name : shadersToDelete) {
        auto it = m_shaders.find(name);
        if (it != m_shaders.end() && it->second.markedForDeletion && it->second.resource.use_count() == 1) {
            Logger::GetInstance().Debug("ResourceManager: 清理未使用着色器: " + name + 
                " (已 " + std::to_string(m_currentFrame - it->second.lastAccessFrame) + " 帧未使用)");
            m_shaders.erase(it);
            ++cleanedCount;
        }
    }
    
    if (cleanedCount > 0) {
        Logger::GetInstance().Info("ResourceManager: 清理了 " + std::to_string(cleanedCount) + " 个未使用资源");
    }
    
    return cleanedCount;
}

size_t ResourceManager::CleanupUnusedType(ResourceType type, uint32_t unusedFrames) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t cleanedCount = 0;
    
    switch (type) {
        case ResourceType::Texture: {
            // 阶段1: 标记
            std::vector<std::string> toDelete;
            for (auto& [name, entry] : m_textures) {
                bool unused = (m_currentFrame - entry.lastAccessFrame) > unusedFrames;
                bool onlyManagerRef = entry.resource.use_count() == 1;
                
                if (unused && onlyManagerRef) {
                    toDelete.push_back(name);
                    entry.markedForDeletion = true;
                }
            }
            
            // 阶段2: 删除
            for (const auto& name : toDelete) {
                auto it = m_textures.find(name);
                if (it != m_textures.end() && it->second.markedForDeletion && it->second.resource.use_count() == 1) {
                    Logger::GetInstance().Debug("ResourceManager: 清理未使用纹理: " + name);
                    m_textures.erase(it);
                    ++cleanedCount;
                }
            }
            break;
        }
            
        case ResourceType::Mesh: {
            std::vector<std::string> toDelete;
            for (auto& [name, entry] : m_meshes) {
                bool unused = (m_currentFrame - entry.lastAccessFrame) > unusedFrames;
                bool onlyManagerRef = entry.resource.use_count() == 1;
                
                if (unused && onlyManagerRef) {
                    toDelete.push_back(name);
                    entry.markedForDeletion = true;
                }
            }
            
            for (const auto& name : toDelete) {
                auto it = m_meshes.find(name);
                if (it != m_meshes.end() && it->second.markedForDeletion && it->second.resource.use_count() == 1) {
                    Logger::GetInstance().Debug("ResourceManager: 清理未使用网格: " + name);
                    m_meshes.erase(it);
                    ++cleanedCount;
                }
            }
            break;
        }
            
        case ResourceType::Material: {
            std::vector<std::string> toDelete;
            for (auto& [name, entry] : m_materials) {
                bool unused = (m_currentFrame - entry.lastAccessFrame) > unusedFrames;
                bool onlyManagerRef = entry.resource.use_count() == 1;
                
                if (unused && onlyManagerRef) {
                    toDelete.push_back(name);
                    entry.markedForDeletion = true;
                }
            }
            
            for (const auto& name : toDelete) {
                auto it = m_materials.find(name);
                if (it != m_materials.end() && it->second.markedForDeletion && it->second.resource.use_count() == 1) {
                    Logger::GetInstance().Debug("ResourceManager: 清理未使用材质: " + name);
                    m_materials.erase(it);
                    ++cleanedCount;
                }
            }
            break;
        }
            
        case ResourceType::Shader: {
            std::vector<std::string> toDelete;
            for (auto& [name, entry] : m_shaders) {
                bool unused = (m_currentFrame - entry.lastAccessFrame) > unusedFrames;
                bool onlyManagerRef = entry.resource.use_count() == 1;
                
                if (unused && onlyManagerRef) {
                    toDelete.push_back(name);
                    entry.markedForDeletion = true;
                }
            }
            
            for (const auto& name : toDelete) {
                auto it = m_shaders.find(name);
                if (it != m_shaders.end() && it->second.markedForDeletion && it->second.resource.use_count() == 1) {
                    Logger::GetInstance().Debug("ResourceManager: 清理未使用着色器: " + name);
                    m_shaders.erase(it);
                    ++cleanedCount;
                }
            }
            break;
        }
    }
    
    if (cleanedCount > 0) {
        Logger::GetInstance().Info("ResourceManager: 清理了 " + std::to_string(cleanedCount) + " 个未使用资源");
    }
    
    return cleanedCount;
}

// ============================================================================
// 统计和监控
// ============================================================================

ResourceStats ResourceManager::GetStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    ResourceStats stats;
    stats.textureCount = m_textures.size();
    stats.meshCount = m_meshes.size();
    stats.materialCount = m_materials.size();
    stats.shaderCount = m_shaders.size();
    stats.totalCount = stats.textureCount + stats.meshCount + stats.materialCount + stats.shaderCount;
    
    // 计算纹理内存
    for (const auto& [name, entry] : m_textures) {
        stats.textureMemory += entry.resource->GetMemoryUsage();
    }
    
    // 计算网格内存
    for (const auto& [name, entry] : m_meshes) {
        stats.meshMemory += entry.resource->GetMemoryUsage();
    }
    
    stats.totalMemory = stats.textureMemory + stats.meshMemory;
    
    return stats;
}

long ResourceManager::GetReferenceCount(ResourceType type, const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    switch (type) {
        case ResourceType::Texture: {
            auto it = m_textures.find(name);
            return (it != m_textures.end()) ? it->second.resource.use_count() : 0;
        }
        case ResourceType::Mesh: {
            auto it = m_meshes.find(name);
            return (it != m_meshes.end()) ? it->second.resource.use_count() : 0;
        }
        case ResourceType::Material: {
            auto it = m_materials.find(name);
            return (it != m_materials.end()) ? it->second.resource.use_count() : 0;
        }
        case ResourceType::Shader: {
            auto it = m_shaders.find(name);
            return (it != m_shaders.end()) ? it->second.resource.use_count() : 0;
        }
    }
    
    return 0;
}

void ResourceManager::PrintStatistics() const {
    auto stats = GetStats();
    
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("资源管理器统计信息");
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("纹理数量: " + std::to_string(stats.textureCount));
    Logger::GetInstance().Info("网格数量: " + std::to_string(stats.meshCount));
    Logger::GetInstance().Info("材质数量: " + std::to_string(stats.materialCount));
    Logger::GetInstance().Info("着色器数量: " + std::to_string(stats.shaderCount));
    Logger::GetInstance().Info("总资源数量: " + std::to_string(stats.totalCount));
    Logger::GetInstance().Info("----------------------------------------");
    Logger::GetInstance().Info("纹理内存: " + std::to_string(stats.textureMemory / 1024) + " KB");
    Logger::GetInstance().Info("网格内存: " + std::to_string(stats.meshMemory / 1024) + " KB");
    Logger::GetInstance().Info("总内存: " + std::to_string(stats.totalMemory / 1024) + " KB");
    Logger::GetInstance().Info("========================================");
}

std::vector<std::string> ResourceManager::ListTextures() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::string> names;
    names.reserve(m_textures.size());
    for (const auto& [name, _] : m_textures) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::string> ResourceManager::ListMeshes() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::string> names;
    names.reserve(m_meshes.size());
    for (const auto& [name, _] : m_meshes) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::string> ResourceManager::ListMaterials() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::string> names;
    names.reserve(m_materials.size());
    for (const auto& [name, _] : m_materials) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::string> ResourceManager::ListShaders() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::string> names;
    names.reserve(m_shaders.size());
    for (const auto& [name, _] : m_shaders) {
        names.push_back(name);
    }
    return names;
}

// ============================================================================
// 高级功能
// ============================================================================

void ResourceManager::ForEachTexture(std::function<void(const std::string&, Ref<Texture>)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& [name, entry] : m_textures) {
        callback(name, entry.resource);
    }
}

void ResourceManager::ForEachMesh(std::function<void(const std::string&, Ref<Mesh>)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& [name, entry] : m_meshes) {
        callback(name, entry.resource);
    }
}

void ResourceManager::ForEachMaterial(std::function<void(const std::string&, Ref<Material>)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& [name, entry] : m_materials) {
        callback(name, entry.resource);
    }
}

void ResourceManager::ForEachShader(std::function<void(const std::string&, Ref<Shader>)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& [name, entry] : m_shaders) {
        callback(name, entry.resource);
    }
}

} // namespace Render

