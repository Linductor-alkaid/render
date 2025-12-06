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
#include "render/lod_loader.h"
#include "render/mesh_loader.h"
#include "render/lod_generator.h"
#include "render/file_utils.h"
#include "render/logger.h"
#include <filesystem>
#include <algorithm>

namespace Render {

// ============================================================================
// 静态辅助方法
// ============================================================================

std::string LODLoader::ExtractBaseName(const std::string& filepath) {
    std::filesystem::path path(filepath);
    std::string filename = path.filename().string();
    
    // 移除扩展名
    size_t lastDot = filename.find_last_of('.');
    if (lastDot != std::string::npos) {
        filename = filename.substr(0, lastDot);
    }
    
    return filename;
}

const std::vector<std::string>& LODLoader::GetDefaultExtensions() {
    static const std::vector<std::string> extensions = {
        "obj", "fbx", "gltf", "glb", "dae", "blend", "3ds", "ply", "stl"
    };
    return extensions;
}

std::string LODLoader::BuildLODFilePath(
    const std::string& basePath,
    int lodLevel,
    const std::string& namingPattern,
    const std::string& fileExtension
) {
    // 提取基础文件名
    std::string baseName = ExtractBaseName(basePath);
    
    // 提取目录路径
    std::filesystem::path path(basePath);
    std::string directory = path.parent_path().string();
    
    // 替换占位符
    std::string result = namingPattern;
    
    // 替换 {name}
    size_t namePos = result.find("{name}");
    if (namePos != std::string::npos) {
        result.replace(namePos, 6, baseName);
    }
    
    // 替换 {level}
    size_t levelPos = result.find("{level}");
    if (levelPos != std::string::npos) {
        result.replace(levelPos, 7, std::to_string(lodLevel));
    }
    
    // 替换 {ext}
    size_t extPos = result.find("{ext}");
    if (extPos != std::string::npos) {
        result.replace(extPos, 5, fileExtension);
    }
    
    // 如果结果包含路径分隔符，需要处理相对路径
    std::filesystem::path finalPath(result);
    if (!finalPath.is_absolute() && !directory.empty()) {
        // 如果模式中包含目录（如 "lod{level}/{name}.{ext}"），需要合并
        if (result.find('/') != std::string::npos || result.find('\\') != std::string::npos) {
            finalPath = std::filesystem::path(directory) / finalPath;
        } else {
            // 否则直接使用目录 + 文件名
            finalPath = std::filesystem::path(directory) / finalPath;
        }
    } else if (!finalPath.is_absolute() && directory.empty()) {
        // 如果 basePath 本身没有目录，直接使用结果
        finalPath = std::filesystem::path(result);
    }
    
    return finalPath.string();
}

Ref<Mesh> LODLoader::LoadSingleLODMesh(
    const std::string& basePath,
    int lodLevel,
    const std::string& namingPattern,
    const std::string& fileExtension
) {
    if (lodLevel < 0 || lodLevel > 3) {
        LOG_ERROR_F("LODLoader::LoadSingleLODMesh: Invalid LOD level %d (must be 0-3)", lodLevel);
        return nullptr;
    }
    
    // 构建文件路径
    std::string filepath = BuildLODFilePath(basePath, lodLevel, namingPattern, fileExtension);
    
    // 检查文件是否存在
    if (!FileUtils::FileExists(filepath)) {
        LOG_WARNING_F("LODLoader::LoadSingleLODMesh: File not found: %s", filepath.c_str());
        return nullptr;
    }
    
    // 加载网格
    auto meshes = MeshLoader::LoadFromFile(filepath);
    if (meshes.empty() || !meshes[0]) {
        LOG_ERROR_F("LODLoader::LoadSingleLODMesh: Failed to load mesh from %s", filepath.c_str());
        return nullptr;
    }
    
    LOG_INFO_F("LODLoader::LoadSingleLODMesh: Successfully loaded LOD%d from %s (%zu triangles)",
               lodLevel, filepath.c_str(), meshes[0]->GetTriangleCount());
    
    return meshes[0];
}

Ref<Mesh> LODLoader::LoadLODMeshWithMultipleExtensions(
    const std::string& basePath,
    int lodLevel,
    const std::string& namingPattern
) {
    // 首先尝试用户指定的扩展名（如果有）
    if (!basePath.empty()) {
        std::string ext = FileUtils::GetFileExtension(basePath);
        if (!ext.empty()) {
            Ref<Mesh> mesh = LoadSingleLODMesh(basePath, lodLevel, namingPattern, ext);
            if (mesh) {
                return mesh;
            }
        }
    }
    
    // 尝试默认扩展名列表
    const auto& extensions = GetDefaultExtensions();
    for (const auto& ext : extensions) {
        Ref<Mesh> mesh = LoadSingleLODMesh(basePath, lodLevel, namingPattern, ext);
        if (mesh) {
            return mesh;
        }
    }
    
    LOG_WARNING_F("LODLoader::LoadLODMeshWithMultipleExtensions: Failed to load LOD%d with any extension", lodLevel);
    return nullptr;
}

// ============================================================================
// 主要接口实现
// ============================================================================

std::vector<Ref<Mesh>> LODLoader::LoadLODMeshesFromFiles(
    Ref<Mesh> baseMesh,
    const LODLoadOptions& options
) {
    std::vector<Ref<Mesh>> lodMeshes(4);  // LOD0, LOD1, LOD2, LOD3
    
    if (options.basePath.empty() && !baseMesh) {
        LOG_ERROR("LODLoader::LoadLODMeshesFromFiles: basePath is empty and baseMesh is null");
        return lodMeshes;
    }
    
    // 确定基础路径
    std::string basePath = options.basePath;
    if (basePath.empty() && baseMesh) {
        // 如果 basePath 为空，尝试从 baseMesh 获取路径（如果 Mesh 有路径信息）
        // 当前 Mesh 没有路径信息，所以需要用户提供 basePath
        LOG_WARNING("LODLoader::LoadLODMeshesFromFiles: basePath is empty, cannot determine file paths");
        return lodMeshes;
    }
    
    // 提取基础文件名（如果 basePath 包含扩展名，需要移除）
    std::string cleanBasePath = basePath;
    std::string ext = FileUtils::GetFileExtension(basePath);
    if (!ext.empty()) {
        // 移除扩展名
        size_t lastDot = cleanBasePath.find_last_of('.');
        if (lastDot != std::string::npos) {
            cleanBasePath = cleanBasePath.substr(0, lastDot);
        }
    }
    
    // LOD0: 使用 baseMesh 或从文件加载
    if (baseMesh) {
        lodMeshes[0] = baseMesh;
        LOG_INFO("LODLoader::LoadLODMeshesFromFiles: Using provided baseMesh as LOD0");
    } else {
        // 尝试加载 LOD0 文件
        if (!options.fileExtension.empty()) {
            lodMeshes[0] = LoadSingleLODMesh(cleanBasePath, 0, options.namingPattern, options.fileExtension);
        } else {
            lodMeshes[0] = LoadLODMeshWithMultipleExtensions(cleanBasePath, 0, options.namingPattern);
        }
        
        if (!lodMeshes[0]) {
            LOG_WARNING("LODLoader::LoadLODMeshesFromFiles: Failed to load LOD0, cannot continue");
            return lodMeshes;
        }
    }
    
    // 加载 LOD1-LOD3
    for (int lodLevel = 1; lodLevel <= 3; ++lodLevel) {
        if (!options.fileExtension.empty()) {
            lodMeshes[lodLevel] = LoadSingleLODMesh(cleanBasePath, lodLevel, options.namingPattern, options.fileExtension);
        } else {
            lodMeshes[lodLevel] = LoadLODMeshWithMultipleExtensions(cleanBasePath, lodLevel, options.namingPattern);
        }
        
        // 如果文件不存在且启用了回退生成
        if (!lodMeshes[lodLevel] && options.loadStrategy.fallbackToGenerate && lodMeshes[0]) {
            LOG_INFO_F("LODLoader::LoadLODMeshesFromFiles: LOD%d file not found, generating automatically", lodLevel);
            
            // 使用 LODGenerator 生成
            Ref<Mesh> generated = LODGenerator::GenerateLODLevel(lodMeshes[0], lodLevel, options.simplifyOptions);
            if (generated) {
                lodMeshes[lodLevel] = generated;
                LOG_INFO_F("LODLoader::LoadLODMeshesFromFiles: Successfully generated LOD%d", lodLevel);
            } else {
                LOG_WARNING_F("LODLoader::LoadLODMeshesFromFiles: Failed to generate LOD%d", lodLevel);
            }
        }
    }
    
    // 统计加载结果
    size_t successCount = 0;
    for (const auto& mesh : lodMeshes) {
        if (mesh) {
            successCount++;
        }
    }
    
    LOG_INFO_F("LODLoader::LoadLODMeshesFromFiles: Loaded %zu/%zu LOD meshes", successCount, lodMeshes.size());
    
    return lodMeshes;
}

std::vector<Ref<Mesh>> LODLoader::GenerateLODMeshes(
    Ref<Mesh> baseMesh,
    const LODLoadOptions& options
) {
    std::vector<Ref<Mesh>> lodMeshes(4);  // LOD0, LOD1, LOD2, LOD3
    
    if (!baseMesh) {
        LOG_ERROR("LODLoader::GenerateLODMeshes: baseMesh is null");
        return lodMeshes;
    }
    
    // LOD0: 使用原始网格
    lodMeshes[0] = baseMesh;
    
    // 生成 LOD1-LOD3
    auto generated = LODGenerator::GenerateLODLevels(baseMesh, options.simplifyOptions);
    
    // generated 返回的是 [LOD1, LOD2, LOD3]
    if (generated.size() >= 3) {
        lodMeshes[1] = generated[0];
        lodMeshes[2] = generated[1];
        lodMeshes[3] = generated[2];
    } else {
        // 如果生成失败，对应位置保持 nullptr
        for (size_t i = 0; i < generated.size(); ++i) {
            lodMeshes[i + 1] = generated[i];
        }
    }
    
    // 统计生成结果
    size_t successCount = 0;
    for (const auto& mesh : lodMeshes) {
        if (mesh) {
            successCount++;
        }
    }
    
    LOG_INFO_F("LODLoader::GenerateLODMeshes: Generated %zu/%zu LOD meshes", successCount - 1, 3);  // -1 因为 LOD0 不算生成
    
    return lodMeshes;
}

LODConfig LODLoader::LoadLODConfig(
    Ref<Mesh> baseMesh,
    const LODLoadOptions& options
) {
    LODConfig config;
    config.enabled = true;
    
    // 设置距离阈值
    if (!options.distanceThresholds.empty()) {
        config.distanceThresholds = options.distanceThresholds;
    } else {
        // 使用默认值
        config.distanceThresholds = {50.0f, 150.0f, 500.0f, 1000.0f};
    }
    
    // 根据选项加载或生成 LOD 网格
    if (options.autoGenerateLOD) {
        // 自动生成模式
        if (!baseMesh && !options.baseMesh) {
            LOG_ERROR("LODLoader::LoadLODConfig: autoGenerateLOD requires baseMesh");
            return config;
        }
        
        Ref<Mesh> sourceMesh = baseMesh ? baseMesh : options.baseMesh;
        config.lodMeshes = GenerateLODMeshes(sourceMesh, options);
        
        LOG_INFO("LODLoader::LoadLODConfig: Generated LOD meshes automatically");
    } else {
        // 从文件加载模式
        Ref<Mesh> sourceMesh = baseMesh ? baseMesh : options.baseMesh;
        config.lodMeshes = LoadLODMeshesFromFiles(sourceMesh, options);
        
        LOG_INFO("LODLoader::LoadLODConfig: Loaded LOD meshes from files");
    }
    
    // 验证配置
    size_t validCount = 0;
    for (const auto& mesh : config.lodMeshes) {
        if (mesh) {
            validCount++;
        }
    }
    
    if (validCount == 0) {
        LOG_WARNING("LODLoader::LoadLODConfig: No valid LOD meshes loaded, LOD will be disabled");
        config.enabled = false;
    } else if (validCount == 1 && config.lodMeshes[0]) {
        LOG_INFO("LODLoader::LoadLODConfig: Only LOD0 loaded, LOD will use original mesh for all levels");
    } else {
        LOG_INFO_F("LODLoader::LoadLODConfig: Successfully configured LOD with %zu levels", validCount);
    }
    
    return config;
}

} // namespace Render

