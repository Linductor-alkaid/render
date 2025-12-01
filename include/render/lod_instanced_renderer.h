#pragma once

#include "render/types.h"
#include "render/mesh.h"
#include "render/material.h"
#include "render/material_sort_key.h"
#include "render/lod_system.h"
#include "render/renderer.h"
#include "render/render_state.h"
#include <vector>
#include <deque>
#include <map>
#include <string>
#include <functional>
#include <algorithm>
#include <chrono>

namespace Render {

// 前向声明
class Renderer;
class RenderState;
class Mesh;
class Material;

/**
 * @brief 实例化渲染的 Per-Instance 数据
 * 
 * 每个实例的独立参数，包括变换矩阵、颜色、自定义参数等
 */
struct InstanceData {
    Matrix4 worldMatrix;         ///< 世界变换矩阵（必需）
    Vector3 worldPosition;        ///< 世界位置（从矩阵提取，用于调试/查询）
    Color instanceColor;          ///< 实例颜色（可选，用于颜色变化）
    Vector4 customParams;         ///< 自定义参数（可选，用于特殊效果）
    float scale = 1.0f;           ///< 实例缩放（可选，如果矩阵已包含则忽略）
    uint32_t instanceID = 0;     ///< 实例 ID（用于调试）
    
    /**
     * @brief 默认构造函数
     */
    InstanceData() = default;
    
    /**
     * @brief 使用矩阵构造
     * @param matrix 世界变换矩阵
     * @param entityID 实体 ID（用于调试）
     */
    InstanceData(const Matrix4& matrix, uint32_t entityID = 0)
        : worldMatrix(matrix)
        , worldPosition(matrix.block<3, 1>(0, 3))  // 提取位置
        , instanceColor(Color::White())
        , customParams(Vector4::Zero())
        , scale(1.0f)
        , instanceID(entityID)
    {}
    
    /**
     * @brief 完整构造
     * @param matrix 世界变换矩阵
     * @param color 实例颜色
     * @param params 自定义参数
     * @param entityID 实体 ID
     */
    InstanceData(const Matrix4& matrix, const Color& color, 
                 const Vector4& params, uint32_t entityID = 0)
        : worldMatrix(matrix)
        , worldPosition(matrix.block<3, 1>(0, 3))
        , instanceColor(color)
        , customParams(params)
        , scale(1.0f)
        , instanceID(entityID)
    {}
};

/**
 * @brief LOD 实例化渲染组
 * 
 * 将相同网格、相同材质、相同 LOD 级别的实例分组
 * 用于批量实例化渲染，减少 Draw Call
 */
struct LODInstancedGroup {
    Ref<Mesh> mesh;              ///< 网格（LOD 级别对应的网格）
    Ref<Material> material;      ///< 材质（LOD 级别对应的材质）
    LODLevel lodLevel;           ///< LOD 级别
    MaterialSortKey sortKey;     ///< 材质排序键（用于排序）
    
    // 实例数据
    std::vector<InstanceData> instances;  ///< 所有实例的数据
    std::vector<ECS::EntityID> entities;  ///< 对应的实体 ID（用于调试）
    
    // ✅ 脏标记系统
    bool isDirty = true;           ///< 数据是否已改变
    size_t lastUploadedCount = 0;  ///< 上次上传的实例数（用于检测数量变化）
    
    /**
     * @brief 获取实例数量
     * @return 实例数量
     */
    [[nodiscard]] size_t GetInstanceCount() const {
        return instances.size();
    }
    
    /**
     * @brief 检查是否为空
     * @return 如果为空返回 true
     */
    [[nodiscard]] bool IsEmpty() const {
        return instances.empty();
    }
    
    /**
     * @brief 清空组
     */
    void Clear() {
        instances.clear();
        entities.clear();
        isDirty = true;  // ✅ 清空后标记为脏
    }
    
    /**
     * @brief 标记为脏（在添加实例后调用）
     */
    void MarkDirty() {
        isDirty = true;
    }
    
    /**
     * @brief 检查是否需要上传
     * @return 如果需要上传返回 true
     */
    [[nodiscard]] bool NeedsUpload() const {
        return isDirty || (lastUploadedCount != instances.size());
    }
    
    /**
     * @brief 标记为已上传
     */
    void MarkUploaded() {
        isDirty = false;
        lastUploadedCount = instances.size();
    }
};

/**
 * @brief LOD 实例化渲染器
 * 
 * 负责收集、分组和渲染 LOD 实例
 * 将相同网格、相同材质、相同 LOD 级别的实例分组，使用 GPU 实例化渲染
 * 
 * **使用示例**：
 * @code
 * LODInstancedRenderer renderer;
 * 
 * // 添加实例
 * for (EntityID entity : entities) {
 *     Matrix4 worldMatrix = GetWorldMatrix(entity);
 *     Ref<Mesh> mesh = GetLODMesh(entity);
 *     Ref<Material> material = GetLODMaterial(entity);
 *     LODLevel lod = GetLODLevel(entity);
 *     
 *     InstanceData instanceData(worldMatrix, entity);
 *     renderer.AddInstance(entity, mesh, material, instanceData, lod);
 * }
 * 
 * // 渲染所有实例
 * renderer.RenderAll(rendererPtr, renderStatePtr);
 * @endcode
 */
class LODInstancedRenderer {
public:
    /**
     * @brief 构造函数
     */
    LODInstancedRenderer() = default;
    
    /**
     * @brief 析构函数
     * 清理所有实例化 VBO
     */
    ~LODInstancedRenderer();
    
    // 禁用拷贝
    LODInstancedRenderer(const LODInstancedRenderer&) = delete;
    LODInstancedRenderer& operator=(const LODInstancedRenderer&) = delete;
    
    /**
     * @brief 添加实例（简化版本，只使用矩阵）
     * 
     * @param entity 实体 ID
     * @param mesh 网格（已选择 LOD 级别）
     * @param material 材质
     * @param worldMatrix 世界变换矩阵
     * @param lodLevel LOD 级别
     */
    void AddInstance(
        ECS::EntityID entity,
        Ref<Mesh> mesh,
        Ref<Material> material,
        const Matrix4& worldMatrix,
        LODLevel lodLevel
    );
    
    /**
     * @brief 添加实例（完整版本，支持所有 per-instance 数据）
     * 
     * @param entity 实体 ID
     * @param mesh 网格（已选择 LOD 级别）
     * @param material 材质
     * @param instanceData 实例数据（包含矩阵、颜色、自定义参数等）
     * @param lodLevel LOD 级别
     */
    void AddInstance(
        ECS::EntityID entity,
        Ref<Mesh> mesh,
        Ref<Material> material,
        const InstanceData& instanceData,
        LODLevel lodLevel
    );
    
    /**
     * @brief 渲染所有实例组
     * 
     * 按材质排序键排序，然后逐个渲染每个组
     * 
     * @param renderer 渲染器
     * @param renderState 渲染状态（可选）
     */
    void RenderAll(Renderer* renderer, RenderState* renderState = nullptr);
    
    /**
     * @brief 清空所有组
     */
    void Clear();
    
    /**
     * @brief 获取统计信息
     */
    struct Stats {
        size_t groupCount = 0;        ///< 组数量
        size_t totalInstances = 0;   ///< 总实例数
        size_t drawCalls = 0;        ///< Draw Call 数量（每个组一次）
        
        // 按 LOD 级别统计
        size_t lod0Instances = 0;    ///< LOD0 实例数
        size_t lod1Instances = 0;    ///< LOD1 实例数
        size_t lod2Instances = 0;    ///< LOD2 实例数
        size_t lod3Instances = 0;    ///< LOD3 实例数
        size_t culledCount = 0;      ///< 剔除数量
        
        // ✅ 新增性能指标
        size_t vboUploadCount = 0;        ///< VBO上传次数
        size_t bytesUploaded = 0;         ///< 总上传字节数
        float uploadTimeMs = 0.0f;       ///< 上传耗时(ms)
        size_t pendingCount = 0;          ///< 待处理实例数
        float sortTimeMs = 0.0f;         ///< 排序耗时(ms)
        float renderTimeMs = 0.0f;       ///< 渲染耗时(ms)
        
        // ✅ 内存统计
        size_t totalAllocatedMemory = 0;  ///< 总分配内存(bytes)
        size_t peakInstanceCount = 0;     ///< 峰值实例数
    };
    
    /**
     * @brief 获取统计信息
     * @return 统计信息
     */
    [[nodiscard]] Stats GetStats() const;
    
    /**
     * @brief 获取指定 LOD 级别的实例数量
     * @param lodLevel LOD 级别
     * @return 实例数量
     */
    [[nodiscard]] size_t GetInstanceCount(LODLevel lodLevel) const;
    
    /**
     * @brief 获取组数量
     * @return 组数量
     */
    [[nodiscard]] size_t GetGroupCount() const {
        return m_groups.size();
    }

    /**
     * @brief 设置每帧最大处理实例数（分批处理）
     * @param maxInstancesPerFrame 每帧最大处理实例数（默认100）
     * 
     * 用于控制分批处理的大小，避免一次性处理大量实例导致卡死
     * 建议值：
     * - 简单场景（<100实例）：100
     * - 中等场景（100-1000实例）：50-100
     * - 复杂场景（1000-5000实例）：20-50
     * - 超复杂场景（>5000实例）：10-20
     */
    void SetMaxInstancesPerFrame(size_t maxInstancesPerFrame) {
        m_maxInstancesPerFrame = maxInstancesPerFrame;
    }

    /**
     * @brief 获取每帧最大处理实例数
     * @return 每帧最大处理实例数
     */
    [[nodiscard]] size_t GetMaxInstancesPerFrame() const {
        return m_maxInstancesPerFrame;
    }

    /**
     * @brief 获取当前待处理的实例数量
     * @return 待处理的实例数量
     */
    [[nodiscard]] size_t GetPendingInstanceCount() const {
        return m_pendingInstances.size();
    }

    /**
     * @brief 设置预估实例数量（用于内存预分配）
     * @param count 预估的总实例数
     */
    void SetEstimatedInstanceCount(size_t count) {
        m_estimatedInstanceCount = count;
    }
    
    /**
     * @brief 设置预估组数量
     * @param count 预估的组数量
     */
    void SetEstimatedGroupCount(size_t count) {
        m_estimatedGroupCount = count;
    }

private:
    /**
     * @brief 分组键
     * 
     * 用于将相同网格、相同材质、相同 LOD 级别的实例分组
     */
    struct GroupKey {
        Ref<Mesh> mesh;
        Ref<Material> material;
        LODLevel lodLevel;
        MaterialSortKey sortKey;
        
        /**
         * @brief 比较运算符（用于 map 排序）
         */
        bool operator<(const GroupKey& other) const {
            // 先按排序键排序（材质优先）
            MaterialSortKeyLess less;
            if (less(sortKey, other.sortKey)) {
                return true;
            }
            if (less(other.sortKey, sortKey)) {
                return false;
            }
            // 再按 LOD 级别排序
            if (lodLevel != other.lodLevel) {
                return static_cast<int>(lodLevel) < static_cast<int>(other.lodLevel);
            }
            // 再按网格指针排序
            if (mesh != other.mesh) {
                return mesh < other.mesh;
            }
            // 最后按材质指针排序
            return material < other.material;
        }
    };
    
    /**
     * @brief 生成材质排序键
     * 
     * @param material 材质
     * @param mesh 网格（可选，用于未来扩展）
     * @return 排序键
     */
    [[nodiscard]] MaterialSortKey GenerateSortKey(Ref<Material> material, Ref<Mesh> mesh = nullptr) const;
    
    /**
     * @brief 渲染单个组
     * 
     * @param group 组指针
     * @param renderer 渲染器
     * @param renderState 渲染状态
     */
    void RenderGroup(
        LODInstancedGroup* group,
        Renderer* renderer,
        RenderState* renderState
    );
    
    /**
     * @brief 上传实例数据到 GPU
     * 
     * 包括：变换矩阵、颜色、自定义参数等
     * 
     * @param instances 实例数据列表
     * @param mesh 网格（用于设置实例化属性）
     */
    void UploadInstanceData(
        const std::vector<InstanceData>& instances,
        Ref<Mesh> mesh
    );
    
    /**
     * @brief 上传实例矩阵到 GPU（location 4-7）
     * 
     * 每个矩阵占用 4 个 vec4（location 4-7）
     * 
     * @param matrices 矩阵列表
     * @param mesh 网格（用于绑定 VAO）
     */
    void UploadInstanceMatrices(
        const std::vector<Matrix4>& matrices,
        Ref<Mesh> mesh
    );
    
    /**
     * @brief 上传实例颜色到 GPU（location 8）
     * 
     * @param colors 颜色列表
     * @param mesh 网格（用于绑定 VAO）
     */
    void UploadInstanceColors(
        const std::vector<Vector4>& colors,
        Ref<Mesh> mesh
    );
    
    /**
     * @brief 上传自定义参数到 GPU（location 9）
     * 
     * @param customParams 自定义参数列表
     * @param mesh 网格（用于绑定 VAO）
     */
    void UploadInstanceCustomParams(
        const std::vector<Vector4>& customParams,
        Ref<Mesh> mesh
    );
    
    // 分组映射：GroupKey -> LODInstancedGroup
    std::map<GroupKey, LODInstancedGroup> m_groups;
    
    // 实例化 VBO 缓存（按网格缓存，避免重复创建）
    struct InstanceVBOs {
        GLuint matrixVBO = 0;      // 矩阵 VBO (location 6-9)
        GLuint colorVBO = 0;       // 颜色 VBO (location 10)
        GLuint paramsVBO = 0;      // 自定义参数 VBO (location 11)
        size_t capacity = 0;        // 矩阵容量
        size_t colorCapacity = 0;  // ✅ 新增：颜色容量
        size_t paramsCapacity = 0; // ✅ 新增：参数容量
    };
    std::map<Ref<Mesh>, InstanceVBOs> m_instanceVBOs;
    
    /**
     * @brief 获取或创建实例化 VBO
     * @param mesh 网格
     * @param requiredCapacity 所需容量
     * @return VBO 结构引用
     */
    InstanceVBOs& GetOrCreateInstanceVBOs(Ref<Mesh> mesh, size_t requiredCapacity);
    
    /**
     * @brief 清理实例化 VBO（在 Clear 时调用）
     */
    void ClearInstanceVBOs();
    
    /**
     * @brief 设置实例化属性到 VAO
     * @param vao VAO ID（必须已经绑定）
     * @param instanceVBOs 实例化 VBO
     * @param instanceCount 实例数量
     * @param renderState 渲染状态（可选，用于管理缓冲区绑定）
     */
    void SetupInstanceAttributes(
        uint32_t vao, 
        const InstanceVBOs& instanceVBOs, 
        size_t instanceCount,
        RenderState* renderState = nullptr
    );

    /**
     * @brief 将待处理实例添加到实际渲染组
     * 
     * 内部方法，用于将队列中的实例添加到实际渲染组
     * 
     * @param entity 实体 ID
     * @param mesh 网格
     * @param material 材质
     * @param instanceData 实例数据
     * @param lodLevel LOD 级别
     */
    void AddInstanceToGroup(
        ECS::EntityID entity,
        Ref<Mesh> mesh,
        Ref<Material> material,
        const InstanceData& instanceData,
        LODLevel lodLevel
    );

    /**
     * @brief 待处理的实例（分批处理队列）
     */
    struct PendingInstance {
        ECS::EntityID entity;
        Ref<Mesh> mesh;
        Ref<Material> material;
        InstanceData instanceData;
        LODLevel lodLevel;
    };

    // 分批处理队列
    std::deque<PendingInstance> m_pendingInstances;  ///< ✅ 改为deque：待处理的实例队列
    size_t m_maxInstancesPerFrame = 100;              ///< 每帧最大处理实例数（默认100）
    size_t m_currentFrameProcessed = 0;                ///< 当前帧已处理的实例数（用于统计）
    
    // ✅ 内存预分配
    size_t m_estimatedInstanceCount = 1000;  ///< 默认预估1000个实例
    size_t m_estimatedGroupCount = 50;       ///< 默认预估50个组
    
    // ✅ 持久统计数据
    mutable Stats m_stats;
};

} // namespace Render

