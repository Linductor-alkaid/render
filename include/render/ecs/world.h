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
#include "entity_manager.h"
#include "component_registry.h"
#include "system.h"
#include <vector>
#include <memory>
#include <shared_mutex>
#include <algorithm>
#include <chrono>

namespace Render {
namespace ECS {

/**
 * @brief World（世界）
 * 
 * ECS 的顶层容器，管理所有实体、组件和系统
 * - 提供统一的实体、组件、系统管理接口
 * - 系统自动按优先级排序
 * - 线程安全的所有操作
 * - 提供性能监控和统计
 * 
 * 注意：为了支持异步回调的安全生命周期管理，World继承自enable_shared_from_this
 * 建议使用std::make_shared<World>()创建World对象
 */
class World : public std::enable_shared_from_this<World> {
public:
    World();
    ~World();
    
    // 禁止拷贝和移动
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) = delete;
    World& operator=(World&&) = delete;
    
    // ==================== 初始化/清理 ====================
    
    /**
     * @brief 初始化 World
     */
    void Initialize();
    
    /**
     * @brief 后初始化（在所有系统注册完成后调用）
     * 
     * 允许系统在此阶段获取对其他系统的引用，不会产生死锁
     */
    void PostInitialize();
    
    /**
     * @brief 关闭 World
     */
    void Shutdown();
    
    /**
     * @brief 检查是否已初始化
     * @return 如果已初始化返回 true
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }
    
    // ==================== 实体管理 ====================
    
    /**
     * @brief 创建实体
     * @param desc 实体描述符
     * @return 新创建的实体 ID
     */
    EntityID CreateEntity(const EntityDescriptor& desc = {});
    
    /**
     * @brief 销毁实体
     * @param entity 实体 ID
     */
    void DestroyEntity(EntityID entity);
    
    /**
     * @brief 检查实体是否有效
     * @param entity 实体 ID
     * @return 如果实体有效返回 true
     */
    [[nodiscard]] bool IsValidEntity(EntityID entity) const;
    
    // ==================== 组件管理 ====================
    
    /**
     * @brief 注册组件类型
     * 
     * 必须在使用组件之前注册组件类型
     * 
     * @tparam T 组件类型
     */
    template<typename T>
    void RegisterComponent() {
        m_componentRegistry.RegisterComponent<T>();
    }
    
    /**
     * @brief 添加组件
     * @tparam T 组件类型
     * @param entity 实体 ID
     * @param component 组件数据
     */
    template<typename T>
    void AddComponent(EntityID entity, const T& component) {
        m_componentRegistry.AddComponent(entity, component);
    }
    
    /**
     * @brief 添加组件（移动语义）
     * @tparam T 组件类型
     * @param entity 实体 ID
     * @param component 组件数据
     */
    template<typename T>
    void AddComponent(EntityID entity, T&& component) {
        m_componentRegistry.AddComponent(entity, std::move(component));
    }
    
    /**
     * @brief 移除组件
     * @tparam T 组件类型
     * @param entity 实体 ID
     */
    template<typename T>
    void RemoveComponent(EntityID entity) {
        m_componentRegistry.RemoveComponent<T>(entity);
    }
    
    /**
     * @brief 获取组件（可修改）
     * @tparam T 组件类型
     * @param entity 实体 ID
     * @return 组件引用
     */
    template<typename T>
    T& GetComponent(EntityID entity) {
        return m_componentRegistry.GetComponent<T>(entity);
    }
    
    /**
     * @brief 获取组件（只读）
     * @tparam T 组件类型
     * @param entity 实体 ID
     * @return 组件常量引用
     */
    template<typename T>
    const T& GetComponent(EntityID entity) const {
        return m_componentRegistry.GetComponent<T>(entity);
    }
    
    /**
     * @brief 检查实体是否有该组件
     * @tparam T 组件类型
     * @param entity 实体 ID
     * @return 如果有该组件返回 true
     */
    template<typename T>
    [[nodiscard]] bool HasComponent(EntityID entity) const {
        return m_componentRegistry.HasComponent<T>(entity);
    }
    
    // ==================== 系统管理 ====================
    
    /**
     * @brief 注册系统
     * @tparam T 系统类型
     * @tparam Args 构造函数参数类型
     * @param args 构造函数参数
     * @return 系统指针
     */
    template<typename T, typename... Args>
    T* RegisterSystem(Args&&... args) {
        std::unique_lock lock(m_mutex);
        
        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        T* systemPtr = system.get();
        
        m_systems.push_back(std::move(system));
        
        // 初始化系统
        systemPtr->OnCreate(this);
        
        // 按优先级排序
        SortSystems();
        
        return systemPtr;
    }
    
    /**
     * @brief 获取系统
     * @tparam T 系统类型
     * @return 系统指针，如果未找到返回 nullptr
     */
    template<typename T>
    T* GetSystem() {
        std::shared_lock lock(m_mutex);
        return GetSystemNoLock<T>();
    }
    
    /**
     * @brief 获取系统（内部使用，不加锁）
     * @tparam T 系统类型
     * @return 系统指针，如果未找到返回 nullptr
     */
    template<typename T>
    T* GetSystemNoLock() {
        for (auto& system : m_systems) {
            T* casted = dynamic_cast<T*>(system.get());
            if (casted) {
                return casted;
            }
        }
        
        return nullptr;
    }
    
    /**
     * @brief 移除系统
     * @tparam T 系统类型
     */
    template<typename T>
    void RemoveSystem() {
        std::unique_lock lock(m_mutex);
        
        auto it = std::remove_if(m_systems.begin(), m_systems.end(),
            [](const std::unique_ptr<System>& system) {
                T* casted = dynamic_cast<T*>(system.get());
                if (casted) {
                    casted->OnDestroy();
                    return true;
                }
                return false;
            });
        
        m_systems.erase(it, m_systems.end());
    }
    
    // ==================== 查询 ====================
    
    /**
     * @brief 查询具有特定组件的实体
     * 
     * 示例：
     * ```cpp
     * auto entities = world.Query<TransformComponent, MeshRenderComponent>();
     * ```
     * 
     * @tparam Components 组件类型列表
     * @return 具有所有指定组件的实体列表
     */
    template<typename... Components>
    [[nodiscard]] std::vector<EntityID> Query() const {
        std::vector<EntityID> result;
        
        // 获取所有实体
        auto allEntities = m_entityManager.GetAllEntities();
        
        // 过滤出具有所有指定组件的实体
        for (const auto& entity : allEntities) {
            if ((m_componentRegistry.HasComponent<Components>(entity) && ...)) {
                result.push_back(entity);
            }
        }
        
        return result;
    }
    
    /**
     * @brief 按标签查询实体
     * @param tag 标签名称
     * @return 具有该标签的实体列表
     */
    [[nodiscard]] std::vector<EntityID> QueryByTag(const std::string& tag) const {
        return m_entityManager.GetEntitiesWithTag(tag);
    }
    
    // ==================== 更新 ====================
    
    /**
     * @brief 更新 World（调用所有系统的 Update）
     * @param deltaTime 帧间隔时间（秒）
     */
    void Update(float deltaTime);
    
    // ==================== 辅助接口 ====================
    
    /**
     * @brief 获取 EntityManager
     * @return EntityManager 引用
     */
    EntityManager& GetEntityManager() { return m_entityManager; }
    
    /**
     * @brief 获取 ComponentRegistry
     * @return ComponentRegistry 引用
     */
    ComponentRegistry& GetComponentRegistry() { return m_componentRegistry; }
    
    // ==================== 统计信息 ====================
    
    /**
     * @brief 统计信息结构体
     */
    struct Statistics {
        size_t entityCount = 0;          ///< 实体总数
        size_t activeEntityCount = 0;    ///< 激活实体数量
        size_t systemCount = 0;          ///< 系统数量
        float lastUpdateTime = 0.0f;     ///< 上次更新耗时（毫秒）
    };
    
    /**
     * @brief 获取统计信息
     * @return 统计信息
     */
    [[nodiscard]] const Statistics& GetStatistics() const { return m_stats; }
    
    /**
     * @brief 打印统计信息到日志
     */
    void PrintStatistics() const;
    
private:
    /**
     * @brief 按优先级排序系统
     */
    void SortSystems();
    
    EntityManager m_entityManager;         ///< 实体管理器
    ComponentRegistry m_componentRegistry; ///< 组件注册表
    std::vector<std::unique_ptr<System>> m_systems;  ///< 系统列表
    
    Statistics m_stats;                    ///< 统计信息
    bool m_initialized = false;            ///< 是否已初始化
    
    mutable std::shared_mutex m_mutex;     ///< 线程安全锁
};

} // namespace ECS
} // namespace Render

