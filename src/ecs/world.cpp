#include "render/ecs/world.h"
#include "render/logger.h"
#include <algorithm>

namespace Render {
namespace ECS {

World::World() {
    Logger::GetInstance().InfoFormat("[World] World created");
}

World::~World() {
    Shutdown();
    Logger::GetInstance().InfoFormat("[World] World destroyed");
}

void World::Initialize() {
    if (m_initialized) {
        Logger::GetInstance().WarningFormat("[World] World already initialized");
        return;
    }
    
    std::unique_lock lock(m_mutex);
    
    m_initialized = true;
    
    Logger::GetInstance().InfoFormat("[World] World initialized");
}

void World::Shutdown() {
    if (!m_initialized) {
        return;
    }
    
    std::unique_lock lock(m_mutex);
    
    // 销毁所有系统
    for (auto& system : m_systems) {
        system->OnDestroy();
    }
    m_systems.clear();
    
    // 清空组件
    m_componentRegistry.Clear();
    
    // 清空实体
    m_entityManager.Clear();
    
    m_initialized = false;
    
    Logger::GetInstance().InfoFormat("[World] World shutdown");
}

EntityID World::CreateEntity(const EntityDescriptor& desc) {
    return m_entityManager.CreateEntity(desc);
}

void World::DestroyEntity(EntityID entity) {
    Logger::GetInstance().DebugFormat("[World] DestroyEntity: Removing components for entity index=%u", entity.index);
    
    // 移除实体的所有组件
    m_componentRegistry.RemoveAllComponents(entity);
    
    Logger::GetInstance().DebugFormat("[World] DestroyEntity: Components removed, destroying entity");
    
    // 销毁实体
    m_entityManager.DestroyEntity(entity);
    
    Logger::GetInstance().DebugFormat("[World] DestroyEntity: Entity destroyed");
}

bool World::IsValidEntity(EntityID entity) const {
    return m_entityManager.IsValid(entity);
}

void World::Update(float deltaTime) {
    if (!m_initialized) {
        Logger::GetInstance().WarningFormat("[World] Attempted to update uninitialized World");
        return;
    }
    
    // 记录开始时间
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 更新统计信息
    m_stats.entityCount = m_entityManager.GetEntityCount();
    m_stats.activeEntityCount = m_entityManager.GetActiveEntityCount();
    m_stats.systemCount = m_systems.size();
    
    // 更新所有启用的系统
    std::shared_lock lock(m_mutex);
    for (auto& system : m_systems) {
        if (system->IsEnabled()) {
            system->Update(deltaTime);
        }
    }
    
    // 记录结束时间
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    m_stats.lastUpdateTime = duration.count() / 1000.0f;  // 转换为毫秒
}

void World::PrintStatistics() const {
    Logger::GetInstance().InfoFormat("[World] === World Statistics ===");
    Logger::GetInstance().InfoFormat("[World]   Entity Count: %zu", m_stats.entityCount);
    Logger::GetInstance().InfoFormat("[World]   Active Entity Count: %zu", m_stats.activeEntityCount);
    Logger::GetInstance().InfoFormat("[World]   System Count: %zu", m_stats.systemCount);
    Logger::GetInstance().InfoFormat("[World]   Last Update Time: %.3f ms", m_stats.lastUpdateTime);
}

void World::SortSystems() {
    std::sort(m_systems.begin(), m_systems.end(),
        [](const std::unique_ptr<System>& a, const std::unique_ptr<System>& b) {
            return a->GetPriority() < b->GetPriority();
        });
}

} // namespace ECS
} // namespace Render

