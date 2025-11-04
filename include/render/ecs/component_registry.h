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
        GetComponentArray<T>()->Add(entity, component);
    }
    
    /**
     * @brief 添加组件（移动语义）
     * @tparam T 组件类型
     * @param entity 实体 ID
     * @param component 组件数据
     */
    template<typename T>
    void AddComponent(EntityID entity, T&& component) {
        GetComponentArray<T>()->Add(entity, std::move(component));
    }
    
    /**
     * @brief 移除组件
     * @tparam T 组件类型
     * @param entity 实体 ID
     */
    template<typename T>
    void RemoveComponent(EntityID entity) {
        GetComponentArray<T>()->Remove(entity);
    }
    
    /**
     * @brief 获取组件（可修改）
     * @tparam T 组件类型
     * @param entity 实体 ID
     * @return 组件引用
     */
    template<typename T>
    T& GetComponent(EntityID entity) {
        return GetComponentArray<T>()->Get(entity);
    }
    
    /**
     * @brief 获取组件（只读）
     * @tparam T 组件类型
     * @param entity 实体 ID
     * @return 组件常量引用
     */
    template<typename T>
    const T& GetComponent(EntityID entity) const {
        return GetComponentArray<T>()->Get(entity);
    }
    
    /**
     * @brief 检查实体是否有该组件
     * @tparam T 组件类型
     * @param entity 实体 ID
     * @return 如果有该组件返回 true
     */
    template<typename T>
    [[nodiscard]] bool HasComponent(EntityID entity) const {
        auto array = GetComponentArray<T>();
        return array ? array->Has(entity) : false;
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
    
    /**
     * @brief 获取指定类型的组件数组
     * @tparam T 组件类型
     * @return 组件数组指针
     */
    template<typename T>
    ComponentArray<T>* GetComponentArray() {
        std::shared_lock lock(m_mutex);
        
        std::type_index typeIndex = std::type_index(typeid(T));
        auto it = m_componentArrays.find(typeIndex);
        
        if (it == m_componentArrays.end()) {
            throw std::runtime_error("Component type not registered");
        }
        
        return static_cast<ComponentArray<T>*>(it->second.get());
    }
    
    /**
     * @brief 获取指定类型的组件数组（只读）
     * @tparam T 组件类型
     * @return 组件数组指针
     */
    template<typename T>
    const ComponentArray<T>* GetComponentArray() const {
        std::shared_lock lock(m_mutex);
        
        std::type_index typeIndex = std::type_index(typeid(T));
        auto it = m_componentArrays.find(typeIndex);
        
        if (it == m_componentArrays.end()) {
            throw std::runtime_error("Component type not registered");
        }
        
        return static_cast<const ComponentArray<T>*>(it->second.get());
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
    std::unordered_map<std::type_index, std::unique_ptr<IComponentArray>> m_componentArrays;
    mutable std::shared_mutex m_mutex;
};

} // namespace ECS
} // namespace Render

