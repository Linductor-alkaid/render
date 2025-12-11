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

#include "entity.h"
#include "render/logger.h"
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <typeindex>
#include <type_traits>
#include <stdexcept>
#include <atomic>
#include <mutex>
#include <functional>
#include <vector>
#include <algorithm>

namespace Render {
namespace ECS {

/**
 * @brief 组件数组基类（类型擦除）
 * 
 * 用于在 ComponentRegistry 中统一存储不同类型的组件数组
 */
class IComponentArray {
public:
    virtual ~IComponentArray() = default;
    
    /**
     * @brief 移除指定实体的组件
     * @param entity 实体 ID
     */
    virtual void RemoveEntity(EntityID entity) = 0;
    
    /**
     * @brief 获取组件数量
     * @return 组件数量
     */
    [[nodiscard]] virtual size_t Size() const = 0;
    
    /**
     * @brief 清空所有组件
     */
    virtual void Clear() = 0;
};

/**
 * @brief 具体类型的组件数组
 * 
 * 使用 unordered_map 存储组件，提供 O(1) 访问速度
 * 线程安全：使用 shared_mutex 支持多读单写
 * 
 * @tparam T 组件类型
 */
template<typename T>
class ComponentArray : public IComponentArray {
public:
    /**
     * @brief 添加组件
     * @param entity 实体 ID
     * @param component 组件数据
     * 
     * @note 如果设置了变化回调，会在添加后调用回调
     * @note 回调在持有写锁的情况下调用，注意避免死锁
     */
    void Add(EntityID entity, const T& component) {
        std::unique_lock lock(m_mutex);
        m_components[entity] = component;
        
        // 如果设置了回调，在添加后调用回调
        // 注意：回调在持有写锁的情况下调用，回调内部应避免再次获取锁
        if (m_changeCallback) {
            try {
                m_changeCallback(entity, component);
            } catch (...) {
                // 忽略回调异常，避免影响组件添加操作
                // 在实际项目中可以考虑记录日志
            }
        }
    }
    
    /**
     * @brief 添加组件（移动语义）
     * @param entity 实体 ID
     * @param component 组件数据
     * 
     * @note 如果设置了变化回调，会在添加后调用回调
     * @note 回调在持有写锁的情况下调用，注意避免死锁
     * @note 移动后component可能已被移动，回调中使用的是存储的副本
     */
    void Add(EntityID entity, T&& component) {
        std::unique_lock lock(m_mutex);
        // 先存储组件（移动后component可能无效）
        m_components[entity] = std::move(component);
        
        // 获取存储的组件引用用于回调
        const T& storedComponent = m_components[entity];
        
        // 如果设置了回调，在添加后调用回调
        // 注意：回调在持有写锁的情况下调用，回调内部应避免再次获取锁
        if (m_changeCallback) {
            try {
                m_changeCallback(entity, storedComponent);
            } catch (...) {
                // 忽略回调异常，避免影响组件添加操作
                // 在实际项目中可以考虑记录日志
            }
        }
    }
    
    /**
     * @brief 移除组件
     * @param entity 实体 ID
     */
    void Remove(EntityID entity) {
        std::unique_lock lock(m_mutex);
        m_components.erase(entity);
    }
    
    /**
     * @brief 获取组件（可修改）
     * @param entity 实体 ID
     * @return 组件引用
     * @throws std::out_of_range 如果实体没有该组件
     */
    T& Get(EntityID entity) {
        std::shared_lock lock(m_mutex);
        auto it = m_components.find(entity);
        if (it == m_components.end()) {
            throw std::out_of_range("Component not found for entity");
        }
        return it->second;
    }
    
    /**
     * @brief 获取组件（只读）
     * @param entity 实体 ID
     * @return 组件常量引用
     * @throws std::out_of_range 如果实体没有该组件
     */
    const T& Get(EntityID entity) const {
        std::shared_lock lock(m_mutex);
        auto it = m_components.find(entity);
        if (it == m_components.end()) {
            throw std::out_of_range("Component not found for entity");
        }
        return it->second;
    }
    
    /**
     * @brief 检查实体是否有该组件
     * @param entity 实体 ID
     * @return 如果有该组件返回 true
     */
    [[nodiscard]] bool Has(EntityID entity) const {
        std::shared_lock lock(m_mutex);
        return m_components.find(entity) != m_components.end();
    }
    
    /**
     * @brief 移除实体的组件（实现 IComponentArray 接口）
     * @param entity 实体 ID
     */
    void RemoveEntity(EntityID entity) override {
        Remove(entity);
    }
    
    /**
     * @brief 获取组件数量
     * @return 组件数量
     */
    [[nodiscard]] size_t Size() const override {
        std::shared_lock lock(m_mutex);
        return m_components.size();
    }
    
    /**
     * @brief 清空所有组件
     */
    void Clear() override {
        std::unique_lock lock(m_mutex);
        m_components.clear();
    }
    
    // ==================== 迭代器支持 ====================
    
    /**
     * @brief 遍历所有组件（需要先获取锁）
     * @param func 回调函数 void(EntityID, T&)
     */
    template<typename Func>
    void ForEach(Func&& func) {
        std::shared_lock lock(m_mutex);
        for (auto& [entity, component] : m_components) {
            func(entity, component);
        }
    }
    
    /**
     * @brief 遍历所有组件（只读）
     * @param func 回调函数 void(EntityID, const T&)
     */
    template<typename Func>
    void ForEach(Func&& func) const {
        std::shared_lock lock(m_mutex);
        for (const auto& [entity, component] : m_components) {
            func(entity, component);
        }
    }
    
    /**
     * @brief 获取所有实体 ID
     * @return 实体 ID 列表
     */
    [[nodiscard]] std::vector<EntityID> GetEntities() const {
        std::shared_lock lock(m_mutex);
        std::vector<EntityID> entities;
        entities.reserve(m_components.size());
        for (const auto& [entity, _] : m_components) {
            entities.push_back(entity);
        }
        return entities;
    }
    
    // ==================== 组件变化回调支持 ====================
    
    /**
     * @brief 设置变化回调
     * @param callback 回调函数 void(EntityID, const T&)
     * 
     * @note 当组件被添加时，会调用此回调
     * @note 回调在持有写锁的情况下调用，注意避免死锁
     * @note 如果回调为空，则清除回调
     */
    void SetChangeCallback(std::function<void(EntityID, const T&)> callback) {
        std::unique_lock lock(m_mutex);
        m_changeCallback = std::move(callback);
    }
    
    /**
     * @brief 清除变化回调
     */
    void ClearChangeCallback() {
        std::unique_lock lock(m_mutex);
        m_changeCallback = nullptr;
    }
    
private:
    std::unordered_map<EntityID, T, EntityID::Hash> m_components;
    mutable std::shared_mutex m_mutex;  ///< 线程安全锁
    std::function<void(EntityID, const T&)> m_changeCallback;  ///< 组件变化回调
};

/**
 * @brief 组件注册表
 * 
 * 管理所有组件类型的存储和访问
 * - 类型安全：使用模板确保类型安全
 * - 线程安全：使用 shared_mutex 保护内部数据结构
 * - 快速访问：O(1) 查询复杂度
 */
class ComponentRegistry {
public:
    ComponentRegistry() = default;
    ~ComponentRegistry() = default;
    
    // 禁止拷贝和移动
    ComponentRegistry(const ComponentRegistry&) = delete;
    ComponentRegistry& operator=(const ComponentRegistry&) = delete;
    ComponentRegistry(ComponentRegistry&&) = delete;
    ComponentRegistry& operator=(ComponentRegistry&&) = delete;
    
    // ==================== 组件类型注册 ====================
    
    /**
     * @brief 注册组件类型
     * 
     * 必须在使用组件之前注册组件类型
     * 
     * @tparam T 组件类型
     */
    template<typename T>
    void RegisterComponent() {
        std::unique_lock lock(m_mutex);
        
        std::type_index typeIndex = std::type_index(typeid(T));
        
        if (m_componentArrays.find(typeIndex) != m_componentArrays.end()) {
            // 组件类型已经注册，直接返回
            return;
        }
        
        m_componentArrays[typeIndex] = std::make_unique<ComponentArray<T>>();
    }
    
    // ==================== 组件操作 ====================
    
    /**
     * @brief 添加组件
     * @tparam T 组件类型
     * @param entity 实体 ID
     * @param component 组件数据
     */
    template<typename T>
    void AddComponent(EntityID entity, const T& component) {
        GetComponentArrayInternal<T>()->Add(entity, component);
    }
    
    /**
     * @brief 添加组件（移动语义）
     * @tparam T 组件类型
     * @param entity 实体 ID
     * @param component 组件数据
     */
    template<typename T>
    void AddComponent(EntityID entity, T&& component) {
        using ComponentType = std::remove_reference_t<T>;
        GetComponentArrayInternal<ComponentType>()->Add(entity, std::move(component));
    }
    
    /**
     * @brief 移除组件
     * @tparam T 组件类型
     * @param entity 实体 ID
     */
    template<typename T>
    void RemoveComponent(EntityID entity) {
        GetComponentArrayInternal<T>()->Remove(entity);
    }
    
    /**
     * @brief 获取组件（可修改）
     * @tparam T 组件类型
     * @param entity 实体 ID
     * @return 组件引用
     */
    template<typename T>
    T& GetComponent(EntityID entity) {
        return GetComponentArrayInternal<T>()->Get(entity);
    }
    
    /**
     * @brief 获取组件（只读）
     * @tparam T 组件类型
     * @param entity 实体 ID
     * @return 组件常量引用
     */
    template<typename T>
    const T& GetComponent(EntityID entity) const {
        return GetComponentArrayInternal<T>()->Get(entity);
    }
    
    /**
     * @brief 检查实体是否有该组件
     * @tparam T 组件类型
     * @param entity 实体 ID
     * @return 如果有该组件返回 true
     */
    template<typename T>
    [[nodiscard]] bool HasComponent(EntityID entity) const {
        // ✅ 修复：如果组件类型未注册，应该返回 false 而不是抛出异常
        try {
            auto array = GetComponentArrayInternal<T>();
            return array ? array->Has(entity) : false;
        } catch (const std::runtime_error&) {
            // 组件类型未注册，返回 false
            return false;
        }
    }
    
    /**
     * @brief 移除实体的所有组件
     * @param entity 实体 ID
     */
    void RemoveAllComponents(EntityID entity) {
        std::shared_lock lock(m_mutex);
        size_t count = 0;
        for (auto& [typeIndex, array] : m_componentArrays) {
            array->RemoveEntity(entity);
            count++;
        }
    }
    
    // ==================== 安全的迭代接口 ====================
    
    /**
     * @brief 遍历指定类型的所有组件（安全接口）
     * @tparam T 组件类型
     * @tparam Func 回调函数类型
     * @param func 回调函数 void(EntityID, T&)
     * 
     * @note 这是推荐的迭代方式，比直接获取组件数组更安全
     * @note 在回调函数执行期间持有锁，确保线程安全
     * @note 如果组件类型未注册，方法会安全返回（不抛出异常）
     */
    template<typename T, typename Func>
    void ForEachComponent(Func&& func) {
        try {
            auto array = GetComponentArrayInternal<T>();
            if (array) {
                array->ForEach(std::forward<Func>(func));
            }
        } catch (const std::runtime_error&) {
            // 组件类型未注册，安全返回
        }
    }
    
    /**
     * @brief 遍历指定类型的所有组件（只读，安全接口）
     * @tparam T 组件类型
     * @tparam Func 回调函数类型
     * @param func 回调函数 void(EntityID, const T&)
     * 
     * @note 这是推荐的迭代方式，比直接获取组件数组更安全
     * @note 在回调函数执行期间持有锁，确保线程安全
     * @note 如果组件类型未注册，方法会安全返回（不抛出异常）
     */
    template<typename T, typename Func>
    void ForEachComponent(Func&& func) const {
        try {
            auto array = GetComponentArrayInternal<T>();
            if (array) {
                array->ForEach(std::forward<Func>(func));
            }
        } catch (const std::runtime_error&) {
            // 组件类型未注册，安全返回
        }
    }
    
    /**
     * @brief 获取具有指定组件的所有实体（安全接口）
     * @tparam T 组件类型
     * @return 实体 ID 列表
     * 
     * @note 返回的是快照，后续实体可能已被删除
     * @note 使用前应该检查实体有效性
     * @note 如果组件类型未注册，返回空列表
     */
    template<typename T>
    [[nodiscard]] std::vector<EntityID> GetEntitiesWithComponent() const {
        try {
            auto array = GetComponentArrayInternal<T>();
            if (array) {
                return array->GetEntities();
            }
        } catch (const std::runtime_error&) {
            // 组件类型未注册，返回空列表
        }
        return {};
    }
    
    /**
     * @brief 获取指定类型的组件数量
     * @tparam T 组件类型
     * @return 组件数量
     * 
     * @note 如果组件类型未注册，返回 0
     */
    template<typename T>
    [[nodiscard]] size_t GetComponentCount() const {
        try {
            auto array = GetComponentArrayInternal<T>();
            return array ? array->Size() : 0;
        } catch (const std::runtime_error&) {
            // 组件类型未注册，返回 0
            return 0;
        }
    }
    
    /**
     * @brief 清空所有组件
     */
    void Clear() {
        std::unique_lock lock(m_mutex);
        for (auto& [typeIndex, array] : m_componentArrays) {
            array->Clear();
        }
    }
    
    // ==================== 测试辅助方法 ====================
    
    /**
     * @brief 获取组件数组（测试用）
     * @tparam T 组件类型
     * @return 组件数组指针（可能为nullptr）
     * 
     * @note 此方法仅供测试使用，生产代码不应使用
     * @note 返回的指针在ComponentRegistry生命周期内有效
     */
    template<typename T>
    ComponentArray<T>* GetComponentArrayForTest() {
        try {
            return GetComponentArrayInternal<T>();
        } catch (const std::runtime_error&) {
            return nullptr;
        }
    }
    
    // ==================== 组件变化事件（新增）====================
    
    /**
     * @brief 组件变化回调函数类型
     * @tparam T 组件类型
     */
    template<typename T>
    using ComponentChangeCallback = std::function<void(EntityID, const T&)>;
    
    /**
     * @brief 注册组件变化回调
     * @tparam T 组件类型
     * @param callback 回调函数 void(EntityID, const T&)
     * @return 回调ID（用于取消注册）
     * 
     * @note 当组件被修改时，会调用所有注册的回调
     * @note 回调在组件修改后立即调用
     * @note 线程安全：回调在持有写锁的情况下调用，注意避免死锁
     * 
     * @example 使用示例
     * @code
     * auto callbackId = registry.RegisterComponentChangeCallback<TransformComponent>(
     *     [](EntityID entity, const TransformComponent& comp) {
     *         // 处理Transform变化
     *     }
     * );
     * @endcode
     */
    template<typename T>
    uint64_t RegisterComponentChangeCallback(ComponentChangeCallback<T> callback);
    
    /**
     * @brief 取消注册组件变化回调
     * @param callbackId 回调ID（由RegisterComponentChangeCallback返回）
     * 
     * @note 线程安全
     * 
     * @example 使用示例
     * @code
     * registry.UnregisterComponentChangeCallback(callbackId);
     * @endcode
     */
    void UnregisterComponentChangeCallback(uint64_t callbackId);
    
    /**
     * @brief 触发组件变化事件（内部使用）
     * @tparam T 组件类型
     * @param entity 实体ID
     * @param component 组件引用
     * 
     * @note 此方法由组件数组或World调用，用于通知组件变化
     * @note 线程安全
     * @note 回调异常不会影响其他回调的执行
     */
    template<typename T>
    void OnComponentChanged(EntityID entity, const T& component);
    
private:
    /**
     * @brief 获取指定类型的组件数组（内部方法，返回裸指针）
     * @tparam T 组件类型
     * @return 组件数组指针
     * @throws std::runtime_error 如果组件类型未注册
     * 
     * @note 此方法仅供内部使用，外部代码应使用安全的公共接口
     * @note 调用者必须确保在使用指针期间持有适当的锁
     */
    template<typename T>
    ComponentArray<T>* GetComponentArrayInternal() {
        std::shared_lock lock(m_mutex);
        
        std::type_index typeIndex = std::type_index(typeid(T));
        auto it = m_componentArrays.find(typeIndex);
        
        if (it == m_componentArrays.end()) {
            throw std::runtime_error("Component type not registered");
        }
        
        return static_cast<ComponentArray<T>*>(it->second.get());
    }
    
    /**
     * @brief 获取指定类型的组件数组（内部方法，只读）
     * @tparam T 组件类型
     * @return 组件数组指针
     * @throws std::runtime_error 如果组件类型未注册
     * 
     * @note 此方法仅供内部使用，外部代码应使用安全的公共接口
     * @note 调用者必须确保在使用指针期间持有适当的锁
     */
    template<typename T>
    const ComponentArray<T>* GetComponentArrayInternal() const {
        std::shared_lock lock(m_mutex);
        
        std::type_index typeIndex = std::type_index(typeid(T));
        auto it = m_componentArrays.find(typeIndex);
        
        if (it == m_componentArrays.end()) {
            throw std::runtime_error("Component type not registered");
        }
        
        return static_cast<const ComponentArray<T>*>(it->second.get());
    }
    
    /**
     * @brief 兼容旧代码：获取组件数组（已废弃）
     * @tparam T 组件类型
     * @return 组件数组指针
     * @deprecated 请使用 ForEachComponent 或 GetEntitiesWithComponent 替代
     * 
     * @warning 此方法返回裸指针，存在生命周期风险
     * @warning 仅为向后兼容保留，新代码不应使用
     */
    template<typename T>
    [[deprecated("Use ForEachComponent or GetEntitiesWithComponent instead")]]
    ComponentArray<T>* GetComponentArray() {
        return GetComponentArrayInternal<T>();
    }
    
    /**
     * @brief 兼容旧代码：获取组件数组（已废弃，只读）
     * @tparam T 组件类型
     * @return 组件数组指针
     * @deprecated 请使用 ForEachComponent 或 GetEntitiesWithComponent 替代
     * 
     * @warning 此方法返回裸指针，存在生命周期风险
     * @warning 仅为向后兼容保留，新代码不应使用
     */
    template<typename T>
    [[deprecated("Use ForEachComponent or GetEntitiesWithComponent instead")]]
    const ComponentArray<T>* GetComponentArray() const {
        return GetComponentArrayInternal<T>();
    }
    
    // ==================== 组件变化事件回调存储 ====================
    
    /**
     * @brief 组件变化回调记录
     */
    struct ComponentChangeCallbackRecord {
        uint64_t id;                                    ///< 回调ID
        std::type_index componentType;                 ///< 组件类型
        std::function<void(EntityID, const void*)> callback;  ///< 类型擦除的回调函数
        
        ComponentChangeCallbackRecord(uint64_t id, std::type_index type, 
                                      std::function<void(EntityID, const void*)> cb)
            : id(id), componentType(type), callback(std::move(cb)) {}
    };
    
    std::atomic<uint64_t> m_nextCallbackId{1};        ///< 下一个回调ID
    std::vector<ComponentChangeCallbackRecord> m_componentChangeCallbacks;  ///< 回调列表
    mutable std::mutex m_callbackMutex;                 ///< 回调列表的互斥锁
    
    std::unordered_map<std::type_index, std::unique_ptr<IComponentArray>> m_componentArrays;
    mutable std::shared_mutex m_mutex;
};

// ==================== 组件变化事件模板方法实现 ====================

template<typename T>
uint64_t ComponentRegistry::RegisterComponentChangeCallback(ComponentChangeCallback<T> callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    
    // 生成唯一回调ID
    uint64_t callbackId = m_nextCallbackId.fetch_add(1);
    
    // 获取组件类型
    std::type_index typeIndex = std::type_index(typeid(T));
    
    // 创建类型擦除的回调包装
    auto typeErasedCallback = [callback](EntityID entity, const void* componentPtr) {
        // 类型安全的转换
        const T* typedComponent = static_cast<const T*>(componentPtr);
        callback(entity, *typedComponent);
    };
    
    // 存储到回调列表
    m_componentChangeCallbacks.emplace_back(
        callbackId, 
        typeIndex, 
        std::move(typeErasedCallback)
    );
    
    return callbackId;
}

inline void ComponentRegistry::UnregisterComponentChangeCallback(uint64_t callbackId) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    
    // 从回调列表中移除指定ID的回调
    auto it = std::remove_if(
        m_componentChangeCallbacks.begin(),
        m_componentChangeCallbacks.end(),
        [callbackId](const ComponentChangeCallbackRecord& record) {
            return record.id == callbackId;
        }
    );
    
    m_componentChangeCallbacks.erase(it, m_componentChangeCallbacks.end());
}

template<typename T>
void ComponentRegistry::OnComponentChanged(EntityID entity, const T& component) {
    std::type_index typeIndex = std::type_index(typeid(T));
    
    Logger::GetInstance().DebugFormat(
        "[ComponentRegistry] OnComponentChanged called for entity %u, type=%s, total callbacks=%zu",
        entity.index, typeIndex.name(), m_componentChangeCallbacks.size()
    );
    
    // 获取回调列表的副本（避免在回调执行期间持有锁）
    std::vector<ComponentChangeCallbackRecord> callbacksToInvoke;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        
        // 查找所有匹配组件类型的回调
        for (const auto& record : m_componentChangeCallbacks) {
            if (record.componentType == typeIndex) {
                callbacksToInvoke.push_back(record);
            }
        }
    }
    
    Logger::GetInstance().DebugFormat(
        "[ComponentRegistry] Found %zu matching callbacks for entity %u", 
        callbacksToInvoke.size(), entity.index
    );
    
    // 调用每个回调（不在持有锁的情况下调用，避免死锁）
    // 处理回调异常，避免一个回调失败影响其他回调
    for (size_t i = 0; i < callbacksToInvoke.size(); ++i) {
        try {
            const auto& record = callbacksToInvoke[i];
            if (record.callback) {
                Logger::GetInstance().DebugFormat(
                    "[ComponentRegistry] Invoking callback %zu for entity %u", i, entity.index
                );
                record.callback(entity, &component);
            } else {
                Logger::GetInstance().WarningFormat(
                    "[ComponentRegistry] Callback %zu for entity %u is null", i, entity.index
                );
            }
        } catch (const std::exception& e) {
            Logger::GetInstance().WarningFormat(
                "[ComponentRegistry] Exception in callback %zu for entity %u: %s", 
                i, entity.index, e.what()
            );
        } catch (...) {
            // 忽略回调异常，继续执行其他回调
            Logger::GetInstance().WarningFormat(
                "[ComponentRegistry] Unknown exception in callback %zu for entity %u", 
                i, entity.index
            );
        }
    }
}

} // namespace ECS
} // namespace Render

