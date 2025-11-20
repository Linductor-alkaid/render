/**
 * @file 54_module_hud_test.cpp
 * @brief 模块和HUD测试 - 测试 CoreRenderModule 自动注册渲染系统和 DebugHUDModule 统计信息显示
 */

#include "render/application/application_host.h"
#include "render/application/modules/core_render_module.h"
#include "render/application/modules/debug_hud_module.h"
#include "render/application/modules/input_module.h"
#include "render/application/scenes/boot_scene.h"
#include "render/async_resource_loader.h"
#include "render/ecs/world.h"
#include "render/ecs/systems.h"
#include "render/ecs/components.h"
#include "render/logger.h"
#include "render/renderer.h"
#include "render/resource_manager.h"
#include "render/render_layer.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <string>

using namespace Render;
using namespace Render::Application;
using namespace Render::ECS;

namespace {

void ConfigureLogger() {
    auto& logger = Logger::GetInstance();
    logger.SetLogToConsole(true);
    logger.SetLogToFile(false);
    logger.SetLogLevel(LogLevel::Debug);  // 启用Debug级别，以便查看LayerMaskDebug信息
}

bool InitializeRenderer(Renderer*& renderer) {
    renderer = Renderer::Create();
    if (!renderer) {
        Logger::GetInstance().Error("[ModuleHUDTest] Failed to create renderer");
        return false;
    }

    if (!renderer->Initialize("Module and HUD Test", 1280, 720)) {
        Logger::GetInstance().Error("[ModuleHUDTest] Failed to initialize renderer");
        Renderer::Destroy(renderer);
        renderer = nullptr;
        return false;
    }

    renderer->SetClearColor(0.1f, 0.12f, 0.16f, 1.0f);
    renderer->SetVSync(true);
    return true;
}

// 验证系统是否已注册
void VerifySystemRegistration(World& world) {
    Logger::GetInstance().Info("[ModuleHUDTest] Verifying system registration...");
    
    bool allSystemsFound = true;
    
    // 检查核心系统
    if (!world.GetSystem<WindowSystem>()) {
        Logger::GetInstance().Warning("[ModuleHUDTest] WindowSystem not found");
        allSystemsFound = false;
    } else {
        Logger::GetInstance().Info("[ModuleHUDTest] ✓ WindowSystem registered");
    }
    
    if (!world.GetSystem<CameraSystem>()) {
        Logger::GetInstance().Warning("[ModuleHUDTest] CameraSystem not found");
        allSystemsFound = false;
    } else {
        Logger::GetInstance().Info("[ModuleHUDTest] ✓ CameraSystem registered");
    }
    
    if (!world.GetSystem<TransformSystem>()) {
        Logger::GetInstance().Warning("[ModuleHUDTest] TransformSystem not found");
        allSystemsFound = false;
    } else {
        Logger::GetInstance().Info("[ModuleHUDTest] ✓ TransformSystem registered");
    }
    
    if (!world.GetSystem<GeometrySystem>()) {
        Logger::GetInstance().Warning("[ModuleHUDTest] GeometrySystem not found");
        allSystemsFound = false;
    } else {
        Logger::GetInstance().Info("[ModuleHUDTest] ✓ GeometrySystem registered");
    }
    
    if (!world.GetSystem<ResourceLoadingSystem>()) {
        Logger::GetInstance().Warning("[ModuleHUDTest] ResourceLoadingSystem not found");
        allSystemsFound = false;
    } else {
        Logger::GetInstance().Info("[ModuleHUDTest] ✓ ResourceLoadingSystem registered");
    }
    
    if (!world.GetSystem<LightSystem>()) {
        Logger::GetInstance().Warning("[ModuleHUDTest] LightSystem not found");
        allSystemsFound = false;
    } else {
        Logger::GetInstance().Info("[ModuleHUDTest] ✓ LightSystem registered");
    }
    
    if (!world.GetSystem<UniformSystem>()) {
        Logger::GetInstance().Warning("[ModuleHUDTest] UniformSystem not found");
        allSystemsFound = false;
    } else {
        Logger::GetInstance().Info("[ModuleHUDTest] ✓ UniformSystem registered");
    }
    
    if (!world.GetSystem<MeshRenderSystem>()) {
        Logger::GetInstance().Warning("[ModuleHUDTest] MeshRenderSystem not found");
        allSystemsFound = false;
    } else {
        Logger::GetInstance().Info("[ModuleHUDTest] ✓ MeshRenderSystem registered");
    }
    
    if (!world.GetSystem<ModelRenderSystem>()) {
        Logger::GetInstance().Warning("[ModuleHUDTest] ModelRenderSystem not found");
        allSystemsFound = false;
    } else {
        Logger::GetInstance().Info("[ModuleHUDTest] ✓ ModelRenderSystem registered");
    }
    
    if (!world.GetSystem<SpriteAnimationSystem>()) {
        Logger::GetInstance().Warning("[ModuleHUDTest] SpriteAnimationSystem not found");
        allSystemsFound = false;
    } else {
        Logger::GetInstance().Info("[ModuleHUDTest] ✓ SpriteAnimationSystem registered");
    }
    
    if (!world.GetSystem<SpriteRenderSystem>()) {
        Logger::GetInstance().Warning("[ModuleHUDTest] SpriteRenderSystem not found");
        allSystemsFound = false;
    } else {
        Logger::GetInstance().Info("[ModuleHUDTest] ✓ SpriteRenderSystem registered");
    }
    
    if (!world.GetSystem<ResourceCleanupSystem>()) {
        Logger::GetInstance().Warning("[ModuleHUDTest] ResourceCleanupSystem not found");
        allSystemsFound = false;
    } else {
        Logger::GetInstance().Info("[ModuleHUDTest] ✓ ResourceCleanupSystem registered");
    }
    
    if (allSystemsFound) {
        Logger::GetInstance().Info("[ModuleHUDTest] ✓ All core systems registered successfully");
    } else {
        Logger::GetInstance().Error("[ModuleHUDTest] ✗ Some systems are missing");
    }
}

// 验证组件是否已注册
void VerifyComponentRegistration(World& world) {
    Logger::GetInstance().Info("[ModuleHUDTest] Verifying component registration...");
    
    // 创建测试实体并添加组件来验证注册
    EntityID testEntity = world.CreateEntity();
    
    // 尝试添加各种组件（验证组件已注册）
    try {
        world.AddComponent<TransformComponent>(testEntity, TransformComponent{});
        Logger::GetInstance().Info("[ModuleHUDTest] ✓ TransformComponent registered");
    } catch (...) {
        Logger::GetInstance().Error("[ModuleHUDTest] ✗ TransformComponent not registered");
    }
    
    try {
        world.AddComponent<MeshRenderComponent>(testEntity, MeshRenderComponent{});
        Logger::GetInstance().Info("[ModuleHUDTest] ✓ MeshRenderComponent registered");
    } catch (...) {
        Logger::GetInstance().Error("[ModuleHUDTest] ✗ MeshRenderComponent not registered");
    }
    
    try {
        world.AddComponent<ModelComponent>(testEntity, ModelComponent{});
        Logger::GetInstance().Info("[ModuleHUDTest] ✓ ModelComponent registered");
    } catch (...) {
        Logger::GetInstance().Error("[ModuleHUDTest] ✗ ModelComponent not registered");
    }
    
    try {
        world.AddComponent<SpriteRenderComponent>(testEntity, SpriteRenderComponent{});
        Logger::GetInstance().Info("[ModuleHUDTest] ✓ SpriteRenderComponent registered");
    } catch (...) {
        Logger::GetInstance().Error("[ModuleHUDTest] ✗ SpriteRenderComponent not registered");
    }
    
    try {
        world.AddComponent<CameraComponent>(testEntity, CameraComponent{});
        Logger::GetInstance().Info("[ModuleHUDTest] ✓ CameraComponent registered");
    } catch (...) {
        Logger::GetInstance().Error("[ModuleHUDTest] ✗ CameraComponent not registered");
    }
    
    try {
        world.AddComponent<LightComponent>(testEntity, LightComponent{});
        Logger::GetInstance().Info("[ModuleHUDTest] ✓ LightComponent registered");
    } catch (...) {
        Logger::GetInstance().Error("[ModuleHUDTest] ✗ LightComponent not registered");
    }
    
    try {
        world.AddComponent<GeometryComponent>(testEntity, GeometryComponent{});
        Logger::GetInstance().Info("[ModuleHUDTest] ✓ GeometryComponent registered");
    } catch (...) {
        Logger::GetInstance().Error("[ModuleHUDTest] ✗ GeometryComponent not registered");
    }
    
    // 清理测试实体
    world.DestroyEntity(testEntity);
    
    Logger::GetInstance().Info("[ModuleHUDTest] Component registration verification complete");
}

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
        asyncLoader.Initialize(1);

        ApplicationHost host;
        ApplicationHost::Config hostConfig{};
        hostConfig.renderer = renderer;
        hostConfig.resourceManager = &resourceManager;
        hostConfig.asyncLoader = &asyncLoader;
        hostConfig.uniformManager = nullptr;

        if (!host.Initialize(hostConfig)) {
            Logger::GetInstance().Error("[ModuleHUDTest] Failed to initialize ApplicationHost");
            asyncLoader.Shutdown();
            Renderer::Destroy(renderer);
            return -1;
        }

        auto& moduleRegistry = host.GetModuleRegistry();
        
        // 注册 CoreRenderModule（会自动注册所有核心系统和组件）
        moduleRegistry.RegisterModule(std::make_unique<CoreRenderModule>());
        Logger::GetInstance().Info("[ModuleHUDTest] Registered CoreRenderModule");
        
        // 注册 InputModule（用于退出控制）
        moduleRegistry.RegisterModule(std::make_unique<InputModule>());
        Logger::GetInstance().Info("[ModuleHUDTest] Registered InputModule");
        
        // 注册 DebugHUDModule（显示统计信息）
        moduleRegistry.RegisterModule(std::make_unique<DebugHUDModule>());
        Logger::GetInstance().Info("[ModuleHUDTest] Registered DebugHUDModule");
        
        // 验证系统注册
        auto& world = host.GetWorld();
        VerifySystemRegistration(world);
        VerifyComponentRegistration(world);
        
        // 注册场景
        host.RegisterSceneFactory("BootScene", []() { return std::make_unique<BootScene>(); });
        host.PushScene("BootScene");
        Logger::GetInstance().Info("[ModuleHUDTest] Pushed BootScene");

        auto* inputModule = static_cast<InputModule*>(moduleRegistry.GetModule("InputModule"));

        // 获取层注册表，用于切换HUD层可见性
        auto& layerRegistry = renderer->GetLayerRegistry();
        RenderLayerId uiLayerId = Layers::UI::Default;
        RenderLayerId worldLayerId = Layers::World::Midground;
        bool hudVisible = true;
        
        // 计算层掩码（参考51测试）
        uint32_t worldMask = 0;
        uint32_t uiMask = 0;
        for (const auto& record : layerRegistry.ListLayers()) {
            if (record.descriptor.id == Layers::World::Midground) {
                worldMask = 1u << record.descriptor.maskIndex;
            } else if (record.descriptor.id == Layers::UI::Default) {
                uiMask = 1u << record.descriptor.maskIndex;
            }
        }
        
        // 确保UI层初始可见
        if (layerRegistry.HasLayer(uiLayerId)) {
            layerRegistry.SetEnabled(uiLayerId, true);
            Logger::GetInstance().InfoFormat("[ModuleHUDTest] UI layer (id=%u) enabled", uiLayerId.value);
        } else {
            Logger::GetInstance().WarningFormat("[ModuleHUDTest] UI layer (id=%u) not found", uiLayerId.value);
        }
        
        // 确保世界层已启用（3D场景渲染需要）
        if (layerRegistry.HasLayer(worldLayerId)) {
            layerRegistry.SetEnabled(worldLayerId, true);
            auto worldDescOpt = layerRegistry.GetDescriptor(worldLayerId);
            auto worldStateOpt = layerRegistry.GetState(worldLayerId);
            if (worldDescOpt.has_value() && worldStateOpt.has_value()) {
                const auto& worldDesc = worldDescOpt.value();
                const auto& worldState = worldStateOpt.value();
                Logger::GetInstance().InfoFormat("[ModuleHUDTest] World layer (id=%u, maskIndex=%u) enabled: %s", 
                                                 worldLayerId.value,
                                                 worldDesc.maskIndex,
                                                 worldState.enabled ? "true" : "false");
            } else {
                Logger::GetInstance().WarningFormat("[ModuleHUDTest] Failed to get world layer descriptor/state");
            }
        } else {
            Logger::GetInstance().WarningFormat("[ModuleHUDTest] World layer (id=%u) not found", worldLayerId.value);
        }
        
        // 获取相机实体并设置初始layerMask（参考51测试）
        auto* cameraSystem = world.GetSystem<CameraSystem>();
        EntityID mainCameraEntity = cameraSystem ? cameraSystem->GetMainCamera() : EntityID::Invalid();
        if (mainCameraEntity != EntityID::Invalid() && world.HasComponent<CameraComponent>(mainCameraEntity)) {
            auto& cameraComponent = world.GetComponent<CameraComponent>(mainCameraEntity);
            // 根据当前层的enabled状态设置layerMask
            bool worldEnabled = false;
            bool uiEnabled = false;
            if (layerRegistry.HasLayer(worldLayerId)) {
                auto worldStateOpt = layerRegistry.GetState(worldLayerId);
                worldEnabled = worldStateOpt.has_value() && worldStateOpt.value().enabled;
            }
            if (layerRegistry.HasLayer(uiLayerId)) {
                auto uiStateOpt = layerRegistry.GetState(uiLayerId);
                uiEnabled = uiStateOpt.has_value() && uiStateOpt.value().enabled;
            }
            cameraComponent.layerMask = (worldEnabled ? worldMask : 0u) | (uiEnabled ? uiMask : 0u);
            Logger::GetInstance().InfoFormat("[ModuleHUDTest] Initial camera layerMask = 0x%08X (world=%s, ui=%s)",
                                             cameraComponent.layerMask,
                                             worldEnabled ? "ON" : "OFF",
                                             uiEnabled ? "ON" : "OFF");
        }

        bool running = true;
        uint64_t frameIndex = 0;
        double absoluteTime = 0.0;

        Logger::GetInstance().Info("[ModuleHUDTest] =========================================");
        Logger::GetInstance().Info("[ModuleHUDTest] Module and HUD Test Started");
        Logger::GetInstance().Info("[ModuleHUDTest] =========================================");
        Logger::GetInstance().Info("[ModuleHUDTest] ");
        Logger::GetInstance().Info("[ModuleHUDTest] Test Features:");
        Logger::GetInstance().Info("[ModuleHUDTest]   1. CoreRenderModule auto-registration");
        Logger::GetInstance().Info("[ModuleHUDTest]   2. DebugHUDModule statistics display");
        Logger::GetInstance().Info("[ModuleHUDTest] ");
        Logger::GetInstance().Info("[ModuleHUDTest] Controls:");
        Logger::GetInstance().Info("[ModuleHUDTest]   - ESC or Close Window: Exit");
        Logger::GetInstance().Info("[ModuleHUDTest]   - H: Toggle HUD layer visibility");
        Logger::GetInstance().Info("[ModuleHUDTest] ");
        Logger::GetInstance().Info("[ModuleHUDTest] The Debug HUD should display:");
        Logger::GetInstance().Info("[ModuleHUDTest]   - Performance stats (FPS, Frame Time)");
        Logger::GetInstance().Info("[ModuleHUDTest]   - Rendering stats (Draw Calls, Batches, Triangles)");
        Logger::GetInstance().Info("[ModuleHUDTest]   - Resource stats (Textures, Meshes, Materials)");
        Logger::GetInstance().Info("[ModuleHUDTest]   - Memory stats (Total, Textures, Meshes)");
        Logger::GetInstance().Info("[ModuleHUDTest] =========================================");

        while (running) {
            renderer->BeginFrame();
            renderer->Clear();

            const float deltaTime = renderer->GetDeltaTime();
            absoluteTime += static_cast<double>(deltaTime);

            FrameUpdateArgs frameArgs{};
            frameArgs.deltaTime = deltaTime;
            frameArgs.absoluteTime = absoluteTime;
            frameArgs.frameIndex = frameIndex++;

            // PreFrame 阶段
            host.GetModuleRegistry().InvokePhase(ModulePhase::PreFrame, frameArgs);

            // 场景更新
            host.GetSceneManager().Update(frameArgs);

            // PostFrame 阶段（DebugHUDModule 会在这里显示统计信息）
            host.GetModuleRegistry().InvokePhase(ModulePhase::PostFrame, frameArgs);

            host.GetContext().lastFrame = frameArgs;

            // 检查退出请求和按键输入（第一次检查）
            bool quitRequested = false;
            if (inputModule) {
                quitRequested = inputModule->WasQuitRequested() ||
                                inputModule->IsKeyDown(SDL_SCANCODE_ESCAPE);
                
                // 检查H键切换HUD层可见性（使用WasKeyPressed检测按键按下事件）
                if (inputModule->WasKeyPressed(SDL_SCANCODE_H)) {
                    hudVisible = !hudVisible;
                    layerRegistry.SetEnabled(uiLayerId, hudVisible);
                    Logger::GetInstance().InfoFormat("[ModuleHUDTest] HUD layer visibility toggled to %s", 
                                                     hudVisible ? "ON" : "OFF");
                    
                    // 更新相机layerMask以反映层的enabled状态（参考51测试）
                    if (mainCameraEntity != EntityID::Invalid() && world.HasComponent<CameraComponent>(mainCameraEntity)) {
                        auto& cameraComponent = world.GetComponent<CameraComponent>(mainCameraEntity);
                        bool worldEnabled = false;
                        bool uiEnabled = false;
                        if (layerRegistry.HasLayer(worldLayerId)) {
                            auto worldStateOpt = layerRegistry.GetState(worldLayerId);
                            worldEnabled = worldStateOpt.has_value() && worldStateOpt.value().enabled;
                        }
                        if (layerRegistry.HasLayer(uiLayerId)) {
                            auto uiStateOpt = layerRegistry.GetState(uiLayerId);
                            uiEnabled = uiStateOpt.has_value() && uiStateOpt.value().enabled;
                        }
                        cameraComponent.layerMask = (worldEnabled ? worldMask : 0u) | (uiEnabled ? uiMask : 0u);
                        Logger::GetInstance().InfoFormat("[ModuleHUDTest] Camera layerMask updated to 0x%08X (world=%s, ui=%s)",
                                                         cameraComponent.layerMask,
                                                         worldEnabled ? "ON" : "OFF",
                                                         uiEnabled ? "ON" : "OFF");
                    }
                }
            }

            // 更新 ECS World（所有注册的系统会在这里运行，包括渲染系统提交渲染对象）
            host.UpdateWorld(deltaTime);

            // 再次检查退出请求（在UpdateWorld之后）
            if (quitRequested) {
                running = false;
                renderer->EndFrame();
                break;
            }

            // 渲染所有提交的渲染对象
            renderer->FlushRenderQueue();
            
            // 每60帧检查一次世界层状态（用于调试）
            if (frameIndex % 60 == 0) {
                if (layerRegistry.HasLayer(worldLayerId)) {
                    auto worldDescOpt = layerRegistry.GetDescriptor(worldLayerId);
                    auto worldStateOpt = layerRegistry.GetState(worldLayerId);
                    if (worldDescOpt.has_value() && worldStateOpt.has_value()) {
                        const auto& worldDesc = worldDescOpt.value();
                        const auto& worldState = worldStateOpt.value();
                        uint32_t activeLayerMask = renderer->GetActiveLayerMask();
                        bool maskAllows = ((activeLayerMask >> worldDesc.maskIndex) & 0x1u) != 0;
                        
                        Logger::GetInstance().InfoFormat(
                            "[ModuleHUDTest] World layer check: enabled=%s, maskIndex=%u, activeMask=0x%08X, maskAllows=%s",
                            worldState.enabled ? "true" : "false",
                            worldDesc.maskIndex,
                            activeLayerMask,
                            maskAllows ? "true" : "false");
                    }
                }
            }
            
            renderer->EndFrame();
            renderer->Present();

            // 处理异步加载任务
            asyncLoader.ProcessCompletedTasks(4);
            
            // 每60帧输出一次统计信息到日志（用于验证）
            if (frameIndex % 60 == 0) {
                auto stats = renderer->GetStats();
                auto resourceStats = resourceManager.GetStats();
                Logger::GetInstance().InfoFormat(
                    "[ModuleHUDTest] Frame %llu - FPS: %.1f, DrawCalls: %u, Batches: %u, "
                    "Triangles: %u, Textures: %zu, Meshes: %zu, Memory: %.2f MB",
                    static_cast<unsigned long long>(frameIndex),
                    stats.fps,
                    stats.drawCalls,
                    stats.batchCount,
                    stats.triangles,
                    resourceStats.textureCount,
                    resourceStats.meshCount,
                    static_cast<float>(resourceStats.totalMemory) / (1024.0f * 1024.0f)
                );
            }
        }

        host.Shutdown();
        asyncLoader.Shutdown();
        Renderer::Destroy(renderer);

        Logger::GetInstance().Info("[ModuleHUDTest] =========================================");
        Logger::GetInstance().Info("[ModuleHUDTest] Test completed successfully");
        Logger::GetInstance().Info("[ModuleHUDTest] =========================================");
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("[ModuleHUDTest] Unhandled std::exception: %s", e.what());
        return -1;
    } catch (...) {
        Logger::GetInstance().Error("[ModuleHUDTest] Unhandled unknown exception");
        return -1;
    }

    return 0;
}

