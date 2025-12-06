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
 * @file 07_thread_safe_test.cpp
 * @brief 着色器系统线程安全测试
 * 
 * 此示例测试着色器系统在多线程环境下的安全性
 */

#include "render/shader_cache.h"
#include "render/logger.h"
#include <glad/glad.h>
#include <SDL3/SDL.h>
#include <thread>
#include <vector>
#include <chrono>

using namespace Render;

// 测试函数：多个线程同时加载着色器
void TestConcurrentLoad(int threadId) {
    LOG_INFO("线程 " + std::to_string(threadId) + " 开始加载着色器");
    
    for (int i = 0; i < 10; ++i) {
        auto& cache = ShaderCache::GetInstance();
        
        // 多个线程尝试加载同一个着色器
        auto shader = cache.LoadShader(
            "basic_shader",
            "shaders/basic.vert",
            "shaders/basic.frag"
        );
        
        if (shader) {
            // 使用着色器
            shader->Use();
            
            // 获取 uniform 管理器并进行操作
            auto* uniformMgr = shader->GetUniformManager();
            if (uniformMgr) {
                uniformMgr->SetFloat("testFloat", static_cast<float>(i));
                uniformMgr->SetInt("testInt", i);
            }
            
            shader->Unuse();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    LOG_INFO("线程 " + std::to_string(threadId) + " 完成");
}

// 测试函数：并发获取和使用着色器
void TestConcurrentUse(int threadId) {
    LOG_INFO("线程 " + std::to_string(threadId) + " 开始使用着色器");
    
    for (int i = 0; i < 20; ++i) {
        auto& cache = ShaderCache::GetInstance();
        auto shader = cache.GetShader("basic_shader");
        
        if (shader) {
            shader->Use();
            
            // 模拟一些操作
            auto* uniformMgr = shader->GetUniformManager();
            if (uniformMgr) {
                uniformMgr->SetFloat("time", static_cast<float>(i));
                uniformMgr->SetVector3("color", Vector3(1.0f, 0.0f, 0.0f));
            }
            
            shader->Unuse();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    LOG_INFO("线程 " + std::to_string(threadId) + " 完成使用");
}

// 测试函数：并发重载着色器
void TestConcurrentReload(int threadId) {
    LOG_INFO("线程 " + std::to_string(threadId) + " 开始重载着色器");
    
    for (int i = 0; i < 5; ++i) {
        auto& cache = ShaderCache::GetInstance();
        cache.ReloadShader("basic_shader");
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    LOG_INFO("线程 " + std::to_string(threadId) + " 完成重载");
}

int main(int argc, char* argv[]) {
    // 初始化日志系统
    Logger::GetInstance().SetLogToConsole(true);
    Logger::GetInstance().SetLogLevel(LogLevel::Info);
    LOG_INFO("========================================");
    LOG_INFO("着色器系统线程安全测试");
    LOG_INFO("========================================");
    
    // 初始化 SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_ERROR("Failed to initialize SDL");
        return -1;
    }
    
    // 设置 OpenGL 属性
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    
    // 创建隐藏窗口用于测试
    SDL_Window* window = SDL_CreateWindow(
        "Thread Safe Test",
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN
    );
    
    if (!window) {
        LOG_ERROR("Failed to create window");
        SDL_Quit();
        return -1;
    }
    
    // 创建 OpenGL 上下文
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        LOG_ERROR("Failed to create OpenGL context");
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
    
    // 测试 1: 并发加载着色器
    LOG_INFO("\n测试 1: 多线程并发加载同一着色器");
    LOG_INFO("----------------------------------------");
    {
        std::vector<std::thread> threads;
        const int numThreads = 5;
        
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back(TestConcurrentLoad, i);
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        LOG_INFO("测试 1 完成\n");
    }
    
    // 打印缓存统计
    ShaderCache::GetInstance().PrintStatistics();
    
    // 测试 2: 并发使用着色器
    LOG_INFO("\n测试 2: 多线程并发使用着色器");
    LOG_INFO("----------------------------------------");
    {
        std::vector<std::thread> threads;
        const int numThreads = 8;
        
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back(TestConcurrentUse, i);
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        LOG_INFO("测试 2 完成\n");
    }
    
    // 测试 3: 并发重载着色器（同时有其他线程在使用）
    LOG_INFO("\n测试 3: 并发重载着色器");
    LOG_INFO("----------------------------------------");
    {
        std::vector<std::thread> threads;
        
        // 启动一些使用线程
        for (int i = 0; i < 4; ++i) {
            threads.emplace_back(TestConcurrentUse, i);
        }
        
        // 启动一些重载线程
        for (int i = 0; i < 2; ++i) {
            threads.emplace_back(TestConcurrentReload, i + 100);
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        LOG_INFO("测试 3 完成\n");
    }
    
    // 最终统计
    LOG_INFO("\n最终统计信息");
    LOG_INFO("========================================");
    ShaderCache::GetInstance().PrintStatistics();
    
    // 清理
    ShaderCache::GetInstance().Clear();
    
    SDL_GL_DestroyContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    LOG_INFO("\n所有线程安全测试完成！");
    LOG_INFO("========================================");
    
    return 0;
}

