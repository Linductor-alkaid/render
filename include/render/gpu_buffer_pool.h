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
#pragma once

#include "render/render_state.h"
#include <cstddef>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <string>

namespace Render {

/**
 * @brief GPU 缓冲描述符
 */
struct BufferDescriptor {
    size_t size = 0;                       ///< 缓冲大小（字节）
    BufferTarget target = BufferTarget::ArrayBuffer;  ///< 缓冲目标
    uint32_t usage = 0x88E4;               ///< GL_STATIC_DRAW (默认)
    
    bool operator==(const BufferDescriptor& other) const {
        return size == other.size &&
               target == other.target &&
               usage == other.usage;
    }
};

/**
 * @brief 缓冲描述符哈希函数
 */
struct BufferDescriptorHash {
    size_t operator()(const BufferDescriptor& desc) const {
        size_t hash = std::hash<size_t>{}(desc.size);
        hash ^= std::hash<int>{}(static_cast<int>(desc.target)) << 1;
        hash ^= std::hash<uint32_t>{}(desc.usage) << 2;
        return hash;
    }
};

/**
 * @brief 缓冲映射策略
 */
enum class BufferMappingStrategy {
    PersistentMapping,      ///< 持久化映射（GL_MAP_PERSISTENT_BIT）
    CoherentMapping,        ///< 一致性映射（GL_MAP_COHERENT_BIT）
    UnsynchronizedMapping,  ///< 非同步映射（GL_MAP_UNSYNCHRONIZED_BIT）
    TraditionalMapping      ///< 传统映射（glMapBuffer/glUnmapBuffer）
};

/**
 * @brief 访问模式（用于选择映射策略）
 */
enum class AccessPattern {
    WriteOnly,     ///< 只写（CPU -> GPU）
    ReadOnly,      ///< 只读（GPU -> CPU）
    ReadWrite      ///< 读写
};

/**
 * @brief GPU 缓冲池
 * 
 * 管理和复用 GPU 缓冲对象，减少分配/释放开销和内存碎片。
 * 
 * 特性：
 * - 自动缓冲复用
 * - 按大小和用途分类管理
 * - 支持不同的映射策略
 * - 线程安全
 * - 自动清理未使用的缓冲
 * 
 * 使用场景：
 * - VBO/IBO 复用
 * - UBO 复用
 * - 临时缓冲管理
 * 
 * 使用示例：
 * @code
 * auto& pool = GPUBufferPool::GetInstance();
 * 
 * // 获取缓冲
 * BufferDescriptor desc;
 * desc.size = 1024 * 1024;  // 1 MB
 * desc.target = BufferTarget::ArrayBuffer;
 * desc.usage = GL_DYNAMIC_DRAW;
 * 
 * uint32_t bufferId = pool.AcquireBuffer(desc);
 * 
 * // 使用缓冲...
 * 
 * // 归还缓冲
 * pool.ReleaseBuffer(bufferId);
 * 
 * // 每帧重置（快速归还所有缓冲）
 * pool.Reset();
 * @endcode
 */
class GPUBufferPool {
public:
    /**
     * @brief 获取单例实例
     */
    static GPUBufferPool& GetInstance();
    
    // 禁止拷贝和移动
    GPUBufferPool(const GPUBufferPool&) = delete;
    GPUBufferPool& operator=(const GPUBufferPool&) = delete;
    GPUBufferPool(GPUBufferPool&&) = delete;
    GPUBufferPool& operator=(GPUBufferPool&&) = delete;
    
    /**
     * @brief 从池中获取缓冲
     * @param desc 缓冲描述符
     * @return OpenGL 缓冲 ID（0 表示失败）
     * 
     * 如果池中有匹配的缓冲，直接返回；
     * 否则创建新缓冲。
     */
    uint32_t AcquireBuffer(const BufferDescriptor& desc);
    
    /**
     * @brief 归还缓冲到池
     * @param bufferId OpenGL 缓冲 ID
     * 
     * 将缓冲标记为可用，等待下次复用。
     * 不会立即删除缓冲。
     */
    void ReleaseBuffer(uint32_t bufferId);
    
    /**
     * @brief 重置池（每帧调用）
     * 
     * 快速归还所有已获取的缓冲，标记为可用。
     * 适合每帧开始时调用。
     */
    void Reset();
    
    /**
     * @brief 清理未使用的缓冲
     * @param unusedFrames 未使用帧数阈值
     * 
     * 删除超过指定帧数未使用的缓冲，释放 GPU 内存。
     */
    void CleanupUnused(uint32_t unusedFrames = 60);
    
    /**
     * @brief 选择最佳映射策略
     * @param target 缓冲目标
     * @param usage 缓冲用途
     * @param size 缓冲大小
     * @param pattern 访问模式
     * @return 推荐的映射策略
     */
    BufferMappingStrategy SelectMappingStrategy(
        BufferTarget target,
        uint32_t usage,
        size_t size,
        AccessPattern pattern) const;
    
    /**
     * @brief 映射缓冲
     * @param bufferId OpenGL 缓冲 ID
     * @param strategy 映射策略
     * @return 映射指针（失败返回 nullptr）
     */
    void* MapBuffer(uint32_t bufferId, BufferMappingStrategy strategy);
    
    /**
     * @brief 取消映射缓冲
     * @param bufferId OpenGL 缓冲 ID
     */
    void UnmapBuffer(uint32_t bufferId);
    
    /**
     * @brief 获取统计信息
     */
    struct Stats {
        size_t totalBuffers = 0;       ///< 总缓冲数
        size_t activeBuffers = 0;      ///< 活跃缓冲数
        size_t availableBuffers = 0;   ///< 可用缓冲数
        size_t totalMemory = 0;        ///< 总内存（字节）
        size_t activeMemory = 0;       ///< 活跃内存（字节）
        uint32_t acquireCount = 0;     ///< 获取次数
        uint32_t releaseCount = 0;     ///< 归还次数
        uint32_t createCount = 0;      ///< 创建次数
        uint32_t reuseCount = 0;       ///< 复用次数
    };
    
    Stats GetStats() const;
    
    /**
     * @brief 重置统计信息
     */
    void ResetStats();
    
    /**
     * @brief 设置内存限制
     * @param bytes 最大内存字节数（0 = 无限制）
     */
    void SetMemoryLimit(size_t bytes);
    
    /**
     * @brief 获取内存限制
     * @return 内存限制（字节），0 表示无限制
     */
    size_t GetMemoryLimit() const;
    
    /**
     * @brief 检查是否超过内存限制
     * @return 是否超限
     */
    bool IsMemoryLimitExceeded() const;
    
    /**
     * @brief 设置内存压力回调
     * @param callback 回调函数
     * 
     * 当内存使用接近或超过限制时触发回调
     */
    using MemoryPressureCallback = std::function<void(const Stats&, bool exceeded)>;
    void SetMemoryPressureCallback(MemoryPressureCallback callback);
    
    /**
     * @brief 预热缓冲池
     * @param descriptors 预分配的缓冲描述符列表
     * 
     * 在初始化时预先创建常用大小的缓冲，减少首帧分配开销
     */
    void PrewarmBuffers(const std::vector<BufferDescriptor>& descriptors);

private:
    GPUBufferPool();
    ~GPUBufferPool();
    
    struct PoolEntry {
        uint32_t bufferId = 0;
        BufferDescriptor desc;
        bool inUse = false;
        size_t lastUsedFrame = 0;
        void* mappedPtr = nullptr;
    };
    
    // 按 usage 分类的池（Static/Dynamic/Stream）
    std::vector<PoolEntry> m_staticPool;
    std::vector<PoolEntry> m_dynamicPool;
    std::vector<PoolEntry> m_streamPool;
    
    // 缓冲 ID 到池条目的查找表
    std::unordered_map<uint32_t, PoolEntry*> m_bufferLookup;
    
    // 统计信息
    Stats m_stats;
    uint64_t m_currentFrame = 0;
    
    // 内存限制和回调
    size_t m_memoryLimit = 0;  ///< 内存限制（0 = 无限制）
    MemoryPressureCallback m_memoryPressureCallback;
    
    mutable std::mutex m_mutex;
    
    // 辅助函数
    std::vector<PoolEntry>* GetPoolForUsage(uint32_t usage);
    PoolEntry* FindAvailableBuffer(const BufferDescriptor& desc);
    uint32_t CreateNewBuffer(const BufferDescriptor& desc);
    void DeleteBuffer(uint32_t bufferId);
    uint32_t GetGLUsage(uint32_t usage) const;
};

/**
 * @brief 缓冲映射管理器
 * 
 * 辅助类，用于管理缓冲映射操作。
 * 可以与 GPUBufferPool 配合使用。
 */
class BufferMappingManager {
public:
    /**
     * @brief 获取单例实例
     */
    static BufferMappingManager& GetInstance();
    
    /**
     * @brief 映射缓冲（自动选择策略）
     * @param bufferId OpenGL 缓冲 ID
     * @param target 缓冲目标
     * @param pattern 访问模式
     * @return 映射指针
     */
    void* Map(uint32_t bufferId, BufferTarget target, AccessPattern pattern);
    
    /**
     * @brief 取消映射
     * @param bufferId OpenGL 缓冲 ID
     */
    void Unmap(uint32_t bufferId);
    
    /**
     * @brief 检查缓冲是否已映射
     * @param bufferId OpenGL 缓冲 ID
     * @return 是否已映射
     */
    bool IsMapped(uint32_t bufferId) const;

private:
    BufferMappingManager() = default;
    ~BufferMappingManager() = default;
    
    struct MappedBuffer {
        uint32_t bufferId = 0;
        void* mappedPtr = nullptr;
        BufferMappingStrategy strategy = BufferMappingStrategy::TraditionalMapping;
        BufferTarget target = BufferTarget::ArrayBuffer;
    };
    
    std::unordered_map<uint32_t, MappedBuffer> m_mappedBuffers;
    mutable std::mutex m_mutex;
};

} // namespace Render

