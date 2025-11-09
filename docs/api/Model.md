# Model API 参考

[返回 API 首页](README.md)

---

## 概述

`Model` 用于描述由多个网格(`Mesh`)与材质(`Material`)组成的组合模型，支持渐进式资源加载、线程安全访问以及统计信息查询。模型中的每个子部件(`ModelPart`)都会记录局部变换、包围盒、阴影标记以及导入时采集到的额外数据（`MeshExtraData`），方便后续渲染管线、动画系统和调试工具复用。

**头文件**: `render/model.h`  
**命名空间**: `Render`

### ✨ 功能特性

- ✅ **线程安全**：所有公共方法均使用读写锁保护，可跨线程读取/修改部件。  
- ✅ **多部件管理**：统一维护网格、材质、局部变换、阴影属性。  
- ✅ **包围盒缓存**：按需懒计算并自动缓存世界包围盒。  
- ✅ **统计信息**：快速获取网格、材质数量及顶点/索引统计。  
- ✅ **蒙皮/额外数据透传**：通过 `ModelPart::extraData` 提供导入阶段收集的 UV、颜色、骨骼等信息。  
- ✅ **ResourceManager 集成**：配合 `ModelLoader` 可直接注册到资源管理系统，并自动建立依赖关系。  

---

## 线程安全

- 所有公共接口均使用 `std::shared_mutex` 保证线程安全。  
- **读操作**(`AccessParts`, `GetBounds`, `GetStatistics` 等) 可在多个线程并发调用。  
- **写操作**(`SetParts`, `AddPart`, `ModifyParts`) 自动获取独占锁，确保一致性。  
- OpenGL 调用不在 `Model` 内部执行，网格上传需在拥有上下文的线程单独完成。

---

## 数据结构

### ModelPart

```cpp
struct ModelPart {
    std::string name;
    Ref<Mesh> mesh;
    Ref<Material> material;
    Matrix4 localTransform;
    AABB localBounds;
    bool castShadows = true;
    bool receiveShadows = true;
    Ref<MeshExtraData> extraData;

    bool HasSkinning() const;
    const MeshSkinningData* GetSkinningData() const;
};
```

**说明**:
- `localTransform`：Assimp 等导入器提供的节点局部变换，可与 `Transform` 组件组合。
- `localBounds`：部件局部空间包围盒，`Model` 会在需要时转换到世界空间。
- `extraData`：包含多 UV 通道、顶点颜色、骨骼权重、原始 Assimp 索引等导入信息；为空表示无额外数据。
- `HasSkinning()` / `GetSkinningData()`：便于快速判断模型是否携带蒙皮数据。

### ModelStatistics

```cpp
struct ModelStatistics {
    size_t meshCount = 0;
    size_t materialCount = 0;
    size_t vertexCount = 0;
    size_t indexCount = 0;

    bool IsEmpty() const;
};
```

用于快速掌握模型复杂度，可在调试界面或性能分析中直接展示。

---

## 主要成员函数

### 基础信息

| 方法 | 说明 |
| --- | --- |
| `SetName(const std::string&)` / `GetName()` | 设置/获取模型名称（默认来自文件名）。 |
| `SetSourcePath(const std::string&)` / `GetSourcePath()` | 记录模型来源路径，便于热重载与日志输出。 |

### 部件管理

| 方法 | 说明 |
| --- | --- |
| `SetParts(std::vector<ModelPart>)` | 批量替换部件，自动刷新统计信息与包围盒。 |
| `AddPart(const ModelPart&)` | 追加单个部件。 |
| `ClearParts()` | 移除所有部件。 |
| `GetPartCount()` / `IsEmpty()` | 查询部件数量与空模型状态。 |
| `AccessParts(Func&&)` | 以读锁方式访问全部部件，常用于遍历或统计。 |
| `ModifyParts(Func&&)` | 在写锁保护下修改部件，可安全执行批量操作。 |

### 包围盒与统计

| 方法 | 说明 |
| --- | --- |
| `GetBounds()` | 返回世界空间包围盒；内部按需懒计算，并在部件更新后自动失效。 |
| `RecalculateBounds()` | 强制重算包围盒，可在大规模修改后手动触发。 |
| `GetStatistics()` | 返回 `ModelStatistics` 副本。 |
| `AreAllMeshesUploaded()` | 检查所有部件网格是否已上传至 GPU。 |
| `HasSkinning()` | 快速检测模型是否包含任意蒙皮骨骼数据。 |

---

## 与 ModelLoader 的协同

- 推荐使用 `ModelLoader::LoadFromFile()` 创建 `Model`，自动填充 `ModelPart::extraData` 等信息。
- `ModelLoader::RegisterResources()` 可将模型及其部件注册到 `ResourceManager`，并建立依赖关系（便于自动清理、引用统计）。
- 配合 `AsyncResourceLoader::LoadModelAsync()` 可实现后台解析 + 主线程上传的异步加载流程。

---

## 使用示例

```cpp
#include <render/model_loader.h>
#include <render/async_resource_loader.h>

ModelLoadOptions options;
options.autoUpload = false;          // 异步加载，主线程再上传
options.registerModel = true;        // 自动注册到 ResourceManager
options.resourcePrefix = "scene01";  // 统一命名前缀

auto result = ModelLoader::LoadFromFile("models/castle.glb", "castle", options);
if (!result.model) {
    Logger::GetInstance().Error("模型加载失败");
    return;
}

// 在主线程上传 GPU 资源
result.model->AccessParts([](const std::vector<ModelPart>& parts) {
    for (const auto& part : parts) {
        if (part.mesh && !part.mesh->IsUploaded()) {
            part.mesh->Upload();
        }
    }
});

// 查询统计信息
auto stats = result.model->GetStatistics();
Logger::GetInstance().InfoFormat("模型包含 %zu 个网格，%zu 个材质，共计 %zu 个顶点",
                                 stats.meshCount, stats.materialCount, stats.vertexCount);
```

---

## 与 ResourceManager 的关系

- 模型注册后可使用 `ResourceManager::GetModel("castle")` 取得 `ModelPtr`。
- `ResourceManager::ListModels()` 列出所有已注册模型名。
- 依赖跟踪（`ResourceDependencyTracker`）会记录模型依赖的网格与材质资源，便于自动清理与可视化。

---

## 常见问题 (FAQ)

**Q1: `extraData` 什么时候为 `nullptr`？**  
A: 当导入器未收集到额外数据（例如纯静态网格）时返回空；可在使用前检查是否存在。

**Q2: 如何与蒙皮动画系统集成？**  
A: 通过 `ModelPart::HasSkinning()` 判断是否存在骨骼，然后从 `extraData->skinning` 读取 `MeshSkinningData` 并生成骨骼组件。

**Q3: 修改部件后需要手动刷新包围盒吗？**  
A: 使用 `ModifyParts()` 修改部件会自动标记包围盒为 dirty，`GetBounds()` 会在下次调用时重算，无需手动介入。

---

[上一篇: MeshLoader](MeshLoader.md) | [下一篇: ModelRenderer](ModelRenderer.md) | [返回 API 首页](README.md)


