#include "render/physics/collision/broad_phase.h"

namespace Render {
namespace Physics {

void SpatialHashBroadPhase::Update(const std::vector<std::pair<ECS::EntityID, AABB>>& entities) {
    // 清空旧数据
    m_spatialHash.clear();
    m_objectCount = entities.size();
    
    // 将每个实体插入到对应的格子中
    for (const auto& [entityId, aabb] : entities) {
        // 计算 AABB 覆盖的格子范围
        CellCoord minCell = WorldToCell(aabb.min);
        CellCoord maxCell = WorldToCell(aabb.max);
        
        // 遍历所有覆盖的格子
        for (int x = minCell.x; x <= maxCell.x; ++x) {
            for (int y = minCell.y; y <= maxCell.y; ++y) {
                for (int z = minCell.z; z <= maxCell.z; ++z) {
                    uint64_t cellHash = HashCell(x, y, z);
                    m_spatialHash[cellHash].push_back(entityId);
                }
            }
        }
    }
}

std::vector<SpatialHashBroadPhase::EntityPair> SpatialHashBroadPhase::DetectPairs() {
    std::vector<EntityPair> pairs;
    std::unordered_set<uint64_t> processedPairs;
    
    // 遍历每个格子
    for (const auto& [cellHash, entities] : m_spatialHash) {
        // 检查同一格子内的实体对
        size_t count = entities.size();
        for (size_t i = 0; i < count; ++i) {
            for (size_t j = i + 1; j < count; ++j) {
                ECS::EntityID entityA = entities[i];
                ECS::EntityID entityB = entities[j];
                
                // 去重：同一对物体可能在多个格子中
                uint64_t pairHash = HashPair(entityA, entityB);
                if (processedPairs.insert(pairHash).second) {
                    pairs.push_back({entityA, entityB});
                }
            }
        }
    }
    
    return pairs;
}

void SpatialHashBroadPhase::Clear() {
    m_spatialHash.clear();
    m_objectCount = 0;
}

// ============================================================================
// OctreeBroadPhase 实现
// ============================================================================

void OctreeBroadPhase::OctreeNode::Subdivide() {
    if (!IsLeaf()) return;  // 已经细分
    
    Vector3 center = bounds.GetCenter();
    Vector3 halfExtents = bounds.GetExtents();
    Vector3 quarterExtents = halfExtents * 0.5f;
    
    // 创建 8 个子节点
    for (int i = 0; i < 8; ++i) {
        Vector3 offset(
            (i & 1) ? quarterExtents.x() : -quarterExtents.x(),
            (i & 2) ? quarterExtents.y() : -quarterExtents.y(),
            (i & 4) ? quarterExtents.z() : -quarterExtents.z()
        );
        
        Vector3 childCenter = center + offset;
        AABB childBounds(childCenter - quarterExtents, childCenter + quarterExtents);
        
        children[i] = std::make_unique<OctreeNode>(childBounds, depth + 1, maxDepth);
    }
}

void OctreeBroadPhase::OctreeNode::Insert(ECS::EntityID entity, const AABB& aabb, int maxObjectsPerNode) {
    // 如果是叶节点且未达到容量限制，直接添加
    if (IsLeaf()) {
        objects.push_back({entity, aabb});
        
        // 如果超过容量且未达到最大深度，细分
        if (static_cast<int>(objects.size()) > maxObjectsPerNode && depth < maxDepth) {
            Subdivide();
            
            // 重新分配物体到子节点
            std::vector<std::pair<ECS::EntityID, AABB>> oldObjects = std::move(objects);
            objects.clear();
            
            for (const auto& [id, objAABB] : oldObjects) {
                Insert(id, objAABB, maxObjectsPerNode);
            }
        }
        return;
    }
    
    // 非叶节点：尝试插入到子节点
    bool inserted = false;
    for (int i = 0; i < 8; ++i) {
        if (children[i] && children[i]->bounds.Intersects(aabb)) {
            // 如果 AABB 完全在子节点内，插入子节点
            Vector3 aabbCenter = aabb.GetCenter();
            Vector3 aabbExtents = aabb.GetExtents();
            
            AABB childBounds = children[i]->bounds;
            bool fullyInside = 
                (aabbCenter.x() - aabbExtents.x() >= childBounds.min.x()) &&
                (aabbCenter.x() + aabbExtents.x() <= childBounds.max.x()) &&
                (aabbCenter.y() - aabbExtents.y() >= childBounds.min.y()) &&
                (aabbCenter.y() + aabbExtents.y() <= childBounds.max.y()) &&
                (aabbCenter.z() - aabbExtents.z() >= childBounds.min.z()) &&
                (aabbCenter.z() + aabbExtents.z() <= childBounds.max.z());
            
            if (fullyInside) {
                children[i]->Insert(entity, aabb, maxObjectsPerNode);
                inserted = true;
                break;
            }
        }
    }
    
    // 如果无法完全插入任何子节点，存储在当前节点
    if (!inserted) {
        objects.push_back({entity, aabb});
    }
}

void OctreeBroadPhase::OctreeNode::QueryPairs(std::vector<EntityPair>& pairs, 
                                                std::unordered_set<uint64_t>& processed) const {
    // 检查当前节点中的物体对
    size_t count = objects.size();
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = i + 1; j < count; ++j) {
            ECS::EntityID a = objects[i].first;
            ECS::EntityID b = objects[j].first;
            
            uint64_t pairHash = OctreeBroadPhase::HashPair(a, b);
            if (processed.insert(pairHash).second) {
                pairs.push_back({a, b});
            }
        }
        
        // 检查当前节点物体与子节点中物体的碰撞
        for (int k = 0; k < 8; ++k) {
            if (children[k]) {
                for (const auto& [childEntity, childAABB] : children[k]->objects) {
                    ECS::EntityID a = objects[i].first;
                    ECS::EntityID b = childEntity;
                    
                    uint64_t pairHash = OctreeBroadPhase::HashPair(a, b);
                    if (processed.insert(pairHash).second) {
                        pairs.push_back({a, b});
                    }
                }
            }
        }
    }
    
    // 递归查询子节点
    for (int i = 0; i < 8; ++i) {
        if (children[i]) {
            children[i]->QueryPairs(pairs, processed);
        }
    }
}

void OctreeBroadPhase::OctreeNode::Clear() {
    objects.clear();
    for (int i = 0; i < 8; ++i) {
        if (children[i]) {
            children[i]->Clear();
            children[i].reset();
        }
    }
}

void OctreeBroadPhase::Update(const std::vector<std::pair<ECS::EntityID, AABB>>& entities) {
    // 清空旧数据
    m_root->Clear();
    m_objectCount = entities.size();
    m_nodeCount = 1;
    
    // 插入所有实体
    for (const auto& [entityId, aabb] : entities) {
        m_root->Insert(entityId, aabb, m_maxObjectsPerNode);
    }
    
    // 统计节点数（递归计数）
    m_nodeCount = CountNodes(m_root.get());
}

std::vector<OctreeBroadPhase::EntityPair> OctreeBroadPhase::DetectPairs() {
    std::vector<EntityPair> pairs;
    std::unordered_set<uint64_t> processed;
    
    m_root->QueryPairs(pairs, processed);
    
    return pairs;
}

void OctreeBroadPhase::Clear() {
    m_root->Clear();
    m_objectCount = 0;
    m_nodeCount = 1;
}

size_t OctreeBroadPhase::CountNodes(const OctreeNode* node) const {
    if (!node) return 0;
    
    size_t count = 1;
    for (int i = 0; i < 8; ++i) {
        if (node->children[i]) {
            count += CountNodes(node->children[i].get());
        }
    }
    return count;
}

} // namespace Physics
} // namespace Render

