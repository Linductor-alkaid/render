/**
 * @file 31_ecs_basic_test.cpp
 * @brief ECS 基础功能测试
 * 
 * 测试内容：
 * - Entity 创建和销毁
 * - Component 添加和查询
 * - System 注册和更新
 * - World 管理
 */

#include <render/ecs/world.h>
#include <render/ecs/components.h>
#include <render/ecs/systems.h>
#include <render/logger.h>
#include <iostream>

using namespace Render;
using namespace Render::ECS;

int main() {
    // Logger 是单例，无需初始化
    Logger::GetInstance().InfoFormat("[ECS Test] === ECS Basic Test ===");
    
    try {
        // ============================================================
        // 1. 创建 World
        // ============================================================
        World world;
        world.Initialize();
        
        Logger::GetInstance().InfoFormat("[ECS Test] World initialized");
        
        // ============================================================
        // 2. 注册组件类型
        // ============================================================
        world.RegisterComponent<TransformComponent>();
        world.RegisterComponent<NameComponent>();
        world.RegisterComponent<TagComponent>();
        world.RegisterComponent<ActiveComponent>();
        world.RegisterComponent<MeshRenderComponent>();
        world.RegisterComponent<CameraComponent>();
        world.RegisterComponent<LightComponent>();
        
        Logger::GetInstance().InfoFormat("[ECS Test] Components registered");
        
        // ============================================================
        // 3. 注册系统
        // ============================================================
        world.RegisterSystem<TransformSystem>();
        world.RegisterSystem<CameraSystem>();
        
        Logger::GetInstance().InfoFormat("[ECS Test] Systems registered");
        
        // ============================================================
        // 4. 创建实体
        // ============================================================
        Logger::GetInstance().InfoFormat("[ECS Test] --- Creating Entities ---");
        
        // 创建相机实体
        EntityID cameraEntity = world.CreateEntity({
            .name = "MainCamera",
            .active = true,
            .tags = {"camera", "main"}
        });
        
        Logger::GetInstance().InfoFormat("[ECS Test] Created camera entity");
        
        // 添加 Transform 组件
        TransformComponent cameraTransform;
        cameraTransform.SetPosition(Vector3(0, 2, 5));
        world.AddComponent(cameraEntity, cameraTransform);
        
        // 添加 Name 组件
        world.AddComponent(cameraEntity, NameComponent("MainCamera"));
        
        // 创建多个立方体实体
        std::vector<EntityID> cubes;
        for (int i = 0; i < 5; ++i) {
            EntityID cube = world.CreateEntity({
                .name = "Cube_" + std::to_string(i),
                .active = true,
                .tags = {"cube", "renderable"}
            });
            
            // 添加 Transform 组件
            TransformComponent transform;
            transform.SetPosition(Vector3(i * 2.0f, 0, 0));
            transform.SetScale(1.0f);
            world.AddComponent(cube, transform);
            
            // 添加 Name 组件
            world.AddComponent(cube, NameComponent("Cube_" + std::to_string(i)));
            
            // 添加 MeshRenderComponent
            MeshRenderComponent meshComp;
            meshComp.meshName = "cube";
            meshComp.materialName = "default";
            meshComp.visible = true;
            world.AddComponent(cube, meshComp);
            
            cubes.push_back(cube);
        }
        
        Logger::GetInstance().InfoFormat("[ECS Test] Created %zu cube entities", cubes.size());
        
        // ============================================================
        // 5. 查询实体
        // ============================================================
        Logger::GetInstance().InfoFormat("[ECS Test] --- Querying Entities ---");
        
        // 查询所有具有 Transform 和 MeshRenderComponent 的实体
        auto renderableEntities = world.Query<TransformComponent, MeshRenderComponent>();
        Logger::GetInstance().InfoFormat("[ECS Test] Found %zu renderable entities", renderableEntities.size());
        
        // 查询具有特定标签的实体
        auto cubeEntities = world.QueryByTag("cube");
        Logger::GetInstance().InfoFormat("[ECS Test] Found %zu cube entities", cubeEntities.size());
        
        // ============================================================
        // 6. 修改组件
        // ============================================================
        Logger::GetInstance().InfoFormat("[ECS Test] --- Modifying Components ---");
        
        for (size_t i = 0; i < cubes.size(); ++i) {
            auto& transform = world.GetComponent<TransformComponent>(cubes[i]);
            
            // 旋转立方体
            float angle = i * 30.0f;
            Quaternion rotation = MathUtils::FromEulerDegrees(0, angle, 0);
            transform.SetRotation(rotation);
            
            Logger::GetInstance().DebugFormat("[ECS Test] Rotated cube %zu by %.1f degrees", i, angle);
        }
        
        // ============================================================
        // 7. 更新 World（模拟游戏循环）
        // ============================================================
        Logger::GetInstance().InfoFormat("[ECS Test] --- Updating World ---");
        
        for (int frame = 0; frame < 3; ++frame) {
            float deltaTime = 0.016f;  // 60 FPS
            
            // 旋转所有立方体
            for (const auto& cube : cubes) {
                auto& transform = world.GetComponent<TransformComponent>(cube);
                Vector3 currentPos = transform.GetPosition();
                currentPos.y() = std::sin(frame * 0.1f);
                transform.SetPosition(currentPos);
            }
            
            // 更新 World
            world.Update(deltaTime);
            
            Logger::GetInstance().InfoFormat("[ECS Test] Frame %d updated", frame + 1);
        }
        
        // ============================================================
        // 8. 统计信息
        // ============================================================
        Logger::GetInstance().InfoFormat("[ECS Test] --- Statistics ---");
        world.PrintStatistics();
        
        // ============================================================
        // 9. 销毁实体
        // ============================================================
        Logger::GetInstance().InfoFormat("[ECS Test] --- Destroying Entities ---");
        
        // 销毁第一个立方体
        Logger::GetInstance().InfoFormat("[ECS Test] About to destroy cube 0...");
        world.DestroyEntity(cubes[0]);
        Logger::GetInstance().InfoFormat("[ECS Test] Destroyed cube 0");
        
        // 验证实体已被销毁
        Logger::GetInstance().InfoFormat("[ECS Test] Validating entity...");
        if (!world.IsValidEntity(cubes[0])) {
            Logger::GetInstance().InfoFormat("[ECS Test] Cube 0 is no longer valid");
        }
        Logger::GetInstance().InfoFormat("[ECS Test] Validation complete");
        
        // ============================================================
        // 10. 清理
        // ============================================================
        Logger::GetInstance().InfoFormat("[ECS Test] --- Cleanup ---");
        world.Shutdown();
        
        Logger::GetInstance().InfoFormat("[ECS Test] === Test Completed Successfully ===");
        
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("[ECS Test] Exception: %s", e.what());
        return 1;
    }
    
    return 0;
}

