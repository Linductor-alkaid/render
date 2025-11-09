# GeometryPreset API 参考

[返回 API 首页](README.md)

---

## 概述

`GeometryPreset` 提供一套可复用的预定义几何体注册与获取接口，简化常见基础网格（立方体、球体、圆柱等）的构建流程。它在引擎启动阶段与 `ResourceManager` 协作，将预设网格按需生成并注册，避免重复创建和内存浪费，方便示例、调试工具与编辑器快速引用。

**头文件**: `render/geometry_preset.h`  
**命名空间**: `Render`

---

## 功能特性

- ✅ **一次注册，全局复用**：通过 `RegisterDefaults()` 将常用几何体生成后缓存到 `ResourceManager`，后续直接按名称获取。  
- ✅ **统一命名规范**：内置名称形如 `geometry::cube`、`geometry::sphere`，便于脚本或配置文件引用。  
- ✅ **线程安全**：内部使用 `std::call_once` 与 `ResourceManager` 的锁保护，保证多线程环境下只会注册一次。  
- ✅ **MeshLoader 集成**：所有预设均由 `MeshLoader` 对应的 `Create*` 方法生成，自动带有法线、切线、纹理坐标。  
- ✅ **可扩展**：支持自定义工厂函数注册新预设形状（例如自定义地形、对齐工具网格）。

---

## 公开接口

| 函数 | 说明 |
| --- | --- |
| `static const std::unordered_map<std::string, PresetInfo>& GetPresetMap()` | 返回内部注册表，用于调试或遍历预设名称。 |
| `static bool HasPreset(const std::string& name)` | 判断指定名称的预设是否存在。 |
| `static Ref<Mesh> GetMesh(ResourceManager& resourceManager, const std::string& name)` | 获取或创建预设网格；若 `ResourceManager` 尚未有缓存，会通过工厂函数生成后注册。 |
| `static void RegisterDefaults(ResourceManager& resourceManager)` | 注册引擎内置的所有预设网格，通常在引擎启动或首帧调用。 |

### PresetInfo 结构

```cpp
struct PresetInfo {
    std::string name;
    std::function<Ref<Mesh>()> factory;
};
```

- `name`：预设名称（与字典键一致）。  
- `factory`：产生 `Mesh` 的工厂函数，一般包装 `MeshLoader::Create*`。

---

## 默认预设列表

`RegisterDefaults()` 默认注册以下网格（均位于 `geometry::` 命名空间下）：

- `cube`、`sphere`、`cylinder`、`cone`、`torus`、`capsule`  
- `quad_xy`（位于 XY 平面）、`triangle`、`circle`  

所有形状都会生成包含位置、法线、切线/副切线、UV 的顶点数据，可直接用于 PBR/法线贴图等渲染流程。

---

## ResourceManager 协同

- `ResourceManager::RegisterDefaultGeometry()` 内部调用 `GeometryPreset::RegisterDefaults()`，并通过 `std::call_once` 确保只执行一次。  
- 当 `GetMesh()` 检测到目标名称尚未注册时，会自动：  
  1. 执行对应工厂函数生成 `Mesh`；  
  2. 调用 `ResourceManager::RegisterMesh()` 缓存；  
  3. 返回注册后的 `Ref<Mesh>`。  
- 若网格已存在，则直接返回缓存副本，不重复生成。

> 建议在程序初始化或首帧调用 `ResourceManager::RegisterDefaultGeometry()`，确保所有预设对 ECS/示例立即可用。

---

## 使用示例

```cpp
#include <render/resource_manager.h>
#include <render/geometry_preset.h>

auto& resourceManager = ResourceManager::GetInstance();
resourceManager.RegisterDefaultGeometry();

auto cubeMesh = GeometryPreset::GetMesh(resourceManager, "geometry::cube");
if (!cubeMesh) {
    Logger::GetInstance().Error("无法取得立方体预设网格");
    return;
}

// 构建材质并注册
auto material = CreateRef<Material>();
material->SetName("geometry::cube_mat");
material->SetDiffuseColor(Color(0.8f, 0.4f, 0.3f, 1.0f));
resourceManager.RegisterMaterial(material->GetName(), material);

// 在 MeshRenderComponent 中使用
MeshRenderComponent meshComp;
meshComp.meshName = "geometry::cube";
meshComp.materialName = material->GetName();
meshComp.mesh = cubeMesh;
meshComp.material = material;
meshComp.resourcesLoaded = true;
```

---

## 常见问题 (FAQ)

**Q1: 如果项目需要自定义预设形状怎么办？**  
A: 可以调用 `GeometryPreset::GetPresetMap()` 并插入新的 `PresetInfo`，或扩展 `RegisterDefaults()` 增加自定义工厂函数。确保生成的网格包含必要的顶点属性以支持现有着色器。

**Q2: 注册时遇到名称冲突会怎样？**  
A: 当 `ResourceManager::RegisterMesh()` 检测到同名网格已存在时会返回 false，并记录日志；`GeometryPreset` 会保持原资源不被覆盖。

**Q3: 是否可以跳过自动注册流程？**  
A: 可以直接调用 `GetMesh()`；若预设未注册，它会内部创建并注册，仅当项目打开场景时再生成需要的网格。

---

[上一篇: ModelLoader](ModelLoader.md) | [下一篇: ResourceManager](ResourceManager.md) | [返回 API 首页](README.md)

