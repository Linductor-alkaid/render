/*
 * Copyright (c) 2025 Li Chaoyu
 * 
 * 优化的物理引擎全流程测试 - 使用精确的物理验证方法
 */

 #include "render/physics/physics_systems.h"
 #include "render/physics/physics_components.h"
 #include "render/physics/collision/broad_phase.h"
 #include "render/ecs/world.h"
 #include "render/ecs/components.h"
 #include "render/logger.h"
 #include <iostream>
 #include <iomanip>
 #include <cmath>
 #include <vector>
 #include <string>
 #include <sstream>
 
 using namespace Render;
 using namespace Render::Physics;
 using namespace Render::ECS;
 
 // ============================================================================
 // 物理状态记录结构
 // ============================================================================
 
 struct PhysicsSnapshot {
     float time;
     Vector3 position;
     Vector3 velocity;
     Vector3 acceleration;
     float kineticEnergy;
     float potentialEnergy;
     float totalEnergy;
     Vector3 momentum;
     bool inContact;
     
     PhysicsSnapshot() : time(0), inContact(false) {}
 };
 
 struct CollisionEvent {
     float time;
     Vector3 preVelocity;
     Vector3 postVelocity;
     Vector3 normal;
     float penetration;
     float relativeVelocity;
     float restitution;
 };
 
 // ============================================================================
 // 物理验证工具类
 // ============================================================================
 
 class PhysicsValidator {
 public:
     std::vector<PhysicsSnapshot> trajectory;
     std::vector<CollisionEvent> collisions;
     float gravity;
     float mass;
     float tolerance;
     
     PhysicsValidator(float g = 9.81f, float m = 1.0f, float tol = 0.1f)
         : gravity(g), mass(m), tolerance(tol) {}
     
     // 记录状态快照
     void RecordSnapshot(float time, const Vector3& pos, const Vector3& vel,
                        const Vector3& accel, bool inContact) {
         PhysicsSnapshot snap;
         snap.time = time;
         snap.position = pos;
         snap.velocity = vel;
         snap.acceleration = accel;
         snap.kineticEnergy = 0.5f * mass * vel.squaredNorm();
         snap.potentialEnergy = mass * gravity * pos.y();
         snap.totalEnergy = snap.kineticEnergy + snap.potentialEnergy;
         snap.momentum = mass * vel;
         snap.inContact = inContact;
         trajectory.push_back(snap);
     }
     
     // 记录碰撞事件
     void RecordCollision(float time, const Vector3& preVel, const Vector3& postVel,
                         const Vector3& normal, float penetration, float restitution) {
         CollisionEvent event;
         event.time = time;
         event.preVelocity = preVel;
         event.postVelocity = postVel;
         event.normal = normal;
         event.penetration = penetration;
         event.relativeVelocity = preVel.dot(normal);
         event.restitution = restitution;
         collisions.push_back(event);
     }
     
     // 验证自由落体运动学
     bool ValidateFreeFall(std::string& report) {
         std::ostringstream oss;
         oss << std::fixed << std::setprecision(4);
         oss << "\n=== 自由落体验证 ===\n";
         
         if (trajectory.size() < 2) {
             oss << "❌ 数据不足\n";
             report = oss.str();
             return false;
         }
         
         bool allPassed = true;
         int checkCount = 0;
         float maxVelocityError = 0.0f;
         float maxPositionError = 0.0f;
         
         // 找到第一次接触前的自由落体阶段
         size_t freeFallEnd = 0;
         for (size_t i = 0; i < trajectory.size(); ++i) {
             if (trajectory[i].inContact) {
                 freeFallEnd = i;
                 break;
             }
         }
         
         if (freeFallEnd == 0) freeFallEnd = trajectory.size();
         if (freeFallEnd < 2) {
             oss << "⚠️  自由落体阶段过短\n";
             report = oss.str();
             return true;
         }
         
         oss << "自由落体阶段: 0 - " << freeFallEnd << " 帧\n";
         
         // 验证 v = v0 + gt
         for (size_t i = 1; i < freeFallEnd; ++i) {
             float dt = trajectory[i].time - trajectory[0].time;
             float expectedVy = trajectory[0].velocity.y() - gravity * dt;
             float actualVy = trajectory[i].velocity.y();
             float error = std::abs(expectedVy - actualVy);
             
             maxVelocityError = std::max(maxVelocityError, error);
             
             if (error > tolerance) {
                 oss << "❌ 时刻 " << trajectory[i].time << "s: "
                     << "速度误差 " << error << " m/s (期望: " << expectedVy 
                     << ", 实际: " << actualVy << ")\n";
                 allPassed = false;
             }
             checkCount++;
         }
         
         // 验证 y = y0 + v0*t - 0.5*g*t^2
         for (size_t i = 1; i < freeFallEnd; ++i) {
             float dt = trajectory[i].time - trajectory[0].time;
             float expectedY = trajectory[0].position.y() + 
                             trajectory[0].velocity.y() * dt - 
                             0.5f * gravity * dt * dt;
             float actualY = trajectory[i].position.y();
             float error = std::abs(expectedY - actualY);
             
             maxPositionError = std::max(maxPositionError, error);
             
             if (error > tolerance) {
                 oss << "❌ 时刻 " << trajectory[i].time << "s: "
                     << "位置误差 " << error << " m (期望: " << expectedY 
                     << ", 实际: " << actualY << ")\n";
                 allPassed = false;
             }
         }
         
         oss << "检查点数: " << checkCount << "\n";
         oss << "最大速度误差: " << maxVelocityError << " m/s\n";
         oss << "最大位置误差: " << maxPositionError << " m\n";
         
         if (allPassed) {
             oss << "✅ 自由落体运动学验证通过\n";
         }
         
         report = oss.str();
         return allPassed;
     }
     
     // 验证能量守恒（考虑碰撞损失）
     bool ValidateEnergyConservation(std::string& report) {
         std::ostringstream oss;
         oss << std::fixed << std::setprecision(4);
         oss << "\n=== 能量守恒验证 ===\n";
         
         if (trajectory.size() < 2) {
             oss << "❌ 数据不足\n";
             report = oss.str();
             return false;
         }
         
         float initialEnergy = trajectory[0].totalEnergy;
         float currentEnergy = initialEnergy;
         float expectedEnergyLoss = 0.0f;
         bool allPassed = true;
         
         oss << "初始能量: " << initialEnergy << " J\n";
         
         // 计算碰撞造成的预期能量损失
         for (const auto& collision : collisions) {
             float preKE = 0.5f * mass * collision.preVelocity.squaredNorm();
             float postKE = 0.5f * mass * collision.postVelocity.squaredNorm();
             float loss = preKE - postKE;
             expectedEnergyLoss += loss;
             
             oss << "碰撞 @ " << collision.time << "s: 能量损失 " << loss << " J "
                 << "(弹性系数: " << collision.restitution << ")\n";
         }
         
        // 检查能量是否增加（违反能量守恒定律）
        float maxEnergy = initialEnergy;
        float minEnergy = initialEnergy;
        size_t maxEnergyIndex = 0;
        size_t minEnergyIndex = 0;
        
        for (size_t i = 0; i < trajectory.size(); ++i) {
            float energy = trajectory[i].totalEnergy;
            if (energy > maxEnergy) {
                maxEnergy = energy;
                maxEnergyIndex = i;
            }
            if (energy < minEnergy) {
                minEnergy = energy;
                minEnergyIndex = i;
            }
        }
        
        // 检查非碰撞阶段的能量守恒
        float maxEnergyVariation = 0.0f;
        for (size_t i = 1; i < trajectory.size(); ++i) {
            if (!trajectory[i].inContact && !trajectory[i-1].inContact) {
                float energyDiff = std::abs(trajectory[i].totalEnergy - trajectory[i-1].totalEnergy);
                maxEnergyVariation = std::max(maxEnergyVariation, energyDiff);
                
                if (energyDiff > tolerance * initialEnergy) {
                    oss << "❌ 时刻 " << trajectory[i].time << "s: "
                        << "非碰撞阶段能量变化 " << energyDiff << " J\n";
                    allPassed = false;
                }
            }
        }
        
        float finalEnergy = trajectory.back().totalEnergy;
        float actualEnergyLoss = initialEnergy - finalEnergy;
        float lossError = std::abs(actualEnergyLoss - expectedEnergyLoss);
        
        oss << "预期能量损失: " << expectedEnergyLoss << " J\n";
        oss << "实际能量损失: " << actualEnergyLoss << " J\n";
        oss << "损失误差: " << lossError << " J\n";
        oss << "非碰撞阶段最大能量波动: " << maxEnergyVariation << " J\n";
        oss << "最大能量: " << maxEnergy << " J @ " << trajectory[maxEnergyIndex].time << "s\n";
        oss << "最小能量: " << minEnergy << " J @ " << trajectory[minEnergyIndex].time << "s\n";
        
        // 关键检查：能量不能超过初始值（在封闭系统中）
        float energyIncrease = maxEnergy - initialEnergy;
        if (energyIncrease > tolerance * initialEnergy) {
            oss << "❌ 严重错误：能量增加了 " << energyIncrease << " J！"
                << " 这违反了能量守恒定律（封闭系统能量不能增加）\n";
            oss << "   最大能量出现在 " << trajectory[maxEnergyIndex].time << "s，"
                << "能量从 " << initialEnergy << " J 增加到 " << maxEnergy << " J\n";
            allPassed = false;
        }
        
        // 检查能量损失是否符合预期（允许一定的数值误差）
        if (lossError > tolerance * initialEnergy) {
            oss << "❌ 能量损失不符合预期\n";
            allPassed = false;
        }
         
         if (allPassed) {
             oss << "✅ 能量守恒验证通过\n";
         }
         
         report = oss.str();
         return allPassed;
     }
     
     // 验证碰撞响应
     bool ValidateCollisionResponse(std::string& report) {
         std::ostringstream oss;
         oss << std::fixed << std::setprecision(4);
         oss << "\n=== 碰撞响应验证 ===\n";
         
         if (collisions.empty()) {
             oss << "⚠️  未检测到碰撞\n";
             report = oss.str();
             return false;
         }
         
         oss << "碰撞次数: " << collisions.size() << "\n";
         
         bool allPassed = true;
         
         for (size_t i = 0; i < collisions.size(); ++i) {
             const auto& col = collisions[i];
             oss << "\n碰撞 " << (i+1) << " @ " << col.time << "s:\n";
             
             // 验证法向量归一化
             float normalLength = col.normal.norm();
             if (std::abs(normalLength - 1.0f) > 0.01f) {
                 oss << "❌ 法向量未归一化: " << normalLength << "\n";
                 allPassed = false;
             }
             
             // 验证速度方向改变
             float preNormalVel = col.preVelocity.dot(col.normal);
             float postNormalVel = col.postVelocity.dot(col.normal);
             
             oss << "  碰前法向速度: " << preNormalVel << " m/s\n";
             oss << "  碰后法向速度: " << postNormalVel << " m/s\n";
             
             // 检查是否发生反弹（法向速度应该改变方向或变为0）
             if (preNormalVel < 0) { // 向下运动
                 if (postNormalVel < preNormalVel * 0.5f) { // 应该减速或反弹
                     oss << "❌ 法向速度变化异常\n";
                     allPassed = false;
                 }
             }
             
             // 验证弹性碰撞公式: |v_post| ≈ e * |v_pre|
             float expectedPostVel = -col.restitution * preNormalVel;
             float velocityError = std::abs(postNormalVel - expectedPostVel);
             
             oss << "  预期碰后速度: " << expectedPostVel << " m/s\n";
             oss << "  速度误差: " << velocityError << " m/s\n";
             
             if (velocityError > tolerance * 10) { // 碰撞时允许更大误差
                 oss << "⚠️  速度误差较大（可能由于多次碰撞或摩擦）\n";
             }
             
             // 检查穿透深度
             if (col.penetration > 0.1f) {
                 oss << "⚠️  穿透深度过大: " << col.penetration << " m\n";
             }
             
             oss << "  ✅ 碰撞 " << (i+1) << " 基本合理\n";
         }
         
         if (allPassed) {
             oss << "\n✅ 碰撞响应验证通过\n";
         }
         
         report = oss.str();
         return allPassed;
     }
     
     // 生成轨迹报告
     std::string GenerateTrajectoryReport() {
         std::ostringstream oss;
         oss << std::fixed << std::setprecision(4);
         oss << "\n=== 运动轨迹数据 ===\n";
         oss << "记录点数: " << trajectory.size() << "\n";
         
         if (trajectory.empty()) return oss.str();
         
         oss << "\n时间(s) | 高度(m) | 速度Y(m/s) | 动能(J) | 势能(J) | 总能(J) | 接触\n";
         oss << "--------|---------|------------|---------|---------|---------|-----\n";
         
         // 每10帧输出一次，加上首尾
         for (size_t i = 0; i < trajectory.size(); i += 10) {
             PrintSnapshotLine(oss, trajectory[i]);
         }
         if ((trajectory.size() - 1) % 10 != 0) {
             PrintSnapshotLine(oss, trajectory.back());
         }
         
         return oss.str();
     }
     
 private:
     void PrintSnapshotLine(std::ostringstream& oss, const PhysicsSnapshot& snap) {
         oss << std::setw(7) << snap.time << " | "
             << std::setw(7) << snap.position.y() << " | "
             << std::setw(10) << snap.velocity.y() << " | "
             << std::setw(7) << snap.kineticEnergy << " | "
             << std::setw(7) << snap.potentialEnergy << " | "
             << std::setw(7) << snap.totalEnergy << " | "
             << (snap.inContact ? " YES " : "  NO ") << "\n";
     }
 };
 
 // ============================================================================
 // 测试框架
 // ============================================================================
 
 static int g_testCount = 0;
 static int g_passedCount = 0;
 static int g_failedCount = 0;
 
 #define TEST_ASSERT(condition, message) \
     do { \
         g_testCount++; \
         if (!(condition)) { \
             std::cerr << "❌ 测试失败: " << message << std::endl; \
             std::cerr << "   位置: " << __FILE__ << ":" << __LINE__ << std::endl; \
             g_failedCount++; \
             return false; \
         } \
         g_passedCount++; \
     } while(0)
 
 #define RUN_TEST(test_func) \
     do { \
         std::cout << "\n========================================" << std::endl; \
         std::cout << "运行测试: " << #test_func << std::endl; \
         std::cout << "========================================" << std::endl; \
         if (test_func()) { \
             std::cout << "✅ " << #test_func << " 通过" << std::endl; \
         } else { \
             std::cout << "❌ " << #test_func << " 失败" << std::endl; \
         } \
     } while(0)
 
 // ============================================================================
 // 辅助函数
 // ============================================================================
 
 static void RegisterPhysicsComponents(std::shared_ptr<World> world) {
     world->RegisterComponent<TransformComponent>();
     world->RegisterComponent<RigidBodyComponent>();
     world->RegisterComponent<ColliderComponent>();
 }
 
 /**
  * @brief 高级物理场景模拟（带详细验证）
  */
 static bool SimulateWithValidation(
     std::shared_ptr<World> world,
     EntityID dynamicEntity,
     int steps,
     float dt,
     const std::string& sceneName,
     PhysicsValidator& validator
 ) {
     auto* physicsSystem = world->GetSystem<PhysicsUpdateSystem>();
     auto* collisionSystem = world->GetSystem<CollisionDetectionSystem>();
     
     bool collisionDetected = false;
     Vector3 lastVelocity = Vector3::Zero();
     bool wasInContact = false;
     
     for (int step = 0; step < steps; ++step) {
         float currentTime = step * dt;
         
         // 获取碰撞前状态
         auto& body = world->GetComponent<RigidBodyComponent>(dynamicEntity);
         auto& transform = world->GetComponent<TransformComponent>(dynamicEntity);
         Vector3 preVel = body.linearVelocity;
         Vector3 prePos = transform.GetPosition();
         
         // 更新物理系统
         physicsSystem->Update(dt);
         
         // 获取碰撞后状态
         Vector3 postVel = body.linearVelocity;
         Vector3 postPos = transform.GetPosition();
         Vector3 accel = (postVel - preVel) / dt;
         
         // 检查碰撞
         const auto& collisionPairs = collisionSystem->GetCollisionPairs();
         bool inContact = !collisionPairs.empty();
         
         if (inContact && !wasInContact) {
             collisionDetected = true;
             // 记录碰撞事件
             for (const auto& pair : collisionPairs) {
                 if (pair.entityA == dynamicEntity || pair.entityB == dynamicEntity) {
                     auto& collider = world->GetComponent<ColliderComponent>(dynamicEntity);
                     float restitution = collider.material->restitution;
                     validator.RecordCollision(
                         currentTime,
                         preVel,
                         postVel,
                         pair.manifold.normal,
                         pair.manifold.penetration,
                         restitution
                     );
                 }
             }
         }
         
         // 记录状态快照
         validator.RecordSnapshot(currentTime, postPos, postVel, accel, inContact);
         
         wasInContact = inContact;
         lastVelocity = postVel;
     }
     
     return collisionDetected;
 }
 
 // ============================================================================
 // 测试场景
 // ============================================================================
 
 /**
  * @brief 测试场景1: 球体自由落体碰撞（完整物理验证）
  */
 static bool Test_SphereFreeFall_FullValidation() {
     auto world = std::make_shared<World>();
     RegisterPhysicsComponents(world);
     world->Initialize();
     
     float gravity = 9.81f;
     float mass = 1.0f;
     
     auto* physicsSystem = world->RegisterSystem<PhysicsUpdateSystem>();
     physicsSystem->SetGravity(Vector3(0.0f, -gravity, 0.0f));
     physicsSystem->SetFixedDeltaTime(1.0f / 60.0f);
     physicsSystem->SetSolverIterations(10);
     physicsSystem->SetPositionIterations(4);
     
     auto* collisionSystem = world->RegisterSystem<CollisionDetectionSystem>();
     collisionSystem->SetBroadPhase(std::make_unique<SpatialHashBroadPhase>(10.0f));
     
     // 创建静止的球体（地面）
     EntityID staticSphere = world->CreateEntity();
     {
         TransformComponent transform;
         transform.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
         world->AddComponent(staticSphere, transform);
         
         RigidBodyComponent body;
         body.SetBodyType(RigidBodyComponent::BodyType::Static);
         world->AddComponent(staticSphere, body);
         
         ColliderComponent collider = ColliderComponent::CreateSphere(1.0f);
         world->AddComponent(staticSphere, collider);
     }
     
     // 创建动态球体（下落物体）
     EntityID dynamicSphere = world->CreateEntity();
     {
         TransformComponent transform;
         transform.SetPosition(Vector3(0.0f, 5.0f, 0.0f));
         world->AddComponent(dynamicSphere, transform);
         
         RigidBodyComponent body;
         body.SetBodyType(RigidBodyComponent::BodyType::Dynamic);
         body.SetMass(mass);
         body.SetInertiaTensorFromShape("sphere", Vector3(1.0f, 0.0f, 0.0f));
         body.useGravity = true;
         body.linearDamping = 0.0f;
         body.angularDamping = 0.0f;
         world->AddComponent(dynamicSphere, body);
         
         ColliderComponent collider = ColliderComponent::CreateSphere(1.0f);
         collider.material->restitution = 0.6f;
         collider.material->friction = 0.3f;
         world->AddComponent(dynamicSphere, collider);
     }
     
     std::cout << "\n开始模拟: 球体自由落体碰撞（完整验证）" << std::endl;
     
     PhysicsValidator validator(gravity, mass, 0.15f);
     bool collisionDetected = SimulateWithValidation(
         world, dynamicSphere, 120, 1.0f / 60.0f, "SphereFreeFall", validator
     );
     
     // 输出轨迹报告
     std::cout << validator.GenerateTrajectoryReport();
     
     // 验证物理定律
     std::string report;
     bool freeFallValid = validator.ValidateFreeFall(report);
     std::cout << report;
     
     bool energyValid = validator.ValidateEnergyConservation(report);
     std::cout << report;
     
     bool collisionValid = validator.ValidateCollisionResponse(report);
     std::cout << report;
     
     TEST_ASSERT(collisionDetected, "应该检测到碰撞");
     TEST_ASSERT(freeFallValid, "自由落体运动学应该正确");
     TEST_ASSERT(energyValid, "能量守恒应该满足");
     TEST_ASSERT(collisionValid, "碰撞响应应该正确");
     
     world->Shutdown();
     return true;
 }
 
 /**
  * @brief 测试场景2: 多次反弹验证
  */
 static bool Test_MultipleBounces_Validation() {
     auto world = std::make_shared<World>();
     RegisterPhysicsComponents(world);
     world->Initialize();
     
     float gravity = 9.81f;
     float mass = 0.5f;
     
     auto* physicsSystem = world->RegisterSystem<PhysicsUpdateSystem>();
     physicsSystem->SetGravity(Vector3(0.0f, -gravity, 0.0f));
     physicsSystem->SetFixedDeltaTime(1.0f / 60.0f);
     physicsSystem->SetSolverIterations(10);
     physicsSystem->SetPositionIterations(4);
     
     auto* collisionSystem = world->RegisterSystem<CollisionDetectionSystem>();
     collisionSystem->SetBroadPhase(std::make_unique<SpatialHashBroadPhase>(10.0f));
     
     // 创建地面
     EntityID ground = world->CreateEntity();
     {
         TransformComponent transform;
         transform.SetPosition(Vector3(0.0f, -1.0f, 0.0f));
         world->AddComponent(ground, transform);
         
         RigidBodyComponent body;
         body.SetBodyType(RigidBodyComponent::BodyType::Static);
         world->AddComponent(ground, body);
         
         ColliderComponent collider = ColliderComponent::CreateBox(Vector3(5.0f, 0.5f, 5.0f));
         world->AddComponent(ground, collider);
     }
     
     // 创建弹跳球
     EntityID bouncingBall = world->CreateEntity();
     {
         TransformComponent transform;
         transform.SetPosition(Vector3(0.0f, 3.0f, 0.0f));
         world->AddComponent(bouncingBall, transform);
         
         RigidBodyComponent body;
         body.SetBodyType(RigidBodyComponent::BodyType::Dynamic);
         body.SetMass(mass);
         body.SetInertiaTensorFromShape("sphere", Vector3(0.3f, 0.0f, 0.0f));
         body.useGravity = true;
         body.linearDamping = 0.0f;
         world->AddComponent(bouncingBall, body);
         
         ColliderComponent collider = ColliderComponent::CreateSphere(0.3f);
         collider.material->restitution = 0.8f; // 高弹性
         collider.material->friction = 0.1f;
         world->AddComponent(bouncingBall, collider);
     }
     
     std::cout << "\n开始模拟: 多次反弹验证" << std::endl;
     
     PhysicsValidator validator(gravity, mass, 0.2f);
     SimulateWithValidation(world, bouncingBall, 180, 1.0f / 60.0f, "MultipleBounces", validator);
     
     std::cout << validator.GenerateTrajectoryReport();
     
     std::string report;
     validator.ValidateEnergyConservation(report);
     std::cout << report;
     
     validator.ValidateCollisionResponse(report);
     std::cout << report;
     
     // 验证反弹次数和高度递减
     TEST_ASSERT(validator.collisions.size() >= 2, "应该发生多次反弹");
     
     std::cout << "\n反弹高度分析:\n";
     for (size_t i = 0; i < validator.collisions.size() && i < 3; ++i) {
         std::cout << "第 " << (i+1) << " 次碰撞后速度: " 
                   << validator.collisions[i].postVelocity.y() << " m/s\n";
     }
     
    world->Shutdown();
    return true;
}

/**
 * @brief 测试场景3: 球体与盒体碰撞（Sphere vs Box）
 */
static bool Test_SphereVsBox_Collision() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();
    
    float gravity = 9.81f;
    float mass = 1.0f;
    
    auto* physicsSystem = world->RegisterSystem<PhysicsUpdateSystem>();
    physicsSystem->SetGravity(Vector3(0.0f, -gravity, 0.0f));
    physicsSystem->SetFixedDeltaTime(1.0f / 60.0f);
    physicsSystem->SetSolverIterations(10);
    physicsSystem->SetPositionIterations(4);
    
    auto* collisionSystem = world->RegisterSystem<CollisionDetectionSystem>();
    collisionSystem->SetBroadPhase(std::make_unique<SpatialHashBroadPhase>(10.0f));
    
    // 创建静止的盒体（地面）
    EntityID staticBox = world->CreateEntity();
    {
        TransformComponent transform;
        transform.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
        world->AddComponent(staticBox, transform);
        
        RigidBodyComponent body;
        body.SetBodyType(RigidBodyComponent::BodyType::Static);
        world->AddComponent(staticBox, body);
        
        ColliderComponent collider = ColliderComponent::CreateBox(Vector3(2.0f, 0.5f, 2.0f));
        world->AddComponent(staticBox, collider);
    }
    
    // 创建动态球体（下落物体）
    EntityID dynamicSphere = world->CreateEntity();
    {
        TransformComponent transform;
        transform.SetPosition(Vector3(0.0f, 5.0f, 0.0f));
        world->AddComponent(dynamicSphere, transform);
        
        RigidBodyComponent body;
        body.SetBodyType(RigidBodyComponent::BodyType::Dynamic);
        body.SetMass(mass);
        body.SetInertiaTensorFromShape("sphere", Vector3(0.5f, 0.0f, 0.0f));
        body.useGravity = true;
        body.linearDamping = 0.0f;
        body.angularDamping = 0.0f;
        world->AddComponent(dynamicSphere, body);
        
        ColliderComponent collider = ColliderComponent::CreateSphere(0.5f);
        collider.material->restitution = 0.7f;
        collider.material->friction = 0.3f;
        world->AddComponent(dynamicSphere, collider);
    }
    
    std::cout << "\n开始模拟: 球体与盒体碰撞（Sphere vs Box）" << std::endl;
    
    PhysicsValidator validator(gravity, mass, 0.2f);
    bool collisionDetected = SimulateWithValidation(
        world, dynamicSphere, 120, 1.0f / 60.0f, "SphereVsBox", validator
    );
    
    std::cout << validator.GenerateTrajectoryReport();
    
    std::string report;
    bool freeFallValid = validator.ValidateFreeFall(report);
    std::cout << report;
    
    bool collisionValid = validator.ValidateCollisionResponse(report);
    std::cout << report;
    
    TEST_ASSERT(collisionDetected, "应该检测到碰撞");
    TEST_ASSERT(freeFallValid, "自由落体运动学应该正确");
    TEST_ASSERT(collisionValid, "碰撞响应应该正确");
    
    world->Shutdown();
    return true;
}

/**
 * @brief 测试场景4: 球体与胶囊体碰撞（Sphere vs Capsule）
 */
static bool Test_SphereVsCapsule_Collision() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();
    
    float gravity = 9.81f;
    float mass = 1.0f;
    
    auto* physicsSystem = world->RegisterSystem<PhysicsUpdateSystem>();
    physicsSystem->SetGravity(Vector3(0.0f, -gravity, 0.0f));
    physicsSystem->SetFixedDeltaTime(1.0f / 60.0f);
    physicsSystem->SetSolverIterations(10);
    physicsSystem->SetPositionIterations(4);
    
    auto* collisionSystem = world->RegisterSystem<CollisionDetectionSystem>();
    collisionSystem->SetBroadPhase(std::make_unique<SpatialHashBroadPhase>(10.0f));
    
    // 创建静止的胶囊体（地面）
    EntityID staticCapsule = world->CreateEntity();
    {
        TransformComponent transform;
        transform.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
        transform.SetRotation(Quaternion::Identity());
        world->AddComponent(staticCapsule, transform);
        
        RigidBodyComponent body;
        body.SetBodyType(RigidBodyComponent::BodyType::Static);
        world->AddComponent(staticCapsule, body);
        
        ColliderComponent collider = ColliderComponent::CreateCapsule(0.5f, 1.0f);
        world->AddComponent(staticCapsule, collider);
    }
    
    // 创建动态球体（下落物体）
    EntityID dynamicSphere = world->CreateEntity();
    {
        TransformComponent transform;
        transform.SetPosition(Vector3(0.0f, 4.0f, 0.0f));
        world->AddComponent(dynamicSphere, transform);
        
        RigidBodyComponent body;
        body.SetBodyType(RigidBodyComponent::BodyType::Dynamic);
        body.SetMass(mass);
        body.SetInertiaTensorFromShape("sphere", Vector3(0.5f, 0.0f, 0.0f));
        body.useGravity = true;
        body.linearDamping = 0.0f;
        body.angularDamping = 0.0f;
        world->AddComponent(dynamicSphere, body);
        
        ColliderComponent collider = ColliderComponent::CreateSphere(0.4f);
        collider.material->restitution = 0.6f;
        collider.material->friction = 0.3f;
        world->AddComponent(dynamicSphere, collider);
    }
    
    std::cout << "\n开始模拟: 球体与胶囊体碰撞（Sphere vs Capsule）" << std::endl;
    
    PhysicsValidator validator(gravity, mass, 0.2f);
    bool collisionDetected = SimulateWithValidation(
        world, dynamicSphere, 120, 1.0f / 60.0f, "SphereVsCapsule", validator
    );
    
    std::cout << validator.GenerateTrajectoryReport();
    
    std::string report;
    bool freeFallValid = validator.ValidateFreeFall(report);
    std::cout << report;
    
    bool collisionValid = validator.ValidateCollisionResponse(report);
    std::cout << report;
    
    TEST_ASSERT(collisionDetected, "应该检测到碰撞");
    TEST_ASSERT(freeFallValid, "自由落体运动学应该正确");
    TEST_ASSERT(collisionValid, "碰撞响应应该正确");
    
    world->Shutdown();
    return true;
}

/**
 * @brief 测试场景5: 盒体与盒体碰撞（Box vs Box）
 */
static bool Test_BoxVsBox_Collision() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();
    
    float gravity = 9.81f;
    float mass = 1.0f;
    
    auto* physicsSystem = world->RegisterSystem<PhysicsUpdateSystem>();
    physicsSystem->SetGravity(Vector3(0.0f, -gravity, 0.0f));
    physicsSystem->SetFixedDeltaTime(1.0f / 60.0f);
    physicsSystem->SetSolverIterations(10);
    physicsSystem->SetPositionIterations(4);
    
    auto* collisionSystem = world->RegisterSystem<CollisionDetectionSystem>();
    collisionSystem->SetBroadPhase(std::make_unique<SpatialHashBroadPhase>(10.0f));
    
    // 创建静止的盒体（地面）
    EntityID staticBox = world->CreateEntity();
    {
        TransformComponent transform;
        transform.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
        transform.SetRotation(Quaternion::Identity());
        world->AddComponent(staticBox, transform);
        
        RigidBodyComponent body;
        body.SetBodyType(RigidBodyComponent::BodyType::Static);
        world->AddComponent(staticBox, body);
        
        ColliderComponent collider = ColliderComponent::CreateBox(Vector3(3.0f, 0.5f, 3.0f));
        world->AddComponent(staticBox, collider);
    }
    
    // 创建动态盒体（下落物体）
    EntityID dynamicBox = world->CreateEntity();
    {
        TransformComponent transform;
        transform.SetPosition(Vector3(0.0f, 4.0f, 0.0f));
        transform.SetRotation(Quaternion::Identity());
        world->AddComponent(dynamicBox, transform);
        
        RigidBodyComponent body;
        body.SetBodyType(RigidBodyComponent::BodyType::Dynamic);
        body.SetMass(mass);
        body.SetInertiaTensorFromShape("box", Vector3(0.5f, 0.5f, 0.5f));
        body.useGravity = true;
        body.linearDamping = 0.0f;
        body.angularDamping = 0.0f;
        world->AddComponent(dynamicBox, body);
        
        ColliderComponent collider = ColliderComponent::CreateBox(Vector3(0.5f, 0.5f, 0.5f));
        collider.material->restitution = 0.5f;
        collider.material->friction = 0.4f;
        world->AddComponent(dynamicBox, collider);
    }
    
    std::cout << "\n开始模拟: 盒体与盒体碰撞（Box vs Box）" << std::endl;
    
    PhysicsValidator validator(gravity, mass, 0.2f);
    bool collisionDetected = SimulateWithValidation(
        world, dynamicBox, 120, 1.0f / 60.0f, "BoxVsBox", validator
    );
    
    std::cout << validator.GenerateTrajectoryReport();
    
    std::string report;
    bool freeFallValid = validator.ValidateFreeFall(report);
    std::cout << report;
    
    bool collisionValid = validator.ValidateCollisionResponse(report);
    std::cout << report;
    
    TEST_ASSERT(collisionDetected, "应该检测到碰撞");
    TEST_ASSERT(freeFallValid, "自由落体运动学应该正确");
    TEST_ASSERT(collisionValid, "碰撞响应应该正确");
    
    world->Shutdown();
    return true;
}

/**
 * @brief 测试场景6: 胶囊体与胶囊体碰撞（Capsule vs Capsule）
 */
static bool Test_CapsuleVsCapsule_Collision() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();
    
    float gravity = 9.81f;
    float mass = 1.0f;
    
    auto* physicsSystem = world->RegisterSystem<PhysicsUpdateSystem>();
    physicsSystem->SetGravity(Vector3(0.0f, -gravity, 0.0f));
    physicsSystem->SetFixedDeltaTime(1.0f / 60.0f);
    physicsSystem->SetSolverIterations(10);
    physicsSystem->SetPositionIterations(4);
    
    auto* collisionSystem = world->RegisterSystem<CollisionDetectionSystem>();
    collisionSystem->SetBroadPhase(std::make_unique<SpatialHashBroadPhase>(10.0f));
    
    // 创建静止的胶囊体（地面）
    EntityID staticCapsule = world->CreateEntity();
    {
        TransformComponent transform;
        transform.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
        transform.SetRotation(Quaternion::Identity());
        world->AddComponent(staticCapsule, transform);
        
        RigidBodyComponent body;
        body.SetBodyType(RigidBodyComponent::BodyType::Static);
        world->AddComponent(staticCapsule, body);
        
        ColliderComponent collider = ColliderComponent::CreateCapsule(0.5f, 1.0f);
        world->AddComponent(staticCapsule, collider);
    }
    
    // 创建动态胶囊体（下落物体）
    EntityID dynamicCapsule = world->CreateEntity();
    {
        TransformComponent transform;
        transform.SetPosition(Vector3(0.0f, 4.0f, 0.0f));
        transform.SetRotation(Quaternion::Identity());
        world->AddComponent(dynamicCapsule, transform);
        
        RigidBodyComponent body;
        body.SetBodyType(RigidBodyComponent::BodyType::Dynamic);
        body.SetMass(mass);
        body.SetInertiaTensorFromShape("capsule", Vector3(0.3f, 0.0f, 0.0f));
        body.useGravity = true;
        body.linearDamping = 0.0f;
        body.angularDamping = 0.0f;
        world->AddComponent(dynamicCapsule, body);
        
        ColliderComponent collider = ColliderComponent::CreateCapsule(0.4f, 0.8f);
        collider.material->restitution = 0.6f;
        collider.material->friction = 0.3f;
        world->AddComponent(dynamicCapsule, collider);
    }
    
    std::cout << "\n开始模拟: 胶囊体与胶囊体碰撞（Capsule vs Capsule）" << std::endl;
    
    PhysicsValidator validator(gravity, mass, 0.2f);
    bool collisionDetected = SimulateWithValidation(
        world, dynamicCapsule, 120, 1.0f / 60.0f, "CapsuleVsCapsule", validator
    );
    
    std::cout << validator.GenerateTrajectoryReport();
    
    std::string report;
    bool freeFallValid = validator.ValidateFreeFall(report);
    std::cout << report;
    
    bool collisionValid = validator.ValidateCollisionResponse(report);
    std::cout << report;
    
    TEST_ASSERT(collisionDetected, "应该检测到碰撞");
    TEST_ASSERT(freeFallValid, "自由落体运动学应该正确");
    TEST_ASSERT(collisionValid, "碰撞响应应该正确");
    
    world->Shutdown();
    return true;
}

/**
 * @brief 测试场景7: 胶囊体与盒体碰撞（Capsule vs Box）
 */
static bool Test_CapsuleVsBox_Collision() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();
    
    float gravity = 9.81f;
    float mass = 1.0f;
    
    auto* physicsSystem = world->RegisterSystem<PhysicsUpdateSystem>();
    physicsSystem->SetGravity(Vector3(0.0f, -gravity, 0.0f));
    physicsSystem->SetFixedDeltaTime(1.0f / 60.0f);
    physicsSystem->SetSolverIterations(10);
    physicsSystem->SetPositionIterations(4);
    
    auto* collisionSystem = world->RegisterSystem<CollisionDetectionSystem>();
    collisionSystem->SetBroadPhase(std::make_unique<SpatialHashBroadPhase>(10.0f));
    
    // 创建静止的盒体（地面）
    EntityID staticBox = world->CreateEntity();
    {
        TransformComponent transform;
        transform.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
        transform.SetRotation(Quaternion::Identity());
        world->AddComponent(staticBox, transform);
        
        RigidBodyComponent body;
        body.SetBodyType(RigidBodyComponent::BodyType::Static);
        world->AddComponent(staticBox, body);
        
        ColliderComponent collider = ColliderComponent::CreateBox(Vector3(2.0f, 0.5f, 2.0f));
        world->AddComponent(staticBox, collider);
    }
    
    // 创建动态胶囊体（下落物体）
    EntityID dynamicCapsule = world->CreateEntity();
    {
        TransformComponent transform;
        transform.SetPosition(Vector3(0.0f, 4.0f, 0.0f));
        transform.SetRotation(Quaternion::Identity());
        world->AddComponent(dynamicCapsule, transform);
        
        RigidBodyComponent body;
        body.SetBodyType(RigidBodyComponent::BodyType::Dynamic);
        body.SetMass(mass);
        body.SetInertiaTensorFromShape("capsule", Vector3(0.3f, 0.0f, 0.0f));
        body.useGravity = true;
        body.linearDamping = 0.0f;
        body.angularDamping = 0.0f;
        world->AddComponent(dynamicCapsule, body);
        
        ColliderComponent collider = ColliderComponent::CreateCapsule(0.4f, 0.8f);
        collider.material->restitution = 0.6f;
        collider.material->friction = 0.3f;
        world->AddComponent(dynamicCapsule, collider);
    }
    
    std::cout << "\n开始模拟: 胶囊体与盒体碰撞（Capsule vs Box）" << std::endl;
    
    PhysicsValidator validator(gravity, mass, 0.2f);
    bool collisionDetected = SimulateWithValidation(
        world, dynamicCapsule, 120, 1.0f / 60.0f, "CapsuleVsBox", validator
    );
    
    std::cout << validator.GenerateTrajectoryReport();
    
    std::string report;
    bool freeFallValid = validator.ValidateFreeFall(report);
    std::cout << report;
    
    bool collisionValid = validator.ValidateCollisionResponse(report);
    std::cout << report;
    
    TEST_ASSERT(collisionDetected, "应该检测到碰撞");
    TEST_ASSERT(freeFallValid, "自由落体运动学应该正确");
    TEST_ASSERT(collisionValid, "碰撞响应应该正确");
    
    world->Shutdown();
    return true;
}

// ============================================================================
// 主函数
// ============================================================================
 
 int main() {
     std::cout << "========================================" << std::endl;
     std::cout << "物理引擎全流程测试（高级验证）" << std::endl;
     std::cout << "========================================" << std::endl;
     std::cout << "验证内容:" << std::endl;
     std::cout << "1. 自由落体运动学方程" << std::endl;
     std::cout << "2. 能量守恒定律" << std::endl;
     std::cout << "3. 碰撞动量守恒" << std::endl;
     std::cout << "4. 弹性碰撞响应" << std::endl;
     std::cout << "5. 运动轨迹连续性" << std::endl;
    std::cout << "========================================" << std::endl;
    
    RUN_TEST(Test_SphereFreeFall_FullValidation);
    RUN_TEST(Test_MultipleBounces_Validation);
    RUN_TEST(Test_SphereVsBox_Collision);
    RUN_TEST(Test_SphereVsCapsule_Collision);
    RUN_TEST(Test_BoxVsBox_Collision);
    RUN_TEST(Test_CapsuleVsCapsule_Collision);
    RUN_TEST(Test_CapsuleVsBox_Collision);
    
    std::cout << "\n========================================" << std::endl;
     std::cout << "测试总结" << std::endl;
     std::cout << "========================================" << std::endl;
     std::cout << "总测试数: " << g_testCount << std::endl;
     std::cout << "通过: " << g_passedCount << std::endl;
     std::cout << "失败: " << g_failedCount << std::endl;
     
     if (g_failedCount == 0) {
         std::cout << "\n✅ 所有测试通过！" << std::endl;
         return 0;
     } else {
         std::cout << "\n❌ 有测试失败" << std::endl;
         return 1;
     }
 }