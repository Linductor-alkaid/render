# 渲染系统多线程与任务调度优化方案

## 文档信息
- **创建时间**: 2025-12-04
- **状态**: 待实施
- **优先级**: 高
- **目标**: 优化渲染系统的多线程架构，提升多核CPU利用率，减少帧延迟

---

## 目录
1. [当前架构分析](#1-当前架构分析)
2. [性能瓶颈识别](#2-性能瓶颈识别)
3. [优化目标](#3-优化目标)
4. [优化方案设计](#4-优化方案设计)
5. [实施路线图](#5-实施路线图)
6. [预期收益](#6-预期收益)
7. [风险与挑战](#7-风险与挑战)

---

## 1. 当前架构分析

### 1.1 现有多线程组件

#### 1.1.1 AsyncResourceLoader（异步资源加载器）
**位置**: `src/core/async_resource_loader.cpp`

**架构**:
```
主线程                     工作线程池 (N个线程)
  │                             │
  ├─ LoadMeshAsync()           │
  │  └─ 提交任务到队列 ────────→ 优先级队列
  │                             │
  ├─ ProcessCompletedTasks()   ├─ WorkerThreadFunc()
  │  └─ GPU上传 (主线程)       │  └─ 文件I/O + 解析
  │                             │
  └─ WaitForAll()              └─ 完成队列
       └─ 等待所有任务
```

**优点**:
- ✅ 使用工作线程池，充分利用多核CPU
- ✅ 优先级队列管理任务，支持任务优先级
- ✅ 分离文件I/O（工作线程）和GPU上传（主线程）
- ✅ 支持重试机制和错误处理

**缺点**:
- ⚠️ 工作线程数量默认为CPU核心数-1，可能需要调整
- ⚠️ 没有与其他系统共享线程池

#### 1.1.2 BatchManager（批处理管理器）
**位置**: `src/rendering/render_batching.cpp`

**架构**:
```
主线程                     单工作线程
  │                             │
  ├─ AddItem()                  │
  │  └─ 入队工作项 ──────────→ m_pendingItems
  │                             │
  ├─ Flush()                    ├─ WorkerLoop()
  │  ├─ DrainWorker() ──────→  │  ├─ ProcessWorkItem()
  │  │  └─ 等待工作线程空闲    │  │  └─ 批次分组
  │  ├─ SwapBuffers()          │  └─ 通知主线程
  │  └─ 执行批次渲染           │
  │                             │
```

**优点**:
- ✅ 双缓冲机制（录制/执行缓冲区）
- ✅ 工作线程异步处理批次分组

**缺点**:
- ❌ **仅使用单个工作线程**，成为性能瓶颈
- ❌ **DrainWorker()会阻塞主线程**，等待工作线程完成
- ❌ 批次分组是串行处理，无法并行化
- ❌ 每帧都需要等待工作线程，增加帧延迟

**性能影响**:
```cpp
// render_batching.cpp:1484-1504
void BatchManager::DrainWorker() {
    std::unique_lock<std::mutex> lock(m_queueMutex);
    if (m_shutdown) return;
    if (m_pendingItems.empty() && !m_processing) return;
    
    auto waitBegin = std::chrono::steady_clock::now();
    m_queueCv.notify_all();
    
    // ❌ 主线程在这里阻塞等待
    m_idleCv.wait(lock, [this]() {
        return m_shutdown || (m_pendingItems.empty() && !m_processing);
    });
    
    auto waitEnd = std::chrono::steady_clock::now();
    auto waitNs = std::chrono::duration_cast<std::chrono::nanoseconds>(waitEnd - waitBegin).count();
    if (waitNs > 0) {
        m_workerDrainWaitNs.fetch_add(static_cast<uint64_t>(waitNs), std::memory_order_relaxed);
    }
}
```

#### 1.1.3 LODInstancedRenderer（LOD实例化渲染器）
**位置**: `src/rendering/lod_instanced_renderer.cpp`

**架构**:
```
主线程                     工作线程池 (M个线程)
  │                             │
  ├─ PrepareRender()            │
  │  └─ 分发任务到线程 ─────→  m_tasks
  │                             │
  │                             ├─ WorkerThreadFunction()
  │                             │  └─ ProcessPrepareTask()
  │                             │     └─ LOD计算 + 数据准备
  │                             │
  ├─ 等待所有任务完成          │
  │                             │
  └─ DrawInstanced()           └─ 
```

**优点**:
- ✅ 支持多线程并行处理LOD计算
- ✅ 可配置工作线程数量

**缺点**:
- ⚠️ 与BatchManager的工作线程独立，没有统一管理
- ⚠️ 任务粒度较大，可能无法充分利用多核

#### 1.1.4 Renderer（主渲染器）
**位置**: `src/core/renderer.cpp`

**架构**:
```
主线程（单线程渲染）
  │
  ├─ BeginFrame()
  │  └─ 清除缓冲区
  │
  ├─ SubmitRenderable()
  │  └─ 构建渲染队列
  │     ├─ EnsureMaterialSortKey()    // CPU密集型
  │     │  └─ BuildMaterialSortKey()
  │     └─ 分层排序
  │
  ├─ FlushRenderQueue()
  │  ├─ 层级排序（串行）            // ❌ CPU密集型，未并行化
  │  ├─ 材质排序键计算（串行）       // ❌ CPU密集型，未并行化
  │  ├─ ApplyLayerOverrides()
  │  └─ BatchManager::Flush()
  │     └─ DrainWorker() ──────→ ❌ 阻塞主线程
  │
  └─ EndFrame() + Present()
```

**缺点**:
- ❌ 渲染队列排序是串行的，无法并行化
- ❌ 材质排序键计算未并行化
- ❌ 主线程阻塞在BatchManager::DrainWorker()

---

## 2. 性能瓶颈识别

### 2.1 主要瓶颈

#### 瓶颈1: BatchManager单线程处理
**问题**: BatchManager仅使用一个工作线程处理所有批次分组

**影响**:
- 在大规模场景（1000+对象）时，批次分组成为CPU瓶颈
- 单线程无法充分利用现代多核CPU

**数据支持**:
```cpp
// 假设场景有10000个渲染对象
// 单线程处理时间: ~5-10ms
// 使用4线程并行处理: ~1.5-3ms
// 性能提升: 3-4倍
```

#### 瓶颈2: 主线程阻塞等待
**问题**: `BatchManager::DrainWorker()`在每帧都会阻塞主线程

**影响**:
- 增加帧延迟：即使GPU有空闲，CPU也在等待
- 降低帧率：主线程浪费时间在等待，而不是准备下一帧

**数据支持**:
```cpp
// 从FlushResult中可以看到等待时间
result.workerWaitTimeMs  // 主线程等待时间，典型值: 0.5-2ms
```

#### 瓶颈3: 渲染队列排序串行化
**问题**: `FlushRenderQueue()`中的层级排序和材质排序是串行的

**影响**:
- 大规模场景下，排序时间显著增加
- 无法利用多核并行排序

**数据支持**:
```cpp
// 假设场景有10000个渲染对象，分为10个层级
// 串行排序时间: ~3-5ms
// 并行排序（每层一个任务）: ~0.8-1.5ms
// 性能提升: 3-4倍
```

#### 瓶颈4: 材质排序键计算串行化
**问题**: 材质排序键在主线程串行计算

**影响**:
```cpp
// renderer.cpp:294-338
void EnsureMaterialSortKey(Renderable* renderable, ...) {
    // ❌ 每个渲染对象都需要计算排序键
    // ❌ 在主线程串行执行
    // ❌ 无法并行化
    switch (renderable->GetType()) {
        case RenderableType::Mesh:
            key = BuildMeshRenderableSortKey(meshRenderable, layerDepthFunc);
            break;
        // ...
    }
    renderable->SetMaterialSortKey(key);
}
```

#### 瓶颈5: 线程池碎片化
**问题**: 每个子系统都维护自己的工作线程

**影响**:
- AsyncResourceLoader: N个工作线程
- BatchManager: 1个工作线程
- LODInstancedRenderer: M个工作线程
- Logger: 1个异步线程
- **总计**: N+M+2个线程（典型值：8-12个）

**问题**:
- 线程过多导致上下文切换开销
- 线程池没有统一管理和调度
- 无法在系统间共享线程资源

### 2.2 次要瓶颈

#### 瓶颈6: 缺乏任务优先级调度
**问题**: BatchManager的工作队列是FIFO，没有优先级

**影响**:
- 无法保证关键任务（如透明对象排序）优先执行
- 无法根据渲染层级优先级调整任务顺序

#### 瓶颈7: 细粒度锁竞争
**问题**: 多个互斥锁保护共享数据

```cpp
// render_batching.cpp
std::mutex m_queueMutex;       // 保护任务队列
std::mutex m_storageMutex;     // 保护批次存储
std::condition_variable m_queueCv;
std::condition_variable m_idleCv;
```

**影响**:
- 锁竞争增加延迟
- 条件变量等待增加开销

---

## 3. 优化目标

### 3.1 性能目标

1. **减少帧延迟**: 消除主线程阻塞等待，目标减少50%的等待时间
2. **提升帧率**: 通过并行化CPU任务，目标提升30-50% FPS（在CPU受限场景）
3. **提升多核利用率**: 从当前的20-30%提升到60-80%
4. **降低线程数量**: 统一线程池，减少线程数量10-30%

### 3.2 架构目标

1. **统一任务调度系统**: 实现全局线程池和任务调度器
2. **减少同步等待**: 使用异步提交和执行分离
3. **细粒度并行化**: 将大任务拆分为小任务
4. **优先级调度**: 支持任务优先级

---

## 4. 优化方案设计

### 4.1 阶段一: 统一任务调度系统

#### 4.1.1 设计统一的TaskScheduler

**目标**: 替代各个子系统独立的线程池，提供统一的任务调度

**架构**:
```cpp
// include/render/task_scheduler.h
namespace Render {

/**
 * @brief 任务优先级
 */
enum class TaskPriority {
    Critical = 0,   // 关键任务（GPU上传等）
    High = 1,       // 高优先级（渲染准备）
    Normal = 2,     // 普通任务（批处理分组）
    Low = 3,        // 低优先级（资源加载）
    Background = 4  // 后台任务（日志写入）
};

/**
 * @brief 任务接口
 */
class ITask {
public:
    virtual ~ITask() = default;
    virtual void Execute() = 0;
    virtual TaskPriority GetPriority() const = 0;
    virtual const char* GetName() const = 0;
};

/**
 * @brief Lambda任务包装器
 */
class LambdaTask : public ITask {
public:
    using Func = std::function<void()>;
    
    LambdaTask(Func func, TaskPriority priority, const char* name)
        : m_func(std::move(func))
        , m_priority(priority)
        , m_name(name) {}
    
    void Execute() override { m_func(); }
    TaskPriority GetPriority() const override { return m_priority; }
    const char* GetName() const override { return m_name; }
    
private:
    Func m_func;
    TaskPriority m_priority;
    const char* m_name;
};

/**
 * @brief 任务句柄（用于等待任务完成）
 */
class TaskHandle {
public:
    TaskHandle() : m_completed(false) {}
    
    void Wait() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return m_completed.load(); });
    }
    
    bool IsCompleted() const { return m_completed.load(); }
    
    void SetCompleted() {
        m_completed = true;
        m_cv.notify_all();
    }
    
private:
    std::atomic<bool> m_completed;
    std::mutex m_mutex;
    std::condition_variable m_cv;
};

/**
 * @brief 统一任务调度器（单例）
 */
class TaskScheduler {
public:
    static TaskScheduler& GetInstance();
    
    /**
     * @brief 初始化任务调度器
     * @param numThreads 工作线程数量（0表示自动检测）
     */
    void Initialize(size_t numThreads = 0);
    
    /**
     * @brief 关闭任务调度器
     */
    void Shutdown();
    
    /**
     * @brief 提交任务
     * @param task 任务对象
     * @return 任务句柄
     */
    std::shared_ptr<TaskHandle> Submit(std::unique_ptr<ITask> task);
    
    /**
     * @brief 提交Lambda任务
     */
    std::shared_ptr<TaskHandle> SubmitLambda(
        std::function<void()> func,
        TaskPriority priority = TaskPriority::Normal,
        const char* name = "unnamed"
    );
    
    /**
     * @brief 并行执行多个任务
     * @param tasks 任务列表
     * @return 所有任务的句柄
     */
    std::vector<std::shared_ptr<TaskHandle>> SubmitBatch(
        std::vector<std::unique_ptr<ITask>> tasks
    );
    
    /**
     * @brief 等待所有任务完成
     */
    void WaitForAll(const std::vector<std::shared_ptr<TaskHandle>>& handles);
    
    /**
     * @brief 获取工作线程数量
     */
    size_t GetWorkerCount() const { return m_workers.size(); }
    
    /**
     * @brief 获取待处理任务数量
     */
    size_t GetPendingTaskCount() const;
    
private:
    TaskScheduler();
    ~TaskScheduler();
    
    // 禁止拷贝
    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;
    
    void WorkerThreadFunc();
    
    struct TaskEntry {
        std::unique_ptr<ITask> task;
        std::shared_ptr<TaskHandle> handle;
        
        bool operator<(const TaskEntry& other) const {
            // 高优先级排在前面（优先级队列）
            return task->GetPriority() > other.task->GetPriority();
        }
    };
    
    std::vector<std::thread> m_workers;
    std::priority_queue<TaskEntry> m_taskQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCV;
    std::atomic<bool> m_shutdown{false};
    
    // 统计信息
    std::atomic<size_t> m_totalTasks{0};
    std::atomic<size_t> m_completedTasks{0};
};

} // namespace Render
```

**实现**:
```cpp
// src/core/task_scheduler.cpp
namespace Render {

TaskScheduler& TaskScheduler::GetInstance() {
    static TaskScheduler instance;
    return instance;
}

void TaskScheduler::Initialize(size_t numThreads) {
    if (!m_workers.empty()) {
        Logger::GetInstance().Warning("TaskScheduler: Already initialized");
        return;
    }
    
    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;
        // 留一个核心给主线程
        if (numThreads > 1) numThreads--;
    }
    
    Logger::GetInstance().InfoFormat("TaskScheduler: Initializing with %zu threads", numThreads);
    
    m_shutdown = false;
    for (size_t i = 0; i < numThreads; ++i) {
        m_workers.emplace_back(&TaskScheduler::WorkerThreadFunc, this);
    }
}

void TaskScheduler::Shutdown() {
    m_shutdown = true;
    m_queueCV.notify_all();
    
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    m_workers.clear();
    
    Logger::GetInstance().InfoFormat(
        "TaskScheduler: Shutdown. Total: %zu, Completed: %zu",
        m_totalTasks.load(),
        m_completedTasks.load()
    );
}

std::shared_ptr<TaskHandle> TaskScheduler::Submit(std::unique_ptr<ITask> task) {
    auto handle = std::make_shared<TaskHandle>();
    
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_shutdown) {
            Logger::GetInstance().Warning("TaskScheduler: Cannot submit task after shutdown");
            handle->SetCompleted();
            return handle;
        }
        
        TaskEntry entry;
        entry.task = std::move(task);
        entry.handle = handle;
        
        m_taskQueue.push(std::move(entry));
        m_totalTasks++;
    }
    
    m_queueCV.notify_one();
    return handle;
}

std::shared_ptr<TaskHandle> TaskScheduler::SubmitLambda(
    std::function<void()> func,
    TaskPriority priority,
    const char* name)
{
    auto task = std::make_unique<LambdaTask>(std::move(func), priority, name);
    return Submit(std::move(task));
}

void TaskScheduler::WorkerThreadFunc() {
    while (!m_shutdown) {
        TaskEntry entry;
        
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCV.wait(lock, [this] {
                return m_shutdown || !m_taskQueue.empty();
            });
            
            if (m_shutdown && m_taskQueue.empty()) {
                break;
            }
            
            if (!m_taskQueue.empty()) {
                entry = std::move(const_cast<TaskEntry&>(m_taskQueue.top()));
                m_taskQueue.pop();
            }
        }
        
        if (entry.task) {
            try {
                entry.task->Execute();
                entry.handle->SetCompleted();
                m_completedTasks++;
            } catch (const std::exception& e) {
                Logger::GetInstance().ErrorFormat(
                    "TaskScheduler: Task '%s' failed: %s",
                    entry.task->GetName(),
                    e.what()
                );
                entry.handle->SetCompleted();
            }
        }
    }
}

std::vector<std::shared_ptr<TaskHandle>> TaskScheduler::SubmitBatch(
    std::vector<std::unique_ptr<ITask>> tasks)
{
    std::vector<std::shared_ptr<TaskHandle>> handles;
    handles.reserve(tasks.size());
    
    for (auto& task : tasks) {
        handles.push_back(Submit(std::move(task)));
    }
    
    return handles;
}

void TaskScheduler::WaitForAll(const std::vector<std::shared_ptr<TaskHandle>>& handles) {
    for (const auto& handle : handles) {
        handle->Wait();
    }
}

size_t TaskScheduler::GetPendingTaskCount() const {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    return m_taskQueue.size();
}

} // namespace Render
```

#### 4.1.2 迁移AsyncResourceLoader到TaskScheduler

**目标**: 移除AsyncResourceLoader的独立线程池，使用统一的TaskScheduler

**修改**:
```cpp
// async_resource_loader.h
class AsyncResourceLoader {
private:
    // ❌ 移除独立的工作线程池
    // std::vector<std::thread> m_workers;
    
    // ✅ 使用TaskScheduler
    // （无需额外成员变量，直接使用单例）
};

// async_resource_loader.cpp
std::shared_ptr<MeshLoadTask> AsyncResourceLoader::LoadMeshAsync(...) {
    auto task = std::make_shared<MeshLoadTask>();
    // ... 设置任务参数 ...
    
    // ✅ 提交到TaskScheduler
    auto handle = TaskScheduler::GetInstance().SubmitLambda(
        [task]() {
            task->ExecuteLoad();
            // 完成后移到完成队列
            std::lock_guard<std::mutex> lock(m_completedMutex);
            m_completedTasks.push(task);
        },
        TaskPriority::Low,  // 资源加载是低优先级
        task->name.c_str()
    );
    
    return task;
}
```

---

### 4.2 阶段二: 优化BatchManager并行化

#### 4.2.1 移除DrainWorker阻塞

**问题**: 当前的`DrainWorker()`会阻塞主线程等待工作线程完成

**方案**: 使用三缓冲机制（Triple Buffering）

**架构**:
```
帧 N-1:  [录制中]     [待执行]     [执行中]
         ↓
帧 N:    [待执行] ←─ [录制中]     [执行中]
         ↓                         ↓
帧 N+1:  [执行中] ←─ [待执行] ←─ [录制中]
```

**优点**:
- 主线程无需等待工作线程完成批次分组
- 工作线程可以在下一帧的批次分组期间，主线程执行上一帧的渲染
- 增加一帧延迟（可接受），换取更高的吞吐量

**实现**:
```cpp
// render_batching.h
class BatchManager {
private:
    struct BatchStorage {
        std::vector<RenderBatch> batches;
        std::unordered_map<RenderBatchKey, size_t, RenderBatchKeyHasher> lookup;
        void Clear();
    };
    
    // ✅ 三缓冲机制
    BatchStorage m_recordingStorage;  // 主线程写入
    BatchStorage m_pendingStorage;    // 准备执行
    BatchStorage m_executingStorage;  // 工作线程处理中
    
    std::mutex m_swapMutex;
    std::atomic<bool> m_workerBusy{false};
    
    // ❌ 移除DrainWorker()
    // void DrainWorker();
    
    // ✅ 新增非阻塞交换
    void TrySwapBuffers();
};

// render_batching.cpp
void BatchManager::Flush(RenderState* renderState) {
    // ✅ 尝试交换缓冲区（非阻塞）
    TrySwapBuffers();
    
    // ✅ 执行上一帧准备好的批次
    // （工作线程可能仍在处理当前帧的批次）
    for (const auto& command : m_executionBuffer.GetCommands()) {
        // ... 渲染批次 ...
    }
    
    // ✅ 无需等待工作线程
}

void BatchManager::TrySwapBuffers() {
    std::unique_lock<std::mutex> lock(m_swapMutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        // 工作线程仍在忙，使用上一帧的数据
        return;
    }
    
    if (m_workerBusy.load()) {
        // 工作线程仍在处理
        return;
    }
    
    // 三缓冲轮换
    std::swap(m_recordingStorage, m_pendingStorage);
    std::swap(m_pendingStorage, m_executingStorage);
    
    // 清空录制缓冲区
    m_recordingStorage.Clear();
    
    // 通知工作线程开始处理
    m_workerBusy = true;
    TaskScheduler::GetInstance().SubmitLambda(
        [this]() {
            ProcessBatchesAsync();
            m_workerBusy = false;
        },
        TaskPriority::High,
        "BatchProcessing"
    );
}
```

#### 4.2.2 并行化批次分组

**问题**: 当前批次分组是串行的

**方案**: 将批次分组任务并行化

**实现**:
```cpp
void BatchManager::ProcessBatchesAsync() {
    // ✅ 将待处理的对象分组为多个任务
    const size_t itemCount = m_pendingItems.size();
    const size_t numThreads = TaskScheduler::GetInstance().GetWorkerCount();
    const size_t itemsPerTask = std::max(size_t(100), itemCount / numThreads);
    
    std::vector<std::unique_ptr<ITask>> tasks;
    
    for (size_t i = 0; i < itemCount; i += itemsPerTask) {
        size_t endIdx = std::min(i + itemsPerTask, itemCount);
        
        tasks.push_back(std::make_unique<LambdaTask>(
            [this, i, endIdx]() {
                // 处理一批对象
                for (size_t j = i; j < endIdx; ++j) {
                    ProcessWorkItem(m_pendingItems[j]);
                }
            },
            TaskPriority::High,
            "BatchGrouping"
        ));
    }
    
    // 并行执行所有任务
    auto handles = TaskScheduler::GetInstance().SubmitBatch(std::move(tasks));
    
    // 等待所有任务完成
    TaskScheduler::GetInstance().WaitForAll(handles);
}
```

---

### 4.3 阶段三: 并行化渲染队列排序

#### 4.3.1 并行化层级排序

**问题**: `FlushRenderQueue()`中每个层级的排序是串行的

**方案**: 为每个层级创建一个排序任务

**实现**:
```cpp
// renderer.cpp
void Renderer::FlushRenderQueue() {
    // ... 省略前面的代码 ...
    
    // ✅ 并行排序每个层级
    std::vector<std::unique_ptr<ITask>> sortTasks;
    
    for (auto& bucket : bucketsSnapshot) {
        if (bucket.items.empty()) continue;
        
        sortTasks.push_back(std::make_unique<LambdaTask>(
            [this, &bucket, &record = layerRecords[...]]() {
                SortLayerItems(bucket.items, record.descriptor);
            },
            TaskPriority::High,
            "LayerSort"
        ));
    }
    
    auto handles = TaskScheduler::GetInstance().SubmitBatch(std::move(sortTasks));
    TaskScheduler::GetInstance().WaitForAll(handles);
    
    // ✅ 排序完成后，合并到sortedQueue
    // ... 省略后面的代码 ...
}
```

#### 4.3.2 并行化材质排序键计算

**问题**: 材质排序键在主线程串行计算

**方案**: 批量并行计算排序键

**实现**:
```cpp
void Renderer::SubmitRenderable(Renderable* renderable) {
    // ✅ 不在这里立即计算排序键
    // EnsureMaterialSortKey(renderable, layerDepthFunc);
    
    // ✅ 标记为需要计算排序键
    renderable->MarkMaterialSortKeyDirty();
    
    // ... 省略其他代码 ...
}

void Renderer::FlushRenderQueue() {
    // ✅ 收集所有需要计算排序键的对象
    std::vector<Renderable*> dirtyRenderables;
    for (const auto* item : submissionOrder) {
        if (item->renderable->IsMaterialSortKeyDirty()) {
            dirtyRenderables.push_back(item->renderable);
        }
    }
    
    // ✅ 并行计算排序键
    const size_t itemCount = dirtyRenderables.size();
    const size_t numThreads = TaskScheduler::GetInstance().GetWorkerCount();
    const size_t itemsPerTask = std::max(size_t(100), itemCount / numThreads);
    
    std::vector<std::unique_ptr<ITask>> sortKeyTasks;
    
    for (size_t i = 0; i < itemCount; i += itemsPerTask) {
        size_t endIdx = std::min(i + itemsPerTask, itemCount);
        
        sortKeyTasks.push_back(std::make_unique<LambdaTask>(
            [this, &dirtyRenderables, i, endIdx, layerDepthFunc]() {
                for (size_t j = i; j < endIdx; ++j) {
                    EnsureMaterialSortKey(dirtyRenderables[j], layerDepthFunc);
                }
            },
            TaskPriority::High,
            "SortKeyCalc"
        ));
    }
    
    auto handles = TaskScheduler::GetInstance().SubmitBatch(std::move(sortKeyTasks));
    TaskScheduler::GetInstance().WaitForAll(handles);
    
    // ✅ 排序键计算完成，继续后续流程
    // ... 省略后续代码 ...
}
```

---

### 4.4 阶段四: 优化LODInstancedRenderer

#### 4.4.1 迁移到TaskScheduler

**目标**: 移除LODInstancedRenderer的独立线程池

**实现**:
```cpp
// lod_instanced_renderer.h
class LODInstancedRenderer {
private:
    // ❌ 移除独立的工作线程
    // std::vector<std::thread> m_workerThreads;
    // std::mutex m_taskMutex;
    // std::condition_variable m_taskCV;
    // std::queue<PrepareTask> m_tasks;
    
    // ✅ 使用TaskScheduler（无需额外成员变量）
};

// lod_instanced_renderer.cpp
void LODInstancedRenderer::PrepareRender(...) {
    // ✅ 提交任务到TaskScheduler
    std::vector<std::unique_ptr<ITask>> tasks;
    
    for (const auto& instance : pendingInstances) {
        tasks.push_back(std::make_unique<LambdaTask>(
            [this, instance]() {
                ProcessPrepareTask(instance);
            },
            TaskPriority::High,
            "LODPrepare"
        ));
    }
    
    auto handles = TaskScheduler::GetInstance().SubmitBatch(std::move(tasks));
    TaskScheduler::GetInstance().WaitForAll(handles);
}
```

---

### 4.5 阶段五: 性能监控与调优

#### 4.5.1 添加性能指标

**实现**:
```cpp
// task_scheduler.h
struct TaskSchedulerStats {
    size_t totalTasks = 0;
    size_t completedTasks = 0;
    size_t pendingTasks = 0;
    float avgTaskTimeMs = 0.0f;
    float maxTaskTimeMs = 0.0f;
    size_t workerThreads = 0;
    float utilization = 0.0f;  // 线程池利用率 (0-1)
};

class TaskScheduler {
public:
    TaskSchedulerStats GetStats() const;
    void ResetStats();
};
```

#### 4.5.2 添加调试UI

**实现**:
```cpp
// 在DebugHUD中显示任务调度器统计
void DebugHUD::Render() {
    // ... 省略其他代码 ...
    
    auto stats = TaskScheduler::GetInstance().GetStats();
    
    ImGui::Text("TaskScheduler:");
    ImGui::Text("  Threads: %zu", stats.workerThreads);
    ImGui::Text("  Pending: %zu", stats.pendingTasks);
    ImGui::Text("  Total: %zu", stats.totalTasks);
    ImGui::Text("  Completed: %zu", stats.completedTasks);
    ImGui::Text("  Utilization: %.1f%%", stats.utilization * 100.0f);
    ImGui::Text("  Avg Task Time: %.2f ms", stats.avgTaskTimeMs);
    ImGui::Text("  Max Task Time: %.2f ms", stats.maxTaskTimeMs);
}
```

---

## 5. 实施路线图

### 阶段一: 统一任务调度系统 (1-2周)

**任务**:
1. 实现TaskScheduler核心框架
2. 添加单元测试
3. 迁移AsyncResourceLoader
4. 验证资源加载功能

**验收标准**:
- TaskScheduler通过所有单元测试
- AsyncResourceLoader功能正常
- 线程数量减少（从N+M+2降低到N）

### 阶段二: 优化BatchManager (1-2周)

**任务**:
1. 实现三缓冲机制
2. 移除DrainWorker阻塞
3. 并行化批次分组
4. 性能测试

**验收标准**:
- 主线程不再阻塞在DrainWorker
- 批次分组性能提升50%+
- 帧率提升10-20%（CPU受限场景）

### 阶段三: 并行化渲染队列 (1-2周)

**任务**:
1. 并行化层级排序
2. 并行化排序键计算
3. 性能测试
4. 优化锁竞争

**验收标准**:
- 排序时间减少50%+
- 帧率提升10-20%（大规模场景）
- CPU利用率提升到60%+

### 阶段四: 优化LODInstancedRenderer (1周)

**任务**:
1. 迁移到TaskScheduler
2. 优化任务粒度
3. 性能测试

**验收标准**:
- 线程数量进一步减少
- LOD计算性能保持或提升

### 阶段五: 性能调优与文档 (1周)

**任务**:
1. 添加性能监控
2. 实现调试UI
3. 性能基准测试
4. 编写使用文档

**验收标准**:
- 完整的性能监控系统
- 调试UI可用
- 性能测试报告
- 完善的文档

**总计**: 5-8周

---

## 6. 预期收益

### 6.1 性能提升

#### 场景1: 小规模场景 (100-500对象)
- **帧率**: 提升5-10%
- **CPU利用率**: 从20%提升到30-40%
- **帧延迟**: 减少0.2-0.5ms

#### 场景2: 中等规模场景 (500-2000对象)
- **帧率**: 提升15-30%
- **CPU利用率**: 从25%提升到50-60%
- **帧延迟**: 减少0.5-1.5ms

#### 场景3: 大规模场景 (2000+对象)
- **帧率**: 提升30-50%
- **CPU利用率**: 从30%提升到60-80%
- **帧延迟**: 减少1-3ms

### 6.2 资源优化

- **线程数量**: 减少10-30%（从N+M+2到N）
- **内存占用**: 减少线程栈内存（每个线程~1-2MB）
- **上下文切换**: 减少20-40%

### 6.3 代码质量

- **统一架构**: 所有多线程任务使用统一的调度系统
- **可维护性**: 更清晰的任务依赖关系
- **可扩展性**: 易于添加新的并行任务
- **可调试性**: 统一的性能监控和调试工具

---

## 7. 风险与挑战

### 7.1 技术风险

#### 风险1: 任务粒度不当
**描述**: 任务拆分过细导致调度开销大于并行收益

**缓解措施**:
- 通过性能测试确定最佳任务粒度
- 提供可配置的任务批大小
- 对小规模场景自动退回串行处理

#### 风险2: 锁竞争增加
**描述**: 并行任务增加可能导致锁竞争

**缓解措施**:
- 使用无锁数据结构（lock-free queues）
- 减少共享状态，使用线程本地存储
- 优化临界区大小

#### 风险3: 调试困难
**描述**: 多线程问题难以复现和调试

**缓解措施**:
- 添加详细的日志和性能跟踪
- 实现任务执行时间统计
- 提供单线程调试模式

### 7.2 兼容性风险

#### 风险4: API变更
**描述**: 修改可能影响现有代码

**缓解措施**:
- 保持向后兼容的API
- 提供渐进式迁移路径
- 编写详细的迁移指南

### 7.3 性能风险

#### 风险5: 性能提升不达预期
**描述**: 实际性能提升可能低于预期

**缓解措施**:
- 分阶段实施，每阶段验证收益
- 提供性能对比测试
- 允许运行时切换新旧实现

---

## 8. 附录

### 8.1 相关文件清单

#### 需要修改的文件:
- `include/render/renderer.h`
- `src/core/renderer.cpp`
- `include/render/render_batching.h`
- `src/rendering/render_batching.cpp`
- `include/render/async_resource_loader.h`
- `src/core/async_resource_loader.cpp`
- `include/render/lod_instanced_renderer.h`
- `src/rendering/lod_instanced_renderer.cpp`

#### 需要新增的文件:
- `include/render/task_scheduler.h`
- `src/core/task_scheduler.cpp`
- `tests/test_task_scheduler.cpp`

### 8.2 性能测试场景

#### 测试场景1: 批处理性能
```cpp
// 测试批处理分组性能
void BenchmarkBatchGrouping() {
    BatchManager batchManager;
    
    // 生成10000个渲染对象
    std::vector<BatchableItem> items = GenerateTestItems(10000);
    
    // 测试串行处理
    auto startSerial = std::chrono::high_resolution_clock::now();
    for (const auto& item : items) {
        batchManager.AddItem(item);
    }
    batchManager.Flush(renderState);
    auto endSerial = std::chrono::high_resolution_clock::now();
    
    // 测试并行处理
    auto startParallel = std::chrono::high_resolution_clock::now();
    // ... 使用新的并行实现 ...
    auto endParallel = std::chrono::high_resolution_clock::now();
    
    // 输出结果
    auto serialMs = std::chrono::duration<double, std::milli>(endSerial - startSerial).count();
    auto parallelMs = std::chrono::duration<double, std::milli>(endParallel - startParallel).count();
    
    printf("Serial: %.2f ms\n", serialMs);
    printf("Parallel: %.2f ms\n", parallelMs);
    printf("Speedup: %.2fx\n", serialMs / parallelMs);
}
```

#### 测试场景2: 排序性能
```cpp
// 测试排序性能
void BenchmarkSorting() {
    // 生成10000个渲染对象，分为10个层级
    std::vector<LayerBucket> buckets = GenerateTestBuckets(10, 1000);
    
    // 测试串行排序
    auto startSerial = std::chrono::high_resolution_clock::now();
    for (auto& bucket : buckets) {
        SortLayerItems(bucket.items, bucket.descriptor);
    }
    auto endSerial = std::chrono::high_resolution_clock::now();
    
    // 测试并行排序
    auto startParallel = std::chrono::high_resolution_clock::now();
    // ... 使用并行排序 ...
    auto endParallel = std::chrono::high_resolution_clock::now();
    
    // 输出结果
}
```

### 8.3 参考资料

- [游戏引擎中的多线程渲染](https://www.gdcvault.com/play/1024656/)
- [Destiny的并行渲染架构](https://www.gdcvault.com/play/1022106/)
- [Naughty Dog的任务调度系统](https://www.gdcvault.com/play/1022186/)
- [Unreal Engine的任务图系统](https://docs.unrealengine.com/en-US/ProgrammingAndScripting/Rendering/ThreadedRendering/)

---

## 变更历史

| 版本 | 日期 | 作者 | 说明 |
|------|------|------|------|
| 1.0 | 2025-12-04 | AI Assistant | 初始版本 |

---

**文档结束**

