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
 * @file 09_texture_thread_safe_test.cpp
 * @brief 纹理系统线程安全测试
 * 
 * 此示例测试纹理系统在多线程环境下的安全性，包括：
 * - 多线程并发加载纹理
 * - 多线程并发使用纹理
 * - 多线程并发访问纹理属性
 * - TextureLoader 的并发缓存操作
 */

#include "render/texture_loader.h"
#include "render/texture.h"
#include "render/logger.h"
#include <glad/glad.h>
#include <SDL3/SDL.h>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>

using namespace Render;

std::atomic<int> g_loadSuccessCount(0);
std::atomic<int> g_loadFailCount(0);
std::atomic<int> g_bindCount(0);
std::atomic<int> g_propertyReadCount(0);

// 测试函数1：多个线程同时加载同一纹理
void TestConcurrentLoad(int threadId) {
    LOG_INFO("线程 " + std::to_string(threadId) + " 开始加载纹理");
    
    for (int i = 0; i < 20; ++i) {
        auto& loader = TextureLoader::GetInstance();
        
        // 多个线程尝试加载同一个纹理（应该只加载一次，其他线程从缓存获取）
        auto texture = loader.LoadTexture(
            "test_texture",
            "textures/test.jpg",
            true
        );
        
        if (texture && texture->IsValid()) {
            g_loadSuccessCount++;
            
            // 读取纹理属性（测试线程安全的属性访问）
            int width = texture->GetWidth();
            int height = texture->GetHeight();
            g_propertyReadCount++;
            
            // 模拟一些处理时间
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        } else {
            g_loadFailCount++;
        }
    }
    
    LOG_INFO("线程 " + std::to_string(threadId) + " 完成加载测试");
}

// 测试函数2：多个线程并发使用纹理
void TestConcurrentUse(int threadId) {
    LOG_INFO("线程 " + std::to_string(threadId) + " 开始使用纹理");
    
    auto& loader = TextureLoader::GetInstance();
    
    for (int i = 0; i < 30; ++i) {
        // 从缓存获取纹理
        auto texture = loader.GetTexture("test_texture");
        
        if (texture && texture->IsValid()) {
            // 绑定纹理（测试线程安全的绑定操作）
            texture->Bind(0);
            g_bindCount++;
            
            // 读取属性
            int width = texture->GetWidth();
            int height = texture->GetHeight();
            TextureFormat format = texture->GetFormat();
            g_propertyReadCount++;
            
            // 模拟使用
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    
    LOG_INFO("线程 " + std::to_string(threadId) + " 完成使用测试");
}

// 测试函数3：并发创建不同的纹理
void TestConcurrentCreateDifferent(int threadId) {
    LOG_INFO("线程 " + std::to_string(threadId) + " 开始创建纹理");
    
    auto& loader = TextureLoader::GetInstance();
    
    // 每个线程创建一个棋盘格纹理
    std::string textureName = "checkerboard_" + std::to_string(threadId);
    
    // 生成棋盘格数据
    const int size = 256;
    std::vector<unsigned char> data(size * size * 4);
    
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int idx = (y * size + x) * 4;
            bool isWhite = ((x / 32) + (y / 32)) % 2 == 0;
            unsigned char color = isWhite ? 255 : 0;
            
            data[idx + 0] = color;
            data[idx + 1] = color;
            data[idx + 2] = color;
            data[idx + 3] = 255;
        }
    }
    
    // 创建纹理
    auto texture = loader.CreateTexture(
        textureName,
        data.data(),
        size,
        size,
        TextureFormat::RGBA,
        true
    );
    
    if (texture && texture->IsValid()) {
        g_loadSuccessCount++;
        
        // 多次访问纹理属性
        for (int i = 0; i < 10; ++i) {
            int width = texture->GetWidth();
            int height = texture->GetHeight();
            g_propertyReadCount++;
            
            texture->Bind(0);
            g_bindCount++;
            
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    } else {
        g_loadFailCount++;
    }
    
    LOG_INFO("线程 " + std::to_string(threadId) + " 完成创建测试");
}

// 测试函数4：并发访问 TextureLoader 的各种方法
void TestConcurrentLoaderMethods(int threadId) {
    LOG_INFO("线程 " + std::to_string(threadId) + " 开始测试加载器方法");
    
    auto& loader = TextureLoader::GetInstance();
    
    for (int i = 0; i < 15; ++i) {
        // 检查纹理是否存在
        bool hasTexture = loader.HasTexture("test_texture");
        
        // 获取纹理数量
        size_t count = loader.GetTextureCount();
        
        // 获取引用计数
        long refCount = loader.GetReferenceCount("test_texture");
        
        // 获取内存使用
        size_t memUsage = loader.GetTotalMemoryUsage();
        
        g_propertyReadCount++;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }
    
    LOG_INFO("线程 " + std::to_string(threadId) + " 完成加载器方法测试");
}

// 测试函数5：异步加载测试
void TestAsyncLoad(int threadId) {
    LOG_INFO("线程 " + std::to_string(threadId) + " 开始异步加载测试");
    
    auto& loader = TextureLoader::GetInstance();
    
    std::string textureName = "async_texture_" + std::to_string(threadId);
    
    // 启动异步加载
    auto future = loader.LoadTextureAsync(
        textureName,
        "textures/test.jpg",
        true
    );
    
    // 模拟做其他工作
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 等待加载完成
    AsyncTextureResult result = future.get();
    
    if (result.success && result.texture) {
        g_loadSuccessCount++;
        
        // 使用纹理
        for (int i = 0; i < 5; ++i) {
            result.texture->Bind(0);
            g_bindCount++;
            
            int width = result.texture->GetWidth();
            int height = result.texture->GetHeight();
            g_propertyReadCount++;
            
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    } else {
        g_loadFailCount++;
        LOG_ERROR("异步加载失败: " + result.error);
    }
    
    LOG_INFO("线程 " + std::to_string(threadId) + " 完成异步加载测试");
}

int main(int argc, char* argv[]) {
    // 初始化日志系统
    Logger::GetInstance().SetLogToConsole(true);
    Logger::GetInstance().SetLogLevel(LogLevel::Info);
    LOG_INFO("========================================");
    LOG_INFO("纹理系统线程安全测试");
    LOG_INFO("========================================");
    
    // 初始化 SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_ERROR("Failed to initialize SDL: " + std::string(SDL_GetError()));
        return -1;
    }
    
    // 设置 OpenGL 属性
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    
    // 创建隐藏窗口用于测试
    SDL_Window* window = SDL_CreateWindow(
        "Texture Thread Safe Test",
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN
    );
    
    if (!window) {
        LOG_ERROR("Failed to create window: " + std::string(SDL_GetError()));
        SDL_Quit();
        return -1;
    }
    
    // 创建 OpenGL 上下文
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        LOG_ERROR("Failed to create OpenGL context: " + std::string(SDL_GetError()));
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    
    // 初始化 GLAD
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        LOG_ERROR("Failed to initialize GLAD");
        SDL_GL_DestroyContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    
    LOG_INFO("OpenGL Context initialized");
    LOG_INFO("OpenGL Version: " + std::string(reinterpret_cast<const char*>(glGetString(GL_VERSION))));
    
    // 检查测试纹理是否存在
    std::ifstream testFile("textures/test.jpg");
    if (!testFile.good()) {
        LOG_WARNING("测试纹理 textures/test.jpg 不存在，某些测试可能失败");
    }
    testFile.close();
    
    // 测试 1: 并发加载同一纹理
    LOG_INFO("\n========================================");
    LOG_INFO("测试 1: 多线程并发加载同一纹理");
    LOG_INFO("========================================");
    {
        g_loadSuccessCount = 0;
        g_loadFailCount = 0;
        std::vector<std::thread> threads;
        const int numThreads = 8;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back(TestConcurrentLoad, i);
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        LOG_INFO("测试 1 完成");
        LOG_INFO("成功加载次数: " + std::to_string(g_loadSuccessCount));
        LOG_INFO("失败次数: " + std::to_string(g_loadFailCount));
        LOG_INFO("耗时: " + std::to_string(duration.count()) + " ms");
    }
    
    // 打印缓存统计
    TextureLoader::GetInstance().PrintStatistics();
    
    // 测试 2: 并发使用纹理
    LOG_INFO("\n========================================");
    LOG_INFO("测试 2: 多线程并发使用纹理");
    LOG_INFO("========================================");
    {
        g_bindCount = 0;
        g_propertyReadCount = 0;
        std::vector<std::thread> threads;
        const int numThreads = 10;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back(TestConcurrentUse, i);
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        LOG_INFO("测试 2 完成");
        LOG_INFO("总绑定次数: " + std::to_string(g_bindCount));
        LOG_INFO("属性读取次数: " + std::to_string(g_propertyReadCount));
        LOG_INFO("耗时: " + std::to_string(duration.count()) + " ms");
    }
    
    // 测试 3: 并发创建不同的纹理
    LOG_INFO("\n========================================");
    LOG_INFO("测试 3: 多线程并发创建不同纹理");
    LOG_INFO("========================================");
    {
        g_loadSuccessCount = 0;
        g_loadFailCount = 0;
        g_bindCount = 0;
        g_propertyReadCount = 0;
        std::vector<std::thread> threads;
        const int numThreads = 6;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back(TestConcurrentCreateDifferent, i);
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        LOG_INFO("测试 3 完成");
        LOG_INFO("成功创建: " + std::to_string(g_loadSuccessCount));
        LOG_INFO("失败: " + std::to_string(g_loadFailCount));
        LOG_INFO("绑定次数: " + std::to_string(g_bindCount));
        LOG_INFO("耗时: " + std::to_string(duration.count()) + " ms");
    }
    
    // 打印缓存统计
    TextureLoader::GetInstance().PrintStatistics();
    
    // 测试 4: 并发访问 TextureLoader 方法
    LOG_INFO("\n========================================");
    LOG_INFO("测试 4: 多线程并发访问 TextureLoader 方法");
    LOG_INFO("========================================");
    {
        g_propertyReadCount = 0;
        std::vector<std::thread> threads;
        const int numThreads = 12;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back(TestConcurrentLoaderMethods, i);
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        LOG_INFO("测试 4 完成");
        LOG_INFO("方法调用次数: " + std::to_string(g_propertyReadCount));
        LOG_INFO("耗时: " + std::to_string(duration.count()) + " ms");
    }
    
    // 测试 5: 异步加载测试
    LOG_INFO("\n========================================");
    LOG_INFO("测试 5: 多线程异步加载");
    LOG_INFO("========================================");
    {
        g_loadSuccessCount = 0;
        g_loadFailCount = 0;
        g_bindCount = 0;
        std::vector<std::thread> threads;
        const int numThreads = 5;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back(TestAsyncLoad, i);
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        LOG_INFO("测试 5 完成");
        LOG_INFO("成功: " + std::to_string(g_loadSuccessCount));
        LOG_INFO("失败: " + std::to_string(g_loadFailCount));
        LOG_INFO("耗时: " + std::to_string(duration.count()) + " ms");
    }
    
    // 最终统计
    LOG_INFO("\n========================================");
    LOG_INFO("最终统计信息");
    LOG_INFO("========================================");
    TextureLoader::GetInstance().PrintStatistics();
    
    // 测试清理功能
    LOG_INFO("\n测试清理未使用的纹理...");
    size_t cleanedCount = TextureLoader::GetInstance().CleanupUnused();
    LOG_INFO("清理了 " + std::to_string(cleanedCount) + " 个未使用的纹理");
    
    TextureLoader::GetInstance().PrintStatistics();
    
    // 清理所有
    LOG_INFO("\n清理所有纹理缓存...");
    TextureLoader::GetInstance().Clear();
    
    // 清理 OpenGL 上下文
    SDL_GL_DestroyContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    LOG_INFO("\n========================================");
    LOG_INFO("所有纹理系统线程安全测试完成！");
    LOG_INFO("========================================");
    
    return 0;
}

