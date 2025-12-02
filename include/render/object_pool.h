#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <cstddef>

namespace Render {

/**
 * @brief 通用对象池模板类
 * 
 * 用于频繁创建/销毁的对象的复用，减少内存分配开销和内存碎片。
 * 
 * 特性：
 * - 线程安全：所有公共方法都是线程安全的
 * - 自动增长：当池为空时自动分配新对象
 * - 容量限制：支持设置最大容量
 * - 批量重置：快速归还所有对象
 * 
 * 使用场景：
 * - SpriteRenderable 对象池
 * - TextRenderable 对象池
 * - 其他频繁创建/销毁的渲染对象
 * 
 * 使用示例：
 * @code
 * ObjectPool<SpriteRenderable> pool(16, 1024);
 * 
 * // 获取对象
 * SpriteRenderable* sprite = pool.Acquire();
 * sprite->SetTexture(texture);
 * // ... 使用 sprite
 * 
 * // 归还对象
 * pool.Release(sprite);
 * 
 * // 或批量归还所有对象
 * pool.Reset();
 * @endcode
 * 
 * @tparam T 对象类型
 */
template<typename T>
class ObjectPool {
public:
    /**
     * @brief 构造函数
     * @param initialSize 初始容量（预分配对象数量）
     * @param maxSize 最大容量（0 表示无限制）
     */
    ObjectPool(size_t initialSize = 16, size_t maxSize = 1024)
        : m_maxSize(maxSize)
        , m_activeCount(0) {
        m_pool.reserve(initialSize);
        m_available.reserve(initialSize);
        
        // 预分配初始对象
        for (size_t i = 0; i < initialSize; ++i) {
            auto obj = std::make_unique<T>();
            m_available.push_back(obj.get());
            m_pool.push_back(std::move(obj));
        }
    }
    
    /**
     * @brief 析构函数
     */
    ~ObjectPool() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_available.clear();
        m_pool.clear();
    }
    
    // 禁止拷贝和移动
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;
    ObjectPool(ObjectPool&&) = delete;
    ObjectPool& operator=(ObjectPool&&) = delete;
    
    /**
     * @brief 从池中获取一个对象
     * @return 对象指针
     * 
     * 注意：
     * - 如果池为空且未达到最大容量，会自动分配新对象
     * - 如果达到最大容量，会返回 nullptr
     * - 调用者需要在使用完毕后调用 Release() 归还对象
     */
    T* Acquire() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        T* obj = nullptr;
        
        if (!m_available.empty()) {
            // 从可用列表中获取
            obj = m_available.back();
            m_available.pop_back();
        } else if (m_maxSize == 0 || m_pool.size() < m_maxSize) {
            // 池为空且未达到最大容量，分配新对象
            auto newObj = std::make_unique<T>();
            obj = newObj.get();
            m_pool.push_back(std::move(newObj));
        } else {
            // 达到最大容量，返回 nullptr
            return nullptr;
        }
        
        ++m_activeCount;
        return obj;
    }
    
    /**
     * @brief 归还对象到池中
     * @param obj 要归还的对象指针
     * 
     * 注意：
     * - 对象必须是从此池中获取的
     * - 归还后不应再使用此对象
     * - 不会调用对象的析构函数，只是标记为可用
     */
    void Release(T* obj) {
        if (!obj) {
            return;
        }
        
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // 验证对象是否属于此池
        bool found = false;
        for (const auto& poolObj : m_pool) {
            if (poolObj.get() == obj) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            // 对象不属于此池，忽略
            return;
        }
        
        // 标记为可用
        m_available.push_back(obj);
        
        if (m_activeCount > 0) {
            --m_activeCount;
        }
    }
    
    /**
     * @brief 批量归还所有对象
     * 
     * 将所有已获取的对象标记为可用。
     * 适用于每帧结束时批量回收所有对象。
     * 
     * 注意：调用此函数后，所有从池中获取的对象都应该被视为无效。
     */
    void Reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // 将所有对象标记为可用
        m_available.clear();
        m_available.reserve(m_pool.size());
        for (const auto& obj : m_pool) {
            m_available.push_back(obj.get());
        }
        
        m_activeCount = 0;
    }
    
    /**
     * @brief 获取当前活跃对象数量
     * @return 活跃对象数量
     */
    size_t GetActiveCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_activeCount;
    }
    
    /**
     * @brief 获取池的总容量
     * @return 池中对象总数（包括活跃和可用）
     */
    size_t GetPoolSize() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_pool.size();
    }
    
    /**
     * @brief 获取可用对象数量
     * @return 可用对象数量
     */
    size_t GetAvailableCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_available.size();
    }
    
    /**
     * @brief 获取最大容量
     * @return 最大容量（0 表示无限制）
     */
    size_t GetMaxSize() const {
        return m_maxSize;
    }
    
    /**
     * @brief 收缩池到指定大小
     * @param targetSize 目标大小
     * 
     * 释放多余的可用对象，但不会影响活跃对象。
     * 适用于内存优化场景。
     */
    void Shrink(size_t targetSize) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_pool.size() <= targetSize) {
            return;
        }
        
        // 只保留活跃对象和部分可用对象
        size_t objectsToRemove = m_pool.size() - targetSize;
        size_t availableToRemove = std::min(objectsToRemove, m_available.size());
        
        // 从可用列表末尾移除
        for (size_t i = 0; i < availableToRemove; ++i) {
            T* objToRemove = m_available.back();
            m_available.pop_back();
            
            // 从池中移除
            auto it = std::find_if(m_pool.begin(), m_pool.end(),
                [objToRemove](const std::unique_ptr<T>& ptr) {
                    return ptr.get() == objToRemove;
                });
            
            if (it != m_pool.end()) {
                m_pool.erase(it);
            }
        }
    }
    
    /**
     * @brief 清空池
     * 
     * 释放所有对象（包括活跃和可用）。
     * 调用后，所有从池中获取的对象指针都会失效。
     */
    void Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_available.clear();
        m_pool.clear();
        m_activeCount = 0;
    }

private:
    std::vector<std::unique_ptr<T>> m_pool;      ///< 对象池存储
    std::vector<T*> m_available;                  ///< 可用对象列表
    size_t m_maxSize;                             ///< 最大容量（0 = 无限制）
    size_t m_activeCount;                         ///< 活跃对象数量
    mutable std::mutex m_mutex;                   ///< 线程安全互斥锁
};

} // namespace Render

