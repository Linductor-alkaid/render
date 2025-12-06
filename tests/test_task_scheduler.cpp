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
#include <atomic>
#include <chrono>
#include <thread>
#include <cassert>
#include <iostream>

using namespace Render;

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "FAILED: " << message << std::endl; \
            std::cerr << "  File: " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

#define RUN_TEST(test_func) \
    do { \
        std::cout << "Running: " << #test_func << "..." << std::endl; \
        TaskScheduler::GetInstance().Initialize(4); \
        bool result = test_func(); \
        TaskScheduler::GetInstance().Shutdown(); \
        if (result) { \
            std::cout << "PASSED: " << #test_func << std::endl; \
        } else { \
            std::cout << "FAILED: " << #test_func << std::endl; \
            return 1; \
        } \
    } while(0)

// 测试1: 基本初始化和关闭
bool Test_InitializeAndShutdown() {
    TEST_ASSERT(TaskScheduler::GetInstance().IsInitialized(), "TaskScheduler should be initialized");
    TEST_ASSERT(TaskScheduler::GetInstance().GetWorkerCount() == 4, "Worker count should be 4");
    return true;
}

// 测试2: 提交单个任务
bool Test_SubmitSingleTask() {
    std::atomic<bool> executed{false};
    
    auto handle = TaskScheduler::GetInstance().SubmitLambda(
        [&executed]() {
            executed = true;
        },
        TaskPriority::Normal,
        "TestTask"
    );
    
    TEST_ASSERT(handle != nullptr, "Handle should not be null");
    handle->Wait();
    
    TEST_ASSERT(executed.load(), "Task should have executed");
    TEST_ASSERT(handle->IsCompleted(), "Handle should be completed");
    return true;
}

// 测试3: 提交多个任务
bool Test_SubmitMultipleTasks() {
    const int taskCount = 100;
    std::atomic<int> counter{0};
    
    std::vector<std::shared_ptr<TaskHandle>> handles;
    
    for (int i = 0; i < taskCount; ++i) {
        auto handle = TaskScheduler::GetInstance().SubmitLambda(
            [&counter]() {
                counter.fetch_add(1, std::memory_order_relaxed);
            },
            TaskPriority::Normal,
            "CounterTask"
        );
        handles.push_back(handle);
    }
    
    // 等待所有任务完成
    TaskScheduler::GetInstance().WaitForAll(handles);
    
    TEST_ASSERT(counter.load() == taskCount, "All tasks should have executed");
    
    // 检查所有句柄都已完成
    for (const auto& handle : handles) {
        TEST_ASSERT(handle->IsCompleted(), "All handles should be completed");
    }
    return true;
}

// 测试4: 任务优先级
bool Test_TaskPriority() {
    std::vector<int> order;
    std::mutex orderMutex;
    
    // 提交低优先级任务
    auto lowHandle = TaskScheduler::GetInstance().SubmitLambda(
        [&]() {
            std::lock_guard<std::mutex> lock(orderMutex);
            order.push_back(0);
        },
        TaskPriority::Low,
        "LowPriorityTask"
    );
    
    // 提交高优先级任务
    auto highHandle = TaskScheduler::GetInstance().SubmitLambda(
        [&]() {
            std::lock_guard<std::mutex> lock(orderMutex);
            order.push_back(1);
        },
        TaskPriority::High,
        "HighPriorityTask"
    );
    
    // 提交关键任务
    auto criticalHandle = TaskScheduler::GetInstance().SubmitLambda(
        [&]() {
            std::lock_guard<std::mutex> lock(orderMutex);
            order.push_back(2);
        },
        TaskPriority::Critical,
        "CriticalTask"
    );
    
    std::vector<std::shared_ptr<TaskHandle>> handles = {lowHandle, highHandle, criticalHandle};
    TaskScheduler::GetInstance().WaitForAll(handles);
    
    TEST_ASSERT(order.size() == 3, "All priority tasks should execute");
    return true;
}

// 测试5: 任务超时等待
bool Test_TaskWaitTimeout() {
    auto handle = TaskScheduler::GetInstance().SubmitLambda(
        []() {
            // 模拟长时间任务
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        },
        TaskPriority::Normal,
        "LongTask"
    );
    
    // 短超时应该失败
    TEST_ASSERT(!handle->WaitFor(100), "Short timeout should fail");
    
    // 长超时应该成功
    TEST_ASSERT(handle->WaitFor(1000), "Long timeout should succeed");
    TEST_ASSERT(handle->IsCompleted(), "Handle should be completed");
    return true;
}

// 测试6: 统计信息
bool Test_Statistics() {
    const int taskCount = 20;
    
    TaskScheduler::GetInstance().ResetStats();
    
    std::vector<std::shared_ptr<TaskHandle>> handles;
    for (int i = 0; i < taskCount; ++i) {
        auto handle = TaskScheduler::GetInstance().SubmitLambda(
            []() {
                // 模拟一些工作
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            },
            TaskPriority::Normal,
            "StatsTask"
        );
        handles.push_back(handle);
    }
    
    TaskScheduler::GetInstance().WaitForAll(handles);
    
    auto stats = TaskScheduler::GetInstance().GetStats();
    
    TEST_ASSERT(stats.totalTasks == taskCount, "Total tasks should match");
    TEST_ASSERT(stats.completedTasks == taskCount, "All tasks should be completed");
    TEST_ASSERT(stats.pendingTasks == 0, "No pending tasks");
    TEST_ASSERT(stats.avgTaskTimeMs > 0.0f, "Average task time should be positive");
    TEST_ASSERT(stats.maxTaskTimeMs > 0.0f, "Max task time should be positive");
    TEST_ASSERT(stats.workerThreads == 4, "Worker count should be 4");
    return true;
}

// 主函数
int main(int argc, char** argv) {
    // 初始化日志系统
    Logger::GetInstance().SetLogLevel(LogLevel::Info);
    
    std::cout << "========================================" << std::endl;
    std::cout << "TaskScheduler Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    
    RUN_TEST(Test_InitializeAndShutdown);
    RUN_TEST(Test_SubmitSingleTask);
    RUN_TEST(Test_SubmitMultipleTasks);
    RUN_TEST(Test_TaskPriority);
    RUN_TEST(Test_TaskWaitTimeout);
    RUN_TEST(Test_Statistics);
    
    std::cout << "========================================" << std::endl;
    std::cout << "All tests passed!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}

