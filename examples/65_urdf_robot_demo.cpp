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
 * @file 65_urdf_robot_demo.cpp
 * @brief URDF机器人加载、渲染与TF可视化演示程序
 * 
 * 本示例展示了如何使用URDF系统：
 * - 加载URDF文件
 * - 渲染机器人模型
 * - 计算和显示关节TF
 * - IMU接口使用
 */

#include <render/renderer.h>
#include <render/logger.h>
#include <render/camera.h>
#include <render/ecs/world.h>
#include <render/ecs/components.h>
#include <render/ecs/systems.h>
#include <render/ecs/physics/physics_components.h>
#include <render/ecs/physics/physics_system.h>
#include <render/robot/robot_systems.h>
#include <render/robot/robot_components.h>
#include <render/robot/joint_tf_system.h>
#include <render/robot/imu_interface.h>
#include <render/shader_cache.h>
#include <render/material.h>
#include <render/mesh_loader.h>
#include <render/math_utils.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <memory>
#include <unordered_map>

using namespace Render;
using namespace Render::ECS;
using namespace Render::ECS::Robot;

void ConfigureLogger() {
    auto& logger = Logger::GetInstance();
    logger.SetLogToConsole(true);
    logger.SetLogToFile(false);
    logger.SetLogLevel(LogLevel::Info);
}

int main() {
    Logger::GetInstance().Info("[URDFRobotDemo] === URDF Robot Demo ===");
    
    ConfigureLogger();
    
    // 初始化渲染器
    Renderer* renderer = Renderer::Create();
    if (!renderer->Initialize("URDF Robot Demo", 1920, 1080)) {
        Logger::GetInstance().Error("[URDFRobotDemo] Failed to initialize renderer");
        return -1;
    }
    
    renderer->SetClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    renderer->SetVSync(true);
    
    // 启用鼠标相对模式（用于视角控制）
    if (auto context = renderer->GetContext()) {
        SDL_SetWindowRelativeMouseMode(context->GetWindow(), true);
    }
    
    Logger::GetInstance().Info("[URDFRobotDemo] Renderer initialized");
    
    // 创建ECS World
    auto world = std::make_shared<World>();
    
    // 初始化World（必须在注册组件和系统之前调用）
    world->Initialize();
    Logger::GetInstance().Info("[URDFRobotDemo] World initialized");
    
    // 注册组件（必须在注册系统之前）
    world->RegisterComponent<TransformComponent>();
    world->RegisterComponent<MeshRenderComponent>();
    world->RegisterComponent<CameraComponent>();
    world->RegisterComponent<RobotComponent>();
    world->RegisterComponent<LinkComponent>();
    world->RegisterComponent<JointComponent>();
    world->RegisterComponent<LightComponent>();  // 注册光源组件
    // 注册物理组件
    world->RegisterComponent<RigidBodyComponent>();
    world->RegisterComponent<ColliderComponent>();
    world->RegisterComponent<PhysicsWorldComponent>();
    world->RegisterComponent<ConstraintComponent>();  // 注册约束组件
    Logger::GetInstance().Info("[URDFRobotDemo] Components registered");
    
    // 注册系统
    world->RegisterSystem<TransformSystem>();
    
    world->RegisterSystem<CameraSystem>();
    
    // UniformSystem用于设置相机矩阵等uniform变量
    world->RegisterSystem<UniformSystem>(renderer);
    
    // 注册光源系统（必须在UniformSystem之后，MeshRenderSystem之前）
    world->RegisterSystem<LightSystem>(renderer);
    
    // 注册物理系统（必须在MeshRenderSystem之前）
    auto* physicsSystem = world->RegisterSystem<PhysicsSystem>();
    
    world->RegisterSystem<MeshRenderSystem>(renderer);
    
    // 注册机器人相关系统并保存指针
    auto* urdfLoadSystem = world->RegisterSystem<URDFLoadSystem>();
    
    auto* jointTFSystem = world->RegisterSystem<JointTFSystem>();
    
    auto* robotRenderSystem = world->RegisterSystem<RobotRenderSystem>();
    
    Logger::GetInstance().Info("[URDFRobotDemo] Systems registered");
    
    // 后初始化（必须在注册所有系统后调用）
    world->PostInitialize();
    Logger::GetInstance().Info("[URDFRobotDemo] World PostInitialize complete");
    
    // ==================== 创建物理世界 ====================
    EntityID physicsWorldEntity = physicsSystem->CreatePhysicsWorld();
    if (!physicsWorldEntity.IsValid()) {
        Logger::GetInstance().Error("[URDFRobotDemo] Failed to create physics world");
        Renderer::Destroy(renderer);
        return -1;
    }
    Logger::GetInstance().Info("[URDFRobotDemo] Physics world created successfully");
    
    // 创建相机
    EntityID cameraEntity = world->CreateEntity();
    CameraComponent cameraComp;
    cameraComp.camera = std::make_shared<Camera>();
    cameraComp.camera->SetPerspective(60.0f, 1920.0f / 1080.0f, 0.1f, 100.0f);
    cameraComp.active = true;
    world->AddComponent(cameraEntity, cameraComp);
    
    // 相机初始位置和朝向
    Vector3 cameraPosition(3.0f, 2.0f, 3.0f);
    Vector3 cameraTarget(10.0f, 0.6f, 0.0f);  // 看向机器人位置
    TransformComponent cameraTransformComp;
    cameraTransformComp.SetPosition(cameraPosition);
    cameraTransformComp.LookAt(cameraTarget);
    world->AddComponent(cameraEntity, cameraTransformComp);
    
    // 计算初始yaw和pitch（用于主循环中的鼠标控制）
    Vector3 toTarget = (cameraTarget - cameraPosition).normalized();
    float cameraYaw = MathUtils::RadiansToDegrees(std::atan2(toTarget.x(), -toTarget.z()));
    float cameraPitch = MathUtils::RadiansToDegrees(std::asin(std::clamp(toTarget.y(), -1.0f, 1.0f)));
    
    Logger::GetInstance().Info("[URDFRobotDemo] Camera created");
    
    // 加载URDF文件
    // 使用项目中的M20机器人模型
    std::string urdfPath = "models/deep_robotics_model/M20/M20_urdf/urdf/M20.urdf";
    std::string meshBasePath = "models/deep_robotics_model/M20/M20_urdf/";
    
    Logger::GetInstance().InfoFormat("[URDFRobotDemo] Loading URDF: %s", urdfPath.c_str());
    
    EntityID robotEntity = urdfLoadSystem->LoadRobot(urdfPath, meshBasePath);
    if (!robotEntity.IsValid()) {
        Logger::GetInstance().Error("[URDFRobotDemo] Failed to load URDF robot");
        Renderer::Destroy(renderer);
        return -1;
    }
    
    Logger::GetInstance().InfoFormat("[URDFRobotDemo] Robot loaded as entity %u", robotEntity.index);
    
    // 设置机器人的初始姿态（让它站立，而不是侧躺）
    if (world->HasComponent<TransformComponent>(robotEntity)) {
        auto& robotTransform = world->GetComponent<TransformComponent>(robotEntity);
        // 机器人初始位置稍微抬高一点，避免陷入地面
        robotTransform.SetPosition(Vector3(0.0f, 0.6f, 0.0f));
        // 如果需要旋转，可以在这里设置（例如绕X轴旋转-90度让它站立）
        // 根据URDF的坐标系，可能需要调整
        robotTransform.SetRotation(MathUtils::AngleAxis(MathUtils::DegreesToRadians(-90.0f), Vector3::UnitX()));
        Logger::GetInstance().Info("[URDFRobotDemo] Set robot initial position");
    }
    
    // 注意：物理组件现在由URDFLoadSystem自动创建，不需要手动添加
    // 如果需要调整物理参数，可以在URDFLoadSystem创建后修改
    Logger::GetInstance().Info("[URDFRobotDemo] Physics components are automatically created by URDFLoadSystem");
    
    // 创建地面
    Logger::GetInstance().Info("[URDFRobotDemo] Creating ground plane...");
    EntityID groundEntity = world->CreateEntity();
    TransformComponent groundTransform;
    groundTransform.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
    groundTransform.SetScale(10.0f);  // 放大地面
    world->AddComponent(groundEntity, groundTransform);
    
    MeshRenderComponent groundMeshComp;
    groundMeshComp.mesh = MeshLoader::CreatePlane(1.0f, 1.0f, 1, 1, Color::White());
    
    // 使用基础着色器
    auto& shaderCache = ShaderCache::GetInstance();
    auto groundShader = shaderCache.GetShader("basic");
    if (!groundShader) {
        groundShader = shaderCache.LoadShader("basic", "shaders/basic.vert", "shaders/basic.frag");
    }
    
    if (groundShader) {
        auto groundMaterial = std::make_shared<Material>();
        groundMaterial->SetShader(groundShader);
        groundMaterial->SetDiffuseColor(Color(0.5f, 0.5f, 0.5f, 1.0f));  // 灰色地面
        groundMeshComp.material = groundMaterial;
    }
    
    groundMeshComp.visible = true;
    groundMeshComp.resourcesLoaded = true;
    world->AddComponent(groundEntity, groundMeshComp);
    
    // 为地面添加物理组件（静态刚体）
    ColliderComponent groundCollider;
    // 地面是10x10的平面，使用盒子碰撞体，高度很小
    groundCollider.SetBox(Vector3(10.0f, 0.1f, 10.0f));
    world->AddComponent(groundEntity, groundCollider);
    
    RigidBodyComponent groundRigidBody;
    groundRigidBody.type = RigidBodyType::Static;
    groundRigidBody.mass = 0.0f;
    groundRigidBody.friction = 0.8f;  // 较高的摩擦系数
    groundRigidBody.restitution = 0.1f;  // 低弹性
    world->AddComponent(groundEntity, groundRigidBody);
    
    Logger::GetInstance().Info("[URDFRobotDemo] Ground plane created with physics");
    
    // 创建光源
    Logger::GetInstance().Info("[URDFRobotDemo] Creating lights...");
    
    // 创建方向光（太阳光）
    EntityID sunLight = world->CreateEntity();
    TransformComponent sunTransform;
    sunTransform.SetPosition(Vector3(-5.0f, 10.0f, 4.0f));
    sunTransform.transform->LookAt(Vector3(0.0f, 0.0f, 0.0f));
    world->AddComponent(sunLight, sunTransform);
    
    LightComponent sunLightComp;
    sunLightComp.type = LightType::Directional;
    sunLightComp.color = Color(1.0f, 0.97f, 0.9f, 1.0f);  // 温暖的白色
    sunLightComp.intensity = 1.2f;
    sunLightComp.enabled = true;
    world->AddComponent(sunLight, sunLightComp);
    
    Logger::GetInstance().Info("[URDFRobotDemo] Lights created");
    
    // 注意：环境光通过材质的 SetAmbientColor 设置，而不是通过 LightComponent
    // 环境光已经在材质的 Phong 着色器中处理
    
    // 设置一些关节的初始位置（示例）
    if (world->HasComponent<RobotComponent>(robotEntity)) {
        const auto& robotComp = world->GetComponent<RobotComponent>(robotEntity);
        if (robotComp.model) {
            // 设置一些关节位置
            std::unordered_map<std::string, float> jointPositions;
            
            // 示例：设置一些关节的角度
            for (const auto& pair : robotComp.model->joints) {
                const std::string& jointName = pair.first;
                const Render::Robot::URDFJoint& joint = pair.second;
                if (joint.type == Render::Robot::JointType::Revolute) {
                    // 设置到中间位置
                    jointPositions[jointName] = (joint.limits.lower + joint.limits.upper) * 0.5f;
                }
            }
            
            jointTFSystem->SetJointPositions(robotEntity, jointPositions);
            Logger::GetInstance().InfoFormat("[URDFRobotDemo] Set %zu joint positions", jointPositions.size());
        }
    }
    
    // 创建IMU接口（示例）
    Render::Robot::IMUInterface imuInterface;
    Transform initialPose(Vector3::Zero(), Quaternion::Identity(), Vector3::Ones());
    imuInterface.SetInitialPose(initialPose);
    
    // 启用TF可视化
    auto& tfVisualizer = robotRenderSystem->GetTFVisualizer();
    tfVisualizer.SetEnabled(true);
    tfVisualizer.SetShowAxes(true);
    tfVisualizer.SetShowConnections(true);
    tfVisualizer.SetAxisLength(0.1f);
    
    Logger::GetInstance().Info("[URDFRobotDemo] Starting main loop...");
    Logger::GetInstance().Info("[URDFRobotDemo] Controls: ESC to exit");
    Logger::GetInstance().Info("[URDFRobotDemo] Controls: WASD 前后左右, Q/E 上下, Shift 加速");
    Logger::GetInstance().Info("[URDFRobotDemo] Controls: 鼠标视角, Tab 捕获/释放鼠标");
    
    // 主循环
    bool running = true;
    bool mouseCaptured = true;
    float time = 0.0f;
    Uint64 prevTicks = SDL_GetTicks();
    
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            
            // 键盘控制
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
                // Tab键切换鼠标捕获模式
                if (event.key.key == SDLK_TAB) {
                    mouseCaptured = !mouseCaptured;
                    if (auto context = renderer->GetContext()) {
                        SDL_SetWindowRelativeMouseMode(context->GetWindow(), mouseCaptured);
                    }
                }
            }
            
            // 鼠标移动控制视角
            if (mouseCaptured && event.type == SDL_EVENT_MOUSE_MOTION) {
                constexpr float sensitivity = 0.15f;
                cameraYaw -= static_cast<float>(event.motion.xrel) * sensitivity;
                cameraPitch -= static_cast<float>(event.motion.yrel) * sensitivity;
                cameraPitch = std::clamp(cameraPitch, -89.0f, 89.0f);
            }
        }
        
        // 计算deltaTime
        Uint64 currentTicks = SDL_GetTicks();
        float deltaTime = static_cast<float>(currentTicks - prevTicks) / 1000.0f;
        prevTicks = currentTicks;
        deltaTime = std::min(deltaTime, 0.033f);  // 限制最大deltaTime
        time += deltaTime;
        
        // 相机移动控制（WASD + Q/E）
        const bool* keyboard = SDL_GetKeyboardState(nullptr);
        float moveSpeed = (keyboard[SDL_SCANCODE_LSHIFT] ? 6.0f : 3.0f) * deltaTime;
        
        const float yawRad = MathUtils::DegreesToRadians(cameraYaw);
        const float pitchRad = MathUtils::DegreesToRadians(cameraPitch);
        
        Quaternion yawRotation = MathUtils::AngleAxis(yawRad, Vector3::UnitY());
        Quaternion pitchRotation = MathUtils::AngleAxis(pitchRad, Vector3::UnitX());
        Quaternion viewRotation = yawRotation * pitchRotation;
        
        Vector3 front = viewRotation * (-Vector3::UnitZ());
        front.normalize();
        Vector3 right = front.cross(Vector3::UnitY()).normalized();
        Vector3 up = right.cross(front).normalized();
        
        if (keyboard[SDL_SCANCODE_W]) cameraPosition += front * moveSpeed;
        if (keyboard[SDL_SCANCODE_S]) cameraPosition -= front * moveSpeed;
        if (keyboard[SDL_SCANCODE_A]) cameraPosition -= right * moveSpeed;
        if (keyboard[SDL_SCANCODE_D]) cameraPosition += right * moveSpeed;
        if (keyboard[SDL_SCANCODE_Q]) cameraPosition -= Vector3::UnitY() * moveSpeed;
        if (keyboard[SDL_SCANCODE_E]) cameraPosition += Vector3::UnitY() * moveSpeed;
        
        // 更新相机Transform
        auto& cameraTransformComp = world->GetComponent<TransformComponent>(cameraEntity);
        cameraTransformComp.SetPosition(cameraPosition);
        cameraTransformComp.SetRotation(viewRotation);
        
        // 更新IMU（示例：模拟IMU数据）
        Render::Robot::IMUData imuData;
        imuData.acceleration = Vector3(0.0f, 0.0f, -9.81f);  // 重力
        imuData.angularVelocity = Vector3(0.0f, 0.0f, 0.0f);
        imuData.timestamp = time;
        imuInterface.UpdateIMUData(imuData);
        imuInterface.Update(deltaTime);
        
        // 开始渲染
        renderer->BeginFrame();
        renderer->Clear();
        
        // 更新ECS系统（这会自动处理MeshRenderSystem的渲染）
        world->Update(deltaTime);
        
        // TF可视化（在world->Update之后，因为需要TF数据）
        Camera* camera = nullptr;
        if (world->HasComponent<CameraComponent>(cameraEntity)) {
            const auto& cameraComp = world->GetComponent<CameraComponent>(cameraEntity);
            camera = cameraComp.camera.get();
        }
        
        if (camera) {
            // TF可视化通过RobotRenderSystem处理
            robotRenderSystem->RenderRobots(renderer, *camera);
        }
        
        // 刷新渲染队列（MeshRenderSystem已经在Update中提交了Renderable）
        renderer->FlushRenderQueue();
        
        // 结束渲染
        renderer->EndFrame();
        renderer->Present();
        
        // 简单的帧率控制
        SDL_Delay(16);
    }
    
    Logger::GetInstance().Info("[URDFRobotDemo] Shutting down...");
    
    // 清理
    Renderer::Destroy(renderer);
    
    Logger::GetInstance().Info("[URDFRobotDemo] Demo completed");
    
    return 0;
}
