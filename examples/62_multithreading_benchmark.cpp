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
 * @file 63_multithreading_benchmark.cpp
 * @brief 多线程优化性能基准测试
 * 
 * 测试TaskScheduler和并行化渲染队列的性能提升
 */

#include "render/renderer.h"
#include "render/task_scheduler.h"
#include "render/mesh_loader.h"
#include "render/material.h"
#include "render/shader_cache.h"
#include "render/logger.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/ecs/systems.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>

using namespace Render;
using namespace Render::ECS;

struct BenchmarkStats {
    size_t frames = 0;
    float totalTimeMs = 0.0f;
    uint64_t totalDrawCalls = 0;
    uint64_t totalBatches = 0;
    uint64_t totalWorkerProcessed = 0;
    float totalWorkerWaitMs = 0.0f;
    
    void Accumulate(const RenderStats& stats, float frameTimeMs) {
        frames++;
        totalTimeMs += frameTimeMs;
        totalDrawCalls += stats.drawCalls;
        totalBatches += stats.batchCount;
        totalWorkerProcessed += stats.workerProcessed;
        totalWorkerWaitMs += stats.workerWaitTimeMs;
    }
    
    void PrintSummary(int objectCount) const {
        if (frames == 0) return;
        
        float avgFPS = (totalTimeMs > 0) ? (frames * 1000.0f / totalTimeMs) : 0.0f;
        float avgFrameTime = totalTimeMs / frames;
        float avgDrawCalls = static_cast<float>(totalDrawCalls) / frames;
        float avgBatches = static_cast<float>(totalBatches) / frames;
        float avgWorkerProcessed = static_cast<float>(totalWorkerProcessed) / frames;
        float avgWorkerWait = totalWorkerWaitMs / frames;
        
        std::cout << "\n========== 结果 (" << objectCount << " 对象) ==========" << std::endl;
        std::cout << "  ⏱️  平均FPS: " << avgFPS << " FPS" << std::endl;
        std::cout << "  ⏱️  平均帧时间: " << avgFrameTime << " ms" << std::endl;
        std::cout << "  🎨 平均DrawCalls: " << avgDrawCalls << std::endl;
        std::cout << "  📦 平均批次数: " << avgBatches << std::endl;
        std::cout << "  🔧 Worker处理项数: " << avgWorkerProcessed << std::endl;
        std::cout << "  ⏳ Worker等待时间: " << avgWorkerWait << " ms" << std::endl;
        
        // 计算性能指标
        if (avgWorkerWait > 0) {
            float waitPercentage = (avgWorkerWait / avgFrameTime) * 100.0f;
            std::cout << "  📊 等待时间占比: " << waitPercentage << "%" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    Logger::GetInstance().SetLogLevel(LogLevel::Info);
    
    std::cout << "========================================" << std::endl;
    std::cout << "多线程优化性能基准测试" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 初始化TaskScheduler
    std::cout << "初始化TaskScheduler..." << std::endl;
    TaskScheduler::GetInstance().Initialize();
    auto taskStats = TaskScheduler::GetInstance().GetStats();
    std::cout << "工作线程数: " << taskStats.workerThreads << std::endl;
    
    // 创建渲染器
    Renderer* renderer = Renderer::Create();
    if (!renderer->Initialize("Multithreading Benchmark", 1280, 720)) {
        std::cerr << "渲染器初始化失败" << std::endl;
        return 1;
    }
    
    renderer->SetVSync(false);
    renderer->SetClearColor(Color(0.05f, 0.05f, 0.08f, 1.0f));
    renderer->SetBatchingMode(BatchingMode::GpuInstancing);
    
    // ✅ 禁用LOD实例化渲染，测试传统批处理流程
    renderer->SetLODInstancingEnabled(false);
    std::cout << "LOD实例化渲染: 已禁用（测试传统批处理）" << std::endl;
    
    // 加载着色器
    auto& shaderCache = ShaderCache::GetInstance();
    auto shader = shaderCache.LoadShader("basic", "shaders/basic.vert", "shaders/basic.frag");
    if (!shader) {
        std::cerr << "加载着色器失败" << std::endl;
        return 1;
    }
    
    auto material = std::make_shared<Material>();
    material->SetShader(shader);
    material->SetDiffuseColor(Color(0.4f, 0.7f, 1.0f, 1.0f));
    
    auto cubeMesh = MeshLoader::CreateCube(1.0f);
    if (!cubeMesh) {
        std::cerr << "创建网格失败" << std::endl;
        return 1;
    }
    
    // 创建ECS World
    auto world = std::make_shared<World>();
    world->Initialize();
    
    world->RegisterComponent<TransformComponent>();
    world->RegisterComponent<MeshRenderComponent>();
    world->RegisterComponent<CameraComponent>();
    world->RegisterComponent<ActiveComponent>();
    world->RegisterComponent<NameComponent>();
    
    world->RegisterSystem<TransformSystem>();
    world->RegisterSystem<CameraSystem>();
    world->RegisterSystem<UniformSystem>(renderer);
    world->RegisterSystem<MeshRenderSystem>(renderer);
    
    world->PostInitialize();
    
    // 创建相机
    EntityID cameraEntity = world->CreateEntity({.name = "Camera", .active = true});
    TransformComponent cameraTransform;
    cameraTransform.SetPosition(Vector3(0.0f, 0.0f, 30.0f));
    world->AddComponent(cameraEntity, cameraTransform);
    
    CameraComponent cameraComp;
    cameraComp.camera = std::make_shared<Camera>();
    cameraComp.camera->SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 200.0f);
    cameraComp.active = true;
    world->AddComponent(cameraEntity, cameraComp);
    
    // 测试不同规模的场景
    std::vector<int> testSizes = {100, 500, 1000, 2000};
    
    for (int objectCount : testSizes) {
        std::cout << "\n========================================" << std::endl;
        std::cout << "测试场景: " << objectCount << " 对象" << std::endl;
        std::cout << "========================================" << std::endl;
        
        // 创建网格对象
        std::vector<EntityID> entities;
        const int gridSize = static_cast<int>(std::sqrt(objectCount));
        const float spacing = 2.0f;
        const float offset = (gridSize - 1) * spacing * 0.5f;
        
        for (int y = 0; y < gridSize; ++y) {
            for (int x = 0; x < gridSize; ++x) {
                EntityID entity = world->CreateEntity({.name = "Cube", .active = true});
                
                TransformComponent transform;
                transform.SetPosition(Vector3(
                    x * spacing - offset,
                    y * spacing - offset,
                    0.0f
                ));
                transform.SetScale(Vector3(0.9f, 0.9f, 0.9f));
                world->AddComponent(entity, transform);
                
                MeshRenderComponent meshComp;
                meshComp.mesh = cubeMesh;
                meshComp.material = material;
                meshComp.visible = true;
                meshComp.layerID = 300;
                meshComp.resourcesLoaded = true;
                world->AddComponent(entity, meshComp);
                
                entities.push_back(entity);
            }
        }
        
        std::cout << "创建了 " << entities.size() << " 个实体" << std::endl;
        
        // 运行基准测试
        const int warmupFrames = 30;
        const int testFrames = 120;
        BenchmarkStats stats;
        
        std::cout << "预热 " << warmupFrames << " 帧..." << std::endl;
        for (int frame = 0; frame < warmupFrames; ++frame) {
            renderer->BeginFrame();
            renderer->Clear();
            world->Update(0.016f);
            renderer->FlushRenderQueue();
            renderer->EndFrame();
            renderer->Present();
        }
        
        std::cout << "测试 " << testFrames << " 帧..." << std::endl;
        
        for (int frame = 0; frame < testFrames; ++frame) {
            auto frameStart = std::chrono::high_resolution_clock::now();
            
            renderer->BeginFrame();
            renderer->Clear();
            world->Update(0.016f);
            renderer->FlushRenderQueue();
            renderer->EndFrame();
            renderer->Present();
            
            auto frameEnd = std::chrono::high_resolution_clock::now();
            float frameTimeMs = std::chrono::duration<float, std::milli>(frameEnd - frameStart).count();
            stats.Accumulate(renderer->GetStats(), frameTimeMs);
        }
        
        // 输出结果
        stats.PrintSummary(objectCount);
        
        // TaskScheduler统计
        auto taskStats = TaskScheduler::GetInstance().GetStats();
        std::cout << "\n🔀 TaskScheduler统计:" << std::endl;
        std::cout << "  ✅ 总任务: " << taskStats.totalTasks << std::endl;
        std::cout << "  ✅ 已完成: " << taskStats.completedTasks << std::endl;
        std::cout << "  ⏱️  平均任务时间: " << taskStats.avgTaskTimeMs << " ms" << std::endl;
        std::cout << "  📊 线程利用率: " << (taskStats.utilization * 100.0f) << "%" << std::endl;
        
        if (taskStats.totalTasks > 0) {
            std::cout << "  🎯 并行化已激活！" << std::endl;
        } else {
            std::cout << "  ⚠️  未触发并行化（项目数量可能低于阈值）" << std::endl;
        }
        
        TaskScheduler::GetInstance().ResetStats();
        
        // 清理实体
        for (auto entity : entities) {
            world->DestroyEntity(entity);
        }
    }
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "基准测试完成" << std::endl;
    std::cout << "========================================" << std::endl;
    
    world->Shutdown();
    Renderer::Destroy(renderer);
    TaskScheduler::GetInstance().Shutdown();
    
    return 0;
}

