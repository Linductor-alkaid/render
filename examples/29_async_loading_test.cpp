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
 * @file 29_async_loading_test.cpp
 * @brief 异步资源加载测试
 * 
 * 本示例演示：
 * 1. AsyncResourceLoader的基本使用
 * 2. 异步加载网格和纹理
 * 3. 主线程处理完成的任务
 * 4. 进度回调和统计信息
 * 5. 大型模型（PMX）的异步加载
 * 
 * 特点：
 * - 后台线程加载数据（不阻塞主线程）
 * - 主线程渲染加载进度
 * - GPU上传在主线程执行（OpenGL要求）
 * - 支持加载进度回调
 * 
 * 控制：
 * - SPACE：开始异步加载
 * - R：重新加载
 * - S：打印统计信息
 * - ESC：退出
 */

#include <render/renderer.h>
#include <render/async_resource_loader.h>
#include <render/resource_manager.h>
#include <render/shader_cache.h>
#include <render/mesh_loader.h>
#include <render/logger.h>
#include <render/types.h>
#include <SDL3/SDL.h>
#include <iostream>
#include <vector>
#include <memory>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Render;

// 全局状态
bool g_loading = false;
size_t g_totalTasks = 0;
size_t g_completedTasks = 0;
std::vector<std::shared_ptr<MeshLoadTask>> g_meshTasks;
std::vector<std::string> g_loadedMeshNames;

/**
 * @brief 开始异步加载测试
 */
void StartAsyncLoading(Renderer& renderer) {
    auto& asyncLoader = AsyncResourceLoader::GetInstance();
    auto& resMgr = ResourceManager::GetInstance();
    
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("开始异步加载测试");
    Logger::GetInstance().Info("========================================");
    
    g_loading = true;
    g_totalTasks = 0;
    g_completedTasks = 0;
    g_meshTasks.clear();
    g_loadedMeshNames.clear();
    
    // 清理旧资源
    resMgr.Clear();
    
    // ========================================================================
    // 测试1: 加载基本几何体（快速测试）
    // ========================================================================
    Logger::GetInstance().Info("\n测试1: 加载基本几何体（用于对比）");
    
    // 创建一些基本网格（同步，作为对比）
    auto cube = MeshLoader::CreateCube();
    resMgr.RegisterMesh("sync_cube", cube);
    
    // ========================================================================
    // 测试2: 异步加载模型文件
    // ========================================================================
    Logger::GetInstance().Info("\n测试2: 异步加载模型文件");
    
    std::vector<std::string> modelPaths = {
        "models/miku/v4c5.0short.pmx",
        "models/miku/v4c5.0.pmx",
        "models/cube.obj",
        "../models/miku/v4c5.0short.pmx",
        "../models/miku/v4c5.0.pmx",
        "../models/cube.obj"
    };
    
    // 尝试异步加载模型
    for (const auto& path : modelPaths) {
        Logger::GetInstance().Info("尝试异步加载: " + path);
        
        auto task = asyncLoader.LoadMeshAsync(
            path,
            "async_model",
            [](const MeshLoadResult& result) {
                if (result.IsSuccess()) {
                    Logger::GetInstance().Info("✅ 异步加载成功: " + result.name);
                    
                    // 注册到资源管理器
                    auto& resMgr = ResourceManager::GetInstance();
                    std::string meshName = "async_mesh_" + std::to_string(g_loadedMeshNames.size());
                    resMgr.RegisterMesh(meshName, result.resource);
                    g_loadedMeshNames.push_back(meshName);
                    
                    g_completedTasks++;
                } else {
                    Logger::GetInstance().Error("❌ 异步加载失败: " + result.name + 
                                                " - " + result.errorMessage);
                }
            },
            10.0f  // 高优先级
        );
        
        if (task) {
            g_meshTasks.push_back(task);
            g_totalTasks++;
            break;  // 找到第一个可用的就停止
        }
    }
    
    // ========================================================================
    // 测试3: 批量异步加载（多个小模型）
    // ========================================================================
    Logger::GetInstance().Info("\n测试3: 批量创建网格（测试并发）");
    
    // 创建多个几何体用于异步上传测试
    for (int i = 0; i < 5; i++) {
        auto task = asyncLoader.LoadMeshAsync(
            "models/cube.obj",  // 如果不存在会失败，没关系
            "batch_mesh_" + std::to_string(i),
            [](const MeshLoadResult& result) {
                if (result.IsSuccess()) {
                    auto& resMgr = ResourceManager::GetInstance();
                    resMgr.RegisterMesh(result.name, result.resource);
                    g_completedTasks++;
                }
            },
            (float)i  // 不同优先级
        );
        
        if (task) {
            g_meshTasks.push_back(task);
            g_totalTasks++;
        }
    }
    
    Logger::GetInstance().Info("\n========================================");
    Logger::GetInstance().Info("异步加载任务已提交");
    Logger::GetInstance().Info("总任务数: " + std::to_string(g_totalTasks));
    Logger::GetInstance().Info("========================================");
}

/**
 * @brief 渲染加载进度
 */
void RenderLoadingProgress(Renderer& renderer, float progress) {
    // 清空屏幕（深蓝色）
    renderer.SetClearColor(0.1f, 0.15f, 0.3f, 1.0f);
    renderer.Clear();
    
    // 这里可以渲染进度条UI
    // 简单起见，我们只在控制台输出和标题栏显示
    static int lastPercent = -1;
    int percent = (int)(progress * 100.0f);
    if (percent != lastPercent) {
        Logger::GetInstance().Info("加载进度: " + std::to_string(percent) + "%");
        lastPercent = percent;
    }
}

/**
 * @brief 渲染已加载的场景
 */
void RenderScene(Renderer& renderer, Ref<Shader> shader, Matrix4& view, Matrix4& projection, float& rotation) {
    auto& resMgr = ResourceManager::GetInstance();
    
    // 清空屏幕
    renderer.SetClearColor(0.15f, 0.15f, 0.2f, 1.0f);
    renderer.Clear();
    
    // 启用深度测试
    auto renderState = renderer.GetRenderState();
    if (renderState) {
        renderState->SetDepthTest(true);
        renderState->SetDepthFunc(DepthFunc::Less);
    }
    
    if (!shader || !shader->IsValid()) {
        return;
    }
    
    shader->Use();
    auto* uniformMgr = shader->GetUniformManager();
    if (!uniformMgr) {
        shader->Unuse();
        return;
    }
    
    // 设置相机矩阵
    uniformMgr->SetMatrix4("uView", view);
    uniformMgr->SetMatrix4("uProjection", projection);
    
    // 设置光照
    Vector3 lightPos(5.0f, 10.0f, 5.0f);
    Vector3 viewPos(0.0f, 2.0f, 5.0f);
    uniformMgr->SetVector3("uLightPos", lightPos);
    uniformMgr->SetVector3("uViewPos", viewPos);
    uniformMgr->SetVector3("uLightColor", Vector3(1.0f, 1.0f, 1.0f));
    
    // 旋转角度
    rotation += 0.5f;
    if (rotation > 360.0f) rotation -= 360.0f;
    float angleRad = rotation * 3.14159f / 180.0f;
    
    // 渲染同步加载的立方体（作为参考）
    auto cube = resMgr.GetMesh("sync_cube");
    if (cube && cube->IsUploaded()) {
        Matrix4 cubeModel = Matrix4::Identity();
        cubeModel(0, 0) = std::cos(angleRad);
        cubeModel(0, 2) = std::sin(angleRad);
        cubeModel(2, 0) = -std::sin(angleRad);
        cubeModel(2, 2) = std::cos(angleRad);
        cubeModel(0, 3) = -3.0f;  // 左边
        cubeModel(1, 3) = 0.5f;   // 稍微抬高
        
        uniformMgr->SetMatrix4("uModel", cubeModel);
        uniformMgr->SetColor("uDiffuseColor", Color(0.3f, 0.7f, 0.3f, 1.0f));
        cube->Draw();
    }
    
    // 渲染异步加载的网格
    float xOffset = 0.0f;
    for (const auto& meshName : g_loadedMeshNames) {
        auto mesh = resMgr.GetMesh(meshName);
        if (mesh && mesh->IsUploaded()) {
            Matrix4 model = Matrix4::Identity();
            
            // 旋转
            model(0, 0) = std::cos(angleRad);
            model(0, 2) = std::sin(angleRad);
            model(2, 0) = -std::sin(angleRad);
            model(2, 2) = std::cos(angleRad);
            
            // 位置（如果是大模型则缩小，否则正常）
            if (meshName.find("async_model") != std::string::npos) {
                // PMX模型通常很大，需要缩小
                model(0, 0) *= 0.08f;
                model(1, 1) *= 0.08f;
                model(2, 2) *= 0.08f;
                model(1, 3) = -0.5f;  // 降低位置
            } else {
                model(0, 3) = xOffset;
                model(1, 3) = 0.5f;
                xOffset += 2.5f;
            }
            
            uniformMgr->SetMatrix4("uModel", model);
            uniformMgr->SetColor("uDiffuseColor", Color(0.7f, 0.3f, 0.7f, 1.0f));
            mesh->Draw();
        }
    }
    
    shader->Unuse();
}

/**
 * @brief 主函数
 */
int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    // 初始化日志
    Logger::GetInstance().SetLogToFile(true);
    Logger::GetInstance().SetLogToConsole(true);
    
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("异步资源加载测试");
    Logger::GetInstance().Info("========================================");
    
    try {
        // 初始化渲染器
        Renderer renderer;
        if (!renderer.Initialize("Async Resource Loading Test - 异步资源加载测试", 1280, 720)) {
            Logger::GetInstance().Error("Failed to initialize renderer");
            return -1;
        }
        
        // ✅ 关键：初始化异步加载器
        auto& asyncLoader = AsyncResourceLoader::GetInstance();
        asyncLoader.Initialize(); 
        
        // 加载着色器
        auto& shaderCache = ShaderCache::GetInstance();
        auto shader = shaderCache.LoadShader(
            "phong",
            "shaders/material_phong.vert",
            "shaders/material_phong.frag"
        );
        
        if (!shader || !shader->IsValid()) {
            Logger::GetInstance().Error("无法加载着色器");
            return -1;
        }
        
        // 设置相机矩阵
        float aspect = static_cast<float>(renderer.GetWidth()) / 
                      static_cast<float>(renderer.GetHeight());
        
        // 投影矩阵（透视）
        Matrix4 projection = Matrix4::Identity();
        float fov = 45.0f * 3.14159f / 180.0f;
        float nearPlane = 0.1f;
        float farPlane = 100.0f;
        float f = 1.0f / std::tan(fov / 2.0f);
        projection(0, 0) = f / aspect;
        projection(1, 1) = f;
        projection(2, 2) = (farPlane + nearPlane) / (nearPlane - farPlane);
        projection(2, 3) = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
        projection(3, 2) = -1.0f;
        projection(3, 3) = 0.0f;
        
        // 视图矩阵
        Matrix4 view = Matrix4::Identity();
        view(1, 3) = -2.0f;  // 向上移动
        view(2, 3) = -5.0f;  // 向后移动
        
        float rotation = 0.0f;
        
        // 打印控制说明
        Logger::GetInstance().Info("\n========================================");
        Logger::GetInstance().Info("控制说明");
        Logger::GetInstance().Info("========================================");
        Logger::GetInstance().Info("SPACE - 开始异步加载");
        Logger::GetInstance().Info("R     - 重新加载");
        Logger::GetInstance().Info("S     - 打印统计信息");
        Logger::GetInstance().Info("ESC   - 退出");
        Logger::GetInstance().Info("========================================\n");
        
        // 主循环
        bool running = true;
        uint64_t lastTime = SDL_GetTicks();
        uint64_t lastStatTime = lastTime;
        
        while (running) {
            // 计算 deltaTime
            uint64_t currentTime = SDL_GetTicks();
            float deltaTime = (currentTime - lastTime) / 1000.0f;
            lastTime = currentTime;
            
            // 处理事件
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                }
                else if (event.type == SDL_EVENT_KEY_DOWN) {
                    switch (event.key.key) {
                        case SDLK_ESCAPE:
                            running = false;
                            break;
                            
                        case SDLK_SPACE:
                            if (!g_loading) {
                                StartAsyncLoading(renderer);
                            }
                            break;
                            
                        case SDLK_R:
                            Logger::GetInstance().Info("\n重新加载...");
                            StartAsyncLoading(renderer);
                            break;
                            
                        case SDLK_S:
                            Logger::GetInstance().Info("\n");
                            asyncLoader.PrintStatistics();
                            ResourceManager::GetInstance().PrintStatistics();
                            break;
                    }
                }
            }
            
            // ✅ 关键：在主循环中处理完成的任务（GPU上传）
            if (g_loading) {
                size_t processed = asyncLoader.ProcessCompletedTasks(10);  // 每帧最多10个
                
                if (processed > 0) {
                    Logger::GetInstance().Debug("本帧处理了 " + std::to_string(processed) + " 个任务");
                }
                
                // 检查是否全部完成
                bool allCompleted = true;
                for (const auto& task : g_meshTasks) {
                    if (task->status != LoadStatus::Completed && 
                        task->status != LoadStatus::Failed) {
                        allCompleted = false;
                        break;
                    }
                }
                
                if (allCompleted) {
                    g_loading = false;
                    Logger::GetInstance().Info("\n========================================");
                    Logger::GetInstance().Info("所有异步加载任务完成！");
                    Logger::GetInstance().Info("========================================");
                    asyncLoader.PrintStatistics();
                }
            }
            
            // 每秒打印一次加载状态
            if (g_loading && (currentTime - lastStatTime) > 1000) {
                size_t pending = asyncLoader.GetPendingTaskCount();
                size_t loading = asyncLoader.GetLoadingTaskCount();
                size_t waiting = asyncLoader.GetWaitingUploadCount();
                
                Logger::GetInstance().Info("加载状态 - 待处理: " + std::to_string(pending) + 
                                          ", 加载中: " + std::to_string(loading) + 
                                          ", 等待上传: " + std::to_string(waiting) + 
                                          ", 已完成: " + std::to_string(g_completedTasks) + 
                                          "/" + std::to_string(g_totalTasks));
                
                lastStatTime = currentTime;
            }
            
            // 渲染
            renderer.BeginFrame();
            
            if (g_loading) {
                float progress = g_totalTasks > 0 ? 
                                (float)g_completedTasks / g_totalTasks : 0.0f;
                RenderLoadingProgress(renderer, progress);
            } else {
                RenderScene(renderer, shader, view, projection, rotation);
            }
            
            renderer.EndFrame();
            renderer.Present();
        }
        
        // 清理
        Logger::GetInstance().Info("\n========================================");
        Logger::GetInstance().Info("清理资源");
        Logger::GetInstance().Info("========================================");
        
        // 关闭异步加载器（等待所有任务完成）
        asyncLoader.Shutdown();
        
        // 清理资源管理器
        ResourceManager::GetInstance().Clear();
        
        renderer.Shutdown();
        Logger::GetInstance().Info("\n程序正常退出");
        Logger::GetInstance().Info("日志已保存到: " + Logger::GetInstance().GetCurrentLogFile());
        
        return 0;
    }
    catch (const std::exception& e) {
        Logger::GetInstance().Error(std::string("Exception: ") + e.what());
        return -1;
    }
}

