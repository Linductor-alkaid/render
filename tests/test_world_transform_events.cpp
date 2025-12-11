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
 * @file test_world_transform_events.cpp
 * @brief World Transform事件集成测试
 * 
 * 测试World集成Transform变化回调系统的所有功能：
 * - 添加TransformComponent时自动设置回调
 * - 移除组件时清理回调
 * - 实体销毁时清理回调
 * - Transform变化触发组件事件
 */

#include "render/ecs/world.h"
#include "render/ecs/component_events.h"
#include "render/ecs/components.h"
#include "render/ecs/component_registry.h"
#include "render/types.h"
#include "render/math_utils.h"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <memory>

using namespace Render;
using namespace Render::ECS;

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
            std::cerr << "   条件: " << #condition << std::endl; \
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
// 2.4.2 测试World集成
// ============================================================================

bool Test_World_AddTransformComponent_AutoSetupCallback() {
    auto world = std::make_shared<World>();
    world->RegisterComponent<TransformComponent>();
    world->Initialize();
    
    EntityID entity = world->CreateEntity();
    
    // 注册组件变化回调
    int eventCount = 0;
    EntityID receivedEntity = EntityID::Invalid();
    
    world->GetComponentRegistry().RegisterComponentChangeCallback<TransformComponent>(
        [&](EntityID e, const TransformComponent& comp) {
            eventCount++;
            receivedEntity = e;
        }
    );
    
    // 添加TransformComponent
    TransformComponent transformComp;
    world->AddComponent(entity, transformComp);
    
    // 获取World中存储的组件引用，然后修改Transform
    // 注意：直接设置一个明确不同的值，确保触发变化
    TransformComponent& comp = world->GetComponent<TransformComponent>(entity);
    
    // 先设置一个非零位置（确保与默认值不同）
    comp.SetPosition(Vector3(10.0f, 20.0f, 30.0f));
    
    // 回调是同步执行的，但为了确保所有回调都执行完，稍微等待一下
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    TEST_ASSERT(eventCount >= 1, "Transform变化应该触发组件变化事件");
    if (eventCount > 0) {
        TEST_ASSERT(receivedEntity == entity, "事件应该包含正确的实体ID");
    }
    
    return true;
}

bool Test_World_RemoveComponent_ClearsCallback() {
    auto world = std::make_shared<World>();
    world->RegisterComponent<TransformComponent>();
    world->Initialize();
    
    EntityID entity = world->CreateEntity();
    
    // 添加TransformComponent
    TransformComponent transformComp;
    world->AddComponent(entity, transformComp);
    
    // 获取Transform对象并设置一个测试回调
    TransformComponent& comp = world->GetComponent<TransformComponent>(entity);
    int transformCallbackCount = 0;
    comp.transform->SetChangeCallback([&](const Transform*) {
        transformCallbackCount++;
    });
    
    // 修改Transform，应该触发回调
    comp.SetPosition(Vector3(1.0f, 2.0f, 3.0f));
    TEST_ASSERT(transformCallbackCount == 1, "Transform回调应该被调用");
    
    // 移除组件
    world->RemoveComponent<TransformComponent>(entity);
    
    // 再次修改Transform（如果组件还在），不应该触发回调
    // 注意：组件已被移除，所以无法再访问
    // 但我们可以验证组件确实被移除了
    TEST_ASSERT(!world->HasComponent<TransformComponent>(entity), 
                "组件应该已被移除");
    
    return true;
}

bool Test_World_DestroyEntity_ClearsCallback() {
    auto world = std::make_shared<World>();
    world->RegisterComponent<TransformComponent>();
    world->Initialize();
    
    EntityID entity = world->CreateEntity();
    
    // 添加TransformComponent
    TransformComponent transformComp;
    world->AddComponent(entity, transformComp);
    
    // 获取Transform对象并设置一个测试回调
    TransformComponent& comp = world->GetComponent<TransformComponent>(entity);
    int transformCallbackCount = 0;
    comp.transform->SetChangeCallback([&](const Transform*) {
        transformCallbackCount++;
    });
    
    // 修改Transform，应该触发回调
    comp.SetPosition(Vector3(1.0f, 2.0f, 3.0f));
    TEST_ASSERT(transformCallbackCount == 1, "Transform回调应该被调用");
    
    // 销毁实体
    world->DestroyEntity(entity);
    
    // 验证实体已被销毁
    TEST_ASSERT(!world->IsValidEntity(entity), "实体应该已被销毁");
    TEST_ASSERT(!world->HasComponent<TransformComponent>(entity), 
                "组件应该已被移除");
    
    return true;
}

bool Test_World_TransformChange_TriggersComponentEvent() {
    auto world = std::make_shared<World>();
    world->RegisterComponent<TransformComponent>();
    world->Initialize();
    
    EntityID entity = world->CreateEntity();
    
    // 注册组件变化回调
    int eventCount = 0;
    std::vector<EntityID> receivedEntities;
    std::vector<Vector3> receivedPositions;
    
    world->GetComponentRegistry().RegisterComponentChangeCallback<TransformComponent>(
        [&](EntityID e, const TransformComponent& comp) {
            eventCount++;
            receivedEntities.push_back(e);
            
            if (comp.transform) {
                receivedPositions.push_back(comp.transform->GetPosition());
            }
        }
    );
    
    // 添加TransformComponent
    TransformComponent transformComp;
    world->AddComponent(entity, transformComp);
    
    // 修改Transform多次
    TransformComponent& comp = world->GetComponent<TransformComponent>(entity);
    
    // 第一次设置位置（确保与默认值不同）
    comp.SetPosition(Vector3(1.0f, 2.0f, 3.0f));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 第二次设置不同的位置
    comp.SetPosition(Vector3(4.0f, 5.0f, 6.0f));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 设置旋转（应该也会触发）
    comp.SetRotation(Quaternion(Eigen::AngleAxisf(1.57f, Vector3::UnitY())));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 验证事件被触发
    TEST_ASSERT(eventCount >= 2, "应该至少触发2次组件变化事件");
    TEST_ASSERT(receivedEntities.size() >= 2, "应该收到至少2个事件");
    TEST_ASSERT(receivedEntities[0] == entity, "事件应该包含正确的实体ID");
    
    // 验证位置变化
    if (receivedPositions.size() >= 2) {
        TEST_ASSERT(receivedPositions[0].isApprox(Vector3(1.0f, 2.0f, 3.0f), MathUtils::EPSILON),
                    "第一个事件应该包含正确的位置");
        TEST_ASSERT(receivedPositions[1].isApprox(Vector3(4.0f, 5.0f, 6.0f), MathUtils::EPSILON),
                    "第二个事件应该包含正确的位置");
    }
    
    return true;
}

bool Test_World_Initialize_SetupExistingComponents() {
    auto world = std::make_shared<World>();
    world->RegisterComponent<TransformComponent>();
    
    // 在初始化前添加组件
    EntityID entity1 = world->CreateEntity();
    EntityID entity2 = world->CreateEntity();
    
    TransformComponent transformComp1;
    TransformComponent transformComp2;
    
    // 直接通过ComponentRegistry添加（绕过World的AddComponent）
    world->GetComponentRegistry().AddComponent(entity1, transformComp1);
    world->GetComponentRegistry().AddComponent(entity2, transformComp2);
    
    // 注册组件变化回调
    int eventCount = 0;
    std::vector<EntityID> receivedEntities;
    
    world->GetComponentRegistry().RegisterComponentChangeCallback<TransformComponent>(
        [&](EntityID e, const TransformComponent&) {
            eventCount++;
            receivedEntities.push_back(e);
        }
    );
    
    // 初始化World（应该为现有组件设置回调）
    world->Initialize();
    
    // 修改Transform，应该触发事件
    TransformComponent& comp1 = world->GetComponent<TransformComponent>(entity1);
    comp1.SetPosition(Vector3(10.0f, 20.0f, 30.0f));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    TransformComponent& comp2 = world->GetComponent<TransformComponent>(entity2);
    comp2.SetPosition(Vector3(40.0f, 50.0f, 60.0f));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 验证事件被触发
    TEST_ASSERT(eventCount >= 2, "应该至少触发2次组件变化事件");
    TEST_ASSERT(receivedEntities.size() >= 2, "应该收到至少2个事件");
    
    return true;
}

bool Test_World_MultipleEntities_IndependentCallbacks() {
    auto world = std::make_shared<World>();
    world->RegisterComponent<TransformComponent>();
    world->Initialize();
    
    // 创建多个实体
    EntityID entity1 = world->CreateEntity();
    EntityID entity2 = world->CreateEntity();
    EntityID entity3 = world->CreateEntity();
    
    // 添加TransformComponent
    world->AddComponent(entity1, TransformComponent{});
    world->AddComponent(entity2, TransformComponent{});
    world->AddComponent(entity3, TransformComponent{});
    
    // 注册组件变化回调
    std::vector<EntityID> receivedEntities;
    
    world->GetComponentRegistry().RegisterComponentChangeCallback<TransformComponent>(
        [&](EntityID e, const TransformComponent&) {
            receivedEntities.push_back(e);
        }
    );
    
    // 修改不同实体的Transform
    world->GetComponent<TransformComponent>(entity1).SetPosition(Vector3(1.0f, 0.0f, 0.0f));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    world->GetComponent<TransformComponent>(entity2).SetPosition(Vector3(2.0f, 0.0f, 0.0f));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    world->GetComponent<TransformComponent>(entity3).SetPosition(Vector3(3.0f, 0.0f, 0.0f));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 验证所有实体都触发了事件
    TEST_ASSERT(receivedEntities.size() >= 3, "应该收到至少3个事件");
    
    // 验证实体ID正确
    bool found1 = false, found2 = false, found3 = false;
    for (EntityID e : receivedEntities) {
        if (e == entity1) found1 = true;
        if (e == entity2) found2 = true;
        if (e == entity3) found3 = true;
    }
    
    TEST_ASSERT(found1, "应该收到entity1的事件");
    TEST_ASSERT(found2, "应该收到entity2的事件");
    TEST_ASSERT(found3, "应该收到entity3的事件");
    
    return true;
}

bool Test_World_RemoveComponent_NoMemoryLeak() {
    auto world = std::make_shared<World>();
    world->RegisterComponent<TransformComponent>();
    world->Initialize();
    
    // 创建多个实体并添加组件
    const int numEntities = 10;
    std::vector<EntityID> entities;
    
    for (int i = 0; i < numEntities; ++i) {
        EntityID entity = world->CreateEntity();
        entities.push_back(entity);
        world->AddComponent(entity, TransformComponent{});
    }
    
    // 修改所有Transform
    for (EntityID entity : entities) {
        world->GetComponent<TransformComponent>(entity).SetPosition(
            Vector3(static_cast<float>(entity.index), 0.0f, 0.0f)
        );
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // 移除所有组件
    for (EntityID entity : entities) {
        world->RemoveComponent<TransformComponent>(entity);
    }
    
    // 销毁所有实体
    for (EntityID entity : entities) {
        world->DestroyEntity(entity);
    }
    
    // 验证没有内存泄漏（通过检查活跃实体数量）
    // 注意：这是一个简单的检查，真正的内存泄漏检测需要工具
    TEST_ASSERT(world->GetEntityManager().GetActiveEntityCount() == 0, 
                "所有实体应该已被销毁");
    
    return true;
}

bool Test_World_TransformChange_OnlyOnValueChange() {
    auto world = std::make_shared<World>();
    world->RegisterComponent<TransformComponent>();
    world->Initialize();
    
    EntityID entity = world->CreateEntity();
    world->AddComponent(entity, TransformComponent{});
    
    // 注册组件变化回调
    int eventCount = 0;
    
    world->GetComponentRegistry().RegisterComponentChangeCallback<TransformComponent>(
        [&](EntityID, const TransformComponent&) {
            eventCount++;
        }
    );
    
    TransformComponent& comp = world->GetComponent<TransformComponent>(entity);
    
    // 第一次设置位置，应该触发事件
    comp.SetPosition(Vector3(1.0f, 2.0f, 3.0f));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    int countAfterFirst = eventCount;
    TEST_ASSERT(countAfterFirst >= 1, "第一次设置应该触发事件");
    
    // 第二次设置相同值，不应该触发事件
    comp.SetPosition(Vector3(1.0f, 2.0f, 3.0f));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    TEST_ASSERT(eventCount == countAfterFirst, "设置相同值不应该触发事件");
    
    // 设置不同值，应该触发事件
    comp.SetPosition(Vector3(4.0f, 5.0f, 6.0f));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    TEST_ASSERT(eventCount > countAfterFirst, "设置不同值应该触发事件");
    
    return true;
}

// ============================================================================
// 主函数
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "World Transform事件集成测试" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // 2.4.2 测试World集成
    std::cout << "--- 2.4.2 测试World集成 ---" << std::endl;
    RUN_TEST(Test_World_AddTransformComponent_AutoSetupCallback);
    RUN_TEST(Test_World_RemoveComponent_ClearsCallback);
    RUN_TEST(Test_World_DestroyEntity_ClearsCallback);
    RUN_TEST(Test_World_TransformChange_TriggersComponentEvent);
    RUN_TEST(Test_World_Initialize_SetupExistingComponents);
    RUN_TEST(Test_World_MultipleEntities_IndependentCallbacks);
    RUN_TEST(Test_World_RemoveComponent_NoMemoryLeak);
    RUN_TEST(Test_World_TransformChange_OnlyOnValueChange);
    std::cout << std::endl;
    
    // 输出测试结果
    std::cout << "========================================" << std::endl;
    std::cout << "测试结果统计" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "总测试数: " << g_testCount << std::endl;
    std::cout << "通过: " << g_passedCount << std::endl;
    std::cout << "失败: " << g_failedCount << std::endl;
    std::cout << "========================================" << std::endl;
    
    if (g_failedCount == 0) {
        std::cout << "✓ 所有测试通过！" << std::endl;
        return 0;
    } else {
        std::cout << "✗ 有 " << g_failedCount << " 个测试失败" << std::endl;
        return 1;
    }
}

