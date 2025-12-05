/**
 * @file test_collision_system.cpp
 * @brief 碰撞检测系统集成测试
 */

#include "render/physics/physics_systems.h"
#include "render/physics/physics_components.h"
#include "render/physics/physics_events.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/application/event_bus.h"
#include <iostream>

using namespace Render;
using namespace Render::Physics;
using namespace Render::ECS;

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
        std::cout << "运行测试: " << #test_func << "..." << std::endl; \
        if (test_func()) { \
            std::cout << "✓ " << #test_func << " 通过" << std::endl; \
        } else { \
            std::cout << "✗ " << #test_func << " 失败" << std::endl; \
        } \
    } while(0)

// ============================================================================
// 测试辅助函数
// ============================================================================

void RegisterPhysicsComponents(std::shared_ptr<World> world) {
    // 显式注册物理组件类型（在 Initialize 之前调用）
    world->RegisterComponent<TransformComponent>();
    world->RegisterComponent<Physics::ColliderComponent>();
    world->RegisterComponent<Physics::RigidBodyComponent>();
}

// ============================================================================
// 碰撞检测系统基础测试
// ============================================================================

bool Test_CollisionSystem_EmptyScene() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);  // 在 Initialize 之前注册
    world->Initialize();
    
    auto* system = world->RegisterSystem<CollisionDetectionSystem>();
    
    system->Update(0.016f);
    
    TEST_ASSERT(system->GetCollisionPairs().empty(), "空场景应该没有碰撞");
    TEST_ASSERT(system->GetStats().totalColliders == 0, "应该没有碰撞体");
    
    world->Shutdown();
    return true;
}

bool Test_CollisionSystem_TwoSpheres_Colliding() {
    std::cout << "  开始测试..." << std::endl;
    
    try {
        auto world = std::make_shared<World>();
        std::cout << "  World 创建完成" << std::endl;
        
        // 先注册组件类型，再初始化
        world->RegisterComponent<TransformComponent>();
        world->RegisterComponent<Physics::ColliderComponent>();
        world->RegisterComponent<Physics::RigidBodyComponent>();
        std::cout << "  组件类型注册完成" << std::endl;
        
        world->Initialize();
        std::cout << "  World 初始化完成" << std::endl;
        
        auto* system = world->RegisterSystem<CollisionDetectionSystem>();
        std::cout << "  System 注册完成" << std::endl;
        
        // 创建实体
        EntityID entity1 = world->CreateEntity();
        EntityID entity2 = world->CreateEntity();
        std::cout << "  实体创建完成" << std::endl;
        
        // 添加 Transform
        TransformComponent transform1, transform2;
        transform1.SetPosition(Vector3(0, 0, 0));
        transform2.SetPosition(Vector3(1.5f, 0, 0));
        std::cout << "  Transform 创建完成" << std::endl;
        
        world->AddComponent(entity1, std::move(transform1));
        std::cout << "  Transform1 添加完成" << std::endl;
        
        world->AddComponent(entity2, std::move(transform2));
        std::cout << "  Transform2 添加完成" << std::endl;
        
        // 添加 Collider
        world->AddComponent(entity1, ColliderComponent::CreateSphere(1.0f));
        std::cout << "  Collider1 添加完成" << std::endl;
        
        world->AddComponent(entity2, ColliderComponent::CreateSphere(1.0f));
        std::cout << "  Collider2 添加完成" << std::endl;
        
        // 更新系统
        system->Update(0.016f);
        std::cout << "  系统更新完成" << std::endl;
        
        TEST_ASSERT(system->GetCollisionPairs().size() == 1, "应该检测到 1 对碰撞");
        TEST_ASSERT(system->GetStats().totalColliders == 2, "应该有 2 个碰撞体");
        
        world->Shutdown();
        std::cout << "  测试完成" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "异常: " << e.what() << std::endl;
        return false;
    }
    
    return true;
}

bool Test_CollisionSystem_TwoSpheres_Separated() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();
    
    auto* system = world->RegisterSystem<CollisionDetectionSystem>();
    
    EntityID entity1 = world->CreateEntity();
    EntityID entity2 = world->CreateEntity();
    
    TransformComponent transform1, transform2;
    transform1.SetPosition(Vector3(0, 0, 0));
    transform2.SetPosition(Vector3(10, 0, 0));  // 距离很远
    
    world->AddComponent(entity1, transform1);
    world->AddComponent(entity2, transform2);
    
    ColliderComponent collider1 = ColliderComponent::CreateSphere(1.0f);
    ColliderComponent collider2 = ColliderComponent::CreateSphere(1.0f);
    
    world->AddComponent(entity1, collider1);
    world->AddComponent(entity2, collider2);
    
    system->Update(0.016f);
    
    TEST_ASSERT(system->GetCollisionPairs().empty(), "分离的球体不应该碰撞");
    
    world->Shutdown();
    return true;
}

// ============================================================================
// 碰撞层测试
// ============================================================================

bool Test_CollisionSystem_LayerFiltering() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();
    
    auto* system = world->RegisterSystem<CollisionDetectionSystem>();
    
    EntityID entity1 = world->CreateEntity();
    EntityID entity2 = world->CreateEntity();
    
    TransformComponent transform1, transform2;
    transform1.SetPosition(Vector3(0, 0, 0));
    transform2.SetPosition(Vector3(1.5f, 0, 0));
    
    world->AddComponent(entity1, transform1);
    world->AddComponent(entity2, transform2);
    
    ColliderComponent collider1 = ColliderComponent::CreateSphere(1.0f);
    ColliderComponent collider2 = ColliderComponent::CreateSphere(1.0f);
    
    // 设置不同的碰撞层，并且掩码不匹配
    collider1.collisionLayer = 0;
    collider1.collisionMask = 0x00000001;  // 只与层 0 碰撞
    
    collider2.collisionLayer = 1;
    collider2.collisionMask = 0x00000002;  // 只与层 1 碰撞
    
    world->AddComponent(entity1, collider1);
    world->AddComponent(entity2, collider2);
    
    system->Update(0.016f);
    
    TEST_ASSERT(system->GetCollisionPairs().empty(), "不匹配的碰撞层不应该碰撞");
    
    world->Shutdown();
    return true;
}

bool Test_CollisionSystem_LayerMatching() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();
    
    auto* system = world->RegisterSystem<CollisionDetectionSystem>();
    
    EntityID entity1 = world->CreateEntity();
    EntityID entity2 = world->CreateEntity();
    
    TransformComponent transform1, transform2;
    transform1.SetPosition(Vector3(0, 0, 0));
    transform2.SetPosition(Vector3(1.5f, 0, 0));
    
    world->AddComponent(entity1, transform1);
    world->AddComponent(entity2, transform2);
    
    ColliderComponent collider1 = ColliderComponent::CreateSphere(1.0f);
    ColliderComponent collider2 = ColliderComponent::CreateSphere(1.0f);
    
    // 设置匹配的碰撞层
    collider1.collisionLayer = 0;
    collider1.collisionMask = 0x00000002;  // 与层 1 碰撞
    
    collider2.collisionLayer = 1;
    collider2.collisionMask = 0x00000001;  // 与层 0 碰撞
    
    world->AddComponent(entity1, collider1);
    world->AddComponent(entity2, collider2);
    
    system->Update(0.016f);
    
    TEST_ASSERT(system->GetCollisionPairs().size() == 1, "匹配的碰撞层应该碰撞");
    
    world->Shutdown();
    return true;
}

// ============================================================================
// 触发器测试
// ============================================================================

bool Test_CollisionSystem_Trigger() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();
    
    auto* system = world->RegisterSystem<CollisionDetectionSystem>();
    Application::EventBus eventBus;
    system->SetEventBus(&eventBus);
    
    // 记录触发事件
    int triggerEnterCount = 0;
    eventBus.Subscribe<TriggerEnterEvent>([&](const TriggerEnterEvent& e) {
        triggerEnterCount++;
    });
    
    EntityID trigger = world->CreateEntity();
    EntityID other = world->CreateEntity();
    
    TransformComponent transform1, transform2;
    transform1.SetPosition(Vector3(0, 0, 0));
    transform2.SetPosition(Vector3(1.5f, 0, 0));
    
    world->AddComponent(trigger, transform1);
    world->AddComponent(other, transform2);
    
    ColliderComponent collider1 = ColliderComponent::CreateSphere(1.0f);
    ColliderComponent collider2 = ColliderComponent::CreateSphere(1.0f);
    
    collider1.isTrigger = true;  // 设置为触发器
    
    world->AddComponent(trigger, collider1);
    world->AddComponent(other, collider2);
    
    system->Update(0.016f);
    
    TEST_ASSERT(triggerEnterCount == 1, "应该触发 TriggerEnter 事件");
    
    world->Shutdown();
    return true;
}

// ============================================================================
// 碰撞事件测试
// ============================================================================

bool Test_CollisionSystem_CollisionEvents() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();
    
    auto* system = world->RegisterSystem<CollisionDetectionSystem>();
    Application::EventBus eventBus;
    system->SetEventBus(&eventBus);
    
    int collisionEnterCount = 0;
    int collisionStayCount = 0;
    
    eventBus.Subscribe<CollisionEnterEvent>([&](const CollisionEnterEvent& e) {
        collisionEnterCount++;
    });
    
    eventBus.Subscribe<CollisionStayEvent>([&](const CollisionStayEvent& e) {
        collisionStayCount++;
    });
    
    EntityID entity1 = world->CreateEntity();
    EntityID entity2 = world->CreateEntity();
    
    TransformComponent transform1, transform2;
    transform1.SetPosition(Vector3(0, 0, 0));
    transform2.SetPosition(Vector3(1.5f, 0, 0));
    
    world->AddComponent(entity1, transform1);
    world->AddComponent(entity2, transform2);
    
    ColliderComponent collider1 = ColliderComponent::CreateSphere(1.0f);
    ColliderComponent collider2 = ColliderComponent::CreateSphere(1.0f);
    
    world->AddComponent(entity1, collider1);
    world->AddComponent(entity2, collider2);
    
    // 第一帧：应该触发 Enter
    system->Update(0.016f);
    TEST_ASSERT(collisionEnterCount == 1, "第一帧应该触发 CollisionEnter");
    
    // 第二帧：应该触发 Stay
    system->Update(0.016f);
    TEST_ASSERT(collisionStayCount == 1, "第二帧应该触发 CollisionStay");
    
    world->Shutdown();
    return true;
}

// ============================================================================
// 性能测试
// ============================================================================

bool Test_CollisionSystem_ManyObjects() {
    auto world = std::make_shared<World>();
    RegisterPhysicsComponents(world);
    world->Initialize();
    
    auto* system = world->RegisterSystem<CollisionDetectionSystem>();
    
    // 创建 100 个球体，排列更紧密以产生碰撞
    const int count = 100;
    for (int i = 0; i < count; ++i) {
        EntityID entity = world->CreateEntity();
        
        TransformComponent transform;
        // 调整间距为 1.8（小于直径 2），让相邻球体碰撞
        transform.SetPosition(Vector3(
            static_cast<float>(i % 10) * 1.8f,
            static_cast<float>(i / 10) * 1.8f,
            0
        ));
        
        world->AddComponent(entity, transform);
        world->AddComponent(entity, Physics::ColliderComponent::CreateSphere(1.0f));
    }
    
    system->Update(0.016f);
    
    TEST_ASSERT(system->GetStats().totalColliders == count, "应该有 100 个碰撞体");
    TEST_ASSERT(system->GetStats().broadPhasePairs > 0, "应该有粗检测对");
    TEST_ASSERT(system->GetStats().actualCollisions > 0, "应该有实际碰撞");
    
    std::cout << "  碰撞体总数: " << system->GetStats().totalColliders << std::endl;
    std::cout << "  粗检测对数: " << system->GetStats().broadPhasePairs << std::endl;
    std::cout << "  细检测次数: " << system->GetStats().narrowPhaseTests << std::endl;
    std::cout << "  实际碰撞数: " << system->GetStats().actualCollisions << std::endl;
    std::cout << "  粗检测耗时: " << system->GetStats().broadPhaseTime << " ms" << std::endl;
    std::cout << "  细检测耗时: " << system->GetStats().narrowPhaseTime << " ms" << std::endl;
    
    // 验证性能
    float totalTime = system->GetStats().broadPhaseTime + system->GetStats().narrowPhaseTime;
    std::cout << "  总耗时: " << totalTime << " ms" << std::endl;
    
    // 100 个物体应该在 5ms 内完成检测
    TEST_ASSERT(totalTime < 5.0f, "100 个物体的碰撞检测应该在 5ms 内完成");
    
    world->Shutdown();
    return true;
}

// ============================================================================
// 主测试函数
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "碰撞检测系统集成测试" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::cout << "\n--- 基础功能测试 ---" << std::endl;
    RUN_TEST(Test_CollisionSystem_EmptyScene);
    RUN_TEST(Test_CollisionSystem_TwoSpheres_Colliding);
    RUN_TEST(Test_CollisionSystem_TwoSpheres_Separated);
    
    std::cout << "\n--- 碰撞层测试 ---" << std::endl;
    RUN_TEST(Test_CollisionSystem_LayerFiltering);
    RUN_TEST(Test_CollisionSystem_LayerMatching);
    
    std::cout << "\n--- 触发器测试 ---" << std::endl;
    RUN_TEST(Test_CollisionSystem_Trigger);
    
    std::cout << "\n--- 碰撞事件测试 ---" << std::endl;
    RUN_TEST(Test_CollisionSystem_CollisionEvents);
    
    std::cout << "\n--- 性能测试 ---" << std::endl;
    RUN_TEST(Test_CollisionSystem_ManyObjects);
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "测试完成" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "总测试数: " << g_testCount << std::endl;
    std::cout << "通过: " << g_passedCount << " ✓" << std::endl;
    std::cout << "失败: " << g_failedCount << " ✗" << std::endl;
    
    if (g_failedCount == 0) {
        std::cout << "\n🎉 所有测试通过！" << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ 有测试失败！" << std::endl;
        return 1;
    }
}

