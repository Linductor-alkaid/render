/*
 * Copyright (c) 2025 Li Chaoyu
 * 
 * This file is part of Render.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * For commercial licensing, please contact: 2052046346@qq.com
 */
#include "render/task_scheduler.h"
#include "render/logger.h"
#include <algorithm>

namespace Render {

// ========================================================================
// TaskHandle 实现
// ========================================================================

void TaskHandle::Wait() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this] { return m_completed.load(std::memory_order_acquire); });
}

bool TaskHandle::WaitFor(uint32_t timeoutMs) {
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_cv.wait_for(
        lock,
        std::chrono::milliseconds(timeoutMs),
        [this] { return m_completed.load(std::memory_order_acquire); }
    );
}

void TaskHandle::SetCompleted() {
    m_completed.store(true, std::memory_order_release);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cv.notify_all();
}

// ========================================================================
// TaskScheduler 实现
// ========================================================================

TaskScheduler& TaskScheduler::GetInstance() {
    static TaskScheduler instance;
    return instance;
}

TaskScheduler::TaskScheduler()
    : m_shutdown(false)
    , m_totalTasks(0)
    , m_completedTasks(0)
    , m_failedTasks(0)
    , m_totalTaskTimeMs(0.0f)
    , m_maxTaskTimeMs(0.0f)
    , m_tasksExecutedSinceUtilUpdate(0) {
    m_statsStartTime = std::chrono::steady_clock::now();
    m_lastUtilizationUpdate = m_statsStartTime;
}

TaskScheduler::~TaskScheduler() {
    Shutdown();
}

void TaskScheduler::Initialize(size_t numThreads) {
    if (!m_workers.empty()) {
        Logger::GetInstance().Warning("TaskScheduler: Already initialized");
        return;
    }
    
    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) {
            numThreads = 4; // 回退值
        }
        // 留一个核心给主线程
        if (numThreads > 1) {
            numThreads--;
        }
    }
    
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("初始化 TaskScheduler");
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().InfoFormat("工作线程数: %zu", numThreads);
    
    m_shutdown = false;
    m_statsStartTime = std::chrono::steady_clock::now();
    m_lastUtilizationUpdate = m_statsStartTime;
    
    for (size_t i = 0; i < numThreads; ++i) {
        m_workers.emplace_back(&TaskScheduler::WorkerThreadFunc, this);
        Logger::GetInstance().DebugFormat("创建工作线程 %zu", i);
    }
    
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("TaskScheduler 初始化完成");
    Logger::GetInstance().Info("========================================");
}

void TaskScheduler::Shutdown() {
    if (m_workers.empty()) {
        return;
    }
    
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("关闭 TaskScheduler");
    Logger::GetInstance().Info("========================================");
    
    // 设置关闭标志
    m_shutdown = true;
    
    // 唤醒所有工作线程
    m_queueCV.notify_all();
    
    // 等待所有线程退出
    Logger::GetInstance().Info("等待工作线程退出...");
    for (size_t i = 0; i < m_workers.size(); ++i) {
        if (m_workers[i].joinable()) {
            m_workers[i].join();
            Logger::GetInstance().DebugFormat("工作线程 %zu 已退出", i);
        }
    }
    
    m_workers.clear();
    
    // 打印统计信息
    Logger::GetInstance().InfoFormat(
        "TaskScheduler 统计: 总任务=%zu, 完成=%zu, 失败=%zu",
        m_totalTasks.load(),
        m_completedTasks.load(),
        m_failedTasks.load()
    );
    
    // 清空剩余任务
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        while (!m_taskQueue.empty()) {
            m_taskQueue.pop();
        }
    }
    
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("TaskScheduler 已关闭");
    Logger::GetInstance().Info("========================================");
}

std::shared_ptr<TaskHandle> TaskScheduler::Submit(std::unique_ptr<ITask> task) {
    if (!task) {
        Logger::GetInstance().Warning("TaskScheduler: Cannot submit null task");
        auto handle = std::make_shared<TaskHandle>();
        handle->SetCompleted();
        return handle;
    }
    
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
        entry.submitTime = std::chrono::steady_clock::now();
        
        m_taskQueue.push(std::move(entry));
        m_totalTasks.fetch_add(1, std::memory_order_relaxed);
    }
    
    m_queueCV.notify_one();
    return handle;
}

std::shared_ptr<TaskHandle> TaskScheduler::SubmitLambda(
    std::function<void()> func,
    TaskPriority priority,
    const char* name)
{
    if (!func) {
        Logger::GetInstance().Warning("TaskScheduler: Cannot submit null lambda");
        auto handle = std::make_shared<TaskHandle>();
        handle->SetCompleted();
        return handle;
    }
    
    auto task = std::make_unique<LambdaTask>(std::move(func), priority, name);
    return Submit(std::move(task));
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
        if (handle) {
            handle->Wait();
        }
    }
}

void TaskScheduler::WorkerThreadFunc() {
    auto threadId = std::this_thread::get_id();
    size_t threadIdHash = std::hash<std::thread::id>{}(threadId);
    
    Logger::GetInstance().DebugFormat("工作线程启动: %zu", threadIdHash);
    
    while (!m_shutdown) {
        TaskEntry entry;
        
        // 从队列获取任务
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            
            // 等待任务或关闭信号
            m_queueCV.wait(lock, [this] {
                return m_shutdown || !m_taskQueue.empty();
            });
            
            if (m_shutdown && m_taskQueue.empty()) {
                break; // 退出线程
            }
            
            if (!m_taskQueue.empty()) {
                entry = std::move(const_cast<TaskEntry&>(m_taskQueue.top()));
                m_taskQueue.pop();
            }
        }
        
        if (!entry.task) {
            continue;
        }
        
        // 执行任务（锁外执行，避免阻塞）
        auto startTime = std::chrono::steady_clock::now();
        
        try {
            Logger::GetInstance().DebugFormat(
                "[Thread:%zu] 执行任务: %s (优先级:%d)",
                threadIdHash,
                entry.task->GetName(),
                static_cast<int>(entry.task->GetPriority())
            );
            
            entry.task->Execute();
            
            auto endTime = std::chrono::steady_clock::now();
            auto durationMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
            
            // 更新统计信息
            {
                std::lock_guard<std::mutex> lock(m_statsMutex);
                m_totalTaskTimeMs += durationMs;
                m_maxTaskTimeMs = std::max(m_maxTaskTimeMs, durationMs);
                m_tasksExecutedSinceUtilUpdate++;
            }
            
            entry.handle->SetCompleted();
            m_completedTasks.fetch_add(1, std::memory_order_relaxed);
            
            Logger::GetInstance().DebugFormat(
                "[Thread:%zu] 任务完成: %s (耗时: %.2f ms)",
                threadIdHash,
                entry.task->GetName(),
                durationMs
            );
            
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat(
                "TaskScheduler: 任务 '%s' 执行失败: %s",
                entry.task->GetName(),
                e.what()
            );
            entry.handle->SetCompleted();
            m_failedTasks.fetch_add(1, std::memory_order_relaxed);
            
        } catch (...) {
            Logger::GetInstance().ErrorFormat(
                "TaskScheduler: 任务 '%s' 执行失败: 未知异常",
                entry.task->GetName()
            );
            entry.handle->SetCompleted();
            m_failedTasks.fetch_add(1, std::memory_order_relaxed);
        }
    }
    
    Logger::GetInstance().DebugFormat("工作线程退出: %zu", threadIdHash);
}

size_t TaskScheduler::GetPendingTaskCount() const {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    return m_taskQueue.size();
}

TaskSchedulerStats TaskScheduler::GetStats() const {
    TaskSchedulerStats stats;
    
    stats.totalTasks = m_totalTasks.load(std::memory_order_relaxed);
    stats.completedTasks = m_completedTasks.load(std::memory_order_relaxed);
    stats.failedTasks = m_failedTasks.load(std::memory_order_relaxed);
    stats.pendingTasks = GetPendingTaskCount();
    stats.workerThreads = m_workers.size();
    
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        
        if (stats.completedTasks > 0) {
            stats.avgTaskTimeMs = m_totalTaskTimeMs / static_cast<float>(stats.completedTasks);
        }
        stats.maxTaskTimeMs = m_maxTaskTimeMs;
        
        // 计算线程池利用率
        auto now = std::chrono::steady_clock::now();
        auto elapsedMs = std::chrono::duration<float, std::milli>(now - m_lastUtilizationUpdate).count();
        
        if (elapsedMs > 0.0f && stats.workerThreads > 0) {
            // 利用率 = 任务总执行时间 / (线程数 * 时间窗口)
            float totalWorkTimeMs = m_totalTaskTimeMs;
            float maxPossibleTimeMs = static_cast<float>(stats.workerThreads) * elapsedMs;
            stats.utilization = std::min(1.0f, totalWorkTimeMs / maxPossibleTimeMs);
        }
    }
    
    return stats;
}

void TaskScheduler::ResetStats() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    m_totalTasks.store(0, std::memory_order_relaxed);
    m_completedTasks.store(0, std::memory_order_relaxed);
    m_failedTasks.store(0, std::memory_order_relaxed);
    m_totalTaskTimeMs = 0.0f;
    m_maxTaskTimeMs = 0.0f;
    m_tasksExecutedSinceUtilUpdate = 0;
    m_statsStartTime = std::chrono::steady_clock::now();
    m_lastUtilizationUpdate = m_statsStartTime;
}

} // namespace Render

