# 异步资源加载器优化文档

## 概览

本文档分析了 `AsyncResourceLoader` 的当前实现,识别潜在问题并提供优化建议。

---

## 1. 严重问题 (Critical Issues)

### 1.1 任务队列缺少优先级调度

**问题描述:**
- 头文件中 `LoadTaskBase` 定义了 `priority` 字段
- 但 `m_pendingTasks` 使用 `std::queue`,按 FIFO 顺序处理
- **高优先级任务可能被大量低优先级任务阻塞**

**影响:**
- 用户交互需要的关键资源(如玩家附近的模型)加载延迟
- 优先级参数形同虚设

**解决方案:**
```cpp
// 使用优先级队列替代普通队列
std::priority_queue<
    std::shared_ptr<LoadTaskBase>,
    std::vector<std::shared_ptr<LoadTaskBase>>,
    TaskComparator
> m_pendingTasks;

// 定义比较器
struct TaskComparator {
    bool operator()(const std::shared_ptr<LoadTaskBase>& a,
                   const std::shared_ptr<LoadTaskBase>& b) const {
        return a->priority < b->priority; // 高优先级优先
    }
};
```

---

### 1.2 Shutdown() 存在资源泄漏风险

**问题描述:**
```cpp
void AsyncResourceLoader::Shutdown() {
    m_running = false;
    m_taskAvailable.notify_all();
    
    // 等待线程退出
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    ClearAllPendingTasks();  // ✅ 在线程退出后清理
}
```

**潜在风险:**
1. 如果工作线程在 `notify_all()` 和 `join()` 之间仍在处理任务
2. `ClearAllPendingTasks()` 可能清理正在被访问的任务
3. 虽然当前实现在 `join()` 后清理,但缺少明确的任务取消机制

**建议优化:**
```cpp
void AsyncResourceLoader::Shutdown() {
    // 1. 设置关闭标志
    m_running = false;
    
    // 2. 唤醒所有等待线程
    m_taskAvailable.notify_all();
    
    // 3. 等待所有线程完成当前任务并退出
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    m_workers.clear();
    
    // 4. 安全清理剩余任务(此时已无线程访问)
    ClearAllPendingTasks();
    
    // 5. 打印统计
    PrintStatistics();
}
```

---

### 1.3 缺少任务取消机制

**问题描述:**
- 用户无法取消已提交的任务
- 场景切换时,旧场景的资源仍在加载

**影响:**
- 浪费CPU/IO资源加载不再需要的资源
- 增加内存压力

**解决方案:**
```cpp
// 添加任务句柄方法
class LoadTaskBase {
public:
    std::atomic<bool> cancelled{false};
    
    void Cancel() { 
        cancelled = true; 
        status = LoadStatus::Failed;
        errorMessage = "Task cancelled by user";
    }
    
    bool IsCancelled() const { return cancelled.load(); }
};

// 在工作线程中检查
void AsyncResourceLoader::WorkerThreadFunc() {
    while (m_running.load()) {
        // ... 获取任务 ...
        
        if (task->IsCancelled()) {
            continue; // 跳过已取消的任务
        }
        
        task->ExecuteLoad();
        
        // 加载完成后再次检查
        if (task->IsCancelled()) {
            continue;
        }
        
        // ... 加入完成队列 ...
    }
}
```

---

## 2. 性能优化 (Performance)

### 2.1 减少锁竞争

**问题描述:**
- `m_pendingMutex` 和 `m_completedMutex` 在高频访问时可能成为瓶颈
- 每次提交/获取任务都需要加锁

**优化方案 - 无锁队列:**
```cpp
// 使用无锁并发队列(需引入第三方库如 moodycamel::ConcurrentQueue)
#include <concurrentqueue.h>

moodycamel::ConcurrentQueue<std::shared_ptr<LoadTaskBase>> m_pendingTasks;
moodycamel::ConcurrentQueue<std::shared_ptr<LoadTaskBase>> m_completedTasks;

// 提交任务无需加锁
void SubmitTask(std::shared_ptr<LoadTaskBase> task) {
    m_pendingTasks.enqueue(task);
    m_taskAvailable.notify_one();
}

// 获取任务无需加锁
bool TryGetTask(std::shared_ptr<LoadTaskBase>& task) {
    return m_pendingTasks.try_dequeue(task);
}
```

**优化方案 - 批量处理:**
```cpp
// 批量处理已完成任务,减少锁获取次数
size_t ProcessCompletedTasks(size_t maxTasks) {
    std::vector<std::shared_ptr<LoadTaskBase>> tasks;
    
    // 一次性获取多个任务
    {
        std::lock_guard<std::mutex> lock(m_completedMutex);
        size_t count = std::min(maxTasks, m_completedTasks.size());
        tasks.reserve(count);
        
        for (size_t i = 0; i < count; ++i) {
            tasks.push_back(m_completedTasks.front());
            m_completedTasks.pop();
        }
    }
    
    // 无锁处理任务
    for (auto& task : tasks) {
        ProcessSingleTask(task);
    }
    
    return tasks.size();
}
```

---

### 2.2 优化线程池利用率

**问题描述:**
```cpp
// 当前实现在无任务时线程阻塞等待
m_taskAvailable.wait(lock, [this]() {
    return !m_pendingTasks.empty() || !m_running.load();
});
```

**优化建议:**
- 添加超时等待,允许线程定期检查状态
- 实现工作窃取(work stealing)提高多核利用率

```cpp
// 超时等待避免永久阻塞
m_taskAvailable.wait_for(lock, std::chrono::milliseconds(100), [this]() {
    return !m_pendingTasks.empty() || !m_running.load();
});

// 如果长时间无任务,可以动态调整线程数
if (idleTime > threshold) {
    // 考虑暂停部分线程
}
```

---

### 2.3 内存池优化

**问题描述:**
- 频繁创建/销毁 `shared_ptr<LoadTask>` 可能导致内存碎片
- 大量小对象分配影响性能

**解决方案:**
```cpp
// 任务对象池
template<typename T>
class ObjectPool {
public:
    std::shared_ptr<T> Acquire() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_pool.empty()) {
            return std::make_shared<T>();
        }
        
        auto obj = m_pool.back();
        m_pool.pop_back();
        return obj;
    }
    
    void Release(std::shared_ptr<T> obj) {
        // 重置对象状态
        obj->Reset();
        
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_pool.size() < MAX_POOL_SIZE) {
            m_pool.push_back(obj);
        }
    }

private:
    std::vector<std::shared_ptr<T>> m_pool;
    std::mutex m_mutex;
    static constexpr size_t MAX_POOL_SIZE = 128;
};

// 使用方式
ObjectPool<MeshLoadTask> m_meshTaskPool;

auto task = m_meshTaskPool.Acquire();
// ... 使用任务 ...
m_meshTaskPool.Release(task);
```

---

## 3. 功能增强 (Features)

### 3.1 添加进度跟踪

**需求:**
- 显示加载进度条
- 实时查询某个任务的完成百分比

**实现:**
```cpp
struct LoadTaskBase {
    std::atomic<float> progress{0.0f}; // 0.0 - 1.0
    
    void UpdateProgress(float value) {
        progress = std::clamp(value, 0.0f, 1.0f);
    }
};

// 在加载函数中更新进度
task->loadFunc = [task]() {
    task->UpdateProgress(0.0f);
    
    // 读取文件
    task->UpdateProgress(0.3f);
    
    // 解析数据
    task->UpdateProgress(0.6f);
    
    // 创建对象
    task->UpdateProgress(0.9f);
    
    task->UpdateProgress(1.0f);
    return result;
};

// 查询总体进度
float AsyncResourceLoader::GetOverallProgress() const {
    size_t total = m_totalTasks.load();
    if (total == 0) return 1.0f;
    
    size_t completed = m_completedCount.load();
    size_t loading = GetLoadingTaskCount();
    
    // 简化计算:已完成 + 正在加载的平均进度
    return static_cast<float>(completed) / total;
}
```

---

### 3.2 依赖关系管理

**需求:**
- 模型依赖纹理和网格
- 纹理加载完成前,模型不应上传

**实现:**
```cpp
struct LoadTaskBase {
    std::vector<std::weak_ptr<LoadTaskBase>> dependencies;
    
    bool AreDependenciesReady() const {
        for (const auto& weak : dependencies) {
            if (auto dep = weak.lock()) {
                if (dep->status != LoadStatus::Completed) {
                    return false;
                }
            }
        }
        return true;
    }
};

// 提交时建立依赖
auto textureTask = LoadTextureAsync("texture.png");
auto modelTask = LoadModelAsync("model.obj");
modelTask->dependencies.push_back(textureTask);

// 处理时检查依赖
size_t ProcessCompletedTasks(size_t maxTasks) {
    std::vector<std::shared_ptr<LoadTaskBase>> readyTasks;
    
    {
        std::lock_guard<std::mutex> lock(m_completedMutex);
        while (!m_completedTasks.empty() && readyTasks.size() < maxTasks) {
            auto task = m_completedTasks.front();
            
            if (task->AreDependenciesReady()) {
                readyTasks.push_back(task);
                m_completedTasks.pop();
            } else {
                // 重新入队等待依赖完成
                m_completedTasks.pop();
                m_completedTasks.push(task);
                break; // 避免死循环
            }
        }
    }
    
    // 处理就绪任务...
}
```

---

### 3.3 资源预加载提示

**需求:**
- 用户提前告知即将需要的资源
- 加载器可以调整优先级和调度策略

**实现:**
```cpp
// 预热接口
void AsyncResourceLoader::PreloadResources(
    const std::vector<std::string>& filepaths,
    AsyncResourceType type,
    float basePriority = 1.0f)
{
    for (size_t i = 0; i < filepaths.size(); ++i) {
        float priority = basePriority - (i * 0.1f); // 越靠前优先级越高
        
        switch (type) {
        case AsyncResourceType::Texture:
            LoadTextureAsync(filepaths[i], "", true, nullptr, priority);
            break;
        case AsyncResourceType::Mesh:
            LoadMeshAsync(filepaths[i], "", nullptr, priority);
            break;
        // ...
        }
    }
}

// 使用示例
loader.PreloadResources({
    "level2/terrain.obj",
    "level2/sky.png",
    "level2/props.obj"
}, AsyncResourceType::Mesh, 5.0f); // 高优先级
```

---

## 4. 错误处理 (Error Handling)

### 4.1 增强错误恢复

**问题描述:**
- 当前失败任务仅记录错误,未尝试重试
- 网络资源加载失败应支持重试机制

**解决方案:**
```cpp
struct LoadTaskBase {
    size_t retryCount = 0;
    size_t maxRetries = 3;
    std::chrono::milliseconds retryDelay{1000};
    
    bool ShouldRetry() const {
        return retryCount < maxRetries && 
               status == LoadStatus::Failed;
    }
};

// 工作线程中实现重试
void AsyncResourceLoader::WorkerThreadFunc() {
    while (m_running.load()) {
        // ... 获取任务 ...
        
        task->ExecuteLoad();
        
        if (task->status == LoadStatus::Failed && task->ShouldRetry()) {
            task->retryCount++;
            
            Logger::GetInstance().Warning(
                "任务失败,重试 " + std::to_string(task->retryCount) + 
                "/" + std::to_string(task->maxRetries));
            
            // 延迟后重新入队
            std::this_thread::sleep_for(task->retryDelay);
            
            {
                std::lock_guard<std::mutex> lock(m_pendingMutex);
                m_pendingTasks.push(task);
            }
            continue;
        }
        
        // ... 正常处理 ...
    }
}
```

---

### 4.2 详细错误分类

**改进:**
```cpp
enum class LoadErrorType {
    None,
    FileNotFound,
    ParseError,
    OutOfMemory,
    GPUUploadFailed,
    InvalidFormat,
    NetworkTimeout,
    Cancelled
};

struct LoadTaskBase {
    LoadErrorType errorType = LoadErrorType::None;
    std::string errorMessage;
    std::string errorDetails; // 堆栈跟踪或详细信息
};

// 在回调中可以根据错误类型采取不同措施
callback([](const MeshLoadResult& result) {
    if (!result.IsSuccess()) {
        switch (result.errorType) {
        case LoadErrorType::FileNotFound:
            // 使用默认网格替代
            break;
        case LoadErrorType::OutOfMemory:
            // 释放缓存,重试
            break;
        // ...
        }
    }
});
```

---

## 5. 线程安全 (Thread Safety)

### 5.1 统计计数器的原子性问题

**潜在问题:**
```cpp
// 当前实现
m_loadingCount++;  // ✅ atomic,线程安全

// 但如果有复合操作:
if (m_loadingCount.load() < MAX_CONCURRENT) {
    m_loadingCount++;  // ❌ 竞态条件!
}
```

**虽然当前代码未出现此问题,但需注意**

**最佳实践:**
```cpp
// 使用 compare_exchange 确保原子性
size_t expected = m_loadingCount.load();
size_t desired;
do {
    if (expected >= MAX_CONCURRENT) {
        return false; // 拒绝任务
    }
    desired = expected + 1;
} while (!m_loadingCount.compare_exchange_weak(expected, desired));
```

---

### 5.2 回调函数的线程安全

**问题描述:**
- 回调函数在主线程执行,但可能访问用户对象
- 用户对象可能在其他线程被销毁

**建议:**
```cpp
// 文档说明回调的线程模型
/**
 * @brief 完成回调
 * @note 回调函数在主线程(OpenGL上下文线程)中执行
 * @warning 如果回调捕获对象引用,确保对象生命周期足够长
 * @warning 避免在回调中执行耗时操作,会阻塞渲染
 */
using CallbackFunc = std::function<void(const MeshLoadResult&)>;

// 使用 weak_ptr 避免悬挂指针
class SceneManager {
public:
    void LoadMesh(const std::string& path) {
        auto weakThis = weak_from_this();
        
        loader.LoadMeshAsync(path, "", [weakThis](const auto& result) {
            if (auto self = weakThis.lock()) {
                self->OnMeshLoaded(result);
            } else {
                // 对象已销毁,忽略回调
            }
        });
    }
};
```

---

## 6. 资源管理 (Resource Management)

### 6.1 内存使用监控

**建议添加:**
```cpp
struct ResourceStats {
    size_t totalMemoryUsed = 0;      // 当前使用内存
    size_t peakMemoryUsed = 0;       // 峰值内存
    size_t textureMemory = 0;        // 纹理内存
    size_t meshMemory = 0;           // 网格内存
};

class AsyncResourceLoader {
private:
    ResourceStats m_stats;
    std::atomic<size_t> m_currentMemory{0};
    
public:
    const ResourceStats& GetStats() const { return m_stats; }
    
    void TrackMemoryAllocation(size_t bytes) {
        size_t current = m_currentMemory.fetch_add(bytes) + bytes;
        
        // 更新峰值
        size_t expected = m_stats.peakMemoryUsed;
        while (current > expected) {
            if (m_stats.peakMemoryUsed.compare_exchange_weak(expected, current)) {
                break;
            }
        }
    }
};
```

---

### 6.2 内存预算控制

**需求:**
- 限制同时加载的资源总大小
- 避免OOM崩溃

**实现:**
```cpp
class AsyncResourceLoader {
private:
    size_t m_memoryBudget = 512 * 1024 * 1024; // 512MB
    std::atomic<size_t> m_estimatedMemory{0};
    
public:
    void SetMemoryBudget(size_t bytes) {
        m_memoryBudget = bytes;
    }
    
    bool CanLoadResource(size_t estimatedSize) {
        return m_estimatedMemory.load() + estimatedSize <= m_memoryBudget;
    }
    
    std::shared_ptr<TextureLoadTask> LoadTextureAsync(...) {
        // 预估纹理大小
        size_t estimatedSize = EstimateTextureSize(filepath);
        
        if (!CanLoadResource(estimatedSize)) {
            Logger::Warning("内存预算不足,延迟加载");
            // 加入低优先级队列或等待
        }
        
        // ... 正常加载 ...
        m_estimatedMemory += estimatedSize;
    }
};
```

---

## 7. 代码质量 (Code Quality)

### 7.1 减少代码重复

**问题描述:**
- `LoadMeshAsync`, `LoadTextureAsync`, `LoadModelAsync` 有大量重复代码

**重构建议:**
```cpp
// 通用任务提交模板
template<typename TaskType>
std::shared_ptr<TaskType> SubmitTask(
    std::shared_ptr<TaskType> task,
    const std::string& name,
    float priority)
{
    if (!m_running.load()) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::NotInitialized, 
                                 "AsyncResourceLoader未初始化"));
        return nullptr;
    }
    
    task->name = name;
    task->priority = priority;
    
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        m_pendingTasks.push(task);
        m_totalTasks++;
    }
    
    m_taskAvailable.notify_one();
    Logger::GetInstance().Info("提交任务: " + task->name);
    
    return task;
}

// 简化接口
std::shared_ptr<MeshLoadTask> LoadMeshAsync(...) {
    auto task = std::make_shared<MeshLoadTask>();
    task->type = AsyncResourceType::Mesh;
    
    // 设置 loadFunc, uploadFunc, callback...
    
    return SubmitTask(task, name.empty() ? filepath : name, priority);
}
```

---

### 7.2 改进日志可读性

**问题描述:**
- 中文日志在某些环境可能乱码
- 缺少统一的日志格式

**建议:**
```cpp
// 添加结构化日志宏
#define ASYNC_LOG_INFO(msg) \
    Logger::GetInstance().InfoFormat("[AsyncLoader] %s", msg)

#define ASYNC_LOG_TASK(threadId, action, taskName) \
    Logger::GetInstance().InfoFormat( \
        "[AsyncLoader][Thread:%zu] %s: %s", \
        threadId, action, taskName.c_str())

// 使用示例
ASYNC_LOG_TASK(threadIdHash, "LoadStart", task->name);
ASYNC_LOG_TASK(threadIdHash, "LoadComplete", task->name);
```

---

## 8. 测试建议 (Testing)

### 8.1 单元测试覆盖

**建议测试场景:**
```cpp
// 1. 基本功能测试
TEST(AsyncLoader, BasicMeshLoading) {
    AsyncResourceLoader& loader = AsyncResourceLoader::GetInstance();
    loader.Initialize(2);
    
    bool callbackCalled = false;
    auto task = loader.LoadMeshAsync("test.obj", "", 
        [&](const auto& result) {
            callbackCalled = true;
            EXPECT_TRUE(result.IsSuccess());
        });
    
    loader.WaitForAll(5.0f);
    loader.ProcessCompletedTasks(10);
    
    EXPECT_TRUE(callbackCalled);
}

// 2. 并发压力测试
TEST(AsyncLoader, ConcurrentLoading) {
    AsyncResourceLoader& loader = AsyncResourceLoader::GetInstance();
    loader.Initialize(4);
    
    std::vector<std::shared_ptr<MeshLoadTask>> tasks;
    for (int i = 0; i < 100; ++i) {
        tasks.push_back(loader.LoadMeshAsync("mesh_" + std::to_string(i) + ".obj"));
    }
    
    EXPECT_TRUE(loader.WaitForAll(30.0f));
    EXPECT_EQ(loader.GetPendingTaskCount(), 0);
}

// 3. 错误处理测试
TEST(AsyncLoader, HandleMissingFile) {
    auto task = loader.LoadMeshAsync("nonexistent.obj");
    loader.WaitForAll(5.0f);
    loader.ProcessCompletedTasks(10);
    
    EXPECT_EQ(task->status, LoadStatus::Failed);
    EXPECT_FALSE(task->errorMessage.empty());
}

// 4. 任务取消测试
TEST(AsyncLoader, TaskCancellation) {
    auto task = loader.LoadMeshAsync("large_mesh.obj");
    task->Cancel();
    
    loader.WaitForAll(5.0f);
    EXPECT_EQ(task->status, LoadStatus::Failed);
}
```

---

## 9. 性能基准 (Benchmarks)

**建议添加性能测试:**
```cpp
void BenchmarkAsyncLoader() {
    AsyncResourceLoader& loader = AsyncResourceLoader::GetInstance();
    
    // 测试1: 吞吐量
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 1000; ++i) {
        loader.LoadTextureAsync("texture_" + std::to_string(i) + ".png");
    }
    
    loader.WaitForAll();
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "加载1000个纹理耗时: " << duration.count() << "ms\n";
    std::cout << "平均每个: " << duration.count() / 1000.0 << "ms\n";
    
    // 测试2: 对比不同线程数的性能
    for (size_t threads : {1, 2, 4, 8}) {
        loader.Initialize(threads);
        // ... 运行相同负载 ...
        // 记录并比较结果
    }
}
```

---

## 10. 总结与优先级

### 🔴 高优先级 (立即修复)
1. **实现优先级队列** - 核心功能缺失
2. **添加任务取消机制** - 资源浪费
3. **增强错误处理** - 提高稳定性

### 🟡 中优先级 (下个版本)
4. 减少锁竞争(无锁队列)
5. 添加进度跟踪
6. 内存预算控制

### 🟢 低优先级 (性能优化)
7. 对象池优化
8. 依赖关系管理
9. 工作窃取调度

### 📊 持续改进
10. 完善单元测试
11. 性能基准测试
12. 代码重构去重

---

## 附录: 参考实现

### A. 完整的优先级队列实现

```cpp
class PriorityTaskQueue {
public:
    void Push(std::shared_ptr<LoadTaskBase> task) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(task);
    }
    
    bool TryPop(std::shared_ptr<LoadTaskBase>& task) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            return false;
        }
        task = m_queue.top();
        m_queue.pop();
        return true;
    }
    
    size_t Size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }
    
    bool Empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

private:
    struct Comparator {
        bool operator()(const std::shared_ptr<LoadTaskBase>& a,
                       const std::shared_ptr<LoadTaskBase>& b) const {
            // 高优先级排在前面
            if (a->priority != b->priority) {
                return a->priority < b->priority;
            }
            // 相同优先级,先提交的先执行
            return a->submitTime > b->submitTime;
        }
    };
    
    std::priority_queue<
        std::shared_ptr<LoadTaskBase>,
        std::vector<std::shared_ptr<LoadTaskBase>>,
        Comparator
    > m_queue;
    
    mutable std::mutex m_mutex;
};
```

---

**文档版本:** 1.0  
**最后更新:** 2025-12-01
**作者:** Linductor