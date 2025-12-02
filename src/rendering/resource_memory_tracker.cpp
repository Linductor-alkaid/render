#include "render/resource_memory_tracker.h"
#include "render/texture.h"
#include "render/mesh.h"
#include "render/shader.h"
#include "render/logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Render {

ResourceMemoryTracker& ResourceMemoryTracker::GetInstance() {
    static ResourceMemoryTracker instance;
    return instance;
}

// ========================================================================
// 纹理追踪
// ========================================================================

void ResourceMemoryTracker::RegisterTexture(Texture* texture) {
    if (!texture) {
        return;
    }
    
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    // 避免重复注册
    if (m_textures.find(texture) != m_textures.end()) {
        return;
    }
    
    TextureEntry entry;
    entry.texture = texture;
    entry.memorySize = CalculateTextureMemory(texture);
    entry.name = "texture_" + std::to_string(reinterpret_cast<uintptr_t>(texture));
    entry.width = texture->GetWidth();
    entry.height = texture->GetHeight();
    
    m_textures[texture] = entry;
    
    LOG_DEBUG_F("ResourceMemoryTracker: 注册纹理 %s (%.2f MB, %ux%u)",
                entry.name.c_str(),
                entry.memorySize / (1024.0f * 1024.0f),
                entry.width,
                entry.height);
}

void ResourceMemoryTracker::UnregisterTexture(Texture* texture) {
    if (!texture) {
        return;
    }
    
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    auto it = m_textures.find(texture);
    if (it != m_textures.end()) {
        LOG_DEBUG_F("ResourceMemoryTracker: 注销纹理 %s (%.2f MB)",
                    it->second.name.c_str(),
                    it->second.memorySize / (1024.0f * 1024.0f));
        m_textures.erase(it);
    }
}

std::vector<ResourceInfo> ResourceMemoryTracker::GetTextureInfoList() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    
    std::vector<ResourceInfo> infos;
    infos.reserve(m_textures.size());
    
    for (const auto& [texture, entry] : m_textures) {
        ResourceInfo info;
        info.name = entry.name;
        info.memorySize = entry.memorySize;
        info.width = entry.width;
        info.height = entry.height;
        infos.push_back(info);
    }
    
    // 按内存大小降序排序
    std::sort(infos.begin(), infos.end(), [](const ResourceInfo& a, const ResourceInfo& b) {
        return a.memorySize > b.memorySize;
    });
    
    return infos;
}

// ========================================================================
// 网格追踪
// ========================================================================

void ResourceMemoryTracker::RegisterMesh(Mesh* mesh) {
    if (!mesh) {
        return;
    }
    
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    // 避免重复注册
    if (m_meshes.find(mesh) != m_meshes.end()) {
        return;
    }
    
    MeshEntry entry;
    entry.mesh = mesh;
    entry.memorySize = CalculateMeshMemory(mesh);
    entry.name = "mesh_" + std::to_string(reinterpret_cast<uintptr_t>(mesh));
    entry.vertexCount = mesh->GetVertexCount();
    entry.indexCount = mesh->GetIndexCount();
    
    m_meshes[mesh] = entry;
    
    LOG_DEBUG_F("ResourceMemoryTracker: 注册网格 %s (%.2f KB, %u vertices, %u indices)",
                entry.name.c_str(),
                entry.memorySize / 1024.0f,
                entry.vertexCount,
                entry.indexCount);
}

void ResourceMemoryTracker::UnregisterMesh(Mesh* mesh) {
    if (!mesh) {
        return;
    }
    
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    auto it = m_meshes.find(mesh);
    if (it != m_meshes.end()) {
        LOG_DEBUG_F("ResourceMemoryTracker: 注销网格 %s (%.2f KB)",
                    it->second.name.c_str(),
                    it->second.memorySize / 1024.0f);
        m_meshes.erase(it);
    }
}

std::vector<ResourceInfo> ResourceMemoryTracker::GetMeshInfoList() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    
    std::vector<ResourceInfo> infos;
    infos.reserve(m_meshes.size());
    
    for (const auto& [mesh, entry] : m_meshes) {
        ResourceInfo info;
        info.name = entry.name;
        info.memorySize = entry.memorySize;
        info.vertexCount = entry.vertexCount;
        info.indexCount = entry.indexCount;
        infos.push_back(info);
    }
    
    // 按内存大小降序排序
    std::sort(infos.begin(), infos.end(), [](const ResourceInfo& a, const ResourceInfo& b) {
        return a.memorySize > b.memorySize;
    });
    
    return infos;
}

// ========================================================================
// 着色器追踪
// ========================================================================

void ResourceMemoryTracker::RegisterShader(Shader* shader) {
    if (!shader) {
        return;
    }
    
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    // 避免重复注册
    if (m_shaders.find(shader) != m_shaders.end()) {
        return;
    }
    
    ShaderEntry entry;
    entry.shader = shader;
    entry.memorySize = CalculateShaderMemory(shader);
    entry.name = "shader_" + std::to_string(shader->GetProgramID());
    
    m_shaders[shader] = entry;
    
    LOG_DEBUG_F("ResourceMemoryTracker: 注册着色器 %s (%.2f KB)",
                entry.name.c_str(),
                entry.memorySize / 1024.0f);
}

void ResourceMemoryTracker::UnregisterShader(Shader* shader) {
    if (!shader) {
        return;
    }
    
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    auto it = m_shaders.find(shader);
    if (it != m_shaders.end()) {
        LOG_DEBUG_F("ResourceMemoryTracker: 注销着色器 %s (%.2f KB)",
                    it->second.name.c_str(),
                    it->second.memorySize / 1024.0f);
        m_shaders.erase(it);
    }
}

std::vector<ResourceInfo> ResourceMemoryTracker::GetShaderInfoList() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    
    std::vector<ResourceInfo> infos;
    infos.reserve(m_shaders.size());
    
    for (const auto& [shader, entry] : m_shaders) {
        ResourceInfo info;
        info.name = entry.name;
        info.memorySize = entry.memorySize;
        infos.push_back(info);
    }
    
    return infos;
}

// ========================================================================
// GPU 缓冲追踪
// ========================================================================

void ResourceMemoryTracker::RegisterBuffer(uint32_t bufferId, size_t size, const std::string& name) {
    if (bufferId == 0) {
        return;
    }
    
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    BufferEntry entry;
    entry.bufferId = bufferId;
    entry.memorySize = size;
    entry.name = name.empty() ? ("buffer_" + std::to_string(bufferId)) : name;
    
    m_buffers[bufferId] = entry;
    
    LOG_DEBUG_F("ResourceMemoryTracker: 注册缓冲 %s (%.2f KB)",
                entry.name.c_str(),
                entry.memorySize / 1024.0f);
}

void ResourceMemoryTracker::UnregisterBuffer(uint32_t bufferId) {
    if (bufferId == 0) {
        return;
    }
    
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    auto it = m_buffers.find(bufferId);
    if (it != m_buffers.end()) {
        LOG_DEBUG_F("ResourceMemoryTracker: 注销缓冲 %s (%.2f KB)",
                    it->second.name.c_str(),
                    it->second.memorySize / 1024.0f);
        m_buffers.erase(it);
    }
}

std::vector<ResourceInfo> ResourceMemoryTracker::GetBufferInfoList() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    
    std::vector<ResourceInfo> infos;
    infos.reserve(m_buffers.size());
    
    for (const auto& [bufferId, entry] : m_buffers) {
        ResourceInfo info;
        info.name = entry.name;
        info.memorySize = entry.memorySize;
        infos.push_back(info);
    }
    
    // 按内存大小降序排序
    std::sort(infos.begin(), infos.end(), [](const ResourceInfo& a, const ResourceInfo& b) {
        return a.memorySize > b.memorySize;
    });
    
    return infos;
}

// ========================================================================
// 统计信息
// ========================================================================

ResourceMemoryStats ResourceMemoryTracker::GetStats() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    
    ResourceMemoryStats stats;
    
    // 纹理统计
    for (const auto& [texture, entry] : m_textures) {
        stats.textureMemory += entry.memorySize;
        stats.textureCount++;
    }
    
    // 网格统计
    for (const auto& [mesh, entry] : m_meshes) {
        stats.meshMemory += entry.memorySize;
        stats.meshCount++;
    }
    
    // 着色器统计
    for (const auto& [shader, entry] : m_shaders) {
        stats.shaderMemory += entry.memorySize;
        stats.shaderCount++;
    }
    
    // 缓冲统计
    for (const auto& [bufferId, entry] : m_buffers) {
        stats.bufferMemory += entry.memorySize;
        stats.bufferCount++;
    }
    
    stats.UpdateTotal();
    
    return stats;
}

bool ResourceMemoryTracker::GenerateReport(const std::string& outputPath) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    
    std::ofstream file(outputPath);
    if (!file.is_open()) {
        LOG_ERROR_F("ResourceMemoryTracker: 无法打开文件 %s", outputPath.c_str());
        return false;
    }
    
    auto stats = GetStats();
    
    // 生成 JSON 格式报告
    file << "{\n";
    file << "  \"summary\": {\n";
    file << "    \"totalMemory\": " << stats.totalMemory << ",\n";
    file << "    \"totalMemoryMB\": " << (stats.totalMemory / (1024.0f * 1024.0f)) << ",\n";
    file << "    \"textureMemory\": " << stats.textureMemory << ",\n";
    file << "    \"meshMemory\": " << stats.meshMemory << ",\n";
    file << "    \"shaderMemory\": " << stats.shaderMemory << ",\n";
    file << "    \"bufferMemory\": " << stats.bufferMemory << ",\n";
    file << "    \"textureCount\": " << stats.textureCount << ",\n";
    file << "    \"meshCount\": " << stats.meshCount << ",\n";
    file << "    \"shaderCount\": " << stats.shaderCount << ",\n";
    file << "    \"bufferCount\": " << stats.bufferCount << "\n";
    file << "  },\n";
    
    // 纹理列表
    file << "  \"textures\": [\n";
    bool first = true;
    for (const auto& [texture, entry] : m_textures) {
        if (!first) file << ",\n";
        file << "    {\n";
        file << "      \"name\": \"" << entry.name << "\",\n";
        file << "      \"size\": " << entry.memorySize << ",\n";
        file << "      \"width\": " << entry.width << ",\n";
        file << "      \"height\": " << entry.height << "\n";
        file << "    }";
        first = false;
    }
    file << "\n  ],\n";
    
    // 网格列表
    file << "  \"meshes\": [\n";
    first = true;
    for (const auto& [mesh, entry] : m_meshes) {
        if (!first) file << ",\n";
        file << "    {\n";
        file << "      \"name\": \"" << entry.name << "\",\n";
        file << "      \"size\": " << entry.memorySize << ",\n";
        file << "      \"vertexCount\": " << entry.vertexCount << ",\n";
        file << "      \"indexCount\": " << entry.indexCount << "\n";
        file << "    }";
        first = false;
    }
    file << "\n  ],\n";
    
    // 着色器列表
    file << "  \"shaders\": [\n";
    first = true;
    for (const auto& [shader, entry] : m_shaders) {
        if (!first) file << ",\n";
        file << "    {\n";
        file << "      \"name\": \"" << entry.name << "\",\n";
        file << "      \"size\": " << entry.memorySize << "\n";
        file << "    }";
        first = false;
    }
    file << "\n  ],\n";
    
    // 缓冲列表
    file << "  \"buffers\": [\n";
    first = true;
    for (const auto& [bufferId, entry] : m_buffers) {
        if (!first) file << ",\n";
        file << "    {\n";
        file << "      \"name\": \"" << entry.name << "\",\n";
        file << "      \"id\": " << entry.bufferId << ",\n";
        file << "      \"size\": " << entry.memorySize << "\n";
        file << "    }";
        first = false;
    }
    file << "\n  ]\n";
    
    file << "}\n";
    file.close();
    
    LOG_INFO_F("ResourceMemoryTracker: 报告已生成 -> %s", outputPath.c_str());
    return true;
}

void ResourceMemoryTracker::Reset() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    LOG_INFO("ResourceMemoryTracker: 重置所有资源追踪");
    m_textures.clear();
    m_meshes.clear();
    m_shaders.clear();
    m_buffers.clear();
}

std::vector<std::string> ResourceMemoryTracker::DetectLeaks() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    
    std::vector<std::string> leaks;
    
    // 简单泄漏检测：列出所有未注销的资源
    // 注意：这是一个简化版本，实际泄漏检测需要更复杂的逻辑
    
    if (!m_textures.empty()) {
        for (const auto& [texture, entry] : m_textures) {
            leaks.push_back("Texture: " + entry.name + " (" + 
                          std::to_string(entry.memorySize / 1024.0f) + " KB)");
        }
    }
    
    if (!m_meshes.empty()) {
        for (const auto& [mesh, entry] : m_meshes) {
            leaks.push_back("Mesh: " + entry.name + " (" + 
                          std::to_string(entry.memorySize / 1024.0f) + " KB)");
        }
    }
    
    if (!m_shaders.empty()) {
        for (const auto& [shader, entry] : m_shaders) {
            leaks.push_back("Shader: " + entry.name);
        }
    }
    
    if (!m_buffers.empty()) {
        for (const auto& [bufferId, entry] : m_buffers) {
            leaks.push_back("Buffer: " + entry.name + " (ID: " + 
                          std::to_string(bufferId) + ", " +
                          std::to_string(entry.memorySize / 1024.0f) + " KB)");
        }
    }
    
    return leaks;
}

// ========================================================================
// 辅助函数
// ========================================================================

size_t ResourceMemoryTracker::CalculateTextureMemory(Texture* texture) const {
    if (!texture) {
        return 0;
    }
    
    uint32_t width = texture->GetWidth();
    uint32_t height = texture->GetHeight();
    
    // 假设 RGBA8 格式，每像素 4 字节
    // TODO: 根据实际格式计算（可以从 Texture 类获取）
    size_t pixelSize = 4;  // RGBA8
    size_t baseSize = static_cast<size_t>(width) * height * pixelSize;
    
    // 考虑 Mipmap（大约额外 33%）
    // TODO: 检查是否实际生成了 Mipmap
    size_t totalSize = baseSize + baseSize / 3;
    
    return totalSize;
}

size_t ResourceMemoryTracker::CalculateMeshMemory(Mesh* mesh) const {
    if (!mesh) {
        return 0;
    }
    
    // 顶点数据大小
    // 假设每个顶点包含：position(12) + texCoord(8) + normal(12) + color(16) = 48 字节
    size_t vertexSize = 48;  // sizeof(Vertex)
    size_t vertexMemory = mesh->GetVertexCount() * vertexSize;
    
    // 索引数据大小
    size_t indexSize = 4;  // sizeof(uint32_t)
    size_t indexMemory = mesh->GetIndexCount() * indexSize;
    
    return vertexMemory + indexMemory;
}

size_t ResourceMemoryTracker::CalculateShaderMemory(Shader* shader) const {
    if (!shader) {
        return 0;
    }
    
    // 着色器程序的内存占用很难精确计算
    // 这里返回一个粗略估计值（每个着色器约 10-50 KB）
    return 32 * 1024;  // 32 KB 估计值
}

} // namespace Render

