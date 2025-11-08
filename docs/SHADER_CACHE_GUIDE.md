# 着色器缓存系统使用指南

[返回文档首页](README.md)

---

## 简介

着色器缓存系统 (`ShaderCache`) 提供了统一的着色器资源管理，支持自动缓存、引用计数和热重载功能。

---

## 基本使用

### 1. 获取单例实例

```cpp
#include "render/shader_cache.h"

using namespace Render;

ShaderCache& cache = ShaderCache::GetInstance();
```

### 2. 加载着色器

#### 从文件加载（推荐）

```cpp
// 加载顶点和片段着色器
auto shader = cache.LoadShader(
    "MyShader",                     // 着色器名称（缓存键）
    "shaders/my_shader.vert",       // 顶点着色器路径
    "shaders/my_shader.frag"        // 片段着色器路径
);

// 加载包含几何着色器的着色器
auto geoShader = cache.LoadShader(
    "GeometryShader",
    "shaders/geo.vert",
    "shaders/geo.frag",
    "shaders/geo.geom"              // 几何着色器路径（可选）
);
```

#### 从源码字符串加载

```cpp
std::string vertexSource = R"(
    #version 450 core
    layout (location = 0) in vec3 aPos;
    void main() {
        gl_Position = vec4(aPos, 1.0);
    }
)";

std::string fragmentSource = R"(
    #version 450 core
    out vec4 FragColor;
    void main() {
        FragColor = vec4(1.0, 0.5, 0.2, 1.0);
    }
)";

auto shader = cache.LoadShaderFromSource(
    "RuntimeShader",
    vertexSource,
    fragmentSource
);
```

### 3. 使用着色器

```cpp
if (shader && shader->IsValid()) {
    shader->Use();
    
    // 设置 uniform
    shader->GetUniformManager()->SetMatrix4("projection", projMatrix);
    shader->GetUniformManager()->SetVector3("lightPos", lightPosition);
    
    // 渲染...
    
    shader->Unuse();
}
```

---

## 高级功能

### 1. 着色器预编译

在程序启动时批量加载着色器，提高运行时性能：

```cpp
// 定义着色器列表
std::vector<std::tuple<std::string, std::string, std::string, std::string>> shaderList = {
    // {名称, 顶点路径, 片段路径, 几何路径}
    {"Basic", "shaders/basic.vert", "shaders/basic.frag", ""},
    {"Lit", "shaders/lit.vert", "shaders/lit.frag", ""},
    {"Particle", "shaders/particle.vert", "shaders/particle.frag", "shaders/particle.geom"},
};

// 预编译
size_t loaded = cache.PrecompileShaders(shaderList);
LOG_INFO("Precompiled " + std::to_string(loaded) + " shaders");
```

### 2. 热重载

支持运行时重新加载着色器，便于开发和调试：

```cpp
// 重载单个着色器
if (cache.ReloadShader("MyShader")) {
    LOG_INFO("Shader reloaded successfully");
}

// 重载所有缓存的着色器
cache.ReloadAll();
```

**使用场景：**
- 着色器开发和调试
- 实时效果调整
- 不重启程序更新着色器

### 3. 获取已缓存的着色器

```cpp
// 获取着色器（不会重新加载）
auto shader = cache.GetShader("MyShader");

if (!shader) {
    LOG_WARNING("Shader not found in cache");
}
```

### 4. 引用计数管理

```cpp
// 查询引用计数
long refCount = cache.GetReferenceCount("MyShader");
LOG_INFO("Shader reference count: " + std::to_string(refCount));

// 注意：只要有 shared_ptr 指向着色器，它就不会被释放
{
    auto shader1 = cache.GetShader("MyShader"); // refCount = 2 (cache + shader1)
    auto shader2 = cache.GetShader("MyShader"); // refCount = 3
    // shader1 和 shader2 离开作用域后，refCount = 1
}
```

### 5. 移除着色器

```cpp
// 从缓存中移除（但不会立即销毁，等待引用计数归零）
cache.RemoveShader("MyShader");
```

### 6. 清空缓存

```cpp
// 清空所有着色器缓存
cache.Clear();
```

---

## 完整示例

### 基础应用程序

```cpp
#include "render/renderer.h"
#include "render/shader_cache.h"
#include "render/logger.h"

int main() {
    // 初始化渲染器
    Renderer* renderer = Renderer::Create();
    renderer->Initialize("My App", 1280, 720);
    
    // 获取着色器缓存
    ShaderCache& shaderCache = ShaderCache::GetInstance();
    
    // 预编译着色器
    std::vector<std::tuple<std::string, std::string, std::string, std::string>> shaders = {
        {"Basic", "shaders/basic.vert", "shaders/basic.frag", ""},
        {"Lit", "shaders/lit.vert", "shaders/lit.frag", ""},
    };
    shaderCache.PrecompileShaders(shaders);
    
    // 获取着色器
    auto basicShader = shaderCache.GetShader("Basic");
    auto litShader = shaderCache.GetShader("Lit");
    
    // 主循环
    bool running = true;
    while (running) {
        // 事件处理
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            
            // 按 R 重载着色器
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_R) {
                shaderCache.ReloadAll();
            }
        }
        
        // 渲染
        renderer->BeginFrame();
        renderer->Clear();
        
        basicShader->Use();
        // 渲染基础物体...
        basicShader->Unuse();
        
        litShader->Use();
        // 渲染光照物体...
        litShader->Unuse();
        
        renderer->EndFrame();
        renderer->Present();
    }
    
    // 清理
    shaderCache.Clear();
    Renderer::Destroy(renderer);
    
    return 0;
}
```

### 着色器管理器封装

如果您需要更高级的管理，可以在 `ShaderCache` 基础上封装：

```cpp
class ShaderManager {
public:
    static void Initialize() {
        ShaderCache& cache = ShaderCache::GetInstance();
        
        // 预编译所有游戏着色器
        std::vector<std::tuple<std::string, std::string, std::string, std::string>> shaders = {
            {"UI", "shaders/ui.vert", "shaders/ui.frag", ""},
            {"Terrain", "shaders/terrain.vert", "shaders/terrain.frag", ""},
            {"Water", "shaders/water.vert", "shaders/water.frag", ""},
        };
        cache.PrecompileShaders(shaders);
    }
    
    static std::shared_ptr<Shader> GetUIShader() {
        return ShaderCache::GetInstance().GetShader("UI");
    }
    
    static std::shared_ptr<Shader> GetTerrainShader() {
        return ShaderCache::GetInstance().GetShader("Terrain");
    }
    
    static std::shared_ptr<Shader> GetWaterShader() {
        return ShaderCache::GetInstance().GetShader("Water");
    }
    
    static void ReloadAll() {
        ShaderCache::GetInstance().ReloadAll();
    }
};
```

---

## 最佳实践

### 1. 命名约定

```cpp
// 使用描述性名称
cache.LoadShader("UIButton", ...);
cache.LoadShader("TerrainDetail", ...);
cache.LoadShader("CharacterSkin", ...);

// 避免通用名称
cache.LoadShader("Shader1", ...);  // ❌ 不好
cache.LoadShader("Test", ...);      // ❌ 不好
```

### 2. 预编译策略

```cpp
// 启动时加载常用着色器
void LoadCoreShaders() {
    auto& cache = ShaderCache::GetInstance();
    // 加载核心着色器
}

// 场景切换时加载场景特定着色器
void LoadSceneShaders(const std::string& sceneName) {
    auto& cache = ShaderCache::GetInstance();
    // 加载场景特定着色器
}
```

### 3. 错误处理

```cpp
auto shader = cache.LoadShader("MyShader", "vertex.vert", "fragment.frag");
if (!shader || !shader->IsValid()) {
    LOG_ERROR("Failed to load shader");
    // 使用后备着色器
    shader = cache.GetShader("FallbackShader");
}
```

### 4. 调试技巧

```cpp
// 在开发模式启用热重载
#ifdef _DEBUG
    if (keyPressed(KEY_F5)) {
        cache.ReloadAll();
        LOG_INFO("Shaders reloaded");
    }
#endif

// 定期打印统计信息
#ifdef _DEBUG
    cache.PrintStatistics();
#endif
```

---

## 性能提示

1. **预编译**: 在加载界面或启动时预编译所有着色器
2. **避免频繁重载**: 重载会重新编译，比较耗时
3. **共享指针**: 使用 `shared_ptr` 避免不必要的拷贝
4. **缓存键**: 使用有意义的名称，便于管理和调试

---

## 故障排查

### 问题：着色器加载失败

```cpp
// 检查文件路径
if (!shader) {
    LOG_ERROR("Shader load failed. Check file paths:");
    LOG_ERROR("  Vertex: shaders/my_shader.vert");
    LOG_ERROR("  Fragment: shaders/my_shader.frag");
}
```

### 问题：重载不生效

```cpp
// 确保文件已修改并保存
bool success = cache.ReloadShader("MyShader");
if (!success) {
    LOG_WARNING("Reload failed. Shader may not have source paths.");
}
```

### 问题：内存泄漏

```cpp
// 定期检查引用计数
long refCount = cache.GetReferenceCount("MyShader");
if (refCount > expected) {
    LOG_WARNING("Unexpected high reference count: " + std::to_string(refCount));
    // 检查是否有忘记释放的 shared_ptr
}
```

---

## API 参考

### ShaderCache 类

| 方法 | 说明 |
|-----|------|
| `GetInstance()` | 获取单例实例 |
| `LoadShader()` | 加载或获取着色器 |
| `LoadShaderFromSource()` | 从源码加载着色器 |
| `GetShader()` | 获取已缓存的着色器 |
| `ReloadShader()` | 重载指定着色器 |
| `ReloadAll()` | 重载所有着色器 |
| `RemoveShader()` | 移除着色器 |
| `Clear()` | 清空缓存 |
| `GetShaderCount()` | 获取缓存数量 |
| `GetReferenceCount()` | 获取引用计数 |
| `PrintStatistics()` | 打印统计信息 |
| `PrecompileShaders()` | 预编译着色器列表 |

---

## 相关文档

- [材质系统指南](MATERIAL_SYSTEM.md)
- [开发指南](DEVELOPMENT_GUIDE.md)
- [API 参考](API_REFERENCE.md)

---

[上一篇：开发指南](DEVELOPMENT_GUIDE.md) | [下一篇：API 参考](API_REFERENCE.md)

