# ECS资源注册最佳实践指南

**目标读者**: 使用ECS系统的开发者  
**前置知识**: 了解ECS基本概念、ResourceManager使用  
**相关文档**: [ECS API](api/ECS.md) | [ResourceManager API](api/ResourceManager.md)

---

## 📋 概述

本文档说明在ECS系统中如何正确注册和管理资源（Mesh、Material、Shader、Texture）。正确的资源注册可以：

- ✅ 避免资源重复加载
- ✅ 实现资源共享和复用
- ✅ 支持资源热重载
- ✅ 正确的生命周期管理

---

## 🎯 核心原则

### 原则 1: 应用层预注册

在应用初始化时预先注册常用资源，而不是在系统运行时动态创建。

**优势**:
- 启动时加载，避免运行时卡顿
- 明确的资源管理流程
- 便于资源预热和验证

**示例**:
```cpp
void Application::InitializeResources() {
    auto& resMgr = ResourceManager::GetInstance();
    auto& shaderCache = ShaderCache::GetInstance();
    
    // 1. 预加载着色器
    shaderCache.LoadShader("basic", "shaders/basic.vert", "shaders/basic.frag");
    shaderCache.LoadShader("phong", "shaders/material_phong.vert", "shaders/material_phong.frag");
    
    // 2. 预创建材质
    auto defaultMat = std::make_shared<Material>();
    defaultMat->SetName("default");
    defaultMat->SetShader(shaderCache.GetShader("basic"));
    defaultMat->SetDiffuseColor(Color(0.8f, 0.8f, 0.8f, 1.0f));
    
    // 3. ✅ 注册到 ResourceManager
    resMgr.RegisterMaterial("default", defaultMat);
    
    Logger::GetInstance().Info("Common resources preloaded");
}
```

### 原则 2: 统一通过 ResourceManager 访问

所有资源的获取都应该通过 ResourceManager，而不是直接使用其他单例（如TextureLoader）。

**错误示例** ❌:
```cpp
// ❌ 错误：混用多个资源管理器
auto texture1 = ResourceManager::GetInstance().GetTexture("tex1");
auto texture2 = TextureLoader::GetInstance().GetTexture("tex2");  // 错误！
```

**正确示例** ✅:
```cpp
// ✅ 正确：统一使用 ResourceManager
auto& resMgr = ResourceManager::GetInstance();
auto texture1 = resMgr.GetTexture("tex1");
auto texture2 = resMgr.GetTexture("tex2");
```

### 原则 3: 检查资源是否已注册

在注册资源前，先检查是否已存在，避免重复注册和警告日志。

```cpp
auto& resMgr = ResourceManager::GetInstance();

// ✅ 正确：先检查再注册
if (!resMgr.HasMaterial("myMaterial")) {
    auto material = std::make_shared<Material>();
    // ... 配置材质 ...
    resMgr.RegisterMaterial("myMaterial", material);
}

// 获取材质（无论是新注册还是已存在）
auto material = resMgr.GetMaterial("myMaterial");
```

---

## 📚 完整示例

### 示例 1: 基本ECS应用的资源注册

```cpp
#include <SDL3/SDL.h>
#include "render/renderer.h"
#include "render/shader_cache.h"
#include "render/resource_manager.h"
#include "render/ecs/world.h"
#include "render/ecs/systems.h"
#include "render/ecs/components.h"

using namespace Render;
using namespace Render::ECS;

class MyApplication {
public:
    bool Initialize() {
        // 1. 初始化渲染器
        m_renderer = std::make_unique<Renderer>();
        if (!m_renderer->Initialize("My App", 1280, 720)) {
            return false;
        }
        
        // 2. 初始化异步加载器
        m_asyncLoader = &AsyncResourceLoader::GetInstance();
        m_asyncLoader->Initialize(4);
        
        // 3. ✅ 预注册资源（关键步骤）
        if (!PreloadResources()) {
            return false;
        }
        
        // 4. 初始化 ECS World
        m_world = std::make_shared<World>();
        m_world->Initialize();
        
        // 5. 注册组件
        RegisterComponents();
        
        // 6. 注册系统
        RegisterSystems();
        
        // 7. 后初始化
        m_world->PostInitialize();
        
        // 8. 创建场景
        CreateScene();
        
        return true;
    }
    
    // ✅ 资源预加载（推荐在应用启动时调用）
    bool PreloadResources() {
        auto& resMgr = ResourceManager::GetInstance();
        auto& shaderCache = ShaderCache::GetInstance();
        
        Logger::GetInstance().Info("=== Preloading Resources ===");
        
        // ==================== 着色器预加载 ====================
        auto basicShader = shaderCache.LoadShader(
            "basic", 
            "shaders/basic.vert", 
            "shaders/basic.frag"
        );
        
        auto phongShader = shaderCache.LoadShader(
            "phong", 
            "shaders/material_phong.vert", 
            "shaders/material_phong.frag"
        );
        
        if (!basicShader || !phongShader) {
            Logger::GetInstance().Error("Failed to load shaders");
            return false;
        }
        
        // ==================== 材质预创建 ====================
        
        // 默认材质（灰色，基础着色器）
        auto defaultMat = std::make_shared<Material>();
        defaultMat->SetName("default");
        defaultMat->SetShader(basicShader);
        defaultMat->SetDiffuseColor(Color(0.8f, 0.8f, 0.8f, 1.0f));
        resMgr.RegisterMaterial("default", defaultMat);
        
        // Phong光照材质（白色，用于纹理模型）
        auto phongMat = std::make_shared<Material>();
        phongMat->SetName("phong");
        phongMat->SetShader(phongShader);
        phongMat->SetAmbientColor(Color(0.2f, 0.2f, 0.2f, 1.0f));
        phongMat->SetDiffuseColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
        phongMat->SetSpecularColor(Color(0.5f, 0.5f, 0.5f, 1.0f));
        phongMat->SetShininess(32.0f);
        resMgr.RegisterMaterial("phong", phongMat);
        
        // 红色材质
        auto redMat = std::make_shared<Material>();
        redMat->SetName("red");
        redMat->SetShader(basicShader);
        redMat->SetDiffuseColor(Color(1.0f, 0.0f, 0.0f, 1.0f));
        resMgr.RegisterMaterial("red", redMat);
        
        // 蓝色材质
        auto blueMat = std::make_shared<Material>();
        blueMat->SetName("blue");
        blueMat->SetShader(basicShader);
        blueMat->SetDiffuseColor(Color(0.0f, 0.0f, 1.0f, 1.0f));
        resMgr.RegisterMaterial("blue", blueMat);
        
        Logger::GetInstance().Info("=== Resources Preloaded ===");
        Logger::GetInstance().InfoFormat("  - Shaders: %zu", shaderCache.GetShaderCount());
        Logger::GetInstance().InfoFormat("  - Materials: %zu", resMgr.GetStats().materialCount);
        
        return true;
    }
    
    void RegisterComponents() {
        m_world->RegisterComponent<TransformComponent>();
        m_world->RegisterComponent<MeshRenderComponent>();
        m_world->RegisterComponent<CameraComponent>();
        m_world->RegisterComponent<LightComponent>();
    }
    
    void RegisterSystems() {
        // 按优先级注册系统
        m_world->RegisterSystem<WindowSystem>(m_renderer.get());
        m_world->RegisterSystem<CameraSystem>();
        m_world->RegisterSystem<TransformSystem>();
        m_world->RegisterSystem<ResourceLoadingSystem>(m_asyncLoader);
        m_world->RegisterSystem<LightSystem>(m_renderer.get());
        m_world->RegisterSystem<UniformSystem>(m_renderer.get());
        m_world->RegisterSystem<MeshRenderSystem>(m_renderer.get());
    }
    
    void CreateScene() {
        auto& resMgr = ResourceManager::GetInstance();
        
        // ==================== 创建相机 ====================
        EntityDescriptor cameraDesc;
        cameraDesc.name = "MainCamera";
        auto cameraEntity = m_world->CreateEntity(cameraDesc);
        
        TransformComponent cameraTransform;
        cameraTransform.SetPosition(Vector3(0, 2, 8));
        cameraTransform.LookAt(Vector3(0, 0, 0));
        m_world->AddComponent(cameraEntity, cameraTransform);
        
        auto camera = std::make_shared<Camera>();
        camera->SetPerspective(60.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);
        
        CameraComponent cameraComp;
        cameraComp.camera = camera;
        cameraComp.active = true;
        m_world->AddComponent(cameraEntity, cameraComp);
        
        // ==================== 创建网格实体 ====================
        
        // 示例1: 使用预注册的材质
        {
            EntityDescriptor desc;
            desc.name = "RedCube";
            auto entity = m_world->CreateEntity(desc);
            
            TransformComponent transform;
            transform.SetPosition(Vector3(-2, 0, 0));
            m_world->AddComponent(entity, transform);
            
            MeshRenderComponent meshComp;
            meshComp.meshName = "models/cube.obj";  // 异步加载
            // ✅ 使用预注册的材质
            meshComp.material = resMgr.GetMaterial("red");
            meshComp.visible = true;
            m_world->AddComponent(entity, meshComp);
        }
        
        // 示例2: 使用materialName（通过ResourceManager获取）
        {
            EntityDescriptor desc;
            desc.name = "BlueCube";
            auto entity = m_world->CreateEntity(desc);
            
            TransformComponent transform;
            transform.SetPosition(Vector3(2, 0, 0));
            m_world->AddComponent(entity, transform);
            
            MeshRenderComponent meshComp;
            meshComp.meshName = "models/cube.obj";
            // ✅ 通过名称引用材质（ResourceLoadingSystem会自动获取）
            meshComp.materialName = "blue";
            meshComp.visible = true;
            m_world->AddComponent(entity, meshComp);
        }
        
        // 示例3: 使用MaterialOverride（每个实体不同外观）
        {
            for (int i = 0; i < 5; i++) {
                EntityDescriptor desc;
                desc.name = "ColoredCube_" + std::to_string(i);
                auto entity = m_world->CreateEntity(desc);
                
                TransformComponent transform;
                float angle = i * 72.0f;  // 圆形排列
                float radius = 4.0f;
                float x = radius * std::cos(angle * 3.14159f / 180.0f);
                float z = radius * std::sin(angle * 3.14159f / 180.0f);
                transform.SetPosition(Vector3(x, 0, z));
                m_world->AddComponent(entity, transform);
                
                MeshRenderComponent meshComp;
                meshComp.meshName = "models/cube.obj";
                meshComp.material = resMgr.GetMaterial("default");  // 共享材质
                
                // ✅ 使用MaterialOverride实现每个实体不同颜色
                // 不修改共享的Material，只在渲染时临时覆盖uniform
                ECS::MaterialOverride override;
                override.diffuseColor = Color(
                    i / 5.0f,           // R
                    1.0f - i / 5.0f,    // G
                    0.5f,               // B
                    1.0f                // A
                );
                meshComp.materialOverride = override;
                
                meshComp.visible = true;
                m_world->AddComponent(entity, meshComp);
            }
        }
        
        Logger::GetInstance().Info("Scene created successfully");
    }
    
    void Update(float deltaTime) {
        // ✅ World::Update 会自动调用 ResourceManager::BeginFrame()
        m_world->Update(deltaTime);
        
        // 渲染
        m_renderer->BeginFrame();
        m_renderer->Clear();
        m_renderer->FlushRenderQueue();
        m_renderer->EndFrame();
        m_renderer->Present();
    }
    
    void Shutdown() {
        // 关闭顺序很重要
        m_world->Shutdown();
        m_asyncLoader->Shutdown();
        m_renderer->Shutdown();
    }
    
private:
    std::unique_ptr<Renderer> m_renderer;
    AsyncResourceLoader* m_asyncLoader = nullptr;
    std::shared_ptr<World> m_world;
};

int main(int argc, char* argv[]) {
    MyApplication app;
    
    if (!app.Initialize()) {
        return -1;
    }
    
    // 主循环
    bool running = true;
    while (running) {
        // 处理事件...
        
        app.Update(0.016f);  // 60 FPS
    }
    
    app.Shutdown();
    return 0;
}
```

---

## 🔧 常见场景

### 场景 1: 动态创建材质（运行时）

如果需要在运行时动态创建材质（不推荐频繁使用）：

```cpp
// 辅助函数：获取或创建材质
std::shared_ptr<Material> GetOrCreateMaterial(const std::string& name, const Color& color) {
    auto& resMgr = ResourceManager::GetInstance();
    
    // 1. 先尝试从缓存获取
    auto material = resMgr.GetMaterial(name);
    if (material) {
        return material;
    }
    
    // 2. 不存在，创建新材质
    auto newMat = std::make_shared<Material>();
    newMat->SetName(name);
    newMat->SetShader(ShaderCache::GetInstance().GetShader("basic"));
    newMat->SetDiffuseColor(color);
    
    // 3. 注册到 ResourceManager
    if (resMgr.RegisterMaterial(name, newMat)) {
        Logger::GetInstance().DebugFormat("Material '%s' created and registered", name.c_str());
        return newMat;
    } else {
        // 注册失败（可能已被其他线程注册），重新获取
        Logger::GetInstance().DebugFormat("Material '%s' was registered by another thread", name.c_str());
        return resMgr.GetMaterial(name);
    }
}

// 使用示例
void CreateEntityWithDynamicMaterial() {
    auto entity = m_world->CreateEntity({.name = "DynamicEntity"});
    
    // ... 添加 TransformComponent ...
    
    MeshRenderComponent meshComp;
    meshComp.meshName = "models/cube.obj";
    // ✅ 使用辅助函数获取或创建材质
    meshComp.material = GetOrCreateMaterial("custom_red", Color(1.0f, 0.0f, 0.0f, 1.0f));
    meshComp.visible = true;
    
    m_world->AddComponent(entity, meshComp);
}
```

### 场景 2: 加载带纹理的模型

```cpp
void LoadTexturedModel() {
    auto& resMgr = ResourceManager::GetInstance();
    auto& asyncLoader = AsyncResourceLoader::GetInstance();
    
    // 1. 先加载纹理（异步）
    asyncLoader.LoadTextureAsync(
        "textures/wood.png",
        "textures/wood.png",
        true,  // 生成 mipmaps
        [&resMgr](const TextureLoadResult& result) {
            if (result.IsSuccess()) {
                // ✅ 注册到 ResourceManager
                resMgr.RegisterTexture("wood_texture", result.resource);
                Logger::GetInstance().Info("Wood texture loaded");
            }
        }
    );
    
    // 2. 创建材质并设置纹理
    auto mat = std::make_shared<Material>();
    mat->SetName("wood_material");
    mat->SetShader(ShaderCache::GetInstance().GetShader("phong"));
    
    // 尝试从ResourceManager获取纹理
    auto woodTexture = resMgr.GetTexture("wood_texture");
    if (woodTexture) {
        mat->SetTexture("diffuse", woodTexture);
    }
    
    // 3. 注册材质
    resMgr.RegisterMaterial("wood_material", mat);
    
    // 4. 创建实体
    auto entity = m_world->CreateEntity({.name = "WoodenBox"});
    
    TransformComponent transform;
    m_world->AddComponent(entity, transform);
    
    MeshRenderComponent meshComp;
    meshComp.meshName = "models/box.obj";
    meshComp.materialName = "wood_material";  // 引用材质名称
    // 或者直接设置：meshComp.material = mat;
    meshComp.visible = true;
    
    m_world->AddComponent(entity, meshComp);
}
```

### 场景 3: 使用纹理覆盖（textureOverrides）

```cpp
void CreateModelWithTextureOverrides() {
    auto entity = m_world->CreateEntity({.name = "TexturedModel"});
    
    // ... 添加 Transform ...
    
    MeshRenderComponent meshComp;
    meshComp.meshName = "models/character.obj";
    meshComp.materialName = "phong";  // 使用预注册的phong材质
    
    // ✅ 添加纹理覆盖（会异步加载）
    meshComp.textureOverrides["diffuse"] = "textures/character_diffuse.png";
    meshComp.textureOverrides["normal"] = "textures/character_normal.png";
    
    // 可选：配置纹理设置
    meshComp.textureSettings["diffuse"].generateMipmaps = true;
    meshComp.textureSettings["normal"].generateMipmaps = true;
    
    meshComp.visible = true;
    m_world->AddComponent(entity, meshComp);
    
    // ResourceLoadingSystem 会自动：
    // 1. 异步加载纹理
    // 2. 注册到 ResourceManager
    // 3. 应用到材质
}
```

---

## ⚠️ 常见错误和解决方案

### 错误 1: 材质未找到

**症状**:
```
[ResourceLoadingSystem] Material not found in ResourceManager: myMaterial
```

**原因**: 材质未被预注册

**解决方案**:
```cpp
// ✅ 方案A：在应用启动时预注册
void Application::PreloadResources() {
    auto mat = std::make_shared<Material>();
    mat->SetName("myMaterial");
    // ... 配置材质 ...
    ResourceManager::GetInstance().RegisterMaterial("myMaterial", mat);
}

// ✅ 方案B：使用默认材质
MeshRenderComponent meshComp;
meshComp.materialName = "default";  // 使用预注册的默认材质
```

### 错误 2: 着色器未找到

**症状**:
```
[ResourceLoadingSystem] Shader not found in ShaderCache: myShader
```

**原因**: 着色器未被预加载

**解决方案**:
```cpp
// ✅ 在应用启动时预加载着色器
void Application::PreloadResources() {
    auto& shaderCache = ShaderCache::GetInstance();
    
    shaderCache.LoadShader("myShader", 
                          "shaders/my.vert", 
                          "shaders/my.frag");
}

// 或者，在MeshRenderComponent中指定着色器路径（动态加载）
MeshRenderComponent meshComp;
meshComp.shaderName = "myShader";
meshComp.shaderVertPath = "shaders/my.vert";
meshComp.shaderFragPath = "shaders/my.frag";
```

### 错误 3: 纹理加载失败

**症状**:
```
[ResourceLoadingSystem] Failed to load texture: textures/missing.png
```

**原因**: 文件不存在或路径错误

**解决方案**:
```cpp
// ✅ 检查文件是否存在
if (!FileExists("textures/myTexture.png")) {
    Logger::GetInstance().Warning("Texture file not found, using fallback");
    // 使用默认纹理或纯色材质
}

// ✅ 使用相对于项目根目录的路径
meshComp.textureOverrides["diffuse"] = "textures/myTexture.png";
// 而不是绝对路径: "C:/absolute/path/myTexture.png"
```

---

## 📊 性能优化建议

### 1. 批量预加载

在加载界面一次性加载所有资源：

```cpp
void Application::ShowLoadingScreen() {
    // 显示加载界面
    
    size_t totalResources = 10;
    size_t loadedResources = 0;
    
    auto& asyncLoader = AsyncResourceLoader::GetInstance();
    
    // 批量提交加载任务
    for (const auto& [name, path] : g_resourceList) {
        asyncLoader.LoadMeshAsync(path, name, 
            [&loadedResources, totalResources](const MeshLoadResult& result) {
                loadedResources++;
                float progress = (float)loadedResources / totalResources;
                UpdateLoadingBar(progress);
            });
    }
    
    // 等待所有任务完成
    while (loadedResources < totalResources) {
        asyncLoader.ProcessCompletedTasks(100);
        SDL_Delay(16);
    }
}
```

### 2. 资源池化

对于频繁创建销毁的材质，使用对象池：

```cpp
class MaterialPool {
public:
    std::shared_ptr<Material> Acquire(const std::string& shaderName) {
        // 从池中获取或创建新材质
        auto mat = m_pool.empty() ? 
                   std::make_shared<Material>() : 
                   m_pool.back(); m_pool.pop_back();
        
        mat->SetShader(ShaderCache::GetInstance().GetShader(shaderName));
        return mat;
    }
    
    void Release(std::shared_ptr<Material> mat) {
        // 重置材质状态
        mat->ResetToDefaults();
        m_pool.push_back(mat);
    }
    
private:
    std::vector<std::shared_ptr<Material>> m_pool;
};
```

### 3. 定期清理未使用资源

```cpp
// 在 World::Update 中（或使用 ResourceCleanupSystem）
void World::Update(float deltaTime) {
    // ... 正常更新 ...
    
    // 每60帧（约1秒）清理一次
    static uint32_t frameCounter = 0;
    if (++frameCounter % 60 == 0) {
        auto& resMgr = ResourceManager::GetInstance();
        size_t cleaned = resMgr.CleanupUnused(60);
        
        if (cleaned > 0) {
            Logger::GetInstance().DebugFormat("Cleaned %zu unused resources", cleaned);
        }
    }
}
```

---

## 🎓 总结

### 关键要点

1. **预注册资源** - 在应用启动时预加载和注册常用资源
2. **统一管理** - 所有资源通过 ResourceManager 统一管理
3. **检查后注册** - 注册前先检查资源是否已存在
4. **使用materialName** - 优先使用名称引用，而不是直接持有shared_ptr
5. **利用MaterialOverride** - 实现每个实体不同外观而不修改共享材质
6. **定期清理** - 使用 ResourceCleanupSystem 或手动清理未使用资源

### 检查清单

在创建ECS应用时，确保：

- [ ] 实现了 `PreloadResources()` 函数
- [ ] 所有着色器都已预加载到 ShaderCache
- [ ] 常用材质已注册到 ResourceManager
- [ ] 使用 `materialName` 而不是直接传递 Material 指针（除非必要）
- [ ] 在 `World::Update()` 中调用了 `ResourceManager::BeginFrame()`（自动）
- [ ] 启用了 ResourceCleanupSystem 或手动清理资源
- [ ] 避免在热路径（如Update循环）中注册资源

---

## 📚 相关文档

- [ECS安全性审查报告](ECS_RESOURCE_MANAGER_SAFETY_REVIEW.md)
- [资源所有权指南](RESOURCE_OWNERSHIP_GUIDE.md)
- [ECS快速开始](ECS_QUICK_START.md)
- [ResourceManager API](api/ResourceManager.md)

---

**最后更新**: 2025-11-05  
**维护者**: Linductor


