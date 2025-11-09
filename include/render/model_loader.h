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


