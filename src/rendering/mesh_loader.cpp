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
#include "render/mesh_loader.h"
#include "render/material.h"
#include "render/texture_loader.h"
#include "render/logger.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <limits>
#include <unordered_map>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/config.h>
#include <filesystem>
#include <thread>
#include <chrono>
#include <functional>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Render {

// ============================================================================
// 辅助函数 - Assimp 常用转换
// ============================================================================

static Matrix4 ConvertMatrix(const aiMatrix4x4& matrix) {
    Matrix4 result;
    result(0, 0) = matrix.a1; result(0, 1) = matrix.a2; result(0, 2) = matrix.a3; result(0, 3) = matrix.a4;
    result(1, 0) = matrix.b1; result(1, 1) = matrix.b2; result(1, 2) = matrix.b3; result(1, 3) = matrix.b4;
    result(2, 0) = matrix.c1; result(2, 1) = matrix.c2; result(2, 2) = matrix.c3; result(2, 3) = matrix.c4;
    result(3, 0) = matrix.d1; result(3, 1) = matrix.d2; result(3, 2) = matrix.d3; result(3, 3) = matrix.d4;
    return result;
}

static std::string ResolveBasePath(const std::string& filepath, const std::string& overrideBasePath) {
    if (!overrideBasePath.empty()) {
        return overrideBasePath;
    }
    size_t lastSlash = filepath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        return filepath.substr(0, lastSlash);
    }
    return ".";
}

static unsigned int GeneratePostProcessFlags(const MeshImportOptions& options) {
    unsigned int flags = 0;
    if (options.triangulate) {
        flags |= aiProcess_Triangulate;
    }
    if (options.generateSmoothNormals) {
        flags |= aiProcess_GenSmoothNormals;
    }
    if (options.calculateTangentSpace) {
        flags |= aiProcess_CalcTangentSpace;
    }
    if (options.joinIdenticalVertices) {
        flags |= aiProcess_JoinIdenticalVertices;
    }
    if (options.sortByPrimitiveType) {
        flags |= aiProcess_SortByPType;
    }
    if (options.improveCacheLocality) {
        flags |= aiProcess_ImproveCacheLocality;
    }
    if (options.optimizeMeshes) {
        flags |= aiProcess_OptimizeMeshes;
    }
    if (options.validateDataStructure) {
        flags |= aiProcess_ValidateDataStructure;
    }
    if (options.generateUVCoords) {
        flags |= aiProcess_GenUVCoords;
    }
    if (options.transformUVCoords) {
        flags |= aiProcess_TransformUVCoords;
    }
    if (options.findInvalidData) {
        flags |= aiProcess_FindInvalidData;
    }
    if (options.populateArmatureData) {
        flags |= aiProcess_PopulateArmatureData;
    }
    return flags;
}

static const aiNode* FindNodeByName(const aiNode* node, const std::string& target) {
    if (!node) {
        return nullptr;
    }
    if (target == node->mName.C_Str()) {
        return node;
    }
    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        if (const aiNode* found = FindNodeByName(node->mChildren[i], target)) {
            return found;
        }
    }
    return nullptr;
}

// ============================================================================
// 辅助函数 - Assimp 网格处理
// ============================================================================

/**
 * @brief 处理单个 Assimp 网格并转换为引擎网格对象
 */
static Ref<Mesh> ProcessAssimpMesh(
    aiMesh* assimpMesh,
    const aiScene* scene,
    bool autoUpload = true,
    MeshExtraData* extraData = nullptr,
    const MeshImportOptions* options = nullptr) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    const bool hasTangents = assimpMesh->HasTangentsAndBitangents();
    bool hasPrimaryUV = assimpMesh->mTextureCoords[0] != nullptr;
    bool requireTangentRecalculate = !hasTangents;
    const float tangentEpsilon = 1e-5f;

    if (extraData) {
        extraData->uvChannels.clear();
        extraData->colorChannels.clear();
        extraData->skinning.Clear();
    }
    
    // 提取顶点数据
    vertices.reserve(assimpMesh->mNumVertices);
    for (uint32_t i = 0; i < assimpMesh->mNumVertices; i++) {
        Vertex vertex;
        vertex.tangent = Vector3::Zero();
        vertex.bitangent = Vector3::Zero();
        
        // 位置（必须有）
        vertex.position = Vector3(
            assimpMesh->mVertices[i].x,
            assimpMesh->mVertices[i].y,
            assimpMesh->mVertices[i].z
        );
        
        // 法线（可选）
        if (assimpMesh->HasNormals()) {
            vertex.normal = Vector3(
                assimpMesh->mNormals[i].x,
                assimpMesh->mNormals[i].y,
                assimpMesh->mNormals[i].z
            );
        } else {
            vertex.normal = Vector3(0.0f, 1.0f, 0.0f);  // 默认向上
        }
        
        // 纹理坐标（可选，使用第一套 UV）
        if (assimpMesh->mTextureCoords[0]) {
            vertex.texCoord = Vector2(
                assimpMesh->mTextureCoords[0][i].x,
                assimpMesh->mTextureCoords[0][i].y
            );
        } else {
            vertex.texCoord = Vector2(0.0f, 0.0f);
        }
        
        // 顶点颜色（可选，使用第一套颜色）
        if (assimpMesh->HasVertexColors(0)) {
            vertex.color = Color(
                assimpMesh->mColors[0][i].r,
                assimpMesh->mColors[0][i].g,
                assimpMesh->mColors[0][i].b,
                assimpMesh->mColors[0][i].a
            );
        } else {
            vertex.color = Color::White();
        }

        if (hasTangents) {
            vertex.tangent = Vector3(
                assimpMesh->mTangents[i].x,
                assimpMesh->mTangents[i].y,
                assimpMesh->mTangents[i].z
            );
            vertex.bitangent = Vector3(
                assimpMesh->mBitangents[i].x,
                assimpMesh->mBitangents[i].y,
                assimpMesh->mBitangents[i].z
            );
            if (vertex.tangent.squaredNorm() < tangentEpsilon || vertex.bitangent.squaredNorm() < tangentEpsilon) {
                requireTangentRecalculate = true;
            }
        }
        
        vertices.push_back(vertex);
    }

    // 收集额外的 UV 通道
    if (extraData) {
        unsigned int uvChannelCount = assimpMesh->GetNumUVChannels();
        if (uvChannelCount > 0) {
            unsigned int channelsToStore = uvChannelCount;
            if (options && !options->gatherAdditionalUVs) {
                channelsToStore = std::min<unsigned int>(1u, uvChannelCount);
            }
            extraData->uvChannels.resize(channelsToStore);
            for (unsigned int channel = 0; channel < channelsToStore; ++channel) {
                auto& uvData = extraData->uvChannels[channel];
                uvData.reserve(assimpMesh->mNumVertices);
                if (assimpMesh->mTextureCoords[channel]) {
                    for (uint32_t i = 0; i < assimpMesh->mNumVertices; ++i) {
                        uvData.emplace_back(
                            assimpMesh->mTextureCoords[channel][i].x,
                            assimpMesh->mTextureCoords[channel][i].y
                        );
                    }
                } else {
                    for (uint32_t i = 0; i < assimpMesh->mNumVertices; ++i) {
                        uvData.emplace_back(0.0f, 0.0f);
                    }
                }
            }
        }

        // 收集额外颜色通道
        unsigned int colorChannelCount = assimpMesh->GetNumColorChannels();
        if (colorChannelCount > 0) {
            unsigned int channelsToStore = colorChannelCount;
            if (options && !options->gatherVertexColors) {
                channelsToStore = std::min<unsigned int>(1u, colorChannelCount);
            }
            extraData->colorChannels.resize(channelsToStore);
            for (unsigned int channel = 0; channel < channelsToStore; ++channel) {
                auto& colorData = extraData->colorChannels[channel];
                colorData.reserve(assimpMesh->mNumVertices);
                if (assimpMesh->HasVertexColors(channel)) {
                    for (uint32_t i = 0; i < assimpMesh->mNumVertices; ++i) {
                        const auto& color = assimpMesh->mColors[channel][i];
                        colorData.emplace_back(color.r, color.g, color.b, color.a);
                    }
                } else {
                    for (uint32_t i = 0; i < assimpMesh->mNumVertices; ++i) {
                        colorData.push_back(Color::White());
                    }
                }
            }
        }
    }
    
    // 提取索引数据
    indices.reserve(assimpMesh->mNumFaces * 3);
    for (uint32_t i = 0; i < assimpMesh->mNumFaces; i++) {
        aiFace face = assimpMesh->mFaces[i];
        for (uint32_t j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }
    
    if (hasTangents) {
        const float EPSILON = 1e-6f;
        for (auto& vertex : vertices) {
            Vector3 normal = vertex.normal;
            if (normal.squaredNorm() < EPSILON) {
                normal = Vector3::UnitY();
            } else {
                normal.normalize();
            }

            Vector3 tangent = vertex.tangent;
            if (tangent.squaredNorm() < EPSILON) {
                tangent = Vector3::UnitX();
            }
            tangent = tangent - normal * normal.dot(tangent);
            float tLen = tangent.norm();
            if (tLen < EPSILON) {
                tangent = Vector3::UnitX();
            } else {
                tangent /= tLen;
            }

            Vector3 bitangent = vertex.bitangent;
            float handedness = 1.0f;
            if (bitangent.squaredNorm() >= EPSILON) {
                handedness = (normal.cross(tangent).dot(bitangent) < 0.0f) ? -1.0f : 1.0f;
            }
            bitangent = normal.cross(tangent) * handedness;

            vertex.normal = normal;
            vertex.tangent = tangent;
            vertex.bitangent = bitangent;
        }
    }

    // 蒙皮数据采集
    if (extraData && options && options->gatherBones && assimpMesh->HasBones()) {
        auto& skinning = extraData->skinning;
        skinning.Clear();
        skinning.bones.reserve(assimpMesh->mNumBones);
        skinning.boneOffsetMatrices.reserve(assimpMesh->mNumBones);
        skinning.vertexWeights.resize(assimpMesh->mNumVertices);
        skinning.boneNameToIndex.reserve(assimpMesh->mNumBones);

        std::vector<std::vector<std::pair<uint32_t, float>>> rawWeights(assimpMesh->mNumVertices);

        for (uint32_t boneIdx = 0; boneIdx < assimpMesh->mNumBones; ++boneIdx) {
            aiBone* bone = assimpMesh->mBones[boneIdx];
            MeshBoneInfo boneInfo;
            boneInfo.name = bone->mName.C_Str();
            boneInfo.parentName.clear();
            boneInfo.vertexWeights.reserve(bone->mNumWeights);
            uint32_t boneIndex = static_cast<uint32_t>(skinning.bones.size());
            skinning.boneNameToIndex[boneInfo.name] = boneIndex;
            skinning.bones.push_back(std::move(boneInfo));
            skinning.boneOffsetMatrices.push_back(ConvertMatrix(bone->mOffsetMatrix));

            for (uint32_t weightIdx = 0; weightIdx < bone->mNumWeights; ++weightIdx) {
                const aiVertexWeight& weight = bone->mWeights[weightIdx];
                if (weight.mVertexId >= assimpMesh->mNumVertices) {
                    continue;
                }
                rawWeights[weight.mVertexId].emplace_back(boneIndex, weight.mWeight);
            }
        }

        // 填充父骨骼信息（如存在）
        if (scene && scene->mRootNode) {
            for (auto& boneInfo : skinning.bones) {
                const aiNode* boneNode = FindNodeByName(scene->mRootNode, boneInfo.name);
                if (!boneNode) {
                    continue;
                }
                if (boneNode->mParent) {
                    boneInfo.parentName = boneNode->mParent->mName.C_Str();
                }
            }
        }

        const bool limitWeights = options->limitBoneWeightsPerVertex && options->maxBoneWeightsPerVertex > 0;
        const bool normalizeWeights = options->normalizeBoneWeights;

        for (uint32_t vertexIndex = 0; vertexIndex < assimpMesh->mNumVertices; ++vertexIndex) {
            auto& weights = rawWeights[vertexIndex];
            if (weights.empty()) {
                continue;
            }

            if (limitWeights && weights.size() > options->maxBoneWeightsPerVertex) {
                std::sort(weights.begin(), weights.end(), [](const auto& lhs, const auto& rhs) {
                    return lhs.second > rhs.second;
                });
                weights.resize(options->maxBoneWeightsPerVertex);
            }

            float weightSum = 0.0f;
            for (const auto& entry : weights) {
                weightSum += entry.second;
            }

            if (weightSum > 0.0f && normalizeWeights) {
                for (auto& entry : weights) {
                    entry.second /= weightSum;
                }
            }

            auto& vertexWeightsForVertex = skinning.vertexWeights[vertexIndex];
            vertexWeightsForVertex.reserve(weights.size());

            for (const auto& entry : weights) {
                const uint32_t boneIndex = entry.first;
                const float weight = entry.second;
                vertexWeightsForVertex.push_back({ boneIndex, weight });
                if (boneIndex < skinning.bones.size()) {
                    skinning.bones[boneIndex].vertexWeights.push_back({ vertexIndex, weight });
                }
            }
        }
    }

    // 创建网格
    auto mesh = CreateRef<Mesh>(vertices, indices);

    if (requireTangentRecalculate) {
        if (hasPrimaryUV) {
            mesh->RecalculateTangents();
        } else {
            Logger::GetInstance().Warning("MeshLoader: 由于缺少有效切线且没有 UV，无法重建切线空间");
        }
    }
    
    // ⭐ v0.12.0: 条件上传（支持异步加载）
    if (autoUpload) {
        mesh->Upload();
    }
    
    Logger::GetInstance().Info("Processed mesh: " + std::to_string(vertices.size()) + 
                               " vertices, " + std::to_string(indices.size() / 3) + " triangles");
    
    return mesh;
}

/**
 * @brief 递归处理 Assimp 场景节点
 */
static void ProcessAssimpNode(aiNode* node, const aiScene* scene, std::vector<Ref<Mesh>>& meshes, bool autoUpload = true) {
    // 处理当前节点的所有网格
    for (uint32_t i = 0; i < node->mNumMeshes; i++) {
        aiMesh* assimpMesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(ProcessAssimpMesh(assimpMesh, scene, autoUpload));
    }
    
    // 递归处理子节点
    for (uint32_t i = 0; i < node->mNumChildren; i++) {
        ProcessAssimpNode(node->mChildren[i], scene, meshes, autoUpload);
    }
}

// ============================================================================
// 辅助函数 - Assimp 材质处理
// ============================================================================

/**
 * @brief 从 Assimp 材质加载纹理
 */
static Ref<Texture> LoadMaterialTexture(
    aiMaterial* mat,
    aiTextureType type,
    const std::string& basePath,
    const std::string& textureName,
    const std::string& slotName,
    bool autoUpload,
    std::vector<MaterialTextureRequest>* pendingRequests)
{
    if (mat->GetTextureCount(type) == 0) {
        return nullptr;
    }
    
    aiString texPath;
    mat->GetTexture(type, 0, &texPath);
    
    std::string texPathStr(texPath.C_Str());
    if (texPathStr.empty()) {
        return nullptr;
    }
    
    // 构建完整路径（简化处理，避免 filesystem 可能的中文路径问题）
    std::string fullPathStr;
    if (!basePath.empty()) {
        fullPathStr = basePath + "/" + texPathStr;
    } else {
        fullPathStr = texPathStr;
    }
    
    // 使用 TextureLoader 加载纹理（带缓存）
    if (!autoUpload) {
        if (pendingRequests) {
            MaterialTextureRequest request;
            request.slotName = slotName;
            request.textureName = textureName;
            request.filePath = fullPathStr;
            request.generateMipmap = true;
            pendingRequests->push_back(std::move(request));
            Logger::GetInstance().Info("Queued texture for deferred upload: " + fullPathStr);
        }
        return nullptr;
    }

    auto texture = TextureLoader::GetInstance().LoadTexture(textureName, fullPathStr);
    
    if (texture) {
        Logger::GetInstance().Info("Loaded texture: " + texPathStr);
    } else {
        Logger::GetInstance().Warning("Failed to load texture: " + fullPathStr);
    }
    
    return texture;
}

/**
 * @brief 从 Assimp 材质创建 Material 对象
 */
static Ref<Material> ProcessAssimpMaterial(
    aiMaterial* aiMat,
    const aiScene* scene,
    const std::string& basePath,
    Ref<Shader> shader,
    uint32_t materialIndex,
    bool autoUpload,
    std::vector<MaterialTextureRequest>* pendingRequests)
{
    auto material = CreateRef<Material>();
    
    // 设置材质名称
    aiString materialName;
    if (aiMat->Get(AI_MATKEY_NAME, materialName) == AI_SUCCESS) {
        material->SetName(std::string(materialName.C_Str()));
    } else {
        material->SetName("Material_" + std::to_string(materialIndex));
    }
    
    // 设置着色器
    if (shader) {
        material->SetShader(shader);
    }
    
    // 获取颜色属性
    aiColor3D color(0.0f, 0.0f, 0.0f);
    
    // 环境光颜色
    if (aiMat->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS) {
        material->SetAmbientColor(Color(color.r, color.g, color.b, 1.0f));
    }
    
    // 漫反射颜色
    if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
        material->SetDiffuseColor(Color(color.r, color.g, color.b, 1.0f));
    }
    
    // 镜面反射颜色
    if (aiMat->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS) {
        material->SetSpecularColor(Color(color.r, color.g, color.b, 1.0f));
    }
    
    // 自发光颜色
    if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS) {
        material->SetEmissiveColor(Color(color.r, color.g, color.b, 1.0f));
    }
    
    // 镜面反射强度（光泽度）
    float shininess = 32.0f;
    if (aiMat->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS) {
        material->SetShininess(shininess);
    }
    
    // 不透明度
    float opacity = 1.0f;
    if (aiMat->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) {
        material->SetOpacity(opacity);
        if (opacity < 1.0f) {
            material->SetBlendMode(BlendMode::Alpha);
            material->SetDepthWrite(false);
        }
    }
    
    // PBR 属性（如果可用）
    float metallic = 0.0f;
    if (aiMat->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS) {
        material->SetMetallic(metallic);
    }
    
    float roughness = 0.5f;
    if (aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) {
        material->SetRoughness(roughness);
    }
    
    // 加载纹理贴图
    std::string matNameStr = material->GetName();
    if (matNameStr.empty()) {
        matNameStr = "Material_" + std::to_string(materialIndex);
        material->SetName(matNameStr);
    }
    
    // 漫反射贴图（使用纹理路径作为唯一标识，而不是材质名）
    // 先获取纹理路径，用作缓存键
    std::string diffuseTexName;
    if (aiMat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
        aiString texPath;
        aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath);
        std::string texPathStr(texPath.C_Str());
        // 使用完整路径作为纹理标识
        diffuseTexName = basePath + "/" + texPathStr;
    }
    
    auto diffuseMap = LoadMaterialTexture(
        aiMat,
        aiTextureType_DIFFUSE,
        basePath,
        diffuseTexName.empty() ? (matNameStr + "_diffuse") : diffuseTexName,
        "diffuseMap",
        autoUpload,
        pendingRequests);
    if (diffuseMap) {
        material->SetTexture("diffuseMap", diffuseMap);
    }
    
    // 镜面反射贴图
    auto specularMap = LoadMaterialTexture(
        aiMat,
        aiTextureType_SPECULAR,
        basePath,
        matNameStr + "_specular",
        "specularMap",
        autoUpload,
        pendingRequests);
    if (specularMap) {
        material->SetTexture("specularMap", specularMap);
    }
    
    // 法线贴图
    auto normalMap = LoadMaterialTexture(
        aiMat,
        aiTextureType_NORMALS,
        basePath,
        matNameStr + "_normal",
        "normalMap",
        autoUpload,
        pendingRequests);
    if (!normalMap) {
        // 有些格式使用 HEIGHT 代替 NORMALS
        normalMap = LoadMaterialTexture(
            aiMat,
            aiTextureType_HEIGHT,
            basePath,
            matNameStr + "_normal",
            "normalMap",
            autoUpload,
            pendingRequests);
    }
    if (normalMap) {
        material->SetTexture("normalMap", normalMap);
    }
    
    // 环境遮蔽贴图
    auto aoMap = LoadMaterialTexture(
        aiMat,
        aiTextureType_AMBIENT_OCCLUSION,
        basePath,
        matNameStr + "_ao",
        "aoMap",
        autoUpload,
        pendingRequests);
    if (aoMap) {
        material->SetTexture("aoMap", aoMap);
    }
    
    // 自发光贴图
    auto emissiveMap = LoadMaterialTexture(
        aiMat,
        aiTextureType_EMISSIVE,
        basePath,
        matNameStr + "_emissive",
        "emissiveMap",
        autoUpload,
        pendingRequests);
    if (emissiveMap) {
        material->SetTexture("emissiveMap", emissiveMap);
    }
    
    Logger::GetInstance().Info("Processed material: " + material->GetName());
    
    return material;
}

/**
 * @brief 递归处理节点（包含材质）
 */
static void ProcessAssimpNodeWithMaterials(
    aiNode* node,
    const aiScene* scene,
    const std::string& basePath,
    Ref<Shader> shader,
    std::vector<MeshWithMaterial>& results)
{
    // 处理当前节点的所有网格
    for (uint32_t i = 0; i < node->mNumMeshes; i++) {
        aiMesh* assimpMesh = scene->mMeshes[node->mMeshes[i]];
        
        // 处理网格（自动上传 - 假设在主线程调用）
        auto mesh = ProcessAssimpMesh(assimpMesh, scene, true);
        
        // 处理材质（如果有）
        Ref<Material> material = nullptr;
        if (assimpMesh->mMaterialIndex >= 0 && 
            assimpMesh->mMaterialIndex < scene->mNumMaterials) {
            aiMaterial* aiMat = scene->mMaterials[assimpMesh->mMaterialIndex];
            material = ProcessAssimpMaterial(
                aiMat,
                scene,
                basePath,
                shader,
                assimpMesh->mMaterialIndex,
                true,
                nullptr);
        }
        
        // 获取网格名称
        std::string meshName = assimpMesh->mName.C_Str();
        if (meshName.empty()) {
            meshName = "Mesh_" + std::to_string(i);
        }
        
        results.push_back(MeshWithMaterial(mesh, material, meshName));
    }
    
    // 递归处理子节点
    for (uint32_t i = 0; i < node->mNumChildren; i++) {
        ProcessAssimpNodeWithMaterials(node->mChildren[i], scene, basePath, shader, results);
    }
}

/**
 * @brief 递归处理节点并采集额外数据
 */
static void ProcessAssimpNodeDetailed(
    aiNode* node,
    const aiScene* scene,
    const MeshImportOptions& options,
    const std::string& basePath,
    Ref<Shader> shader,
    const Matrix4& parentTransform,
    std::vector<MeshImportResult>& results)
{
    Matrix4 localTransform = ConvertMatrix(node->mTransformation);
    Matrix4 worldTransform = parentTransform * localTransform;

    for (uint32_t i = 0; i < node->mNumMeshes; ++i) {
        aiMesh* assimpMesh = scene->mMeshes[node->mMeshes[i]];

        MeshExtraData extra;
        extra.localTransform = localTransform;
        extra.worldTransform = worldTransform;
        extra.assimpMeshIndex = node->mMeshes[i];

        auto mesh = ProcessAssimpMesh(assimpMesh, scene, options.autoUpload, &extra, &options);

        Ref<Material> material = nullptr;
        if (options.loadMaterials &&
            assimpMesh->mMaterialIndex >= 0 &&
            assimpMesh->mMaterialIndex < scene->mNumMaterials) {
            material = ProcessAssimpMaterial(
                scene->mMaterials[assimpMesh->mMaterialIndex],
                scene,
                basePath,
                shader,
                assimpMesh->mMaterialIndex,
                options.autoUpload,
                &extra.pendingTextureRequests
            );
        }

        std::string meshName = assimpMesh->mName.C_Str();
        if (meshName.empty()) {
            meshName = node->mName.C_Str();
        }
        if (meshName.empty()) {
            meshName = "Mesh_" + std::to_string(results.size());
        }

        MeshImportResult result;
        result.mesh = mesh;
        result.material = material;
        result.name = meshName;
        result.extra = std::move(extra);
        results.push_back(std::move(result));
    }

    for (uint32_t i = 0; i < node->mNumChildren; ++i) {
        ProcessAssimpNodeDetailed(
            node->mChildren[i],
            scene,
            options,
            basePath,
            shader,
            worldTransform,
            results
        );
    }
}

// ============================================================================
// MeshLoader - 文件加载实现
// ============================================================================

std::vector<Ref<Mesh>> MeshLoader::LoadFromFile(const std::string& filepath, bool flipUVs, bool autoUpload) {
    std::vector<Ref<Mesh>> meshes;
    
    Logger::GetInstance().Info("Loading model from file: " + filepath + 
                               (autoUpload ? " (自动上传)" : " (延迟上传)"));
    
    // 创建 Assimp 导入器
    Assimp::Importer importer;
    
    // 设置后处理标志
    unsigned int postProcessFlags = 
        aiProcess_Triangulate |           // 转换为三角形
        aiProcess_GenSmoothNormals |      // 生成平滑法线（如果没有）
        aiProcess_CalcTangentSpace |      // 计算切线空间（用于法线贴图）
        aiProcess_JoinIdenticalVertices | // 合并相同顶点（优化）
        aiProcess_SortByPType |           // 按原始类型排序
        aiProcess_ImproveCacheLocality |  // 改善顶点缓存局部性
        aiProcess_OptimizeMeshes |        // 优化网格
        aiProcess_ValidateDataStructure;  // 验证数据结构
    
    // 如果需要翻转 UV（OpenGL 约定）
    if (flipUVs) {
        postProcessFlags |= aiProcess_FlipUVs;
    }
    
    // 读取文件
    const aiScene* scene = importer.ReadFile(filepath, postProcessFlags);
    
    // 检查加载错误
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        Logger::GetInstance().Error("Assimp failed to load model: " + std::string(importer.GetErrorString()));
        return meshes;
    }
    
    Logger::GetInstance().Info("Model loaded successfully. Processing meshes...");
    
    // 递归处理场景中的所有节点和网格
    ProcessAssimpNode(scene->mRootNode, scene, meshes, autoUpload);
    
    Logger::GetInstance().Info("Model loading complete. Total meshes: " + std::to_string(meshes.size()));
    
    return meshes;
}

Ref<Mesh> MeshLoader::LoadMeshFromFile(const std::string& filepath, uint32_t meshIndex, bool flipUVs, bool autoUpload) {
    auto meshes = LoadFromFile(filepath, flipUVs, autoUpload);
    
    if (meshes.empty()) {
        Logger::GetInstance().Error("No meshes found in file: " + filepath);
        return nullptr;
    }
    
    if (meshIndex >= meshes.size()) {
        Logger::GetInstance().Warning("Mesh index " + std::to_string(meshIndex) + 
                                      " out of range (total: " + std::to_string(meshes.size()) + 
                                      "). Returning first mesh.");
        return meshes[0];
    }
    
    return meshes[meshIndex];
}

std::vector<MeshWithMaterial> MeshLoader::LoadFromFileWithMaterials(
    const std::string& filepath,
    const std::string& basePath,
    bool flipUVs,
    Ref<Shader> shader)
{
    std::vector<MeshWithMaterial> results;
    
    Logger::GetInstance().Info("Loading model with materials from file: " + filepath);
    
    // 确定纹理搜索基础路径
    std::string actualBasePath = ResolveBasePath(filepath, basePath);
    
    Logger::GetInstance().Info("Texture base path: " + actualBasePath);
    
    // 创建 Assimp 导入器
    Assimp::Importer importer;
    
    // 设置后处理标志
    unsigned int postProcessFlags = 
        aiProcess_Triangulate |           // 转换为三角形
        aiProcess_GenSmoothNormals |      // 生成平滑法线
        aiProcess_CalcTangentSpace |      // 计算切线空间（法线贴图）
        aiProcess_JoinIdenticalVertices | // 合并相同顶点
        aiProcess_SortByPType |           // 按原始类型排序
        aiProcess_ImproveCacheLocality |  // 改善顶点缓存局部性
        aiProcess_OptimizeMeshes |        // 优化网格
        aiProcess_ValidateDataStructure;  // 验证数据结构
    
    if (flipUVs) {
        postProcessFlags |= aiProcess_FlipUVs;
    }
    
    // 读取文件
    const aiScene* scene = importer.ReadFile(filepath, postProcessFlags);
    
    // 检查加载错误
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        Logger::GetInstance().Error("Assimp failed to load model: " + std::string(importer.GetErrorString()));
        return results;
    }
    
    Logger::GetInstance().Info("Model loaded successfully.");
    Logger::GetInstance().Info("Materials in scene: " + std::to_string(scene->mNumMaterials));
    Logger::GetInstance().Info("Processing meshes with materials...");
    
    // 递归处理场景中的所有节点、网格和材质
    ProcessAssimpNodeWithMaterials(scene->mRootNode, scene, actualBasePath, shader, results);
    
    Logger::GetInstance().Info("Model loading complete. Total meshes: " + std::to_string(results.size()));
    
    // 统计材质数量
    int materialsLoaded = 0;
    for (const auto& item : results) {
        if (item.material) {
            materialsLoaded++;
        }
    }
    Logger::GetInstance().Info("Materials loaded: " + std::to_string(materialsLoaded) + " / " + 
                               std::to_string(results.size()));
    
    return results;
}

std::vector<MeshImportResult> MeshLoader::LoadDetailedFromFile(
    const std::string& filepath,
    const MeshImportOptions& options,
    const std::string& basePath,
    Ref<Shader> shader)
{
    std::vector<MeshImportResult> results;

    Logger::GetInstance().Info("Loading detailed model from file: " + filepath);

    Assimp::Importer importer;
    if (options.limitBoneWeightsPerVertex && options.maxBoneWeightsPerVertex > 0) {
        importer.SetPropertyInteger(
            AI_CONFIG_PP_LBW_MAX_WEIGHTS,
            static_cast<int>(options.maxBoneWeightsPerVertex)
        );
    }

    unsigned int postProcessFlags = GeneratePostProcessFlags(options);
    if (options.flipUVs) {
        postProcessFlags |= aiProcess_FlipUVs;
    }

    const aiScene* scene = importer.ReadFile(filepath, postProcessFlags);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        Logger::GetInstance().Error("Assimp failed to load model: " + std::string(importer.GetErrorString()));
        return results;
    }

    std::string actualBasePath = ResolveBasePath(filepath, basePath);
    Matrix4 identity = Matrix4::Identity();

    ProcessAssimpNodeDetailed(
        scene->mRootNode,
        scene,
        options,
        actualBasePath,
        shader,
        identity,
        results
    );

    Logger::GetInstance().Info("Detailed model loading complete. Total meshes: " + std::to_string(results.size()));

    return results;
}

// ============================================================================
// MeshLoader 实现
// ============================================================================

namespace {

float SanitizePositive(const char* name, float value, float fallback = 1.0f) {
    if (value > std::numeric_limits<float>::epsilon()) {
        return value;
    }
    Logger::GetInstance().WarningFormat("[MeshLoader] %s must be > 0 (received %.3f). Using fallback %.3f.",
        name, value, fallback);
    return fallback;
}

float SanitizeNonNegative(const char* name, float value, float fallback = 0.0f) {
    if (value >= 0.0f) {
        return value;
    }
    Logger::GetInstance().WarningFormat("[MeshLoader] %s must be >= 0 (received %.3f). Using fallback %.3f.",
        name, value, fallback);
    return fallback;
}

uint32_t SanitizeSegments(const char* name, uint32_t value, uint32_t minValue, uint32_t fallback) {
    if (value >= minValue) {
        return value;
    }
    uint32_t clamped = std::max(minValue, fallback);
    Logger::GetInstance().WarningFormat("[MeshLoader] %s must be >= %u (received %u). Using %u.",
        name, minValue, value, clamped);
    return clamped;
}

} // namespace

Ref<Mesh> MeshLoader::CreatePlane(float width, float height, 
                                   uint32_t widthSegments, uint32_t heightSegments,
                                   const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    width = SanitizePositive("width", width);
    height = SanitizePositive("height", height);
    widthSegments = SanitizeSegments("widthSegments", widthSegments, 1u, 1u);
    heightSegments = SanitizeSegments("heightSegments", heightSegments, 1u, 1u);
    
    float halfWidth = width * 0.5f;
    float halfHeight = height * 0.5f;
    
    // 生成顶点
    for (uint32_t y = 0; y <= heightSegments; ++y) {
        for (uint32_t x = 0; x <= widthSegments; ++x) {
            float u = static_cast<float>(x) / widthSegments;
            float v = static_cast<float>(y) / heightSegments;
            
            Vertex vertex;
            vertex.position = Vector3(
                (u - 0.5f) * width,
                0.0f,
                (v - 0.5f) * height
            );
            vertex.texCoord = Vector2(u, v);
            vertex.normal = Vector3(0.0f, 1.0f, 0.0f);  // 向上
            vertex.color = color;
            
            vertices.push_back(vertex);
        }
    }
    
    // 生成索引
    for (uint32_t y = 0; y < heightSegments; ++y) {
        for (uint32_t x = 0; x < widthSegments; ++x) {
            uint32_t i0 = y * (widthSegments + 1) + x;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + widthSegments + 1;
            uint32_t i3 = i2 + 1;
            
            // 第一个三角形
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);
            
            // 第二个三角形
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }
    
    auto mesh = CreateRef<Mesh>(vertices, indices);
    mesh->RecalculateTangents();
    mesh->Upload();
    
    Logger::GetInstance().Info("Created plane mesh: " + std::to_string(vertices.size()) + " vertices");
    return mesh;
}

Ref<Mesh> MeshLoader::CreateCube(float width, float height, float depth, const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    width = SanitizePositive("width", width, 1.0f);
    height = SanitizePositive("height", height, 1.0f);
    depth = SanitizePositive("depth", depth, 1.0f);

    width = SanitizePositive("width", width, 1.0f);
    height = SanitizePositive("height", height, 1.0f);

    float hw = width * 0.5f;
    float hh = height * 0.5f;
    float hd = depth * 0.5f;
    
    // 每个面 4 个顶点，6 个面共 24 个顶点
    // 这样每个面可以有独立的法线和纹理坐标
    
    // 前面 (+Z)
    vertices.push_back(Vertex(Vector3(-hw, -hh,  hd), Vector2(0, 1), Vector3(0, 0, 1), color));
    vertices.push_back(Vertex(Vector3( hw, -hh,  hd), Vector2(1, 1), Vector3(0, 0, 1), color));
    vertices.push_back(Vertex(Vector3( hw,  hh,  hd), Vector2(1, 0), Vector3(0, 0, 1), color));
    vertices.push_back(Vertex(Vector3(-hw,  hh,  hd), Vector2(0, 0), Vector3(0, 0, 1), color));
    
    // 后面 (-Z)
    vertices.push_back(Vertex(Vector3( hw, -hh, -hd), Vector2(0, 1), Vector3(0, 0, -1), color));
    vertices.push_back(Vertex(Vector3(-hw, -hh, -hd), Vector2(1, 1), Vector3(0, 0, -1), color));
    vertices.push_back(Vertex(Vector3(-hw,  hh, -hd), Vector2(1, 0), Vector3(0, 0, -1), color));
    vertices.push_back(Vertex(Vector3( hw,  hh, -hd), Vector2(0, 0), Vector3(0, 0, -1), color));
    
    // 右面 (+X)
    vertices.push_back(Vertex(Vector3( hw, -hh,  hd), Vector2(0, 1), Vector3(1, 0, 0), color));
    vertices.push_back(Vertex(Vector3( hw, -hh, -hd), Vector2(1, 1), Vector3(1, 0, 0), color));
    vertices.push_back(Vertex(Vector3( hw,  hh, -hd), Vector2(1, 0), Vector3(1, 0, 0), color));
    vertices.push_back(Vertex(Vector3( hw,  hh,  hd), Vector2(0, 0), Vector3(1, 0, 0), color));
    
    // 左面 (-X)
    vertices.push_back(Vertex(Vector3(-hw, -hh, -hd), Vector2(0, 1), Vector3(-1, 0, 0), color));
    vertices.push_back(Vertex(Vector3(-hw, -hh,  hd), Vector2(1, 1), Vector3(-1, 0, 0), color));
    vertices.push_back(Vertex(Vector3(-hw,  hh,  hd), Vector2(1, 0), Vector3(-1, 0, 0), color));
    vertices.push_back(Vertex(Vector3(-hw,  hh, -hd), Vector2(0, 0), Vector3(-1, 0, 0), color));
    
    // 上面 (+Y)
    vertices.push_back(Vertex(Vector3(-hw,  hh,  hd), Vector2(0, 1), Vector3(0, 1, 0), color));
    vertices.push_back(Vertex(Vector3( hw,  hh,  hd), Vector2(1, 1), Vector3(0, 1, 0), color));
    vertices.push_back(Vertex(Vector3( hw,  hh, -hd), Vector2(1, 0), Vector3(0, 1, 0), color));
    vertices.push_back(Vertex(Vector3(-hw,  hh, -hd), Vector2(0, 0), Vector3(0, 1, 0), color));
    
    // 下面 (-Y)
    vertices.push_back(Vertex(Vector3(-hw, -hh, -hd), Vector2(0, 1), Vector3(0, -1, 0), color));
    vertices.push_back(Vertex(Vector3( hw, -hh, -hd), Vector2(1, 1), Vector3(0, -1, 0), color));
    vertices.push_back(Vertex(Vector3( hw, -hh,  hd), Vector2(1, 0), Vector3(0, -1, 0), color));
    vertices.push_back(Vertex(Vector3(-hw, -hh,  hd), Vector2(0, 0), Vector3(0, -1, 0), color));
    
    // 索引（每个面 2 个三角形）
    for (uint32_t i = 0; i < 6; ++i) {
        uint32_t base = i * 4;
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }
    
    auto mesh = CreateRef<Mesh>(vertices, indices);
    mesh->RecalculateTangents();
    mesh->Upload();
    
    Logger::GetInstance().Info("Created cube mesh: " + std::to_string(vertices.size()) + " vertices");
    return mesh;
}

Ref<Mesh> MeshLoader::CreateSphere(float radius, uint32_t segments, uint32_t rings, const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    radius = SanitizePositive("radius", radius, 0.5f);
    segments = SanitizeSegments("segments", segments, 3u, 32u);
    rings = SanitizeSegments("rings", rings, 2u, 16u);
    
    // 生成顶点
    for (uint32_t ring = 0; ring <= rings; ++ring) {
        float v = static_cast<float>(ring) / rings;
        float phi = v * static_cast<float>(M_PI);  // 0 到 π
        
        for (uint32_t seg = 0; seg <= segments; ++seg) {
            float u = static_cast<float>(seg) / segments;
            float theta = u * 2.0f * static_cast<float>(M_PI);  // 0 到 2π
            
            // 球面坐标转换为笛卡尔坐标
            float x = radius * std::sin(phi) * std::cos(theta);
            float y = radius * std::cos(phi);
            float z = radius * std::sin(phi) * std::sin(theta);
            
            Vertex vertex;
            vertex.position = Vector3(x, y, z);
            vertex.texCoord = Vector2(u, v);
            vertex.normal = Vector3(x, y, z).normalized();  // 球体法线即为归一化位置向量
            vertex.color = color;
            
            vertices.push_back(vertex);
        }
    }
    
    // 生成索引（逆时针为正面，从外向内看）
    for (uint32_t ring = 0; ring < rings; ++ring) {
        for (uint32_t seg = 0; seg < segments; ++seg) {
            uint32_t i0 = ring * (segments + 1) + seg;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + segments + 1;
            uint32_t i3 = i2 + 1;
            
            // 第一个三角形
            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i2);
            
            // 第二个三角形
            indices.push_back(i2);
            indices.push_back(i1);
            indices.push_back(i3);
        }
    }
    
    auto mesh = CreateRef<Mesh>(vertices, indices);
    mesh->RecalculateTangents();
    mesh->Upload();
    
    Logger::GetInstance().Info("Created sphere mesh: " + std::to_string(vertices.size()) + " vertices");
    return mesh;
}

Ref<Mesh> MeshLoader::CreateCylinder(float radiusTop, float radiusBottom, float height, 
                                      uint32_t segments, const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    radiusTop = SanitizeNonNegative("radiusTop", radiusTop, 0.5f);
    radiusBottom = SanitizeNonNegative("radiusBottom", radiusBottom, 0.5f);
    height = SanitizePositive("height", height, 1.0f);
    segments = SanitizeSegments("segments", segments, 3u, 32u);
    float halfHeight = height * 0.5f;
    
    // ========== 侧面 ==========
    // 为侧面生成顶点（每个高度层一圈顶点）
    for (uint32_t i = 0; i <= segments; ++i) {
        float u = static_cast<float>(i) / segments;
        float theta = u * 2.0f * static_cast<float>(M_PI);
        float cosTheta = std::cos(theta);
        float sinTheta = std::sin(theta);
        
        // 顶部顶点
        Vertex topVertex;
        topVertex.position = Vector3(radiusTop * cosTheta, halfHeight, radiusTop * sinTheta);
        topVertex.texCoord = Vector2(u, 0);
        topVertex.normal = Vector3(cosTheta, 0, sinTheta);
        topVertex.color = color;
        vertices.push_back(topVertex);
        
        // 底部顶点
        Vertex bottomVertex;
        bottomVertex.position = Vector3(radiusBottom * cosTheta, -halfHeight, radiusBottom * sinTheta);
        bottomVertex.texCoord = Vector2(u, 1);
        bottomVertex.normal = Vector3(cosTheta, 0, sinTheta);
        bottomVertex.color = color;
        vertices.push_back(bottomVertex);
    }
    
    // 侧面索引（从外部看，逆时针为正面）
    // 顶点布局：i0=top, i1=bottom, i2=next_top, i3=next_bottom
    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t i0 = i * 2;        // 当前圆周的顶部
        uint32_t i1 = i0 + 1;       // 当前圆周的底部
        uint32_t i2 = i0 + 2;       // 下一个圆周的顶部
        uint32_t i3 = i2 + 1;       // 下一个圆周的底部
        
        // 从外部看这个四边形，逆时针应该是：i0 → i2 → i1
        indices.push_back(i0);
        indices.push_back(i2);
        indices.push_back(i1);
        
        // 第二个三角形：i1 → i2 → i3
        indices.push_back(i1);
        indices.push_back(i2);
        indices.push_back(i3);
    }
    
    // ========== 顶面盖子 ==========
    uint32_t topCapStart = vertices.size();
    
    // 顶面中心点
    vertices.push_back(Vertex(Vector3(0, halfHeight, 0), Vector2(0.5f, 0.5f), Vector3(0, 1, 0), color));
    uint32_t topCenter = topCapStart;
    
    // 顶面圆周顶点
    for (uint32_t i = 0; i <= segments; ++i) {
        float u = static_cast<float>(i) / segments;
        float theta = u * 2.0f * static_cast<float>(M_PI);
        float cosTheta = std::cos(theta);
        float sinTheta = std::sin(theta);
        
        Vertex v;
        v.position = Vector3(radiusTop * cosTheta, halfHeight, radiusTop * sinTheta);
        v.texCoord = Vector2(cosTheta * 0.5f + 0.5f, sinTheta * 0.5f + 0.5f);
        v.normal = Vector3(0, 1, 0);
        v.color = color;
        vertices.push_back(v);
    }
    
    // 顶面索引（从上方看，顺时针，正面朝上）
    for (uint32_t i = 0; i < segments; ++i) {
        indices.push_back(topCenter);
        indices.push_back(topCapStart + 1 + i + 1);
        indices.push_back(topCapStart + 1 + i);
    }
    
    // ========== 底面盖子 ==========
    uint32_t bottomCapStart = vertices.size();
    
    // 底面中心点
    vertices.push_back(Vertex(Vector3(0, -halfHeight, 0), Vector2(0.5f, 0.5f), Vector3(0, -1, 0), color));
    uint32_t bottomCenter = bottomCapStart;
    
    // 底面圆周顶点
    for (uint32_t i = 0; i <= segments; ++i) {
        float u = static_cast<float>(i) / segments;
        float theta = u * 2.0f * static_cast<float>(M_PI);
        float cosTheta = std::cos(theta);
        float sinTheta = std::sin(theta);
        
        Vertex v;
        v.position = Vector3(radiusBottom * cosTheta, -halfHeight, radiusBottom * sinTheta);
        v.texCoord = Vector2(cosTheta * 0.5f + 0.5f, sinTheta * 0.5f + 0.5f);
        v.normal = Vector3(0, -1, 0);
        v.color = color;
        vertices.push_back(v);
    }
    
    // 底面索引（从下方看，顺时针，正面朝下；从上方看是逆时针）
    for (uint32_t i = 0; i < segments; ++i) {
        indices.push_back(bottomCenter);
        indices.push_back(bottomCapStart + 1 + i);
        indices.push_back(bottomCapStart + 1 + i + 1);
    }
    
    auto mesh = CreateRef<Mesh>(vertices, indices);
    mesh->RecalculateTangents();
    mesh->Upload();
    
    Logger::GetInstance().Info("Created cylinder mesh: " + std::to_string(vertices.size()) + " vertices");
    return mesh;
}

Ref<Mesh> MeshLoader::CreateCone(float radius, float height, uint32_t segments, const Color& color) {
    // 圆锥就是顶部半径为 0 的圆柱
    return CreateCylinder(0.0f, radius, height, segments, color);
}

Ref<Mesh> MeshLoader::CreateTorus(float majorRadius, float minorRadius, 
                                   uint32_t majorSegments, uint32_t minorSegments,
                                   const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    majorRadius = SanitizePositive("majorRadius", majorRadius, 1.0f);
    minorRadius = SanitizePositive("minorRadius", minorRadius, 0.3f);
    majorSegments = SanitizeSegments("majorSegments", majorSegments, 3u, 32u);
    minorSegments = SanitizeSegments("minorSegments", minorSegments, 3u, 16u);
    
    // 生成顶点
    for (uint32_t i = 0; i <= majorSegments; ++i) {
        float u = static_cast<float>(i) / majorSegments;
        float theta = u * 2.0f * static_cast<float>(M_PI);
        float cosTheta = std::cos(theta);
        float sinTheta = std::sin(theta);
        
        for (uint32_t j = 0; j <= minorSegments; ++j) {
            float v = static_cast<float>(j) / minorSegments;
            float phi = v * 2.0f * static_cast<float>(M_PI);
            float cosPhi = std::cos(phi);
            float sinPhi = std::sin(phi);
            
            // 计算位置
            float x = (majorRadius + minorRadius * cosPhi) * cosTheta;
            float y = minorRadius * sinPhi;
            float z = (majorRadius + minorRadius * cosPhi) * sinTheta;
            
            // 计算法线
            Vector3 center(majorRadius * cosTheta, 0, majorRadius * sinTheta);
            Vector3 pos(x, y, z);
            Vector3 normal = (pos - center).normalized();
            
            Vertex vertex;
            vertex.position = pos;
            vertex.texCoord = Vector2(u, v);
            vertex.normal = normal;
            vertex.color = color;
            
            vertices.push_back(vertex);
        }
    }
    
    // 生成索引（逆时针为正面）
    for (uint32_t i = 0; i < majorSegments; ++i) {
        for (uint32_t j = 0; j < minorSegments; ++j) {
            uint32_t i0 = i * (minorSegments + 1) + j;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + minorSegments + 1;
            uint32_t i3 = i2 + 1;
            
            // 第一个三角形
            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i2);
            
            // 第二个三角形
            indices.push_back(i2);
            indices.push_back(i1);
            indices.push_back(i3);
        }
    }
    
    auto mesh = CreateRef<Mesh>(vertices, indices);
    mesh->RecalculateTangents();
    mesh->Upload();
    
    Logger::GetInstance().Info("Created torus mesh: " + std::to_string(vertices.size()) + " vertices");
    return mesh;
}

Ref<Mesh> MeshLoader::CreateCapsule(float radius, float height, uint32_t segments, 
                                     uint32_t rings, const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    radius = SanitizePositive("radius", radius, 0.5f);
    height = SanitizeNonNegative("height", height, 1.0f);
    segments = SanitizeSegments("segments", segments, 3u, 32u);
    rings = SanitizeSegments("rings", rings, 1u, 8u);
    
    float halfHeight = height * 0.5f;
    
    // 顶部半球（从赤道到顶点）
    for (uint32_t ring = 0; ring <= rings; ++ring) {
        float v = static_cast<float>(ring) / rings;
        float phi = v * static_cast<float>(M_PI) * 0.5f;  // 0 到 π/2
        
        for (uint32_t seg = 0; seg <= segments; ++seg) {
            float u = static_cast<float>(seg) / segments;
            float theta = u * 2.0f * static_cast<float>(M_PI);
            
            float x = radius * std::cos(phi) * std::cos(theta);
            float y = radius * std::sin(phi) + halfHeight;
            float z = radius * std::cos(phi) * std::sin(theta);
            
            Vertex vertex;
            vertex.position = Vector3(x, y, z);
            vertex.texCoord = Vector2(u, v * 0.25f);
            vertex.normal = Vector3(x, y - halfHeight, z).normalized();
            vertex.color = color;
            
            vertices.push_back(vertex);
        }
    }
    
    // 中间圆柱部分（连接两个半球）
    uint32_t cylinderStart = vertices.size();
    for (uint32_t seg = 0; seg <= segments; ++seg) {
        float u = static_cast<float>(seg) / segments;
        float theta = u * 2.0f * static_cast<float>(M_PI);
        float cosTheta = std::cos(theta);
        float sinTheta = std::sin(theta);
        
        // 顶部圆周（在 +halfHeight）
        Vertex topVertex;
        topVertex.position = Vector3(radius * cosTheta, halfHeight, radius * sinTheta);
        topVertex.texCoord = Vector2(u, 0.25f);
        topVertex.normal = Vector3(cosTheta, 0, sinTheta);
        topVertex.color = color;
        vertices.push_back(topVertex);
        
        // 底部圆周（在 -halfHeight）
        Vertex bottomVertex;
        bottomVertex.position = Vector3(radius * cosTheta, -halfHeight, radius * sinTheta);
        bottomVertex.texCoord = Vector2(u, 0.75f);
        bottomVertex.normal = Vector3(cosTheta, 0, sinTheta);
        bottomVertex.color = color;
        vertices.push_back(bottomVertex);
    }
    
    // 底部半球（从赤道到底部顶点）
    uint32_t bottomStart = vertices.size();
    for (uint32_t ring = 0; ring <= rings; ++ring) {
        float v = static_cast<float>(ring) / rings;
        float phi = v * static_cast<float>(M_PI) * 0.5f;  // 0 到 π/2
        
        for (uint32_t seg = 0; seg <= segments; ++seg) {
            float u = static_cast<float>(seg) / segments;
            float theta = u * 2.0f * static_cast<float>(M_PI);
            
            float x = radius * std::cos(phi) * std::cos(theta);
            float y = -radius * std::sin(phi) - halfHeight;
            float z = radius * std::cos(phi) * std::sin(theta);
            
            Vertex vertex;
            vertex.position = Vector3(x, y, z);
            vertex.texCoord = Vector2(u, 0.75f + v * 0.25f);
            vertex.normal = Vector3(x, y + halfHeight, z).normalized();
            vertex.color = color;
            
            vertices.push_back(vertex);
        }
    }
    
    // 顶部半球索引（逆时针为正面，从外向内看）
    for (uint32_t ring = 0; ring < rings; ++ring) {
        for (uint32_t seg = 0; seg < segments; ++seg) {
            uint32_t i0 = ring * (segments + 1) + seg;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + segments + 1;
            uint32_t i3 = i2 + 1;
            
            // 第一个三角形（反转）
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);
            
            // 第二个三角形（反转）
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }
    
    // 中间圆柱部分索引（与圆柱侧面相同的卷绕顺序）
    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t i0 = cylinderStart + i * 2;       // 当前顶部
        uint32_t i1 = i0 + 1;                      // 当前底部
        uint32_t i2 = i0 + 2;                      // 下一个顶部
        uint32_t i3 = i2 + 1;                      // 下一个底部
        
        // 第一个三角形（反转）
        indices.push_back(i0);
        indices.push_back(i2);
        indices.push_back(i1);
        
        // 第二个三角形（反转）
        indices.push_back(i1);
        indices.push_back(i2);
        indices.push_back(i3);
    }
    
    // 底部半球索引（逆时针为正面）
    for (uint32_t ring = 0; ring < rings; ++ring) {
        for (uint32_t seg = 0; seg < segments; ++seg) {
            uint32_t i0 = bottomStart + ring * (segments + 1) + seg;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + segments + 1;
            uint32_t i3 = i2 + 1;
            
            // 第一个三角形
            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i2);
            
            // 第二个三角形
            indices.push_back(i2);
            indices.push_back(i1);
            indices.push_back(i3);
        }
    }
    
    auto mesh = CreateRef<Mesh>(vertices, indices);
    mesh->RecalculateTangents();
    mesh->Upload();
    
    Logger::GetInstance().Info("Created capsule mesh: " + std::to_string(vertices.size()) + " vertices");
    return mesh;
}

Ref<Mesh> MeshLoader::CreateQuad(float width, float height, const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    float hw = width * 0.5f;
    float hh = height * 0.5f;
    
    // 四个顶点（XY 平面，法线向 +Z）
    vertices.push_back(Vertex(Vector3(-hw, -hh, 0), Vector2(0, 0), Vector3(0, 0, 1), color));
    vertices.push_back(Vertex(Vector3( hw, -hh, 0), Vector2(1, 0), Vector3(0, 0, 1), color));
    vertices.push_back(Vertex(Vector3( hw,  hh, 0), Vector2(1, 1), Vector3(0, 0, 1), color));
    vertices.push_back(Vertex(Vector3(-hw,  hh, 0), Vector2(0, 1), Vector3(0, 0, 1), color));
    
    // 两个三角形
    indices = { 0, 1, 2, 0, 2, 3 };
    
    auto mesh = CreateRef<Mesh>(vertices, indices);
    mesh->RecalculateTangents();
    mesh->Upload();
    
    Logger::GetInstance().Info("Created quad mesh: 4 vertices");
    return mesh;
}

Ref<Mesh> MeshLoader::CreateTriangle(float size, const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    size = SanitizePositive("size", size, 1.0f);
    float h = size * 0.86602540378f;  // sqrt(3) / 2
    float halfSize = size * 0.5f;
    
    // 等边三角形（XY 平面，法线向 +Z）
    vertices.push_back(Vertex(Vector3(0, h * 0.5f, 0), Vector2(0.5f, 0), Vector3(0, 0, 1), color));
    vertices.push_back(Vertex(Vector3(-halfSize, -h * 0.5f, 0), Vector2(0, 1), Vector3(0, 0, 1), color));
    vertices.push_back(Vertex(Vector3(halfSize, -h * 0.5f, 0), Vector2(1, 1), Vector3(0, 0, 1), color));
    
    indices = { 0, 1, 2 };
    
    auto mesh = CreateRef<Mesh>(vertices, indices);
    mesh->RecalculateTangents();
    mesh->Upload();
    
    Logger::GetInstance().Info("Created triangle mesh: 3 vertices");
    return mesh;
}

Ref<Mesh> MeshLoader::CreateCircle(float radius, uint32_t segments, const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    radius = SanitizePositive("radius", radius, 0.5f);
    segments = SanitizeSegments("segments", segments, 3u, 32u);
    
    // 中心点
    vertices.push_back(Vertex(Vector3(0, 0, 0), Vector2(0.5f, 0.5f), Vector3(0, 0, 1), color));
    
    // 圆周顶点
    for (uint32_t i = 0; i <= segments; ++i) {
        float u = static_cast<float>(i) / segments;
        float theta = u * 2.0f * static_cast<float>(M_PI);
        float x = radius * std::cos(theta);
        float y = radius * std::sin(theta);
        
        Vertex vertex;
        vertex.position = Vector3(x, y, 0);
        vertex.texCoord = Vector2(x / radius * 0.5f + 0.5f, y / radius * 0.5f + 0.5f);
        vertex.normal = Vector3(0, 0, 1);
        vertex.color = color;
        
        vertices.push_back(vertex);
    }
    
    // 生成三角形扇形索引
    for (uint32_t i = 0; i < segments; ++i) {
        indices.push_back(0);
        indices.push_back(i + 1);
        indices.push_back(i + 2);
    }
    
    auto mesh = CreateRef<Mesh>(vertices, indices);
    mesh->RecalculateTangents();
    mesh->Upload();
    
    Logger::GetInstance().Info("Created circle mesh: " + std::to_string(vertices.size()) + " vertices");
    return mesh;
}

// ============================================================================
// 批量资源管理
// ============================================================================

size_t MeshLoader::BatchUpload(
    const std::vector<Ref<Mesh>>& meshes,
    size_t maxConcurrent,
    std::function<void(size_t current, size_t total, const Ref<Mesh>& mesh)> progressCallback)
{
    if (meshes.empty()) {
        Logger::GetInstance().Warning("MeshLoader::BatchUpload: 网格列表为空");
        return 0;
    }
    
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("批量上传网格: " + std::to_string(meshes.size()) + " 个");
    Logger::GetInstance().Info("最大并发数: " + std::to_string(maxConcurrent));
    Logger::GetInstance().Info("========================================");
    
    size_t uploadedCount = 0;
    size_t skippedCount = 0;
    size_t failedCount = 0;
    
    // 确保maxConcurrent至少为1
    if (maxConcurrent < 1) {
        maxConcurrent = 1;
    }
    
    // 分批上传
    for (size_t i = 0; i < meshes.size(); i += maxConcurrent) {
        size_t batchEnd = std::min(i + maxConcurrent, meshes.size());
        size_t batchSize = batchEnd - i;
        
        Logger::GetInstance().Debug("批次 " + std::to_string(i / maxConcurrent + 1) + 
                                    ": 上传 " + std::to_string(i) + "-" + 
                                    std::to_string(batchEnd - 1) + " (" + 
                                    std::to_string(batchSize) + " 个网格)");
        
        // 上传当前批次的网格
        for (size_t j = i; j < batchEnd; ++j) {
            const auto& mesh = meshes[j];
            
            if (!mesh) {
                Logger::GetInstance().Warning("MeshLoader::BatchUpload: 网格 " + 
                                             std::to_string(j) + " 为空，跳过");
                skippedCount++;
                continue;
            }
            
            try {
                // 检查是否已上传
                if (mesh->IsUploaded()) {
                    Logger::GetInstance().Debug("网格 " + std::to_string(j) + " 已上传，跳过");
                    skippedCount++;
                } else {
                    // 上传网格
                    mesh->Upload();
                    uploadedCount++;
                    
                    Logger::GetInstance().Debug("✅ 网格 " + std::to_string(j) + 
                                               " 上传成功 (" + 
                                               std::to_string(mesh->GetVertexCount()) + " 顶点)");
                }
                
                // 调用进度回调
                if (progressCallback) {
                    progressCallback(j + 1, meshes.size(), mesh);
                }
                
            } catch (const std::exception& e) {
                Logger::GetInstance().Error("网格 " + std::to_string(j) + 
                                           " 上传失败: " + std::string(e.what()));
                failedCount++;
                
                // 继续上传下一个（不中断）
            }
        }
        
        // 批次之间短暂休息，让OpenGL驱动有时间处理
        if (batchEnd < meshes.size() && batchSize > 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    // 输出统计信息
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("批量上传完成:");
    Logger::GetInstance().Info("  - 成功上传: " + std::to_string(uploadedCount) + " 个");
    Logger::GetInstance().Info("  - 已跳过: " + std::to_string(skippedCount) + " 个");
    if (failedCount > 0) {
        Logger::GetInstance().Warning("  - 上传失败: " + std::to_string(failedCount) + " 个");
    }
    Logger::GetInstance().Info("========================================");
    
    return uploadedCount;
}

} // namespace Render

