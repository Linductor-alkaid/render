#include "render/mesh_loader.h"
#include "render/material.h"
#include "render/texture_loader.h"
#include "render/logger.h"
#include <cmath>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <filesystem>
#include <thread>
#include <chrono>
#include <functional>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Render {

// ============================================================================
// 辅助函数 - Assimp 网格处理
// ============================================================================

/**
 * @brief 处理单个 Assimp 网格并转换为引擎网格对象
 */
static Ref<Mesh> ProcessAssimpMesh(aiMesh* assimpMesh, const aiScene* scene, bool autoUpload = true) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    // 提取顶点数据
    vertices.reserve(assimpMesh->mNumVertices);
    for (uint32_t i = 0; i < assimpMesh->mNumVertices; i++) {
        Vertex vertex;
        
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
        
        vertices.push_back(vertex);
    }
    
    // 提取索引数据
    indices.reserve(assimpMesh->mNumFaces * 3);
    for (uint32_t i = 0; i < assimpMesh->mNumFaces; i++) {
        aiFace face = assimpMesh->mFaces[i];
        for (uint32_t j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }
    
    // 创建网格
    auto mesh = CreateRef<Mesh>(vertices, indices);
    
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
    const std::string& textureName)
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
    uint32_t materialIndex)
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
    
    auto diffuseMap = LoadMaterialTexture(aiMat, aiTextureType_DIFFUSE, basePath, 
                                          diffuseTexName.empty() ? (matNameStr + "_diffuse") : diffuseTexName);
    if (diffuseMap) {
        material->SetTexture("diffuseMap", diffuseMap);
    }
    
    // 镜面反射贴图
    auto specularMap = LoadMaterialTexture(aiMat, aiTextureType_SPECULAR, basePath,
                                           matNameStr + "_specular");
    if (specularMap) {
        material->SetTexture("specularMap", specularMap);
    }
    
    // 法线贴图
    auto normalMap = LoadMaterialTexture(aiMat, aiTextureType_NORMALS, basePath,
                                         matNameStr + "_normal");
    if (!normalMap) {
        // 有些格式使用 HEIGHT 代替 NORMALS
        normalMap = LoadMaterialTexture(aiMat, aiTextureType_HEIGHT, basePath,
                                       matNameStr + "_normal");
    }
    if (normalMap) {
        material->SetTexture("normalMap", normalMap);
    }
    
    // 环境遮蔽贴图
    auto aoMap = LoadMaterialTexture(aiMat, aiTextureType_AMBIENT_OCCLUSION, basePath,
                                     matNameStr + "_ao");
    if (aoMap) {
        material->SetTexture("aoMap", aoMap);
    }
    
    // 自发光贴图
    auto emissiveMap = LoadMaterialTexture(aiMat, aiTextureType_EMISSIVE, basePath,
                                           matNameStr + "_emissive");
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
        
        // 处理网格（材质加载始终自动上传）
        auto mesh = ProcessAssimpMesh(assimpMesh, scene, true);
        
        // 处理材质（如果有）
        Ref<Material> material = nullptr;
        if (assimpMesh->mMaterialIndex >= 0 && 
            assimpMesh->mMaterialIndex < scene->mNumMaterials) {
            aiMaterial* aiMat = scene->mMaterials[assimpMesh->mMaterialIndex];
            material = ProcessAssimpMaterial(aiMat, scene, basePath, shader, 
                                             assimpMesh->mMaterialIndex);
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
    std::string actualBasePath = basePath;
    if (actualBasePath.empty()) {
        // 使用模型文件所在目录作为基础路径（简化处理）
        size_t lastSlash = filepath.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            actualBasePath = filepath.substr(0, lastSlash);
        } else {
            actualBasePath = ".";
        }
    }
    
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

// ============================================================================
// MeshLoader 实现
// ============================================================================

Ref<Mesh> MeshLoader::CreatePlane(float width, float height, 
                                   uint32_t widthSegments, uint32_t heightSegments,
                                   const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    // 确保至少有 1 个分段
    widthSegments = std::max(1u, widthSegments);
    heightSegments = std::max(1u, heightSegments);
    
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
    mesh->Upload();
    
    Logger::GetInstance().Info("Created plane mesh: " + std::to_string(vertices.size()) + " vertices");
    return mesh;
}

Ref<Mesh> MeshLoader::CreateCube(float width, float height, float depth, const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
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
    mesh->Upload();
    
    Logger::GetInstance().Info("Created cube mesh: " + std::to_string(vertices.size()) + " vertices");
    return mesh;
}

Ref<Mesh> MeshLoader::CreateSphere(float radius, uint32_t segments, uint32_t rings, const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    segments = std::max(3u, segments);
    rings = std::max(2u, rings);
    
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
    mesh->Upload();
    
    Logger::GetInstance().Info("Created sphere mesh: " + std::to_string(vertices.size()) + " vertices");
    return mesh;
}

Ref<Mesh> MeshLoader::CreateCylinder(float radiusTop, float radiusBottom, float height, 
                                      uint32_t segments, const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    segments = std::max(3u, segments);
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
    
    majorSegments = std::max(3u, majorSegments);
    minorSegments = std::max(3u, minorSegments);
    
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
    mesh->Upload();
    
    Logger::GetInstance().Info("Created torus mesh: " + std::to_string(vertices.size()) + " vertices");
    return mesh;
}

Ref<Mesh> MeshLoader::CreateCapsule(float radius, float height, uint32_t segments, 
                                     uint32_t rings, const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    segments = std::max(3u, segments);
    rings = std::max(1u, rings);
    
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
    vertices.push_back(Vertex(Vector3(-hw, -hh, 0), Vector2(0, 1), Vector3(0, 0, 1), color));
    vertices.push_back(Vertex(Vector3( hw, -hh, 0), Vector2(1, 1), Vector3(0, 0, 1), color));
    vertices.push_back(Vertex(Vector3( hw,  hh, 0), Vector2(1, 0), Vector3(0, 0, 1), color));
    vertices.push_back(Vertex(Vector3(-hw,  hh, 0), Vector2(0, 0), Vector3(0, 0, 1), color));
    
    // 两个三角形
    indices = { 0, 1, 2, 0, 2, 3 };
    
    auto mesh = CreateRef<Mesh>(vertices, indices);
    mesh->Upload();
    
    Logger::GetInstance().Info("Created quad mesh: 4 vertices");
    return mesh;
}

Ref<Mesh> MeshLoader::CreateTriangle(float size, const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    float h = size * 0.866f;  // sqrt(3) / 2
    float halfSize = size * 0.5f;
    
    // 等边三角形（XY 平面，法线向 +Z）
    vertices.push_back(Vertex(Vector3(0, h * 0.5f, 0), Vector2(0.5f, 0), Vector3(0, 0, 1), color));
    vertices.push_back(Vertex(Vector3(-halfSize, -h * 0.5f, 0), Vector2(0, 1), Vector3(0, 0, 1), color));
    vertices.push_back(Vertex(Vector3(halfSize, -h * 0.5f, 0), Vector2(1, 1), Vector3(0, 0, 1), color));
    
    indices = { 0, 1, 2 };
    
    auto mesh = CreateRef<Mesh>(vertices, indices);
    mesh->Upload();
    
    Logger::GetInstance().Info("Created triangle mesh: 3 vertices");
    return mesh;
}

Ref<Mesh> MeshLoader::CreateCircle(float radius, uint32_t segments, const Color& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    segments = std::max(3u, segments);
    
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

