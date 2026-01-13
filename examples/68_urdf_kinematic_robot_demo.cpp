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
 * @file 68_urdf_kinematic_robot_demo.cpp
 * @brief URDF机器人运动学关节控制演示程序
 * 
 * 本示例展示了如何使用运动学关节控制：
 * - 加载URDF文件（使用Kinematic刚体类型）
 * - 运动学关节控制（直接设置角度/角速度，不通过物理模拟）
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
#include <render/robot/robot_control_system.h>
#include <render/robot/imu_interface.h>
#include <render/shader_cache.h>
#include <render/material.h>
#include <render/mesh_loader.h>
#include <render/math_utils.h>
#include <SDL3/SDL.h>
#include <LinearMath/btVector3.h>
#include <BulletCollision/CollisionDispatch/btCollisionObject.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>

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
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] === URDF Kinematic Robot Demo ===");
    
    ConfigureLogger();
    
    // 初始化渲染器
    Renderer* renderer = Renderer::Create();
    if (!renderer->Initialize("URDF Kinematic Robot Demo", 1920, 1080)) {
        Logger::GetInstance().Error("[URDFKinematicRobotDemo] Failed to initialize renderer");
        return -1;
    }
    
    renderer->SetClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    renderer->SetVSync(true);
    
    // 启用鼠标相对模式（用于视角控制）
    if (auto context = renderer->GetContext()) {
        SDL_SetWindowRelativeMouseMode(context->GetWindow(), true);
    }
    
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] Renderer initialized");
    
    // 创建ECS World
    auto world = std::make_shared<World>();
    
    // 初始化World（必须在注册组件和系统之前调用）
    world->Initialize();
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] World initialized");
    
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
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] Components registered");
    
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
    
    auto* robotControlSystem = world->RegisterSystem<RobotControlSystem>();
    
    auto* robotRenderSystem = world->RegisterSystem<RobotRenderSystem>();
    
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] Systems registered");
    
    // 后初始化（必须在注册所有系统后调用）
    world->PostInitialize();
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] World PostInitialize complete");
    
    // ==================== 创建物理世界 ====================
    EntityID physicsWorldEntity = physicsSystem->CreatePhysicsWorld();
    if (!physicsWorldEntity.IsValid()) {
        Logger::GetInstance().Error("[URDFKinematicRobotDemo] Failed to create physics world");
        Renderer::Destroy(renderer);
        return -1;
    }
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] Physics world created successfully");
    
    // 创建相机
    EntityID cameraEntity = world->CreateEntity();
    CameraComponent cameraComp;
    cameraComp.camera = std::make_shared<Camera>();
    cameraComp.camera->SetPerspective(60.0f, 1920.0f / 1080.0f, 0.1f, 100.0f);
    cameraComp.active = true;
    world->AddComponent(cameraEntity, cameraComp);
    
    // 相机初始位置和朝向
    Vector3 cameraPosition(3.0f, 2.0f, 3.0f);
    Vector3 cameraTarget(0.0f, 0.6f, 0.0f);  // 看向机器人位置
    TransformComponent cameraTransformComp;
    cameraTransformComp.SetPosition(cameraPosition);
    cameraTransformComp.LookAt(cameraTarget);
    world->AddComponent(cameraEntity, cameraTransformComp);
    
    // 计算初始yaw和pitch（用于主循环中的鼠标控制）
    Vector3 toTarget = (cameraTarget - cameraPosition).normalized();
    float cameraYaw = MathUtils::RadiansToDegrees(std::atan2(toTarget.x(), -toTarget.z()));
    float cameraPitch = MathUtils::RadiansToDegrees(std::asin(std::clamp(toTarget.y(), -1.0f, 1.0f)));
    
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] Camera created");
    
    // 加载URDF文件
    // 使用项目中的M20机器人模型
    std::string urdfPath = "models/deep_robotics_model/M20/M20_urdf/urdf/M20.urdf";
    std::string meshBasePath = "models/deep_robotics_model/M20/M20_urdf/";
    
    Logger::GetInstance().InfoFormat("[URDFKinematicRobotDemo] Loading URDF: %s", urdfPath.c_str());
    
    // 使用Kinematic刚体类型（运动学模式，不进行物理模拟）
    EntityID robotEntity = urdfLoadSystem->LoadRobot(urdfPath, meshBasePath, RigidBodyType::Kinematic);
    if (!robotEntity.IsValid()) {
        Logger::GetInstance().Error("[URDFKinematicRobotDemo] Failed to load URDF robot");
        Renderer::Destroy(renderer);
        return -1;
    }
    
    Logger::GetInstance().InfoFormat("[URDFKinematicRobotDemo] Robot loaded as entity %u (Kinematic mode)", robotEntity.index);
    
    // 设置机器人的初始姿态（让它站立，而不是侧躺）
    if (world->HasComponent<TransformComponent>(robotEntity)) {
        auto& robotTransform = world->GetComponent<TransformComponent>(robotEntity);
        // 机器人初始位置稍微抬高一点，避免陷入地面
        robotTransform.SetPosition(Vector3(0.0f, 0.6f, 0.0f));
        // 如果需要旋转，可以在这里设置（例如绕X轴旋转-90度让它站立）
        // 根据URDF的坐标系，可能需要调整
        robotTransform.SetRotation(MathUtils::AngleAxis(MathUtils::DegreesToRadians(-90.0f), Vector3::UnitX()));
        Logger::GetInstance().Info("[URDFKinematicRobotDemo] Set robot initial position");
    }
    
    // 重要：将所有link的刚体类型改为Kinematic（LoadRobot只设置了baseLink为Kinematic）
    // 这样机器人就不会与地面发生物理碰撞，完全由运动学控制
    if (world->HasComponent<RobotComponent>(robotEntity)) {
        const auto& robotComp = world->GetComponent<RobotComponent>(robotEntity);
        int kinematicLinkCount = 0;
        
        for (const auto& pair : robotComp.linkEntityMap) {
            const std::string& linkName = pair.first;
            EntityID linkEntity = pair.second;
            if (linkEntity.IsValid() && world->HasComponent<RigidBodyComponent>(linkEntity)) {
                auto& rigidBody = world->GetComponent<RigidBodyComponent>(linkEntity);
                
                // 设置为Kinematic类型
                rigidBody.type = RigidBodyType::Kinematic;
                rigidBody.useGravity = false;
                rigidBody.syncMode = RigidBodyComponent::SyncMode::TransformToPhysics;
                
                // 如果Bullet刚体已经创建，直接更新其标志
                if (rigidBody.bulletRigidBody) {
                    btRigidBody* bulletBody = static_cast<btRigidBody*>(rigidBody.bulletRigidBody);
                    // 清除之前的标志
                    int flags = bulletBody->getCollisionFlags();
                    flags &= ~(btCollisionObject::CF_STATIC_OBJECT | btCollisionObject::CF_KINEMATIC_OBJECT);
                    // 设置Kinematic标志
                    flags |= btCollisionObject::CF_KINEMATIC_OBJECT;
                    bulletBody->setCollisionFlags(flags);
                    // 禁用休眠
                    bulletBody->setActivationState(DISABLE_DEACTIVATION);
                    // 禁用重力
                    bulletBody->setGravity(btVector3(0, 0, 0));
                }
                
                kinematicLinkCount++;
                Logger::GetInstance().InfoFormat("[URDFKinematicRobotDemo] Set link '%s' to Kinematic type", linkName.c_str());
            }
        }
        Logger::GetInstance().InfoFormat("[URDFKinematicRobotDemo] Set %d links to Kinematic type (no physics collision)", kinematicLinkCount);
    }
    
    // 注意：物理组件现在由URDFLoadSystem自动创建
    // 在Kinematic模式下，所有link的刚体类型都改为Kinematic，不进行物理模拟
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] All links configured for Kinematic mode (no physics collision)");
    
    // 创建地面
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] Creating ground plane...");
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
    
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] Ground plane created with physics");
    
    // 创建光源
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] Creating lights...");
    
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
    
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] Lights created");
    
    // 注意：环境光通过材质的 SetAmbientColor 设置，而不是通过 LightComponent
    // 环境光已经在材质的 Phong 着色器中处理
    
    // 设置关节控制（示例）
    std::vector<std::string> controllableJoints;  // 存储可控制的关节名称
    Logger::GetInstance().InfoFormat("[URDFKinematicRobotDemo] Checking robot entity %u for RobotComponent...", robotEntity.index);
    if (world->HasComponent<RobotComponent>(robotEntity)) {
        const auto& robotComp = world->GetComponent<RobotComponent>(robotEntity);
        Logger::GetInstance().InfoFormat("[URDFKinematicRobotDemo] RobotComponent found, model pointer: %p", robotComp.model.get());
        if (robotComp.model) {
            Logger::GetInstance().InfoFormat("[URDFKinematicRobotDemo] Robot model has %zu joints", robotComp.model->joints.size());
            // 设置一些关节位置（用于TF计算）
            std::unordered_map<std::string, float> jointPositions;
            
            // 示例：设置所有关节为运动学控制（Kinematic）
            // 按照URDF文件中的定义顺序遍历关节（通过jointOrder）
            int controlledJointCount = 0;
            int revoluteJointCount = 0;
            int prismaticJointCount = 0;
            int continuousJointCount = 0;
            int fixedJointCount = 0;
            int otherJointCount = 0;
            
            for (const std::string& jointName : robotComp.model->jointOrder) {
                const auto& jointIt = robotComp.model->joints.find(jointName);
                if (jointIt == robotComp.model->joints.end()) continue;
                const Render::Robot::URDFJoint& joint = jointIt->second;
                
                // 获取关节实体并验证
                EntityID jointEntity = robotControlSystem->GetJointEntity(robotEntity, jointName);
                if (!jointEntity.IsValid()) {
                    Logger::GetInstance().WarningFormat("[URDFKinematicRobotDemo] Joint '%s' entity not found, skipping", 
                        jointName.c_str());
                    continue;
                }
                
                if (!world->HasComponent<JointComponent>(jointEntity)) {
                    Logger::GetInstance().WarningFormat("[URDFKinematicRobotDemo] Joint '%s' has no JointComponent, skipping", 
                        jointName.c_str());
                    continue;
                }
                
                // 设置所有关节为Kinematic模式
                robotControlSystem->SetJointControlMode(robotEntity, jointName, 
                                                       JointComponent::ControlMode::Kinematic);
                
                // 验证设置是否成功
                auto& jointComp = world->GetComponent<JointComponent>(jointEntity);
                if (jointComp.controlMode != JointComponent::ControlMode::Kinematic) {
                    Logger::GetInstance().ErrorFormat("[URDFKinematicRobotDemo] Failed to set joint '%s' to Kinematic mode! Current mode: %d", 
                        jointName.c_str(), static_cast<int>(jointComp.controlMode));
                } else {
                    Logger::GetInstance().InfoFormat("[URDFKinematicRobotDemo] Successfully set joint '%s' to Kinematic mode", 
                        jointName.c_str());
                }
                
                if (joint.type == Render::Robot::JointType::Revolute) {
                    revoluteJointCount++;
                    // 设置到中间位置
                    float midPosition = (joint.limits.lower + joint.limits.upper) * 0.5f;
                    jointPositions[jointName] = midPosition;
                    robotControlSystem->SetJointTargetPosition(robotEntity, jointName, midPosition);
                    
                    // 保存可控制的关节名称（用于键盘控制）
                    controllableJoints.push_back(jointName);
                    controlledJointCount++;
                } else if (joint.type == Render::Robot::JointType::Continuous) {
                    continuousJointCount++;
                    // Continuous关节没有位置限制，初始位置设为0
                    jointPositions[jointName] = 0.0f;
                    robotControlSystem->SetJointTargetVelocity(robotEntity, jointName, 0.0f);  // 初始速度为0
                    
                    // 保存可控制的关节名称（用于键盘控制）
                    controllableJoints.push_back(jointName);
                    controlledJointCount++;
                } else if (joint.type == Render::Robot::JointType::Prismatic) {
                    prismaticJointCount++;
                    // Prismatic关节（平移关节）：设置到中间位置
                    float midPosition = (joint.limits.lower + joint.limits.upper) * 0.5f;
                    jointPositions[jointName] = midPosition;
                    robotControlSystem->SetJointTargetPosition(robotEntity, jointName, midPosition);
                    
                    // 保存可控制的关节名称（用于键盘控制）
                    controllableJoints.push_back(jointName);
                    controlledJointCount++;
                    
                    Logger::GetInstance().InfoFormat("[URDFKinematicRobotDemo] Configured Prismatic joint '%s' for kinematic control", 
                        jointName.c_str());
                } else if (joint.type == Render::Robot::JointType::Fixed) {
                    fixedJointCount++;
                    // Fixed关节不需要控制，但也要设置为Kinematic模式（虽然不会移动）
                    jointPositions[jointName] = 0.0f;
                    Logger::GetInstance().InfoFormat("[URDFKinematicRobotDemo] Fixed joint '%s' set to Kinematic mode (no control needed)", 
                        jointName.c_str());
                } else {
                    otherJointCount++;
                    Logger::GetInstance().InfoFormat("[URDFKinematicRobotDemo] Joint '%s' type is %d (not handled for control)", 
                        jointName.c_str(), static_cast<int>(joint.type));
                }
            }
            
            Logger::GetInstance().InfoFormat("[URDFKinematicRobotDemo] Joint type summary: %d Revolute, %d Continuous, %d Prismatic, %d Fixed, %d other types", 
                revoluteJointCount, continuousJointCount, prismaticJointCount, fixedJointCount, otherJointCount);
            
            jointTFSystem->SetJointPositions(robotEntity, jointPositions);
            Logger::GetInstance().InfoFormat("[URDFKinematicRobotDemo] Set %zu joint positions for TF calculation", 
                jointPositions.size());
            Logger::GetInstance().InfoFormat("[URDFKinematicRobotDemo] Configured %d joints for kinematic control", 
                controlledJointCount);
            Logger::GetInstance().InfoFormat("[URDFKinematicRobotDemo] Controllable joints count: %zu", 
                controllableJoints.size());
            if (!controllableJoints.empty()) {
                Logger::GetInstance().InfoFormat("[URDFKinematicRobotDemo] First controllable joint: %s", 
                    controllableJoints[0].c_str());
            } else {
                Logger::GetInstance().Warning("[URDFKinematicRobotDemo] No controllable joints found! Check if robot has Revolute joints.");
            }
        } else {
            Logger::GetInstance().Error("[URDFKinematicRobotDemo] RobotComponent.model is null!");
        }
    } else {
        Logger::GetInstance().ErrorFormat("[URDFKinematicRobotDemo] Robot entity %u does not have RobotComponent!", robotEntity.index);
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
    
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] Starting main loop...");
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] === Camera Controls ===");
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] ESC: 退出");
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] WASD: 前后左右移动, Q/E: 上下移动, Shift: 加速");
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] 鼠标: 视角控制, Tab: 捕获/释放鼠标");
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] === Kinematic Joint Controls ===");
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] 1-9: 直接设置前9个关节角度 (增加角度)");
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] Ctrl+1-9: 直接设置前9个关节角度 (减少角度)");
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] R: 切换为位置控制（直接设置角度）");
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] T: 切换为速度控制（设置角速度，通过积分计算位置）");
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] Space: 启用/禁用自动动画演示");
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] 注意：运动学控制不通过物理模拟，响应更快更平滑");
    
    // 控制状态
    bool autoAnimationEnabled = true;  // 自动动画演示
    JointComponent::ControlMode currentControlMode = JointComponent::ControlMode::Kinematic;  // 默认使用运动学模式
    int selectedJointIndex = 0;  // 当前选中的关节索引
    
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
                // 调试：输出所有按键（仅限字母键）
                if ((event.key.key >= 'a' && event.key.key <= 'z') || 
                    (event.key.key >= 'A' && event.key.key <= 'Z')) {
                    Logger::GetInstance().InfoFormat(
                        "[URDFKinematicRobotDemo] Key pressed: keycode=0x%08X ('%c'), scancode=%d",
                        event.key.key, static_cast<char>(event.key.key), event.key.scancode
                    );
                }
                
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
                
                // 关节控制
                if (!controllableJoints.empty()) {
                    // 获取当前键盘状态（用于检查Ctrl键）
                    const bool* keyboardState = SDL_GetKeyboardState(nullptr);
                    
                    // 数字键1-9控制关节位置
                    if (event.key.key >= SDLK_1 && event.key.key <= SDLK_9) {
                        int jointIndex = static_cast<int>(event.key.key - SDLK_1);
                        if (jointIndex >= 0 && jointIndex < static_cast<int>(controllableJoints.size())) {
                            const std::string& jointName = controllableJoints[jointIndex];
                            
                            Logger::GetInstance().InfoFormat(
                                "[URDFKinematicRobotDemo] Keyboard %d pressed, controlling joint index %d: '%s'",
                                jointIndex + 1, jointIndex, jointName.c_str()
                            );
                            
                            // 检查是否按住Ctrl（减少角度）
                            bool decrease = (keyboardState[SDL_SCANCODE_LCTRL] || keyboardState[SDL_SCANCODE_RCTRL]);
                            
                            // 获取当前目标位置
                            float currentTarget = robotControlSystem->GetJointPosition(robotEntity, jointName);
                            
                            // 获取关节限制
                            EntityID jointEntity = robotControlSystem->GetJointEntity(robotEntity, jointName);
                            if (jointEntity.IsValid() && world->HasComponent<JointComponent>(jointEntity)) {
                                const auto& jointComp = world->GetComponent<JointComponent>(jointEntity);
                                
                                // 检查是否为Continuous关节（轮关节）
                                bool isContinuous = (jointComp.type == Render::Robot::JointType::Continuous);
                                
                                float step = 0.0f;
                                if (isContinuous) {
                                    // Continuous关节使用固定步长（0.1弧度）
                                    step = 0.1f;
                                } else {
                                    // Revolute关节使用范围的10%作为步长
                                    if (jointComp.limits.lower < jointComp.limits.upper) {
                                        step = (jointComp.limits.upper - jointComp.limits.lower) * 0.1f;
                                    } else {
                                        // 如果limits无效，使用固定步长
                                        step = 0.1f;
                                    }
                                }
                                
                                float newTarget = currentTarget + (decrease ? -step : step);
                                
                                // 限制在关节范围内（仅对Revolute关节）
                                if (!isContinuous && jointComp.limits.lower < jointComp.limits.upper) {
                                    newTarget = std::clamp(newTarget, jointComp.limits.lower, jointComp.limits.upper);
                                }
                                // Continuous关节不受限制，可以无限旋转
                                
                                robotControlSystem->SetJointTargetPosition(robotEntity, jointName, newTarget);
                                
                                Logger::GetInstance().InfoFormat(
                                    "[URDFKinematicRobotDemo] Joint '%s' (type=%d) target position: %.3f rad (%.1f deg), step=%.3f",
                                    jointName.c_str(), static_cast<int>(jointComp.type), 
                                    newTarget, MathUtils::RadiansToDegrees(newTarget), step
                                );
                            } else {
                                Logger::GetInstance().WarningFormat(
                                    "[URDFKinematicRobotDemo] Joint entity for '%s' is invalid or missing JointComponent",
                                    jointName.c_str()
                                );
                            }
                        } else {
                            Logger::GetInstance().WarningFormat(
                                "[URDFKinematicRobotDemo] Keyboard %d pressed, but joint index %d is out of range (controllableJoints.size()=%zu)",
                                jointIndex + 1, jointIndex, controllableJoints.size()
                            );
                        }
                    }
                    
                    // 控制模式切换（Kinematic模式下的位置和速度控制）
                    // SDL3中SDLK_R实际上是'r'的Unicode值(0x72)，SDLK_T是't'(0x74)
                    // 注意：SDL3的keycode是Unicode值，小写字母的Unicode值就是SDLK_*的值
                    if (event.key.key == SDLK_R || event.key.key == 'r' || event.key.key == 'R') {
                        // 切换到Kinematic位置控制（直接设置角度）
                        currentControlMode = JointComponent::ControlMode::Kinematic;
                        for (const auto& jointName : controllableJoints) {
                            robotControlSystem->SetJointControlMode(robotEntity, jointName, currentControlMode);
                            // 清除速度，使用位置控制
                            robotControlSystem->SetJointTargetVelocity(robotEntity, jointName, 0.0f);
                        }
                        Logger::GetInstance().InfoFormat(
                            "[URDFKinematicRobotDemo] Switched to Kinematic Position Control (keycode=0x%08X, %zu joints)",
                            event.key.key, controllableJoints.size()
                        );
                    } else if (event.key.key == SDLK_T || event.key.key == 't' || event.key.key == 'T') {
                        // 切换到Kinematic速度控制（设置角速度，通过积分计算位置）
                        currentControlMode = JointComponent::ControlMode::Kinematic;
                        for (const auto& jointName : controllableJoints) {
                            robotControlSystem->SetJointControlMode(robotEntity, jointName, currentControlMode);
                            // 设置初始目标速度（如果之前是位置控制，速度可能为0）
                            // 这里可以设置一个默认速度，或者保持当前速度
                            float currentVel = robotControlSystem->GetJointVelocity(robotEntity, jointName);
                            robotControlSystem->SetJointTargetVelocity(robotEntity, jointName, currentVel);
                        }
                        Logger::GetInstance().InfoFormat(
                            "[URDFKinematicRobotDemo] Switched to Kinematic Velocity Control (keycode=0x%08X, %zu joints)",
                            event.key.key, controllableJoints.size()
                        );
                    }
                    
                    // 空格键切换自动动画
                    if (event.key.key == SDLK_SPACE) {
                        autoAnimationEnabled = !autoAnimationEnabled;
                        Logger::GetInstance().InfoFormat(
                            "[URDFKinematicRobotDemo] Auto animation: %s",
                            autoAnimationEnabled ? "ENABLED" : "DISABLED"
                        );
                    }
                } else {
                    // 如果controllableJoints为空，输出调试信息
                    if (event.key.key == SDLK_R || event.key.key == 'r' || event.key.key == 'R' ||
                        event.key.key == SDLK_T || event.key.key == 't' || event.key.key == 'T') {
                        Logger::GetInstance().WarningFormat(
                            "[URDFKinematicRobotDemo] Control mode key pressed (0x%08X) but no controllable joints! controllableJoints.size()=%zu",
                            event.key.key, controllableJoints.size()
                        );
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
        
        // 自动动画演示（Kinematic模式）
        if (autoAnimationEnabled && !controllableJoints.empty() && 
            currentControlMode == JointComponent::ControlMode::Kinematic) {
            // 检查当前控制方式：通过检查关节的targetVelocity是否非零来判断
            // 如果targetVelocity非零，使用速度控制；否则使用位置控制
            bool useVelocityControl = false;
            if (!controllableJoints.empty()) {
                EntityID firstJointEntity = robotControlSystem->GetJointEntity(robotEntity, controllableJoints[0]);
                if (firstJointEntity.IsValid() && world->HasComponent<JointComponent>(firstJointEntity)) {
                    const auto& jointComp = world->GetComponent<JointComponent>(firstJointEntity);
                    useVelocityControl = (std::abs(jointComp.targetVelocity) > 1e-6f);
                }
            }
            
            if (useVelocityControl) {
                // Kinematic速度控制：设置角速度，通过积分计算位置
                for (size_t i = 0; i < controllableJoints.size() && i < 6; ++i) {
                    const std::string& jointName = controllableJoints[i];
                    
                    // 获取关节类型
                    EntityID jointEntity = robotControlSystem->GetJointEntity(robotEntity, jointName);
                    if (jointEntity.IsValid() && world->HasComponent<JointComponent>(jointEntity)) {
                        const auto& jointComp = world->GetComponent<JointComponent>(jointEntity);
                        bool isContinuous = (jointComp.type == Render::Robot::JointType::Continuous);
                        
                        if (isContinuous) {
                            // Continuous关节：使用恒定速度或正弦波速度
                            float targetVelocity = 1.0f + 0.5f * std::sin(time * 0.3f + static_cast<float>(i) * 1.0f);
                            robotControlSystem->SetJointTargetVelocity(robotEntity, jointName, targetVelocity);
                        } else {
                            // Revolute关节：使用正弦波生成目标速度
                            float targetVelocity = 0.5f * std::sin(time * 0.3f + static_cast<float>(i) * 1.0f);
                            robotControlSystem->SetJointTargetVelocity(robotEntity, jointName, targetVelocity);
                        }
                    }
                }
            } else {
                // Kinematic位置控制：直接设置角度
                for (size_t i = 0; i < controllableJoints.size() && i < 6; ++i) {
                    const std::string& jointName = controllableJoints[i];
                    
                    // 获取关节限制
                    EntityID jointEntity = robotControlSystem->GetJointEntity(robotEntity, jointName);
                    if (jointEntity.IsValid() && world->HasComponent<JointComponent>(jointEntity)) {
                        const auto& jointComp = world->GetComponent<JointComponent>(jointEntity);
                        
                        // 检查是否为Continuous关节
                        bool isContinuous = (jointComp.type == Render::Robot::JointType::Continuous);
                        
                        if (isContinuous) {
                            // Continuous关节：使用连续旋转
                            float targetPosition = time * 0.5f + static_cast<float>(i) * 1.0f;  // 连续旋转
                            robotControlSystem->SetJointTargetPosition(robotEntity, jointName, targetPosition);
                        } else if (jointComp.limits.lower < jointComp.limits.upper) {
                            // Revolute关节：使用正弦波生成平滑的关节运动
                            float range = (jointComp.limits.upper - jointComp.limits.lower) * 0.5f;  // 50%范围
                            float center = (jointComp.limits.lower + jointComp.limits.upper) * 0.5f;
                            float phase = time * 0.5f + static_cast<float>(i) * 0.8f;  // 不同相位
                            float targetPosition = center + range * std::sin(phase);
                            
                            robotControlSystem->SetJointTargetPosition(robotEntity, jointName, targetPosition);
                        }
                    }
                }
            }
        }
        
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
    
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] Shutting down...");
    
    // 清理
    Renderer::Destroy(renderer);
    
    Logger::GetInstance().Info("[URDFKinematicRobotDemo] Demo completed");
    
    return 0;
}
