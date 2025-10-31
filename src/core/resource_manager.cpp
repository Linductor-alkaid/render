#include "render/resource_manager.h"
#include "render/logger.h"
#include "render/error.h"
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
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::NullPointer, 
                                 "ResourceManager: 尝试注册空纹理: " + name));
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_textures.find(name) != m_textures.end()) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::ResourceAlreadyExists, 
                                   "ResourceManager: 纹理已存在: " + name));
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
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::NullPointer, 
                                 "ResourceManager: 尝试注册空网格: " + name));
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_meshes.find(name) != m_meshes.end()) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::ResourceAlreadyExists, 
                                   "ResourceManager: 网格已存在: " + name));
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
    
    Logger::GetInstance().Info("ResourceManager: 开始清空所有资源");
    
    // 清理传统资源存储
    m_textures.clear();
    m_meshes.clear();
    m_materials.clear();
    m_shaders.clear();
    
    // 清理句柄系统
    m_textureSlots.Clear();
    m_meshSlots.Clear();
    m_materialSlots.Clear();
    m_shaderSlots.Clear();
    
    // 清理名称到句柄的映射
    m_textureHandles.clear();
    m_meshHandles.clear();
    m_materialHandles.clear();
    m_shaderHandles.clear();
    
    Logger::GetInstance().Info("ResourceManager: 所有资源已清空");
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

// ============================================================================
// 智能句柄系统
// ============================================================================

// 纹理句柄
TextureHandle ResourceManager::CreateTextureHandle(const std::string& name, Ref<Texture> texture) {
    if (!texture) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::NullPointer, 
                                 "ResourceManager: 尝试创建空纹理句柄: " + name));
        return TextureHandle();
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 检查是否已存在
    auto it = m_textureHandles.find(name);
    if (it != m_textureHandles.end()) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::ResourceAlreadyExists, 
                                   "ResourceManager: 纹理句柄已存在: " + name));
        return it->second;
    }
    
    // 创建句柄
    TextureHandle handle = m_textureSlots.Allocate(texture, name, m_currentFrame);
    m_textureHandles[name] = handle;
    
    Logger::GetInstance().Debug("ResourceManager: 创建纹理句柄: " + name + 
                               " (ID: " + std::to_string(handle.GetID()) + ")");
    return handle;
}

MeshHandle ResourceManager::CreateMeshHandle(const std::string& name, Ref<Mesh> mesh) {
    if (!mesh) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::NullPointer, 
                                 "ResourceManager: 尝试创建空网格句柄: " + name));
        return MeshHandle();
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_meshHandles.find(name);
    if (it != m_meshHandles.end()) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::ResourceAlreadyExists, 
                                   "ResourceManager: 网格句柄已存在: " + name));
        return it->second;
    }
    
    MeshHandle handle = m_meshSlots.Allocate(mesh, name, m_currentFrame);
    m_meshHandles[name] = handle;
    
    Logger::GetInstance().Debug("ResourceManager: 创建网格句柄: " + name + 
                               " (ID: " + std::to_string(handle.GetID()) + ")");
    return handle;
}

MaterialHandle ResourceManager::CreateMaterialHandle(const std::string& name, Ref<Material> material) {
    if (!material) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::NullPointer, 
                                 "ResourceManager: 尝试创建空材质句柄: " + name));
        return MaterialHandle();
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_materialHandles.find(name);
    if (it != m_materialHandles.end()) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::ResourceAlreadyExists, 
                                   "ResourceManager: 材质句柄已存在: " + name));
        return it->second;
    }
    
    MaterialHandle handle = m_materialSlots.Allocate(material, name, m_currentFrame);
    m_materialHandles[name] = handle;
    
    Logger::GetInstance().Debug("ResourceManager: 创建材质句柄: " + name + 
                               " (ID: " + std::to_string(handle.GetID()) + ")");
    return handle;
}

ShaderHandle ResourceManager::CreateShaderHandle(const std::string& name, Ref<Shader> shader) {
    if (!shader) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::NullPointer, 
                                 "ResourceManager: 尝试创建空着色器句柄: " + name));
        return ShaderHandle();
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_shaderHandles.find(name);
    if (it != m_shaderHandles.end()) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::ResourceAlreadyExists, 
                                   "ResourceManager: 着色器句柄已存在: " + name));
        return it->second;
    }
    
    ShaderHandle handle = m_shaderSlots.Allocate(shader, name, m_currentFrame);
    m_shaderHandles[name] = handle;
    
    Logger::GetInstance().Debug("ResourceManager: 创建着色器句柄: " + name + 
                               " (ID: " + std::to_string(handle.GetID()) + ")");
    return handle;
}

// 通过句柄获取资源
Texture* ResourceManager::GetTextureByHandle(const TextureHandle& handle) {
    m_textureSlots.UpdateAccessFrame(handle, m_currentFrame);
    return m_textureSlots.Get(handle);
}

Ref<Texture> ResourceManager::GetTextureSharedByHandle(const TextureHandle& handle) {
    m_textureSlots.UpdateAccessFrame(handle, m_currentFrame);
    return m_textureSlots.GetShared(handle);
}

bool ResourceManager::IsTextureHandleValid(const TextureHandle& handle) const {
    return m_textureSlots.IsValid(handle);
}

Mesh* ResourceManager::GetMeshByHandle(const MeshHandle& handle) {
    m_meshSlots.UpdateAccessFrame(handle, m_currentFrame);
    return m_meshSlots.Get(handle);
}

Ref<Mesh> ResourceManager::GetMeshSharedByHandle(const MeshHandle& handle) {
    m_meshSlots.UpdateAccessFrame(handle, m_currentFrame);
    return m_meshSlots.GetShared(handle);
}

bool ResourceManager::IsMeshHandleValid(const MeshHandle& handle) const {
    return m_meshSlots.IsValid(handle);
}

Material* ResourceManager::GetMaterialByHandle(const MaterialHandle& handle) {
    m_materialSlots.UpdateAccessFrame(handle, m_currentFrame);
    return m_materialSlots.Get(handle);
}

Ref<Material> ResourceManager::GetMaterialSharedByHandle(const MaterialHandle& handle) {
    m_materialSlots.UpdateAccessFrame(handle, m_currentFrame);
    return m_materialSlots.GetShared(handle);
}

bool ResourceManager::IsMaterialHandleValid(const MaterialHandle& handle) const {
    return m_materialSlots.IsValid(handle);
}

Shader* ResourceManager::GetShaderByHandle(const ShaderHandle& handle) {
    m_shaderSlots.UpdateAccessFrame(handle, m_currentFrame);
    return m_shaderSlots.Get(handle);
}

Ref<Shader> ResourceManager::GetShaderSharedByHandle(const ShaderHandle& handle) {
    m_shaderSlots.UpdateAccessFrame(handle, m_currentFrame);
    return m_shaderSlots.GetShared(handle);
}

bool ResourceManager::IsShaderHandleValid(const ShaderHandle& handle) const {
    return m_shaderSlots.IsValid(handle);
}

// 热重载
bool ResourceManager::ReloadTexture(const TextureHandle& handle, Ref<Texture> newTexture) {
    if (!newTexture) {
        return false;
    }
    
    bool result = m_textureSlots.Reload(handle, newTexture);
    
    if (result) {
        Logger::GetInstance().Info("ResourceManager: 热重载纹理 (ID: " + 
                                  std::to_string(handle.GetID()) + ")");
    }
    
    return result;
}

bool ResourceManager::ReloadMesh(const MeshHandle& handle, Ref<Mesh> newMesh) {
    if (!newMesh) {
        return false;
    }
    
    bool result = m_meshSlots.Reload(handle, newMesh);
    
    if (result) {
        Logger::GetInstance().Info("ResourceManager: 热重载网格 (ID: " + 
                                  std::to_string(handle.GetID()) + ")");
    }
    
    return result;
}

bool ResourceManager::ReloadMaterial(const MaterialHandle& handle, Ref<Material> newMaterial) {
    if (!newMaterial) {
        return false;
    }
    
    bool result = m_materialSlots.Reload(handle, newMaterial);
    
    if (result) {
        Logger::GetInstance().Info("ResourceManager: 热重载材质 (ID: " + 
                                  std::to_string(handle.GetID()) + ")");
    }
    
    return result;
}

bool ResourceManager::ReloadShader(const ShaderHandle& handle, Ref<Shader> newShader) {
    if (!newShader) {
        return false;
    }
    
    bool result = m_shaderSlots.Reload(handle, newShader);
    
    if (result) {
        Logger::GetInstance().Info("ResourceManager: 热重载着色器 (ID: " + 
                                  std::to_string(handle.GetID()) + ")");
    }
    
    return result;
}

// 通过句柄移除资源
bool ResourceManager::RemoveTextureByHandle(const TextureHandle& handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 从槽管理器中释放
    m_textureSlots.Free(handle);
    
    // 从名称映射中移除
    for (auto it = m_textureHandles.begin(); it != m_textureHandles.end(); ++it) {
        if (it->second == handle) {
            Logger::GetInstance().Debug("ResourceManager: 移除纹理句柄: " + it->first);
            m_textureHandles.erase(it);
            return true;
        }
    }
    
    return false;
}

bool ResourceManager::RemoveMeshByHandle(const MeshHandle& handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_meshSlots.Free(handle);
    
    for (auto it = m_meshHandles.begin(); it != m_meshHandles.end(); ++it) {
        if (it->second == handle) {
            Logger::GetInstance().Debug("ResourceManager: 移除网格句柄: " + it->first);
            m_meshHandles.erase(it);
            return true;
        }
    }
    
    return false;
}

bool ResourceManager::RemoveMaterialByHandle(const MaterialHandle& handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_materialSlots.Free(handle);
    
    for (auto it = m_materialHandles.begin(); it != m_materialHandles.end(); ++it) {
        if (it->second == handle) {
            Logger::GetInstance().Debug("ResourceManager: 移除材质句柄: " + it->first);
            m_materialHandles.erase(it);
            return true;
        }
    }
    
    return false;
}

bool ResourceManager::RemoveShaderByHandle(const ShaderHandle& handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_shaderSlots.Free(handle);
    
    for (auto it = m_shaderHandles.begin(); it != m_shaderHandles.end(); ++it) {
        if (it->second == handle) {
            Logger::GetInstance().Debug("ResourceManager: 移除着色器句柄: " + it->first);
            m_shaderHandles.erase(it);
            return true;
        }
    }
    
    return false;
}

// 统计信息
ResourceManager::HandleStats ResourceManager::GetHandleStats() const {
    HandleStats stats;
    
    stats.textureSlots = m_textureSlots.GetTotalSlots();
    stats.textureActiveSlots = m_textureSlots.GetActiveCount();
    stats.textureFreeSlots = m_textureSlots.GetFreeSlots();
    
    stats.meshSlots = m_meshSlots.GetTotalSlots();
    stats.meshActiveSlots = m_meshSlots.GetActiveCount();
    stats.meshFreeSlots = m_meshSlots.GetFreeSlots();
    
    stats.materialSlots = m_materialSlots.GetTotalSlots();
    stats.materialActiveSlots = m_materialSlots.GetActiveCount();
    stats.materialFreeSlots = m_materialSlots.GetFreeSlots();
    
    stats.shaderSlots = m_shaderSlots.GetTotalSlots();
    stats.shaderActiveSlots = m_shaderSlots.GetActiveCount();
    stats.shaderFreeSlots = m_shaderSlots.GetFreeSlots();
    
    return stats;
}

} // namespace Render

