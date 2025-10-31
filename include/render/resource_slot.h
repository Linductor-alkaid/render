#pragma once

#include "resource_handle.h"
#include <vector>
#include <memory>
#include <mutex>
#include <cassert>

namespace Render {

/**
 * @brief 资源槽
 * 
 * 用于存储资源及其元数据的槽位。
 * 每个槽包含：
 * - 资源指针
 * - 代数（用于检测悬空句柄）
 * - 活跃标志
 */
template<typename T>
struct ResourceSlot {
    std::shared_ptr<T> resource;        // 资源指针
    ResourceGeneration generation = 0;  // 当前代数
    bool active = false;                // 是否活跃
    std::string name;                   // 资源名称（用于调试和热重载）
    uint32_t lastAccessFrame = 0;       // 最后访问帧号
    
    ResourceSlot() = default;
};

/**
 * @brief 资源槽管理器
 * 
 * 管理资源槽的分配、释放和访问。
 * 使用自由列表（free list）实现 O(1) 的分配和释放。
 * 
 * 特性：
 * - O(1) 的资源访问
 * - O(1) 的句柄创建和销毁
 * - 支持 ID 重用（代数递增）
 * - 自动检测悬空句柄
 * - 线程安全
 */
template<typename T>
class ResourceSlotManager {
public:
    /**
     * @brief 构造函数
     * @param initialCapacity 初始容量
     */
    ResourceSlotManager(size_t initialCapacity = 256) {
        m_slots.reserve(initialCapacity);
    }
    
    /**
     * @brief 分配一个槽并创建句柄
     * @param resource 资源指针
     * @param name 资源名称（用于调试）
     * @param currentFrame 当前帧号
     * @return 资源句柄
     */
    ResourceHandle<T> Allocate(std::shared_ptr<T> resource, 
                               const std::string& name,
                               uint32_t currentFrame) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        ResourceID id;
        ResourceGeneration generation;
        
        // 尝试从自由列表获取 ID
        if (!m_freeList.empty()) {
            id = m_freeList.back();
            m_freeList.pop_back();
            
            assert(id < m_slots.size());
            assert(!m_slots[id].active);
            
            // 代数递增（检测悬空句柄）
            m_slots[id].generation++;
            generation = m_slots[id].generation;
            
            // 更新槽信息
            m_slots[id].resource = std::move(resource);
            m_slots[id].active = true;
            m_slots[id].name = name;
            m_slots[id].lastAccessFrame = currentFrame;
        }
        else {
            // 分配新槽
            id = static_cast<ResourceID>(m_slots.size());
            generation = 0;
            
            ResourceSlot<T> slot;
            slot.resource = std::move(resource);
            slot.generation = generation;
            slot.active = true;
            slot.name = name;
            slot.lastAccessFrame = currentFrame;
            
            m_slots.push_back(std::move(slot));
        }
        
        return ResourceHandle<T>(id, generation);
    }
    
    /**
     * @brief 释放槽
     * @param handle 资源句柄
     */
    void Free(const ResourceHandle<T>& handle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        ResourceID id = handle.GetID();
        if (id >= m_slots.size()) {
            return;  // 无效 ID
        }
        
        ResourceSlot<T>& slot = m_slots[id];
        
        // 检查代数是否匹配
        if (slot.generation != handle.GetGeneration()) {
            return;  // 悬空句柄
        }
        
        if (!slot.active) {
            return;  // 已经释放
        }
        
        // 释放资源
        slot.resource.reset();
        slot.active = false;
        slot.name.clear();
        
        // 添加到自由列表
        m_freeList.push_back(id);
    }
    
    /**
     * @brief 根据句柄获取资源
     * @param handle 资源句柄
     * @return 资源指针，如果无效返回 nullptr
     */
    T* Get(const ResourceHandle<T>& handle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return GetUnsafe(handle);
    }
    
    /**
     * @brief 根据句柄获取资源的 shared_ptr
     * @param handle 资源句柄
     * @return 资源的 shared_ptr，如果无效返回 nullptr
     */
    std::shared_ptr<T> GetShared(const ResourceHandle<T>& handle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        ResourceID id = handle.GetID();
        if (id >= m_slots.size()) {
            return nullptr;
        }
        
        ResourceSlot<T>& slot = m_slots[id];
        
        // 检查代数和活跃状态
        if (slot.generation != handle.GetGeneration() || !slot.active) {
            return nullptr;
        }
        
        return slot.resource;
    }
    
    /**
     * @brief 检查句柄是否有效
     * @param handle 资源句柄
     * @return 如果句柄有效返回 true
     */
    bool IsValid(const ResourceHandle<T>& handle) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return IsValidUnsafe(handle);
    }
    
    /**
     * @brief 更新资源的访问帧
     * @param handle 资源句柄
     * @param frame 当前帧号
     */
    void UpdateAccessFrame(const ResourceHandle<T>& handle, uint32_t frame) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        ResourceID id = handle.GetID();
        if (id >= m_slots.size()) {
            return;
        }
        
        ResourceSlot<T>& slot = m_slots[id];
        if (slot.generation == handle.GetGeneration() && slot.active) {
            slot.lastAccessFrame = frame;
        }
    }
    
    /**
     * @brief 热重载资源
     * @param handle 资源句柄
     * @param newResource 新资源
     * @return 是否成功
     * 
     * 保持句柄和代数不变，只替换资源内容。
     * 所有持有该句柄的对象会自动使用新资源。
     */
    bool Reload(const ResourceHandle<T>& handle, std::shared_ptr<T> newResource) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        ResourceID id = handle.GetID();
        if (id >= m_slots.size()) {
            return false;
        }
        
        ResourceSlot<T>& slot = m_slots[id];
        
        // 检查代数和活跃状态
        if (slot.generation != handle.GetGeneration() || !slot.active) {
            return false;
        }
        
        // 替换资源
        slot.resource = std::move(newResource);
        return true;
    }
    
    /**
     * @brief 清理未使用的资源
     * @param currentFrame 当前帧号
     * @param unusedFrames 多少帧未使用后清理
     * @return 清理的资源数量
     */
    size_t CleanupUnused(uint32_t currentFrame, uint32_t unusedFrames) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        size_t cleanedCount = 0;
        std::vector<ResourceID> toDelete;
        
        // 阶段1: 查找未使用的资源
        for (size_t i = 0; i < m_slots.size(); ++i) {
            ResourceSlot<T>& slot = m_slots[i];
            
            if (!slot.active) {
                continue;
            }
            
            // 检查是否长时间未使用
            bool unused = (currentFrame - slot.lastAccessFrame) > unusedFrames;
            
            // 检查是否只有管理器持有引用
            bool onlyManagerRef = slot.resource.use_count() == 1;
            
            if (unused && onlyManagerRef) {
                toDelete.push_back(static_cast<ResourceID>(i));
            }
        }
        
        // 阶段2: 删除资源
        for (ResourceID id : toDelete) {
            ResourceSlot<T>& slot = m_slots[id];
            
            // 再次检查（避免竞态）
            if (slot.active && slot.resource.use_count() == 1) {
                slot.resource.reset();
                slot.active = false;
                slot.name.clear();
                
                m_freeList.push_back(id);
                ++cleanedCount;
            }
        }
        
        return cleanedCount;
    }
    
    /**
     * @brief 清空所有资源
     */
    void Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_slots.clear();
        m_freeList.clear();
    }
    
    /**
     * @brief 获取活跃资源数量
     */
    size_t GetActiveCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t count = 0;
        for (const auto& slot : m_slots) {
            if (slot.active) {
                ++count;
            }
        }
        return count;
    }
    
    /**
     * @brief 获取总槽数量
     */
    size_t GetTotalSlots() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_slots.size();
    }
    
    /**
     * @brief 获取自由槽数量
     */
    size_t GetFreeSlots() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_freeList.size();
    }
    
    /**
     * @brief 遍历所有活跃资源
     * @param callback 回调函数 (handle, resource)
     */
    void ForEach(std::function<void(const ResourceHandle<T>&, std::shared_ptr<T>)> callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (size_t i = 0; i < m_slots.size(); ++i) {
            ResourceSlot<T>& slot = m_slots[i];
            if (slot.active && slot.resource) {
                ResourceHandle<T> handle(static_cast<ResourceID>(i), slot.generation);
                callback(handle, slot.resource);
            }
        }
    }

private:
    /**
     * @brief 获取资源（不加锁，内部使用）
     */
    T* GetUnsafe(const ResourceHandle<T>& handle) {
        ResourceID id = handle.GetID();
        if (id >= m_slots.size()) {
            return nullptr;
        }
        
        ResourceSlot<T>& slot = m_slots[id];
        
        // 检查代数和活跃状态
        if (slot.generation != handle.GetGeneration() || !slot.active) {
            return nullptr;
        }
        
        return slot.resource.get();
    }
    
    /**
     * @brief 检查句柄是否有效（不加锁，内部使用）
     */
    bool IsValidUnsafe(const ResourceHandle<T>& handle) const {
        ResourceID id = handle.GetID();
        if (id >= m_slots.size()) {
            return false;
        }
        
        const ResourceSlot<T>& slot = m_slots[id];
        return slot.generation == handle.GetGeneration() && slot.active;
    }

private:
    std::vector<ResourceSlot<T>> m_slots;   // 资源槽数组
    std::vector<ResourceID> m_freeList;     // 可重用的 ID 列表
    mutable std::mutex m_mutex;             // 线程安全
};

} // namespace Render

