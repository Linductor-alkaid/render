# TextureLoader API 参考

[返回 API 首页](README.md)

---

## 概述

`TextureLoader` 是一个单例类，提供纹理加载、缓存和资源管理功能。支持同步和异步加载，自动引用计数，线程安全。

**头文件**: `render/texture_loader.h`  
**命名空间**: `Render`  
**设计模式**: 单例（Singleton）

---

## 类定义

```cpp
class TextureLoader {
public:
    static TextureLoader& GetInstance();
    
    // 同步加载和创建
    TexturePtr LoadTexture(const std::string& name, const std::string& filepath, 
                          bool generateMipmap = true);
    TexturePtr CreateTexture(const std::string& name, const void* data, int width, int height,
                            TextureFormat format = TextureFormat::RGBA, bool generateMipmap = true);
    
    // 异步加载
    std::future<AsyncTextureResult> LoadTextureAsync(const std::string& name,
                                                     const std::string& filepath,
                                                     bool generateMipmap = true);
    
    // 获取和管理
    TexturePtr GetTexture(const std::string& name);
    bool HasTexture(const std::string& name) const;
    bool RemoveTexture(const std::string& name);
    void Clear();
    
    // 统计和信息
    size_t GetTextureCount() const;
    long GetReferenceCount(const std::string& name) const;
    size_t GetTotalMemoryUsage() const;
    void PrintStatistics() const;
    
    // 批量操作
    size_t PreloadTextures(const std::vector<std::tuple<std::string, std::string, bool>>& textureList);
    size_t CleanupUnused();
};
```

---

## 数据结构

### AsyncTextureResult

异步纹理加载结果结构。

```cpp
struct AsyncTextureResult {
    bool success;           // 是否加载成功
    TexturePtr texture;     // 纹理指针
    std::string error;      // 错误信息（如果失败）
};
```

---

## 单例访问

### `GetInstance()`

获取 `TextureLoader` 单例实例。

```cpp
static TextureLoader& GetInstance()
```

**返回值**: `TextureLoader` 引用

**示例**:
```cpp
auto& loader = TextureLoader::GetInstance();
```

---

## 同步加载

### `LoadTexture()`

同步加载纹理（带缓存）。

```cpp
TexturePtr LoadTexture(const std::string& name, const std::string& filepath, 
                      bool generateMipmap = true)
```

**参数**:
- `name` - 纹理名称（用作缓存键）
- `filepath` - 纹理文件路径
- `generateMipmap` - 是否生成 Mipmap（默认 `true`）

**返回值**: 成功返回 `TexturePtr`，失败返回 `nullptr`

**行为**:
- 如果纹理已缓存，直接返回缓存中的纹理（不会重复加载）
- 如果未缓存，加载文件并添加到缓存

**示例**:
```cpp
auto& loader = TextureLoader::GetInstance();

// 首次加载（从文件）
auto texture1 = loader.LoadTexture("my_texture", "textures/test.png", true);

// 再次加载（从缓存）
auto texture2 = loader.LoadTexture("my_texture", "textures/test.png", true);

// texture1 和 texture2 是同一个对象
assert(texture1.get() == texture2.get());
```

---

## 从内存创建

### `CreateTexture()`

从内存数据创建纹理（带缓存）。

```cpp
TexturePtr CreateTexture(const std::string& name, const void* data, int width, int height,
                        TextureFormat format = TextureFormat::RGBA, bool generateMipmap = true)
```

**参数**:
- `name` - 纹理名称（用作缓存键）
- `data` - 纹理数据指针
- `width` - 纹理宽度
- `height` - 纹理高度
- `format` - 纹理格式（默认 `RGBA`）
- `generateMipmap` - 是否生成 Mipmap（默认 `true`）

**返回值**: 成功返回 `TexturePtr`，失败返回 `nullptr`

**示例**:
```cpp
std::vector<unsigned char> checkerboard(256 * 256 * 4);
// ... 生成棋盘格数据 ...

auto& loader = TextureLoader::GetInstance();
auto texture = loader.CreateTexture("checkerboard", 
                                   checkerboard.data(), 
                                   256, 256,
                                   TextureFormat::RGBA, 
                                   true);
```

---

## 异步加载

### `LoadTextureAsync()`

异步加载纹理。

```cpp
std::future<AsyncTextureResult> LoadTextureAsync(const std::string& name,
                                                 const std::string& filepath,
                                                 bool generateMipmap = true)
```

**参数**:
- `name` - 纹理名称
- `filepath` - 纹理文件路径
- `generateMipmap` - 是否生成 Mipmap（默认 `true`）

**返回值**: `std::future<AsyncTextureResult>` 对象

**行为**:
- 如果纹理已缓存，立即返回已缓存的纹理
- 如果未缓存，在后台线程中加载

**示例**:
```cpp
auto& loader = TextureLoader::GetInstance();

// 启动异步加载
auto future = loader.LoadTextureAsync("large_texture", "textures/large.png", true);

// 继续执行其他任务
DoOtherWork();

// 等待加载完成
AsyncTextureResult result = future.get();
if (result.success) {
    TexturePtr texture = result.texture;
    // 使用纹理...
} else {
    LOG_ERROR("加载失败: " + result.error);
}
```

**注意**: 异步加载的纹理需要在主线程（OpenGL 上下文线程）中创建 OpenGL 对象。当前实现是简化版，实际应用中可能需要更复杂的上下文管理。

---

## 获取纹理

### `GetTexture()`

获取已缓存的纹理。

```cpp
TexturePtr GetTexture(const std::string& name)
```

**参数**:
- `name` - 纹理名称

**返回值**: 成功返回 `TexturePtr`，未找到返回 `nullptr`

**示例**:
```cpp
auto& loader = TextureLoader::GetInstance();

// 检查并获取
TexturePtr texture = loader.GetTexture("my_texture");
if (texture) {
    texture->Bind(0);
} else {
    LOG_WARNING("纹理不存在");
}
```

### `HasTexture()`

检查纹理是否已缓存。

```cpp
bool HasTexture(const std::string& name) const
```

**参数**:
- `name` - 纹理名称

**返回值**: 已缓存返回 `true`，否则返回 `false`

**示例**:
```cpp
if (loader.HasTexture("my_texture")) {
    auto texture = loader.GetTexture("my_texture");
    // 使用纹理...
}
```

---

## 资源管理

### `RemoveTexture()`

从缓存中移除指定纹理。

```cpp
bool RemoveTexture(const std::string& name)
```

**参数**:
- `name` - 纹理名称

**返回值**: 成功移除返回 `true`，未找到返回 `false`

**示例**:
```cpp
loader.RemoveTexture("unused_texture");
```

### `Clear()`

清空所有缓存（释放所有纹理）。

```cpp
void Clear()
```

**注意**: 只有当纹理的引用计数之外的引用都已释放时，纹理才会真正被删除。

**示例**:
```cpp
loader.Clear();
```

### `CleanupUnused()`

清理未使用的纹理（引用计数为 1，仅被缓存持有）。

```cpp
size_t CleanupUnused()
```

**返回值**: 清理的纹理数量

**示例**:
```cpp
// 清理未使用的纹理
size_t cleaned = loader.CleanupUnused();
LOG_INFO("清理了 " + std::to_string(cleaned) + " 个未使用的纹理");
```

---

## 统计信息

### `GetTextureCount()`

获取缓存中的纹理数量。

```cpp
size_t GetTextureCount() const
```

**返回值**: 纹理数量

**示例**:
```cpp
LOG_INFO("缓存中有 " + std::to_string(loader.GetTextureCount()) + " 个纹理");
```

### `GetReferenceCount()`

获取纹理的引用计数。

```cpp
long GetReferenceCount(const std::string& name) const
```

**参数**:
- `name` - 纹理名称

**返回值**: 引用计数，未找到返回 0

**示例**:
```cpp
long refCount = loader.GetReferenceCount("my_texture");
LOG_INFO("纹理引用计数: " + std::to_string(refCount));
```

### `GetTotalMemoryUsage()`

获取总纹理内存使用量（估算）。

```cpp
size_t GetTotalMemoryUsage() const
```

**返回值**: 内存字节数

**示例**:
```cpp
size_t memoryBytes = loader.GetTotalMemoryUsage();
LOG_INFO("纹理总内存: " + std::to_string(memoryBytes / 1024 / 1024) + " MB");
```

### `PrintStatistics()`

打印缓存统计信息到日志。

```cpp
void PrintStatistics() const
```

**输出示例**:
```
========================================
纹理缓存统计信息
========================================
缓存纹理数量: 3
总内存使用量（估算）: 6 MB
----------------------------------------
纹理详情:
  - wall: 1024x1024, 引用计数: 2, 内存: ~4096 KB
  - floor: 512x512, 引用计数: 1, 内存: ~1024 KB
  - char: 256x256, 引用计数: 3, 内存: ~256 KB
========================================
```

**示例**:
```cpp
loader.PrintStatistics();
```

---

## 批量操作

### `PreloadTextures()`

批量预加载纹理列表。

```cpp
size_t PreloadTextures(const std::vector<std::tuple<std::string, std::string, bool>>& textureList)
```

**参数**:
- `textureList` - 纹理定义列表，每个元素为 `{名称, 路径, 是否生成Mipmap}`

**返回值**: 成功加载的纹理数量

**示例**:
```cpp
auto& loader = TextureLoader::GetInstance();

std::vector<std::tuple<std::string, std::string, bool>> textures = {
    {"wall", "textures/wall.png", true},
    {"floor", "textures/floor.png", true},
    {"character", "textures/character.png", true}
};

size_t loaded = loader.PreloadTextures(textures);
carboxymethylcellulose (CMC)（羧甲基纤维素钠）
LOG_INFO("预加载了 " + std::to_string(loaded) + " 个纹理");
```

---

## 使用示例

### 基本使用

```cpp
#include "render/texture_loader.h"

int main() {
    auto& loader = TextureLoader::GetInstance();
    
    // 加载纹理
    auto texture = loader.LoadTexture("test", "textures/test.png", true);
    if (!texture) {
        LOG_ERROR("纹理加载失败");
        return -1;
    }
    
    // 使用纹理...
    texture->Bind(0);
    
    // 在另一个地方获取同一个纹理
    auto sameTexture = loader.GetTexture("test");
    assert(texture.get() == sameTexture.get());
    
    // 打印统计
    loader.PrintStatistics();
    
    return 0;
}
```

### 场景切换示例

```cpp
class SceneManager {
private:
    TextureLoader& m_loader = TextureLoader::GetInstance();
    
public:
    void LoadLevel1() {
        // 清理旧场景纹理
        m_loader.CleanupUnused();
        
        // 加载新场景纹理
        m_loader.PreloadTextures({
            {"level1_wall", "level1/wall.png", true},
            {"level1_floor", "level1/floor.png", true},
            {"level1_ceiling", "level1/ceiling.png", true}
        });
    }
    
    void UnloadCurrentLevel() {
        m_loader.Clear();
    }
};
```

### 纹理引用计数管理

```cpp
auto& loader = TextureLoader::GetInstance();

{
    // texture1 引用计数 = 2 (缓存 + 变量)
    auto texture1 = loader.LoadTexture("tex", "test.png");
    
    {
        // texture2 引用计数 = 3 (缓存 + texture1 + texture2)
        auto texture2 = loader.GetTexture("tex");
        LOG_INFO("引用计数: " + std::to_string(loader.GetReferenceCount("tex")));
    }
    // texture2 销毁，引用计数 = 2
}

// texture1 销毁，引用计数 = 1（仅缓存持有）

// 清理未使用的纹理
size_t cleaned = loader.CleanupUnused();
LOG_INFO("清理了 " + std::to_string(cleaned) + " 个纹理");  // 会清理 "tex"
```

---

## 线程安全

`TextureLoader` 是线程安全的，可以在多线程环境中使用。内部使用 `std::mutex` 保护 `m_textures` 访问。

```cpp
// 在多个线程中加载纹理（安全）
std::vector<std::thread> threads;

for (const auto& path : texturePaths) {
    threads.emplace_back([&loader, path]() {
        loader.LoadTexture(path, path, true);
    });
}

for (auto& thread : threads) {
    thread.join();
}
```

---

## 性能建议

### 1. 命名规范

使用有意义的纹理名称，避免重复加载：

```cpp
// 好的命名
loader.LoadTexture("level1_wall_01", "walls/brick.png");
loader.LoadTexture("level1_wall_02", "walls/stone.png");

// 避免使用文件路径作为名称（容易出错）
loader.LoadTexture("walls/brick.png", "walls/brick.png");  // 可以但不够清晰
```

### 2. 预加载策略

在场景加载时预加载纹理，避免运行时的加载延迟：

```cpp
void LoadScene() {
    auto& loader = TextureLoader::GetInstance();
    
    // 预加载所有需要的纹理
    loader.PreloadTextures({
        {"char_model", "models/char.png", true},
        {"char_normal", "models/char_n.png", true},
        {"char_specular", "models/char_s.png", true}
    });
}
```

### 3. 内存管理

定期清理未使用的纹理：

```cpp
// 在每个场景切换后
loader.CleanupUnused();

// 在游戏退出前
loader.Clear();
```

---

## 常见问题

### Q: 为什么再次调用 `LoadTexture()` 不会重新加载文件？

A: 这是缓存机制的设计。如果纹理已缓存，直接返回缓存中的纹理，避免重复加载。如果需要强制重新加载，先调用 `RemoveTexture()` 再调用 `LoadTexture()`。

### Q: 如何判断纹理是否真正被释放？

A: 检查引用计数：

```cpp
long refCount = loader.GetReferenceCount("texture_name");
if (refCount == 1) {
    // 仅缓存持有，可以安全清理
    loader.RemoveTexture("texture_name");
}
```

### Q: 异步加载如何在线程间安全使用？

A: 当前实现中，异步加载在后台线程中进行，但 OpenGL 对象必须在主线程创建。对于复杂的多线程场景，建议在主线程中进行所有纹理加载。

---

## 相关类

- **[Texture](Texture.md)** - 底层纹理对象
- **[Shader](Shader.md)** - 着色器程序管理
- **[ShaderCache](ShaderCache.md)** - 着色器缓存系统（类似设计）

---

## 参考

- [Texture API](Texture.md)
- [纹理系统使用指南 Sponsor](../TEXTURE_SYSTEM.md)
- [Shader API](Shader.md)
- [ShaderCache API](ShaderCache.md)
- [示例程序](../../examples/05_texture_test.cpp)

---

[返回 API 首页](README.md) | [上一篇: Texture](Texture.md) | [下一篇: Renderer](Renderer.md)

