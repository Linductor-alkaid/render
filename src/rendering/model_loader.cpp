#include "render/model_loader.h"

#include "render/resource_manager.h"
#include "render/logger.h"

#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <atomic>

namespace Render {

namespace {

std::string ResolveModelName(const std::string& filepath, const std::string& explicitName) {
    if (!explicitName.empty()) {
        return explicitName;
    }

    std::string name = FileUtils::GetFileName(filepath);
    if (!name.empty()) {
        return name;
    }

    static std::atomic<uint64_t> counter{0};
    uint64_t value = counter.fetch_add(1, std::memory_order_relaxed);
    return "Model_" + std::to_string(value);
}

std::string SanitizePartName(const std::string& name, size_t index) {
    if (!name.empty()) {
        return name;
    }
    return "Part_" + std::to_string(index);
}

std::string GenerateUniqueName(const std::string& base,
                               const std::function<bool(const std::string&)>& existsFunc) {
    if (!existsFunc || !base.empty()) {
        if (!existsFunc || !existsFunc(base)) {
            return base;
        }
    }

    std::string sanitizedBase = base.empty() ? "Resource" : base;
    int suffix = 1;
    std::string candidate;
    do {
        candidate = sanitizedBase + "_" + std::to_string(suffix++);
    } while (existsFunc && existsFunc(candidate));
    return candidate;
}

void CopyParts(const ModelPtr& model, std::vector<ModelPart>& dst) {
    dst.clear();
    if (!model) {
        return;
    }

    model->AccessParts([&dst](const std::vector<ModelPart>& parts) {
        dst = parts;
    });
}

} // namespace

ModelLoadOutput ModelLoader::LoadFromFile(const std::string& filepath,
                                          const std::string& modelName,
                                          const ModelLoadOptions& options) {
    ModelLoadOutput output = BuildModel(filepath, modelName, options);
    if (!output.model) {
        return output;
    }

    if (options.registerModel || options.registerMeshes || options.registerMaterials) {
        ModelLoadOutput registered = RegisterResourcesInternal(
            output.modelName,
            output.model,
            options,
            nullptr,
            nullptr
        );

        output.meshResourceNames = std::move(registered.meshResourceNames);
        output.materialResourceNames = std::move(registered.materialResourceNames);
    }

    return output;
}

void ModelLoader::RegisterResources(const std::string& modelName,
                                    const ModelPtr& model,
                                    const ModelLoadOptions& options,
                                    std::vector<std::string>* outMeshNames,
                                    std::vector<std::string>* outMaterialNames) {
    RegisterResourcesInternal(
        modelName,
        model,
        options,
        outMeshNames,
        outMaterialNames
    );
}

ModelLoadOutput ModelLoader::BuildModel(const std::string& filepath,
                                        const std::string& modelName,
                                        const ModelLoadOptions& options) {
    ModelLoadOutput output;

    MeshImportOptions importOptions;
    importOptions.flipUVs = options.flipUVs;
    importOptions.autoUpload = options.autoUpload;
    importOptions.loadMaterials = true;

    auto importResults = MeshLoader::LoadDetailedFromFile(
        filepath,
        importOptions,
        options.basePath,
        options.shaderOverride
    );

    if (importResults.empty()) {
        Logger::GetInstance().Warning("ModelLoader: 未能从文件加载任何网格: " + filepath);
        return output;
    }

    std::string finalName = ResolveModelName(filepath, modelName);
    ModelPtr model = std::make_shared<Model>(finalName);
    model->SetSourcePath(filepath);

    std::vector<ModelPart> parts;
    parts.reserve(importResults.size());

    for (size_t i = 0; i < importResults.size(); ++i) {
        const auto& result = importResults[i];
        ModelPart part;
        part.name = SanitizePartName(result.name, i);
        part.mesh = result.mesh;
        part.material = result.material;
        part.localTransform = result.extra.localTransform;
        part.castShadows = true;
        part.receiveShadows = true;
        part.extraData = CreateRef<MeshExtraData>(result.extra);

        if (result.mesh) {
            part.localBounds = result.mesh->CalculateBounds();
        }

        parts.push_back(std::move(part));
    }

    model->SetParts(std::move(parts));

    output.model = model;
    output.modelName = finalName;

    return output;
}

ModelLoadOutput ModelLoader::RegisterResourcesInternal(const std::string& modelName,
                                                       const ModelPtr& model,
                                                       const ModelLoadOptions& options,
                                                       std::vector<std::string>* meshNames,
                                                       std::vector<std::string>* materialNames) {
    ModelLoadOutput output;
    output.model = model;
    output.modelName = modelName;

    if (!model) {
        return output;
    }

    std::vector<ModelPart> parts;
    CopyParts(model, parts);

    auto& resourceManager = ResourceManager::GetInstance();
    std::string prefix = options.resourcePrefix.empty() ? modelName : options.resourcePrefix;

    std::unordered_map<const Mesh*, std::string> meshNameMap;
    std::unordered_map<const Material*, std::string> materialNameMap;
    std::unordered_set<std::string> pendingMeshNames;
    std::unordered_set<std::string> pendingMaterialNames;
    std::vector<std::string> dependencyNames;

    auto uniqueMeshName = [&](const std::string& base) -> std::string {
        return GenerateUniqueName(
            base,
            [&](const std::string& candidate) {
                if (pendingMeshNames.find(candidate) != pendingMeshNames.end()) {
                    return true;
                }
                return resourceManager.HasMesh(candidate);
            }
        );
    };

    auto uniqueMaterialName = [&](const std::string& base) -> std::string {
        return GenerateUniqueName(
            base,
            [&](const std::string& candidate) {
                if (pendingMaterialNames.find(candidate) != pendingMaterialNames.end()) {
                    return true;
                }
                return resourceManager.HasMaterial(candidate);
            }
        );
    };

    for (size_t i = 0; i < parts.size(); ++i) {
        const auto& part = parts[i];
        std::string partName = SanitizePartName(part.name, i);

        if (options.registerMeshes && part.mesh) {
            auto meshPtr = part.mesh.get();
            auto it = meshNameMap.find(meshPtr);
            if (it == meshNameMap.end()) {
                std::string baseName = prefix.empty()
                    ? ("Mesh::" + partName)
                    : (prefix + "::Mesh::" + partName);
                std::string meshResourceName = uniqueMeshName(baseName);
                if (resourceManager.RegisterMesh(meshResourceName, part.mesh)) {
                    meshNameMap.emplace(meshPtr, meshResourceName);
                    pendingMeshNames.insert(meshResourceName);
                    output.meshResourceNames.push_back(meshResourceName);
                    dependencyNames.push_back(meshResourceName);
                } else {
                    Logger::GetInstance().Warning("ModelLoader: 注册网格失败（可能已存在）: " + meshResourceName);
                }
            } else {
                dependencyNames.push_back(it->second);
            }
        }

        if (options.registerMaterials && part.material) {
            auto materialPtr = part.material.get();
            auto it = materialNameMap.find(materialPtr);
            if (it == materialNameMap.end()) {
                std::string baseName = prefix.empty()
                    ? ("Material::" + partName)
                    : (prefix + "::Material::" + partName);
                std::string materialResourceName = uniqueMaterialName(baseName);
                if (resourceManager.RegisterMaterial(materialResourceName, part.material)) {
                    materialNameMap.emplace(materialPtr, materialResourceName);
                    pendingMaterialNames.insert(materialResourceName);
                    output.materialResourceNames.push_back(materialResourceName);
                    dependencyNames.push_back(materialResourceName);
                } else {
                    Logger::GetInstance().Warning("ModelLoader: 注册材质失败（可能已存在）: " + materialResourceName);
                }
            } else {
                dependencyNames.push_back(it->second);
            }
        }
    }

    if (meshNames) {
        *meshNames = output.meshResourceNames;
    }
    if (materialNames) {
        *materialNames = output.materialResourceNames;
    }

    if (options.registerModel) {
        if (!resourceManager.RegisterModel(modelName, model)) {
            Logger::GetInstance().Warning("ModelLoader: 模型名称已存在，未覆盖: " + modelName);
        }
    }

    if (options.updateDependencyGraph) {
        if (resourceManager.HasModel(modelName)) {
            resourceManager.UpdateResourceDependencies(modelName, dependencyNames);
        }
    }

    return output;
}

} // namespace Render


