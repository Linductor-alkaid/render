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
    
    // 两阶段接口
    bool DecodeTextureToStaging(const std::string& filepath,
                                bool generateMipmap,
                                TextureStagingData* outData,
                                std::string* errorMessage = nullptr);
    TexturePtr UploadStagedTexture(const std::string& name,
                                   TextureStagingData&& stagingData);
    
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

### TextureStagingData （v0.13+）

`DecodeTextureToStaging()` 的输出结构，封装 CPU 解码后的像素数据。  
在工作线程可安全创建，随后通过 `UploadStagedTexture()` 在主线程完成 GPU 上传。

```cpp
struct TextureStagingData {
    std::vector<std::uint8_t> pixels; // RGBA32 原始像素
    int width = 0;
    int height = 0;
    TextureFormat format = TextureFormat::RGBA;
    bool generateMipmap = true;

    bool IsValid() const {
        return width > 0 && height > 0 && !pixels.empty();
    }
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

## 两阶段接口

### `DecodeTextureToStaging()`

在 **任意线程** 将纹理文件解码为 `TextureStagingData`，不产生任何 OpenGL 调用。

```cpp
bool DecodeTextureToStaging(const std::string& filepath,
                            bool generateMipmap,
                            TextureStagingData* outData,
                            std::string* errorMessage = nullptr)
```

**用途**:
- 配合 `AsyncResourceLoader`，在后台线程执行磁盘 I/O 与像素转换。
- 可以根据 `generateMipmap` 标志决定是否在上传阶段生成 mipmap。

**注意**:
- `outData` 必须非空；函数内部会填充 `pixels / width / height / format`。
- 若返回 `false`，`errorMessage`（若提供）会包含详细错误信息，同时日志会记录具体原因。

### `UploadStagedTexture()`

在 **拥有 OpenGL 上下文的线程** 将 staging 数据上传至 GPU，并加入缓存。

```cpp
TexturePtr UploadStagedTexture(const std::string& name,
                               TextureStagingData&& stagingData)
```

**行为**:
- 如果 `name` 非空且缓存中已存在同名纹理，会直接返回缓存中的纹理并跳过上传。
- 上传成功后会将纹理放入缓存，供 `ResourceManager` 或其他系统复用。
- 失败时返回 `nullptr` 并记录错误。

**典型流程**:
```cpp
TextureLoader::TextureStagingData staging;
if (TextureLoader::GetInstance().DecodeTextureToStaging("textures/stone.png", true, &staging)) {
    auto texture = TextureLoader::GetInstance().UploadStagedTexture("stone", std::move(staging));
    if (texture) {
        ResourceManager::GetInstance().RegisterTexture("stone", texture);
    }
}
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
- 如果纹理已缓存，返回一个 `std::future`（deferred）立即提供缓存结果。
- 如果未缓存，返回的 `future` 会在调用 `get()` 时触发同步加载。由于内部需要在主线程执行 `UploadStagedTexture()`，请确保在拥有 OpenGL 上下文的线程调用 `future.get()`。
- 在更复杂的场景中推荐使用 `AsyncResourceLoader::LoadTextureAsync()`，该接口已经整合 staged pipeline 与主线程上传。

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

**注意**:
- `future` 使用 `std::launch::deferred`，不会自动启动后台线程。
- 调用 `future.get()` 时所在的线程必须持有 OpenGL 上下文，否则会触发 `GL_THREAD_CHECK` 断言。
- 对于真正的后台加载，请优先使用 `AsyncResourceLoader`。

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

**✅ `TextureLoader` 和 `Texture` 都是线程安全的**（自 2025-10-28 更新）。

### 线程安全保证

#### TextureLoader
- 所有公共方法都使用 `std::mutex` 保护缓存访问
- 多个线程可以安全地调用 `LoadTexture()`、`GetTexture()` 等方法
- **双重检查锁定优化**: `LoadTexture()` 和 `CreateTexture()` 使用双重检查锁定模式，将耗时的文件 IO 操作移到锁外执行，大幅提升并发性能
- 异步加载使用正确的双重检查锁定模式
- 支持并发读取纹理属性和统计信息

#### Texture
- 所有公共方法都使用互斥锁保护
- `LoadFromFile()` 将文件 IO 操作移到锁外，只在创建 OpenGL 纹理时持锁
- 可以从多个线程安全地访问纹理属性
- 移动操作使用 `std::scoped_lock` 避免死锁

### 线程安全使用模式

#### 模式 1: 并发加载（安全）

```cpp
auto& loader = TextureLoader::GetInstance();
std::vector<std::thread> threads;

for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&loader, i]() {
        std::string name = "texture_" + std::to_string(i);
        std::string path = "textures/tex_" + std::to_string(i) + ".png";
        
        auto texture = loader.LoadTexture(name, path, true);  // 线程安全
        if (texture) {
            // 安全地读取属性
            int width = texture->GetWidth();
            int height = texture->GetHeight();
        }
    });
}

for (auto& thread : threads) {
    thread.join();
}
```

#### 模式 2: 并发访问已加载的纹理（安全）

```cpp
// 主线程中预加载
auto& loader = TextureLoader::GetInstance();
loader.LoadTexture("shared_texture", "textures/shared.png", true);

// 多个工作线程中使用
std::vector<std::thread> workers;
for (int i = 0; i < 5; ++i) {
    workers.emplace_back([&loader]() {
        // 从缓存获取（线程安全）
        auto texture = loader.GetTexture("shared_texture");
        
        if (texture) {
            // 安全地访问纹理
            texture->Bind(0);           // 线程安全（但 OpenGL 调用需要在主线程）
            int w = texture->GetWidth(); // 线程安全
        }
    });
}

for (auto& worker : workers) {
    worker.join();
}
```

#### 模式 3: 并发统计查询（安全）

```cpp
// 监控线程
std::thread monitor([&loader]() {
    while (running) {
        // 所有这些调用都是线程安全的
        size_t count = loader.GetTextureCount();
        size_t memory = loader.GetTotalMemoryUsage();
        long refCount = loader.GetReferenceCount("my_texture");
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
});
```

#### 模式 4: 异步加载（安全）

```cpp
auto& loader = TextureLoader::GetInstance();
std::vector<std::future<AsyncTextureResult>> futures;

// 启动多个异步加载
for (int i = 0; i < 5; ++i) {
    std::string name = "async_tex_" + std::to_string(i);
    std::string path = "textures/tex_" + std::to_string(i) + ".png";
    
    futures.push_back(loader.LoadTextureAsync(name, path, true));
}

// 等待所有加载完成
for (auto& future : futures) {
    AsyncTextureResult result = future.get();
    if (result.success) {
        // 使用纹理...
    }
}
```

### 重要限制

#### ⚠️ OpenGL 上下文限制

虽然类本身是线程安全的，但 **OpenGL 调用必须在创建上下文的线程中执行**（通常是主线程）：

```cpp
// ✅ 正确：在主线程中加载和绑定
void MainThread() {
    auto& loader = TextureLoader::GetInstance();
    auto texture = loader.LoadTexture("tex", "test.png", true);
    texture->Bind(0);              // OpenGL 调用 - 在主线程中安全
    glDrawArrays(...);             // OpenGL 调用 - 在主线程中安全
}

// ✅ 正确：工作线程中读取属性
void WorkerThread() {
    auto& loader = TextureLoader::GetInstance();
    auto texture = loader.GetTexture("tex");
    
    if (texture) {
        int width = texture->GetWidth();   // 安全：只读取数据
        int height = texture->GetHeight(); // 安全：只读取数据
        // 不进行 OpenGL 调用
    }
}

// ❌ 错误：不要在工作线程中进行 OpenGL 调用
void BadWorkerThread() {
    auto& loader = TextureLoader::GetInstance();
    auto texture = loader.GetTexture("tex");
    
    texture->Bind(0);              // 危险！OpenGL 调用不在主线程
    texture->SetFilter(...);       // 危险！会调用 glTexParameteri
}
```

### 性能考虑

1. **锁的开销**: 互斥锁操作有一定开销，但相对于纹理加载和 OpenGL 操作通常可以忽略
2. **缓存优势**: 多线程同时请求同一纹理时，只会加载一次，其他线程从缓存获取
3. **避免锁竞争**: 预加载纹理可以减少运行时的锁竞争

### 测试

项目包含专门的线程安全测试程序：

```bash
# 运行纹理系统线程安全测试
./build/bin/Release/09_texture_thread_safe_test.exe
```

测试内容包括：
1. 多线程并发加载同一纹理
2. 多线程并发使用纹理
3. 多线程并发创建不同纹理
4. 多线程并发访问 TextureLoader 方法
5. 多线程异步加载

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

### Q: 如何在多线程环境中安全地使用纹理系统？

A: 详见上方**线程安全**章节。简要总结：
- `TextureLoader` 和 `Texture` 都是线程安全的
- 可以从多个线程并发加载和访问纹理
- OpenGL 调用必须在创建上下文的线程中执行
- 参考测试程序 `09_texture_thread_safe_test.cpp`

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

