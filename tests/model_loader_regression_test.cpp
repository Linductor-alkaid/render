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
#include <render/model_loader.h>
#include <render/resource_manager.h>
#include <render/logger.h>

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Render;

namespace {

bool ValidateTangentSpace(const Ref<Mesh>& mesh) {
    if (!mesh) {
        std::cerr << "Mesh pointer is null" << std::endl;
        return false;
    }

    bool tangentsValid = true;
    mesh->AccessVertices([&](const std::vector<Vertex>& data) {
        const float epsilon = 1e-4f;
        for (size_t i = 0; i < data.size(); ++i) {
            const auto& v = data[i];
            if (v.tangent.squaredNorm() < epsilon || v.bitangent.squaredNorm() < epsilon) {
                std::cerr << "Vertex " << i << " has invalid tangent/bitangent" << std::endl;
                tangentsValid = false;
                break;
            }
        }
    });
    return tangentsValid;
}

} // namespace

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    Logger::GetInstance().SetLogToConsole(false);
    Logger::GetInstance().SetLogToFile(false);

    auto& resourceManager = ResourceManager::GetInstance();
    resourceManager.ClearType(ResourceType::Model);
    resourceManager.ClearType(ResourceType::Mesh);
    resourceManager.ClearType(ResourceType::Material);

    ModelLoadOptions options;
    options.autoUpload = false;
    options.registerModel = false;
    options.registerMeshes = false;
    options.registerMaterials = false;
    options.updateDependencyGraph = false;

    const std::filesystem::path projectRoot = std::filesystem::path(PROJECT_SOURCE_DIR);
    const std::filesystem::path modelPath = projectRoot / "models" / "cube.obj";
    const std::string sourceModel = modelPath.string();
    const std::string tempModelName = "unit_test_model_temp";

    auto loadResult = ModelLoader::LoadFromFile(sourceModel, tempModelName, options);
    if (!loadResult.model) {
        std::cerr << "Failed to load model: " << sourceModel << std::endl;
        return 1;
    }

    std::cout << "Loaded model: " << sourceModel << std::endl;

    bool tangentsOk = true;
    bool uploadStateOk = true;

    loadResult.model->AccessParts([&](const std::vector<ModelPart>& parts) {
        std::cout << "Model part count: " << parts.size() << std::endl;
        if (parts.empty()) {
            tangentsOk = false;
            std::cerr << "Model contains no parts" << std::endl;
            return;
        }
        for (size_t index = 0; index < parts.size(); ++index) {
            const auto& part = parts[index];
            std::cout << "Part " << index << " name=" << part.name << std::endl;
            if (!part.mesh) {
                std::cerr << "Model part has null mesh" << std::endl;
                if (part.extraData) {
                    std::cerr << "  extraData present: assimpMeshIndex=" << part.extraData->assimpMeshIndex << std::endl;
                }
                tangentsOk = false;
                continue;
            }
            if (!ValidateTangentSpace(part.mesh)) {
                tangentsOk = false;
            }
            UploadState state = part.mesh->GetUploadState();
            std::cout << "Mesh upload state: " << static_cast<int>(state) << std::endl;
            if (state != UploadState::NotUploaded) {
                uploadStateOk = false;
                std::cerr << "Mesh upload state expected NotUploaded" << std::endl;
            }
        }
    });

    if (!tangentsOk || !uploadStateOk) {
        return 1;
    }

    ModelLoadOptions registerOptions = options;
    registerOptions.registerModel = true;
    registerOptions.registerMeshes = true;
    registerOptions.registerMaterials = true;
    registerOptions.updateDependencyGraph = false;
    registerOptions.resourcePrefix = "unit_test";

    std::vector<std::string> meshNames;
    std::vector<std::string> materialNames;
    const std::string registeredModelName = "unit_test_registered_model";
    ModelLoader::RegisterResources(registeredModelName, loadResult.model, registerOptions, &meshNames, &materialNames);

    long modelRef = resourceManager.GetReferenceCount(ResourceType::Model, registeredModelName);
    std::cout << "Model ref count: " << modelRef << std::endl;
    if (modelRef <= 0) {
        std::cerr << "Model reference count invalid" << std::endl;
        return 1;
    }

    for (const auto& meshName : meshNames) {
        long count = resourceManager.GetReferenceCount(ResourceType::Mesh, meshName);
        std::cout << "Mesh ref count for " << meshName << ": " << count << std::endl;
        if (count <= 0) {
            std::cerr << "Mesh reference count invalid for " << meshName << std::endl;
            return 1;
        }
    }

    for (const auto& materialName : materialNames) {
        long count = resourceManager.GetReferenceCount(ResourceType::Material, materialName);
        std::cout << "Material ref count for " << materialName << ": " << count << std::endl;
        if (count <= 0) {
            std::cerr << "Material reference count invalid for " << materialName << std::endl;
            return 1;
        }
    }

    std::cout << "Model loader regression test passed." << std::endl;
    return 0;
}
