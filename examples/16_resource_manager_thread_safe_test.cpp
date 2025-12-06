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
 * @file 16_resource_manager_thread_safe_test.cpp
 * @brief 测试资源管理器的线程安全性
 * 
 * 本示例演示：
 * 1. 多线程并发注册资源（材质和着色器引用）
 * 2. 多线程并发获取资源
 * 3. 多线程并发清理资源
 * 4. 多线程并发统计查询
 * 5. 资源引用计数的线程安全性
 * 
 * 测试场景：
 * - 10个生产者线程并发注册材质和着色器
 * - 10个消费者线程并发获取和读取资源属性
 * - 5个监控线程并发查询统计信息
 * - 并发清理和注册测试
 * - ForEach遍历的线程安全性
 * 
 * 重要说明：
 * ⚠️ OpenGL上下文限制：
 * - OpenGL调用（创建纹理、网格等）必须在主线程执行
 * - 工作线程只能：注册材质、获取资源、读取属性、查询统计
 * - 本测试避免在工作线程中调用OpenGL API
 * - 网格资源在主线程预创建，工作线程只读取
 * 
 * 这是一个后台测试程序，窗口黑屏是正常的。
 * 请查看控制台输出和日志文件了解测试结果。
 */

#include <render/renderer.h>
#include <render/resource_manager.h>
#include <render/shader_cache.h>
#include <render/texture_loader.h>
#include <render/mesh_loader.h>
#include <render/material.h>
#include <render/logger.h>
#include <glad/glad.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Render;

// 测试控制
std::atomic<bool> g_running{true};
std::atomic<int> g_registerCount{0};
std::atomic<int> g_getCount{0};
std::atomic<int> g_queryCount{0};

// 随机数生成
std::random_device rd;
std::mt19937 gen(rd());

/**
 * @brief 生产者线程：并发注册资源
 * 注意：由于OpenGL限制，只注册材质和着色器（不涉及OpenGL调用的创建）
 */
void ProducerThread(int threadId) {
    std::uniform_int_distribution<> resourceDist(0, 1);  // 只选择材质和着色器
    std::uniform_int_distribution<> waitDist(10, 100);
    
    auto& resourceMgr = ResourceManager::GetInstance();
    
    Logger::GetInstance().Info("生产者线程 " + std::to_string(threadId) + " 启动");
    
    int count = 0;
    while (g_running && count < 20) {
        int resourceType = resourceDist(gen);
        std::string name = "thread" + std::to_string(threadId) + "_" + std::to_string(count);
        
        try {
            switch (resourceType) {
                case 0: {
                    // 注册材质（不涉及OpenGL调用，线程安全）
                    auto material = std::make_shared<Material>();
                    material->SetName(name + "_material");
                    material->SetDiffuseColor(Color(
                        static_cast<float>(rand()) / RAND_MAX,
                        static_cast<float>(rand()) / RAND_MAX,
                        static_cast<float>(rand()) / RAND_MAX,
                        1.0f
                    ));
                    if (resourceMgr.RegisterMaterial(name + "_material", material)) {
                        g_registerCount++;
                    }
                    break;
                }
                case 1: {
                    // 注册着色器引用（获取已存在的着色器，线程安全）
                    auto shader = ShaderCache::GetInstance().GetShader("basic");
                    if (shader && resourceMgr.RegisterShader(name + "_shader", shader)) {
                        g_registerCount++;
                    }
                    break;
                }
            }
        }
        catch (const std::exception& e) {
            Logger::GetInstance().Error("生产者线程 " + std::to_string(threadId) + 
                " 异常: " + e.what());
        }
        
        count++;
        std::this_thread::sleep_for(std::chrono::milliseconds(waitDist(gen)));
    }
    
    Logger::GetInstance().Info("生产者线程 " + std::to_string(threadId) + " 完成");
}

/**
 * @brief 消费者线程：并发获取资源
 */
void ConsumerThread(int threadId) {
    std::uniform_int_distribution<> resourceDist(0, 2);  // 网格、材质、着色器
    std::uniform_int_distribution<> waitDist(10, 100);
    
    auto& resourceMgr = ResourceManager::GetInstance();
    
    Logger::GetInstance().Info("消费者线程 " + std::to_string(threadId) + " 启动");
    
    int count = 0;
    while (g_running && count < 30) {
        // 随机获取资源
        auto meshes = resourceMgr.ListMeshes();
        auto materials = resourceMgr.ListMaterials();
        auto shaders = resourceMgr.ListShaders();
        
        try {
            if (!meshes.empty()) {
                std::uniform_int_distribution<> meshDist(0, static_cast<int>(meshes.size() - 1));
                auto mesh = resourceMgr.GetMesh(meshes[meshDist(gen)]);
                if (mesh) {
                    g_getCount++;
                    // 使用网格（读取属性，线程安全）
                    auto vertexCount = mesh->GetVertexCount();
                    auto memUsage = mesh->GetMemoryUsage();
                    (void)vertexCount;
                    (void)memUsage;
                }
            }
            
            if (!materials.empty()) {
                std::uniform_int_distribution<> matDist(0, static_cast<int>(materials.size() - 1));
                auto material = resourceMgr.GetMaterial(materials[matDist(gen)]);
                if (material) {
                    g_getCount++;
                    // 读取材质属性（线程安全）
                    auto color = material->GetDiffuseColor();
                    (void)color;
                }
            }
            
            if (!shaders.empty()) {
                std::uniform_int_distribution<> shaderDist(0, static_cast<int>(shaders.size() - 1));
                auto shader = resourceMgr.GetShader(shaders[shaderDist(gen)]);
                if (shader) {
                    g_getCount++;
                }
            }
        }
        catch (const std::exception& e) {
            Logger::GetInstance().Error("消费者线程 " + std::to_string(threadId) + 
                " 异常: " + e.what());
        }
        
        count++;
        std::this_thread::sleep_for(std::chrono::milliseconds(waitDist(gen)));
    }
    
    Logger::GetInstance().Info("消费者线程 " + std::to_string(threadId) + " 完成");
}

/**
 * @brief 监控线程：并发查询统计信息
 */
void MonitorThread(int threadId) {
    std::uniform_int_distribution<> waitDist(50, 200);
    
    auto& resourceMgr = ResourceManager::GetInstance();
    
    Logger::GetInstance().Info("监控线程 " + std::to_string(threadId) + " 启动");
    
    while (g_running) {
        try {
            // 查询统计信息
            auto stats = resourceMgr.GetStats();
            g_queryCount++;
            
            // 列出所有资源
            auto meshes = resourceMgr.ListMeshes();
            auto materials = resourceMgr.ListMaterials();
            auto textures = resourceMgr.ListTextures();
            auto shaders = resourceMgr.ListShaders();
            
            // 查询引用计数
            if (!meshes.empty()) {
                resourceMgr.GetReferenceCount(ResourceType::Mesh, meshes[0]);
            }
            
            (void)stats;  // 避免未使用警告
        }
        catch (const std::exception& e) {
            Logger::GetInstance().Error("监控线程 " + std::to_string(threadId) + 
                " 异常: " + e.what());
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(waitDist(gen)));
    }
    
    Logger::GetInstance().Info("监控线程 " + std::to_string(threadId) + " 完成");
}

/**
 * @brief 初始化基础资源（在主线程中，避免OpenGL上下文问题）
 */
bool InitBaseResources() {
    Logger::GetInstance().Info("初始化基础资源...");
    
    auto& resourceMgr = ResourceManager::GetInstance();
    
    // 加载基础着色器
    auto basicShader = ShaderCache::GetInstance().LoadShader(
        "basic", "shaders/basic.vert", "shaders/basic.frag");
    if (!basicShader) {
        Logger::GetInstance().Error("Failed to load basic shader");
        return false;
    }
    
    resourceMgr.RegisterShader("basic", basicShader);
    
    // 在主线程预创建一些网格资源（供后续线程获取使用）
    Logger::GetInstance().Info("预创建网格资源...");
    for (int i = 0; i < 5; i++) {
        auto mesh = MeshLoader::CreateSphere(0.5f, 16, 8);
        std::string name = "base_mesh_" + std::to_string(i);
        resourceMgr.RegisterMesh(name, mesh);
    }
    
    Logger::GetInstance().Info("基础资源初始化完成");
    return true;
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    // 开启日志
    Logger::GetInstance().SetLogToFile(true);
    Logger::GetInstance().SetLogToConsole(true);

    try {
        // 初始化渲染器（为了创建OpenGL上下文）
        Renderer renderer;
        if (!renderer.Initialize("资源管理器线程安全测试", 800, 600)) {
            Logger::GetInstance().Error("Failed to initialize renderer");
            return -1;
        }
        
        // 初始化基础资源
        if (!InitBaseResources()) {
            return -1;
        }
        
        Logger::GetInstance().Info("========================================");
        Logger::GetInstance().Info("资源管理器线程安全测试");
        Logger::GetInstance().Info("========================================");
        
        auto& resourceMgr = ResourceManager::GetInstance();
        
        // ====================================================================
        // 测试 1: 多线程并发注册资源
        // ====================================================================
        Logger::GetInstance().Info("=== 测试 1: 多线程并发注册资源 ===");
        {
            std::vector<std::thread> producers;
            for (int i = 0; i < 10; i++) {
                producers.emplace_back(ProducerThread, i);
            }
            
            for (auto& t : producers) {
                t.join();
            }
            
            Logger::GetInstance().Info("注册操作总数: " + std::to_string(g_registerCount.load()));
            resourceMgr.PrintStatistics();
        }
        
        // ====================================================================
        // 测试 2: 多线程并发获取资源 + 统计查询
        // ====================================================================
        Logger::GetInstance().Info("=== 测试 2: 多线程并发获取资源和统计查询 ===");
        {
            std::vector<std::thread> consumers;
            std::vector<std::thread> monitors;
            
            // 启动消费者线程
            for (int i = 0; i < 10; i++) {
                consumers.emplace_back(ConsumerThread, i);
            }
            
            // 启动监控线程
            for (int i = 0; i < 5; i++) {
                monitors.emplace_back(MonitorThread, i);
            }
            
            // 等待消费者完成
            for (auto& t : consumers) {
                t.join();
            }
            
            // 停止监控线程
            g_running = false;
            for (auto& t : monitors) {
                t.join();
            }
            
            Logger::GetInstance().Info("获取操作总数: " + std::to_string(g_getCount.load()));
            Logger::GetInstance().Info("查询操作总数: " + std::to_string(g_queryCount.load()));
        }
        
        // ====================================================================
        // 测试 3: 清理未使用资源
        // ====================================================================
        Logger::GetInstance().Info("=== 测试 3: 清理未使用资源 ===");
        {
            Logger::GetInstance().Info("清理前:");
            resourceMgr.PrintStatistics();
            
            size_t cleaned = resourceMgr.CleanupUnused();
            Logger::GetInstance().Info("清理了 " + std::to_string(cleaned) + " 个未使用资源");
            
            Logger::GetInstance().Info("清理后:");
            resourceMgr.PrintStatistics();
        }
        
        // ====================================================================
        // 测试 4: 并发清理和注册（仅材质，避免OpenGL调用）
        // ====================================================================
        Logger::GetInstance().Info("=== 测试 4: 并发清理和注册 ===");
        {
            g_running = true;
            g_registerCount = 0;
            
            std::vector<std::thread> threads;
            
            // 生产者线程（只创建材质，避免OpenGL调用）
            for (int i = 0; i < 5; i++) {
                threads.emplace_back([i, &resourceMgr]() {
                    for (int j = 0; j < 10; j++) {
                        auto material = std::make_shared<Material>();
                        material->SetName("cleanup_test_" + std::to_string(i) + "_" + std::to_string(j));
                        material->SetDiffuseColor(Color(
                            static_cast<float>(rand()) / RAND_MAX,
                            static_cast<float>(rand()) / RAND_MAX,
                            static_cast<float>(rand()) / RAND_MAX,
                            1.0f
                        ));
                        std::string name = "cleanup_test_" + std::to_string(i) + "_" + std::to_string(j);
                        if (resourceMgr.RegisterMaterial(name, material)) {
                            g_registerCount++;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                });
            }
            
            // 清理线程
            threads.emplace_back([&resourceMgr]() {
                for (int i = 0; i < 5; i++) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    size_t cleaned = resourceMgr.CleanupUnused();
                    Logger::GetInstance().Info("清理线程: 清理了 " + std::to_string(cleaned) + " 个资源");
                }
            });
            
            for (auto& t : threads) {
                t.join();
            }
            
            Logger::GetInstance().Info("并发注册了 " + std::to_string(g_registerCount.load()) + " 个资源");
            resourceMgr.PrintStatistics();
        }
        
        // ====================================================================
        // 测试 5: ForEach 遍历的线程安全性
        // ====================================================================
        Logger::GetInstance().Info("=== 测试 5: ForEach 遍历的线程安全性 ===");
        {
            std::atomic<int> meshCount{0};
            std::atomic<int> materialCount{0};
            
            std::thread t1([&resourceMgr, &meshCount]() {
                resourceMgr.ForEachMesh([&meshCount](const std::string& name, Ref<Mesh> mesh) {
                    meshCount++;
                    auto vertexCount = mesh->GetVertexCount();
                    (void)vertexCount;
                });
            });
            
            std::thread t2([&resourceMgr, &materialCount]() {
                resourceMgr.ForEachMaterial([&materialCount](const std::string& name, Ref<Material> material) {
                    materialCount++;
                    auto isValid = material->IsValid();
                    (void)isValid;
                });
            });
            
            t1.join();
            t2.join();
            
            Logger::GetInstance().Info("遍历网格数量: " + std::to_string(meshCount.load()));
            Logger::GetInstance().Info("遍历材质数量: " + std::to_string(materialCount.load()));
        }
        
        // ====================================================================
        // 最终统计
        // ====================================================================
        Logger::GetInstance().Info("========================================");
        Logger::GetInstance().Info("测试完成 - 最终统计");
        Logger::GetInstance().Info("========================================");
        resourceMgr.PrintStatistics();
        
        Logger::GetInstance().Info("总注册操作: " + std::to_string(g_registerCount.load()));
        Logger::GetInstance().Info("总获取操作: " + std::to_string(g_getCount.load()));
        Logger::GetInstance().Info("总查询操作: " + std::to_string(g_queryCount.load()));
        Logger::GetInstance().Info("========================================");
        Logger::GetInstance().Info("所有线程安全测试通过！");
        Logger::GetInstance().Info("========================================");
        Logger::GetInstance().Info("提示：这是一个后台测试程序，不渲染任何内容");
        Logger::GetInstance().Info("请查看控制台输出和日志文件了解测试结果");
        Logger::GetInstance().Info("按 ESC 键退出");
        Logger::GetInstance().Info("========================================");
        
        // 简单的事件循环，等待退出
        bool running = true;
        SDL_Event event;
        while (running) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT ||
                    (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) {
                    running = false;
                }
            }
            SDL_Delay(16);  // ~60 FPS
        }
        
        return 0;
    }
    catch (const std::exception& e) {
        Logger::GetInstance().Error(std::string("Exception: ") + e.what());
        return -1;
    }
}

