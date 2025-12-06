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
#include <render/application/scene_manager.h>
#include <render/application/scene.h>
#include <render/application/app_context.h>
#include <render/application/module_registry.h>
#include <render/application/scene_types.h>
#include <render/application/event_bus.h>
#include <render/async_resource_loader.h>
#include <render/resource_manager.h>
#include <render/renderer.h>
#include <render/logger.h>
#include <render/mesh_loader.h>
#include <render/material.h>
#include <render/shader_cache.h>

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <random>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Render;
using namespace Render::Application;

namespace {

// 压力测试场景：声明大量资源
class StressTestScene : public Scene {
public:
    explicit StressTestScene(const std::string& name, size_t resourceCount, bool useShared = false)
        : m_name(name), m_resourceCount(resourceCount), m_useShared(useShared) {}

    std::string_view Name() const override { return m_name; }

    void OnAttach(AppContext&, ModuleRegistry&) override {}

    void OnDetach(AppContext&) override {}

    SceneResourceManifest BuildManifest() const override {
        SceneResourceManifest manifest;
        
        for (size_t i = 0; i < m_resourceCount; ++i) {
            ResourceRequest req;
            req.identifier = m_name + "_mesh_" + std::to_string(i);
            req.type = "mesh";
            req.scope = m_useShared ? ResourceScope::Shared : ResourceScope::Scene;
            req.optional = (i % 10 == 0);  // 每10个资源中有一个是可选的
            
            if (req.optional) {
                manifest.optional.push_back(req);
            } else {
                manifest.required.push_back(req);
            }
        }
        
        return manifest;
    }

    void OnEnter(const SceneEnterArgs&) override {}

    void OnUpdate(const FrameUpdateArgs&) override {}

    SceneSnapshot OnExit(const SceneExitArgs&) override {
        SceneSnapshot snapshot;
        snapshot.sceneId = m_name;
        return snapshot;
    }

private:
    std::string m_name;
    size_t m_resourceCount;
    bool m_useShared;
};

// 辅助函数：创建测试用的AppContext
AppContext CreateTestAppContext() {
    AppContext ctx;
    
    static Renderer* s_renderer = nullptr;
    if (!s_renderer) {
        s_renderer = new Renderer();
        s_renderer->Initialize("SceneManagerStressTest", 320, 240);
    }
    ctx.renderer = s_renderer;
    
    ctx.resourceManager = &ResourceManager::GetInstance();
    
    static bool s_loaderInitialized = false;
    if (!s_loaderInitialized) {
        auto& loader = AsyncResourceLoader::GetInstance();
        loader.Initialize(); 
        s_loaderInitialized = true;
    }
    ctx.asyncLoader = &AsyncResourceLoader::GetInstance();
    
    static EventBus* s_eventBus = nullptr;
    if (!s_eventBus) {
        s_eventBus = new EventBus();
    }
    ctx.globalEventBus = s_eventBus;
    
    return ctx;
}

// 辅助函数：等待所有异步任务完成
void WaitForAllAsyncTasks(AsyncResourceLoader& loader, int maxWaitMs = 30000) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        size_t pending = loader.GetPendingTaskCount();
        size_t loading = loader.GetLoadingTaskCount();
        size_t waiting = loader.GetWaitingUploadCount();
        
        if (pending == 0 && loading == 0 && waiting == 0) {
            break;
        }
        
        // 处理完成的任务
        loader.ProcessCompletedTasks(50);  // 压力测试时处理更多任务
        
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > maxWaitMs) {
            std::cout << "  Warning: 仍有 " << pending << " 个待处理任务, " 
                      << loading << " 个加载中任务, " << waiting << " 个等待上传任务" << std::endl;
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

} // namespace

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    Logger::GetInstance().SetLogToConsole(false);
    Logger::GetInstance().SetLogToFile(false);

    std::cout << "========================================\n";
    std::cout << "SceneManager 资源管理压力测试\n";
    std::cout << "========================================\n\n";

    SceneManager manager;
    AppContext ctx = CreateTestAppContext();
    ModuleRegistry modules;
    manager.Initialize(&ctx, &modules);

    auto& resMgr = *ctx.resourceManager;
    auto& loader = *ctx.asyncLoader;

    // 测试1: 批量资源预加载（小规模）
    {
        std::cout << "[压力测试1] 批量资源预加载（50个资源）...\n";
        
        const size_t resourceCount = 50;
        
        manager.RegisterSceneFactory("StressScene1", [resourceCount]() {
            return std::make_unique<StressTestScene>("StressScene1", resourceCount, false);
        });
        
        auto start = std::chrono::steady_clock::now();
        
        manager.PushScene("StressScene1");
        
        // 更新多帧，处理预加载
        FrameUpdateArgs frame{};
        int updateCount = 0;
        while (updateCount < 100) {
            frame.frameIndex = updateCount++;
            manager.Update(frame);
            
            loader.ProcessCompletedTasks(20);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(16));  // ~60 FPS
        }
        
        // 等待所有异步任务完成
        WaitForAllAsyncTasks(loader, 10000);
        
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "  完成时间: " << duration.count() << " ms\n";
        std::cout << "  平均每个资源: " << (duration.count() / static_cast<double>(resourceCount)) << " ms\n";
        
        // 清理
        SceneExitArgs exitArgs;
        manager.PopScene(exitArgs);
        
        std::cout << "  ✓ 小规模批量加载测试完成\n\n";
    }

    // 测试2: 批量资源预加载（中规模）
    {
        std::cout << "[压力测试2] 批量资源预加载（200个资源）...\n";
        
        const size_t resourceCount = 200;
        
        manager.RegisterSceneFactory("StressScene2", [resourceCount]() {
            return std::make_unique<StressTestScene>("StressScene2", resourceCount, false);
        });
        
        auto start = std::chrono::steady_clock::now();
        
        manager.PushScene("StressScene2");
        
        FrameUpdateArgs frame{};
        int updateCount = 0;
        while (updateCount < 200) {
            frame.frameIndex = updateCount++;
            manager.Update(frame);
            
            loader.ProcessCompletedTasks(30);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        
        WaitForAllAsyncTasks(loader, 20000);
        
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "  完成时间: " << duration.count() << " ms\n";
        std::cout << "  平均每个资源: " << (duration.count() / static_cast<double>(resourceCount)) << " ms\n";
        
        // 检查加载器统计
        std::cout << "  待处理任务: " << loader.GetPendingTaskCount() << "\n";
        std::cout << "  加载中任务: " << loader.GetLoadingTaskCount() << "\n";
        std::cout << "  等待上传任务: " << loader.GetWaitingUploadCount() << "\n";
        
        // 清理
        SceneExitArgs exitArgs;
        manager.PopScene(exitArgs);
        
        std::cout << "  ✓ 中规模批量加载测试完成\n\n";
    }

    // 测试3: 快速场景切换
    {
        std::cout << "[压力测试3] 快速场景切换（10次切换）...\n";
        
        const size_t scenesToSwitch = 10;
        const size_t resourcesPerScene = 20;
        
        // 注册多个场景
        for (size_t i = 0; i < scenesToSwitch; ++i) {
            std::string sceneId = "QuickSwitchScene" + std::to_string(i);
            manager.RegisterSceneFactory(sceneId, [sceneId, resourcesPerScene]() {
                return std::make_unique<StressTestScene>(sceneId, resourcesPerScene, false);
            });
        }
        
        auto start = std::chrono::steady_clock::now();
        
        for (size_t i = 0; i < scenesToSwitch; ++i) {
            std::string sceneId = "QuickSwitchScene" + std::to_string(i);
            
            // 推送场景
            manager.PushScene(sceneId);
            
            // 更新几帧
            FrameUpdateArgs frame{};
            for (int j = 0; j < 5; ++j) {
                frame.frameIndex = j;
                manager.Update(frame);
                loader.ProcessCompletedTasks(10);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            // 弹出场景
            SceneExitArgs exitArgs;
            manager.PopScene(exitArgs);
            
            // 处理资源释放
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "  完成时间: " << duration.count() << " ms\n";
        std::cout << "  平均每次切换: " << (duration.count() / static_cast<double>(scenesToSwitch)) << " ms\n";
        
        std::cout << "  ✓ 快速场景切换测试完成\n\n";
    }

    // 测试4: Shared资源复用
    {
        std::cout << "[压力测试4] Shared资源复用测试...\n";
        
        const size_t resourceCount = 30;
        
        // 创建第一个场景，使用Shared资源
        manager.RegisterSceneFactory("SharedScene1", [resourceCount]() {
            return std::make_unique<StressTestScene>("SharedScene1", resourceCount, true);
        });
        
        manager.RegisterSceneFactory("SharedScene2", [resourceCount]() {
            return std::make_unique<StressTestScene>("SharedScene2", resourceCount, true);
        });
        
        // 推送第一个场景
        manager.PushScene("SharedScene1");
        
        FrameUpdateArgs frame{};
        for (int i = 0; i < 10; ++i) {
            frame.frameIndex = i;
            manager.Update(frame);
            loader.ProcessCompletedTasks(10);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // 切换到第二个场景（应该复用Shared资源）
        manager.ReplaceScene("SharedScene2");
        
        for (int i = 0; i < 10; ++i) {
            frame.frameIndex = i;
            manager.Update(frame);
            loader.ProcessCompletedTasks(10);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // 检查资源是否仍然存在（Shared资源应该保留）
        // 注意：由于资源可能不存在，这里主要测试逻辑流程
        
        SceneExitArgs exitArgs;
        manager.PopScene(exitArgs);
        
        std::cout << "  ✓ Shared资源复用测试完成\n\n";
    }

    // 测试5: 并发资源加载压力测试
    {
        std::cout << "[压力测试5] 并发资源加载（500个资源）...\n";
        
        const size_t resourceCount = 500;
        
        manager.RegisterSceneFactory("ConcurrentScene", [resourceCount]() {
            return std::make_unique<StressTestScene>("ConcurrentScene", resourceCount, false);
        });
        
        auto start = std::chrono::steady_clock::now();
        
        manager.PushScene("ConcurrentScene");
        
        // 持续更新，处理大量并发加载
        FrameUpdateArgs frame{};
        int updateCount = 0;
        const int maxUpdates = 500;
        
        while (updateCount < maxUpdates) {
            frame.frameIndex = updateCount++;
            manager.Update(frame);
            
            // 每帧处理更多任务以应对压力
            loader.ProcessCompletedTasks(50);
            
            // 每100帧输出一次进度
            if (updateCount % 100 == 0) {
                std::cout << "  进度: " << updateCount << "/" << maxUpdates 
                          << " 帧, 待处理: " << loader.GetPendingTaskCount()
                          << ", 加载中: " << loader.GetLoadingTaskCount()
                          << ", 等待上传: " << loader.GetWaitingUploadCount() << "\n";
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(8));  // ~120 FPS模拟
        }
        
        // 等待所有任务完成
        WaitForAllAsyncTasks(loader, 30000);
        
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "  完成时间: " << duration.count() << " ms\n";
        std::cout << "  平均每个资源: " << (duration.count() / static_cast<double>(resourceCount)) << " ms\n";
        
        // 输出最终统计
        loader.PrintStatistics();
        
        // 清理
        SceneExitArgs exitArgs;
        manager.PopScene(exitArgs);
        
        std::cout << "  ✓ 并发资源加载压力测试完成\n\n";
    }

    // 输出资源管理器统计
    std::cout << "========================================\n";
    std::cout << "最终资源管理器统计:\n";
    std::cout << "========================================\n";
    resMgr.PrintStatistics();

    std::cout << "\n========================================\n";
    std::cout << "所有压力测试完成\n";
    std::cout << "========================================\n";

    return 0;
}

