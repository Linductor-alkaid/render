# 异步资源加载系统设计

[返回文档首页](README.md)

---

## 概述

异步资源加载允许在后台线程加载模型、纹理等资源，避免阻塞主渲染线程，提升用户体验。

**核心原则**:
- 📁 **文件I/O**: 在工作线程执行（最耗时）
- 🔧 **数据解析**: 在工作线程执行
- 🎨 **GPU上传**: 在主线程执行（OpenGL限制）
- 🔒 **线程安全**: 使用任务队列和状态同步

---

## 架构设计

### 系统组件

```
┌─────────────────┐
│  主线程         │
│  (Render Loop)  │
└────────┬────────┘
         │
         ├──► 提交加载任务
         │
         ├──► 轮询完成任务
         │
         └──► GPU上传（Upload）
              
┌─────────────────┐
│  工作线程池     │
│  (Worker Pool)  │
└────────┬────────┘
         │
         ├──► 文件读取
         │
         ├──► 数据解析
         │
         └──► 生成网格/纹理对象
```

### 关键类

1. **AsyncResourceLoader** - 异步加载器（单例）
2. **ResourceLoadTask** - 加载任务
3. **LoadTaskQueue** - 线程安全任务队列
4. **ResourceLoadResult** - 加载结果

---

## 实现方案

### 1. 定义加载任务结构

```cpp
// include/render/async_resource_loader.h

#pragma once

#include "render/types.h"
#include "render/mesh.h"
#include "render/texture.h"
#include "render/material.h"
#include <memory>
#include <string>
#include <functional>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

namespace Render {

/**
 * @brief 资源类型
 */
enum class AsyncResourceType {
    Mesh,
    Texture,
    Material,
    Model  // 包含多个网格和材质
};

/**
 * @brief 加载状态
 */
enum class LoadStatus {
    Pending,        // 等待中
    Loading,        // 加载中（工作线程）
    Loaded,         // 已加载（等待GPU上传）
    Uploading,      // 上传中（主线程）
    Completed,      // 完成
    Failed          // 失败
};

/**
 * @brief 加载结果
 */
template<typename T>
struct LoadResult {
    std::shared_ptr<T> resource;
    std::string name;
    LoadStatus status;
    std::string errorMessage;
    
    bool IsSuccess() const { 
        return status == LoadStatus::Completed; 
    }
};

/**
 * @brief 加载任务（模板）
 */
template<typename T>
struct LoadTask {
    using LoadFunc = std::function<std::shared_ptr<T>()>;  // 工作线程执行
    using UploadFunc = std::function<void(std::shared_ptr<T>)>;  // 主线程执行
    using CallbackFunc = std::function<void(const LoadResult<T>&)>;  // 完成回调
    
    std::string name;               // 资源名称
    AsyncResourceType type;         // 资源类型
    LoadFunc loadFunc;              // 加载函数（工作线程）
    UploadFunc uploadFunc;          // 上传函数（主线程）
    CallbackFunc callback;          // 完成回调（可选）
    
    std::shared_ptr<T> result;      // 加载结果
    std::atomic<LoadStatus> status{LoadStatus::Pending};
    std::string errorMessage;
    float priority = 0.0f;          // 优先级（越高越优先）
};

// 类型别名
using MeshLoadTask = LoadTask<Mesh>;
using TextureLoadTask = LoadTask<Texture>;
using MaterialLoadTask = LoadTask<Material>;

/**
 * @brief 异步资源加载器（单例）
 * 
 * 提供异步资源加载功能，支持后台线程加载和主线程GPU上传
 * 
 * 线程模型：
 * - 工作线程池：执行文件I/O和数据解析
 * - 主线程：轮询完成的任务并执行GPU上传
 * 
 * 使用流程：
 * 1. 主线程提交加载任务（LoadMeshAsync）
 * 2. 工作线程执行文件加载和数据解析
 * 3. 主线程轮询并处理完成的任务（ProcessCompletedTasks）
 * 4. 主线程执行GPU上传
 * 5. 调用完成回调（如果提供）
 * 
 * 线程安全：
 * - ✅ 所有公共方法都是线程安全的
 * - ✅ 使用任务队列和条件变量同步
 * - ✅ GPU上传保证在主线程执行
 */
class AsyncResourceLoader {
public:
    /**
     * @brief 获取单例实例
     */
    static AsyncResourceLoader& GetInstance();
    
    /**
     * @brief 初始化异步加载器
     * @param numThreads 工作线程数（默认为CPU核心数）
     */
    void Initialize(size_t numThreads = 0);
    
    /**
     * @brief 关闭异步加载器（等待所有任务完成）
     */
    void Shutdown();
    
    // ========================================================================
    // 异步加载接口
    // ========================================================================
    
    /**
     * @brief 异步加载网格
     * @param filepath 文件路径
     * @param name 资源名称（用于ResourceManager注册）
     * @param callback 完成回调（可选）
     * @param priority 优先级（默认0，越高越优先）
     * @return 任务句柄（可用于查询状态）
     */
    std::shared_ptr<MeshLoadTask> LoadMeshAsync(
        const std::string& filepath,
        const std::string& name = "",
        std::function<void(const LoadResult<Mesh>&)> callback = nullptr,
        float priority = 0.0f
    );
    
    /**
     * @brief 异步加载纹理
     */
    std::shared_ptr<TextureLoadTask> LoadTextureAsync(
        const std::string& filepath,
        const std::string& name = "",
        bool generateMipmap = true,
        std::function<void(const LoadResult<Texture>&)> callback = nullptr,
        float priority = 0.0f
    );
    
    /**
     * @brief 异步加载模型（多网格+材质）
     * @param filepath 模型文件路径
     * @param name 资源名称前缀
     * @param shader 材质着色器
     * @param callback 完成回调
     * @param priority 优先级
     * @return 任务列表
     */
    std::vector<std::shared_ptr<MeshLoadTask>> LoadModelAsync(
        const std::string& filepath,
        const std::string& name,
        Ref<Shader> shader,
        std::function<void(size_t loaded, size_t total)> progressCallback = nullptr,
        float priority = 0.0f
    );
    
    // ========================================================================
    // 任务处理（主线程调用）
    // ========================================================================
    
    /**
     * @brief 处理已完成的加载任务（执行GPU上传）
     * @param maxTasks 本帧最多处理的任务数（默认10，避免帧率下降）
     * @return 处理的任务数
     * 
     * 注意：必须在主线程（渲染循环）中调用
     */
    size_t ProcessCompletedTasks(size_t maxTasks = 10);
    
    /**
     * @brief 等待所有任务完成
     * @param timeout 超时时间（秒，0表示无限等待）
     * @return true表示所有任务完成，false表示超时
     */
    bool WaitForAll(float timeout = 0.0f);
    
    // ========================================================================
    // 状态查询
    // ========================================================================
    
    /**
     * @brief 获取待处理任务数
     */
    size_t GetPendingTaskCount() const;
    
    /**
     * @brief 获取正在加载的任务数
     */
    size_t GetLoadingTaskCount() const;
    
    /**
     * @brief 获取等待上传的任务数
     */
    size_t GetWaitingUploadCount() const;
    
    /**
     * @brief 打印加载统计信息
     */
    void PrintStatistics() const;
    
private:
    AsyncResourceLoader();
    ~AsyncResourceLoader();
    
    // 禁止拷贝
    AsyncResourceLoader(const AsyncResourceLoader&) = delete;
    AsyncResourceLoader& operator=(const AsyncResourceLoader&) = delete;
    
    // 工作线程函数
    void WorkerThreadFunc();
    
    // 任务队列
    std::queue<std::shared_ptr<void>> m_pendingTasks;   // 待处理任务
    std::queue<std::shared_ptr<void>> m_completedTasks; // 已完成任务（等待上传）
    
    // 线程同步
    mutable std::mutex m_pendingMutex;
    mutable std::mutex m_completedMutex;
    std::condition_variable m_taskAvailable;
    
    // 工作线程池
    std::vector<std::thread> m_workers;
    std::atomic<bool> m_running{false};
    
    // 统计信息
    std::atomic<size_t> m_totalTasks{0};
    std::atomic<size_t> m_completedTasks{0};
    std::atomic<size_t> m_failedTasks{0};
};

} // namespace Render
```

---

### 2. 实现AsyncResourceLoader

```cpp
// src/core/async_resource_loader.cpp

#include "render/async_resource_loader.h"
#include "render/mesh_loader.h"
#include "render/texture_loader.h"
#include "render/logger.h"
#include "render/gl_thread_checker.h"
#include <algorithm>

namespace Render {

AsyncResourceLoader& AsyncResourceLoader::GetInstance() {
    static AsyncResourceLoader instance;
    return instance;
}

AsyncResourceLoader::AsyncResourceLoader() {
}

AsyncResourceLoader::~AsyncResourceLoader() {
    Shutdown();
}

void AsyncResourceLoader::Initialize(size_t numThreads) {
    if (m_running) {
        Logger::GetInstance().Warning("AsyncResourceLoader: 已经初始化");
        return;
    }
    
    // 默认使用CPU核心数
    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;  // 回退值
    }
    
    Logger::GetInstance().Info("AsyncResourceLoader: 初始化 " + 
                               std::to_string(numThreads) + " 个工作线程");
    
    m_running = true;
    
    // 创建工作线程
    for (size_t i = 0; i < numThreads; ++i) {
        m_workers.emplace_back(&AsyncResourceLoader::WorkerThreadFunc, this);
    }
    
    Logger::GetInstance().Info("AsyncResourceLoader: 初始化完成");
}

void AsyncResourceLoader::Shutdown() {
    if (!m_running) {
        return;
    }
    
    Logger::GetInstance().Info("AsyncResourceLoader: 关闭中...");
    
    // 通知工作线程退出
    m_running = false;
    m_taskAvailable.notify_all();
    
    // 等待所有线程完成
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    m_workers.clear();
    
    Logger::GetInstance().Info("AsyncResourceLoader: 已关闭");
}

void AsyncResourceLoader::WorkerThreadFunc() {
    Logger::GetInstance().Debug("工作线程启动: " + 
                                std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())));
    
    while (m_running) {
        std::shared_ptr<void> task;
        
        // 从队列获取任务
        {
            std::unique_lock<std::mutex> lock(m_pendingMutex);
            
            // 等待任务或退出信号
            m_taskAvailable.wait(lock, [this]() {
                return !m_pendingTasks.empty() || !m_running;
            });
            
            if (!m_running) {
                break;  // 退出线程
            }
            
            if (!m_pendingTasks.empty()) {
                task = m_pendingTasks.front();
                m_pendingTasks.pop();
            }
        }
        
        if (!task) {
            continue;
        }
        
        // 执行任务（锁外执行，避免阻塞）
        try {
            // 尝试转换为不同类型的任务
            if (auto meshTask = std::static_pointer_cast<MeshLoadTask>(task)) {
                meshTask->status = LoadStatus::Loading;
                
                // 执行加载函数（文件I/O和数据解析）
                meshTask->result = meshTask->loadFunc();
                
                if (meshTask->result) {
                    meshTask->status = LoadStatus::Loaded;  // 等待GPU上传
                    
                    // 移到完成队列
                    std::lock_guard<std::mutex> lock(m_completedMutex);
                    m_completedTasks.push(task);
                } else {
                    meshTask->status = LoadStatus::Failed;
                    meshTask->errorMessage = "加载失败";
                    m_failedTasks++;
                }
            }
            // ... 其他资源类型类似处理
            
        } catch (const std::exception& e) {
            Logger::GetInstance().Error("工作线程异常: " + std::string(e.what()));
        }
    }
    
    Logger::GetInstance().Debug("工作线程退出");
}

std::shared_ptr<MeshLoadTask> AsyncResourceLoader::LoadMeshAsync(
    const std::string& filepath,
    const std::string& name,
    std::function<void(const LoadResult<Mesh>&)> callback,
    float priority)
{
    auto task = std::make_shared<MeshLoadTask>();
    task->name = name.empty() ? filepath : name;
    task->type = AsyncResourceType::Mesh;
    task->priority = priority;
    task->callback = callback;
    
    // 定义加载函数（在工作线程执行）
    task->loadFunc = [filepath]() -> Ref<Mesh> {
        Logger::GetInstance().Debug("工作线程加载网格: " + filepath);
        
        // 加载文件并解析（不调用Upload）
        auto meshes = MeshLoader::LoadFromFile(filepath, true);
        if (!meshes.empty()) {
            return meshes[0];  // 返回第一个网格
        }
        return nullptr;
    };
    
    // 定义上传函数（在主线程执行）
    task->uploadFunc = [](Ref<Mesh> mesh) {
        if (mesh && !mesh->IsUploaded()) {
            GL_THREAD_CHECK();  // 确保在主线程
            mesh->Upload();
        }
    };
    
    // 提交任务
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        m_pendingTasks.push(task);
        m_totalTasks++;
    }
    
    // 通知工作线程
    m_taskAvailable.notify_one();
    
    Logger::GetInstance().Info("提交异步加载任务: " + task->name);
    return task;
}

size_t AsyncResourceLoader::ProcessCompletedTasks(size_t maxTasks) {
    GL_THREAD_CHECK();  // 确保在主线程
    
    size_t processed = 0;
    
    for (size_t i = 0; i < maxTasks; ++i) {
        std::shared_ptr<void> task;
        
        // 获取已完成的任务
        {
            std::lock_guard<std::mutex> lock(m_completedMutex);
            if (m_completedTasks.empty()) {
                break;
            }
            task = m_completedTasks.front();
            m_completedTasks.pop();
        }
        
        if (!task) {
            break;
        }
        
        try {
            // 处理网格任务
            if (auto meshTask = std::static_pointer_cast<MeshLoadTask>(task)) {
                meshTask->status = LoadStatus::Uploading;
                
                // 执行GPU上传（主线程）
                if (meshTask->uploadFunc && meshTask->result) {
                    meshTask->uploadFunc(meshTask->result);
                }
                
                meshTask->status = LoadStatus::Completed;
                m_completedTasks++;
                processed++;
                
                // 调用回调
                if (meshTask->callback) {
                    LoadResult<Mesh> result;
                    result.resource = meshTask->result;
                    result.name = meshTask->name;
                    result.status = LoadStatus::Completed;
                    meshTask->callback(result);
                }
                
                Logger::GetInstance().Info("✅ 异步加载完成: " + meshTask->name);
            }
            // ... 其他类型
            
        } catch (const std::exception& e) {
            Logger::GetInstance().Error("GPU上传异常: " + std::string(e.what()));
        }
    }
    
    return processed;
}

size_t AsyncResourceLoader::GetPendingTaskCount() const {
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    return m_pendingTasks.size();
}

size_t AsyncResourceLoader::GetWaitingUploadCount() const {
    std::lock_guard<std::mutex> lock(m_completedMutex);
    return m_completedTasks.size();
}

void AsyncResourceLoader::PrintStatistics() const {
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("异步加载器统计");
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("总任务数: " + std::to_string(m_totalTasks.load()));
    Logger::GetInstance().Info("已完成: " + std::to_string(m_completedTasks.load()));
    Logger::GetInstance().Info("失败: " + std::to_string(m_failedTasks.load()));
    Logger::GetInstance().Info("待处理: " + std::to_string(GetPendingTaskCount()));
    Logger::GetInstance().Info("等待上传: " + std::to_string(GetWaitingUploadCount()));
    Logger::GetInstance().Info("========================================");
}

} // namespace Render
```

---

## 使用示例

### 示例1: 基本异步加载

```cpp
#include <render/async_resource_loader.h>
#include <render/resource_manager.h>

int main() {
    // 初始化渲染器
    Renderer renderer;
    renderer.Initialize();
    
    // 初始化异步加载器（4个工作线程）
    auto& asyncLoader = AsyncResourceLoader::GetInstance();
    asyncLoader.Initialize(4);
    
    auto& resMgr = ResourceManager::GetInstance();
    
    // 提交异步加载任务
    auto task1 = asyncLoader.LoadMeshAsync(
        "models/character.fbx",
        "character_mesh",
        [&resMgr](const LoadResult<Mesh>& result) {
            if (result.IsSuccess()) {
                // 注册到资源管理器
                resMgr.RegisterMesh(result.name, result.resource);
                Logger::GetInstance().Info("✅ 网格加载完成: " + result.name);
            }
        }
    );
    
    auto task2 = asyncLoader.LoadTextureAsync(
        "textures/ground.png",
        "ground_texture",
        true,  // 生成mipmap
        [&resMgr](const LoadResult<Texture>& result) {
            if (result.IsSuccess()) {
                resMgr.RegisterTexture(result.name, result.resource);
                Logger::GetInstance().Info("✅ 纹理加载完成: " + result.name);
            }
        }
    );
    
    Logger::GetInstance().Info("异步加载任务已提交");
    
    // 主循环
    bool loading = true;
    while (running) {
        // 处理事件
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
        }
        
        // ✅ 关键：在主线程处理完成的任务
        if (loading) {
            size_t processed = asyncLoader.ProcessCompletedTasks(10);  // 每帧最多10个
            
            if (task1->status == LoadStatus::Completed && 
                task2->status == LoadStatus::Completed) {
                loading = false;
                Logger::GetInstance().Info("所有资源加载完成！");
            }
        }
        
        // 渲染
        renderer.BeginFrame();
        renderer.Clear();
        
        // 渲染场景...
        
        renderer.EndFrame();
        renderer.Present();
    }
    
    // 清理
    asyncLoader.Shutdown();
    renderer.Shutdown();
    
    return 0;
}
```

---

### 示例2: 加载大型PMX模型（异步）

```cpp
#include <render/async_resource_loader.h>

void LoadMikuModelAsync() {
    auto& asyncLoader = AsyncResourceLoader::GetInstance();
    auto& resMgr = ResourceManager::GetInstance();
    
    // 加载着色器（同步，因为很快）
    auto shader = ShaderCache::GetInstance().LoadShader(
        "phong", "shaders/material_phong.vert", "shaders/material_phong.frag");
    
    // 异步加载模型
    Logger::GetInstance().Info("开始异步加载Miku模型...");
    
    auto tasks = asyncLoader.LoadModelAsync(
        "models/miku/v4c5.0.pmx",
        "miku",
        shader,
        [](size_t loaded, size_t total) {
            float progress = (float)loaded / total * 100.0f;
            Logger::GetInstance().Info("模型加载进度: " + 
                                       std::to_string((int)progress) + "%");
        },
        10.0f  // 高优先级
    );
    
    Logger::GetInstance().Info("提交了 " + std::to_string(tasks.size()) + " 个网格加载任务");
    
    // 在渲染循环中处理
    // （见示例1的ProcessCompletedTasks调用）
}
```

---

### 示例3: 场景切换时的异步加载

```cpp
class SceneManager {
private:
    AsyncResourceLoader& m_asyncLoader = AsyncResourceLoader::GetInstance();
    ResourceManager& m_resMgr = ResourceManager::GetInstance();
    std::vector<std::shared_ptr<MeshLoadTask>> m_loadingTasks;
    bool m_loading = false;
    
public:
    void LoadSceneAsync(const std::string& sceneName) {
        Logger::GetInstance().Info("开始加载场景: " + sceneName);
        
        // 清理旧场景
        m_resMgr.CleanupUnused(0);
        m_loadingTasks.clear();
        m_loading = true;
        
        // 提交加载任务
        if (sceneName == "level1") {
            m_loadingTasks.push_back(
                m_asyncLoader.LoadMeshAsync("models/level1/ground.obj", "level1_ground")
            );
            m_loadingTasks.push_back(
                m_asyncLoader.LoadMeshAsync("models/level1/building1.fbx", "building1")
            );
            // ... 更多资源
        }
        
        Logger::GetInstance().Info("提交了 " + std::to_string(m_loadingTasks.size()) + 
                                   " 个加载任务");
    }
    
    void Update() {
        if (!m_loading) {
            return;
        }
        
        // 检查所有任务是否完成
        bool allCompleted = true;
        for (const auto& task : m_loadingTasks) {
            if (task->status != LoadStatus::Completed &&
                task->status != LoadStatus::Failed) {
                allCompleted = false;
                break;
            }
        }
        
        if (allCompleted) {
            m_loading = false;
            Logger::GetInstance().Info("✅ 场景加载完成");
            
            // 打印统计
            m_asyncLoader.PrintStatistics();
        }
    }
    
    bool IsLoading() const { return m_loading; }
    
    float GetLoadingProgress() const {
        if (m_loadingTasks.empty()) return 1.0f;
        
        size_t completed = 0;
        for (const auto& task : m_loadingTasks) {
            if (task->status == LoadStatus::Completed) {
                completed++;
            }
        }
        
        return (float)completed / m_loadingTasks.size();
    }
};

// 使用
SceneManager sceneManager;
sceneManager.LoadSceneAsync("level1");

// 在渲染循环中
while (running) {
    // 处理异步任务
    asyncLoader.ProcessCompletedTasks(10);
    sceneManager.Update();
    
    // 渲染加载界面
    if (sceneManager.IsLoading()) {
        RenderLoadingScreen(sceneManager.GetLoadingProgress());
    } else {
        RenderGame();
    }
}
```

---

## 高级功能

### 优先级队列

```cpp
// 修改任务队列为优先级队列
struct TaskCompare {
    template<typename T>
    bool operator()(const std::shared_ptr<LoadTask<T>>& a,
                   const std::shared_ptr<LoadTask<T>>& b) const {
        return a->priority < b->priority;  // 优先级高的先执行
    }
};

std::priority_queue<std::shared_ptr<void>, 
                    std::vector<std::shared_ptr<void>>, 
                    TaskCompare> m_pendingTasks;

// 使用
asyncLoader.LoadMeshAsync("important.obj", "important", nullptr, 100.0f);  // 高优先级
asyncLoader.LoadMeshAsync("background.obj", "bg", nullptr, 1.0f);  // 低优先级
```

---

### 流式加载（边加载边显示）

```cpp
void LoadLargeModelStreaming() {
    auto& asyncLoader = AsyncResourceLoader::GetInstance();
    auto& resMgr = ResourceManager::GetInstance();
    
    // 加载模型
    auto modelParts = MeshLoader::LoadFromFileWithMaterials("models/large_model.fbx");
    
    Logger::GetInstance().Info("模型包含 " + std::to_string(modelParts.size()) + " 个部件");
    
    // 为每个部件创建异步任务
    for (size_t i = 0; i < modelParts.size(); ++i) {
        const auto& part = modelParts[i];
        std::string meshName = "model_part_" + std::to_string(i);
        
        // 创建上传任务（数据已加载，只需上传）
        auto task = std::make_shared<MeshLoadTask>();
        task->name = meshName;
        task->result = part.mesh;  // 直接使用已加载的网格
        task->status = LoadStatus::Loaded;  // 跳过加载阶段
        task->uploadFunc = [meshName, &resMgr](Ref<Mesh> mesh) {
            mesh->Upload();
            resMgr.RegisterMesh(meshName, mesh);
            Logger::GetInstance().Debug("部件上传完成: " + meshName);
        };
        
        // 提交到完成队列（等待上传）
        std::lock_guard<std::mutex> lock(asyncLoader.m_completedMutex);
        asyncLoader.m_completedTasks.push(task);
    }
    
    Logger::GetInstance().Info("流式上传任务已提交");
}

// 在渲染循环中
while (running) {
    // 每帧处理5个上传任务（流式显示）
    size_t uploaded = asyncLoader.ProcessCompletedTasks(5);
    
    if (uploaded > 0) {
        Logger::GetInstance().Debug("本帧上传了 " + std::to_string(uploaded) + " 个网格");
    }
    
    // 渲染已上传的部件
    RenderUploadedParts();
}
```

---

## 性能对比

### 同步加载 vs 异步加载

| 场景 | 同步加载 | 异步加载 | 改善 |
|------|----------|----------|------|
| 加载100个网格 | 主线程阻塞5秒 | 后台加载，主线程流畅 | ✅ 0卡顿 |
| 加载大型PMX模型 | 窗口无响应3秒 | 显示加载进度，可交互 | ✅ 用户体验佳 |
| 场景切换 | 黑屏等待 | 渐进式显示 | ✅ 流畅过渡 |
| 首次启动 | 长时间白屏 | 显示Logo/进度条 | ✅ 专业体验 |

---

## 注意事项

### 1. OpenGL上下文限制

⚠️ **关键**: GPU上传必须在主线程

```cpp
// ✅ 正确：分离加载和上传
task->loadFunc = []() {
    // 工作线程：文件I/O和数据解析
    auto mesh = LoadMeshData("file.obj");  // 不调用Upload
    return mesh;
};

task->uploadFunc = [](Ref<Mesh> mesh) {
    // 主线程：GPU上传
    GL_THREAD_CHECK();
    mesh->Upload();
};

// ❌ 错误：在工作线程调用Upload
task->loadFunc = []() {
    auto mesh = MeshLoader::LoadFromFile("file.obj");  // 内部调用Upload！
    return mesh;  // ❌ 会崩溃！
};
```

### 2. 内存管理

```cpp
// 大量异步加载时注意内存使用
void LoadManyModels() {
    auto& asyncLoader = AsyncResourceLoader::GetInstance();
    
    // ❌ 可能导致内存不足
    for (int i = 0; i < 1000; i++) {
        asyncLoader.LoadMeshAsync("model_" + std::to_string(i) + ".fbx", ...);
    }
    
    // ✅ 推荐：分批加载
    const size_t BATCH_SIZE = 50;
    for (size_t batch = 0; batch < 1000; batch += BATCH_SIZE) {
        // 加载一批
        for (size_t i = batch; i < batch + BATCH_SIZE && i < 1000; i++) {
            asyncLoader.LoadMeshAsync(...);
        }
        
        // 等待这批完成
        asyncLoader.WaitForAll();
        
        // 处理上传
        while (asyncLoader.GetWaitingUploadCount() > 0) {
            asyncLoader.ProcessCompletedTasks(10);
        }
    }
}
```

### 3. 任务生命周期

```cpp
// ✅ 保持任务句柄的引用
std::vector<std::shared_ptr<MeshLoadTask>> m_activeTasks;

void LoadResources() {
    auto task = asyncLoader.LoadMeshAsync(...);
    m_activeTasks.push_back(task);  // 保持引用
}

// ❌ 不保持引用可能导致任务被过早释放
void LoadResources() {
    asyncLoader.LoadMeshAsync(...);  // 任务可能在完成前被释放
}
```

---

## 集成到现有代码

### 修改MeshLoader支持非上传加载

```cpp
// include/render/mesh_loader.h

class MeshLoader {
public:
    /**
     * @brief 从文件加载模型（不上传GPU）
     * @param filepath 文件路径
     * @param flipUVs 是否翻转UV
     * @param autoUpload 是否自动上传（默认false，用于异步加载）
     * @return 网格列表
     */
    static std::vector<Ref<Mesh>> LoadFromFile(
        const std::string& filepath,
        bool flipUVs = true,
        bool autoUpload = true  // ⭐ 新增参数
    );
};

// src/rendering/mesh_loader.cpp

std::vector<Ref<Mesh>> MeshLoader::LoadFromFile(
    const std::string& filepath,
    bool flipUVs,
    bool autoUpload)  // ⭐ 新增
{
    // ... 加载和解析数据 ...
    
    // 创建网格对象
    auto mesh = CreateRef<Mesh>(vertices, indices);
    
    // ⭐ 条件上传
    if (autoUpload) {
        mesh->Upload();  // 同步上传
    }
    // 否则返回未上传的网格，由调用者决定何时上传
    
    return meshes;
}
```

---

## 性能建议

### 1. 工作线程数

```cpp
// CPU核心数 = 8
// - 推荐4-6个工作线程（留出核心给主线程和系统）
asyncLoader.Initialize(6);

// CPU核心数 = 4
// - 推荐2-3个工作线程
asyncLoader.Initialize(2);
```

### 2. 每帧处理任务数

```cpp
// 60 FPS目标 (16.67ms/帧)
asyncLoader.ProcessCompletedTasks(5);   // 保守，约5ms

// 30 FPS目标 (33.33ms/帧)
asyncLoader.ProcessCompletedTasks(10);  // 适中，约10ms

// 加载界面（FPS不重要）
asyncLoader.ProcessCompletedTasks(50);  // 激进，快速完成
```

### 3. 批次大小

```cpp
// 小型资源（快速加载）
MeshLoader::BatchUpload(meshes, 10);  // 每批10个

// 大型资源（慢速加载）
MeshLoader::BatchUpload(meshes, 3);   // 每批3个，避免卡顿
```

---

## 与现有系统集成

### 与ResourceManager集成

```cpp
class AsyncResourceManager {
public:
    // 异步加载并自动注册到ResourceManager
    void LoadMeshAsync(const std::string& filepath, const std::string& name) {
        auto& asyncLoader = AsyncResourceLoader::GetInstance();
        auto& resMgr = ResourceManager::GetInstance();
        
        asyncLoader.LoadMeshAsync(
            filepath,
            name,
            [&resMgr, name](const LoadResult<Mesh>& result) {
                if (result.IsSuccess()) {
                    resMgr.RegisterMesh(name, result.resource);
                }
            }
        );
    }
    
    // 在渲染循环调用
    void Update() {
        AsyncResourceLoader::GetInstance().ProcessCompletedTasks(10);
    }
};
```

---

## 最佳实践

### ✅ DO

1. **在主循环处理完成任务**
```cpp
while (running) {
    asyncLoader.ProcessCompletedTasks(10);  // 每帧处理
    // ... 渲染 ...
}
```

2. **提供加载反馈**
```cpp
auto task = asyncLoader.LoadMeshAsync(..., callback);
// 显示加载动画/进度条
```

3. **异常处理**
```cpp
asyncLoader.LoadMeshAsync(..., [](const LoadResult<Mesh>& result) {
    if (!result.IsSuccess()) {
        Logger::GetInstance().Error("加载失败: " + result.errorMessage);
        // 使用默认资源
    }
});
```

### ❌ DON'T

1. **不要在工作线程调用OpenGL**
```cpp
// ❌ 错误
task->loadFunc = []() {
    auto mesh = ...;
    mesh->Upload();  // ❌ 崩溃！
    return mesh;
};
```

2. **不要忘记处理完成任务**
```cpp
// ❌ 忘记调用ProcessCompletedTasks
asyncLoader.LoadMeshAsync(...);
// 任务加载完成但永远不会上传到GPU
```

3. **不要同时加载过多资源**
```cpp
// ❌ 内存爆炸
for (int i = 0; i < 10000; i++) {
    asyncLoader.LoadMeshAsync(...);
}
```

---

## 实施路线图

### Phase 1: 基础框架（2天）

- [ ] 创建`AsyncResourceLoader`类
- [ ] 实现任务队列和工作线程池
- [ ] 实现基本的异步网格加载
- [ ] 单元测试

### Phase 2: 功能完善（2天）

- [ ] 添加纹理异步加载
- [ ] 添加模型异步加载（多网格+材质）
- [ ] 实现优先级队列
- [ ] 添加进度回调

### Phase 3: 集成和优化（1天）

- [ ] 与ResourceManager集成
- [ ] 性能测试和优化
- [ ] 文档编写
- [ ] 示例程序（29_async_loading_test）

### Phase 4: 高级特性（可选，2天）

- [ ] 流式加载（LOD）
- [ ] 依赖管理（资源间依赖）
- [ ] 缓存预热（预加载常用资源）
- [ ] 内存限制和LRU淘汰

---
