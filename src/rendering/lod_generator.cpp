#include "render/lod_generator.h"
#include "render/mesh.h"
#include "render/lod_system.h"
#include "render/logger.h"
#include "../../third_party/meshoptimizer/src/meshoptimizer.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <unordered_map>

namespace Render {

// ============================================================================
// 辅助函数实现
// ============================================================================

size_t LODGenerator::CalculateTargetIndexCount(
    size_t originalIndexCount,
    int lodLevel,
    const SimplifyOptions& options
) {
    size_t originalTriangleCount = originalIndexCount / 3;
    size_t targetTriangleCount = 0;
    
    if (options.mode == SimplifyOptions::Mode::TargetTriangleCount) {
        // 使用指定的三角形数量
        switch (lodLevel) {
            case 1:
                targetTriangleCount = options.triangleCounts.lod1;
                if (targetTriangleCount == 0) {
                    targetTriangleCount = static_cast<size_t>(originalTriangleCount * 0.5);  // 默认 50%
                }
                break;
            case 2:
                targetTriangleCount = options.triangleCounts.lod2;
                if (targetTriangleCount == 0) {
                    targetTriangleCount = static_cast<size_t>(originalTriangleCount * 0.25);  // 默认 25%
                }
                break;
            case 3:
                targetTriangleCount = options.triangleCounts.lod3;
                if (targetTriangleCount == 0) {
                    targetTriangleCount = static_cast<size_t>(originalTriangleCount * 0.1);  // 默认 10%
                }
                break;
            default:
                LOG_ERROR_F("LODGenerator: Invalid LOD level: %d", lodLevel);
                return 0;
        }
    } else {
        // TargetError 模式：使用误差，让 meshoptimizer 决定三角形数量
        // 返回一个较大的值，让简化算法根据误差自动决定
        targetTriangleCount = 1;  // 最小值，让算法根据误差决定
    }
    
    // 确保至少保留一些三角形
    targetTriangleCount = std::max(targetTriangleCount, size_t(1));
    
    // 不能超过原始数量
    targetTriangleCount = std::min(targetTriangleCount, originalTriangleCount);
    
    return targetTriangleCount * 3;  // 返回索引数量
}

void LODGenerator::ExtractPositions(
    const std::vector<Vertex>& vertices,
    std::vector<float>& positions
) {
    positions.clear();
    positions.reserve(vertices.size() * 3);
    
    for (const auto& v : vertices) {
        positions.push_back(v.position.x());
        positions.push_back(v.position.y());
        positions.push_back(v.position.z());
    }
}

void LODGenerator::ExtractAttributes(
    const std::vector<Vertex>& vertices,
    std::vector<float>& attributes
) {
    attributes.clear();
    attributes.reserve(vertices.size() * 8);  // normal(3) + texCoord(2) + color(3)
    
    for (const auto& v : vertices) {
        // 法线 (3 floats)
        attributes.push_back(v.normal.x());
        attributes.push_back(v.normal.y());
        attributes.push_back(v.normal.z());
        
        // 纹理坐标 (2 floats)
        attributes.push_back(v.texCoord.x());
        attributes.push_back(v.texCoord.y());
        
        // 颜色 (3 floats, RGB)
        attributes.push_back(v.color.r);
        attributes.push_back(v.color.g);
        attributes.push_back(v.color.b);
    }
}

void LODGenerator::BuildAttributeWeights(
    const SimplifyOptions& options,
    std::vector<float>& weights
) {
    weights.clear();
    weights.reserve(8);  // 8 个属性
    
    // 法线权重 (3个)
    weights.push_back(options.attributeWeights.normal);
    weights.push_back(options.attributeWeights.normal);
    weights.push_back(options.attributeWeights.normal);
    
    // 纹理坐标权重 (2个)
    weights.push_back(options.attributeWeights.texCoord);
    weights.push_back(options.attributeWeights.texCoord);
    
    // 颜色权重 (3个)
    weights.push_back(options.attributeWeights.color);
    weights.push_back(options.attributeWeights.color);
    weights.push_back(options.attributeWeights.color);
}

void LODGenerator::RebuildVertices(
    const std::vector<Vertex>& sourceVertices,
    const std::vector<uint32_t>& simplifiedIndices,
    std::vector<Vertex>& simplifiedVertices,
    std::vector<uint32_t>& remappedIndices
) {
    simplifiedVertices.clear();
    remappedIndices = simplifiedIndices;
    
    // 收集使用的顶点索引
    std::unordered_set<uint32_t> usedIndices;
    for (uint32_t idx : simplifiedIndices) {
        if (idx < sourceVertices.size()) {
            usedIndices.insert(idx);
        }
    }
    
    // 创建顶点重映射表
    std::unordered_map<uint32_t, uint32_t> remap;
    uint32_t newVertexIndex = 0;
    
    for (uint32_t i = 0; i < sourceVertices.size(); ++i) {
        if (usedIndices.count(i) > 0) {
            remap[i] = newVertexIndex++;
            simplifiedVertices.push_back(sourceVertices[i]);
        }
    }
    
    // 重映射索引
    for (uint32_t& idx : remappedIndices) {
        auto it = remap.find(idx);
        if (it != remap.end()) {
            idx = it->second;
        } else {
            // 不应该发生，但为了安全起见
            idx = 0;
        }
    }
}

// ============================================================================
// 核心简化实现
// ============================================================================

Ref<Mesh> LODGenerator::SimplifyMeshInternal(
    Ref<Mesh> sourceMesh,
    int lodLevel,
    const SimplifyOptions& options
) {
    if (!sourceMesh) {
        LOG_ERROR_F("LODGenerator: Source mesh is null");
        return nullptr;
    }
    
    // 提取源网格数据
    std::vector<Vertex> sourceVertices;
    std::vector<uint32_t> sourceIndices;
    
    sourceMesh->AccessVertices([&](const std::vector<Vertex>& vs) {
        sourceVertices = vs;
    });
    
    sourceMesh->AccessIndices([&](const std::vector<uint32_t>& is) {
        sourceIndices = is;
    });
    
    if (sourceVertices.empty() || sourceIndices.empty()) {
        LOG_ERROR_F("LODGenerator: Source mesh has no vertices or indices");
        return nullptr;
    }
    
    // 计算目标索引数量
    size_t targetIndexCount = CalculateTargetIndexCount(
        sourceIndices.size(),
        lodLevel,
        options
    );
    
    if (targetIndexCount == 0 || targetIndexCount >= sourceIndices.size()) {
        LOG_WARNING_F("LODGenerator: Target index count is invalid or not less than source. Source: %zu, Target: %zu", 
                 sourceIndices.size(), targetIndexCount);
        // 如果目标数量大于等于源数量，返回源网格的副本
        Ref<Mesh> copy = std::make_shared<Mesh>(sourceVertices, sourceIndices);
        copy->Upload();
        return copy;
    }
    
    // 提取顶点位置
    std::vector<float> positions;
    ExtractPositions(sourceVertices, positions);
    
    // 提取顶点属性
    std::vector<float> attributes;
    ExtractAttributes(sourceVertices, attributes);
    
    // 计算误差缩放因子
    float errorScale = meshopt_simplifyScale(
        positions.data(),
        sourceVertices.size(),
        sizeof(float) * 3
    );
    
    // 计算目标误差
    float targetError = 0.0f;
    if (options.mode == SimplifyOptions::Mode::TargetError) {
        switch (lodLevel) {
            case 1:
                targetError = options.targetErrors.lod1 / errorScale;
                break;
            case 2:
                targetError = options.targetErrors.lod2 / errorScale;
                break;
            case 3:
                targetError = options.targetErrors.lod3 / errorScale;
                break;
            default:
                targetError = 0.01f / errorScale;  // 默认 1%
        }
    } else {
        // TargetTriangleCount 模式：使用较小的误差，让算法尽可能接近目标数量
        targetError = 1e-3f;  // 很小的误差，优先满足三角形数量
    }
    
    // 构建属性权重
    std::vector<float> attributeWeights;
    BuildAttributeWeights(options, attributeWeights);
    
    // 准备简化选项标志
    unsigned int meshoptFlags = 0;
    if (options.flags & SimplifyOptions::LockBorder) {
        meshoptFlags |= meshopt_SimplifyLockBorder;
    }
    if (options.flags & SimplifyOptions::Sparse) {
        meshoptFlags |= meshopt_SimplifySparse;
    }
    if (options.flags & SimplifyOptions::Regularize) {
        meshoptFlags |= meshopt_SimplifyRegularize;
    }
    if (options.flags & SimplifyOptions::Permissive) {
        meshoptFlags |= meshopt_SimplifyPermissive;
    }
    
    // 执行简化
    std::vector<unsigned int> simplifiedIndices(sourceIndices.size());
    float resultError = 0.0f;
    
    size_t newIndexCount = meshopt_simplifyWithAttributes(
        simplifiedIndices.data(),
        sourceIndices.data(),
        sourceIndices.size(),
        positions.data(),
        sourceVertices.size(),
        sizeof(float) * 3,
        attributes.data(),
        sizeof(float) * 8,
        attributeWeights.data(),
        8,  // 属性数量
        nullptr,  // vertex_lock
        targetIndexCount,
        targetError,
        meshoptFlags,
        &resultError
    );
    
    if (newIndexCount == 0 || newIndexCount >= sourceIndices.size()) {
        LOG_WARNING_F("LODGenerator: Simplification failed or produced no reduction. Source: %zu, Result: %zu", 
                 sourceIndices.size(), newIndexCount);
        // 简化失败，返回源网格的副本
        Ref<Mesh> copy = std::make_shared<Mesh>(sourceVertices, sourceIndices);
        copy->Upload();
        return copy;
    }
    
    simplifiedIndices.resize(newIndexCount);
    
    // 重建顶点数据
    std::vector<Vertex> simplifiedVertices;
    std::vector<uint32_t> remappedIndices;
    RebuildVertices(sourceVertices, simplifiedIndices, simplifiedVertices, remappedIndices);
    
    // 创建新的网格
    Ref<Mesh> simplifiedMesh = std::make_shared<Mesh>(simplifiedVertices, remappedIndices);
    
    // 重新计算法线
    if (options.recalculateNormals) {
        simplifiedMesh->RecalculateNormals();
    }
    
    // 重新计算切线
    if (options.recalculateTangents) {
        simplifiedMesh->RecalculateTangents();
    }
    
    // 上传到 GPU
    simplifiedMesh->Upload();
    
    LOG_INFO_F("LODGenerator: Generated LOD%d - Triangles: %zu -> %zu (reduction: %.1f%%)", 
             lodLevel,
             sourceIndices.size() / 3,
             newIndexCount / 3,
             (1.0f - (float)newIndexCount / sourceIndices.size()) * 100.0f);
    
    return simplifiedMesh;
}

// ============================================================================
// 公共 API 实现
// ============================================================================

std::vector<Ref<Mesh>> LODGenerator::GenerateLODLevels(
    Ref<Mesh> sourceMesh,
    const SimplifyOptions& options
) {
    std::vector<Ref<Mesh>> lodMeshes(3);  // LOD1, LOD2, LOD3
    
    for (int i = 1; i <= 3; ++i) {
        lodMeshes[i - 1] = GenerateLODLevel(sourceMesh, i, options);
    }
    
    return lodMeshes;
}

Ref<Mesh> LODGenerator::GenerateLODLevel(
    Ref<Mesh> sourceMesh,
    int lodLevel,
    const SimplifyOptions& options
) {
    if (lodLevel < 1 || lodLevel > 3) {
        LOG_ERROR_F("LODGenerator: Invalid LOD level: %d. Must be 1, 2, or 3", lodLevel);
        return nullptr;
    }
    
    return SimplifyMeshInternal(sourceMesh, lodLevel, options);
}

bool LODGenerator::AutoConfigureLOD(
    Ref<Mesh> sourceMesh,
    LODConfig& config,
    const SimplifyOptions& options
) {
    if (!sourceMesh) {
        LOG_ERROR_F("LODGenerator: Source mesh is null");
        return false;
    }
    
    // 生成所有 LOD 级别
    auto lodMeshes = GenerateLODLevels(sourceMesh, options);
    
    // 检查是否至少有一个级别生成成功
    bool hasAnyLOD = false;
    for (const auto& mesh : lodMeshes) {
        if (mesh) {
            hasAnyLOD = true;
            break;
        }
    }
    
    if (!hasAnyLOD) {
        LOG_ERROR_F("LODGenerator: Failed to generate any LOD levels");
        return false;
    }
    
    // 配置到 LODConfig
    config.lodMeshes.clear();
    config.lodMeshes.reserve(4);  // LOD0, LOD1, LOD2, LOD3
    
    // LOD0 使用源网格（如果需要，可以添加，但通常不需要）
    // config.lodMeshes.push_back(sourceMesh);  // 可选
    
    // 添加生成的 LOD 级别
    for (const auto& mesh : lodMeshes) {
        config.lodMeshes.push_back(mesh);
    }
    
    LOG_INFO_F("LODGenerator: Auto-configured LOD with %zu levels", lodMeshes.size());
    
    return true;
}

std::vector<std::vector<Ref<Mesh>>> LODGenerator::BatchGenerateLODLevels(
    const std::vector<Ref<Mesh>>& sourceMeshes,
    const SimplifyOptions& options
) {
    std::vector<std::vector<Ref<Mesh>>> allLODs;
    allLODs.reserve(sourceMeshes.size());
    
    for (const auto& sourceMesh : sourceMeshes) {
        auto lodMeshes = GenerateLODLevels(sourceMesh, options);
        allLODs.push_back(std::move(lodMeshes));
    }
    
    LOG_INFO_F("LODGenerator: Batch generated LOD levels for %zu meshes", sourceMeshes.size());
    
    return allLODs;
}

LODGenerator::SimplifyOptions LODGenerator::GetRecommendedOptions(Ref<Mesh> sourceMesh) {
    SimplifyOptions options;
    
    if (!sourceMesh) {
        return options;  // 返回默认选项
    }
    
    size_t triangleCount = sourceMesh->GetTriangleCount();
    
    // 根据三角形数量选择模式
    if (triangleCount > 10000) {
        // 高多边形：使用目标三角形数量
        options.mode = SimplifyOptions::Mode::TargetTriangleCount;
        options.triangleCounts.lod1 = static_cast<size_t>(triangleCount * 0.5);
        options.triangleCounts.lod2 = static_cast<size_t>(triangleCount * 0.25);
        options.triangleCounts.lod3 = static_cast<size_t>(triangleCount * 0.1);
    } else {
        // 低多边形：使用目标误差
        options.mode = SimplifyOptions::Mode::TargetError;
        options.targetErrors.lod1 = 0.01f;
        options.targetErrors.lod2 = 0.03f;
        options.targetErrors.lod3 = 0.05f;
    }
    
    // 根据网格复杂度设置属性权重
    if (triangleCount > 50000) {
        // 高复杂度：降低属性权重以加快简化速度
        options.attributeWeights.normal = 0.8f;
        options.attributeWeights.texCoord = 0.8f;
        options.attributeWeights.color = 0.3f;
    }
    
    // 默认重新计算法线
    options.recalculateNormals = true;
    
    return options;
}

bool LODGenerator::ValidateSimplifiedMesh(Ref<Mesh> simplifiedMesh, Ref<Mesh> sourceMesh) {
    if (!simplifiedMesh || !sourceMesh) {
        return false;
    }
    
    // 检查基本数据
    size_t simplifiedVertices = simplifiedMesh->GetVertexCount();
    size_t simplifiedIndices = simplifiedMesh->GetIndexCount();
    size_t sourceVertices = sourceMesh->GetVertexCount();
    size_t sourceIndices = sourceMesh->GetIndexCount();
    
    // 简化后的网格应该比源网格小
    if (simplifiedVertices > sourceVertices || simplifiedIndices > sourceIndices) {
        LOG_WARNING_F("LODGenerator: Simplified mesh is not smaller than source mesh");
        return false;
    }
    
    // 至少应该有一些顶点和索引
    if (simplifiedVertices == 0 || simplifiedIndices == 0) {
        LOG_WARNING_F("LODGenerator: Simplified mesh has no vertices or indices");
        return false;
    }
    
    // 索引数量应该是 3 的倍数（三角形）
    if (simplifiedIndices % 3 != 0) {
        LOG_WARNING_F("LODGenerator: Simplified mesh index count is not a multiple of 3");
        return false;
    }
    
    // 检查索引是否在有效范围内
    bool indicesValid = true;
    simplifiedMesh->AccessIndices([&](const std::vector<uint32_t>& indices) {
        for (uint32_t idx : indices) {
            if (idx >= simplifiedVertices) {
                indicesValid = false;
                return;
            }
        }
    });
    
    if (!indicesValid) {
        LOG_WARNING_F("LODGenerator: Simplified mesh has invalid indices");
        return false;
    }
    
    return true;
}

} // namespace Render

