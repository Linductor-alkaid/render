#include "render/ecs/entity_manager.h"
#include "render/logger.h"
#include <algorithm>

namespace Render {
namespace ECS {

EntityManager::EntityManager() {
    Logger::GetInstance().InfoFormat("[EntityManager] EntityManager initialized");
}

EntityManager::~EntityManager() {
    Clear();
    Logger::GetInstance().InfoFormat("[EntityManager] EntityManager destroyed");
}

EntityID EntityManager::CreateEntity(const EntityDescriptor& desc) {
    std::unique_lock lock(m_mutex);
    
    uint32_t index;
    uint32_t version = 0;
    
    // 尝试复用空闲索引
    if (!m_freeIndices.empty()) {
        index = m_freeIndices.front();
        m_freeIndices.pop();
        version = m_entities[index].version;
    } else {
        // 创建新索引
        index = static_cast<uint32_t>(m_entities.size());
        m_entities.emplace_back();
    }
    
    // 初始化实体数据
    EntityData& data = m_entities[index];
    data.version = version;
    data.active = desc.active;
    data.name = desc.name;
    data.tags.clear();
    
    // 添加标签
    for (const auto& tag : desc.tags) {
        data.tags.insert(tag);
    }
    
    EntityID entityID{ index, version };
    
    // 更新标签索引
    for (const auto& tag : desc.tags) {
        m_tagIndex[tag].insert(entityID);
    }
    
    Logger::GetInstance().DebugFormat("[EntityManager] Created entity: index=%u, version=%u, name=\"%s\"", 
                  index, version, desc.name.c_str());
    
    return entityID;
}

void EntityManager::DestroyEntity(EntityID entity) {
    std::unique_lock lock(m_mutex);
    
    // 内部验证（不再次加锁）
    if (entity.index >= m_entities.size() || m_entities[entity.index].version != entity.version) {
        Logger::GetInstance().WarningFormat("[EntityManager] Attempted to destroy invalid entity: index=%u, version=%u", 
                       entity.index, entity.version);
        return;
    }
    
    EntityData& data = m_entities[entity.index];
    
    // 从标签索引中移除
    for (const auto& tag : data.tags) {
        auto it = m_tagIndex.find(tag);
        if (it != m_tagIndex.end()) {
            it->second.erase(entity);
            if (it->second.empty()) {
                m_tagIndex.erase(it);
            }
        }
    }
    
    // 清空数据
    data.tags.clear();
    data.name.clear();
    data.active = false;
    data.version++; // 递增版本号，使旧的 EntityID 引用失效
    
    // 将索引加入空闲队列
    m_freeIndices.push(entity.index);
    
    Logger::GetInstance().DebugFormat("[EntityManager] Destroyed entity: index=%u, new_version=%u", 
                  entity.index, data.version);
}

bool EntityManager::IsValid(EntityID entity) const {
    std::shared_lock lock(m_mutex);
    
    if (entity.index >= m_entities.size()) {
        return false;
    }
    
    return m_entities[entity.index].version == entity.version;
}

void EntityManager::SetName(EntityID entity, const std::string& name) {
    std::unique_lock lock(m_mutex);
    
    if (!IsValid(entity)) {
        Logger::GetInstance().WarningFormat("[EntityManager] Attempted to set name on invalid entity");
        return;
    }
    
    m_entities[entity.index].name = name;
}

std::string EntityManager::GetName(EntityID entity) const {
    std::shared_lock lock(m_mutex);
    
    if (!IsValid(entity)) {
        return "";
    }
    
    return m_entities[entity.index].name;
}

void EntityManager::SetActive(EntityID entity, bool active) {
    std::unique_lock lock(m_mutex);
    
    if (!IsValid(entity)) {
        Logger::GetInstance().WarningFormat("[EntityManager] Attempted to set active state on invalid entity");
        return;
    }
    
    m_entities[entity.index].active = active;
}

bool EntityManager::IsActive(EntityID entity) const {
    std::shared_lock lock(m_mutex);
    
    if (!IsValid(entity)) {
        return false;
    }
    
    return m_entities[entity.index].active;
}

void EntityManager::AddTag(EntityID entity, const std::string& tag) {
    std::unique_lock lock(m_mutex);
    
    if (!IsValid(entity)) {
        Logger::GetInstance().WarningFormat("[EntityManager] Attempted to add tag to invalid entity");
        return;
    }
    
    EntityData& data = m_entities[entity.index];
    
    // 添加到实体的标签集合
    if (data.tags.insert(tag).second) {
        // 添加到标签索引
        m_tagIndex[tag].insert(entity);
    }
}

void EntityManager::RemoveTag(EntityID entity, const std::string& tag) {
    std::unique_lock lock(m_mutex);
    
    if (!IsValid(entity)) {
        Logger::GetInstance().WarningFormat("[EntityManager] Attempted to remove tag from invalid entity");
        return;
    }
    
    EntityData& data = m_entities[entity.index];
    
    // 从实体的标签集合中移除
    if (data.tags.erase(tag) > 0) {
        // 从标签索引中移除
        auto it = m_tagIndex.find(tag);
        if (it != m_tagIndex.end()) {
            it->second.erase(entity);
            if (it->second.empty()) {
                m_tagIndex.erase(it);
            }
        }
    }
}

bool EntityManager::HasTag(EntityID entity, const std::string& tag) const {
    std::shared_lock lock(m_mutex);
    
    if (!IsValid(entity)) {
        return false;
    }
    
    const EntityData& data = m_entities[entity.index];
    return data.tags.find(tag) != data.tags.end();
}

std::vector<std::string> EntityManager::GetTags(EntityID entity) const {
    std::shared_lock lock(m_mutex);
    
    if (!IsValid(entity)) {
        return {};
    }
    
    const EntityData& data = m_entities[entity.index];
    return std::vector<std::string>(data.tags.begin(), data.tags.end());
}

std::vector<EntityID> EntityManager::GetAllEntities() const {
    std::shared_lock lock(m_mutex);
    
    std::vector<EntityID> entities;
    entities.reserve(m_entities.size());
    
    for (uint32_t i = 0; i < m_entities.size(); ++i) {
        EntityID id{ i, m_entities[i].version };
        if (IsValid(id)) {
            entities.push_back(id);
        }
    }
    
    return entities;
}

std::vector<EntityID> EntityManager::GetEntitiesWithTag(const std::string& tag) const {
    std::shared_lock lock(m_mutex);
    
    auto it = m_tagIndex.find(tag);
    if (it == m_tagIndex.end()) {
        return {};
    }
    
    // 过滤掉无效的实体
    std::vector<EntityID> entities;
    entities.reserve(it->second.size());
    
    for (const auto& entity : it->second) {
        if (IsValid(entity)) {
            entities.push_back(entity);
        }
    }
    
    return entities;
}

std::vector<EntityID> EntityManager::GetActiveEntities() const {
    std::shared_lock lock(m_mutex);
    
    std::vector<EntityID> entities;
    
    for (uint32_t i = 0; i < m_entities.size(); ++i) {
        EntityID id{ i, m_entities[i].version };
        if (IsValid(id) && m_entities[i].active) {
            entities.push_back(id);
        }
    }
    
    return entities;
}

size_t EntityManager::GetEntityCount() const {
    std::shared_lock lock(m_mutex);
    
    size_t count = 0;
    for (uint32_t i = 0; i < m_entities.size(); ++i) {
        EntityID id{ i, m_entities[i].version };
        if (IsValid(id)) {
            count++;
        }
    }
    
    return count;
}

size_t EntityManager::GetActiveEntityCount() const {
    std::shared_lock lock(m_mutex);
    
    size_t count = 0;
    for (uint32_t i = 0; i < m_entities.size(); ++i) {
        EntityID id{ i, m_entities[i].version };
        if (IsValid(id) && m_entities[i].active) {
            count++;
        }
    }
    
    return count;
}

void EntityManager::Clear() {
    std::unique_lock lock(m_mutex);
    
    m_entities.clear();
    m_tagIndex.clear();
    
    // 清空空闲索引队列
    while (!m_freeIndices.empty()) {
        m_freeIndices.pop();
    }
    
    Logger::GetInstance().InfoFormat("[EntityManager] Cleared all entities");
}

} // namespace ECS
} // namespace Render

