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
#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <string>

namespace Render {
namespace ECS {

/**
 * @brief 实体管理器
 * 
 * 管理实体的创建、销毁和查询，支持：
 * - 版本号机制防止悬空引用
 * - 索引复用优化内存使用
 * - 标签系统支持快速查询
 * - 线程安全的所有操作
 */
class EntityManager {
public:
    EntityManager();
    ~EntityManager();
    
    // ==================== 实体创建/销毁 ====================
    
    /**
     * @brief 创建实体
     * @param desc 实体描述符
     * @return 新创建的实体 ID
     */
    EntityID CreateEntity(const EntityDescriptor& desc = {});
    
    /**
     * @brief 销毁实体
     * @param entity 要销毁的实体 ID
     */
    void DestroyEntity(EntityID entity);
    
    /**
     * @brief 检查实体是否有效
     * @param entity 实体 ID
     * @return 如果实体有效返回 true
     */
    [[nodiscard]] bool IsValid(EntityID entity) const;
    
    // ==================== 实体信息 ====================
    
    /**
     * @brief 设置实体名称
     * @param entity 实体 ID
     * @param name 实体名称
     */
    void SetName(EntityID entity, const std::string& name);
    
    /**
     * @brief 获取实体名称
     * @param entity 实体 ID
     * @return 实体名称
     */
    [[nodiscard]] std::string GetName(EntityID entity) const;
    
    /**
     * @brief 设置实体激活状态
     * @param entity 实体 ID
     * @param active 是否激活
     */
    void SetActive(EntityID entity, bool active);
    
    /**
     * @brief 获取实体激活状态
     * @param entity 实体 ID
     * @return 实体是否激活
     */
    [[nodiscard]] bool IsActive(EntityID entity) const;
    
    // ==================== 标签系统 ====================
    
    /**
     * @brief 添加标签
     * @param entity 实体 ID
     * @param tag 标签名称
     */
    void AddTag(EntityID entity, const std::string& tag);
    
    /**
     * @brief 移除标签
     * @param entity 实体 ID
     * @param tag 标签名称
     */
    void RemoveTag(EntityID entity, const std::string& tag);
    
    /**
     * @brief 检查实体是否有指定标签
     * @param entity 实体 ID
     * @param tag 标签名称
     * @return 如果有该标签返回 true
     */
    [[nodiscard]] bool HasTag(EntityID entity, const std::string& tag) const;
    
    /**
     * @brief 获取实体的所有标签
     * @param entity 实体 ID
     * @return 标签列表
     */
    [[nodiscard]] std::vector<std::string> GetTags(EntityID entity) const;
    
    // ==================== 查询 ====================
    
    /**
     * @brief 获取所有实体
     * @return 所有实体的 ID 列表
     */
    [[nodiscard]] std::vector<EntityID> GetAllEntities() const;
    
    /**
     * @brief 获取具有指定标签的实体
     * @param tag 标签名称
     * @return 具有该标签的实体列表
     */
    [[nodiscard]] std::vector<EntityID> GetEntitiesWithTag(const std::string& tag) const;
    
    /**
     * @brief 获取所有激活的实体
     * @return 激活的实体列表
     */
    [[nodiscard]] std::vector<EntityID> GetActiveEntities() const;
    
    // ==================== 统计 ====================
    
    /**
     * @brief 获取实体总数（包括非激活实体）
     * @return 实体总数
     */
    [[nodiscard]] size_t GetEntityCount() const;
    
    /**
     * @brief 获取激活实体数量
     * @return 激活实体数量
     */
    [[nodiscard]] size_t GetActiveEntityCount() const;
    
    /**
     * @brief 清除所有实体
     */
    void Clear();
    
private:
    /// 实体数据
    struct EntityData {
        uint32_t version;                          ///< 版本号
        bool active;                               ///< 激活状态
        std::string name;                          ///< 实体名称
        std::unordered_set<std::string> tags;      ///< 标签集合
    };
    
    /**
     * @brief 检查实体是否有效（内部方法，不加锁）
     * @param entity 实体 ID
     * @return 如果实体有效返回 true
     * 
     * @note 调用者必须已经持有 m_mutex 的锁
     * @note 此方法用于避免递归锁问题
     */
    [[nodiscard]] bool IsValidNoLock(EntityID entity) const {
        if (entity.index >= m_entities.size()) {
            return false;
        }
        return m_entities[entity.index].version == entity.version;
    }
    
    std::vector<EntityData> m_entities;            ///< 实体数据（索引对应）
    std::queue<uint32_t> m_freeIndices;            ///< 空闲索引队列（复用）
    
    /// 标签索引（用于快速按标签查询）
    std::unordered_map<std::string, std::unordered_set<EntityID, EntityID::Hash>> m_tagIndex;
    
    mutable std::shared_mutex m_mutex;             ///< 线程安全锁
};

} // namespace ECS
} // namespace Render

