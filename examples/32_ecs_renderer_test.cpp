/**
 * @file 32_ecs_renderer_test.cpp
 * @brief ECS + Renderer 集成测试
 * 
 * 测试内容：
 * - Renderable 提交到渲染队列
 * - MeshRenderable 渲染
 * - 渲染队列排序（按层级和优先级）
 * - 完整的渲染循环
 */

#include <render/renderer.h>
#include <render/ecs/world.h>
#include <render/ecs/components.h>
#include <render/ecs/systems.h>
#include <render/renderable.h>
#include <render/mesh_loader.h>
#include <render/shader_cache.h>
#include <render/logger.h>
#include <render/material.h>
#include <SDL3/SDL.h>

using namespace Render;
using namespace Render::ECS;

int main() {
    Logger::GetInstance().InfoFormat("[ECS Renderer Test] === ECS + Renderer Integration Test ===");
    
    try {
        // ============================================================
        // 1. 初始化渲染器
        // ============================================================
        Renderer* renderer = Renderer::Create();
        if (!renderer->Initialize("ECS Renderer Test", 1280, 720)) {
            Logger::GetInstance().ErrorFormat("[ECS Renderer Test] Failed to initialize renderer");
            return 1;
        }
        
        Logger::GetInstance().InfoFormat("[ECS Renderer Test] Renderer initialized");
        
        // ============================================================
        // 2. 加载着色器
        // ============================================================
        auto& shaderCache = ShaderCache::GetInstance();
        auto shader = shaderCache.LoadShader(
            "basic",
            "shaders/basic.vert",
            "shaders/basic.frag"
        );
        
        if (!shader) {
            Logger::GetInstance().ErrorFormat("[ECS Renderer Test] Failed to load shader");
            Renderer::Destroy(renderer);
            return 1;
        }
        
        Logger::GetInstance().InfoFormat("[ECS Renderer Test] Shader loaded");
        
        // ============================================================
        // 3. 创建材质
        // ============================================================
        auto material = std::make_shared<Material>();
        material->SetShader(shader);
        material->SetDiffuseColor(Color(0.8f, 0.2f, 0.2f, 1.0f));  // 红色
        
        Logger::GetInstance().InfoFormat("[ECS Renderer Test] Material created");
        
        // ============================================================
        // 4. 创建网格（立方体）
        // ============================================================
        auto mesh = MeshLoader::CreateCube(1.0f);
        Logger::GetInstance().InfoFormat("[ECS Renderer Test] Mesh created");
        
        // ============================================================
        // 5. 创建 World 和 ECS 系统
        // ============================================================
        World world;
        world.Initialize();
        
        // 注册组件
        world.RegisterComponent<TransformComponent>();
        world.RegisterComponent<MeshRenderComponent>();
        world.RegisterComponent<CameraComponent>();
        
        // 注册系统（注意：传入 renderer 指针）
        world.RegisterSystem<CameraSystem>();
        world.RegisterSystem<TransformSystem>();
        world.RegisterSystem<MeshRenderSystem>(renderer);
        
        Logger::GetInstance().InfoFormat("[ECS Renderer Test] World and systems initialized");
        
        // ============================================================
        // 6. 创建相机实体
        // ============================================================
        EntityID cameraEntity = world.CreateEntity({
            .name = "MainCamera",
            .active = true
        });
        
        TransformComponent cameraTransform;
        cameraTransform.SetPosition(Vector3(0, 0, 5));
        world.AddComponent(cameraEntity, cameraTransform);
        
        CameraComponent cameraComp;
        cameraComp.camera = std::make_shared<Camera>();
        cameraComp.camera->SetPerspective(45.0f, 16.0f/9.0f, 0.1f, 1000.0f);
        cameraComp.active = true;
        world.AddComponent(cameraEntity, cameraComp);
        
        Logger::GetInstance().InfoFormat("[ECS Renderer Test] Camera created");
        
        // ============================================================
        // 7. 创建多个立方体实体
        // ============================================================
        std::vector<EntityID> cubes;
        for (int i = 0; i < 3; ++i) {
            EntityID cube = world.CreateEntity({
                .name = "Cube_" + std::to_string(i)
            });
            
            TransformComponent transform;
            transform.SetPosition(Vector3(i * 2.0f - 2.0f, 0, 0));
            world.AddComponent(cube, transform);
            
            MeshRenderComponent meshComp;
            meshComp.mesh = mesh;
            meshComp.material = material;
            meshComp.visible = true;
            meshComp.resourcesLoaded = true;  // 直接设置为已加载
            meshComp.layerID = 300 + i;  // 不同层级
            meshComp.renderPriority = i;  // 不同优先级
            world.AddComponent(cube, meshComp);
            
            cubes.push_back(cube);
        }
        
        Logger::GetInstance().InfoFormat("[ECS Renderer Test] Created %zu cubes", cubes.size());
        
        // ============================================================
        // 8. 主渲染循环
        // ============================================================
        Logger::GetInstance().InfoFormat("[ECS Renderer Test] Starting render loop...");
        Logger::GetInstance().InfoFormat("[ECS Renderer Test] Press ESC or close window to exit");
        
        // 设置全局的 view 和 projection 矩阵
        Matrix4 view = Matrix4::Identity();
        view(2, 3) = -5.0f; // 相机向后移动5个单位
        
        float aspect = 1280.0f / 720.0f;
        float fov = 45.0f * 3.14159f / 180.0f;
        float nearPlane = 0.1f;
        float farPlane = 100.0f;
        float f = 1.0f / std::tan(fov / 2.0f);
        
        Matrix4 projection = Matrix4::Identity();
        projection(0, 0) = f / aspect;
        projection(1, 1) = f;
        projection(2, 2) = (farPlane + nearPlane) / (nearPlane - farPlane);
        projection(2, 3) = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
        projection(3, 2) = -1.0f;
        projection(3, 3) = 0.0f;
        
        bool running = true;
        int frameCount = 0;
        
        while (running) {
            // 事件处理
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                }
                if (event.type == SDL_EVENT_KEY_DOWN &&
                    event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
            }
            
            float deltaTime = 0.016f;  // 60 FPS
            
            // 旋转立方体
            for (size_t i = 0; i < cubes.size(); ++i) {
                auto& transform = world.GetComponent<TransformComponent>(cubes[i]);
                float angle = frameCount * 2.0f + i * 120.0f;
                Quaternion rotation = MathUtils::FromEulerDegrees(0, angle, 0);
                transform.SetRotation(rotation);
            }
            
            // 开始渲染帧
            renderer->BeginFrame();
            renderer->Clear();
            
            // 设置全局 uniform（view 和 projection）
            shader->Use();
            auto uniformMgr = shader->GetUniformManager();
            if (uniformMgr) {
                uniformMgr->SetMatrix4("view", view);
                uniformMgr->SetMatrix4("projection", projection);
                uniformMgr->SetColor("color", Color(0.8f, 0.2f, 0.2f, 1.0f));
                uniformMgr->SetBool("useTexture", false);
                uniformMgr->SetBool("useVertexColor", false);
            }
            
            // ECS 更新（这会调用 MeshRenderSystem::Update，向队列提交 Renderable）
            world.Update(deltaTime);
            
            // 检查队列大小（在 Flush 之前）
            size_t queueSize = renderer->GetRenderQueueSize();
            
            // 渲染队列中的所有对象
            renderer->FlushRenderQueue();
            
            // 结束渲染帧
            renderer->EndFrame();
            renderer->Present();
            
            frameCount++;
            
            // 每60帧打印一次统计信息
            if (frameCount % 60 == 0) {
                Logger::GetInstance().InfoFormat(
                    "[ECS Renderer Test] Frame %d: %zu objects submitted",
                    frameCount, queueSize
                );
            }
        }
        
        Logger::GetInstance().InfoFormat("[ECS Renderer Test] Rendered %d frames", frameCount);
        
        // ============================================================
        // 9. 统计信息
        // ============================================================
        world.PrintStatistics();
        auto stats = renderer->GetStats();
        Logger::GetInstance().InfoFormat("[ECS Renderer Test] Total draw calls: %u", stats.drawCalls);
        
        // ============================================================
        // 10. 清理
        // ============================================================
        world.Shutdown();
        Renderer::Destroy(renderer);
        
        Logger::GetInstance().InfoFormat("[ECS Renderer Test] === Test Completed Successfully ===");
        
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("[ECS Renderer Test] Exception: %s", e.what());
        return 1;
    }
    
    return 0;
}

