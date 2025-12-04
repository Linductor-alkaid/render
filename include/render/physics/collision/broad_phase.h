#pragma once

#include "render/types.h"
#include "render/ecs/entity.h"
#include "render/physics/physics_components.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace Render {
namespace Physics {

/**
 * @brief 粗检测阶段抽象基类
 * 
 * 粗检测用于快速筛选出可能发生碰撞的物体对
 * 减少细检测的计算量
 */
class BroadPhase {
public:
    using EntityPair = std::pair<ECS::EntityID, ECS::EntityID>;
    
    virtual ~BroadPhase() = default;
    
    /**
     * @brief 更新空间分区结构
     * @param entities 所有需要检测的实体及其碰撞体信息
     */
    virtual void Update(const std::vector<std::pair<ECS::EntityID, AABB>>& entities) = 0;
    
    /**
     * @brief 检测可能碰撞的实体对
     * @return 实体对列表
     */
    virtual std::vector<EntityPair> DetectPairs() = 0;
    
    /**
     * @brief 清空数据结构
     */
    virtual void Clear() = 0;
    
    /**
     * @brief 获取统计信息
     */
    virtual size_t GetCellCount() const { return 0; }
    virtual size_t GetObjectCount() const { return 0; }
};

// ============================================================================
// 空间哈希粗检测
// ============================================================================

/**
 * @brief 空间哈希粗检测实现
 * 
 * 将空间划分为均匀格子，物体按位置哈希到对应格子
 * 只检测同一格子或相邻格子中的物体
 * 
 * 优点：
 * - 实现简单
 * - 更新快速
 * - 适合动态场景
 * 
 * 缺点：
 * - 物体大小差异大时效率降低
 * - 格子大小需要调优
 */
class SpatialHashBroadPhase : public BroadPhase {
public:
    /**
     * @brief 构造函数
     * @param cellSize 格子大小（世界单位）
     */
    explicit SpatialHashBroadPhase(float cellSize = 5.0f)
        : m_cellSize(std::max(cellSize, 0.1f)) {}
    
    void Update(const std::vector<std::pair<ECS::EntityID, AABB>>& entities) override;
    std::vector<EntityPair> DetectPairs() override;
    void Clear() override;
    
    size_t GetCellCount() const override { return m_spatialHash.size(); }
    size_t GetObjectCount() const override { return m_objectCount; }
    
    /**
     * @brief 设置格子大小
     */
    void SetCellSize(float cellSize) { 
        m_cellSize = std::max(cellSize, 0.1f); 
    }
    
    float GetCellSize() const { return m_cellSize; }
    
private:
    /// 格子大小
    float m_cellSize;
    
    /// 空间哈希表：格子哈希 -> 实体列表
    std::unordered_map<uint64_t, std::vector<ECS::EntityID>> m_spatialHash;
    
    /// 物体总数
    size_t m_objectCount = 0;
    
    /**
     * @brief 将世界坐标转换为格子坐标
     */
    struct CellCoord {
        int x, y, z;
        
        bool operator==(const CellCoord& other) const {
            return x == other.x && y == other.y && z == other.z;
        }
    };
    
    CellCoord WorldToCell(const Vector3& worldPos) const {
        return CellCoord{
            static_cast<int>(std::floor(worldPos.x() / m_cellSize)),
            static_cast<int>(std::floor(worldPos.y() / m_cellSize)),
            static_cast<int>(std::floor(worldPos.z() / m_cellSize))
        };
    }
    
    /**
     * @brief 计算格子的哈希值
     */
    uint64_t HashCell(int x, int y, int z) const {
        // 使用大素数组合哈希
        const uint64_t p1 = 73856093;
        const uint64_t p2 = 19349663;
        const uint64_t p3 = 83492791;
        
        return ((static_cast<uint64_t>(x) * p1) ^
                (static_cast<uint64_t>(y) * p2) ^
                (static_cast<uint64_t>(z) * p3));
    }
    
    uint64_t HashCell(const CellCoord& coord) const {
        return HashCell(coord.x, coord.y, coord.z);
    }
    
    /**
     * @brief 计算实体对的哈希（用于去重）
     */
    uint64_t HashPair(ECS::EntityID a, ECS::EntityID b) const {
        // 确保顺序一致（小的在前）
        if (a.index > b.index) std::swap(a, b);
        return (static_cast<uint64_t>(a.index) << 32) | static_cast<uint64_t>(b.index);
    }
};

// ============================================================================
// 八叉树粗检测
// ============================================================================

/**
 * @brief 八叉树粗检测实现
 * 
 * 递归细分空间为八个子空间，将物体分配到叶节点
 * 
 * 优点：
 * - 适合静态场景
 * - 空间利用率高
 * - 查询效率好
 * 
 * 缺点：
 * - 动态场景更新开销大
 * - 实现复杂
 */
class OctreeBroadPhase : public BroadPhase {
public:
    /**
     * @brief 构造函数
     * @param bounds 世界边界
     * @param maxDepth 最大深度
     * @param maxObjectsPerNode 每个节点最多容纳的物体数
     */
    explicit OctreeBroadPhase(const AABB& bounds = AABB(Vector3(-100, -100, -100), Vector3(100, 100, 100)),
                              int maxDepth = 8,
                              int maxObjectsPerNode = 8)
        : m_maxDepth(maxDepth)
        , m_maxObjectsPerNode(maxObjectsPerNode)
        , m_root(std::make_unique<OctreeNode>(bounds, 0, maxDepth)) {}
    
    void Update(const std::vector<std::pair<ECS::EntityID, AABB>>& entities) override;
    std::vector<EntityPair> DetectPairs() override;
    void Clear() override;
    
    size_t GetCellCount() const override { return m_nodeCount; }
    size_t GetObjectCount() const override { return m_objectCount; }
    
private:
    /**
     * @brief 八叉树节点
     */
    struct OctreeNode {
        AABB bounds;                                    // 节点边界
        int depth;                                      // 当前深度
        int maxDepth;                                   // 最大深度
        std::vector<std::pair<ECS::EntityID, AABB>> objects;  // 存储的物体
        std::unique_ptr<OctreeNode> children[8];        // 8个子节点
        
        OctreeNode(const AABB& b, int d, int md)
            : bounds(b), depth(d), maxDepth(md) {}
        
        /**
         * @brief 是否为叶节点
         */
        bool IsLeaf() const {
            return children[0] == nullptr;
        }
        
        /**
         * @brief 细分节点
         */
        void Subdivide();
        
        /**
         * @brief 插入物体
         */
        void Insert(ECS::EntityID entity, const AABB& aabb, int maxObjectsPerNode);
        
        /**
         * @brief 查询可能碰撞的物体对
         */
        void QueryPairs(std::vector<EntityPair>& pairs, std::unordered_set<uint64_t>& processed) const;
        
        /**
         * @brief 清空节点
         */
        void Clear();
    };
    
    int m_maxDepth;
    int m_maxObjectsPerNode;
    std::unique_ptr<OctreeNode> m_root;
    size_t m_nodeCount = 0;
    size_t m_objectCount = 0;
    
    /**
     * @brief 计算实体对哈希
     */
    static uint64_t HashPair(ECS::EntityID a, ECS::EntityID b) {
        if (a.index > b.index) std::swap(a, b);
        return (static_cast<uint64_t>(a.index) << 32) | static_cast<uint64_t>(b.index);
    }
    
    /**
     * @brief 递归计算节点数
     */
    size_t CountNodes(const OctreeNode* node) const;
};

} // namespace Physics
} // namespace Render

