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
#include "render/lod_generator.h"
#include "render/mesh.h"
#include "render/model.h"
#include "render/mesh_loader.h"
#include "render/lod_system.h"
#include "render/logger.h"
#include "render/texture.h"
#include "render/material.h"
#include "../../third_party/meshoptimizer/src/meshoptimizer.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <cstdio>

namespace Render {

// ============================================================================
// 辅助函数实现
// ============================================================================

/**
 * @brief 清理文件名，将非ASCII字符替换为安全的ASCII字符
 * 
 * 将所有非ASCII字符（包括中文）替换为下划线，避免编码问题
 * 
 * @param filename 原始文件名
 * @param partIndex 部分索引（用于确保唯一性）
 * @return std::string 清理后的文件名
 */
static std::string SanitizeFilename(const std::string& filename, size_t partIndex) {
    std::string result;
    result.reserve(filename.size());
    
    for (unsigned char c : filename) {
        // 保留ASCII可打印字符（32-126），包括字母、数字、常见标点
        if (c >= 32 && c <= 126) {
            // 排除文件系统非法字符
            if (c != '/' && c != '\\' && c != ':' && c != '*' && 
                c != '?' && c != '"' && c != '<' && c != '>' && c != '|') {
                result += c;
            } else {
                result += '_';
            }
        } else {
            // 非ASCII字符（包括中文）替换为下划线
            result += '_';
        }
    }
    
    // 移除连续的下划线
    std::string cleaned;
    cleaned.reserve(result.size());
    bool lastWasUnderscore = false;
    for (char c : result) {
        if (c == '_') {
            if (!lastWasUnderscore) {
                cleaned += c;
            }
            lastWasUnderscore = true;
        } else {
            cleaned += c;
            lastWasUnderscore = false;
        }
    }
    
    // 移除开头和结尾的下划线
    while (!cleaned.empty() && cleaned.front() == '_') {
        cleaned.erase(cleaned.begin());
    }
    while (!cleaned.empty() && cleaned.back() == '_') {
        cleaned.pop_back();
    }
    
    // 如果清理后为空，使用默认名称
    if (cleaned.empty()) {
        cleaned = "part";
    }
    
    // 始终添加索引后缀以确保唯一性，避免多个中文名称清理后相同
    // 格式：cleaned_partIndex
    cleaned += "_" + std::to_string(partIndex);
    
    return cleaned;
}

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

bool LODGenerator::SaveMeshToOBJ(Ref<Mesh> mesh, const std::string& filepath) {
    if (!mesh) {
        LOG_ERROR_F("LODGenerator::SaveMeshToOBJ: Mesh is null");
        return false;
    }
    
    // 确保目录存在（std::filesystem::path 构造函数会自动处理 UTF-8 字符串）
    std::filesystem::path path(filepath);
    std::filesystem::path dir = path.parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir)) {
        try {
            std::filesystem::create_directories(dir);
        } catch (const std::exception& e) {
            LOG_ERROR_F("LODGenerator::SaveMeshToOBJ: Failed to create directory: %s", e.what());
            return false;
        }
    }
    
    // 使用二进制模式打开文件，确保 UTF-8 编码正确保存
    // 在 Windows 上，std::filesystem::path 内部使用 UTF-16，但我们可以直接使用 string() 方法
    // 或者使用 path.c_str() 在 Windows 上会返回宽字符，但 std::ofstream 需要 char*
    // 所以使用 path.string() 获取 UTF-8 字符串，或者使用 path.native() 在 Windows 上获取宽字符
#ifdef _WIN32
    // 在 Windows 上，使用 path.u8string() 获取 UTF-8 字符串，然后转换为本地编码
    // 或者直接使用 path.string()，它会自动转换
    std::ofstream file(path, std::ios::binary);
#else
    std::ofstream file(path, std::ios::binary);
#endif
    if (!file.is_open()) {
        LOG_ERROR_F("LODGenerator::SaveMeshToOBJ: Failed to open file for writing: %s", filepath.c_str());
        return false;
    }
    
    // 写入 UTF-8 BOM（可选，但有助于某些编辑器识别 UTF-8 编码）
    const unsigned char utf8_bom[] = {0xEF, 0xBB, 0xBF};
    file.write(reinterpret_cast<const char*>(utf8_bom), 3);
    
    // 写入 OBJ 文件头（使用 UTF-8 编码）
    std::string header = "# OBJ file exported by LODGenerator\n";
    header += "# Vertices: " + std::to_string(mesh->GetVertexCount()) + "\n";
    header += "# Triangles: " + std::to_string(mesh->GetTriangleCount()) + "\n";
    header += "\n";
    file.write(header.c_str(), header.size());
    
    // 辅助函数：将数值转换为字符串并写入文件
    auto writeFloat = [](std::ofstream& f, float value) {
        // 使用足够的精度，避免科学计数法
        char buffer[32];
        int len = snprintf(buffer, sizeof(buffer), "%.9g", value);
        if (len > 0 && len < static_cast<int>(sizeof(buffer))) {
            f.write(buffer, len);
        }
    };
    
    auto writeUint = [](std::ofstream& f, uint32_t value) {
        char buffer[32];
        int len = snprintf(buffer, sizeof(buffer), "%u", value);
        if (len > 0 && len < static_cast<int>(sizeof(buffer))) {
            f.write(buffer, len);
        }
    };
    
    // 写入顶点数据
    mesh->AccessVertices([&](const std::vector<Vertex>& vertices) {
        for (const auto& v : vertices) {
            file.write("v ", 2);
            writeFloat(file, v.position.x());
            file.write(" ", 1);
            writeFloat(file, v.position.y());
            file.write(" ", 1);
            writeFloat(file, v.position.z());
            file.write("\n", 1);
        }
    });
    
    file.write("\n", 1);
    
    // 写入纹理坐标
    mesh->AccessVertices([&](const std::vector<Vertex>& vertices) {
        for (const auto& v : vertices) {
            file.write("vt ", 3);
            writeFloat(file, v.texCoord.x());
            file.write(" ", 1);
            writeFloat(file, v.texCoord.y());
            file.write("\n", 1);
        }
    });
    
    file.write("\n", 1);
    
    // 写入法线
    mesh->AccessVertices([&](const std::vector<Vertex>& vertices) {
        for (const auto& v : vertices) {
            file.write("vn ", 3);
            writeFloat(file, v.normal.x());
            file.write(" ", 1);
            writeFloat(file, v.normal.y());
            file.write(" ", 1);
            writeFloat(file, v.normal.z());
            file.write("\n", 1);
        }
    });
    
    file.write("\n", 1);
    
    // 写入面（三角形）
    mesh->AccessIndices([&](const std::vector<uint32_t>& indices) {
        for (size_t i = 0; i < indices.size(); i += 3) {
            if (i + 2 < indices.size()) {
                // OBJ 格式索引从 1 开始，格式为: v/vt/vn
                uint32_t idx0 = indices[i] + 1;
                uint32_t idx1 = indices[i + 1] + 1;
                uint32_t idx2 = indices[i + 2] + 1;
                
                file.write("f ", 2);
                writeUint(file, idx0);
                file.write("/", 1);
                writeUint(file, idx0);
                file.write("/", 1);
                writeUint(file, idx0);
                file.write(" ", 1);
                writeUint(file, idx1);
                file.write("/", 1);
                writeUint(file, idx1);
                file.write("/", 1);
                writeUint(file, idx1);
                file.write(" ", 1);
                writeUint(file, idx2);
                file.write("/", 1);
                writeUint(file, idx2);
                file.write("/", 1);
                writeUint(file, idx2);
                file.write("\n", 1);
            }
        }
    });
    
    file.close();
    
    LOG_INFO_F("LODGenerator::SaveMeshToOBJ: Successfully saved mesh to %s (vertices: %zu, triangles: %zu)",
               filepath.c_str(), mesh->GetVertexCount(), mesh->GetTriangleCount());
    
    return true;
}

bool LODGenerator::SaveLODMeshesToFiles(
    Ref<Mesh> sourceMesh,
    const std::vector<Ref<Mesh>>& lodMeshes,
    const std::string& baseFilepath
) {
    // 清理基础文件路径，移除非ASCII字符
    // 对于基础路径，不使用索引（因为它是唯一的）
    std::filesystem::path basePath(baseFilepath);
    std::string baseFilename = basePath.filename().string();
    std::string sanitizedBase;
    sanitizedBase.reserve(baseFilename.size());
    
    for (unsigned char c : baseFilename) {
        if (c >= 32 && c <= 126 && c != '/' && c != '\\' && c != ':' && 
            c != '*' && c != '?' && c != '"' && c != '<' && c != '>' && c != '|') {
            sanitizedBase += c;
        } else {
            sanitizedBase += '_';
        }
    }
    
    // 移除连续的下划线
    std::string cleaned;
    cleaned.reserve(sanitizedBase.size());
    bool lastWasUnderscore = false;
    for (char c : sanitizedBase) {
        if (c == '_') {
            if (!lastWasUnderscore) {
                cleaned += c;
            }
            lastWasUnderscore = true;
        } else {
            cleaned += c;
            lastWasUnderscore = false;
        }
    }
    
    // 移除开头和结尾的下划线
    while (!cleaned.empty() && cleaned.front() == '_') {
        cleaned.erase(cleaned.begin());
    }
    while (!cleaned.empty() && cleaned.back() == '_') {
        cleaned.pop_back();
    }
    
    if (cleaned.empty()) {
        cleaned = "model";
    }
    
    std::string sanitizedDir = basePath.parent_path().string();
    std::string finalBasePath = sanitizedDir.empty() ? cleaned : (sanitizedDir + "/" + cleaned);
    
    bool allSuccess = true;
    
    // 保存 LOD0（源网格）
    if (sourceMesh) {
        std::string lod0Path = finalBasePath + "_lod0.obj";
        if (!SaveMeshToOBJ(sourceMesh, lod0Path)) {
            LOG_ERROR_F("LODGenerator::SaveLODMeshesToFiles: Failed to save LOD0 to %s", lod0Path.c_str());
            allSuccess = false;
        }
    }
    
    // 保存 LOD1-LOD3
    for (size_t i = 0; i < lodMeshes.size() && i < 3; ++i) {
        if (lodMeshes[i]) {
            std::string lodPath = finalBasePath + "_lod" + std::to_string(i + 1) + ".obj";
            if (!SaveMeshToOBJ(lodMeshes[i], lodPath)) {
                LOG_ERROR_F("LODGenerator::SaveLODMeshesToFiles: Failed to save LOD%zu to %s", i + 1, lodPath.c_str());
                allSuccess = false;
            }
        }
    }
    
    if (allSuccess) {
        LOG_INFO_F("LODGenerator::SaveLODMeshesToFiles: Successfully saved all LOD meshes to %s", finalBasePath.c_str());
    }
    
    return allSuccess;
}

std::vector<Ref<Model>> LODGenerator::GenerateModelLODLevels(
    Ref<Model> sourceModel,
    const SimplifyOptions& options
) {
    std::vector<Ref<Model>> lodModels(4);  // LOD0, LOD1, LOD2, LOD3
    
    if (!sourceModel) {
        LOG_ERROR_F("LODGenerator::GenerateModelLODLevels: Source model is null");
        return lodModels;
    }
    
    // 获取源模型的所有部分
    std::vector<ModelPart> sourceParts;
    sourceModel->AccessParts([&](const std::vector<ModelPart>& parts) {
        sourceParts = parts;
    });
    
    if (sourceParts.empty()) {
        LOG_WARNING_F("LODGenerator::GenerateModelLODLevels: Source model has no parts");
        return lodModels;
    }
    
    LOG_INFO_F("LODGenerator::GenerateModelLODLevels: Processing model with %zu parts", sourceParts.size());
    
    // LOD0: 原始模型（不简化）
    lodModels[0] = std::make_shared<Model>(sourceModel->GetName() + "_LOD0");
    lodModels[0]->SetSourcePath(sourceModel->GetSourcePath());
    lodModels[0]->ModifyParts([&](std::vector<ModelPart>& parts) {
        parts = sourceParts;  // 直接复制原始部分
    });
    
    // 为每个 LOD 级别（1-3）生成简化模型
    for (int lodLevel = 1; lodLevel <= 3; ++lodLevel) {
        auto lodModel = std::make_shared<Model>(sourceModel->GetName() + "_LOD" + std::to_string(lodLevel));
        lodModel->SetSourcePath(sourceModel->GetSourcePath());
        
        std::vector<ModelPart> lodParts;
        lodParts.reserve(sourceParts.size());
        
        // 为每个部分生成对应的 LOD 级别
        for (size_t partIndex = 0; partIndex < sourceParts.size(); ++partIndex) {
            const auto& sourcePart = sourceParts[partIndex];
            
            if (!sourcePart.mesh) {
                // 如果没有网格，直接复制原始部分
                lodParts.push_back(sourcePart);
                continue;
            }
            
            // 生成该部分的 LOD 级别网格
            Ref<Mesh> lodMesh = GenerateLODLevel(sourcePart.mesh, lodLevel, options);
            
            // 创建新的 ModelPart
            ModelPart lodPart = sourcePart;
            if (lodMesh) {
                lodPart.mesh = lodMesh;
                LOG_INFO_F("LODGenerator::GenerateModelLODLevels: Part %zu (%s) LOD%d: %zu -> %zu triangles",
                           partIndex, sourcePart.name.c_str(), lodLevel,
                           sourcePart.mesh->GetTriangleCount(), lodMesh->GetTriangleCount());
            } else {
                // 简化失败，使用原始网格
                LOG_WARNING_F("LODGenerator::GenerateModelLODLevels: Failed to generate LOD%d for part %zu (%s), using original",
                             lodLevel, partIndex, sourcePart.name.c_str());
            }
            
            lodParts.push_back(lodPart);
        }
        
        lodModel->ModifyParts([&](std::vector<ModelPart>& parts) {
            parts = std::move(lodParts);
        });
        
        lodModels[lodLevel] = lodModel;
    }
    
    LOG_INFO_F("LODGenerator::GenerateModelLODLevels: Successfully generated LOD levels for model with %zu parts",
               sourceParts.size());
    
    return lodModels;
}

std::vector<Ref<Mesh>> LODGenerator::GenerateModelPartLODLevels(
    Ref<Model> sourceModel,
    size_t partIndex,
    const SimplifyOptions& options
) {
    std::vector<Ref<Mesh>> lodMeshes(4);  // LOD0, LOD1, LOD2, LOD3
    
    if (!sourceModel) {
        LOG_ERROR_F("LODGenerator::GenerateModelPartLODLevels: Source model is null");
        return lodMeshes;
    }
    
    Ref<Mesh> sourceMesh = nullptr;
    std::string partName;
    
    sourceModel->AccessParts([&](const std::vector<ModelPart>& parts) {
        if (partIndex < parts.size()) {
            sourceMesh = parts[partIndex].mesh;
            partName = parts[partIndex].name;
        }
    });
    
    if (!sourceMesh) {
        LOG_ERROR_F("LODGenerator::GenerateModelPartLODLevels: Part %zu not found or has no mesh", partIndex);
        return lodMeshes;
    }
    
    // LOD0: 原始网格
    lodMeshes[0] = sourceMesh;
    
    // 生成 LOD1-LOD3
    for (int lodLevel = 1; lodLevel <= 3; ++lodLevel) {
        lodMeshes[lodLevel] = GenerateLODLevel(sourceMesh, lodLevel, options);
        if (lodMeshes[lodLevel]) {
            LOG_INFO_F("LODGenerator::GenerateModelPartLODLevels: Part %zu (%s) LOD%d: %zu -> %zu triangles",
                       partIndex, partName.c_str(), lodLevel,
                       sourceMesh->GetTriangleCount(), lodMeshes[lodLevel]->GetTriangleCount());
        }
    }
    
    return lodMeshes;
}

bool LODGenerator::SaveModelLODToFiles(
    Ref<Model> sourceModel,
    const std::string& baseFilepath,
    const SimplifyOptions& options
) {
    if (!sourceModel) {
        LOG_ERROR_F("LODGenerator::SaveModelLODToFiles: Source model is null");
        return false;
    }
    
    // 获取模型的所有部分
    std::vector<ModelPart> sourceParts;
    sourceModel->AccessParts([&](const std::vector<ModelPart>& parts) {
        sourceParts = parts;
    });
    
    if (sourceParts.empty()) {
        LOG_WARNING_F("LODGenerator::SaveModelLODToFiles: Model has no parts");
        return false;
    }
    
    // 清理基础文件路径，移除非ASCII字符
    // 对于基础路径，不使用索引（因为它是唯一的）
    std::filesystem::path basePath(baseFilepath);
    std::string baseFilename = basePath.filename().string();
    std::string sanitizedBase;
    sanitizedBase.reserve(baseFilename.size());
    
    for (unsigned char c : baseFilename) {
        if (c >= 32 && c <= 126 && c != '/' && c != '\\' && c != ':' && 
            c != '*' && c != '?' && c != '"' && c != '<' && c != '>' && c != '|') {
            sanitizedBase += c;
        } else {
            sanitizedBase += '_';
        }
    }
    
    // 移除连续的下划线
    std::string cleaned;
    cleaned.reserve(sanitizedBase.size());
    bool lastWasUnderscore = false;
    for (char c : sanitizedBase) {
        if (c == '_') {
            if (!lastWasUnderscore) {
                cleaned += c;
            }
            lastWasUnderscore = true;
        } else {
            cleaned += c;
            lastWasUnderscore = false;
        }
    }
    
    // 移除开头和结尾的下划线
    while (!cleaned.empty() && cleaned.front() == '_') {
        cleaned.erase(cleaned.begin());
    }
    while (!cleaned.empty() && cleaned.back() == '_') {
        cleaned.pop_back();
    }
    
    if (cleaned.empty()) {
        cleaned = "model";
    }
    
    std::string sanitizedDir = basePath.parent_path().string();
    std::string finalBasePath = sanitizedDir.empty() ? cleaned : (sanitizedDir + "/" + cleaned);
    
    LOG_INFO_F("LODGenerator::SaveModelLODToFiles: Saving model with %zu parts to %s",
               sourceParts.size(), finalBasePath.c_str());
    
    bool allSuccess = true;
    
    // 为每个部分生成并保存 LOD 级别
    for (size_t partIndex = 0; partIndex < sourceParts.size(); ++partIndex) {
        const auto& part = sourceParts[partIndex];
        
        if (!part.mesh) {
            LOG_WARNING_F("LODGenerator::SaveModelLODToFiles: Part %zu (%s) has no mesh, skipping",
                         partIndex, part.name.c_str());
            continue;
        }
        
        // 生成该部分的所有 LOD 级别
        auto lodMeshes = GenerateModelPartLODLevels(sourceModel, partIndex, options);
        
        // 构建文件名（清理非ASCII字符，避免编码问题）
        // 使用索引确保唯一性，即使多个中文名称清理后相同也不会冲突
        std::string partName = part.name.empty() ? ("part_" + std::to_string(partIndex)) : SanitizeFilename(part.name, partIndex);
        
        // 保存每个 LOD 级别
        for (int lodLevel = 0; lodLevel <= 3; ++lodLevel) {
            if (!lodMeshes[lodLevel]) {
                if (lodLevel == 0) {
                    LOG_ERROR_F("LODGenerator::SaveModelLODToFiles: Part %zu (%s) has no LOD0 mesh",
                               partIndex, part.name.c_str());
                    allSuccess = false;
                } else {
                    LOG_WARNING_F("LODGenerator::SaveModelLODToFiles: Part %zu (%s) LOD%d generation failed, skipping",
                                 partIndex, part.name.c_str(), lodLevel);
                }
                continue;
            }
            
            // 构建文件路径（使用清理后的基础路径）
            // 统一使用部分索引来确保一一对应，格式：basePath_partIndex_lodLevel.obj
            // 这样即使部分名称清理后相同，也能通过索引正确匹配
            std::string filepath;
            if (sourceParts.size() == 1) {
                // 单部分模型：直接使用基础路径（兼容旧格式）
                filepath = finalBasePath + "_lod" + std::to_string(lodLevel) + ".obj";
            } else {
                // 多部分模型：使用部分索引确保唯一对应
                // 格式：basePath_partIndex_lodLevel.obj
                filepath = finalBasePath + "_part" + std::to_string(partIndex) + "_lod" + std::to_string(lodLevel) + ".obj";
            }
            
            if (!SaveMeshToOBJ(lodMeshes[lodLevel], filepath)) {
                LOG_ERROR_F("LODGenerator::SaveModelLODToFiles: Failed to save part %zu (%s) LOD%d to %s",
                           partIndex, part.name.c_str(), lodLevel, filepath.c_str());
                allSuccess = false;
            }
        }
    }
    
    if (allSuccess) {
        LOG_INFO_F("LODGenerator::SaveModelLODToFiles: Successfully saved all LOD levels for model with %zu parts",
                   sourceParts.size());
    }
    
    return allSuccess;
}

Ref<Mesh> LODGenerator::LoadPartLODMesh(
    const std::string& baseFilepath,
    size_t partIndex,
    int lodLevel,
    size_t totalParts
) {
    if (lodLevel < 0 || lodLevel > 3) {
        LOG_ERROR_F("LODGenerator::LoadPartLODMesh: Invalid LOD level %d (must be 0-3)", lodLevel);
        return nullptr;
    }
    
    if (partIndex >= totalParts) {
        LOG_ERROR_F("LODGenerator::LoadPartLODMesh: Part index %zu out of range (total: %zu)", partIndex, totalParts);
        return nullptr;
    }
    
    // 清理基础文件路径（与保存时使用相同的逻辑）
    std::filesystem::path basePath(baseFilepath);
    std::string baseFilename = basePath.filename().string();
    std::string sanitizedBase;
    sanitizedBase.reserve(baseFilename.size());
    
    for (unsigned char c : baseFilename) {
        if (c >= 32 && c <= 126 && c != '/' && c != '\\' && c != ':' && 
            c != '*' && c != '?' && c != '"' && c != '<' && c != '>' && c != '|') {
            sanitizedBase += c;
        } else {
            sanitizedBase += '_';
        }
    }
    
    // 移除连续的下划线
    std::string cleaned;
    cleaned.reserve(sanitizedBase.size());
    bool lastWasUnderscore = false;
    for (char c : sanitizedBase) {
        if (c == '_') {
            if (!lastWasUnderscore) {
                cleaned += c;
            }
            lastWasUnderscore = true;
        } else {
            cleaned += c;
            lastWasUnderscore = false;
        }
    }
    
    // 移除开头和结尾的下划线
    while (!cleaned.empty() && cleaned.front() == '_') {
        cleaned.erase(cleaned.begin());
    }
    while (!cleaned.empty() && cleaned.back() == '_') {
        cleaned.pop_back();
    }
    
    if (cleaned.empty()) {
        cleaned = "model";
    }
    
    std::string sanitizedDir = basePath.parent_path().string();
    std::string finalBasePath = sanitizedDir.empty() ? cleaned : (sanitizedDir + "/" + cleaned);
    
    // 构建文件路径（与保存时使用相同的格式）
    std::string filepath;
    if (totalParts == 1) {
        // 单部分模型：直接使用基础路径
        filepath = finalBasePath + "_lod" + std::to_string(lodLevel) + ".obj";
    } else {
        // 多部分模型：使用部分索引
        filepath = finalBasePath + "_part" + std::to_string(partIndex) + "_lod" + std::to_string(lodLevel) + ".obj";
    }
    
    // 检查文件是否存在
    if (!std::filesystem::exists(filepath)) {
        LOG_WARNING_F("LODGenerator::LoadPartLODMesh: File not found: %s", filepath.c_str());
        return nullptr;
    }
    
    // 使用 MeshLoader 加载文件
    auto meshes = MeshLoader::LoadFromFile(filepath);
    if (meshes.empty() || !meshes[0]) {
        LOG_ERROR_F("LODGenerator::LoadPartLODMesh: Failed to load mesh from %s", filepath.c_str());
        return nullptr;
    }
    
    LOG_INFO_F("LODGenerator::LoadPartLODMesh: Successfully loaded part %zu LOD%d from %s (%zu triangles)",
               partIndex, lodLevel, filepath.c_str(), meshes[0]->GetTriangleCount());
    
    return meshes[0];
}

std::vector<std::vector<Ref<Mesh>>> LODGenerator::LoadModelLODMeshes(
    Ref<Model> sourceModel,
    const std::string& baseFilepath
) {
    std::vector<std::vector<Ref<Mesh>>> result;
    
    if (!sourceModel) {
        LOG_ERROR_F("LODGenerator::LoadModelLODMeshes: Source model is null");
        return result;
    }
    
    // 获取模型的所有部分
    std::vector<ModelPart> sourceParts;
    sourceModel->AccessParts([&](const std::vector<ModelPart>& parts) {
        sourceParts = parts;
    });
    
    if (sourceParts.empty()) {
        LOG_WARNING_F("LODGenerator::LoadModelLODMeshes: Model has no parts");
        return result;
    }
    
    size_t partCount = sourceParts.size();
    result.resize(partCount);
    
    LOG_INFO_F("LODGenerator::LoadModelLODMeshes: Loading LOD meshes for model with %zu parts", partCount);
    
    // 为每个部分加载所有 LOD 级别
    for (size_t partIndex = 0; partIndex < partCount; ++partIndex) {
        result[partIndex].resize(4);  // LOD0, LOD1, LOD2, LOD3
        
        for (int lodLevel = 0; lodLevel <= 3; ++lodLevel) {
            result[partIndex][lodLevel] = LoadPartLODMesh(baseFilepath, partIndex, lodLevel, partCount);
            
            if (!result[partIndex][lodLevel]) {
                LOG_WARNING_F("LODGenerator::LoadModelLODMeshes: Failed to load part %zu LOD%d", partIndex, lodLevel);
            }
        }
    }
    
    // 统计加载结果
    size_t successCount = 0;
    size_t totalCount = partCount * 4;
    for (const auto& partLODs : result) {
        for (const auto& mesh : partLODs) {
            if (mesh) {
                successCount++;
            }
        }
    }
    
    LOG_INFO_F("LODGenerator::LoadModelLODMeshes: Loaded %zu/%zu LOD meshes", successCount, totalCount);
    
    return result;
}

// ============================================================================
// 纹理 LOD 功能实现（使用 Mipmap）
// ============================================================================

bool LODGenerator::EnsureTextureMipmap(Ref<Texture> texture) {
    if (!texture) {
        LOG_WARNING_F("LODGenerator::EnsureTextureMipmap: 纹理指针为空");
        return false;
    }
    
    if (!texture->IsValid()) {
        LOG_WARNING_F("LODGenerator::EnsureTextureMipmap: 纹理无效");
        return false;
    }
    
    // 检查纹理是否已经有 mipmap
    // 由于 Texture 类没有公开查询 mipmap 状态的方法，我们直接生成
    // GenerateMipmap() 内部会处理重复生成的情况
    
    // 生成 mipmap
    texture->GenerateMipmap();
    
    // 设置正确的过滤模式（三线性过滤，支持 mipmap）
    texture->SetFilter(TextureFilter::Mipmap, TextureFilter::Linear);
    
    LOG_DEBUG_F("LODGenerator::EnsureTextureMipmap: 为纹理配置 mipmap 成功");
    return true;
}

bool LODGenerator::ConfigureMaterialTextureLOD(Ref<Material> material) {
    if (!material) {
        LOG_WARNING_F("LODGenerator::ConfigureMaterialTextureLOD: 材质指针为空");
        return false;
    }
    
    bool allSuccess = true;
    size_t textureCount = 0;
    size_t successCount = 0;
    
    // 遍历材质的所有纹理
    material->ForEachTexture([&](const std::string& name, const Ref<Texture>& texture) {
        textureCount++;
        if (EnsureTextureMipmap(texture)) {
            successCount++;
        } else {
            allSuccess = false;
            LOG_WARNING_F("LODGenerator::ConfigureMaterialTextureLOD: 纹理 '%s' 配置失败", name.c_str());
        }
    });
    
    if (textureCount == 0) {
        LOG_WARNING_F("LODGenerator::ConfigureMaterialTextureLOD: 材质没有纹理");
        return false;
    }
    
    LOG_INFO_F("LODGenerator::ConfigureMaterialTextureLOD: 配置了 %zu/%zu 个纹理的 mipmap", 
               successCount, textureCount);
    
    return allSuccess;
}

bool LODGenerator::AutoConfigureTextureLOD(Ref<Material> material) {
    if (!material) {
        LOG_WARNING_F("LODGenerator::AutoConfigureTextureLOD: 材质指针为空");
        return false;
    }
    
    // 配置材质的所有纹理使用 mipmap
    bool success = ConfigureMaterialTextureLOD(material);
    
    if (success) {
        LOG_INFO_F("LODGenerator::AutoConfigureTextureLOD: 材质纹理 LOD 配置成功");
    } else {
        LOG_WARNING_F("LODGenerator::AutoConfigureTextureLOD: 部分纹理配置失败");
    }
    
    return success;
}

bool LODGenerator::ConfigureModelTextureLOD(Ref<Model> model) {
    if (!model) {
        LOG_WARNING_F("LODGenerator::ConfigureModelTextureLOD: 模型指针为空");
        return false;
    }
    
    bool allSuccess = true;
    size_t partCount = 0;
    size_t successCount = 0;
    
    // 遍历模型的所有部分
    model->AccessParts([&](const std::vector<ModelPart>& parts) {
        partCount = parts.size();
        
        for (size_t i = 0; i < parts.size(); ++i) {
            const auto& part = parts[i];
            if (part.material) {
                if (ConfigureMaterialTextureLOD(part.material)) {
                    successCount++;
                } else {
                    allSuccess = false;
                    LOG_WARNING_F("LODGenerator::ConfigureModelTextureLOD: 部分 %zu 的材质配置失败", i);
                }
            }
        }
    });
    
    if (partCount == 0) {
        LOG_WARNING_F("LODGenerator::ConfigureModelTextureLOD: 模型没有部分");
        return false;
    }
    
    LOG_INFO_F("LODGenerator::ConfigureModelTextureLOD: 配置了 %zu/%zu 个部分的材质纹理 LOD", 
               successCount, partCount);
    
    return allSuccess;
}

bool LODGenerator::AutoConfigureTextureLODStrategy(
    LODConfig& config,
    Ref<Material> material) {
    
    // 设置纹理策略为使用 mipmap
    config.textureStrategy = TextureLODStrategy::UseMipmap;
    
    // 如果提供了材质，配置该材质的纹理
    if (material) {
        bool success = AutoConfigureTextureLOD(material);
        if (!success) {
            LOG_WARNING_F("LODGenerator::AutoConfigureTextureLODStrategy: 材质纹理配置失败");
            return false;
        }
    }
    
    LOG_INFO_F("LODGenerator::AutoConfigureTextureLODStrategy: 纹理 LOD 策略配置成功（使用 Mipmap）");
    return true;
}

} // namespace Render

