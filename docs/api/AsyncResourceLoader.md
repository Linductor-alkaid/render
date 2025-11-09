# AsyncResourceLoader API 参考

[返回 API 首页](README.md)

---

## 概述

`AsyncResourceLoader` 是一个单例类，提供后台线程异步资源加载功能。它支持在工作线程加载文件和解析数据，在主线程执行GPU上传，避免阻塞渲染循环。

**版本**: v0.12.0  
**头文件**: `<render/async_resource_loader.h>`  
**命名空间**: `Render`

---

## 核心特性

- **线程池**: 多个工作线程并发加载资源
- **任务队列**: 线程安全的任务提交和调度
- **主线程GPU上传**: 符合OpenGL上下文要求
- **进度回调**: 实时通知加载状态
- **优先级控制**: 支持任务优先级排序
- **统计信息**: 详细的加载统计数据

---

## 线程模型

```
主线程 (OpenGL Context)
├── 提交加载任务 (LoadMeshAsync)
├── 轮询完成任务 (ProcessCompletedTasks)
└── 执行GPU上传 (mesh->Upload())

工作线程池 (N个线程)
├── 从队列获取任务
├── 执行文件I/O
├── 解析数据
└── 放入完成队列
```

---

## 类结构

### AsyncResourceLoader

```cpp
class AsyncResourceLoader {
public:
    // 单例访问
    static AsyncResourceLoader& GetInstance();
    
    // 初始化/关闭
    void Initialize(size_t numThreads = 0);
    void Shutdown();
    bool IsInitialized() const;
    
    // 异步加载接口
    std::shared_ptr<MeshLoadTask> LoadMeshAsync(
        const std::string& filepath,
        const std::string& name = "",
        std::function<void(const MeshLoadResult&)> callback = nullptr,
        float priority = 0.0f
    );
    
    std::shared_ptr<TextureLoadTask> LoadTextureAsync(
        const std::string& filepath,
        const std::string& name = "",
        bool generateMipmap = true,
        std::function<void(const TextureLoadResult&)> callback = nullptr,
        float priority = 0.0f
    );
    
    std::shared_ptr<ModelLoadTask> LoadModelAsync(
        const std::string& filepath,
        const std::string& name = "",
        const ModelLoadOptions& options = {},
        std::function<void(const ModelLoadResult&)> callback = nullptr,
        float priority = 0.0f
    );
    
    // 主线程处理
    size_t ProcessCompletedTasks(size_t maxTasks = 10);
    bool WaitForAll(float timeoutSeconds = 0.0f);
    
    // 状态查询
    size_t GetPendingTaskCount() const;
    size_t GetLoadingTaskCount() const;
    size_t GetWaitingUploadCount() const;
    void PrintStatistics() const;
};
```

---

## 枚举类型

### LoadStatus

加载任务的状态：

```cpp
enum class LoadStatus {
    Pending,        // 等待中（在队列中）
    Loading,        // 加载中（工作线程执行）
    Loaded,         // 已加载（等待GPU上传）
    Uploading,      // 上传中（主线程执行）
    Completed,      // 完成
    Failed          // 失败
};
```

### AsyncResourceType

资源类型：

```cpp
enum class AsyncResourceType {
    Mesh,       // 网格
    Texture,    // 纹理
    Material,   // 材质
    Model       // 模型（多网格+材质）
};
```

---

## 加载任务结构

### MeshLoadTask

```cpp
struct MeshLoadTask {
    std::string name;                       // 资源名称
    AsyncResourceType type;                 // 类型 = Mesh
    std::atomic<LoadStatus> status;         // 当前状态
    std::string errorMessage;               // 错误信息
    float priority;                         // 优先级
    
    Ref<Mesh> result;                      // 加载结果
    std::function<void(const MeshLoadResult&)> callback;  // 完成回调
};
```

### LoadResult<T>

```cpp
template<typename T>
struct LoadResult {
    std::shared_ptr<T> resource;
    std::string name;
    LoadStatus status;
    std::string errorMessage;
    
    bool IsSuccess() const;
};
```

### ModelLoadTask / ModelLoadResult (v1.1 新增)

```cpp
struct ModelLoadTask : LoadTaskBase {
    ModelLoadOptions requestedOptions;
    std::string filepath;
    std::string overrideName;
    ModelLoadOutput result;
    std::function<void(const ModelLoadResult&)> callback;
};

struct ModelLoadResult : LoadResult<Model> {
    std::vector<std::string> meshResourceNames;
    std::vector<std::string> materialResourceNames;
};
```

> `ModelLoadOptions` 定义于 [ModelLoader](ModelLoader.md)，用于控制 UV 翻转、自动上传和资源注册策略。

---

## API 详解

### 1. 初始化

#### Initialize

```cpp
void Initialize(size_t numThreads = 0);
```

**说明**: 初始化异步加载器，创建工作线程池

**参数**:
- `numThreads`: 工作线程数
  - `0` (默认): 自动使用 `CPU核心数 - 1`
  - `> 0`: 指定线程数

**注意**:
- 必须在使用前调用
- 只能初始化一次（重复调用会警告）
- 通常在程序启动时初始化

**示例**:

```cpp
auto& loader = AsyncResourceLoader::GetInstance();
loader.Initialize(4);  // 使用4个工作线程
```

---

#### Shutdown

```cpp
void Shutdown();
```

**说明**: 关闭异步加载器，等待所有任务完成并退出工作线程

**注意**:
- 会阻塞直到所有工作线程退出
- 自动在析构时调用
- 通常在程序退出前调用

**示例**:

```cpp
loader.Shutdown();  // 清理资源
```

---

### 2. 异步加载

#### LoadMeshAsync

```cpp
std::shared_ptr<MeshLoadTask> LoadMeshAsync(
    const std::string& filepath,
    const std::string& name = "",
    std::function<void(const MeshLoadResult&)> callback = nullptr,
    float priority = 0.0f
);
```

**说明**: 异步加载网格文件

**参数**:
- `filepath`: 模型文件路径 (支持 .obj, .fbx, .pmx 等)
- `name`: 资源名称（用于ResourceManager注册）
- `callback`: 完成回调函数（可选）
- `priority`: 优先级（越高越优先，默认0）

**返回值**: 任务句柄（可用于查询状态）

**回调时机**: 在主线程，GPU上传完成后

**示例**:

```cpp
auto task = loader.LoadMeshAsync(
    "models/character.fbx",
    "character",
    [](const MeshLoadResult& result) {
        if (result.IsSuccess()) {
            // 成功：资源已上传到GPU
            Logger::GetInstance().Info("✅ 加载成功: " + result.name);
        } else {
            // 失败：输出错误信息
            Logger::GetInstance().Error("❌ 加载失败: " + result.errorMessage);
        }
    },
    10.0f  // 高优先级
);

// 检查任务状态
if (task->status == LoadStatus::Completed) {
    auto mesh = task->result;  // 获取结果
}
```

---

#### LoadTextureAsync

```cpp
std::shared_ptr<TextureLoadTask> LoadTextureAsync(
    const std::string& filepath,
    const std::string& name = "",
    bool generateMipmap = true,
    std::function<void(const TextureLoadResult&)> callback = nullptr,
    float priority = 0.0f
);
```

**说明**: 异步加载纹理文件

**参数**:
- `filepath`: 纹理文件路径 (支持 .png, .jpg, .bmp 等)
- `name`: 资源名称
- `generateMipmap`: 是否生成mipmap
- `callback`: 完成回调
- `priority`: 优先级

**返回值**: 任务句柄

**示例**:

```cpp
auto task = loader.LoadTextureAsync(
    "textures/diffuse.png",
    "diffuse",
    true,  // 生成mipmap
    [](const TextureLoadResult& result) {
        if (result.IsSuccess()) {
            ResourceManager::GetInstance().RegisterTexture(result.name, result.resource);
        }
    }
);
```

---

#### LoadModelAsync ⭐ v1.1 新增

```cpp
std::shared_ptr<ModelLoadTask> LoadModelAsync(
    const std::string& filepath,
    const std::string& name = "",
    const ModelLoadOptions& options = {},
    std::function<void(const ModelLoadResult&)> callback = nullptr,
    float priority = 0.0f
);
```

**说明**: 利用后台线程解析完整模型（多网格 + 材质），在主线程完成网格上传及资源注册。

**关键点**:
- 工作线程中默认禁用 GPU 上传与资源注册，解析结果存放在 `ModelLoadOutput`。  
- 主线程在 `ProcessCompletedTasks()` 阶段根据 `options.autoUpload` 对所有 `ModelPart` 触发 `mesh->Upload()`。  
- 根据 `options.registerModel / registerMeshes / registerMaterials` 决定是否调用 `ModelLoader::RegisterResources()`，并更新依赖关系。  
- 回调得到 `ModelLoadResult`，可直接访问 `ModelPtr` 以及实际注册的资源名称列表。

**示例**:

```cpp
ModelLoadOptions options;
options.autoUpload = true;
options.resourcePrefix = "level01";

loader.LoadModelAsync(
    "models/level01.glb",
    "level01",
    options,
    [](const ModelLoadResult& result) {
        if (!result.IsSuccess()) {
            Logger::GetInstance().Error("模型加载失败: " + result.errorMessage);
            return;
        }

        Logger::GetInstance().InfoFormat(
            "模型 %s 加载完成，注册了 %zu 个网格",
            result.name.c_str(),
            result.meshResourceNames.size()
        );

        // 将模型提交给场景或 ECS
    }
);
```

---

### 3. 主线程处理

#### ProcessCompletedTasks

```cpp
size_t ProcessCompletedTasks(size_t maxTasks = 10);
```

**说明**: 处理已完成的加载任务（执行GPU上传）

**参数**:
- `maxTasks`: 本帧最多处理的任务数（默认10）

**返回值**: 实际处理的任务数

**调用时机**: ⭐ **必须在主渲染循环中调用**

**性能建议**:
- 60 FPS: `maxTasks = 5` (保守，约5ms)
- 30 FPS: `maxTasks = 10` (适中，约10ms)
- 加载界面: `maxTasks = 50` (激进，快速完成)

**示例**:

```cpp
// 主循环
while (running) {
    // ✅ 关键：每帧处理完成的任务
    size_t processed = loader.ProcessCompletedTasks(10);
    
    if (processed > 0) {
        Logger::GetInstance().Debug("本帧处理了 " + std::to_string(processed) + " 个任务");
    }
    
    // 渲染
    renderer.BeginFrame();
    // ... 渲染代码 ...
    renderer.EndFrame();
    renderer.Present();
}
```

**警告**: ⚠️ 不调用此方法会导致任务永远不会上传到GPU！

---

#### WaitForAll

```cpp
bool WaitForAll(float timeoutSeconds = 0.0f);
```

**说明**: 等待所有任务完成

**参数**:
- `timeoutSeconds`: 超时时间（秒）
  - `0.0` (默认): 无限等待
  - `> 0.0`: 指定超时时间

**返回值**:
- `true`: 所有任务完成
- `false`: 超时

**示例**:

```cpp
// 场景切换：等待所有资源加载完成
loader.LoadMeshAsync("scene1_asset1.obj");
loader.LoadMeshAsync("scene1_asset2.fbx");
// ...

// 等待最多10秒
if (loader.WaitForAll(10.0f)) {
    Logger::GetInstance().Info("所有资源加载完成");
    StartGame();
} else {
    Logger::GetInstance().Warning("加载超时");
}
```

---

### 4. 状态查询

#### GetPendingTaskCount

```cpp
size_t GetPendingTaskCount() const;
```

**说明**: 获取待处理（在队列中）的任务数

**线程安全**: ✅ 是

---

#### GetLoadingTaskCount

```cpp
size_t GetLoadingTaskCount() const;
```

**说明**: 获取正在加载（工作线程执行中）的任务数

---

#### GetWaitingUploadCount

```cpp
size_t GetWaitingUploadCount() const;
```

**说明**: 获取等待上传（已加载完成，等待GPU上传）的任务数

---

#### PrintStatistics

```cpp
void PrintStatistics() const;
```

**说明**: 打印加载统计信息到日志

**输出示例**:

```
========================================
异步加载器统计
========================================
总任务数: 150
已完成: 145
失败: 3
正在加载: 2
待处理: 5
等待上传: 0
========================================
```

---

## 使用模式

### 模式1: 基本异步加载

```cpp
#include <render/renderer.h>
#include <render/async_resource_loader.h>
#include <render/resource_manager.h>
#include <render/shader_cache.h>

int main() {
    // 初始化渲染器
    Renderer renderer;
    renderer.Initialize("异步加载示例", 1280, 720);
    
    // 初始化异步加载器
    auto& loader = AsyncResourceLoader::GetInstance();
    loader.Initialize(4);  // 4个工作线程
    
    // 提交加载任务
    loader.LoadMeshAsync(
        "models/character.fbx",
        "character",
        [](const MeshLoadResult& result) {
            if (result.IsSuccess()) {
                // 在回调中访问单例，避免lambda捕获问题
                auto& resMgr = ResourceManager::GetInstance();
                resMgr.RegisterMesh(result.name, result.resource);
                Logger::GetInstance().Info("✅ 加载完成: " + result.name);
            }
        }
    );
    
    // 主循环
    bool running = true;
    while (running) {
        // ✅ 关键：处理完成的任务（在主线程）
        loader.ProcessCompletedTasks(10);
        
        // 事件处理
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
        }
        
        // 渲染
        renderer.BeginFrame();
        renderer.Clear();
        
        // 渲染已加载的网格
        auto& resMgr = ResourceManager::GetInstance();
        auto mesh = resMgr.GetMesh("character");
        if (mesh && mesh->IsUploaded()) {
            // mesh->Draw();
        }
        
        renderer.EndFrame();
        renderer.Present();
    }
    
    // 清理
    loader.Shutdown();
    renderer.Shutdown();
    return 0;
}
```

---

### 模式2: 加载进度显示

```cpp
// 全局变量
std::atomic<size_t> g_loadingTotal = 0;
std::atomic<size_t> g_loadingCompleted = 0;

void LoadSceneAsync() {
    auto& loader = AsyncResourceLoader::GetInstance();
    
    std::vector<std::string> assetPaths = {
        "models/building1.fbx",
        "models/building2.fbx",
        "models/terrain.obj",
        // ... 更多资源
    };
    
    g_loadingTotal = assetPaths.size();
    g_loadingCompleted = 0;
    
    for (const auto& path : assetPaths) {
        loader.LoadMeshAsync(
            path,
            path,
            [](const MeshLoadResult& result) {
                g_loadingCompleted++;
                
                float progress = (float)g_loadingCompleted / g_loadingTotal * 100.0f;
                Logger::GetInstance().Info("加载进度: " + std::to_string((int)progress) + "%");
            }
        );
    }
}

// 在渲染循环中
while (loading) {
    loader.ProcessCompletedTasks(10);
    
    // 渲染加载界面
    RenderLoadingScreen((float)g_loadingCompleted / g_loadingTotal);
    
    if (g_loadingCompleted >= g_loadingTotal) {
        loading = false;
    }
}
```

---

### 模式3: 优先级控制

```cpp
// 优先加载关键资源
loader.LoadMeshAsync("player.fbx", "player", callback, 100.0f);   // 最高优先级
loader.LoadMeshAsync("enemy.fbx", "enemy", callback, 50.0f);      // 中优先级

// 背景资源稍后加载
loader.LoadMeshAsync("tree1.obj", "tree1", callback, 1.0f);       // 低优先级
loader.LoadMeshAsync("grass.obj", "grass", callback, 0.5f);       // 最低优先级
```

---

### 模式4: 流式加载（LOD）

```cpp
// 加载不同LOD级别
loader.LoadMeshAsync("model_lod0.fbx", "model_high", callback, 10.0f);   // 高精度，高优先级
loader.LoadMeshAsync("model_lod1.fbx", "model_mid", callback, 5.0f);     // 中精度
loader.LoadMeshAsync("model_lod2.fbx", "model_low", callback, 1.0f);     // 低精度

// 先显示低精度，逐步替换为高精度
```

---

## 性能建议

### 1. 工作线程数

```cpp
// CPU核心数 = 8
loader.Initialize(6);  // 推荐4-6个（留核心给主线程）

// CPU核心数 = 4
loader.Initialize(2);  // 推荐2-3个
```

**原则**: 留1-2个核心给主线程和系统

---

### 2. 每帧处理任务数

根据目标帧率调整：

```cpp
// 60 FPS (16.67ms/帧)
loader.ProcessCompletedTasks(5);   // 保守

// 30 FPS (33.33ms/帧)
loader.ProcessCompletedTasks(10);  // 适中

// 加载界面（FPS不重要）
loader.ProcessCompletedTasks(50);  // 激进
```

---

### 3. 内存管理

```cpp
// ❌ 错误：一次性加载太多
for (int i = 0; i < 10000; i++) {
    loader.LoadMeshAsync("model_" + std::to_string(i) + ".fbx");
}  // 内存爆炸！

// ✅ 正确：分批加载
const size_t BATCH_SIZE = 50;
for (size_t batch = 0; batch < total; batch += BATCH_SIZE) {
    // 加载一批
    for (size_t i = batch; i < batch + BATCH_SIZE; i++) {
        loader.LoadMeshAsync(...);
    }
    // 等待完成
    loader.WaitForAll();
}
```

---

## 线程安全

| 方法 | 线程安全 | 调用线程 |
|------|----------|----------|
| `Initialize` | ⚠️ 否 | 主线程（一次） |
| `Shutdown` | ⚠️ 否 | 主线程 |
| `LoadMeshAsync` | ✅ 是 | 任意线程 |
| `LoadTextureAsync` | ✅ 是 | 任意线程 |
| `LoadModelAsync` | ✅ 是 | 任意线程 |
| `ProcessCompletedTasks` | ⚠️ **主线程** | ⭐ 主线程（必须） |
| `WaitForAll` | ✅ 是 | 任意线程 |
| `Get*Count` | ✅ 是 | 任意线程 |
| `PrintStatistics` | ✅ 是 | 任意线程 |

**关键**: `ProcessCompletedTasks` 必须在主线程（OpenGL上下文线程）调用！

---

## 常见问题

### Q1: 为什么需要异步加载？

**A**: 
- 同步加载会阻塞主线程，导致窗口无响应
- 大型模型加载可能需要数秒
- 异步加载提升用户体验，可以显示进度

---

### Q2: 可以在工作线程调用OpenGL吗？

**A**: ⚠️ **不可以！**

OpenGL要求所有调用在创建上下文的线程执行。异步加载器的设计就是为了解决这个问题：
- 工作线程：文件I/O + 数据解析（不涉及OpenGL）
- 主线程：GPU上传（`mesh->Upload()`，调用OpenGL）

---

### Q3: 忘记调用 ProcessCompletedTasks 会怎样？

**A**: 
- 任务会卡在 `Loaded` 状态（永远不会上传到GPU）
- 回调不会被调用
- 资源无法使用

**解决**: 在主循环中每帧调用 `ProcessCompletedTasks()`

---

### Q4: 如何知道所有资源都加载完了？

**A**: 有三种方式：

```cpp
// 方式1: 检查任务状态
std::vector<std::shared_ptr<MeshLoadTask>> tasks;
tasks.push_back(loader.LoadMeshAsync("asset1.obj"));
tasks.push_back(loader.LoadMeshAsync("asset2.obj"));

// 检查所有任务
bool allDone = true;
for (const auto& task : tasks) {
    if (task->status != LoadStatus::Completed && 
        task->status != LoadStatus::Failed) {
        allDone = false;
        break;
    }
}

// 方式2: 使用 WaitForAll
if (loader.WaitForAll(10.0f)) {
    Logger::GetInstance().Info("所有任务完成！");
}

// 方式3: 计数器（在回调中）
std::atomic<size_t> completed = 0;
size_t total = 10;
for (size_t i = 0; i < total; i++) {
    loader.LoadMeshAsync(
        "model_" + std::to_string(i) + ".obj",
        "mesh_" + std::to_string(i),
        [&completed](const MeshLoadResult& result) {
            completed++;
        }
    );
}

// 在主循环中检查
while (running) {
    loader.ProcessCompletedTasks(10);
    
    if (completed >= total) {
        Logger::GetInstance().Info("所有资源加载完成！");
        // 进入游戏...
    }
}
```

---

### Q5: 加载失败如何处理？

**A**:

```cpp
loader.LoadMeshAsync(
    "models/character.fbx",
    "character",
    [](const MeshLoadResult& result) {
        auto& resMgr = ResourceManager::GetInstance();
        
        if (result.IsSuccess()) {
            // 成功：注册资源
            resMgr.RegisterMesh(result.name, result.resource);
            Logger::GetInstance().Info("✅ 加载成功: " + result.name);
        } else {
            // 失败：使用默认资源
            Logger::GetInstance().Error("❌ 加载失败: " + result.name);
            Logger::GetInstance().Error("错误信息: " + result.errorMessage);
            
            // 创建默认立方体作为占位符
            auto fallback = MeshLoader::CreateCube(1.0f, Color::Red());
            resMgr.RegisterMesh(result.name, fallback);
            Logger::GetInstance().Info("使用默认立方体替代");
        }
    }
);
```

---

## 示例程序

参考：`examples/29_async_loading_test.cpp`

### 运行测试

```bash
cd build/bin
./29_async_loading_test
```

### 控制说明

- **SPACE**: 开始异步加载
- **R**: 重新加载
- **S**: 打印统计信息
- **ESC**: 退出

### 测试内容

测试程序演示了：
1. ✅ **同步加载**：立方体（绿色，左侧）- 作为对比
2. ✅ **异步加载**：PMX模型或其他网格（紫色，中间）
3. ✅ **批量加载**：多个小型网格（测试并发）
4. ✅ **进度显示**：加载过程中显示进度
5. ✅ **3D渲染**：带光照、相机和旋转动画

### 预期效果

- **加载阶段**: 深蓝色背景 + 控制台进度信息
- **完成后**: 显示旋转的3D模型，自动旋转
- **无卡顿**: 加载过程中窗口保持响应

### 关键代码片段

```cpp
// 1. 初始化
auto& loader = AsyncResourceLoader::GetInstance();
loader.Initialize(4);

// 2. 提交异步加载任务
auto task = loader.LoadMeshAsync(
    "models/miku/v4c5.0short.pmx",
    "async_model",
    [](const MeshLoadResult& result) {
        if (result.IsSuccess()) {
            auto& resMgr = ResourceManager::GetInstance();
            std::string meshName = "async_mesh_0";
            resMgr.RegisterMesh(meshName, result.resource);
        }
    },
    10.0f  // 高优先级
);

// 3. 主循环中处理完成的任务
while (running) {
    // ✅ GPU上传在这里执行（主线程）
    loader.ProcessCompletedTasks(10);
    
    // 渲染
    renderer.BeginFrame();
    // ... 渲染代码 ...
    renderer.EndFrame();
    renderer.Present();
}

// 4. 清理
loader.Shutdown();
```

---

## 版本历史

### v1.1.0 (2025-11-09)

- ✅ 新增 `LoadModelAsync()` 支持完整模型异步加载  
- ✅ 集成 `ModelLoader`，自动上传网格并注册资源  
- ✅ 回调返回 `ModelLoadResult`，包含注册后的资源名称

### v0.12.0 (2025-11-03)

- ✅ 初始版本
- ✅ 支持网格和纹理异步加载
- ✅ 工作线程池
- ✅ 任务队列和回调
- ✅ 统计信息

---

## 相关文档

- [MeshLoader API](MeshLoader.md) - 网格加载器（支持 `autoUpload` 参数）
- [ModelLoader API](ModelLoader.md) - 模型加载与资源注册
- [ResourceManager API](ResourceManager.md) - 资源管理器
- [Renderer API](Renderer.md) - 渲染器
- [异步资源加载系统设计](../todolists/ASYNC_RESOURCE_LOADING.md) - 完整设计文档

---

[上一篇: TextureLoader](TextureLoader.md) | [下一篇: ResourceManager](ResourceManager.md) | [返回 API 首页](README.md)

