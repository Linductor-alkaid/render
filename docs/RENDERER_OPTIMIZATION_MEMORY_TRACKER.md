# ResourceMemoryTracker 内存追踪系统

[返回文档首页](README.md) | [返回优化进度](RENDERER_OPTIMIZATION_PROGRESS.md)

---

## 系统概述

`ResourceMemoryTracker` 是一个资源内存追踪和统计系统，用于监控和分析渲染引擎的内存使用情况。

**功能特性**:
- ✅ 实时内存统计
- ✅ 详细资源列表
- ✅ 内存泄漏检测
- ✅ 导出报告（JSON 格式）
- ✅ 线程安全设计

**版本**: 1.0  
**完成日期**: 2025-01-21  

---

## 核心功能

### 1. 资源注册和追踪

系统追踪以下资源类型：
- **纹理（Texture）**: 包含分辨率和格式信息
- **网格（Mesh）**: 包含顶点和索引数量
- **着色器（Shader）**: 包含程序 ID
- **GPU 缓冲（Buffer）**: 包含缓冲 ID 和大小

### 2. 内存统计

提供以下统计信息：
- 各类资源的内存占用
- 资源数量
- 总内存使用量
- 详细资源列表

### 3. 报告生成

支持导出 JSON 格式的详细报告，包含：
- 总览统计
- 各资源类型列表
- 按内存大小排序

---

## 使用方法

### 基础使用

```cpp
#include "render/resource_memory_tracker.h"

// 获取单例实例
auto& tracker = ResourceMemoryTracker::GetInstance();

// 注册纹理
Texture* texture = LoadTexture("path/to/texture.png");
tracker.RegisterTexture(texture);

// 注册网格
Mesh* mesh = CreateMesh();
tracker.RegisterMesh(mesh);

// 注册着色器
Shader* shader = LoadShader("vert.glsl", "frag.glsl");
tracker.RegisterShader(shader);

// 注册 GPU 缓冲
uint32_t vbo = CreateVertexBuffer();
tracker.RegisterBuffer(vbo, bufferSize, "VertexBuffer_Main");
```

### 获取统计信息

```cpp
// 获取总体统计
auto stats = tracker.GetStats();

printf("=== 资源内存统计 ===\n");
printf("总内存: %.2f MB\n", stats.totalMemory / (1024.0f * 1024.0f));
printf("纹理内存: %.2f MB (%u 个)\n", 
       stats.textureMemory / (1024.0f * 1024.0f), 
       stats.textureCount);
printf("网格内存: %.2f KB (%u 个)\n", 
       stats.meshMemory / 1024.0f, 
       stats.meshCount);
printf("着色器内存: %.2f KB (%u 个)\n", 
       stats.shaderMemory / 1024.0f, 
       stats.shaderCount);
printf("缓冲内存: %.2f KB (%u 个)\n", 
       stats.bufferMemory / 1024.0f, 
       stats.bufferCount);
```

### 详细资源列表

```cpp
// 获取纹理列表（按内存大小排序）
auto textures = tracker.GetTextureInfoList();
for (const auto& info : textures) {
    printf("纹理: %s - %.2f MB (%ux%u)\n",
           info.name.c_str(),
           info.memorySize / (1024.0f * 1024.0f),
           info.width,
           info.height);
}

// 获取网格列表
auto meshes = tracker.GetMeshInfoList();
for (const auto& info : meshes) {
    printf("网格: %s - %.2f KB (%u vertices, %u indices)\n",
           info.name.c_str(),
           info.memorySize / 1024.0f,
           info.vertexCount,
           info.indexCount);
}
```

### 生成报告

```cpp
// 导出 JSON 报告
bool success = tracker.GenerateReport("memory_report.json");
if (success) {
    printf("内存报告已生成: memory_report.json\n");
}
```

**报告格式示例**:
```json
{
  "summary": {
    "totalMemory": 52428800,
    "totalMemoryMB": 50.0,
    "textureMemory": 41943040,
    "meshMemory": 10485760,
    "shaderMemory": 0,
    "bufferMemory": 0,
    "textureCount": 10,
    "meshCount": 5,
    "shaderCount": 3,
    "bufferCount": 0
  },
  "textures": [
    {
      "name": "diffuse_texture",
      "size": 4194304,
      "width": 1024,
      "height": 1024
    }
  ],
  "meshes": [
    {
      "name": "character_mesh",
      "size": 2097152,
      "vertexCount": 10000,
      "indexCount": 15000
    }
  ]
}
```

### 内存泄漏检测

```cpp
// 检测未注销的资源（可能的泄漏）
auto leaks = tracker.DetectLeaks();
if (!leaks.empty()) {
    printf("检测到 %zu 个可能的内存泄漏:\n", leaks.size());
    for (const auto& leak : leaks) {
        printf("  - %s\n", leak.c_str());
    }
}
```

### 资源注销

```cpp
// 资源销毁时注销
tracker.UnregisterTexture(texture);
tracker.UnregisterMesh(mesh);
tracker.UnregisterShader(shader);
tracker.UnregisterBuffer(vbo);
```

---

## 集成到资源管理器

建议在 `ResourceManager` 中集成内存追踪：

```cpp
class ResourceManager {
public:
    Ref<Texture> LoadTexture(const std::string& path) {
        auto texture = LoadTextureImpl(path);
        if (texture) {
            // 注册到内存追踪器
            ResourceMemoryTracker::GetInstance().RegisterTexture(texture.get());
        }
        return texture;
    }
    
    void UnloadTexture(Texture* texture) {
        if (texture) {
            // 注销内存追踪
            ResourceMemoryTracker::GetInstance().UnregisterTexture(texture);
        }
        UnloadTextureImpl(texture);
    }
    
    // 类似地集成到 Mesh、Shader 等资源
};
```

---

## 内存计算说明

### 纹理内存

```cpp
// 基础大小 = 宽度 × 高度 × 每像素字节数
// 当前假设: RGBA8 格式，每像素 4 字节
size_t baseSize = width * height * 4;

// 考虑 Mipmap（额外约 33%）
size_t totalSize = baseSize + baseSize / 3;
```

**常见格式占用**:
- RGBA8: 4 字节/像素
- RGB8: 3 字节/像素
- RGBA16F: 8 字节/像素
- RGBA32F: 16 字节/像素

### 网格内存

```cpp
// 顶点数据 = 顶点数 × 顶点大小
// 当前假设: 每顶点 48 字节（position + texCoord + normal + color）
size_t vertexMemory = vertexCount * 48;

// 索引数据 = 索引数 × 4 字节（uint32_t）
size_t indexMemory = indexCount * 4;

size_t totalMemory = vertexMemory + indexMemory;
```

### 着色器内存

着色器占用较小，使用固定估计值：
```cpp
size_t shaderMemory = 32 * 1024;  // 32 KB
```

---

## 性能影响

### 内存开销

每个追踪的资源额外占用约 64-128 字节（取决于名称长度）：
- `TextureEntry`: ~80 字节
- `MeshEntry`: ~80 字节
- `ShaderEntry`: ~64 字节
- `BufferEntry`: ~64 字节

对于 1000 个资源，额外开销约 **64-128 KB**，可以忽略不计。

### 性能开销

- **注册/注销**: O(1) - 哈希表查找和插入
- **统计查询**: O(n) - 遍历所有资源
- **报告生成**: O(n) - 遍历和文件 I/O
- **线程同步**: 使用读写锁，支持并发读取

**建议**:
- 注册/注销操作很轻量，可以频繁调用
- 统计查询适合每秒或每帧调用
- 报告生成较耗时，建议按需调用（如调试时）

---

## 调试技巧

### 1. 在调试 HUD 中显示

```cpp
void RenderDebugHUD() {
    auto& tracker = ResourceMemoryTracker::GetInstance();
    auto stats = tracker.GetStats();
    
    DrawText("Memory Usage:");
    DrawText("  Total: %.2f MB", stats.totalMemory / (1024.0f * 1024.0f));
    DrawText("  Textures: %.2f MB (%u)", 
             stats.textureMemory / (1024.0f * 1024.0f), 
             stats.textureCount);
    DrawText("  Meshes: %.2f KB (%u)", 
             stats.meshMemory / 1024.0f, 
             stats.meshCount);
}
```

### 2. 定期内存检查

```cpp
class MemoryMonitor {
public:
    void Update(float deltaTime) {
        m_timer += deltaTime;
        
        // 每 5 秒检查一次
        if (m_timer >= 5.0f) {
            auto& tracker = ResourceMemoryTracker::GetInstance();
            auto stats = tracker.GetStats();
            
            LOG_INFO("内存使用: %.2f MB (纹理: %.2f MB, 网格: %.2f KB)",
                     stats.totalMemory / (1024.0f * 1024.0f),
                     stats.textureMemory / (1024.0f * 1024.0f),
                     stats.meshMemory / 1024.0f);
            
            m_timer = 0.0f;
        }
    }
    
private:
    float m_timer = 0.0f;
};
```

### 3. 退出时检测泄漏

```cpp
void OnApplicationExit() {
    auto& tracker = ResourceMemoryTracker::GetInstance();
    auto leaks = tracker.DetectLeaks();
    
    if (!leaks.empty()) {
        LOG_WARNING("检测到 %zu 个未释放的资源:", leaks.size());
        for (const auto& leak : leaks) {
            LOG_WARNING("  - %s", leak.c_str());
        }
        
        // 生成详细报告
        tracker.GenerateReport("memory_leaks.json");
    } else {
        LOG_INFO("所有资源已正确释放");
    }
}
```

---

## 扩展方向

### 1. 更精确的内存计算

```cpp
// 从 Texture 类获取实际格式
size_t CalculateTextureMemory(Texture* texture) {
    uint32_t width = texture->GetWidth();
    uint32_t height = texture->GetHeight();
    TextureFormat format = texture->GetFormat();
    
    size_t bytesPerPixel = GetBytesPerPixel(format);
    size_t baseSize = width * height * bytesPerPixel;
    
    if (texture->HasMipmaps()) {
        baseSize += baseSize / 3;
    }
    
    return baseSize;
}
```

### 2. 实时内存监控

```cpp
class ResourceMemoryTracker {
public:
    // 设置内存警告阈值
    void SetMemoryWarningThreshold(size_t bytes);
    
    // 检查是否超过阈值
    bool IsMemoryWarning() const;
    
    // 内存变化回调
    using MemoryChangeCallback = std::function<void(const ResourceMemoryStats&)>;
    void SetMemoryChangeCallback(MemoryChangeCallback callback);
};
```

### 3. 历史记录

```cpp
struct MemorySnapshot {
    ResourceMemoryStats stats;
    std::chrono::steady_clock::time_point timestamp;
};

class ResourceMemoryTracker {
public:
    // 保存当前快照
    void SaveSnapshot();
    
    // 获取历史快照
    std::vector<MemorySnapshot> GetHistory() const;
    
    // 清除历史
    void ClearHistory();
};
```

---

## 相关文档

- [RENDERER_OPTIMIZATION_PLAN.md](RENDERER_OPTIMIZATION_PLAN.md) - 优化方案详细设计
- [RENDERER_OPTIMIZATION_PROGRESS.md](RENDERER_OPTIMIZATION_PROGRESS.md) - 优化进度跟踪
- [RESOURCE_OWNERSHIP_GUIDE.md](RESOURCE_OWNERSHIP_GUIDE.md) - 资源所有权指南

---

[返回文档首页](README.md) | [返回优化进度](RENDERER_OPTIMIZATION_PROGRESS.md)

