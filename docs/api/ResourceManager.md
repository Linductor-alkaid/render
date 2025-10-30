# ResourceManager API 参考

[返回 API 首页](README.md)

---

## 概述

`ResourceManager` 是一个单例类，提供统一的资源管理接口，用于管理所有渲染资源（纹理、网格、材质、着色器）。它提供注册、获取、释放、统计等功能，确保资源的生命周期管理和线程安全。

**头文件**: `render/resource_manager.h`  
**命名空间**: `Render`  
**设计模式**: 单例（Singleton）

### 核心特性

✅ **统一管理**: 统一管理纹理、网格、材质、着色器四种资源类型  
✅ **线程安全**: 所有公共方法使用互斥锁保护，支持多线程访问  
✅ **引用计数**: 基于 `std::shared_ptr` 的自动引用计数管理  
✅ **智能清理**: 基于帧追踪和两阶段清理策略，安全清理未使用资源  
✅ **访问追踪**: 自动追踪资源访问帧，防止意外删除活跃资源  
✅ **资源统计**: 提供详细的资源统计和监控功能  
✅ **批量操作**: 支持批量清理和遍历操作  

---

## 类定义

```cpp
class ResourceManager {
public:
    static ResourceManager& GetInstance();
    
    // 纹理管理
    bool RegisterTexture(const std::string& name, Ref<Texture> texture);
    Ref<Texture> GetTexture(const std::string& name);
    bool RemoveTexture(const std::string& name);
    bool HasTexture(const std::string& name) const;
    
    // 网格管理
    bool RegisterMesh(const std::string& name, Ref<Mesh> mesh);
    Ref<Mesh> GetMesh(const std::string& name);
    bool RemoveMesh(const std::string& name);
    bool HasMesh(const std::string& name) const;
    
    // 材质管理
    bool RegisterMaterial(const std::string& name, Ref<Material> material);
    Ref<Material> GetMaterial(const std::string& name);
    bool RemoveMaterial(const std::string& name);
    bool HasMaterial(const std::string& name) const;
    
    // 着色器管理
    bool RegisterShader(const std::string& name, Ref<Shader> shader);
    Ref<Shader> GetShader(const std::string& name);
    bool RemoveShader(const std::string& name);
    bool HasShader(const std::string& name) const;
    
    // 帧管理
    void BeginFrame();
    
    // 批量操作
    void Clear();
    void ClearType(ResourceType type);
    size_t CleanupUnused(uint32_t unusedFrames = 60);
    size_t CleanupUnusedType(ResourceType type, uint32_t unusedFrames = 60);
    
    // 统计和监控
    ResourceStats GetStats() const;
    long GetReferenceCount(ResourceType type, const std::string& name) const;
    void PrintStatistics() const;
    std::vector<std::string> ListTextures() const;
    std::vector<std::string> ListMeshes() const;
    std::vector<std::string> ListMaterials() const;
    std::vector<std::string> ListShaders() const;
    
    // 高级功能
    void ForEachTexture(std::function<void(const std::string&, Ref<Texture>)> callback);
    void ForEachMesh(std::function<void(const std::string&, Ref<Mesh>)> callback);
    void ForEachMaterial(std::function<void(const std::string&, Ref<Material>)> callback);
    void ForEachShader(std::function<void(const std::string&, Ref<Shader>)> callback);
};
```

---

## 数据结构

### ResourceType

资源类型枚举。

```cpp
enum class ResourceType {
    Texture,    // 纹理
    Mesh,       // 网格
    Material,   // 材质
    Shader      // 着色器
};
```

### ResourceEntry<T>

资源条目结构，封装资源引用和访问信息。

```cpp
template<typename T>
struct ResourceEntry {
    std::shared_ptr<T> resource;        // 资源引用
    bool markedForDeletion = false;     // 删除标记（用于两阶段清理）
    uint32_t lastAccessFrame = 0;       // 最后访问帧号
};
```

**说明**: 内部使用的结构，用户代码不需要直接访问。

### ResourceStats

资源统计信息结构。

```cpp
struct ResourceStats {
    size_t textureCount;      // 纹理数量
    size_t meshCount;          // 网格数量
    size_t materialCount;      // 材质数量
    size_t shaderCount;        // 着色器数量
    size_t totalCount;         // 总资源数量
    
    size_t textureMemory;      // 纹理内存（字节）
    size_t meshMemory;         // 网格内存（字节）
    size_t totalMemory;        // 总内存（字节）
};
```

---

## 单例访问

### GetInstance()

获取 `ResourceManager` 单例实例。

```cpp
static ResourceManager& GetInstance()
```

**返回值**: `ResourceManager` 引用

**示例**:
```cpp
auto& resMgr = ResourceManager::GetInstance();
```

---

## 帧管理

### BeginFrame()

开始新的一帧，更新帧计数器。

```cpp
void BeginFrame()
```

**说明**: 
- 应在每帧开始时调用（通常在主循环中）
- 用于跟踪资源访问，支持基于帧数的资源清理
- 内部维护帧计数器，每次调用递增

**示例**:
```cpp
auto& resMgr = ResourceManager::GetInstance();

// 主循环
while (running) {
    // 每帧开始时调用
    resMgr.BeginFrame();
    
    // ... 渲染代码 ...
    
    // 定期清理未使用资源
    if (frameCount % 60 == 0) {
        resMgr.CleanupUnused(60);  // 清理60帧未使用的资源
    }
}
```

**重要**: 
- 如果不调用 `BeginFrame()`，资源的访问帧信息不会更新
- `CleanupUnused()` 依赖于帧追踪信息，建议始终调用 `BeginFrame()`

---

## 纹理管理

### RegisterTexture()

注册纹理资源。

```cpp
bool RegisterTexture(const std::string& name, Ref<Texture> texture)
```

**参数**:
- `name` - 纹理名称（唯一标识）
- `texture` - 纹理对象

**返回值**: 成功返回 `true`，名称冲突或纹理为空返回 `false`

**示例**:
```cpp
auto& resMgr = ResourceManager::GetInstance();
auto texture = TextureLoader::GetInstance().LoadTexture("my_tex", "textures/test.png");

if (resMgr.RegisterTexture("my_texture", texture)) {
    LOG_INFO("纹理注册成功");
}
```

### GetTexture()

获取纹理资源。

```cpp
Ref<Texture> GetTexture(const std::string& name)
```

**返回值**: 纹理对象，不存在返回 `nullptr`

**说明**: 
- 每次调用会自动更新资源的最后访问帧号
- 访问过的资源不会被立即清理，除非长时间未使用

**示例**:
```cpp
auto texture = resMgr.GetTexture("my_texture");
if (texture) {
    texture->Bind(0);
}
```

**注意**: 其他 `Get*` 方法（`GetMesh()`、`GetMaterial()`、`GetShader()`）也会自动更新访问帧

### RemoveTexture()

移除纹理资源。

```cpp
bool RemoveTexture(const std::string& name)
```

**返回值**: 成功移除返回 `true`

### HasTexture()

检查纹理是否存在。

```cpp
bool HasTexture(const std::string& name) const
```

---

## 网格管理

### RegisterMesh()

注册网格资源。

```cpp
bool RegisterMesh(const std::string& name, Ref<Mesh> mesh)
```

**示例**:
```cpp
auto& resMgr = ResourceManager::GetInstance();
auto cube = MeshLoader::CreateCube();

if (resMgr.RegisterMesh("my_cube", cube)) {
    LOG_INFO("网格注册成功");
}
```

### GetMesh()

获取网格资源。

```cpp
Ref<Mesh> GetMesh(const std::string& name)
```

**示例**:
```cpp
auto mesh = resMgr.GetMesh("my_cube");
if (mesh) {
    mesh->Draw();
}
```

### RemoveMesh() / HasMesh()

移除和检查网格资源。

```cpp
bool RemoveMesh(const std::string& name)
bool HasMesh(const std::string& name) const
```

---

## 材质管理

### RegisterMaterial()

注册材质资源。

```cpp
bool RegisterMaterial(const std::string& name, Ref<Material> material)
```

**示例**:
```cpp
auto& resMgr = ResourceManager::GetInstance();
auto material = std::make_shared<Material>();
material->SetName("red_plastic");
material->SetDiffuseColor(Color::Red());

if (resMgr.RegisterMaterial("red_plastic", material)) {
    LOG_INFO("材质注册成功");
}
```

### GetMaterial()

获取材质资源。

```cpp
Ref<Material> GetMaterial(const std::string& name)
```

**示例**:
```cpp
auto material = resMgr.GetMaterial("red_plastic");
if (material) {
    material->Bind();
    mesh->Draw();
    material->Unbind();
}
```

### RemoveMaterial() / HasMaterial()

移除和检查材质资源。

```cpp
bool RemoveMaterial(const std::string& name)
bool HasMaterial(const std::string& name) const
```

---

## 着色器管理

### RegisterShader()

注册着色器资源。

```cpp
bool RegisterShader(const std::string& name, Ref<Shader> shader)
```

**示例**:
```cpp
auto& resMgr = ResourceManager::GetInstance();
auto shader = ShaderCache::GetInstance().LoadShader("phong", "phong.vert", "phong.frag");

if (resMgr.RegisterShader("phong", shader)) {
    LOG_INFO("着色器注册成功");
}
```

### GetShader()

获取着色器资源。

```cpp
Ref<Shader> GetShader(const std::string& name)
```

### RemoveShader() / HasShader()

移除和检查着色器资源。

```cpp
bool RemoveShader(const std::string& name)
bool HasShader(const std::string& name) const
```

---

## 批量操作

### Clear()

清空所有资源。

```cpp
void Clear()
```

**说明**: 清空所有类型的资源。只有当资源的外部引用都已释放时，资源才会真正被删除。

**示例**:
```cpp
resMgr.Clear();
```

### ClearType()

清空指定类型的资源。

```cpp
void ClearType(ResourceType type)
```

**示例**:
```cpp
// 只清空纹理
resMgr.ClearType(ResourceType::Texture);
```

### CleanupUnused()

清理未使用的资源（引用计数为1，仅被管理器持有）。

```cpp
size_t CleanupUnused(uint32_t unusedFrames = 60)
```

**参数**:
- `unusedFrames` - 资源多少帧未使用后清理（默认60帧）

**返回值**: 清理的资源数量

**清理策略**:
1. **两阶段清理**: 先标记待删除资源，再检查并删除，避免竞态条件
2. **帧数判断**: 只清理超过 `unusedFrames` 帧未访问的资源
3. **引用计数**: 只清理引用计数为1（仅被管理器持有）的资源
4. **详细日志**: 清理时输出资源名称和未使用的帧数

**示例**:
```cpp
// 清理60帧未使用的资源（默认）
size_t cleaned = resMgr.CleanupUnused();
LOG_INFO("清理了 " + std::to_string(cleaned) + " 个未使用资源");

// 清理30帧未使用的资源（更激进）
size_t cleaned = resMgr.CleanupUnused(30);

// 立即清理所有未使用资源
size_t cleaned = resMgr.CleanupUnused(0);
```

**最佳实践**:
```cpp
void MainLoop() {
    auto& resMgr = ResourceManager::GetInstance();
    int frameCount = 0;
    
    while (running) {
        resMgr.BeginFrame();  // 更新帧计数
        
        // 渲染代码...
        
        // 每60帧清理一次
        if (++frameCount % 60 == 0) {
            resMgr.CleanupUnused(120);  // 清理120帧未使用的资源
        }
    }
}
```

### CleanupUnusedType()

清理指定类型的未使用资源。

```cpp
size_t CleanupUnusedType(ResourceType type, uint32_t unusedFrames = 60)
```

**参数**:
- `type` - 资源类型
- `unusedFrames` - 资源多少帧未使用后清理（默认60帧）

**示例**:
```cpp
// 只清理60帧未使用的网格
size_t cleaned = resMgr.CleanupUnusedType(ResourceType::Mesh, 60);

// 立即清理所有未使用的纹理
size_t cleaned = resMgr.CleanupUnusedType(ResourceType::Texture, 0);
```

---

## 统计和监控

### GetStats()

获取资源统计信息。

```cpp
ResourceStats GetStats() const
```

**返回值**: 资源统计信息结构

**示例**:
```cpp
auto stats = resMgr.GetStats();
LOG_INFO("总资源数: " + std::to_string(stats.totalCount));
LOG_INFO("总内存: " + std::to_string(stats.totalMemory / 1024) + " KB");
```

### GetReferenceCount()

获取指定资源的引用计数。

```cpp
long GetReferenceCount(ResourceType type, const std::string& name) const
```

**返回值**: 引用计数，资源不存在返回 0

**示例**:
```cpp
long refCount = resMgr.GetReferenceCount(ResourceType::Mesh, "my_cube");
LOG_INFO("引用计数: " + std::to_string(refCount));
```

### PrintStatistics()

打印资源统计信息到日志。

```cpp
void PrintStatistics() const
```

**输出示例**:
```
========================================
资源管理器统计信息
========================================
纹理数量: 5
网格数量: 10
材质数量: 8
着色器数量: 3
总资源数量: 26
----------------------------------------
纹理内存: 2048 KB
网格内存: 512 KB
总内存: 2560 KB
========================================
```

### ListTextures() / ListMeshes() / ListMaterials() / ListShaders()

列出所有资源名称。

```cpp
std::vector<std::string> ListTextures() const
std::vector<std::string> ListMeshes() const
std::vector<std::string> ListMaterials() const
std::vector<std::string> ListShaders() const
```

**示例**:
```cpp
auto meshes = resMgr.ListMeshes();
for (const auto& name : meshes) {
    LOG_INFO("网格: " + name);
}
```

---

## 高级功能

### ForEachTexture()

遍历所有纹理。

```cpp
void ForEachTexture(std::function<void(const std::string&, Ref<Texture>)> callback)
```

**示例**:
```cpp
resMgr.ForEachTexture([](const std::string& name, Ref<Texture> texture) {
    LOG_INFO("纹理: " + name + ", 大小: " + 
        std::to_string(texture->GetWidth()) + "x" + std::to_string(texture->GetHeight()));
});
```

### ForEachMesh()

遍历所有网格。

```cpp
void ForEachMesh(std::function<void(const std::string&, Ref<Mesh>)> callback)
```

**示例**:
```cpp
resMgr.ForEachMesh([](const std::string& name, Ref<Mesh> mesh) {
    LOG_INFO("网格: " + name + ", 顶点数: " + std::to_string(mesh->GetVertexCount()));
});
```

### ForEachMaterial() / ForEachShader()

遍历材质和着色器。

```cpp
void ForEachMaterial(std::function<void(const std::string&, Ref<Material>)> callback)
void ForEachShader(std::function<void(const std::string&, Ref<Shader>)> callback)
```

---

## 完整示例

### 基本使用

```cpp
#include <render/resource_manager.h>
#include <render/mesh_loader.h>
#include <render/texture_loader.h>
#include <render/material.h>
#include <render/shader_cache.h>

int main() {
    auto& resMgr = ResourceManager::GetInstance();
    
    // 1. 注册网格
    auto cube = MeshLoader::CreateCube();
    resMgr.RegisterMesh("my_cube", cube);
    
    // 2. 注册纹理
    auto texture = TextureLoader::GetInstance().LoadTexture("tex", "test.png");
    resMgr.RegisterTexture("my_texture", texture);
    
    // 3. 注册材质
    auto material = std::make_shared<Material>();
    material->SetDiffuseColor(Color::Red());
    resMgr.RegisterMaterial("my_material", material);
    
    // 4. 主循环
    while (running) {
        // 每帧开始
        resMgr.BeginFrame();
        
        // 使用资源
        auto mesh = resMgr.GetMesh("my_cube");
        auto mat = resMgr.GetMaterial("my_material");
        
        mat->Bind();
        mesh->Draw();
        mat->Unbind();
        
        // 定期清理
        static int frameCount = 0;
        if (++frameCount % 60 == 0) {
            resMgr.CleanupUnused(60);
        }
    }
    
    // 5. 打印统计
    resMgr.PrintStatistics();
    
    return 0;
}
```

### 场景管理

```cpp
class SceneManager {
private:
    ResourceManager& m_resourceMgr = ResourceManager::GetInstance();
    
public:
    void LoadScene(const std::string& sceneName) {
        // 清理旧场景资源（立即清理）
        m_resourceMgr.CleanupUnused(0);
        
        // 加载新场景资源
        if (sceneName == "level1") {
            LoadLevel1Resources();
        } else if (sceneName == "level2") {
            LoadLevel2Resources();
        }
        
        m_resourceMgr.PrintStatistics();
    }
    
    void LoadLevel1Resources() {
        // 加载网格
        auto ground = MeshLoader::CreatePlane(50.0f, 50.0f);
        m_resourceMgr.RegisterMesh("level1_ground", ground);
        
        // 加载纹理
        auto groundTex = TextureLoader::GetInstance().LoadTexture(
            "level1_ground_tex", "textures/level1/ground.png");
        m_resourceMgr.RegisterTexture("level1_ground_tex", groundTex);
        
        // 加载材质
        auto material = std::make_shared<Material>();
        material->SetTexture("diffuseMap", groundTex);
        m_resourceMgr.RegisterMaterial("level1_ground_mat", material);
    }
    
    void UnloadCurrentScene() {
        m_resourceMgr.Clear();
    }
};
```

### 资源引用计数管理

```cpp
void TestResourceLifetime() {
    auto& resMgr = ResourceManager::GetInstance();
    
    // 创建并注册网格
    auto cube = MeshLoader::CreateCube();
    resMgr.RegisterMesh("test_cube", cube);
    
    // 引用计数 = 2 (resMgr + cube变量)
    long refCount = resMgr.GetReferenceCount(ResourceType::Mesh, "test_cube");
    LOG_INFO("引用计数: " + std::to_string(refCount));  // 输出: 2
    
    {
        // 获取网格，引用计数增加
        auto meshCopy = resMgr.GetMesh("test_cube");
        refCount = resMgr.GetReferenceCount(ResourceType::Mesh, "test_cube");
        LOG_INFO("引用计数: " + std::to_string(refCount));  // 输出: 3
    }
    
    // meshCopy 销毁，引用计数减少
    refCount = resMgr.GetReferenceCount(ResourceType::Mesh, "test_cube");
    LOG_INFO("引用计数: " + std::to_string(refCount));  // 输出: 2
    
    // cube 销毁
    cube.reset();
    refCount = resMgr.GetReferenceCount(ResourceType::Mesh, "test_cube");
    LOG_INFO("引用计数: " + std::to_string(refCount));  // 输出: 1
    
    // 清理未使用资源（引用计数为1）
    size_t cleaned = resMgr.CleanupUnused();
    LOG_INFO("清理了 " + std::to_string(cleaned) + " 个资源");  // 输出: 1
}
```

---

## 线程安全

**✅ ResourceManager 是完全线程安全的**。

### 线程安全保证

- 所有公共方法都使用 `std::mutex` 保护
- 多个线程可以安全地并发注册、获取、移除资源
- 统计和查询操作是线程安全的
- `ForEach` 遍历操作在持锁状态下执行，确保一致性

### 线程安全使用示例

#### 多线程并发注册资源

```cpp
auto& resMgr = ResourceManager::GetInstance();
std::vector<std::thread> threads;

for (int i = 0; i < 10; i++) {
    threads.emplace_back([&resMgr, i]() {
        // 并发创建和注册网格
        auto mesh = MeshLoader::CreateSphere(0.5f, 32, 16);
        std::string name = "sphere_" + std::to_string(i);
        resMgr.RegisterMesh(name, mesh);  // 线程安全
    });
}

for (auto& t : threads) {
    t.join();
}
```

#### 多线程并发获取资源

```cpp
auto& resMgr = ResourceManager::GetInstance();

// 主线程中预加载资源
auto cube = MeshLoader::CreateCube();
resMgr.RegisterMesh("shared_cube", cube);

// 多个工作线程并发获取
std::vector<std::thread> workers;
for (int i = 0; i < 5; i++) {
    workers.emplace_back([&resMgr]() {
        // 安全地获取和使用资源
        auto mesh = resMgr.GetMesh("shared_cube");  // 线程安全
        if (mesh) {
            auto vertexCount = mesh->GetVertexCount();
            // 处理网格...
        }
    });
}

for (auto& w : workers) {
    w.join();
}
```

#### 多线程统计查询

```cpp
// 监控线程
std::thread monitor([&resMgr]() {
    while (running) {
        // 所有这些调用都是线程安全的
        auto stats = resMgr.GetStats();
        auto meshes = resMgr.ListMeshes();
        long refCount = resMgr.GetReferenceCount(ResourceType::Mesh, "my_mesh");
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
});
```

### 性能考虑

1. **锁粒度**: 所有操作使用单一互斥锁，简单可靠但可能在高并发下有竞争
2. **批量操作**: 批量注册资源时，可以考虑在单线程中完成以减少锁竞争
3. **读多写少**: 如果场景中读操作远多于写操作，当前实现已足够高效

### 测试

项目包含专门的线程安全测试程序：

```bash
# 运行资源管理器线程安全测试
./build/bin/Release/16_resource_manager_thread_safe_test.exe
```

测试内容包括：
1. 多线程并发注册资源
2. 多线程并发获取资源
3. 多线程并发统计查询
4. 并发清理和注册
5. ForEach 遍历的线程安全性

---

## 与其他系统的集成

### 与 TextureLoader 集成

`ResourceManager` 可以与 `TextureLoader` 配合使用：

```cpp
auto& resMgr = ResourceManager::GetInstance();
auto& texLoader = TextureLoader::GetInstance();

// 加载纹理并注册到资源管理器
auto texture = texLoader.LoadTexture("my_tex", "textures/test.png");
resMgr.RegisterTexture("my_texture", texture);

// 稍后获取
auto tex = resMgr.GetTexture("my_texture");
```

### 与 ShaderCache 集成

```cpp
auto& resMgr = ResourceManager::GetInstance();
auto& shaderCache = ShaderCache::GetInstance();

// 加载着色器并注册
auto shader = shaderCache.LoadShader("phong", "phong.vert", "phong.frag");
resMgr.RegisterShader("phong", shader);
```

### 与 MeshLoader 集成

```cpp
auto& resMgr = ResourceManager::GetInstance();

// 从文件加载模型
auto meshes = MeshLoader::LoadFromFile("models/character.fbx");
for (size_t i = 0; i < meshes.size(); i++) {
    std::string name = "character_mesh_" + std::to_string(i);
    resMgr.RegisterMesh(name, meshes[i]);
}
```

---

## 注意事项

### 1. 名称冲突

重复注册同名资源会失败：

```cpp
// 第一次注册成功
resMgr.RegisterMesh("cube", mesh1);  // 返回 true

// 第二次注册失败（名称冲突）
resMgr.RegisterMesh("cube", mesh2);  // 返回 false，输出警告
```

### 2. 引用计数

资源的生命周期由 `shared_ptr` 管理：

```cpp
// 注册后引用计数 = 1
resMgr.RegisterMesh("cube", cube);

// 获取后引用计数增加
auto mesh1 = resMgr.GetMesh("cube");  // 引用计数 = 2
auto mesh2 = resMgr.GetMesh("cube");  // 引用计数 = 3

// 变量销毁后引用计数减少
mesh1.reset();  // 引用计数 = 2
mesh2.reset();  // 引用计数 = 1

// 清理未使用资源
resMgr.CleanupUnused();  // 会清理引用计数为1的资源
```

**⚠️ 最佳实践：保持对活动资源的引用**

如果你在渲染循环中使用 `CleanupUnused()`，需要确保当前使用的资源不会被清理：

```cpp
// ❌ 错误：每帧获取临时引用
void RenderScene() {
    auto mesh = resMgr.GetMesh("cube");  // 临时引用
    mesh->Draw();
    // 函数结束后引用计数回到1，可能被 CleanupUnused() 清理
}

// ✅ 正确：保持对活动资源的持久引用
Ref<Mesh> activeMesh;  // 全局或类成员

void InitScene() {
    auto cube = MeshLoader::CreateCube();
    resMgr.RegisterMesh("cube", cube);
    activeMesh = resMgr.GetMesh("cube");  // 保持引用，引用计数 = 2
}

void RenderScene() {
    if (activeMesh) {
        activeMesh->Draw();  // 使用持久引用
    }
}

// 现在调用 CleanupUnused() 不会清理 activeMesh（引用计数 >= 2）
resMgr.CleanupUnused();
```

完整示例请参考 `examples/15_resource_manager_test.cpp`。

### 3. 帧追踪和清理策略

**新的清理机制** (2025-10-30 更新):

ResourceManager 现在使用基于帧追踪的智能清理策略：

```cpp
// 每帧更新
void MainLoop() {
    auto& resMgr = ResourceManager::GetInstance();
    
    while (running) {
        // 步骤1: 开始新帧
        resMgr.BeginFrame();  // 更新帧计数器
        
        // 步骤2: 获取和使用资源
        auto mesh = resMgr.GetMesh("cube");  // 自动更新访问帧
        mesh->Draw();
        
        // 步骤3: 定期清理
        if (frameCount % 60 == 0) {
            // 清理60帧未访问的资源
            resMgr.CleanupUnused(60);
        }
    }
}
```

**优势**:
- ✅ **防止意外删除**: 刚使用的资源不会被立即清理
- ✅ **避免竞态条件**: 两阶段清理确保引用计数检查的一致性
- ✅ **灵活控制**: 可配置未使用帧数阈值
- ✅ **详细日志**: 输出清理信息和资源未使用的帧数

### 4. Clear() vs CleanupUnused()

- `Clear()`: 立即移除管理器中的所有引用，但资源可能仍然存活（如果外部有引用）
- `CleanupUnused(unusedFrames)`: 只清理长时间未使用且未被外部持有的资源
  - 基于帧数判断（默认60帧）
  - 基于引用计数（必须为1）
  - 使用两阶段清理，避免竞态条件

---

## 更新日志

### 版本 2.0 (2025-10-30)

**重大改进**: 引入帧追踪和两阶段清理机制

#### 新增功能
- ✅ 添加 `BeginFrame()` 方法，用于帧计数和资源访问追踪
- ✅ 添加 `ResourceEntry<T>` 结构，封装资源引用和访问信息
- ✅ `CleanupUnused()` 新增 `unusedFrames` 参数（默认60帧）
- ✅ `CleanupUnusedType()` 新增 `unusedFrames` 参数（默认60帧）

#### 改进
- ✅ **两阶段清理策略**: 先标记，再删除，避免竞态条件
- ✅ **帧数判断**: 基于最后访问帧号，不会删除刚使用的资源
- ✅ **自动追踪**: 所有 `Get*` 方法自动更新资源访问帧
- ✅ **详细日志**: 清理时输出资源未使用的帧数

#### 示例
```cpp
// 新的使用方式
while (running) {
    resMgr.BeginFrame();  // 每帧调用
    
    // 使用资源
    auto mesh = resMgr.GetMesh("cube");  // 自动更新访问帧
    mesh->Draw();
    
    // 定期清理
    if (frameCount % 60 == 0) {
        resMgr.CleanupUnused(60);  // 清理60帧未使用的资源
    }
}
```

#### 向后兼容
- ✅ 所有现有 API 保持兼容
- ✅ `CleanupUnused()` 可不传参数（默认60帧）
- ✅ 如果不调用 `BeginFrame()`，行为与旧版本类似（但建议调用）

---

## 相关文档

- [Texture API](Texture.md)
- [Mesh API](Mesh.md)
- [Material API](Material.md)
- [Shader API](Shader.md)
- [TextureLoader API](TextureLoader.md)
- [MeshLoader API](MeshLoader.md)
- [ShaderCache API](ShaderCache.md)
- [示例程序: 15_resource_manager_test](../../examples/15_resource_manager_test.cpp)
- [线程安全测试: 16_resource_manager_thread_safe_test](../../examples/16_resource_manager_thread_safe_test.cpp) 🔒

---

[上一篇: Material](Material.md) | [返回 API 首页](README.md)

