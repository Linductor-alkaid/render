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
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <typeindex>
#include <stdexcept>

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
     */
    void Add(EntityID entity, const T& component) {
        std::unique_lock lock(m_mutex);
        m_components[entity] = component;
    }
    
    /**
     * @brief 添加组件（移动语义）
     * @param entity 实体 ID
     * @param component 组件数据
     */
    void Add(EntityID entity, T&& component) {
        std::unique_lock lock(m_mutex);
        m_components[entity] = std::move(component);
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
    
private:
    std::unordered_map<EntityID, T, EntityID::Hash> m_components;
    mutable std::shared_mutex m_mutex;  ///< 线程安全锁
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
        GetComponentArrayInternal<T>()->Add(entity, std::move(component));
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
    
    std::unordered_map<std::type_index, std::unique_ptr<IComponentArray>> m_componentArrays;
    mutable std::shared_mutex m_mutex;
};

} // namespace ECS
} // namespace Render

