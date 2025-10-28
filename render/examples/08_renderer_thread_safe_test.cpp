/**
 * @file 08_renderer_thread_safe_test.cpp
 * @brief Renderer 类线程安全测试
 * 
 * 此示例测试 Renderer 类在多线程环境下的安全性
 */

#include "render/renderer.h"
#include "render/logger.h"
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>

using namespace Render;

// 全局 Renderer 实例
static Renderer* g_renderer = nullptr;
static std::atomic<bool> g_running(true);

/**
 * @brief 测试函数：多个线程并发获取渲染器状态
 */
void TestConcurrentStateQueries(int threadId) {
    LOG_INFO("线程 " + std::to_string(threadId) + " 开始查询状态");
    
    int iterations = 0;
    while (g_running && iterations < 100) {
        if (g_renderer && g_renderer->IsInitialized()) {
            // 并发查询各种状态
            int width = g_renderer->GetWidth();
            int height = g_renderer->GetHeight();
            float deltaTime = g_renderer->GetDeltaTime();
            float fps = g_renderer->GetFPS();
            RenderStats stats = g_renderer->GetStats();
            
            // 验证数据的合理性
            if (width > 0 && height > 0) {
                // 正常
                iterations++;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    LOG_INFO("线程 " + std::to_string(threadId) + " 完成状态查询，迭代次数: " + std::to_string(iterations));
}

/**
 * @brief 测试函数：多个线程并发修改渲染器设置
 */
void TestConcurrentSettingChanges(int threadId) {
    LOG_INFO("线程 " + std::to_string(threadId) + " 开始修改设置");
    
    int iterations = 0;
    while (g_running && iterations < 50) {
        if (g_renderer && g_renderer->IsInitialized()) {
            // 并发修改各种设置
            float r = (threadId * 0.1f);
            float g = (threadId * 0.15f);
            float b = (threadId * 0.2f);
            g_renderer->SetClearColor(r, g, b, 1.0f);
            
            // 获取渲染状态并修改
            auto* renderState = g_renderer->GetRenderState();
            if (renderState) {
                renderState->SetDepthTest(iterations % 2 == 0);
                renderState->SetBlendMode(iterations % 2 == 0 ? BlendMode::Alpha : BlendMode::None);
            }
            
            iterations++;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    LOG_INFO("线程 " + std::to_string(threadId) + " 完成设置修改，迭代次数: " + std::to_string(iterations));
}

/**
 * @brief 测试函数：模拟渲染循环
 */
void TestRenderLoop() {
    LOG_INFO("渲染循环线程开始");
    
    int frameCount = 0;
    while (g_running && frameCount < 100) {
        if (g_renderer && g_renderer->IsInitialized()) {
            g_renderer->BeginFrame();
            
            // 模拟一些渲染操作
            g_renderer->Clear(true, true, false);
            
            // 获取并打印统计信息（每20帧一次）
            if (frameCount % 20 == 0) {
                RenderStats stats = g_renderer->GetStats();
                LOG_INFO("帧 " + std::to_string(frameCount) + 
                        " - FPS: " + std::to_string(stats.fps) + 
                        " - 帧时间: " + std::to_string(stats.frameTime) + "ms");
            }
            
            g_renderer->EndFrame();
            g_renderer->Present();
            
            frameCount++;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
    
    LOG_INFO("渲染循环线程完成，总帧数: " + std::to_string(frameCount));
}

/**
 * @brief 测试函数：并发访问 OpenGL 上下文
 */
void TestConcurrentContextAccess(int threadId) {
    LOG_INFO("线程 " + std::to_string(threadId) + " 开始访问上下文");
    
    int iterations = 0;
    while (g_running && iterations < 100) {
        if (g_renderer && g_renderer->IsInitialized()) {
            // 获取上下文指针（不实际调用 OpenGL 函数，因为可能不在正确的线程）
            auto* context = g_renderer->GetContext();
            if (context) {
                // 只是验证指针有效性
                bool isInit = context->IsInitialized();
                if (isInit) {
                    iterations++;
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    LOG_INFO("线程 " + std::to_string(threadId) + " 完成上下文访问，迭代次数: " + std::to_string(iterations));
}

int main(int argc, char* argv[]) {
    // 初始化日志系统
    Logger::GetInstance().SetLogToConsole(true);
    Logger::GetInstance().SetLogLevel(LogLevel::Info);
    
    LOG_INFO("========================================");
    LOG_INFO("Renderer 线程安全测试");
    LOG_INFO("========================================");
    
    // 创建并初始化渲染器
    g_renderer = Renderer::Create();
    if (!g_renderer->Initialize("Renderer Thread Safety Test", 800, 600)) {
        LOG_ERROR("Failed to initialize renderer");
        Renderer::Destroy(g_renderer);
        return -1;
    }
    
    LOG_INFO("Renderer 初始化成功\n");
    
    // ========================================================================
    // 测试 1: 并发状态查询
    // ========================================================================
    LOG_INFO("测试 1: 多线程并发查询状态");
    LOG_INFO("----------------------------------------");
    {
        g_running = true;
        std::vector<std::thread> threads;
        const int numThreads = 8;
        
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back(TestConcurrentStateQueries, i);
        }
        
        // 让测试运行一段时间
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        LOG_INFO("测试 1 完成\n");
    }
    
    // ========================================================================
    // 测试 2: 并发设置修改
    // ========================================================================
    LOG_INFO("测试 2: 多线程并发修改设置");
    LOG_INFO("----------------------------------------");
    {
        g_running = true;
        std::vector<std::thread> threads;
        const int numThreads = 6;
        
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back(TestConcurrentSettingChanges, i);
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        LOG_INFO("测试 2 完成\n");
    }
    
    // ========================================================================
    // 测试 3: 渲染循环 + 并发查询
    // ========================================================================
    LOG_INFO("测试 3: 渲染循环同时进行并发查询和设置");
    LOG_INFO("----------------------------------------");
    {
        g_running = true;
        std::vector<std::thread> threads;
        
        // 启动渲染循环
        threads.emplace_back(TestRenderLoop);
        
        // 启动查询线程
        for (int i = 0; i < 4; ++i) {
            threads.emplace_back(TestConcurrentStateQueries, i + 100);
        }
        
        // 启动设置修改线程
        for (int i = 0; i < 3; ++i) {
            threads.emplace_back(TestConcurrentSettingChanges, i + 200);
        }
        
        // 启动上下文访问线程
        for (int i = 0; i < 2; ++i) {
            threads.emplace_back(TestConcurrentContextAccess, i + 300);
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(3));
        g_running = false;
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        LOG_INFO("测试 3 完成\n");
    }
    
    // ========================================================================
    // 测试 4: 压力测试 - 大量线程同时访问
    // ========================================================================
    LOG_INFO("测试 4: 压力测试 - 20个线程同时访问");
    LOG_INFO("----------------------------------------");
    {
        g_running = true;
        std::vector<std::thread> threads;
        const int numThreads = 20;
        
        for (int i = 0; i < numThreads; ++i) {
            if (i % 3 == 0) {
                threads.emplace_back(TestConcurrentStateQueries, i + 400);
            } else if (i % 3 == 1) {
                threads.emplace_back(TestConcurrentSettingChanges, i + 400);
            } else {
                threads.emplace_back(TestConcurrentContextAccess, i + 400);
            }
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        LOG_INFO("测试 4 完成\n");
    }
    
    // ========================================================================
    // 清理
    // ========================================================================
    LOG_INFO("========================================");
    LOG_INFO("清理资源...");
    
    // 最终统计
    RenderStats finalStats = g_renderer->GetStats();
    LOG_INFO("最终统计:");
    LOG_INFO("  - FPS: " + std::to_string(finalStats.fps));
    LOG_INFO("  - 帧时间: " + std::to_string(finalStats.frameTime) + "ms");
    LOG_INFO("  - 绘制调用: " + std::to_string(finalStats.drawCalls));
    
    Renderer::Destroy(g_renderer);
    g_renderer = nullptr;
    
    LOG_INFO("========================================");
    LOG_INFO("所有线程安全测试完成！");
    LOG_INFO("========================================");
    
    return 0;
}

