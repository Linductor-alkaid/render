#pragma once

#include <functional>
#include <memory>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

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
    
    /**
     * @brief 等待任务完成
     */
    void Wait();
    
    /**
     * @brief 等待任务完成（带超时）
     * @param timeoutMs 超时时间（毫秒）
     * @return true表示任务完成，false表示超时
     */
    bool WaitFor(uint32_t timeoutMs);
    
    /**
     * @brief 检查任务是否完成
     */
    bool IsCompleted() const { return m_completed.load(std::memory_order_acquire); }
    
    /**
     * @brief 设置任务完成（由TaskScheduler内部调用）
     */
    void SetCompleted();
    
private:
    std::atomic<bool> m_completed;
    std::mutex m_mutex;
    std::condition_variable m_cv;
};

/**
 * @brief 任务调度器统计信息
 */
struct TaskSchedulerStats {
    size_t totalTasks = 0;          // 总任务数
    size_t completedTasks = 0;      // 已完成任务数
    size_t pendingTasks = 0;        // 待处理任务数
    size_t failedTasks = 0;         // 失败任务数
    float avgTaskTimeMs = 0.0f;     // 平均任务执行时间（毫秒）
    float maxTaskTimeMs = 0.0f;     // 最大任务执行时间（毫秒）
    size_t workerThreads = 0;       // 工作线程数
    float utilization = 0.0f;       // 线程池利用率 (0-1)
    
    void Reset() {
        totalTasks = 0;
        completedTasks = 0;
        pendingTasks = 0;
        failedTasks = 0;
        avgTaskTimeMs = 0.0f;
        maxTaskTimeMs = 0.0f;
        utilization = 0.0f;
    }
};

/**
 * @brief 统一任务调度器（单例）
 * 
 * 提供统一的任务调度和线程池管理
 * 
 * 线程安全：
 * - ✅ 所有公共方法都是线程安全的
 * - ✅ 使用优先级队列管理任务
 * - ✅ 支持任务等待和同步
 * 
 * 使用示例：
 * ```cpp
 * // 初始化
 * TaskScheduler::GetInstance().Initialize(4); // 4个工作线程
 * 
 * // 提交Lambda任务
 * auto handle = TaskScheduler::GetInstance().SubmitLambda(
 *     []() { 
 *         // 执行任务
 *     },
 *     TaskPriority::High,
 *     "MyTask"
 * );
 * 
 * // 等待任务完成
 * handle->Wait();
 * 
 * // 批量提交任务
 * std::vector<std::unique_ptr<ITask>> tasks;
 * // ... 添加任务 ...
 * auto handles = TaskScheduler::GetInstance().SubmitBatch(std::move(tasks));
 * TaskScheduler::GetInstance().WaitForAll(handles);
 * 
 * // 关闭
 * TaskScheduler::GetInstance().Shutdown();
 * ```
 */
class TaskScheduler {
public:
    /**
     * @brief 获取单例实例
     */
    static TaskScheduler& GetInstance();
    
    /**
     * @brief 初始化任务调度器
     * @param numThreads 工作线程数量（0表示自动检测为CPU核心数-1）
     */
    void Initialize(size_t numThreads = 0);
    
    /**
     * @brief 关闭任务调度器（等待所有任务完成）
     */
    void Shutdown();
    
    /**
     * @brief 检查是否已初始化
     */
    bool IsInitialized() const { return !m_workers.empty(); }
    
    /**
     * @brief 提交任务
     * @param task 任务对象
     * @return 任务句柄
     */
    std::shared_ptr<TaskHandle> Submit(std::unique_ptr<ITask> task);
    
    /**
     * @brief 提交Lambda任务
     * @param func 任务函数
     * @param priority 任务优先级
     * @param name 任务名称（用于调试）
     * @return 任务句柄
     */
    std::shared_ptr<TaskHandle> SubmitLambda(
        std::function<void()> func,
        TaskPriority priority = TaskPriority::Normal,
        const char* name = "unnamed"
    );
    
    /**
     * @brief 批量提交任务
     * @param tasks 任务列表
     * @return 所有任务的句柄列表
     */
    std::vector<std::shared_ptr<TaskHandle>> SubmitBatch(
        std::vector<std::unique_ptr<ITask>> tasks
    );
    
    /**
     * @brief 等待所有任务完成
     * @param handles 任务句柄列表
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
    
    /**
     * @brief 获取统计信息
     */
    TaskSchedulerStats GetStats() const;
    
    /**
     * @brief 重置统计信息
     */
    void ResetStats();
    
private:
    TaskScheduler();
    ~TaskScheduler();
    
    // 禁止拷贝和移动
    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;
    TaskScheduler(TaskScheduler&&) = delete;
    TaskScheduler& operator=(TaskScheduler&&) = delete;
    
    void WorkerThreadFunc();
    
    struct TaskEntry {
        std::unique_ptr<ITask> task;
        std::shared_ptr<TaskHandle> handle;
        std::chrono::steady_clock::time_point submitTime;
        
        bool operator<(const TaskEntry& other) const {
            // 优先级队列：高优先级排在前面
            if (task->GetPriority() != other.task->GetPriority()) {
                return task->GetPriority() > other.task->GetPriority();
            }
            // 相同优先级，先提交的先执行
            return submitTime > other.submitTime;
        }
    };
    
    std::vector<std::thread> m_workers;
    std::priority_queue<TaskEntry> m_taskQueue;
    mutable std::mutex m_queueMutex;
    std::condition_variable m_queueCV;
    std::atomic<bool> m_shutdown{false};
    
    // 统计信息
    std::atomic<size_t> m_totalTasks{0};
    std::atomic<size_t> m_completedTasks{0};
    std::atomic<size_t> m_failedTasks{0};
    
    mutable std::mutex m_statsMutex;
    float m_totalTaskTimeMs = 0.0f;
    float m_maxTaskTimeMs = 0.0f;
    std::chrono::steady_clock::time_point m_statsStartTime;
    std::chrono::steady_clock::time_point m_lastUtilizationUpdate;
    size_t m_tasksExecutedSinceUtilUpdate = 0;
};

} // namespace Render

