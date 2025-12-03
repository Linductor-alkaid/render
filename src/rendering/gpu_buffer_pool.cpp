#include "render/gpu_buffer_pool.h"
#include "render/logger.h"
#include "render/gl_thread_checker.h"
#include <glad/glad.h>
#include <algorithm>

namespace Render {

// ========================================================================
// OpenGL 状态保存/恢复辅助类
// ========================================================================

/**
 * @brief OpenGL 缓冲绑定状态保存器
 * 
 * RAII类,自动保存和恢复OpenGL缓冲绑定状态
 * 这是解决HUD渲染问题的关键!
 */
class GLBufferBindingGuard {
public:
    explicit GLBufferBindingGuard(GLenum target) : m_target(target) {
        // 保存当前绑定的buffer
        GLenum bindingQuery = 0;
        switch (target) {
            case GL_ARRAY_BUFFER:
                bindingQuery = GL_ARRAY_BUFFER_BINDING;
                break;
            case GL_ELEMENT_ARRAY_BUFFER:
                bindingQuery = GL_ELEMENT_ARRAY_BUFFER_BINDING;
                break;
            case GL_UNIFORM_BUFFER:
                bindingQuery = GL_UNIFORM_BUFFER_BINDING;
                break;
            case GL_SHADER_STORAGE_BUFFER:
                bindingQuery = GL_SHADER_STORAGE_BUFFER_BINDING;
                break;
            default:
                m_savedBinding = 0;
                return;
        }
        
        GLint binding = 0;
        glGetIntegerv(bindingQuery, &binding);
        m_savedBinding = static_cast<GLuint>(binding);
    }
    
    ~GLBufferBindingGuard() {
        // 恢复之前的绑定
        if (m_target != 0) {
            glBindBuffer(m_target, m_savedBinding);
        }
    }
    
    // 禁止拷贝
    GLBufferBindingGuard(const GLBufferBindingGuard&) = delete;
    GLBufferBindingGuard& operator=(const GLBufferBindingGuard&) = delete;

private:
    GLenum m_target;
    GLuint m_savedBinding;
};

// ========================================================================
// GPUBufferPool 修复实现
// ========================================================================

GPUBufferPool& GPUBufferPool::GetInstance() {
    static GPUBufferPool instance;
    return instance;
}

GPUBufferPool::GPUBufferPool() {
    m_staticPool.reserve(32);
    m_dynamicPool.reserve(64);
    m_streamPool.reserve(16);
}

GPUBufferPool::~GPUBufferPool() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& entry : m_staticPool) {
        if (entry.bufferId != 0) {
            DeleteBuffer(entry.bufferId);
        }
    }
    for (auto& entry : m_dynamicPool) {
        if (entry.bufferId != 0) {
            DeleteBuffer(entry.bufferId);
        }
    }
    for (auto& entry : m_streamPool) {
        if (entry.bufferId != 0) {
            DeleteBuffer(entry.bufferId);
        }
    }
    
    m_staticPool.clear();
    m_dynamicPool.clear();
    m_streamPool.clear();
    m_bufferLookup.clear();
}

uint32_t GPUBufferPool::AcquireBuffer(const BufferDescriptor& desc) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    ++m_stats.acquireCount;
    
    // 检查内存限制 - 提前检查避免分配失败
    if (m_memoryLimit > 0) {
        auto currentStats = GetStats();
        if (currentStats.totalMemory + desc.size > m_memoryLimit) {
            // 尝试清理未使用的缓冲
            CleanupUnused(10);  // 清理10帧未使用的
            
            // 重新检查
            currentStats = GetStats();
            if (currentStats.totalMemory + desc.size > m_memoryLimit) {
                LOG_ERROR_F("GPUBufferPool: 内存不足,无法分配 %.2f KB", 
                            desc.size / 1024.0f);
                return 0;
            }
        }
    }
    
    // 查找可用的匹配缓冲
    PoolEntry* entry = FindAvailableBuffer(desc);
    
    if (entry) {
        entry->inUse = true;
        entry->lastUsedFrame = m_currentFrame;
        ++m_stats.reuseCount;
        
        LOG_DEBUG_F("GPUBufferPool: 复用缓冲 ID=%u (%.2f KB)",
                    entry->bufferId,
                    desc.size / 1024.0f);
        
        return entry->bufferId;
    }
    
    // 没有可用缓冲,创建新的
    uint32_t bufferId = CreateNewBuffer(desc);
    if (bufferId == 0) {
        LOG_ERROR("GPUBufferPool: 创建缓冲失败");
        return 0;
    }
    
    // 添加到池
    PoolEntry newEntry;
    newEntry.bufferId = bufferId;
    newEntry.desc = desc;
    newEntry.inUse = true;
    newEntry.lastUsedFrame = m_currentFrame;
    
    auto* pool = GetPoolForUsage(desc.usage);
    if (pool) {
        pool->push_back(newEntry);
        m_bufferLookup[bufferId] = &pool->back();
    }
    
    ++m_stats.createCount;
    
    LOG_DEBUG_F("GPUBufferPool: 创建新缓冲 ID=%u (%.2f KB)",
                bufferId,
                desc.size / 1024.0f);
    
    // 检查内存压力
    if (m_memoryPressureCallback) {
        auto currentStats = GetStats();
        bool exceeded = m_memoryLimit > 0 && currentStats.totalMemory > m_memoryLimit;
        
        if (exceeded) {
            LOG_WARNING_F("GPUBufferPool: 内存使用超限 (%.2f MB / %.2f MB)",
                          currentStats.totalMemory / (1024.0f * 1024.0f),
                          m_memoryLimit / (1024.0f * 1024.0f));
        }
        
        m_memoryPressureCallback(currentStats, exceeded);
    }
    
    return bufferId;
}

void GPUBufferPool::ReleaseBuffer(uint32_t bufferId) {
    if (bufferId == 0) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_bufferLookup.find(bufferId);
    if (it != m_bufferLookup.end()) {
        // 如果缓冲被映射,先取消映射
        if (it->second->mappedPtr) {
            LOG_WARNING_F("GPUBufferPool: 归还时缓冲 ID=%u 仍被映射,自动取消映射", bufferId);
            UnmapBuffer(bufferId);
        }
        
        it->second->inUse = false;
        ++m_stats.releaseCount;
        
        LOG_DEBUG_F("GPUBufferPool: 归还缓冲 ID=%u", bufferId);
    }
}

void GPUBufferPool::Reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 标记所有缓冲为可用
    auto resetPool = [&](std::vector<PoolEntry>& pool) {
        for (auto& entry : pool) {
            // 如果缓冲被映射,取消映射
            if (entry.mappedPtr) {
                LOG_WARNING_F("GPUBufferPool: Reset时缓冲 ID=%u 仍被映射,强制取消映射", 
                              entry.bufferId);
                UnmapBuffer(entry.bufferId);
            }
            entry.inUse = false;
        }
    };
    
    resetPool(m_staticPool);
    resetPool(m_dynamicPool);
    resetPool(m_streamPool);
    
    ++m_currentFrame;
}

void GPUBufferPool::CleanupUnused(uint32_t unusedFrames) {
    // 注意:这个函数可能被AcquireBuffer调用,所以不能再次加锁
    // 调用者必须已经持有锁
    
    auto cleanupPool = [&](std::vector<PoolEntry>& pool) {
        auto it = pool.begin();
        while (it != pool.end()) {
            if (!it->inUse && (m_currentFrame - it->lastUsedFrame) >= unusedFrames) {
                LOG_DEBUG_F("GPUBufferPool: 清理未使用缓冲 ID=%u (未使用 %llu 帧)",
                            it->bufferId,
                            static_cast<unsigned long long>(m_currentFrame - it->lastUsedFrame));
                
                DeleteBuffer(it->bufferId);
                m_bufferLookup.erase(it->bufferId);
                it = pool.erase(it);
            } else {
                ++it;
            }
        }
    };
    
    cleanupPool(m_staticPool);
    cleanupPool(m_dynamicPool);
    cleanupPool(m_streamPool);
}

BufferMappingStrategy GPUBufferPool::SelectMappingStrategy(
    BufferTarget /* target */,
    uint32_t usage,
    size_t size,
    AccessPattern /* pattern */) const {
    
    if (usage == GL_STREAM_DRAW || usage == GL_STREAM_READ || usage == GL_STREAM_COPY) {
        return BufferMappingStrategy::UnsynchronizedMapping;
    }
    
    if (usage == GL_DYNAMIC_DRAW || usage == GL_DYNAMIC_READ || usage == GL_DYNAMIC_COPY) {
        if (size > 1024 * 1024) {
            return BufferMappingStrategy::PersistentMapping;
        }
        return BufferMappingStrategy::TraditionalMapping;
    }
    
    return BufferMappingStrategy::TraditionalMapping;
}

void* GPUBufferPool::MapBuffer(uint32_t bufferId, BufferMappingStrategy strategy) {
    if (bufferId == 0) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_bufferLookup.find(bufferId);
    if (it == m_bufferLookup.end()) {
        LOG_ERROR_F("GPUBufferPool: 缓冲 ID=%u 不在池中", bufferId);
        return nullptr;
    }
    
    PoolEntry* entry = it->second;
    if (entry->mappedPtr) {
        LOG_WARNING_F("GPUBufferPool: 缓冲 ID=%u 已经被映射", bufferId);
        return entry->mappedPtr;
    }
    
    GL_THREAD_CHECK();
    
    uint32_t glTarget = 0;
    switch (entry->desc.target) {
        case BufferTarget::ArrayBuffer: glTarget = GL_ARRAY_BUFFER; break;
        case BufferTarget::ElementArrayBuffer: glTarget = GL_ELEMENT_ARRAY_BUFFER; break;
        case BufferTarget::UniformBuffer: glTarget = GL_UNIFORM_BUFFER; break;
        case BufferTarget::ShaderStorageBuffer: glTarget = GL_SHADER_STORAGE_BUFFER; break;
    }
    
    // 关键修复:保存和恢复OpenGL状态
    GLBufferBindingGuard guard(glTarget);
    
    glBindBuffer(glTarget, bufferId);
    
    void* ptr = nullptr;
    
    switch (strategy) {
        case BufferMappingStrategy::PersistentMapping:
            ptr = glMapBufferRange(glTarget, 0, entry->desc.size,
                                  GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
            break;
            
        case BufferMappingStrategy::CoherentMapping:
            ptr = glMapBufferRange(glTarget, 0, entry->desc.size,
                                  GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT);
            break;
            
        case BufferMappingStrategy::UnsynchronizedMapping:
            ptr = glMapBufferRange(glTarget, 0, entry->desc.size,
                                  GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
            break;
            
        case BufferMappingStrategy::TraditionalMapping:
            ptr = glMapBuffer(glTarget, GL_WRITE_ONLY);
            break;
    }
    
    if (ptr) {
        entry->mappedPtr = ptr;
        LOG_DEBUG_F("GPUBufferPool: 映射缓冲 ID=%u", bufferId);
    } else {
        LOG_ERROR_F("GPUBufferPool: 映射缓冲 ID=%u 失败", bufferId);
    }
    
    return ptr;
    // guard析构时自动恢复之前的绑定
}

void GPUBufferPool::UnmapBuffer(uint32_t bufferId) {
    if (bufferId == 0) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_bufferLookup.find(bufferId);
    if (it == m_bufferLookup.end()) {
        return;
    }
    
    PoolEntry* entry = it->second;
    if (!entry->mappedPtr) {
        return;
    }
    
    GL_THREAD_CHECK();
    
    uint32_t glTarget = 0;
    switch (entry->desc.target) {
        case BufferTarget::ArrayBuffer: glTarget = GL_ARRAY_BUFFER; break;
        case BufferTarget::ElementArrayBuffer: glTarget = GL_ELEMENT_ARRAY_BUFFER; break;
        case BufferTarget::UniformBuffer: glTarget = GL_UNIFORM_BUFFER; break;
        case BufferTarget::ShaderStorageBuffer: glTarget = GL_SHADER_STORAGE_BUFFER; break;
    }
    
    // 关键修复:保存和恢复OpenGL状态
    GLBufferBindingGuard guard(glTarget);
    
    glBindBuffer(glTarget, bufferId);
    glUnmapBuffer(glTarget);
    
    entry->mappedPtr = nullptr;
    
    LOG_DEBUG_F("GPUBufferPool: 取消映射缓冲 ID=%u", bufferId);
    // guard析构时自动恢复之前的绑定
}

GPUBufferPool::Stats GPUBufferPool::GetStats() const {
    // 注意:调用者可能已经持有锁,所以不能再次加锁
    // 如果需要线程安全,调用者应该持有锁
    
    Stats stats = m_stats;
    stats.totalBuffers = m_staticPool.size() + m_dynamicPool.size() + m_streamPool.size();
    stats.activeBuffers = 0;
    stats.availableBuffers = 0;
    stats.totalMemory = 0;
    stats.activeMemory = 0;
    
    auto countPool = [&](const std::vector<PoolEntry>& pool) {
        for (const auto& entry : pool) {
            stats.totalMemory += entry.desc.size;
            if (entry.inUse) {
                stats.activeBuffers++;
                stats.activeMemory += entry.desc.size;
            } else {
                stats.availableBuffers++;
            }
        }
    };
    
    countPool(m_staticPool);
    countPool(m_dynamicPool);
    countPool(m_streamPool);
    
    return stats;
}

void GPUBufferPool::ResetStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats = Stats{};
}

void GPUBufferPool::SetMemoryLimit(size_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_memoryLimit = bytes;
    LOG_INFO_F("GPUBufferPool: 设置内存限制 %.2f MB", bytes / (1024.0f * 1024.0f));
}

size_t GPUBufferPool::GetMemoryLimit() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_memoryLimit;
}

bool GPUBufferPool::IsMemoryLimitExceeded() const {
    if (m_memoryLimit == 0) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    auto stats = GetStats();
    return stats.totalMemory > m_memoryLimit;
}

void GPUBufferPool::SetMemoryPressureCallback(MemoryPressureCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_memoryPressureCallback = callback;
}

void GPUBufferPool::PrewarmBuffers(const std::vector<BufferDescriptor>& descriptors) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    LOG_INFO_F("GPUBufferPool: 预热 %zu 个缓冲", descriptors.size());
    
    for (const auto& desc : descriptors) {
        uint32_t bufferId = CreateNewBuffer(desc);
        if (bufferId == 0) {
            LOG_WARNING_F("GPUBufferPool: 预热缓冲失败 (%.2f KB)", desc.size / 1024.0f);
            continue;
        }
        
        PoolEntry entry;
        entry.bufferId = bufferId;
        entry.desc = desc;
        entry.inUse = false;
        entry.lastUsedFrame = m_currentFrame;
        
        auto* pool = GetPoolForUsage(desc.usage);
        if (pool) {
            pool->push_back(entry);
            m_bufferLookup[bufferId] = &pool->back();
        }
    }
    
    auto stats = GetStats();
    LOG_INFO_F("GPUBufferPool: 预热完成,总内存 %.2f MB", stats.totalMemory / (1024.0f * 1024.0f));
}

// ========================================================================
// 私有辅助函数
// ========================================================================

std::vector<GPUBufferPool::PoolEntry>* GPUBufferPool::GetPoolForUsage(uint32_t usage) {
    if (usage == GL_STATIC_DRAW || usage == GL_STATIC_READ || usage == GL_STATIC_COPY) {
        return &m_staticPool;
    }
    else if (usage == GL_DYNAMIC_DRAW || usage == GL_DYNAMIC_READ || usage == GL_DYNAMIC_COPY) {
        return &m_dynamicPool;
    }
    else if (usage == GL_STREAM_DRAW || usage == GL_STREAM_READ || usage == GL_STREAM_COPY) {
        return &m_streamPool;
    }
    
    return &m_dynamicPool;
}

GPUBufferPool::PoolEntry* GPUBufferPool::FindAvailableBuffer(const BufferDescriptor& desc) {
    auto* pool = GetPoolForUsage(desc.usage);
    if (!pool) {
        return nullptr;
    }
    
    PoolEntry* bestMatch = nullptr;
    size_t bestSizeDiff = SIZE_MAX;
    
    for (auto& entry : *pool) {
        if (!entry.inUse && entry.desc.target == desc.target) {
            if (entry.desc.size >= desc.size) {
                size_t sizeDiff = entry.desc.size - desc.size;
                // 限制最大浪费为 100%
                if (sizeDiff <= desc.size && sizeDiff < bestSizeDiff) {
                    bestMatch = &entry;
                    bestSizeDiff = sizeDiff;
                    if (sizeDiff == 0) {
                        break;
                    }
                }
            }
        }
    }
    
    return bestMatch;
}

uint32_t GPUBufferPool::CreateNewBuffer(const BufferDescriptor& desc) {
    GL_THREAD_CHECK();
    
    uint32_t bufferId = 0;
    glGenBuffers(1, &bufferId);
    
    if (bufferId == 0) {
        LOG_ERROR("GPUBufferPool: glGenBuffers 失败");
        return 0;
    }
    
    uint32_t glTarget = 0;
    switch (desc.target) {
        case BufferTarget::ArrayBuffer: glTarget = GL_ARRAY_BUFFER; break;
        case BufferTarget::ElementArrayBuffer: glTarget = GL_ELEMENT_ARRAY_BUFFER; break;
        case BufferTarget::UniformBuffer: glTarget = GL_UNIFORM_BUFFER; break;
        case BufferTarget::ShaderStorageBuffer: glTarget = GL_SHADER_STORAGE_BUFFER; break;
    }
    
    // 关键修复:保存和恢复OpenGL状态
    GLBufferBindingGuard guard(glTarget);
    
    glBindBuffer(glTarget, bufferId);
    glBufferData(glTarget, desc.size, nullptr, desc.usage);
    
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        LOG_ERROR_F("GPUBufferPool: glBufferData 失败 (error=0x%X)", error);
        glDeleteBuffers(1, &bufferId);
        return 0;
    }
    
    return bufferId;
    // guard析构时自动恢复之前的绑定
}

void GPUBufferPool::DeleteBuffer(uint32_t bufferId) {
    if (bufferId == 0) {
        return;
    }
    
    GL_THREAD_CHECK();
    glDeleteBuffers(1, &bufferId);
}

uint32_t GPUBufferPool::GetGLUsage(uint32_t usage) const {
    return usage;
}

// ========================================================================
// BufferMappingManager 修复实现
// ========================================================================

BufferMappingManager& BufferMappingManager::GetInstance() {
    static BufferMappingManager instance;
    return instance;
}

void* BufferMappingManager::Map(uint32_t bufferId, BufferTarget target, AccessPattern pattern) {
    if (bufferId == 0) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_mappedBuffers.find(bufferId);
    if (it != m_mappedBuffers.end()) {
        LOG_WARNING_F("BufferMappingManager: 缓冲 ID=%u 已经被映射", bufferId);
        return it->second.mappedPtr;
    }
    
    GL_THREAD_CHECK();
    
    uint32_t glTarget = 0;
    switch (target) {
        case BufferTarget::ArrayBuffer: glTarget = GL_ARRAY_BUFFER; break;
        case BufferTarget::ElementArrayBuffer: glTarget = GL_ELEMENT_ARRAY_BUFFER; break;
        case BufferTarget::UniformBuffer: glTarget = GL_UNIFORM_BUFFER; break;
        case BufferTarget::ShaderStorageBuffer: glTarget = GL_SHADER_STORAGE_BUFFER; break;
    }
    
    // 关键修复:保存和恢复OpenGL状态
    GLBufferBindingGuard guard(glTarget);
    
    glBindBuffer(glTarget, bufferId);
    
    GLbitfield access = 0;
    switch (pattern) {
        case AccessPattern::WriteOnly:
            access = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT;
            break;
        case AccessPattern::ReadOnly:
            access = GL_MAP_READ_BIT;
            break;
        case AccessPattern::ReadWrite:
            access = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
            break;
    }
    
    GLint bufferSize = 0;
    glGetBufferParameteriv(glTarget, GL_BUFFER_SIZE, &bufferSize);
    
    void* ptr = glMapBufferRange(glTarget, 0, bufferSize, access);
    
    if (ptr) {
        MappedBuffer mapped;
        mapped.bufferId = bufferId;
        mapped.mappedPtr = ptr;
        mapped.strategy = BufferMappingStrategy::TraditionalMapping;
        mapped.target = target;
        
        m_mappedBuffers[bufferId] = mapped;
        
        LOG_DEBUG_F("BufferMappingManager: 映射缓冲 ID=%u", bufferId);
    } else {
        LOG_ERROR_F("BufferMappingManager: 映射缓冲 ID=%u 失败", bufferId);
    }
    
    return ptr;
    // guard析构时自动恢复之前的绑定
}

void BufferMappingManager::Unmap(uint32_t bufferId) {
    if (bufferId == 0) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_mappedBuffers.find(bufferId);
    if (it == m_mappedBuffers.end()) {
        return;
    }
    
    GL_THREAD_CHECK();
    
    uint32_t glTarget = 0;
    switch (it->second.target) {
        case BufferTarget::ArrayBuffer: glTarget = GL_ARRAY_BUFFER; break;
        case BufferTarget::ElementArrayBuffer: glTarget = GL_ELEMENT_ARRAY_BUFFER; break;
        case BufferTarget::UniformBuffer: glTarget = GL_UNIFORM_BUFFER; break;
        case BufferTarget::ShaderStorageBuffer: glTarget = GL_SHADER_STORAGE_BUFFER; break;
    }
    
    // 关键修复:保存和恢复OpenGL状态
    GLBufferBindingGuard guard(glTarget);
    
    glBindBuffer(glTarget, bufferId);
    glUnmapBuffer(glTarget);
    
    m_mappedBuffers.erase(it);
    
    LOG_DEBUG_F("BufferMappingManager: 取消映射缓冲 ID=%u", bufferId);
    // guard析构时自动恢复之前的绑定
}

bool BufferMappingManager::IsMapped(uint32_t bufferId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_mappedBuffers.find(bufferId) != m_mappedBuffers.end();
}

} // namespace Render