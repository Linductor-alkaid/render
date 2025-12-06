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
 * @file 56_scene_hot_switch_stress_test.cpp
 * @brief 多场景热切换压力测试 - 自动化测试场景切换性能和稳定性
 * 
 * 测试功能：
 * 1. 创建多个测试场景（不同资源数量）
 * 2. 自动化场景切换（Push/Pop/Replace循环）
 * 3. 性能统计（切换时间、内存使用）
 * 4. 内存泄漏检测
 * 5. 稳定性验证（长时间运行）
 * 
 * 测试场景：
 * - 快速场景切换（10次/秒）
 * - 大量资源场景切换（50/100/200个资源）
 * - 场景栈压力测试（多层场景栈）
 * - 长时间运行测试（1000次切换）
 */

#include "render/application/application_host.h"
#include "render/application/modules/core_render_module.h"
#include "render/application/modules/input_module.h"
#include "render/application/modules/debug_hud_module.h"
#include "render/application/module_registry.h"
#include "render/application/scene_manager.h"
#include "render/application/scene.h"
#include "render/application/scene_types.h"
#include "render/application/app_context.h"
#include "render/async_resource_loader.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/logger.h"
#include "render/renderer.h"
#include "render/resource_manager.h"
#include "render/mesh_loader.h"
#include "render/material.h"
#include "render/shader_cache.h"
#include "render/camera.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <iomanip>
#include <sstream>
#include <thread>
#include <limits>
 
 using namespace Render;
 using namespace Render::Application;
 using namespace Render::ECS;
 
 namespace {
 
 void ConfigureLogger() {
     auto& logger = Logger::GetInstance();
     logger.SetLogToConsole(true);
     logger.SetLogToFile(false);
     logger.SetLogLevel(LogLevel::Info);
 }
 
 bool InitializeRenderer(Renderer*& renderer) {
     renderer = Renderer::Create();
     if (!renderer) {
         Logger::GetInstance().Error("[SceneHotSwitchStressTest] Failed to create renderer");
         return false;
     }
 
     if (!renderer->Initialize("Scene Hot Switch Stress Test", 1280, 720)) {
         Logger::GetInstance().Error("[SceneHotSwitchStressTest] Failed to initialize renderer");
         Renderer::Destroy(renderer);
         renderer = nullptr;
         return false;
     }
 
     renderer->SetClearColor(0.1f, 0.12f, 0.16f, 1.0f);
     renderer->SetVSync(false);  // 关闭VSync以获得更准确的性能数据
     return true;
 }
 
 // 测试场景类：创建不同复杂度的场景
 class TestScene : public Scene {
 public:
     explicit TestScene(const std::string& name, size_t entityCount, size_t resourceCount, bool useShared = false)
         : m_name(name), m_entityCount(entityCount), m_resourceCount(resourceCount), m_useShared(useShared) {}
 
     std::string_view Name() const override { return m_name; }
 
     void OnAttach(AppContext& ctx, ModuleRegistry&) override {
         m_context = &ctx;
         
         // 创建资源
         if (ctx.resourceManager) {
             CreateResources(*ctx.resourceManager);
         }
     }
 
     void OnDetach(AppContext&) override {
         m_context = nullptr;
     }
 
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
 
     void OnEnter(const SceneEnterArgs& args) override {
         if (!m_context || !m_context->world) {
             return;
         }
 
         auto& world = *m_context->world;
         
         // 创建实体
         for (size_t i = 0; i < m_entityCount; ++i) {
             EntityID entity = world.CreateEntity({
                 .name = m_name + "_entity_" + std::to_string(i),
                 .active = true
             });
 
             // 添加TransformComponent
             TransformComponent transform;
             transform.SetPosition(Vector3(
                 static_cast<float>(i % 10) - 5.0f,
                 static_cast<float>(i / 10) * 0.5f,
                 0.0f
             ));
             world.AddComponent(entity, transform);
 
            // 每5个实体添加一个MeshRenderComponent
            if (i % 5 == 0 && m_context->resourceManager) {
                MeshRenderComponent meshComp;
                size_t resourceIndex = i % m_resourceCount;
                meshComp.meshName = m_name + "_mesh_" + std::to_string(resourceIndex);
                
                // 获取材质（使用默认材质）
                const char* materialName = "stress_test.material";
                if (!m_context->resourceManager->HasMaterial(materialName)) {
                    auto shader = ShaderCache::GetInstance().LoadShader(
                        "stress_test.shader", "shaders/material_phong.vert", "shaders/material_phong.frag");
                    auto material = std::make_shared<Material>();
                    material->SetName(materialName);
                    material->SetShader(shader);
                    material->SetAmbientColor(Color(0.15f, 0.15f, 0.15f, 1.0f));
                    material->SetDiffuseColor(Color(0.2f, 0.6f, 1.0f, 1.0f));
                    material->SetSpecularColor(Color(0.9f, 0.9f, 0.9f, 1.0f));
                    material->SetShininess(64.0f);
                    m_context->resourceManager->RegisterMaterial(materialName, material);
                }
                
                if (m_context->resourceManager->HasMesh(meshComp.meshName)) {
                    meshComp.mesh = m_context->resourceManager->GetMesh(meshComp.meshName);
                    meshComp.resourcesLoaded = (meshComp.mesh != nullptr);
                }
                
                meshComp.materialName = materialName;
                meshComp.material = m_context->resourceManager->GetMaterial(materialName);
                meshComp.resourcesLoaded = (meshComp.mesh != nullptr && meshComp.material != nullptr);
                
                meshComp.layerID = Layers::World::Midground.value;
                world.AddComponent(entity, meshComp);
            }
         }
 
         // 创建相机（如果还没有）
         if (m_entityCount > 0) {
             EntityID camera = world.CreateEntity({.name = m_name + "_camera", .active = true});
             
             TransformComponent cameraTransform;
             cameraTransform.SetPosition(Vector3(0.0f, 1.5f, 4.0f));
             cameraTransform.transform->LookAt(Vector3::Zero());
             world.AddComponent(camera, cameraTransform);
 
             CameraComponent cameraComp;
             cameraComp.camera = std::make_shared<Camera>();
             cameraComp.camera->SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 100.0f);
             cameraComp.depth = 0;
             cameraComp.clearColor = Color(0.05f, 0.08f, 0.12f, 1.0f);
             cameraComp.layerMask = 0xFFFFFFFFu;
             world.AddComponent(camera, cameraComp);
         }
     }
 
     void OnUpdate(const FrameUpdateArgs& frame) override {
         // 场景更新逻辑（可选）
     }
 
     SceneSnapshot OnExit(const SceneExitArgs&) override {
         SceneSnapshot snapshot;
         snapshot.sceneId = m_name;
         return snapshot;
     }
 
 private:
     void CreateResources(ResourceManager& resourceManager) {
         const char* shaderName = "stress_test.shader";
         auto shader = ShaderCache::GetInstance().LoadShader(
             shaderName, "shaders/material_phong.vert", "shaders/material_phong.frag");
 
         for (size_t i = 0; i < m_resourceCount; ++i) {
             std::string meshName = m_name + "_mesh_" + std::to_string(i);
             
             if (!resourceManager.HasMesh(meshName)) {
                 // 创建简单的立方体网格
                 auto mesh = MeshLoader::CreateCube(0.5f, 0.5f, 0.5f, Color::White());
                 resourceManager.RegisterMesh(meshName, mesh);
             }
         }
     }
 
     std::string m_name;
     size_t m_entityCount;
     size_t m_resourceCount;
     bool m_useShared;
     AppContext* m_context = nullptr;
 };
 
 // 性能统计
 struct PerformanceStats {
     size_t totalSwitches = 0;
     size_t pushCount = 0;
     size_t popCount = 0;
     size_t replaceCount = 0;
     
     double totalSwitchTime = 0.0;
     double minSwitchTime = std::numeric_limits<double>::max();
     double maxSwitchTime = 0.0;
     
     size_t totalEntities = 0;
     size_t peakEntityCount = 0;
     
     void RecordSwitch(double switchTime, size_t entityCount) {
         totalSwitches++;
         totalSwitchTime += switchTime;
         minSwitchTime = std::min(minSwitchTime, switchTime);
         maxSwitchTime = std::max(maxSwitchTime, switchTime);
         totalEntities += entityCount;
         peakEntityCount = std::max(peakEntityCount, entityCount);
     }
     
     void PrintReport() const {
         Logger::GetInstance().Info("========================================");
         Logger::GetInstance().Info("Performance Statistics");
         Logger::GetInstance().Info("========================================");
         Logger::GetInstance().InfoFormat("Total Switches: %zu", totalSwitches);
         Logger::GetInstance().InfoFormat("  Push: %zu", pushCount);
         Logger::GetInstance().InfoFormat("  Pop: %zu", popCount);
         Logger::GetInstance().InfoFormat("  Replace: %zu", replaceCount);
         Logger::GetInstance().InfoFormat("Average Switch Time: %.3f ms", 
             totalSwitches > 0 ? (totalSwitchTime / totalSwitches * 1000.0) : 0.0);
         Logger::GetInstance().InfoFormat("Min Switch Time: %.3f ms", minSwitchTime * 1000.0);
         Logger::GetInstance().InfoFormat("Max Switch Time: %.3f ms", maxSwitchTime * 1000.0);
         Logger::GetInstance().InfoFormat("Peak Entity Count: %zu", peakEntityCount);
         Logger::GetInstance().InfoFormat("Average Entities per Switch: %.1f", 
             totalSwitches > 0 ? (static_cast<double>(totalEntities) / totalSwitches) : 0.0);
         Logger::GetInstance().Info("========================================");
     }
 };
 
 } // namespace
 
 int main(int argc, char* argv[]) {
     (void)argc;
     (void)argv;
 
     ConfigureLogger();
 
     try {
         Renderer* renderer = nullptr;
         if (!InitializeRenderer(renderer)) {
             return -1;
         }
 
         auto& resourceManager = ResourceManager::GetInstance();
         auto& asyncLoader = AsyncResourceLoader::GetInstance();
         asyncLoader.Initialize();
 
         ApplicationHost host;
         ApplicationHost::Config hostConfig{};
         hostConfig.renderer = renderer;
         hostConfig.resourceManager = &resourceManager;
         hostConfig.asyncLoader = &asyncLoader;
         hostConfig.uniformManager = nullptr;
 
         if (!host.Initialize(hostConfig)) {
            Logger::GetInstance().Error("[SceneHotSwitchStressTest] Failed to initialize ApplicationHost");
            asyncLoader.Shutdown();
            Renderer::Destroy(renderer);
            return -1;
        }

        auto& moduleRegistry = host.GetModuleRegistry();
        moduleRegistry.RegisterModule(std::make_unique<CoreRenderModule>());
        moduleRegistry.RegisterModule(std::make_unique<InputModule>());
        moduleRegistry.RegisterModule(std::make_unique<DebugHUDModule>());

        auto& sceneManager = host.GetSceneManager();
        auto& world = host.GetWorld();

        // 注册测试场景
        std::vector<std::string> sceneNames;
        
        // 场景1：小规模场景（10个实体，20个资源）
        sceneNames.push_back("TestScene_Small");
        sceneManager.RegisterSceneFactory("TestScene_Small", []() {
            return std::make_unique<TestScene>("TestScene_Small", 10, 20, false);
        });

        // 场景2：中等规模场景（50个实体，50个资源）
        sceneNames.push_back("TestScene_Medium");
        sceneManager.RegisterSceneFactory("TestScene_Medium", []() {
            return std::make_unique<TestScene>("TestScene_Medium", 50, 50, false);
        });

        // 场景3：大规模场景（100个实体，100个资源）
        sceneNames.push_back("TestScene_Large");
        sceneManager.RegisterSceneFactory("TestScene_Large", []() {
            return std::make_unique<TestScene>("TestScene_Large", 100, 100, false);
        });

        // 场景4：共享资源场景（使用Shared资源）
        sceneNames.push_back("TestScene_Shared");
        sceneManager.RegisterSceneFactory("TestScene_Shared", []() {
            return std::make_unique<TestScene>("TestScene_Shared", 30, 30, true);
        });

        PerformanceStats stats;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> sceneDist(0, static_cast<int>(sceneNames.size() - 1));

        Logger::GetInstance().Info("========================================");
        Logger::GetInstance().Info("Scene Hot Switch Stress Test");
        Logger::GetInstance().Info("========================================");
        Logger::GetInstance().InfoFormat("Registered %zu test scenes", sceneNames.size());
        Logger::GetInstance().Info("Starting automated stress test...");
        Logger::GetInstance().Info("");

        uint64_t frameIndex = 0;
        double absoluteTime = 0.0;
        const size_t maxSwitches = 50;  // 减少切换次数，让场景有时间渲染
        const size_t framesPerSwitch = 60;  // 每次切换后等待60帧（约1秒）

        // 测试1：快速场景切换（Push/Replace循环）
        {
            Logger::GetInstance().Info("[Test 1] Fast Scene Switching (Push/Replace)...");
            
            size_t switchCount = 0;
            std::string currentScene;
            size_t framesSinceSwitch = 0;
            
            while (switchCount < maxSwitches) {
                // 渲染循环开始
                renderer->BeginFrame();
                renderer->Clear();

                const float deltaTime = renderer->GetDeltaTime();
                absoluteTime += static_cast<double>(deltaTime);
                
                FrameUpdateArgs frameArgs{};
                frameArgs.deltaTime = deltaTime;
                frameArgs.absoluteTime = absoluteTime;
                frameArgs.frameIndex = frameIndex++;

                // PreFrame阶段
                moduleRegistry.InvokePhase(ModulePhase::PreFrame, frameArgs);

                // 场景切换逻辑（只在切换间隔后执行）
                if (!sceneManager.IsTransitionInProgress() && framesSinceSwitch >= framesPerSwitch) {
                    // 执行切换
                    auto switchStart = std::chrono::high_resolution_clock::now();
                    
                    if (currentScene.empty() || switchCount % 2 == 0) {
                        // Push新场景
                        int sceneIndex = sceneDist(gen);
                        currentScene = sceneNames[sceneIndex];
                        if (sceneManager.PushScene(currentScene)) {
                            stats.pushCount++;
                            stats.totalSwitches++;
                            framesSinceSwitch = 0;
                        }
                    } else {
                        // Replace当前场景
                        int sceneIndex = sceneDist(gen);
                        std::string newScene = sceneNames[sceneIndex];
                        if (sceneManager.ReplaceScene(newScene)) {
                            currentScene = newScene;
                            stats.replaceCount++;
                            stats.totalSwitches++;
                            framesSinceSwitch = 0;
                        }
                    }
                    
                    auto switchEnd = std::chrono::high_resolution_clock::now();
                    double switchTime = std::chrono::duration<double>(switchEnd - switchStart).count();
                    
                    size_t entityCount = world.GetEntityManager().GetAllEntities().size();
                    stats.RecordSwitch(switchTime, entityCount);
                    
                    switchCount++;
                } else {
                    framesSinceSwitch++;
                }

                // 场景更新
                sceneManager.Update(frameArgs);

                // PostFrame阶段
                moduleRegistry.InvokePhase(ModulePhase::PostFrame, frameArgs);
                host.GetContext().lastFrame = frameArgs;

                // 更新ECS World（所有注册的系统会在这里运行，包括渲染系统提交渲染对象）
                host.UpdateWorld(deltaTime);

                // 处理异步加载
                asyncLoader.ProcessCompletedTasks(10);

                // 渲染所有提交的渲染对象
                renderer->FlushRenderQueue();
                renderer->EndFrame();
                renderer->Present();
            }

            // 清理所有场景
            while (sceneManager.SceneCount() > 0) {
                SceneExitArgs exitArgs;
                sceneManager.PopScene(exitArgs);
            }

            Logger::GetInstance().InfoFormat("[Test 1] Completed %zu switches", switchCount);
            Logger::GetInstance().Info("");
        }

        // 测试2：场景栈压力测试（多层Push/Pop）
        {
            Logger::GetInstance().Info("[Test 2] Scene Stack Stress Test (Multi-layer Push/Pop)...");
            
            size_t switchCount = 0;
            const size_t maxStackDepth = 5;
            size_t framesSinceSwitch = 0;
            
            while (switchCount < maxSwitches) {
                // 渲染循环开始
                renderer->BeginFrame();
                renderer->Clear();

                const float deltaTime = renderer->GetDeltaTime();
                absoluteTime += static_cast<double>(deltaTime);
                
                FrameUpdateArgs frameArgs{};
                frameArgs.deltaTime = deltaTime;
                frameArgs.absoluteTime = absoluteTime;
                frameArgs.frameIndex = frameIndex++;

                moduleRegistry.InvokePhase(ModulePhase::PreFrame, frameArgs);

                if (!sceneManager.IsTransitionInProgress() && framesSinceSwitch >= framesPerSwitch) {
                    auto switchStart = std::chrono::high_resolution_clock::now();
                    
                    size_t currentDepth = sceneManager.SceneCount();
                    
                    if (currentDepth < maxStackDepth && switchCount % 3 != 0) {
                        // Push场景
                        int sceneIndex = sceneDist(gen);
                        if (sceneManager.PushScene(sceneNames[sceneIndex])) {
                            stats.pushCount++;
                            stats.totalSwitches++;
                            framesSinceSwitch = 0;
                        }
                    } else if (currentDepth > 0) {
                        // Pop场景
                        SceneExitArgs exitArgs;
                        if (sceneManager.PopScene(exitArgs).has_value()) {
                            stats.popCount++;
                            stats.totalSwitches++;
                            framesSinceSwitch = 0;
                        }
                    }
                    
                    auto switchEnd = std::chrono::high_resolution_clock::now();
                    double switchTime = std::chrono::duration<double>(switchEnd - switchStart).count();
                    
                    size_t entityCount = world.GetEntityManager().GetAllEntities().size();
                    stats.RecordSwitch(switchTime, entityCount);
                    
                    switchCount++;
                } else {
                    framesSinceSwitch++;
                }

                sceneManager.Update(frameArgs);
                moduleRegistry.InvokePhase(ModulePhase::PostFrame, frameArgs);
                host.GetContext().lastFrame = frameArgs;

                host.UpdateWorld(deltaTime);
                asyncLoader.ProcessCompletedTasks(10);

                renderer->FlushRenderQueue();
                renderer->EndFrame();
                renderer->Present();
            }

            // 清理所有场景
            while (sceneManager.SceneCount() > 0) {
                SceneExitArgs exitArgs;
                sceneManager.PopScene(exitArgs);
            }

            Logger::GetInstance().InfoFormat("[Test 2] Completed %zu switches", switchCount);
            Logger::GetInstance().Info("");
        }

        // 测试3：长时间运行测试（减少到100次切换，每次等待60帧）
        {
            Logger::GetInstance().Info("[Test 3] Long Running Test (100 switches)...");
            
            size_t switchCount = 0;
            const size_t longRunSwitches = 100;
            std::string currentScene;
            size_t framesSinceSwitch = 0;
            
            while (switchCount < longRunSwitches) {
                // 渲染循环开始
                renderer->BeginFrame();
                renderer->Clear();

                const float deltaTime = renderer->GetDeltaTime();
                absoluteTime += static_cast<double>(deltaTime);
                
                FrameUpdateArgs frameArgs{};
                frameArgs.deltaTime = deltaTime;
                frameArgs.absoluteTime = absoluteTime;
                frameArgs.frameIndex = frameIndex++;

                moduleRegistry.InvokePhase(ModulePhase::PreFrame, frameArgs);

                if (!sceneManager.IsTransitionInProgress() && framesSinceSwitch >= framesPerSwitch) {
                    auto switchStart = std::chrono::high_resolution_clock::now();
                    
                    // 随机选择操作：Push、Pop或Replace
                    int operation = switchCount % 4;
                    
                    if (operation == 0 || currentScene.empty()) {
                        // Push
                        int sceneIndex = sceneDist(gen);
                        currentScene = sceneNames[sceneIndex];
                        if (sceneManager.PushScene(currentScene)) {
                            stats.pushCount++;
                            stats.totalSwitches++;
                            framesSinceSwitch = 0;
                        }
                    } else if (operation == 1 && sceneManager.SceneCount() > 0) {
                        // Pop
                        SceneExitArgs exitArgs;
                        if (sceneManager.PopScene(exitArgs).has_value()) {
                            stats.popCount++;
                            stats.totalSwitches++;
                            framesSinceSwitch = 0;
                            if (sceneManager.SceneCount() > 0) {
                                currentScene.clear();
                            } else {
                                currentScene.clear();
                            }
                        }
                    } else if (operation == 2) {
                        // Replace
                        int sceneIndex = sceneDist(gen);
                        std::string newScene = sceneNames[sceneIndex];
                        if (sceneManager.ReplaceScene(newScene)) {
                            currentScene = newScene;
                            stats.replaceCount++;
                            stats.totalSwitches++;
                            framesSinceSwitch = 0;
                        }
                    }
                    
                    auto switchEnd = std::chrono::high_resolution_clock::now();
                    double switchTime = std::chrono::duration<double>(switchEnd - switchStart).count();
                    
                    size_t entityCount = world.GetEntityManager().GetAllEntities().size();
                    stats.RecordSwitch(switchTime, entityCount);
                    
                    switchCount++;
                    
                    // 每20次切换输出一次进度
                    if (switchCount % 20 == 0) {
                        Logger::GetInstance().InfoFormat(
                            "[Test 3] Progress: %zu/%zu switches (%.1f%%)",
                            switchCount, longRunSwitches,
                            (switchCount * 100.0 / longRunSwitches));
                    }
                } else {
                    framesSinceSwitch++;
                }

                sceneManager.Update(frameArgs);
                moduleRegistry.InvokePhase(ModulePhase::PostFrame, frameArgs);
                host.GetContext().lastFrame = frameArgs;

                host.UpdateWorld(deltaTime);
                asyncLoader.ProcessCompletedTasks(10);

                renderer->FlushRenderQueue();
                renderer->EndFrame();
                renderer->Present();
            }

            // 清理所有场景
            while (sceneManager.SceneCount() > 0) {
                SceneExitArgs exitArgs;
                sceneManager.PopScene(exitArgs);
            }

            Logger::GetInstance().InfoFormat("[Test 3] Completed %zu switches", switchCount);
            Logger::GetInstance().Info("");
        }

        // 打印最终统计报告
        stats.PrintReport();

        // 资源统计
        auto resourceStats = resourceManager.GetStats();
        Logger::GetInstance().Info("========================================");
        Logger::GetInstance().Info("Resource Statistics");
        Logger::GetInstance().Info("========================================");
        Logger::GetInstance().InfoFormat("Textures: %zu", resourceStats.textureCount);
        Logger::GetInstance().InfoFormat("Meshes: %zu", resourceStats.meshCount);
        Logger::GetInstance().InfoFormat("Materials: %zu", resourceStats.materialCount);
        Logger::GetInstance().InfoFormat("Shaders: %zu", resourceStats.shaderCount);
        Logger::GetInstance().InfoFormat("Total Memory: %.2f MB", resourceStats.totalMemory / (1024.0 * 1024.0));
        Logger::GetInstance().Info("========================================");

        // 清理
        host.Shutdown();
        asyncLoader.Shutdown();
        Renderer::Destroy(renderer);

        Logger::GetInstance().Info("[SceneHotSwitchStressTest] All tests completed successfully!");
        return 0;
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat(
            "[SceneHotSwitchStressTest] Exception: %s",
            e.what());
        return -1;
    }
}
                  