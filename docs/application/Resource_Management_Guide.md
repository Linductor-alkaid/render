[返回文档首页](../README.md)

---

# 资源管理使用指南

**版本**: v1.0  
**更新时间**: 2025-11-18  
**相关文档**: [APPLICATION_LAYER_PROGRESS_REPORT.md](APPLICATION_LAYER_PROGRESS_REPORT.md)

---

## 1. 概述

应用层资源管理系统提供了完整的资源生命周期管理，包括：

- **资源清单系统**：场景声明所需资源
- **自动预加载**：检测缺失资源并自动触发异步加载
- **细粒度释放控制**：根据资源范围（Scene/Shared）精确控制释放时机
- **进度跟踪**：实时监控资源加载进度

### 核心组件

| 组件 | 说明 |
|------|------|
| `SceneResourceManifest` | 资源清单结构，包含必需和可选资源列表 |
| `ResourceRequest` | 单个资源请求，包含标识符、类型、范围和可选标记 |
| `ResourceScope` | 资源范围枚举：`Scene`（场景专用）或 `Shared`（共享） |
| `SceneManager` | 场景管理器，负责资源预加载和释放 |

---

## 2. 资源清单系统

### 2.1 声明资源

场景通过 `BuildManifest()` 方法声明所需资源。有两种方式：

#### 方式1：使用 SceneGraph 节点（推荐）

```cpp
class MyNode : public SceneNode {
public:
    MyNode() : SceneNode("MyNode") {
        // 声明必需资源
        RegisterRequiredResource("my_mesh", "mesh");
        RegisterRequiredResource("my_material", "material");
        
        // 声明可选资源
        RegisterOptionalResource("optional_texture", "texture");
        
        // 声明共享资源（多个场景共用）
        RegisterRequiredResource("shared_font", "font", ResourceScope::Shared);
    }
};
```

#### 方式2：直接实现 BuildManifest()

```cpp
class MyScene : public Scene {
    SceneResourceManifest BuildManifest() const override {
        SceneResourceManifest manifest;
        
        // 必需资源
        ResourceRequest req1;
        req1.identifier = "my_mesh";
        req1.type = "mesh";
        req1.scope = ResourceScope::Scene;
        req1.optional = false;
        manifest.required.push_back(req1);
        
        // 可选资源
        ResourceRequest req2;
        req2.identifier = "optional_texture";
        req2.type = "texture";
        req2.scope = ResourceScope::Scene;
        req2.optional = true;
        manifest.optional.push_back(req2);
        
        // 共享资源
        ResourceRequest req3;
        req3.identifier = "shared_font";
        req3.type = "font";
        req3.scope = ResourceScope::Shared;
        req3.optional = false;
        manifest.required.push_back(req3);
        
        return manifest;
    }
};
```

### 2.2 支持的资源类型

| 资源类型 | 类型字符串 | 说明 |
|---------|-----------|------|
| 网格 | `"mesh"` | 3D 网格数据 |
| 材质 | `"material"` | 材质属性 |
| 纹理 | `"texture"` | 2D 纹理图像 |
| 模型 | `"model"` | 完整模型（包含多个网格和材质） |
| 精灵图集 | `"sprite_atlas"` | 2D 精灵图集 |
| 字体 | `"font"` | 字体资源 |
| 着色器 | `"shader"` | 着色器程序 |

### 2.3 资源范围（ResourceScope）

#### ResourceScope::Scene

场景专用资源，场景退出时自动释放。

**适用场景**：
- 场景特定的资源
- 临时资源
- 大型资源（释放以节省内存）

**示例**：
```cpp
RegisterRequiredResource("level1_terrain", "mesh", ResourceScope::Scene);
```

#### ResourceScope::Shared

共享资源，场景退出后保留在 ResourceManager 中。

**适用场景**：
- 多个场景共用的资源
- UI 资源
- 常用资源（避免重复加载）

**示例**：
```cpp
RegisterRequiredResource("ui_font", "font", ResourceScope::Shared);
RegisterRequiredResource("common_texture", "texture", ResourceScope::Shared);
```

---

## 3. 资源预加载机制

### 3.1 自动预加载流程

当场景被推送到 SceneManager 时，会自动执行以下流程：

1. **构建资源清单**：调用 `BuildManifest()` 获取资源列表
2. **检测资源可用性**：检查每个资源是否已在 ResourceManager 中
3. **触发异步加载**：缺失的资源自动提交到 `AsyncResourceLoader`
4. **阻塞进入**：必需资源未加载完成时，场景不会进入 `OnEnter()`
5. **进度跟踪**：发布 `ScenePreloadProgressEvent` 事件

### 3.2 必需资源 vs 可选资源

#### 必需资源（Required）

- 场景进入前必须加载完成
- 缺失时会阻塞场景进入
- 自动触发异步加载

```cpp
// 必需资源：场景无法进入直到资源加载完成
RegisterRequiredResource("player_model", "model");
```

#### 可选资源（Optional）

- 缺失不影响场景进入
- 也会触发异步加载，但优先级较低
- 适合用于增强效果或备用资源

```cpp
// 可选资源：场景可以进入，资源稍后加载
RegisterOptionalResource("enhancement_texture", "texture");
```

### 3.3 监听预加载进度

订阅 `ScenePreloadProgressEvent` 事件以监控加载进度：

```cpp
#include <render/application/events/scene_events.h>

void OnPreloadProgress(const Events::ScenePreloadProgressEvent& event) {
    std::cout << "场景: " << event.sceneId << "\n";
    std::cout << "必需资源: " << event.requiredLoaded 
              << "/" << event.requiredTotal << "\n";
    std::cout << "可选资源: " << event.optionalLoaded 
              << "/" << event.optionalTotal << "\n";
    std::cout << "完成: " << (event.completed ? "是" : "否") << "\n";
}

// 订阅事件
eventBus->Subscribe<Events::ScenePreloadProgressEvent>(
    OnPreloadProgress,
    EventPriority::Normal
);
```

### 3.4 手动预加载资源

如果需要手动触发资源加载，可以使用 `AsyncResourceLoader`：

```cpp
#include <render/async_resource_loader.h>

auto& loader = AsyncResourceLoader::GetInstance();

// 加载网格
auto meshTask = loader.LoadMeshAsync(
    "models/character.fbx",
    "character",
    [](const MeshLoadResult& result) {
        if (result.IsSuccess()) {
            ResourceManager::GetInstance().RegisterMesh(
                result.name, 
                result.resource
            );
        }
    },
    10.0f  // 优先级
);

// 在主循环中处理完成的任务
while (running) {
    loader.ProcessCompletedTasks(10);
    // ... 其他逻辑
}
```

---

## 4. 资源释放策略

### 4.1 自动释放机制

场景退出时，SceneManager 会根据资源清单自动释放资源：

- **Scene 范围资源**：自动从 ResourceManager 中移除
- **Shared 范围资源**：保留在 ResourceManager 中

### 4.2 释放时机

资源释放发生在场景的 `Detach` 阶段：

```
场景生命周期：
  Attach → Preload → Enter → Update → Exit → Detach
                                    ↑
                              资源释放发生在这里
```

### 4.3 释放示例

```cpp
class Level1Scene : public Scene {
    SceneResourceManifest BuildManifest() const override {
        SceneResourceManifest manifest;
        
        // Scene 资源：退出时释放
        ResourceRequest sceneMesh;
        sceneMesh.identifier = "level1_terrain";
        sceneMesh.type = "mesh";
        sceneMesh.scope = ResourceScope::Scene;
        manifest.required.push_back(sceneMesh);
        
        // Shared 资源：退出时保留
        ResourceRequest sharedFont;
        sharedFont.identifier = "ui_font";
        sharedFont.type = "font";
        sharedFont.scope = ResourceScope::Shared;
        manifest.required.push_back(sharedFont);
        
        return manifest;
    }
};

// 场景切换
sceneManager.PushScene("Level1Scene");
// ... 使用场景 ...
sceneManager.PopScene(exitArgs);
// level1_terrain 被释放
// ui_font 保留在 ResourceManager 中
```

### 4.4 引用计数

ResourceManager 使用引用计数管理资源。只有当引用计数为 1（仅被 ResourceManager 持有）时，资源才会被真正释放。

**注意**：如果场景或其他代码仍持有资源的 `shared_ptr`，资源不会被释放。

```cpp
// ❌ 错误：持有资源引用会阻止释放
Ref<Mesh> myMesh = resourceManager.GetMesh("level1_terrain");
sceneManager.PopScene(exitArgs);
// myMesh 仍持有引用，资源不会被释放

// ✅ 正确：不持有长期引用
void Render() {
    auto mesh = resourceManager.GetMesh("level1_terrain");
    if (mesh) {
        mesh->Draw();
    }
    // 函数结束后引用自动释放
}
```

---

## 5. 完整示例

### 5.1 使用 SceneGraph 的完整场景

```cpp
#include <render/application/scene_graph.h>
#include <render/application/scene.h>
#include <render/mesh_loader.h>
#include <render/material.h>
#include <render/shader_cache.h>

class GameLevelScene : public Scene {
public:
    std::string_view Name() const override { return "GameLevel"; }
    
    void OnAttach(AppContext& ctx, ModuleRegistry& modules) override {
        // 创建场景图
        m_sceneGraph = std::make_unique<SceneGraph>();
        
        // 创建节点
        auto terrainNode = std::make_shared<TerrainNode>();
        auto playerNode = std::make_shared<PlayerNode>();
        
        m_sceneGraph->SetRoot(terrainNode);
        terrainNode->AddChild(playerNode);
        
        // 附加场景图
        m_sceneGraph->Attach(*this, ctx);
    }
    
    SceneResourceManifest BuildManifest() const override {
        // SceneGraph 自动收集所有节点的资源声明
        return m_sceneGraph->BuildManifest();
    }
    
    void OnEnter(const SceneEnterArgs& args) override {
        m_sceneGraph->Enter(args);
    }
    
    void OnUpdate(const FrameUpdateArgs& frame) override {
        m_sceneGraph->Update(frame);
    }
    
    SceneSnapshot OnExit(const SceneExitArgs& args) override {
        m_sceneGraph->Exit();
        SceneSnapshot snapshot;
        snapshot.sceneId = "GameLevel";
        return snapshot;
    }
    
    void OnDetach(AppContext& ctx) override {
        m_sceneGraph->Detach();
    }
    
private:
    std::unique_ptr<SceneGraph> m_sceneGraph;
};

// 地形节点
class TerrainNode : public SceneNode {
public:
    TerrainNode() : SceneNode("TerrainNode") {
        // 声明必需资源（Scene 范围）
        RegisterRequiredResource("level_terrain", "mesh");
        RegisterRequiredResource("terrain_material", "material");
        
        // 声明可选资源
        RegisterOptionalResource("terrain_detail", "texture");
    }
    
protected:
    void OnAttach(Scene&, AppContext& ctx) override {
        auto& rm = *ctx.resourceManager;
        
        // 如果资源不存在，创建并注册
        if (!rm.HasMesh("level_terrain")) {
            auto mesh = MeshLoader::CreatePlane(100.0f, 100.0f);
            rm.RegisterMesh("level_terrain", mesh);
        }
        
        if (!rm.HasMaterial("terrain_material")) {
            auto shader = ShaderCache::GetInstance().LoadShader(
                "terrain_shader", 
                "shaders/terrain.vert", 
                "shaders/terrain.frag"
            );
            auto material = std::make_shared<Material>();
            material->SetShader(shader);
            rm.RegisterMaterial("terrain_material", material);
        }
    }
    
    void OnEnter(const SceneEnterArgs&) override {
        // 创建地形实体
        auto& world = GetWorld();
        auto& rm = GetResourceManager();
        
        auto entity = world.CreateEntity({.name = "Terrain"});
        
        ECS::MeshRenderComponent meshComp;
        meshComp.meshName = "level_terrain";
        meshComp.materialName = "terrain_material";
        meshComp.mesh = rm.GetMesh("level_terrain");
        meshComp.material = rm.GetMaterial("terrain_material");
        world.AddComponent(entity, meshComp);
    }
};
```

### 5.2 直接使用资源清单的场景

```cpp
class SimpleScene : public Scene {
public:
    std::string_view Name() const override { return "SimpleScene"; }
    
    void OnAttach(AppContext&, ModuleRegistry&) override {
        // 场景附加逻辑
    }
    
    SceneResourceManifest BuildManifest() const override {
        SceneResourceManifest manifest;
        
        // 必需资源：场景专用
        ResourceRequest meshReq;
        meshReq.identifier = "models/cube.obj";
        meshReq.type = "mesh";
        meshReq.scope = ResourceScope::Scene;
        meshReq.optional = false;
        manifest.required.push_back(meshReq);
        
        // 可选资源：增强效果
        ResourceRequest texReq;
        texReq.identifier = "textures/enhancement.png";
        texReq.type = "texture";
        texReq.scope = ResourceScope::Scene;
        texReq.optional = true;
        manifest.optional.push_back(texReq);
        
        // 共享资源：UI 字体
        ResourceRequest fontReq;
        fontReq.identifier = "fonts/ui_font.ttf";
        fontReq.type = "font";
        fontReq.scope = ResourceScope::Shared;
        fontReq.optional = false;
        manifest.required.push_back(fontReq);
        
        return manifest;
    }
    
    void OnEnter(const SceneEnterArgs&) override {
        // 场景进入逻辑
        // 此时所有必需资源已加载完成
    }
    
    void OnUpdate(const FrameUpdateArgs&) override {
        // 场景更新逻辑
    }
    
    SceneSnapshot OnExit(const SceneExitArgs&) override {
        SceneSnapshot snapshot;
        snapshot.sceneId = "SimpleScene";
        return snapshot;
    }
    
    void OnDetach(AppContext&) override {
        // 场景分离逻辑
        // Scene 范围的资源此时已被释放
    }
};
```

---

## 6. 最佳实践

### 6.1 资源命名规范

使用清晰的命名空间避免冲突：

```cpp
// ✅ 推荐：使用场景/模块前缀
RegisterRequiredResource("level1.terrain.mesh", "mesh");
RegisterRequiredResource("level1.player.model", "model");
RegisterRequiredResource("ui.common.font", "font", ResourceScope::Shared);

// ❌ 不推荐：通用名称容易冲突
RegisterRequiredResource("mesh", "mesh");
RegisterRequiredResource("texture", "texture");
```

### 6.2 资源范围选择

**使用 Scene 范围**：
- 场景特定的资源
- 大型资源（释放以节省内存）
- 临时资源

**使用 Shared 范围**：
- UI 资源（字体、图标）
- 常用资源（避免重复加载）
- 多个场景共用的资源

### 6.3 必需 vs 可选资源

**必需资源**：
- 场景核心功能所需
- 阻塞场景进入的资源

**可选资源**：
- 增强效果
- 备用资源
- 可以稍后加载的资源

### 6.4 避免资源泄漏

```cpp
// ❌ 错误：持有长期引用
class MyScene {
    Ref<Mesh> m_mesh;  // 会阻止资源释放
    
    void OnEnter(const SceneEnterArgs&) {
        m_mesh = GetResourceManager().GetMesh("my_mesh");
    }
};

// ✅ 正确：不持有长期引用
class MyScene {
    void OnEnter(const SceneEnterArgs&) {
        // 每次使用时获取
        auto mesh = GetResourceManager().GetMesh("my_mesh");
        if (mesh) {
            mesh->Draw();
        }
    }
};

// ✅ 或者：使用 ECS 组件持有引用（组件销毁时自动释放）
class MyScene {
    void OnEnter(const SceneEnterArgs&) {
        auto entity = world.CreateEntity();
        ECS::MeshRenderComponent comp;
        comp.mesh = GetResourceManager().GetMesh("my_mesh");
        world.AddComponent(entity, comp);
        // 实体销毁时组件自动销毁，引用自动释放
    }
};
```

### 6.5 性能优化

1. **预加载关键资源**：在场景切换前预加载下一个场景的资源
2. **使用 Shared 资源**：多个场景共用的资源标记为 Shared
3. **合理使用可选资源**：非关键资源标记为可选，不阻塞场景进入
4. **批量加载**：使用 `LoadModelAsync` 加载完整模型（包含多个资源）

---

## 7. 常见问题

### Q1: 资源加载失败怎么办？

**A**: 资源加载失败时，`AsyncResourceLoader` 会调用回调并设置错误信息。场景会一直等待必需资源加载完成，如果加载失败，场景可能永远不会进入。

**建议**：
- 在资源加载回调中处理错误
- 提供默认资源作为后备
- 使用可选资源避免阻塞

### Q2: 如何知道资源是否已加载？

**A**: 使用 `ResourceManager::HasMesh()`、`HasTexture()` 等方法检查：

```cpp
auto& rm = ResourceManager::GetInstance();
if (rm.HasMesh("my_mesh")) {
    auto mesh = rm.GetMesh("my_mesh");
    // 使用资源
}
```

### Q3: 场景退出后资源何时释放？

**A**: 资源释放发生在场景的 `Detach` 阶段。只有 `ResourceScope::Scene` 的资源会被释放，`ResourceScope::Shared` 的资源会保留。

### Q4: 如何手动释放资源？

**A**: 使用 `ResourceManager` 的 `Remove*` 方法：

```cpp
auto& rm = ResourceManager::GetInstance();
rm.RemoveMesh("my_mesh");
rm.RemoveTexture("my_texture");
```