/**
 * @file 55_scene_serialization_test.cpp
 * @brief 场景序列化测试 - 测试场景保存和加载功能
 * 
 * 测试功能：
 * 1. 创建场景并添加实体和组件
 * 2. 保存场景到JSON文件
 * 3. 从JSON文件加载场景
 * 4. 验证加载的场景是否正确
 * 
 * 操作说明：
 * - S键：保存当前场景
 * - L键：加载场景
 * - ESC键：退出
 */

#include "render/application/application_host.h"
#include "render/application/modules/core_render_module.h"
#include "render/application/modules/debug_hud_module.h"
#include "render/application/modules/input_module.h"
#include "render/application/module_registry.h"
#include "render/application/scene_manager.h"
#include "render/application/scene_serializer.h"
#include "render/async_resource_loader.h"
#include "render/ecs/world.h"
#include "render/ecs/systems.h"
#include "render/ecs/components.h"
#include "render/logger.h"
#include "render/renderer.h"
#include "render/resource_manager.h"
#include "render/camera.h"
#include "render/mesh_loader.h"
#include "render/material.h"
#include "render/shader_cache.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <filesystem>

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
        Logger::GetInstance().Error("[SceneSerializationTest] Failed to create renderer");
        return false;
    }

    if (!renderer->Initialize("Scene Serialization Test", 1280, 720)) {
        Logger::GetInstance().Error("[SceneSerializationTest] Failed to initialize renderer");
        Renderer::Destroy(renderer);
        renderer = nullptr;
        return false;
    }

    renderer->SetClearColor(0.1f, 0.12f, 0.16f, 1.0f);
    renderer->SetVSync(true);
    return true;
}

// 创建测试场景
void CreateTestScene(World& world, ResourceManager& resourceManager) {
    Logger::GetInstance().Info("[SceneSerializationTest] Creating test scene...");

    // 创建相机
    EntityID camera = world.CreateEntity({.name = "TestCamera", .active = true});
    
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

    // 创建光源
    EntityID light = world.CreateEntity({.name = "TestLight", .active = true});
    
    TransformComponent lightTransform;
    lightTransform.SetPosition(Vector3(2.0f, 3.0f, 2.0f));
    world.AddComponent(light, lightTransform);

    LightComponent lightComp;
    lightComp.type = ECS::LightType::Point;
    lightComp.color = Color(1.0f, 0.95f, 0.85f);
    lightComp.intensity = 4.0f;
    lightComp.range = 10.0f;
    lightComp.enabled = true;
    world.AddComponent(light, lightComp);

    // 创建立方体网格和材质
    const char* meshName = "test.cube.mesh";
    const char* materialName = "test.cube.material";
    const char* shaderName = "test.cube.shader";

    if (!resourceManager.HasMesh(meshName)) {
        auto mesh = MeshLoader::CreateCube(1.0f, 1.0f, 1.0f, Color::White());
        resourceManager.RegisterMesh(meshName, mesh);
    }

    auto shader = ShaderCache::GetInstance().LoadShader(
        shaderName, "shaders/material_phong.vert", "shaders/material_phong.frag");

    if (!resourceManager.HasMaterial(materialName)) {
        auto material = std::make_shared<Material>();
        material->SetName(materialName);
        material->SetShader(shader);
        material->SetAmbientColor(Color(0.15f, 0.15f, 0.15f, 1.0f));
        material->SetDiffuseColor(Color(0.2f, 0.6f, 1.0f, 1.0f));
        material->SetSpecularColor(Color(0.9f, 0.9f, 0.9f, 1.0f));
        material->SetShininess(64.0f);
        resourceManager.RegisterMaterial(materialName, material);
    }

    // 创建立方体实体
    EntityID cube = world.CreateEntity({.name = "TestCube", .active = true});
    
    TransformComponent cubeTransform;
    cubeTransform.SetPosition(Vector3::Zero());
    world.AddComponent(cube, cubeTransform);

    MeshRenderComponent meshComp;
    meshComp.meshName = meshName;
    meshComp.materialName = materialName;
    meshComp.mesh = resourceManager.GetMesh(meshName);
    meshComp.material = resourceManager.GetMaterial(materialName);
    meshComp.resourcesLoaded = (meshComp.mesh != nullptr && meshComp.material != nullptr);
    meshComp.layerID = Layers::World::Midground.value;
    meshComp.SetDiffuseColor(Color(0.3f, 0.7f, 1.0f, 1.0f));
    world.AddComponent(cube, meshComp);

    Logger::GetInstance().InfoFormat(
        "[SceneSerializationTest] Test scene created: %zu entities",
        world.GetEntityManager().GetAllEntities().size());
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
            Logger::GetInstance().Error("[SceneSerializationTest] Failed to initialize ApplicationHost");
            asyncLoader.Shutdown();
            Renderer::Destroy(renderer);
            return -1;
        }

        auto& moduleRegistry = host.GetModuleRegistry();
        
        // 注册模块
        moduleRegistry.RegisterModule(std::make_unique<CoreRenderModule>());
        moduleRegistry.RegisterModule(std::make_unique<InputModule>());
        moduleRegistry.RegisterModule(std::make_unique<DebugHUDModule>());
        
        auto& world = host.GetWorld();
        auto& sceneManager = host.GetSceneManager();
        
        // 创建测试场景
        CreateTestScene(world, resourceManager);

        SceneSerializer serializer;
        const std::string savePath = "test_scene.json";
        bool sceneSaved = false;

        Logger::GetInstance().Info("[SceneSerializationTest] ========================================");
        Logger::GetInstance().Info("[SceneSerializationTest] Controls:");
        Logger::GetInstance().Info("[SceneSerializationTest]   S - Save scene to test_scene.json");
        Logger::GetInstance().Info("[SceneSerializationTest]   L - Load scene from test_scene.json");
        Logger::GetInstance().Info("[SceneSerializationTest]   ESC - Exit");
        Logger::GetInstance().Info("[SceneSerializationTest] ========================================");

        auto* inputModule = static_cast<InputModule*>(moduleRegistry.GetModule("InputModule"));
        bool running = true;
        
        uint64_t frameIndex = 0;
        double absoluteTime = 0.0;

        // 主循环
        while (running) {
            // 处理输入
            if (inputModule) {
                if (inputModule->WasKeyPressed(SDL_SCANCODE_ESCAPE)) {
                    running = false;
                }

                if (inputModule->WasKeyPressed(SDL_SCANCODE_S)) {
                    // 保存场景（直接使用SceneSerializer，因为测试程序没有通过SceneManager创建场景）
                    Logger::GetInstance().Info("[SceneSerializationTest] Saving scene...");
                    SceneSerializer serializer;
                    if (serializer.SaveScene(world, "TestScene", savePath)) {
                        sceneSaved = true;
                        Logger::GetInstance().InfoFormat(
                            "[SceneSerializationTest] Scene saved successfully to '%s'",
                            savePath.c_str());
                    } else {
                        Logger::GetInstance().Error("[SceneSerializationTest] Failed to save scene");
                    }
                }

                if (inputModule->WasKeyPressed(SDL_SCANCODE_L)) {
                    // 加载场景（直接使用SceneSerializer）
                    if (std::filesystem::exists(savePath)) {
                        Logger::GetInstance().Info("[SceneSerializationTest] Loading scene...");
                        
                        // 清空当前场景
                        auto allEntities = world.GetEntityManager().GetAllEntities();
                        for (const auto& entity : allEntities) {
                            world.DestroyEntity(entity);
                        }

                        // 加载场景（资源应该已经在ResourceManager中，不需要重新创建）
                        SceneSerializer serializer;
                        auto& ctx = host.GetContext();
                        auto sceneName = serializer.LoadScene(world, savePath, ctx);
                        if (sceneName.has_value()) {
                            Logger::GetInstance().InfoFormat(
                                "[SceneSerializationTest] Scene '%s' loaded successfully from '%s'",
                                sceneName.value().c_str(), savePath.c_str());
                            
                            // 验证加载的实体
                            auto loadedEntities = world.GetEntityManager().GetAllEntities();
                            Logger::GetInstance().InfoFormat(
                                "[SceneSerializationTest] Loaded %zu entities",
                                loadedEntities.size());
                        } else {
                            Logger::GetInstance().Error("[SceneSerializationTest] Failed to load scene");
                        }
                    } else {
                        Logger::GetInstance().WarningFormat(
                            "[SceneSerializationTest] Scene file '%s' not found. Save scene first (S key).",
                            savePath.c_str());
                    }
                }
            }

            // 渲染循环开始
            renderer->BeginFrame();
            renderer->Clear();

            // 更新
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

            // PostFrame 阶段
            host.GetModuleRegistry().InvokePhase(ModulePhase::PostFrame, frameArgs);

            host.GetContext().lastFrame = frameArgs;

            // 更新 ECS World（所有注册的系统会在这里运行，包括渲染系统提交渲染对象）
            host.UpdateWorld(deltaTime);

            // 渲染所有提交的渲染对象
            renderer->FlushRenderQueue();
            
            renderer->EndFrame();
            renderer->Present();
        }

        // 清理
        asyncLoader.Shutdown();
        Renderer::Destroy(renderer);

        Logger::GetInstance().Info("[SceneSerializationTest] Exiting...");
        return 0;
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat(
            "[SceneSerializationTest] Exception: %s",
            e.what());
        return -1;
    }
}

