# ModelLoader API 参考

[返回 API 首页](README.md)

---

## 概述

`ModelLoader` 提供高层模型导入与资源注册流程：基于 `MeshLoader::LoadDetailedFromFile()` 解析多网格模型，构建 `Model` 对象，并可按需将模型、网格、材质统一注册到 `ResourceManager`。同时支持与 `AsyncResourceLoader` 协作，实现后台解析 + 主线程上传的异步加载。

**头文件**: `render/model_loader.h`  
**命名空间**: `Render`

---

## 功能特性

- ✅ **多部件模型构建**：一次性输出 `ModelPtr`，包含局部变换、包围盒与 `MeshExtraData`。  
- ✅ **自动资源注册**：可选择将模型/网格/材质统一区分命名后写入 `ResourceManager`。  
- ✅ **依赖跟踪**：注册时自动更新 `ResourceDependencyTracker`，便于可视化与清理。  
- ✅ **自定义命名策略**：支持资源前缀、模型显式命名。  
- ✅ **延迟上传兼容**：可生成未上传的网格，供异步流程在主线程统一 `Upload()`。  
- ✅ **与 AsyncResourceLoader 集成**：在后台线程解析模型，在主线程上传/注册资源。  

---

## 结构概览

### ModelLoadOptions

```cpp
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
```

**关键字段说明**:
- `flipUVs`：是否在导入阶段翻转 UV（OpenGL 约定）。  
- `autoUpload`：若为 `true`，在主线程阶段自动调用 `mesh->Upload()`；设为 `false` 可实现延迟上传。  
- `register*`：控制是否向 `ResourceManager` 注册对应资源。  
- `resourcePrefix`：注册资源名称的统一前缀，如 `"scene01" → scene01::Mesh::Body"`。  
- `basePath`：纹理搜索路径，默认使用模型文件所在目录。  
- `shaderOverride`：导入材质时强制指定着色器。  

### ModelLoadOutput

```cpp
struct ModelLoadOutput {
    ModelPtr model;
    std::string modelName;
    std::vector<std::string> meshResourceNames;
    std::vector<std::string> materialResourceNames;

    bool IsValid() const;
};
```

---

## 核心 API

### LoadFromFile

```cpp
static ModelLoadOutput LoadFromFile(
    const std::string& filepath,
    const std::string& modelName = "",
    const ModelLoadOptions& options = {}
);
```

**流程**:
1. 使用 `MeshLoader::LoadDetailedFromFile()` 导入所有网格与材质。  
2. 组装 `ModelPart`（含 `MeshExtraData`、局部变换、包围盒）。  
3. 根据 `options.autoUpload` 决定是否直接上传网格。  
4. 根据 `options.register*` 选择是否写入资源管理器、更新依赖关系。  

**返回**: `ModelLoadOutput`  
- `model`：成功时返回 `ModelPtr`（失败为 `nullptr`）。  
- `modelName`：模型名称，默认使用文件名（不含扩展名）。  
- `meshResourceNames` / `materialResourceNames`：注册成功的资源名称列表。  

### RegisterResources

```cpp
static void RegisterResources(
    const std::string& modelName,
    const ModelPtr& model,
    const ModelLoadOptions& options,
    std::vector<std::string>* outMeshNames = nullptr,
    std::vector<std::string>* outMaterialNames = nullptr
);
```

独立的资源注册接口，可用于：
- 在异步流程中：后台解析模型 → 主线程手动上传 → 调用此函数注册。  
- 在自定义构建 `Model` 后：统一纳入资源管理器。  

命名策略与 `LoadFromFile()` 完全一致，将根据 `resourcePrefix` + 部件 `name` 生成唯一标识。若目标名称已存在，保持原资源不被覆盖并记录日志。

---

## 异步加载工作流

搭配 `AsyncResourceLoader::LoadModelAsync()` 可实现：

1. **工作线程**：调用 `ModelLoader::LoadFromFile()`，`autoUpload=false`，不注册资源。  
2. **主线程**：在 `ModelLoadTask::ExecuteUpload()` 中上传所有网格并调用 `RegisterResources()`。  
3. **回调**：`ModelLoadResult` 返回 `ModelPtr` 与注册的资源名，方便 ECS 组件或场景管理器引用。  

示例：

```cpp
ModelLoadOptions options;
options.autoUpload = true;
options.resourcePrefix = "level01";

auto task = AsyncResourceLoader::GetInstance().LoadModelAsync(
    "models/level01.glb",
    "level01",
    options,
    [](const ModelLoadResult& result) {
        if (!result.IsSuccess()) {
            Logger::GetInstance().Error("模型加载失败: " + result.errorMessage);
            return;
        }

        Logger::GetInstance().InfoFormat(
            "模型 %s 加载完成，注册网格 %zu 个",
            result.name.c_str(),
            result.meshResourceNames.size()
        );

        auto model = result.resource;
        // 将模型提交给场景或 ECS 系统
    }
);
```

记得在渲染循环主线程调用 `AsyncResourceLoader::ProcessCompletedTasks()`，触发上传与回调。

---

## 命名与依赖策略

- 资源命名格式默认遵循：  
  - 网格：`{prefix}::Mesh::{partName}`  
  - 材质：`{prefix}::Material::{partName}`  
  - 模型：显式传入 `modelName` 或使用文件名。  
- `prefix` 默认为 `modelName`，可通过 `resourcePrefix` 覆盖。  
- 注册后 `ResourceDependencyTracker` 会记录模型 → 网格/材质的依赖，用于图形化展示和自动清理。  

---

## 常见问题 (FAQ)

- **Q: 如何获取导入时的骨骼、UV 等数据？**  
  A: 通过 `ModelPart::extraData` 访问 `MeshExtraData`，其中包含骨骼权重 (`MeshSkinningData`)、多 UV 通道、顶点颜色等信息。

- **Q: 可以只注册模型，不注册网格/材质吗？**  
  A: 可以，设置 `registerMeshes=false` 或 `registerMaterials=false` 即可；此时 `Model` 仍然保持对原始资源的引用。

- **Q: 如果资源名称冲突会怎样？**  
  A: `ModelLoader` 会自动追加后缀确保唯一性，并在日志中提示冲突情况。

- **Q: 如何实现纯解析，不上传/不注册？**  
  A: 调用 `LoadFromFile()` 时设置 `autoUpload=false` 且 `register*` 全为 `false`，即可获得纯数据结果，便于自定义流程处理。

---

[上一篇: ModelRenderer](ModelRenderer.md) | [下一篇: ResourceManager](ResourceManager.md) | [返回 API 首页](README.md)


