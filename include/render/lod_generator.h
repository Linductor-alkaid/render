#pragma once

#include "render/types.h"
#include "render/mesh.h"
#include "render/lod_system.h"
#include <vector>
#include <memory>
#include <unordered_set>
#include <unordered_map>

// Forward declaration for meshoptimizer
struct meshopt_Stream;

namespace Render {

/**
 * @brief LOD 网格生成器
 * 
 * 使用 meshoptimizer 库自动生成不同 LOD 级别的网格
 * 
 * **使用示例**：
 * @code
 * // 基本使用
 * Ref<Mesh> sourceMesh = LoadMesh("tree.obj");
 * auto lodMeshes = LODGenerator::GenerateLODLevels(sourceMesh);
 * 
 * // 自动配置 LODConfig
 * LODConfig config;
 * LODGenerator::AutoConfigureLOD(sourceMesh, config);
 * @endcode
 */
class LODGenerator {
public:
    /**
     * @brief LOD 简化配置
     */
    struct SimplifyOptions {
        /**
         * @brief 简化模式
         */
        enum class Mode {
            TargetTriangleCount,  ///< 目标三角形数量（推荐）
            TargetError          ///< 目标误差（相对网格范围）
        };
        
        Mode mode = Mode::TargetTriangleCount;
        
        /**
         * @brief 目标三角形数量（用于 TargetTriangleCount 模式）
         * 
         * LOD1: 通常保留 50-70% 的三角形
         * LOD2: 通常保留 20-40% 的三角形
         * LOD3: 通常保留 10-20% 的三角形
         * 
         * 如果某个级别为 0，表示自动计算（使用默认比例）
         */
        struct TriangleCounts {
            size_t lod1 = 0;  ///< LOD1 目标三角形数量（0 = 自动，默认 50%）
            size_t lod2 = 0;  ///< LOD2 目标三角形数量（0 = 自动，默认 25%）
            size_t lod3 = 0;  ///< LOD3 目标三角形数量（0 = 自动，默认 10%）
        } triangleCounts;
        
        /**
         * @brief 目标误差（用于 TargetError 模式，范围 [0..1]）
         * 
         * 例如：0.01 = 1% 变形误差
         */
        struct TargetErrors {
            float lod1 = 0.01f;  ///< LOD1 目标误差（1%）
            float lod2 = 0.03f;  ///< LOD2 目标误差（3%）
            float lod3 = 0.05f;  ///< LOD3 目标误差（5%）
        } targetErrors;
        
        /**
         * @brief 简化选项标志
         */
        enum SimplifyFlags {
            LockBorder = 1 << 0,      ///< 锁定边界顶点（不移动）
            Sparse = 1 << 1,          ///< 稀疏简化（更快但质量稍低）
            Regularize = 1 << 2,      ///< 正则化（更平滑）
            Permissive = 1 << 3       ///< 允许跨属性不连续边折叠
        };
        
        unsigned int flags = 0;  ///< 标志位组合
        
        /**
         * @brief 属性权重（用于保留顶点属性）
         */
        struct AttributeWeights {
            float normal = 1.0f;      ///< 法线权重
            float texCoord = 1.0f;    ///< 纹理坐标权重
            float color = 0.5f;       ///< 颜色权重（通常较低）
        } attributeWeights;
        
        bool recalculateNormals = true;    ///< 是否重新计算法线（简化后）
        bool recalculateTangents = false;  ///< 是否重新计算切线（简化后）
    };
    
    /**
     * @brief 生成单个网格的 LOD 级别
     * 
     * 从源网格（LOD0）生成 LOD1、LOD2、LOD3 三个级别的简化网格
     * 
     * @param sourceMesh 源网格（LOD0）
     * @param options 简化选项
     * @return std::vector<Ref<Mesh>> LOD 网格数组 [LOD1, LOD2, LOD3]
     * 
     * @note 如果某个级别简化失败，对应位置为 nullptr
     * @note 返回的网格已经调用 Upload()，可以直接使用
     * 
     * **使用示例**：
     * @code
     * Ref<Mesh> sourceMesh = LoadMesh("tree.obj");
     * auto lodMeshes = LODGenerator::GenerateLODLevels(sourceMesh);
     * 
     * if (lodMeshes[0] && lodMeshes[1] && lodMeshes[2]) {
     *     // 所有 LOD 级别生成成功
     *     std::cout << "LOD1: " << lodMeshes[0]->GetTriangleCount() << " 三角形" << std::endl;
     *     std::cout << "LOD2: " << lodMeshes[1]->GetTriangleCount() << " 三角形" << std::endl;
     *     std::cout << "LOD3: " << lodMeshes[2]->GetTriangleCount() << " 三角形" << std::endl;
     * }
     * @endcode
     */
    static std::vector<Ref<Mesh>> GenerateLODLevels(
        Ref<Mesh> sourceMesh,
        const SimplifyOptions& options = SimplifyOptions{}
    );
    
    /**
     * @brief 生成单个 LOD 级别
     * 
     * @param sourceMesh 源网格
     * @param lodLevel 目标 LOD 级别（1, 2, 或 3）
     * @param options 简化选项
     * @return Ref<Mesh> 简化后的网格，失败返回 nullptr
     * 
     * @note 返回的网格已经调用 Upload()，可以直接使用
     * 
     * **使用示例**：
     * @code
     * Ref<Mesh> sourceMesh = LoadMesh("tree.obj");
     * Ref<Mesh> lod1Mesh = LODGenerator::GenerateLODLevel(sourceMesh, 1);
     * if (lod1Mesh) {
     *     std::cout << "LOD1 生成成功: " << lod1Mesh->GetTriangleCount() << " 三角形" << std::endl;
     * }
     * @endcode
     */
    static Ref<Mesh> GenerateLODLevel(
        Ref<Mesh> sourceMesh,
        int lodLevel,
        const SimplifyOptions& options = SimplifyOptions{}
    );
    
    /**
     * @brief 自动配置 LODConfig
     * 
     * 从源网格生成所有 LOD 级别并自动配置到 LODConfig
     * 
     * @param sourceMesh 源网格
     * @param config 要配置的 LODConfig（会被修改）
     * @param options 简化选项
     * @return bool 是否成功
     * 
     * **使用示例**：
     * @code
     * Ref<Mesh> sourceMesh = LoadMesh("tree.obj");
     * LODConfig config;
     * config.enabled = true;
     * config.distanceThresholds = {50.0f, 150.0f, 500.0f, 1000.0f};
     * 
     * if (LODGenerator::AutoConfigureLOD(sourceMesh, config)) {
     *     // config.lodMeshes 已经包含了 LOD1-LOD3 的网格
     *     LODComponent lodComp;
     *     lodComp.config = config;
     *     world->AddComponent<LODComponent>(entity, lodComp);
     * }
     * @endcode
     */
    static bool AutoConfigureLOD(
        Ref<Mesh> sourceMesh,
        LODConfig& config,
        const SimplifyOptions& options = SimplifyOptions{}
    );
    
    /**
     * @brief 批量生成多个网格的 LOD 级别
     * 
     * @param sourceMeshes 源网格数组
     * @param options 简化选项
     * @return std::vector<std::vector<Ref<Mesh>>> 每个网格的 LOD 级别数组
     * 
     * @note 返回的网格已经调用 Upload()，可以直接使用
     * 
     * **使用示例**：
     * @code
     * std::vector<Ref<Mesh>> sourceMeshes = {
     *     LoadMesh("tree1.obj"),
     *     LoadMesh("tree2.obj"),
     *     LoadMesh("tree3.obj")
     * };
     * 
     * auto allLODs = LODGenerator::BatchGenerateLODLevels(sourceMeshes);
     * // allLODs[i][j] = 第 i 个网格的第 j 个 LOD 级别（j=0=LOD1, j=1=LOD2, j=2=LOD3）
     * @endcode
     */
    static std::vector<std::vector<Ref<Mesh>>> BatchGenerateLODLevels(
        const std::vector<Ref<Mesh>>& sourceMeshes,
        const SimplifyOptions& options = SimplifyOptions{}
    );
    
    /**
     * @brief 获取推荐的简化配置
     * 
     * 根据源网格的三角形数量自动计算推荐的简化参数
     * 
     * @param sourceMesh 源网格
     * @return SimplifyOptions 推荐的配置
     * 
     * **使用示例**：
     * @code
     * Ref<Mesh> sourceMesh = LoadMesh("tree.obj");
     * auto options = LODGenerator::GetRecommendedOptions(sourceMesh);
     * 
     * // 可以根据需要调整
     * options.flags |= LODGenerator::SimplifyOptions::LockBorder;
     * 
     * auto lodMeshes = LODGenerator::GenerateLODLevels(sourceMesh, options);
     * @endcode
     */
    static SimplifyOptions GetRecommendedOptions(Ref<Mesh> sourceMesh);
    
    /**
     * @brief 验证简化结果
     * 
     * 检查简化后的网格是否有效（顶点数、索引数、拓扑等）
     * 
     * @param simplifiedMesh 简化后的网格
     * @param sourceMesh 源网格（用于对比）
     * @return bool 是否有效
     */
    static bool ValidateSimplifiedMesh(Ref<Mesh> simplifiedMesh, Ref<Mesh> sourceMesh);

private:
    /**
     * @brief 计算目标索引数量
     */
    static size_t CalculateTargetIndexCount(
        size_t originalIndexCount,
        int lodLevel,
        const SimplifyOptions& options
    );
    
    /**
     * @brief 提取顶点位置数据
     */
    static void ExtractPositions(
        const std::vector<Vertex>& vertices,
        std::vector<float>& positions
    );
    
    /**
     * @brief 提取顶点属性数据
     */
    static void ExtractAttributes(
        const std::vector<Vertex>& vertices,
        std::vector<float>& attributes
    );
    
    /**
     * @brief 构建属性权重数组
     */
    static void BuildAttributeWeights(
        const SimplifyOptions& options,
        std::vector<float>& weights
    );
    
    /**
     * @brief 从简化后的索引重建顶点数据
     */
    static void RebuildVertices(
        const std::vector<Vertex>& sourceVertices,
        const std::vector<uint32_t>& simplifiedIndices,
        std::vector<Vertex>& simplifiedVertices,
        std::vector<uint32_t>& remappedIndices
    );
    
    /**
     * @brief 执行网格简化（内部实现）
     */
    static Ref<Mesh> SimplifyMeshInternal(
        Ref<Mesh> sourceMesh,
        int lodLevel,
        const SimplifyOptions& options
    );
};

} // namespace Render

