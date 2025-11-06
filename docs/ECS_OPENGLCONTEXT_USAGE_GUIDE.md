# ECS中OpenGLContext使用最佳实践指南

**目标读者**: 使用ECS系统的开发者  
**前置知识**: 了解ECS基本概念、OpenGL基础  
**相关文档**: [OpenGLContext API](api/OpenGLContext.md) | [ECS API](api/ECS.md) | [OpenGLContext安全审查](ECS_OPENGLCONTEXT_SAFETY_REVIEW.md)

---

## 📋 概述

本文档说明在ECS系统中如何正确使用OpenGLContext，包括初始化、窗口管理、线程安全等最佳实践。

---

## 🎯 核心原则

### 原则 1: 先初始化Renderer，再注册ECS系统

OpenGLContext由Renderer管理，必须在注册依赖它的ECS系统之前初始化。

**正确的初始化顺序**:
```cpp
// 1. ✅ 先初始化Renderer（内部会初始化OpenGLContext）
auto renderer = std::make_unique<Renderer>();
if (!renderer->Initialize("My App", 1280, 720)) {
    return false;
}

// 2. ✅ 检查OpenGL扩展支持（可选但推荐）
auto context = renderer->GetContext();
if (!GLExtensionChecker::CheckRequiredExtensions(context.get())) {
    LOG_ERROR("Hardware does not meet minimum requirements");
    return false;
}

// 3. ✅ 初始化ECS World
auto world = std::make_shared<World>();
world->Initialize();

// 4. ✅ 注册依赖Renderer的系统（此时Renderer已初始化）
world->RegisterSystem<WindowSystem>(renderer.get());
world->RegisterSystem<MeshRenderSystem>(renderer.get());
// ...
```

**错误示例** ❌:
```cpp
// ❌ 错误：先创建World和系统，再初始化Renderer
auto renderer = std::make_unique<Renderer>();
auto world = std::make_shared<World>();
world->Initialize();
world->RegisterSystem<WindowSystem>(renderer.get());  // ❌ 此时Renderer未初始化！

renderer->Initialize("My App", 1280, 720);  // 太晚了
```

---

### 原则 2: 使用事件驱动的窗口大小变化处理

OpenGLContext提供了窗口大小变化回调机制，应该使用它而不是轮询检测。

**正确示例** ✅:
```cpp
void WindowSystem::OnCreate(World* world) {
    System::OnCreate(world);
    
    // ✅ 使用OpenGLContext的窗口大小变化回调机制
    auto context = m_renderer->GetContext();
    if (context && context->IsInitialized()) {
        context->AddResizeCallback([this](int width, int height) {
            this->OnWindowResized(width, height);
        });
    }
}

void WindowSystem::OnWindowResized(int width, int height) {
    // 事件驱动：只在窗口大小实际改变时才调用
    UpdateCameraAspectRatios(width, height);
    UpdateViewport(width, height);
}
```

**错误示例** ❌:
```cpp
void WindowSystem::Update(float deltaTime) {
    // ❌ 每帧轮询检测窗口大小变化（低效）
    int currentWidth = m_renderer->GetWidth();
    int currentHeight = m_renderer->GetHeight();
    
    if (currentWidth != m_lastWidth || currentHeight != m_lastHeight) {
        UpdateCameraAspectRatios();
        UpdateViewport();
        m_lastWidth = currentWidth;
        m_lastHeight = currentHeight;
    }
}
```

**优势**:
- ✅ 零轮询开销（不需要每帧检查）
- ✅ 不漏掉快速变化（即使在两帧之间变化多次也能捕获）
- ✅ 符合现代事件驱动设计

---

### 原则 3: 始终检查Context是否有效和已初始化

在使用OpenGLContext之前，必须检查它是否存在且已初始化。

**正确示例** ✅:
```cpp
void MySystem::OnCreate(World* world) {
    System::OnCreate(world);
    
    // ✅ 完整检查
    if (!m_renderer) {
        LOG_ERROR("[MySystem] Renderer is null");
        return;
    }
    
    if (!m_renderer->IsInitialized()) {
        LOG_ERROR("[MySystem] Renderer not initialized");
        return;
    }
    
    auto context = m_renderer->GetContext();
    if (!context) {
        LOG_ERROR("[MySystem] OpenGLContext is null");
        return;
    }
    
    if (!context->IsInitialized()) {
        LOG_ERROR("[MySystem] OpenGLContext not initialized");
        return;
    }
    
    // 现在可以安全使用context
    int width = context->GetWidth();
    int height = context->GetHeight();
}
```

---

### 原则 4: 在应用初始化时检查OpenGL扩展支持

如果引擎依赖特定的OpenGL扩展，应该在初始化时检查。

**示例**:
```cpp
#include "render/gl_extension_checker.h"

bool Application::Initialize() {
    // 1. 初始化Renderer
    m_renderer = std::make_unique<Renderer>();
    if (!m_renderer->Initialize("My App", 1280, 720)) {
        return false;
    }
    
    // 2. ✅ 检查OpenGL扩展支持
    auto context = m_renderer->GetContext();
    if (!GLExtensionChecker::CheckRequiredExtensions(context.get())) {
        LOG_ERROR("Hardware does not meet minimum requirements");
        return false;
    }
    
    // 检查推荐的扩展（警告级别）
    GLExtensionChecker::CheckRecommendedExtensions(context.get());
    
    // 3. 继续初始化...
    m_world = std::make_shared<World>();
    // ...
    
    return true;
}
```

**自定义扩展检查**:
如果需要检查特定的扩展，可以在`gl_extension_checker.cpp`中修改：

```cpp
std::vector<std::string> GLExtensionChecker::GetRequiredExtensions() {
    return {
        "GL_ARB_direct_state_access",        // DSA支持
        "GL_ARB_shader_storage_buffer_object", // SSBO支持
    };
}

std::vector<std::string> GLExtensionChecker::GetRecommendedExtensions() {
    return {
        "GL_ARB_bindless_texture",  // 无绑定纹理（性能优化）
        "GL_ARB_buffer_storage",    // 持久映射缓冲区
    };
}
```

---

## 📚 完整示例

### 示例 1: 标准ECS应用的OpenGLContext使用

```cpp
#include <SDL3/SDL.h>
#include "render/renderer.h"
#include "render/gl_extension_checker.h"
#include "render/ecs/world.h"
#include "render/ecs/systems.h"
#include "render/ecs/components.h"
#include "render/shader_cache.h"
#include "render/resource_manager.h"

using namespace Render;
using namespace Render::ECS;

class MyApplication {
public:
    bool Initialize() {
        // ============================================================
        // 1. 初始化渲染器（包含OpenGLContext）
        // ============================================================
        m_renderer = std::make_unique<Renderer>();
        if (!m_renderer->Initialize("My Application", 1920, 1080)) {
            Logger::GetInstance().Error("Failed to initialize renderer");
            return false;
        }
        
        Logger::GetInstance().Info("✓ Renderer initialized");
        
        // ============================================================
        // 2. 检查OpenGL扩展支持
        // ============================================================
        auto context = m_renderer->GetContext();
        if (!context) {
            Logger::GetInstance().Error("OpenGLContext is null");
            return false;
        }
        
        // 输出OpenGL信息
        Logger::GetInstance().InfoFormat("OpenGL Version: %s", context->GetGLVersion().c_str());
        Logger::GetInstance().InfoFormat("GPU: %s", context->GetGPUInfo().c_str());
        
        // 检查必需的扩展
        if (!GLExtensionChecker::CheckRequiredExtensions(context.get())) {
            Logger::GetInstance().Error("Hardware does not meet minimum requirements");
            return false;
        }
        
        // 检查推荐的扩展（警告级别，不影响运行）
        GLExtensionChecker::CheckRecommendedExtensions(context.get());
        
        // ============================================================
        // 3. 设置渲染状态
        // ============================================================
        auto renderState = m_renderer->GetRenderState();
        renderState->SetDepthTest(true);
        renderState->SetCullFace(CullFace::Back);
        renderState->SetBlendMode(BlendMode::Alpha);
        renderState->SetClearColor(Color(0.1f, 0.1f, 0.15f, 1.0f));
        
        // ============================================================
        // 4. 预加载资源
        // ============================================================
        if (!PreloadResources()) {
            return false;
        }
        
        // ============================================================
        // 5. 初始化ECS World
        // ============================================================
        m_world = std::make_shared<World>();
        m_world->Initialize();
        Logger::GetInstance().Info("✓ ECS World initialized");
        
        // ============================================================
        // 6. 注册组件
        // ============================================================
        m_world->RegisterComponent<TransformComponent>();
        m_world->RegisterComponent<MeshRenderComponent>();
        m_world->RegisterComponent<CameraComponent>();
        m_world->RegisterComponent<LightComponent>();
        
        // ============================================================
        // 7. 注册系统（注意：Renderer已经初始化）
        // ============================================================
        m_world->RegisterSystem<WindowSystem>(m_renderer.get());      // ✅ 使用回调机制
        m_world->RegisterSystem<CameraSystem>();
        m_world->RegisterSystem<TransformSystem>();
        m_world->RegisterSystem<ResourceLoadingSystem>(&m_asyncLoader);
        m_world->RegisterSystem<LightSystem>(m_renderer.get());
        m_world->RegisterSystem<UniformSystem>(m_renderer.get());
        m_world->RegisterSystem<MeshRenderSystem>(m_renderer.get());
        
        // ============================================================
        // 8. 后初始化
        // ============================================================
        m_world->PostInitialize();
        
        // ============================================================
        // 9. 创建场景
        // ============================================================
        CreateScene();
        
        Logger::GetInstance().Info("✓ Application initialized successfully");
        return true;
    }
    
    bool PreloadResources() {
        auto& resMgr = ResourceManager::GetInstance();
        auto& shaderCache = ShaderCache::GetInstance();
        
        // 加载着色器
        auto shader = shaderCache.LoadShader("basic", 
                                            "shaders/basic.vert", 
                                            "shaders/basic.frag");
        if (!shader) {
            Logger::GetInstance().Error("Failed to load shader");
            return false;
        }
        
        // 创建默认材质
        auto defaultMat = std::make_shared<Material>();
        defaultMat->SetName("default");
        defaultMat->SetShader(shader);
        defaultMat->SetDiffuseColor(Color(0.8f, 0.8f, 0.8f, 1.0f));
        
        resMgr.RegisterMaterial("default", defaultMat);
        
        Logger::GetInstance().Info("✓ Resources preloaded");
        return true;
    }
    
    void CreateScene() {
        // 创建相机
        auto cameraEntity = m_world->CreateEntity({.name = "MainCamera"});
        
        auto camera = std::make_shared<Camera>();
        camera->SetPerspective(60.0f, 1920.0f / 1080.0f, 0.1f, 1000.0f);
        
        TransformComponent cameraTransform;
        cameraTransform.SetPosition(Vector3(0, 2, 8));
        cameraTransform.LookAt(Vector3(0, 0, 0));
        m_world->AddComponent(cameraEntity, cameraTransform);
        
        CameraComponent cameraComp;
        cameraComp.camera = camera;
        cameraComp.active = true;
        m_world->AddComponent(cameraEntity, cameraComp);
        
        // 创建光源
        auto lightEntity = m_world->CreateEntity({.name = "MainLight"});
        
        TransformComponent lightTransform;
        lightTransform.SetPosition(Vector3(5, 10, 5));
        m_world->AddComponent(lightEntity, lightTransform);
        
        LightComponent lightComp;
        lightComp.color = Color(1.0f, 1.0f, 1.0f, 1.0f);
        lightComp.intensity = 1.0f;
        lightComp.enabled = true;
        m_world->AddComponent(lightEntity, lightComp);
        
        Logger::GetInstance().Info("✓ Scene created");
    }
    
    void Update(float deltaTime) {
        // ECS更新
        m_world->Update(deltaTime);
        
        // 渲染
        m_renderer->BeginFrame();
        m_renderer->Clear();
        m_renderer->FlushRenderQueue();
        m_renderer->EndFrame();
        m_renderer->Present();
    }
    
    void Shutdown() {
        m_world->Shutdown();
        m_renderer->Shutdown();
    }
    
private:
    std::unique_ptr<Renderer> m_renderer;
    std::shared_ptr<World> m_world;
    AsyncResourceLoader m_asyncLoader;
};

int main(int argc, char* argv[]) {
    MyApplication app;
    
    if (!app.Initialize()) {
        return -1;
    }
    
    // 主循环
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }
        
        app.Update(0.016f);  // 60 FPS
    }
    
    app.Shutdown();
    return 0;
}
```

---

## ⚠️ 常见错误和解决方案

### 错误 1: Renderer未初始化就注册系统

**症状**:
```
[WindowSystem] OpenGLContext not initialized
```

**原因**: 在调用`Renderer::Initialize()`之前注册了依赖它的系统

**解决方案**:
```cpp
// ✅ 正确顺序
renderer->Initialize(...);  // 先初始化
world->RegisterSystem<WindowSystem>(renderer.get());  // 再注册系统

// ❌ 错误顺序
world->RegisterSystem<WindowSystem>(renderer.get());  // 注册系统
renderer->Initialize(...);  // 太晚了
```

---

### 错误 2: 窗口大小变化未响应

**症状**: 改变窗口大小后，相机宽高比没有更新，画面变形

**原因**: WindowSystem未正确使用回调机制，或回调未被触发

**解决方案**:
```cpp
// ✅ 确保WindowSystem使用回调机制
void WindowSystem::OnCreate(World* world) {
    auto context = m_renderer->GetContext();
    if (context && context->IsInitialized()) {
        context->AddResizeCallback([this](int width, int height) {
            this->OnWindowResized(width, height);
        });
    }
}

// ✅ 在回调中更新相机和视口
void WindowSystem::OnWindowResized(int width, int height) {
    UpdateCameraAspectRatios(width, height);
    UpdateViewport(width, height);
}
```

---

### 错误 3: Context空指针崩溃

**症状**: 程序在调用`context->GetWidth()`时崩溃

**原因**: 未检查Context是否为空

**解决方案**:
```cpp
// ✅ 完整的空指针检查
auto context = m_renderer->GetContext();
if (!context) {
    LOG_ERROR("OpenGLContext is null");
    return;
}

if (!context->IsInitialized()) {
    LOG_ERROR("OpenGLContext not initialized");
    return;
}

// 现在可以安全使用
int width = context->GetWidth();
```

---

### 错误 4: 跨线程调用OpenGL

**症状**: 程序在非主线程调用OpenGL函数时崩溃或出现奇怪的行为

**原因**: OpenGL调用必须在创建上下文的线程（通常是主线程）中执行

**解决方案**:
```cpp
// ✅ 在所有OpenGL调用前添加线程检查
void MyRenderFunction() {
    GL_THREAD_CHECK();  // 确保在OpenGL线程
    
    // OpenGL调用
    glClear(GL_COLOR_BUFFER_BIT);
    // ...
}
```

**注意**: `GL_THREAD_CHECK()`需要在编译选项中启用（通常在Debug模式下启用）

---

## 📊 性能优化建议

### 1. 使用回调机制而非轮询

```cpp
// ✅ 高效：事件驱动
context->AddResizeCallback([this](int w, int h) {
    OnWindowResized(w, h);
});

// ❌ 低效：每帧轮询
void Update() {
    if (context->GetWidth() != m_lastWidth) {
        // ...
    }
}
```

### 2. 缓存Context指针避免重复获取

```cpp
// ✅ 在OnCreate中缓存Context
void MySystem::OnCreate(World* world) {
    m_context = m_renderer->GetContext();
}

// ✅ 直接使用缓存的指针
void MySystem::Update(float deltaTime) {
    if (m_context && m_context->IsInitialized()) {
        int width = m_context->GetWidth();
        // ...
    }
}
```

---

## 🎓 总结

### 关键要点

1. **初始化顺序** - 先Renderer，再ECS系统
2. **事件驱动** - 使用窗口大小变化回调，不轮询
3. **完整检查** - 始终检查Context是否有效和已初始化
4. **扩展检查** - 在初始化时验证硬件支持
5. **线程安全** - 使用GL_THREAD_CHECK确保在OpenGL线程

### 检查清单

在创建ECS应用时，确保：

- [ ] Renderer在注册系统之前已初始化
- [ ] 使用`GLExtensionChecker`检查OpenGL扩展支持
- [ ] WindowSystem使用回调机制而非轮询
- [ ] 所有使用Context的地方都检查了有效性
- [ ] 在Debug模式下启用了GL_THREAD_CHECK
- [ ] 输出了OpenGL版本和GPU信息用于调试

---

## 📚 相关文档

- [OpenGLContext安全审查报告](ECS_OPENGLCONTEXT_SAFETY_REVIEW.md)
- [OpenGLContext API文档](api/OpenGLContext.md)
- [GLThreadChecker API文档](api/GLThreadChecker.md)
- [ECS API文档](api/ECS.md)

---

**最后更新**: 2025-11-05  
**维护者**: RenderEngine开发团队

