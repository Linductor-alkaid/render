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
    
    /**
     * @brief 保存网格到 OBJ 文件
     * 
     * 将网格数据导出为 OBJ 格式文件
     * 
     * @param mesh 要保存的网格
     * @param filepath 输出文件路径
     * @return bool 是否成功
     * 
     * **使用示例**：
     * @code
     * Ref<Mesh> lodMesh = LODGenerator::GenerateLODLevel(sourceMesh, 1);
     * if (lodMesh) {
     *     LODGenerator::SaveMeshToOBJ(lodMesh, "model_lod1.obj");
     * }
     * @endcode
     */
    static bool SaveMeshToOBJ(Ref<Mesh> mesh, const std::string& filepath);
    
    /**
     * @brief 批量保存 LOD 网格到文件
     * 
     * 将生成的 LOD 级别网格保存为文件，文件名会自动添加 LOD 后缀
     * 
     * @param sourceMesh 源网格（可选，用于保存 LOD0）
     * @param lodMeshes LOD 网格数组 [LOD1, LOD2, LOD3]
     * @param baseFilepath 基础文件路径（不含扩展名），例如 "models/miku"
     * @return bool 是否全部成功
     * 
     * **使用示例**：
     * @code
     * Ref<Mesh> sourceMesh = LoadMesh("miku.obj");
     * auto lodMeshes = LODGenerator::GenerateLODLevels(sourceMesh);
     * 
     * // 保存所有 LOD 级别
     * LODGenerator::SaveLODMeshesToFiles(sourceMesh, lodMeshes, "output/miku");
     * // 会生成: miku_lod0.obj, miku_lod1.obj, miku_lod2.obj, miku_lod3.obj
     * @endcode
     */
    static bool SaveLODMeshesToFiles(
        Ref<Mesh> sourceMesh,
        const std::vector<Ref<Mesh>>& lodMeshes,
        const std::string& baseFilepath
    );
    
    /**
     * @brief 为 Model 生成 LOD 级别
     * 
     * 为 Model 中的每个部分（ModelPart）生成对应的 LOD 级别网格
     * 返回一个包含所有 LOD 级别的 Model 数组
     * 
     * @param sourceModel 源模型
     * @param options 简化选项
     * @return std::vector<Ref<Model>> LOD 模型数组 [LOD0, LOD1, LOD2, LOD3]
     * 
     * @note LOD0 是原始模型（不进行简化）
     * @note 如果某个部分的简化失败，该部分在对应 LOD 级别中会使用原始网格
     * 
     * **使用示例**：
     * @code
     * Ref<Model> sourceModel = ModelLoader::LoadFromFile("miku.pmx", "miku").model;
     * auto lodModels = LODGenerator::GenerateModelLODLevels(sourceModel);
     * 
     * // lodModels[0] 是原始模型
     * // lodModels[1] 是所有部分都简化为 LOD1 的模型
     * // lodModels[2] 是所有部分都简化为 LOD2 的模型
     * // lodModels[3] 是所有部分都简化为 LOD3 的模型
     * @endcode
     */
    static std::vector<Ref<Model>> GenerateModelLODLevels(
        Ref<Model> sourceModel,
        const SimplifyOptions& options = SimplifyOptions{}
    );
    
    /**
     * @brief 为 Model 的单个部分生成 LOD 级别
     * 
     * @param sourceModel 源模型
     * @param partIndex 部分索引
     * @param options 简化选项
     * @return std::vector<Ref<Mesh>> LOD 网格数组 [LOD0, LOD1, LOD2, LOD3]
     * 
     * @note 如果简化失败，对应位置为 nullptr
     */
    static std::vector<Ref<Mesh>> GenerateModelPartLODLevels(
        Ref<Model> sourceModel,
        size_t partIndex,
        const SimplifyOptions& options = SimplifyOptions{}
    );
    
    /**
     * @brief 保存 Model 的所有 LOD 级别到文件
     * 
     * 为 Model 的每个部分生成 LOD 级别并保存为独立的 OBJ 文件
     * 
     * @param sourceModel 源模型
     * @param baseFilepath 基础文件路径（不含扩展名），例如 "output/miku"
     * @param options 简化选项
     * @return bool 是否全部成功
     * 
     * **文件命名规则**：
     * - 单部分模型: miku_lod0.obj, miku_lod1.obj, ...
     * - 多部分模型: miku_part0_lod0.obj, miku_part0_lod1.obj, miku_part1_lod0.obj, ...
     * 
     * **使用示例**：
     * @code
     * Ref<Model> model = ModelLoader::LoadFromFile("miku.pmx", "miku").model;
     * 
     * // 保存所有部分的所有 LOD 级别
     * LODGenerator::SaveModelLODToFiles(model, "output/miku");
     * @endcode
     */
    static bool SaveModelLODToFiles(
        Ref<Model> sourceModel,
        const std::string& baseFilepath,
        const SimplifyOptions& options = SimplifyOptions{}
    );
    
    /**
     * @brief 加载指定部分的 LOD 网格
     * 
     * 根据部分索引和 LOD 级别加载对应的网格文件，确保与原模型部分一一对应
     * 
     * @param baseFilepath 基础文件路径（与 SaveModelLODToFiles 使用的路径相同）
     * @param partIndex 部分索引（从0开始）
     * @param lodLevel LOD 级别（0-3）
     * @param totalParts 模型总部分数（用于判断是单部分还是多部分模型）
     * @return Ref<Mesh> 加载的网格，失败返回 nullptr
     * 
     * **使用示例**：
     * @code
     * // 加载第0个部分的LOD1网格
     * Ref<Mesh> lod1Mesh = LODGenerator::LoadPartLODMesh("output/miku", 0, 1, model->GetPartCount());
     * @endcode
     */
    static Ref<Mesh> LoadPartLODMesh(
        const std::string& baseFilepath,
        size_t partIndex,
        int lodLevel,
        size_t totalParts
    );
    
    /**
     * @brief 加载模型的所有 LOD 级别
     * 
     * 为模型的每个部分加载所有 LOD 级别的网格，返回一个映射表
     * 
     * @param sourceModel 原始模型（用于获取部分信息）
     * @param baseFilepath 基础文件路径（与 SaveModelLODToFiles 使用的路径相同）
     * @return std::vector<std::vector<Ref<Mesh>>> 二维数组 [partIndex][lodLevel]，失败的部分为 nullptr
     * 
     * **使用示例**：
     * @code
     * auto lodMeshes = LODGenerator::LoadModelLODMeshes(model, "output/miku");
     * // lodMeshes[0][1] 是第0个部分的LOD1网格
     * // lodMeshes[1][2] 是第1个部分的LOD2网格
     * @endcode
     */
    static std::vector<std::vector<Ref<Mesh>>> LoadModelLODMeshes(
        Ref<Model> sourceModel,
        const std::string& baseFilepath
    );

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

