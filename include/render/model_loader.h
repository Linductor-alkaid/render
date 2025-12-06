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

#include "model.h"
#include "mesh_loader.h"
#include "file_utils.h"
#include "shader.h"

#include <string>
#include <vector>

namespace Render {

struct ModelLoadOptions {
    bool flipUVs = true;
    bool autoUpload = true;

    bool registerModel = true;
    bool registerMeshes = true;
    bool registerMaterials = true;
    bool updateDependencyGraph = true;

    std::string basePath;
    std::string resourcePrefix;

    Ref<Shader> shaderOverride = nullptr;
};

struct ModelLoadOutput {
    ModelPtr model;
    std::string modelName;
    std::vector<std::string> meshResourceNames;
    std::vector<std::string> materialResourceNames;

    bool IsValid() const { return static_cast<bool>(model); }
};

class ModelLoader {
public:
    static ModelLoadOutput LoadFromFile(const std::string& filepath,
                                        const std::string& modelName = "",
                                        const ModelLoadOptions& options = {});

    static void RegisterResources(const std::string& modelName,
                                  const ModelPtr& model,
                                  const ModelLoadOptions& options,
                                  std::vector<std::string>* outMeshNames = nullptr,
                                  std::vector<std::string>* outMaterialNames = nullptr);

private:
    static ModelLoadOutput BuildModel(const std::string& filepath,
                                      const std::string& modelName,
                                      const ModelLoadOptions& options);

    static ModelLoadOutput RegisterResourcesInternal(const std::string& modelName,
                                                     const ModelPtr& model,
                                                     const ModelLoadOptions& options,
                                                     std::vector<std::string>* meshNames,
                                                     std::vector<std::string>* materialNames);
};

} // namespace Render


