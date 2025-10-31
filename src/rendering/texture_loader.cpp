#include "render/texture_loader.h"
#include "render/logger.h"
#include "render/error.h"
#include <algorithm>

namespace Render {

TextureLoader& TextureLoader::GetInstance() {
    static TextureLoader instance;
    return instance;
}

TexturePtr TextureLoader::LoadTexture(const std::string& name,
                                      const std::string& filepath,
                                      bool generateMipmap) {
    // 第一次检查缓存
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_textures.find(name);
        if (it != m_textures.end()) {
            Logger::GetInstance().Info("纹理 '" + name + "' 从缓存中获取 (引用计数: " + 
                     std::to_string(it->second.use_count()) + ")");
            return it->second;
        }
    }
    
    // 在锁外加载纹理（避免长时间持锁）
    Logger::GetInstance().Info("加载新纹理: " + name + " (路径: " + filepath + ")");
    
    auto texture = LoadTextureInternal(filepath, generateMipmap);
    if (!texture) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::TextureUploadFailed, 
                                 "TextureLoader: 加载纹理失败: " + name));
        return nullptr;
    }
    
    // 第二次检查并添加到缓存（双重检查锁定模式）
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // 再次检查是否已被其他线程添加
        auto it = m_textures.find(name);
        if (it != m_textures.end()) {
            Logger::GetInstance().Info("纹理 '" + name + "' 已被其他线程加载");
            return it->second;
        }
        
        m_textures[name] = texture;
        Logger::GetInstance().Info("纹理 '" + name + "' 缓存成功");
    }
    
    return texture;
}

TexturePtr TextureLoader::CreateTexture(const std::string& name,
                                       const void* data,
                                       int width,
                                       int height,
                                       TextureFormat format,
                                       bool generateMipmap) {
    // 第一次检查缓存
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_textures.find(name);
        if (it != m_textures.end()) {
            Logger::GetInstance().Info("纹理 '" + name + "' 从缓存中获取 (引用计数: " + 
                     std::to_string(it->second.use_count()) + ")");
            return it->second;
        }
    }
    
    // 在锁外创建纹理（避免长时间持锁）
    Logger::GetInstance().Info("创建新纹理: " + name + " (" + 
                std::to_string(width) + "x" + std::to_string(height) + ")");
    
    auto texture = std::make_shared<Texture>();
    if (!texture->CreateFromData(data, width, height, format, generateMipmap)) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::TextureUploadFailed, 
                                 "TextureLoader: 创建纹理失败: " + name));
        return nullptr;
    }
    
    // 第二次检查并添加到缓存
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // 再次检查是否已被其他线程添加
        auto it = m_textures.find(name);
        if (it != m_textures.end()) {
            Logger::GetInstance().Info("纹理 '" + name + "' 已被其他线程添加");
            return it->second;
        }
        
        m_textures[name] = texture;
        Logger::GetInstance().Info("纹理 '" + name + "' 缓存成功");
    }
    
    return texture;
}

std::future<AsyncTextureResult> TextureLoader::LoadTextureAsync(const std::string& name,
                                                                 const std::string& filepath,
                                                                 bool generateMipmap) {
    // 检查是否已缓存（在异步开始前）
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_textures.find(name);
        if (it != m_textures.end()) {
            Logger::GetInstance().Info("纹理 '" + name + "' 从缓存中获取（异步请求）");
            
            // 立即返回已缓存的结果
            AsyncTextureResult result;
            result.success = true;
            result.texture = it->second;
            result.error = "";
            
            return std::async(std::launch::deferred, [result]() { return result; });
        }
    }
    
    // 异步加载
    Logger::GetInstance().Info("开始异步加载纹理: " + name + " (路径: " + filepath + ")");
    
    return std::async(std::launch::async, [this, name, filepath, generateMipmap]() {
        AsyncTextureResult result;
        
        // 加载纹理
        auto texture = LoadTextureInternal(filepath, generateMipmap);
        
        if (texture) {
            // 添加到缓存（需要加锁）
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                
                // 再次检查是否已被其他线程添加
                auto it = m_textures.find(name);
                if (it != m_textures.end()) {
                    Logger::GetInstance().Info("纹理 '" + name + "' 已被其他线程加载");
                    result.texture = it->second;
                } else {
                    m_textures[name] = texture;
                    result.texture = texture;
                    Logger::GetInstance().Info("纹理 '" + name + "' 异步加载完成并缓存");
                }
            }
            
            result.success = true;
            result.error = "";
        } else {
            result.success = false;
            result.texture = nullptr;
            result.error = "无法加载纹理文件: " + filepath;
            Logger::GetInstance().Error("异步加载纹理失败: " + name);
        }
        
        return result;
    });
}

TexturePtr TextureLoader::GetTexture(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_textures.find(name);
    if (it != m_textures.end()) {
        return it->second;
    }
    
    Logger::GetInstance().Warning("纹理 '" + name + "' 未在缓存中找到");
    return nullptr;
}

bool TextureLoader::HasTexture(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_textures.find(name) != m_textures.end();
}

bool TextureLoader::RemoveTexture(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_textures.find(name);
    if (it == m_textures.end()) {
        Logger::GetInstance().Warning("无法移除纹理 '" + name + "': 未在缓存中找到");
        return false;
    }
    
    Logger::GetInstance().Info("从缓存中移除纹理: " + name + 
                " (引用计数: " + std::to_string(it->second.use_count()) + ")");
    m_textures.erase(it);
    
    return true;
}

void TextureLoader::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Logger::GetInstance().Info("清空纹理缓存 (" + std::to_string(m_textures.size()) + " 个纹理)");
    m_textures.clear();
}

size_t TextureLoader::GetTextureCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_textures.size();
}

long TextureLoader::GetReferenceCount(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_textures.find(name);
    if (it != m_textures.end()) {
        return it->second.use_count();
    }
    
    return 0;
}

void TextureLoader::PrintStatistics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 计算总内存使用（内部实现，避免重复加锁）
    size_t totalBytes = 0;
    for (const auto& pair : m_textures) {
        const TexturePtr& texture = pair.second;
        int width = texture->GetWidth();
        int height = texture->GetHeight();
        size_t baseSize = width * height * 4;
        size_t totalSize = baseSize * 4 / 3; // 包含 Mipmap
        totalBytes += totalSize;
    }
    
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("纹理缓存统计信息");
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("缓存纹理数量: " + std::to_string(m_textures.size()));
    Logger::GetInstance().Info("总内存使用量（估算）: " + 
                std::to_string(totalBytes / 1024 / 1024) + " MB");
    
    if (!m_textures.empty()) {
        Logger::GetInstance().Info("----------------------------------------");
        Logger::GetInstance().Info("纹理详情:");
        
        for (const auto& pair : m_textures) {
            const std::string& name = pair.first;
            const TexturePtr& texture = pair.second;
            
            long refCount = pair.second.use_count();
            int width = texture->GetWidth();
            int height = texture->GetHeight();
            size_t memSize = width * height * 4; // 估算为 RGBA
            
            Logger::GetInstance().Info("  - " + name + ": " + 
                        std::to_string(width) + "x" + std::to_string(height) + 
                        ", 引用计数: " + std::to_string(refCount) + 
                        ", 内存: ~" + std::to_string(memSize / 1024) + " KB");
        }
    }
    
    Logger::GetInstance().Info("========================================");
}

size_t TextureLoader::PreloadTextures(const std::vector<std::tuple<std::string, std::string, bool>>& textureList) {
    Logger::GetInstance().Info("预加载 " + std::to_string(textureList.size()) + " 个纹理...");
    
    size_t successCount = 0;
    size_t failCount = 0;
    
    for (const auto& texDef : textureList) {
        const std::string& name = std::get<0>(texDef);
        const std::string& filepath = std::get<1>(texDef);
        bool generateMipmap = std::get<2>(texDef);
        
        if (LoadTexture(name, filepath, generateMipmap)) {
            successCount++;
        } else {
            failCount++;
        }
    }
    
    Logger::GetInstance().Info("预加载完成: 成功 " + std::to_string(successCount) + 
                ", 失败 " + std::to_string(failCount));
    
    return successCount;
}

size_t TextureLoader::CleanupUnused() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t removedCount = 0;
    
    auto it = m_textures.begin();
    while (it != m_textures.end()) {
        // 引用计数为 1 表示只被缓存持有，没有外部引用
        if (it->second.use_count() == 1) {
            Logger::GetInstance().Debug("清理未使用的纹理: " + it->first);
            it = m_textures.erase(it);
            removedCount++;
        } else {
            ++it;
        }
    }
    
    if (removedCount > 0) {
        Logger::GetInstance().Info("清理了 " + std::to_string(removedCount) + " 个未使用的纹理");
    }
    
    return removedCount;
}

size_t TextureLoader::GetTotalMemoryUsage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t totalBytes = 0;
    
    for (const auto& pair : m_textures) {
        const TexturePtr& texture = pair.second;
        int width = texture->GetWidth();
        int height = texture->GetHeight();
        
        // 估算内存使用（假设 RGBA 格式）
        size_t baseSize = width * height * 4;
        
        // 如果有 Mipmap，增加约 1/3 的内存
        // Mipmap 链: 原始 + 1/4 + 1/16 + ... ≈ 原始 * 1.33
        size_t totalSize = baseSize * 4 / 3;
        
        totalBytes += totalSize;
    }
    
    return totalBytes;
}

TexturePtr TextureLoader::LoadTextureInternal(const std::string& filepath, bool generateMipmap) {
    auto texture = std::make_shared<Texture>();
    
    if (!texture->LoadFromFile(filepath, generateMipmap)) {
        return nullptr;
    }
    
    return texture;
}

} // namespace Render

