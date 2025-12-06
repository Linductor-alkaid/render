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
#pragma once

#include "render/types.h"
#include "render/mesh.h"
#include "render/model.h"
#include "render/lod_system.h"
#include "render/lod_generator.h"
#include "render/file_utils.h"
#include "render/logger.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <regex>

namespace Render {

/**
 * @brief LOD 加载选项
 * 
 * 配置 LOD 数据的加载方式，支持从文件加载或自动生成
 */
struct LODLoadOptions {
    /**
     * @brief 基础路径或基础网格
     * 
     * 如果从文件加载：
     * - basePath: 基础文件路径（不含扩展名），例如 "models/tree"
     * - baseMesh: 可以为 nullptr
     * 
     * 如果自动生成：
     * - basePath: 可选，用于保存生成的LOD文件
     * - baseMesh: 必需，源网格（LOD0）
     */
    std::string basePath;
    Ref<Mesh> baseMesh = nullptr;
    
    /**
     * @brief LOD 文件命名模式
     * 
     * 支持占位符：
     * - {name}: 基础文件名（不含路径和扩展名）
     * - {level}: LOD 级别（0, 1, 2, 3）
     * - {ext}: 文件扩展名（不含点号）
     * 
     * 默认模式: "{name}_lod{level}.{ext}"
     * 示例: "tree_lod1.obj", "tree_lod2.obj"
     * 
     * 其他常见模式：
     * - "{name}.{level}.{ext}" -> "tree.1.obj"
     * - "{name}_LOD{level}.{ext}" -> "tree_LOD1.obj"
     * - "lod{level}/{name}.{ext}" -> "lod1/tree.obj"
     */
    std::string namingPattern = "{name}_lod{level}.{ext}";
    
    /**
     * @brief 文件扩展名（用于从文件加载）
     * 
     * 如果为空，将尝试常见的扩展名（.obj, .fbx, .gltf等）
     */
    std::string fileExtension = "obj";
    
    /**
     * @brief 是否自动生成 LOD（通过网格简化）
     * 
     * 如果为 true，将使用 LODGenerator 自动生成 LOD 级别
     * 如果为 false，将从文件加载 LOD 网格
     */
    bool autoGenerateLOD = false;
    
    /**
     * @brief 自动生成的简化配置
     * 
     * 仅在 autoGenerateLOD = true 时使用
     */
    LODGenerator::SimplifyOptions simplifyOptions;
    
    /**
     * @brief 加载策略
     */
    struct LoadStrategy {
        /**
         * @brief 是否预加载所有 LOD 级别
         * 
         * 如果为 true，一次性加载所有 LOD 级别
         * 如果为 false，按需加载（当前未实现，保留用于未来扩展）
         */
        bool preloadAllLODs = true;
        
        /**
         * @brief 是否异步加载
         * 
         * 当前版本暂不支持异步加载，保留用于未来扩展
         */
        bool asyncLoad = false;
        
        /**
         * @brief 如果某个 LOD 级别文件不存在，是否使用自动生成作为回退
         * 
         * 如果为 true，当文件不存在时，将自动生成该 LOD 级别
         * 如果为 false，文件不存在时返回 nullptr
         */
        bool fallbackToGenerate = false;
    } loadStrategy;
    
    /**
     * @brief 距离阈值配置
     * 
     * 如果为空，将使用默认值 {50.0f, 150.0f, 500.0f, 1000.0f}
     */
    std::vector<float> distanceThresholds;
    
    /**
     * @brief 默认构造函数
     */
    LODLoadOptions() = default;
    
    /**
     * @brief 从基础路径构造（用于从文件加载）
     */
    explicit LODLoadOptions(const std::string& path) : basePath(path) {}
    
    /**
     * @brief 从基础网格构造（用于自动生成）
     */
    explicit LODLoadOptions(Ref<Mesh> mesh) : baseMesh(mesh), autoGenerateLOD(true) {}
};

/**
 * @brief LOD 加载器
 * 
 * 提供统一的 LOD 数据加载接口，支持从文件加载或自动生成
 * 
 * **使用示例**：
 * @code
 * // 方式1: 从文件加载
 * LODLoadOptions options;
 * options.basePath = "models/tree";
 * options.namingPattern = "{name}_lod{level}.{ext}";
 * options.fileExtension = "obj";
 * 
 * LODConfig config = LODLoader::LoadLODConfig(nullptr, options);
 * 
 * // 方式2: 自动生成
 * Ref<Mesh> baseMesh = MeshLoader::LoadMeshFromFile("tree.obj");
 * LODLoadOptions options(baseMesh);
 * options.simplifyOptions = LODGenerator::GetRecommendedOptions(baseMesh);
 * 
 * LODConfig config = LODLoader::LoadLODConfig(baseMesh, options);
 * 
 * // 方式3: 混合模式（文件优先，不存在则生成）
 * LODLoadOptions options;
 * options.basePath = "models/tree";
 * options.baseMesh = baseMesh;
 * options.loadStrategy.fallbackToGenerate = true;
 * 
 * LODConfig config = LODLoader::LoadLODConfig(baseMesh, options);
 * @endcode
 */
class LODLoader {
public:
    /**
     * @brief 加载 LOD 配置
     * 
     * 根据选项从文件加载或自动生成 LOD 网格，并配置 LODConfig
     * 
     * @param baseMesh 基础网格（LOD0），如果为 nullptr 且 autoGenerateLOD=false，将从文件加载
     * @param options 加载选项
     * @return LODConfig 配置好的 LOD 配置
     * 
     * @note 如果从文件加载，baseMesh 可以为 nullptr（将从 basePath 加载 LOD0）
     * @note 如果自动生成，baseMesh 必须提供
     */
    static LODConfig LoadLODConfig(
        Ref<Mesh> baseMesh,
        const LODLoadOptions& options
    );
    
    /**
     * @brief 从文件加载 LOD 网格
     * 
     * 根据命名模式加载不同 LOD 级别的网格文件
     * 
     * @param baseMesh 基础网格（LOD0），如果为 nullptr，将从文件加载
     * @param options 加载选项
     * @return std::vector<Ref<Mesh>> LOD 网格数组 [LOD0, LOD1, LOD2, LOD3]
     * 
     * @note 如果某个级别文件不存在，对应位置为 nullptr
     * @note 如果 loadStrategy.fallbackToGenerate = true，将自动生成缺失的级别
     */
    static std::vector<Ref<Mesh>> LoadLODMeshesFromFiles(
        Ref<Mesh> baseMesh,
        const LODLoadOptions& options
    );
    
    /**
     * @brief 自动生成 LOD 网格
     * 
     * 使用 LODGenerator 自动生成 LOD 级别
     * 
     * @param baseMesh 基础网格（LOD0），必须提供
     * @param options 加载选项（使用 simplifyOptions）
     * @return std::vector<Ref<Mesh>> LOD 网格数组 [LOD0, LOD1, LOD2, LOD3]
     * 
     * @note LOD0 返回 baseMesh（不进行简化）
     * @note 如果某个级别生成失败，对应位置为 nullptr
     */
    static std::vector<Ref<Mesh>> GenerateLODMeshes(
        Ref<Mesh> baseMesh,
        const LODLoadOptions& options
    );
    
    /**
     * @brief 加载单个 LOD 级别的网格
     * 
     * @param basePath 基础文件路径
     * @param lodLevel LOD 级别（0-3）
     * @param namingPattern 命名模式
     * @param fileExtension 文件扩展名
     * @return Ref<Mesh> 加载的网格，失败返回 nullptr
     */
    static Ref<Mesh> LoadSingleLODMesh(
        const std::string& basePath,
        int lodLevel,
        const std::string& namingPattern = "{name}_lod{level}.{ext}",
        const std::string& fileExtension = "obj"
    );
    
    /**
     * @brief 解析命名模式并构建文件路径
     * 
     * @param basePath 基础文件路径
     * @param lodLevel LOD 级别
     * @param namingPattern 命名模式
     * @param fileExtension 文件扩展名
     * @return std::string 构建的文件路径
     * 
     * **示例**：
     * @code
     * // 输入: basePath="models/tree", lodLevel=1, pattern="{name}_lod{level}.{ext}", ext="obj"
     * // 输出: "models/tree_lod1.obj"
     * 
     * // 输入: basePath="models/tree", lodLevel=2, pattern="lod{level}/{name}.{ext}", ext="obj"
     * // 输出: "lod2/tree.obj"
     * @endcode
     */
    static std::string BuildLODFilePath(
        const std::string& basePath,
        int lodLevel,
        const std::string& namingPattern,
        const std::string& fileExtension
    );
    
    /**
     * @brief 尝试多个扩展名加载文件
     * 
     * 按顺序尝试常见的网格文件扩展名
     * 
     * @param basePath 基础文件路径（不含扩展名）
     * @param lodLevel LOD 级别
     * @param namingPattern 命名模式
     * @return Ref<Mesh> 加载的网格，失败返回 nullptr
     */
    static Ref<Mesh> LoadLODMeshWithMultipleExtensions(
        const std::string& basePath,
        int lodLevel,
        const std::string& namingPattern
    );

private:
    /**
     * @brief 提取基础文件名（不含路径和扩展名）
     */
    static std::string ExtractBaseName(const std::string& filepath);
    
    /**
     * @brief 尝试的扩展名列表（按优先级排序）
     */
    static const std::vector<std::string>& GetDefaultExtensions();
};

} // namespace Render

