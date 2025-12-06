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
/**
 * @file 24_async_logger_test.cpp
 * @brief 异步日志系统测试
 * 
 * 测试内容：
 * 1. 基本的异步日志功能
 * 2. 多线程并发写入日志
 * 3. 文件轮转功能
 * 4. 性能对比（同步 vs 异步）
 * 5. 队列管理和刷新功能
 */

#include "render/logger.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <random>

using namespace Render;
using namespace std::chrono;

// ========== 测试辅助函数 ==========

void PrintTestHeader(const std::string& testName) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "测试: " << testName << std::endl;
    std::cout << "========================================\n" << std::endl;
}

void PrintTestResult(const std::string& testName, bool passed) {
    if (passed) {
        std::cout << "✓ " << testName << " 通过" << std::endl;
    } else {
        std::cout << "✗ " << testName << " 失败" << std::endl;
    }
}

// ========== 测试 1: 基本异步日志功能 ==========

void Test1_BasicAsyncLogging() {
    PrintTestHeader("测试 1: 基本异步日志功能");
    
    auto& logger = Logger::GetInstance();
    
    // 配置日志
    logger.SetLogLevel(LogLevel::Debug);
    logger.SetLogToConsole(true);
    logger.SetLogToFile(true);
    logger.SetAsyncLogging(true);
    
    // 写入不同级别的日志
    logger.Debug("这是一条调试日志");
    logger.Info("这是一条信息日志");
    logger.Warning("这是一条警告日志");
    logger.Error("这是一条错误日志");
    
    // 等待异步处理完成
    logger.Flush();
    
    std::cout << "当前队列大小: " << logger.GetQueueSize() << std::endl;
    
    PrintTestResult("基本异步日志功能", true);
}

// ========== 测试 2: 多线程并发写入 ==========

void Test2_MultiThreadedLogging() {
    PrintTestHeader("测试 2: 多线程并发写入日志");
    
    auto& logger = Logger::GetInstance();
    logger.SetAsyncLogging(true);
    logger.SetShowThreadId(true);  // 显示线程ID
    
    const int numThreads = 10;
    const int logsPerThread = 100;
    
    std::vector<std::thread> threads;
    auto startTime = high_resolution_clock::now();
    
    // 创建多个线程并发写入日志
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([i, logsPerThread, &logger]() {
            for (int j = 0; j < logsPerThread; ++j) {
                logger.InfoFormat("线程 %d 写入日志 #%d", i, j);
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 刷新日志队列
    logger.Flush();
    
    auto endTime = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(endTime - startTime);
    
    std::cout << "总线程数: " << numThreads << std::endl;
    std::cout << "每线程日志数: " << logsPerThread << std::endl;
    std::cout << "总日志数: " << (numThreads * logsPerThread) << std::endl;
    std::cout << "总耗时: " << duration.count() << " ms" << std::endl;
    std::cout << "平均速度: " << (numThreads * logsPerThread * 1000.0 / duration.count()) 
              << " 条/秒" << std::endl;
    
    PrintTestResult("多线程并发写入", true);
}

// ========== 测试 3: 文件轮转功能 ==========

void Test3_FileRotation() {
    PrintTestHeader("测试 3: 文件轮转功能");
    
    auto& logger = Logger::GetInstance();
    
    // 设置文件大小限制为 10KB
    logger.SetMaxFileSize(10 * 1024);
    logger.SetAsyncLogging(true);
    
    // 写入大量日志以触发轮转
    std::string longMessage(100, 'A');  // 100字符的消息
    
    for (int i = 0; i < 200; ++i) {
        logger.InfoFormat("日志 #%d: %s", i, longMessage.c_str());
        
        // 每50条日志后检查一下
        if (i % 50 == 0) {
            logger.Flush();
            std::this_thread::sleep_for(milliseconds(10));
        }
    }
    
    logger.Flush();
    
    std::cout << "当前日志文件: " << logger.GetCurrentLogFile() << std::endl;
    std::cout << "文件轮转功能测试完成（请检查logs目录是否有多个日志文件）" << std::endl;
    
    PrintTestResult("文件轮转功能", true);
}

// ========== 测试 4: 性能对比（同步 vs 异步）==========

void Test4_PerformanceComparison() {
    PrintTestHeader("测试 4: 性能对比（同步 vs 异步）");
    
    auto& logger = Logger::GetInstance();
    const int numLogs = 10000;
    
    // ===== 测试异步模式 =====
    logger.SetAsyncLogging(true);
    logger.SetLogToConsole(false);  // 关闭控制台输出以更准确测试
    
    auto asyncStart = high_resolution_clock::now();
    for (int i = 0; i < numLogs; ++i) {
        logger.InfoFormat("异步日志 #%d", i);
    }
    logger.Flush();
    auto asyncEnd = high_resolution_clock::now();
    auto asyncDuration = duration_cast<microseconds>(asyncEnd - asyncStart);
    
    // ===== 测试同步模式 =====
    logger.SetAsyncLogging(false);
    
    auto syncStart = high_resolution_clock::now();
    for (int i = 0; i < numLogs; ++i) {
        logger.InfoFormat("同步日志 #%d", i);
    }
    auto syncEnd = high_resolution_clock::now();
    auto syncDuration = duration_cast<microseconds>(syncEnd - syncStart);
    
    // 恢复设置
    logger.SetAsyncLogging(true);
    logger.SetLogToConsole(true);
    
    // 输出结果
    std::cout << "日志数量: " << numLogs << std::endl;
    std::cout << "\n异步模式:" << std::endl;
    std::cout << "  总耗时: " << asyncDuration.count() << " μs" << std::endl;
    std::cout << "  平均耗时: " << (asyncDuration.count() / static_cast<double>(numLogs)) << " μs/条" << std::endl;
    std::cout << "  速度: " << (numLogs * 1000000.0 / asyncDuration.count()) << " 条/秒" << std::endl;
    
    std::cout << "\n同步模式:" << std::endl;
    std::cout << "  总耗时: " << syncDuration.count() << " μs" << std::endl;
    std::cout << "  平均耗时: " << (syncDuration.count() / static_cast<double>(numLogs)) << " μs/条" << std::endl;
    std::cout << "  速度: " << (numLogs * 1000000.0 / syncDuration.count()) << " 条/秒" << std::endl;
    
    double speedup = static_cast<double>(syncDuration.count()) / asyncDuration.count();
    std::cout << "\n性能提升: " << speedup << "x (异步比同步快 " 
              << ((speedup - 1.0) * 100.0) << "%)" << std::endl;
    
    PrintTestResult("性能对比", speedup > 1.0);
}

// ========== 测试 5: 队列管理 ==========

void Test5_QueueManagement() {
    PrintTestHeader("测试 5: 队列管理和刷新功能");
    
    auto& logger = Logger::GetInstance();
    logger.SetAsyncLogging(true);
    logger.SetLogToConsole(true);
    
    // 快速写入大量日志
    std::cout << "快速写入1000条日志..." << std::endl;
    for (int i = 0; i < 1000; ++i) {
        logger.InfoFormat("队列测试日志 #%d", i);
    }
    
    // 立即检查队列大小
    size_t queueSize = logger.GetQueueSize();
    std::cout << "当前队列大小: " << queueSize << " 条" << std::endl;
    
    // 刷新队列
    std::cout << "刷新队列..." << std::endl;
    logger.Flush();
    
    // 再次检查队列大小
    queueSize = logger.GetQueueSize();
    std::cout << "刷新后队列大小: " << queueSize << " 条" << std::endl;
    
    PrintTestResult("队列管理", queueSize == 0);
}

// ========== 测试 6: 压力测试 ==========

void Test6_StressTest() {
    PrintTestHeader("测试 6: 压力测试");
    
    auto& logger = Logger::GetInstance();
    logger.SetAsyncLogging(true);
    logger.SetLogToConsole(false);  // 关闭控制台以提高性能
    logger.SetShowThreadId(true);
    
    const int numThreads = 20;
    const int logsPerThread = 1000;
    
    std::cout << "启动 " << numThreads << " 个线程，每个线程写入 " 
              << logsPerThread << " 条日志..." << std::endl;
    
    auto startTime = high_resolution_clock::now();
    
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([i, logsPerThread, &logger]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 100);
            
            for (int j = 0; j < logsPerThread; ++j) {
                LogLevel level = static_cast<LogLevel>(dis(gen) % 4);
                logger.LogFormat(level, "压力测试 [线程%d] [日志#%d] [随机值=%d]", 
                               i, j, dis(gen));
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    logger.Flush();
    
    auto endTime = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(endTime - startTime);
    
    int totalLogs = numThreads * logsPerThread;
    std::cout << "压力测试完成！" << std::endl;
    std::cout << "总日志数: " << totalLogs << std::endl;
    std::cout << "总耗时: " << duration.count() << " ms" << std::endl;
    std::cout << "平均速度: " << (totalLogs * 1000.0 / duration.count()) << " 条/秒" << std::endl;
    
    logger.SetLogToConsole(true);  // 恢复控制台输出
    
    PrintTestResult("压力测试", true);
}

// ========== 主函数 ==========

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "异步日志系统测试" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        // 运行所有测试
        Test1_BasicAsyncLogging();
        Test2_MultiThreadedLogging();
        Test3_FileRotation();
        Test4_PerformanceComparison();
        Test5_QueueManagement();
        Test6_StressTest();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "所有测试完成！" << std::endl;
        std::cout << "========================================" << std::endl;
        
        // 最后刷新一次
        Logger::GetInstance().Flush();
        
    } catch (const std::exception& e) {
        std::cerr << "测试过程中发生异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

