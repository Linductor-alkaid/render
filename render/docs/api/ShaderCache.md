# ShaderCache API 参考

[返回 API 首页](README.md)

---

## 概述

`ShaderCache` 是着色器缓存管理器（单例模式），提供统一的着色器资源管理，支持自动缓存、引用计数和热重载功能。

**头文件**: `render/shader_cache.h`  
**命名空间**: `Render`

---

## 类定义

```cpp
class ShaderCache {
public:
    static ShaderCache& GetInstance();
    
    std::shared_ptr<Shader> LoadShader(const std::string& name,
                                       const std::string& vertexPath,
                                       const std::string& fragmentPath,
                                       const std::string& geometryPath = "");
    
    std::shared_ptr<Shader> GetShader(const std::string& name);
    bool ReloadShader(const std::string& name);
    void ReloadAll();
    
    // ... 更多方法见下文
};
```

---

## 单例访问

### GetInstance

获取单例实例。

```cpp
static ShaderCache& GetInstance();
```

**返回值**: `ShaderCache` 引用

**示例**:
```cpp
ShaderCache& cache = ShaderCache::GetInstance();
```

---

## 加载和获取

### LoadShader

加载或获取着色器（自动缓存）。

```cpp
std::shared_ptr<Shader> LoadShader(const std::string& name,
                                   const std::string& vertexPath,
                                   const std::string& fragmentPath,
                                   const std::string& geometryPath = "");
```

**参数**:
- `name` - 着色器名称（缓存键）
- `vertexPath` - 顶点着色器路径
- `fragmentPath` - 片段着色器路径
- `geometryPath` - 几何着色器路径（可选）

**返回值**: 着色器 `shared_ptr`，失败返回 `nullptr`

**特性**:
- 如果着色器已缓存，直接返回（不重复加载）
- 自动引用计数管理
- 线程安全（单例）

**示例**:
```cpp
auto& cache = ShaderCache::GetInstance();

// 第一次加载（编译）
auto shader = cache.LoadShader("Basic", "shaders/basic.vert", "shaders/basic.frag");

// 再次获取（从缓存）
auto sameShader = cache.LoadShader("Basic", "shaders/basic.vert", "shaders/basic.frag");

// shader 和 sameShader 指向同一个对象
assert(shader.get() == sameShader.get());
```

---

### LoadShaderFromSource

从源码加载或获取着色器。

```cpp
std::shared_ptr<Shader> LoadShaderFromSource(const std::string& name,
                                              const std::string& vertexSource,
                                              const std::string& fragmentSource,
                                              const std::string& geometrySource = "");
```

**参数**:
- `name` - 着色器名称
- `vertexSource` - 顶点着色器源码
- `fragmentSource` - 片段着色器源码
- `geometrySource` - 几何着色器源码（可选）

**返回值**: 着色器 `shared_ptr`，失败返回 `nullptr`

**示例**:
```cpp
std::string vertSource = "#version 450 core\n...";
std::string fragSource = "#version 450 core\n...";

auto shader = cache.LoadShaderFromSource("Runtime", vertSource, fragSource);
```

---

### GetShader

获取已缓存的着色器。

```cpp
std::shared_ptr<Shader> GetShader(const std::string& name);
```

**参数**:
- `name` - 着色器名称

**返回值**: 着色器 `shared_ptr`，未找到返回 `nullptr`

**说明**: 不会加载新着色器，仅从缓存获取。

**示例**:
```cpp
auto shader = cache.GetShader("Basic");
if (!shader) {
    LOG_WARNING("Shader 'Basic' not found in cache");
}
```

---

## 重载功能

### ReloadShader

重载指定着色器。

```cpp
bool ReloadShader(const std::string& name);
```

**参数**:
- `name` - 着色器名称

**返回值**: 成功返回 `true`，失败返回 `false`

**说明**: 从文件重新编译着色器，所有 `shared_ptr` 引用的着色器都会更新。

**示例**:
```cpp
// 按 R 键重载着色器
if (event.key.key == SDLK_R) {
    if (cache.ReloadShader("Basic")) {
        LOG_INFO("Shader reloaded successfully");
    }
}
```

---

### ReloadAll

重载所有缓存的着色器。

```cpp
void ReloadAll();
```

**说明**: 批量重载，适合开发时快速迭代。

**示例**:
```cpp
// F5 键重载所有着色器
if (event.key.key == SDLK_F5) {
    cache.ReloadAll();
    LOG_INFO("All shaders reloaded");
}
```

---

## 管理功能

### RemoveShader

从缓存中移除着色器。

```cpp
void RemoveShader(const std::string& name);
```

**参数**:
- `name` - 着色器名称

**说明**: 
- 从缓存移除，但不会立即销毁
- 着色器会在所有 `shared_ptr` 释放后自动销毁

**示例**:
```cpp
cache.RemoveShader("OldShader");
```

---

### Clear

清空所有缓存。

```cpp
void Clear();
```

**说明**: 清空缓存，着色器在引用计数归零后销毁。

**示例**:
```cpp
// 场景切换时清空
cache.Clear();
```

---

## 统计和查询

### GetShaderCount

获取缓存中的着色器数量。

```cpp
size_t GetShaderCount() const;
```

**返回值**: 着色器数量

**示例**:
```cpp
size_t count = cache.GetShaderCount();
LOG_INFO("Cached shaders: " + std::to_string(count));
```

---

### GetReferenceCount

获取着色器引用计数。

```cpp
long GetReferenceCount(const std::string& name) const;
```

**参数**:
- `name` - 着色器名称

**返回值**: 引用计数，未找到返回 0

**说明**: 引用计数 = 缓存本身(1) + 外部 `shared_ptr` 数量

**示例**:
```cpp
long refCount = cache.GetReferenceCount("Basic");
LOG_INFO("RefCount: " + std::to_string(refCount));

// refCount = 1: 仅缓存持有
// refCount > 1: 有外部引用
```

---

### PrintStatistics

打印缓存统计信息。

```cpp
void PrintStatistics() const;
```

**输出示例**:
```
========================================
Shader Cache Statistics
========================================
Total shaders in cache: 3
Shader details:
  - Basic (ID: 1, RefCount: 2)
  - Lit (ID: 2, RefCount: 1)
  - Particle (ID: 3, RefCount: 3)
========================================
```

**示例**:
```cpp
// 按 S 键显示统计信息
if (event.key.key == SDLK_S) {
    cache.PrintStatistics();
}
```

---

## 预编译功能

### PrecompileShaders

预编译着色器列表。

```cpp
size_t PrecompileShaders(
    const std::vector<std::tuple<std::string, std::string, std::string, std::string>>& shaderList);
```

**参数**:
- `shaderList` - 着色器定义列表：`{名称, 顶点路径, 片段路径, 几何路径}`

**返回值**: 成功加载的着色器数量

**示例**:
```cpp
// 启动时预编译
std::vector<std::tuple<std::string, std::string, std::string, std::string>> shaders = {
    {"Basic", "shaders/basic.vert", "shaders/basic.frag", ""},
    {"Lit", "shaders/lit.vert", "shaders/lit.frag", ""},
    {"Particle", "shaders/particle.vert", "shaders/particle.frag", "shaders/particle.geom"},
};

size_t loaded = cache.PrecompileShaders(shaders);
LOG_INFO("Precompiled " + std::to_string(loaded) + "/" + std::to_string(shaders.size()) + " shaders");
```

---

## 完整示例

### 基础使用

```cpp
#include "render/shader_cache.h"
#include "render/logger.h"

// 获取缓存实例
ShaderCache& cache = ShaderCache::GetInstance();

// 加载着色器
auto basicShader = cache.LoadShader("Basic", "shaders/basic.vert", "shaders/basic.frag");
auto litShader = cache.LoadShader("Lit", "shaders/lit.vert", "shaders/lit.frag");

// 使用着色器
if (basicShader && basicShader->IsValid()) {
    basicShader->Use();
    // 渲染...
    basicShader->Unuse();
}

// 再次获取（从缓存）
auto sameBasicShader = cache.GetShader("Basic");
```

---

### 预编译和管理

```cpp
// 预编译所有着色器
void PreloadAllShaders() {
    ShaderCache& cache = ShaderCache::GetInstance();
    
    std::vector<std::tuple<std::string, std::string, std::string, std::string>> shaders = {
        {"UI", "shaders/ui.vert", "shaders/ui.frag", ""},
        {"Terrain", "shaders/terrain.vert", "shaders/terrain.frag", ""},
        {"Water", "shaders/water.vert", "shaders/water.frag", ""},
        {"Sky", "shaders/sky.vert", "shaders/sky.frag", ""},
        {"Particle", "shaders/particle.vert", "shaders/particle.frag", "shaders/particle.geom"},
    };
    
    size_t loaded = cache.PrecompileShaders(shaders);
    
    if (loaded < shaders.size()) {
        LOG_WARNING("Some shaders failed to load");
    }
    
    cache.PrintStatistics();
}

int main() {
    // 初始化渲染器...
    
    // 预加载
    PreloadAllShaders();
    
    // 主循环...
    
    // 清理
    ShaderCache::GetInstance().Clear();
}
```

---

### 热重载开发工作流

```cpp
// 开发模式下的着色器热重载
void HandleShaderReload(const SDL_Event& event) {
    ShaderCache& cache = ShaderCache::GetInstance();
    
    if (event.type == SDL_EVENT_KEY_DOWN) {
        switch (event.key.key) {
            case SDLK_F5:
                // F5 重载所有
                LOG_INFO("Reloading all shaders...");
                cache.ReloadAll();
                cache.PrintStatistics();
                break;
                
            case SDLK_F6:
                // F6 重载特定着色器
                if (cache.ReloadShader("CurrentShader")) {
                    LOG_INFO("Current shader reloaded");
                }
                break;
                
            case SDLK_F7:
                // F7 显示统计信息
                cache.PrintStatistics();
                break;
        }
    }
}
```

---

### 着色器管理器封装

```cpp
// 对 ShaderCache 的高级封装
class GameShaderManager {
public:
    static void Initialize() {
        auto& cache = ShaderCache::GetInstance();
        
        // 预编译游戏着色器
        std::vector<std::tuple<std::string, std::string, std::string, std::string>> shaders = {
            {"Character", "shaders/character.vert", "shaders/character.frag", ""},
            {"Environment", "shaders/env.vert", "shaders/env.frag", ""},
            {"Effects", "shaders/effects.vert", "shaders/effects.frag", "shaders/effects.geom"},
        };
        
        cache.PrecompileShaders(shaders);
    }
    
    static std::shared_ptr<Shader> GetCharacterShader() {
        return ShaderCache::GetInstance().GetShader("Character");
    }
    
    static std::shared_ptr<Shader> GetEnvironmentShader() {
        return ShaderCache::GetInstance().GetShader("Environment");
    }
    
    static std::shared_ptr<Shader> GetEffectsShader() {
        return ShaderCache::GetInstance().GetShader("Effects");
    }
    
    static void ReloadAll() {
        ShaderCache::GetInstance().ReloadAll();
    }
    
    static void Shutdown() {
        ShaderCache::GetInstance().Clear();
    }
};

// 使用
GameShaderManager::Initialize();
auto shader = GameShaderManager::GetCharacterShader();
```

---

## 性能和内存

### 引用计数示例

```cpp
ShaderCache& cache = ShaderCache::GetInstance();

// 加载着色器（RefCount = 1，缓存持有）
auto shader1 = cache.LoadShader("Test", "test.vert", "test.frag");
LOG_INFO("RefCount: " + std::to_string(cache.GetReferenceCount("Test"))); // 2

{
    // 创建更多引用（RefCount = 3）
    auto shader2 = cache.GetShader("Test");
    auto shader3 = cache.GetShader("Test");
    LOG_INFO("RefCount: " + std::to_string(cache.GetReferenceCount("Test"))); // 4
    
    // shader2 和 shader3 离开作用域（RefCount = 2）
}

LOG_INFO("RefCount: " + std::to_string(cache.GetReferenceCount("Test"))); // 2

// 从缓存移除（RefCount = 1）
cache.RemoveShader("Test");
LOG_INFO("RefCount: " + std::to_string(cache.GetReferenceCount("Test"))); // 0（已移除）

// shader1 离开作用域时，着色器被销毁
```

---

## 最佳实践

1. **启动预编译**: 在加载界面预编译所有着色器
2. **使用 GetShader**: 获取已缓存的着色器，避免重复加载
3. **开发热重载**: 使用快捷键快速重载着色器
4. **命名规范**: 使用描述性的着色器名称
5. **监控引用**: 定期检查引用计数，避免内存泄漏

---

## 注意事项

1. **单例模式**: 全局唯一，线程安全
2. **shared_ptr**: 自动内存管理，无需手动delete
3. **热重载**: 仅对从文件加载的着色器有效
4. **引用计数**: 缓存本身算一个引用
5. **性能**: 缓存避免重复编译，提升性能

---

## 相关文档

- [Shader API](Shader.md)
- [着色器缓存使用指南](../SHADER_CACHE_GUIDE.md)
- [示例程序: 03_geometry_shader_test](../../examples/03_geometry_shader_test.cpp)

---

[上一篇: Shader](Shader.md) | [下一篇: UniformManager](UniformManager.md)

