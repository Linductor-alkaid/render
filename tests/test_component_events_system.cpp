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
 * @file test_component_events_system.cpp
 * @brief 组件变化事件系统测试
 * 
 * 测试组件变化事件系统的所有功能：
 * - 事件类型定义
 * - ComponentRegistry回调机制
 * - ComponentArray变化通知
 */

#include "render/ecs/component_events.h"
#include "render/ecs/component_registry.h"
#include "render/ecs/components.h"
#include "render/ecs/entity.h"
#include "render/types.h"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

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
// 1.4.1 测试事件类型定义
// ============================================================================

bool Test_ComponentChangeEvent_Construction() {
    EntityID entity{1, 1};
    std::type_index typeIndex = std::type_index(typeid(TransformComponent));
    
    ComponentChangeEvent event(entity, typeIndex);
    
    TEST_ASSERT(event.entity.index == 1, "实体索引应该正确");
    TEST_ASSERT(event.entity.version == 1, "实体版本应该正确");
    TEST_ASSERT(event.componentType == typeIndex, "组件类型应该正确");
    
    return true;
}

bool Test_TransformComponentChangeEvent_Construction() {
    EntityID entity{2, 1};
    Vector3 position(1.0f, 2.0f, 3.0f);
    Quaternion rotation = Quaternion::Identity();
    Vector3 scale(1.0f, 1.0f, 1.0f);
    
    TransformComponentChangeEvent event(entity, position, rotation, scale);
    
    TEST_ASSERT(event.entity.index == 2, "实体索引应该正确");
    TEST_ASSERT(event.position == position, "位置应该正确");
    TEST_ASSERT(event.rotation.coeffs().isApprox(rotation.coeffs()), "旋转应该正确");
    TEST_ASSERT(event.scale == scale, "缩放应该正确");
    TEST_ASSERT(event.componentType == std::type_index(typeid(TransformComponent)), 
                "组件类型应该是TransformComponent");
    
    return true;
}

bool Test_TransformComponentChangeEvent_Inheritance() {
    EntityID entity{3, 1};
    Vector3 position(1.0f, 2.0f, 3.0f);
    Quaternion rotation = Quaternion::Identity();
    Vector3 scale(1.0f, 1.0f, 1.0f);
    
    TransformComponentChangeEvent event(entity, position, rotation, scale);
    
    // 测试继承关系
    ComponentChangeEvent* basePtr = &event;
    TEST_ASSERT(basePtr != nullptr, "应该可以转换为基类指针");
    TEST_ASSERT(basePtr->entity.index == 3, "基类指针访问应该正确");
    
    return true;
}

// ============================================================================
// 1.4.2 测试ComponentRegistry回调机制
// ============================================================================

bool Test_ComponentRegistry_CallbackRegistration() {
    ComponentRegistry registry;
    registry.RegisterComponent<TransformComponent>();
    
    bool callbackCalled = false;
    EntityID calledEntity = EntityID::Invalid();
    TransformComponent calledComponent;
    
    uint64_t callbackId = registry.RegisterComponentChangeCallback<TransformComponent>(
        [&](EntityID entity, const TransformComponent& component) {
            callbackCalled = true;
            calledEntity = entity;
            calledComponent = component;
        }
    );
    
    TEST_ASSERT(callbackId > 0, "回调ID应该大于0");
    TEST_ASSERT(!callbackCalled, "回调不应该立即被调用");
    
    return true;
}

bool Test_ComponentRegistry_CallbackInvocation() {
    ComponentRegistry registry;
    registry.RegisterComponent<TransformComponent>();
    
    bool callbackCalled = false;
    EntityID calledEntity = EntityID::Invalid();
    TransformComponent calledComponent;
    
    registry.RegisterComponentChangeCallback<TransformComponent>(
        [&](EntityID entity, const TransformComponent& component) {
            callbackCalled = true;
            calledEntity = entity;
            calledComponent = component;
        }
    );
    
    EntityID entity{10, 1};
    TransformComponent component;
    component.SetPosition(Vector3(5.0f, 6.0f, 7.0f));
    
    // 触发组件变化事件
    registry.OnComponentChanged(entity, component);
    
    TEST_ASSERT(callbackCalled, "回调应该被调用");
    TEST_ASSERT(calledEntity.index == 10, "回调应该收到正确的实体ID");
    TEST_ASSERT(calledComponent.GetPosition().isApprox(Vector3(5.0f, 6.0f, 7.0f)), 
                "回调应该收到正确的组件数据");
    
    return true;
}

bool Test_ComponentRegistry_CallbackUnregistration() {
    ComponentRegistry registry;
    registry.RegisterComponent<TransformComponent>();
    
    int callbackCallCount = 0;
    
    uint64_t callbackId = registry.RegisterComponentChangeCallback<TransformComponent>(
        [&](EntityID, const TransformComponent&) {
            callbackCallCount++;
        }
    );
    
    EntityID entity{20, 1};
    TransformComponent component;
    
    // 第一次调用
    registry.OnComponentChanged(entity, component);
    TEST_ASSERT(callbackCallCount == 1, "第一次调用应该成功");
    
    // 取消注册
    registry.UnregisterComponentChangeCallback(callbackId);
    
    // 第二次调用（应该不触发回调）
    registry.OnComponentChanged(entity, component);
    TEST_ASSERT(callbackCallCount == 1, "取消注册后不应该再调用回调");
    
    return true;
}

bool Test_ComponentRegistry_MultipleCallbacks() {
    ComponentRegistry registry;
    registry.RegisterComponent<TransformComponent>();
    
    int callback1Count = 0;
    int callback2Count = 0;
    int callback3Count = 0;
    
    registry.RegisterComponentChangeCallback<TransformComponent>(
        [&](EntityID, const TransformComponent&) { callback1Count++; }
    );
    registry.RegisterComponentChangeCallback<TransformComponent>(
        [&](EntityID, const TransformComponent&) { callback2Count++; }
    );
    registry.RegisterComponentChangeCallback<TransformComponent>(
        [&](EntityID, const TransformComponent&) { callback3Count++; }
    );
    
    EntityID entity{30, 1};
    TransformComponent component;
    
    registry.OnComponentChanged(entity, component);
    
    TEST_ASSERT(callback1Count == 1, "第一个回调应该被调用");
    TEST_ASSERT(callback2Count == 1, "第二个回调应该被调用");
    TEST_ASSERT(callback3Count == 1, "第三个回调应该被调用");
    
    return true;
}

bool Test_ComponentRegistry_CallbackExceptionHandling() {
    ComponentRegistry registry;
    registry.RegisterComponent<TransformComponent>();
    
    int callback1Count = 0;
    int callback2Count = 0;
    int callback3Count = 0;
    
    // 第一个回调正常
    registry.RegisterComponentChangeCallback<TransformComponent>(
        [&](EntityID, const TransformComponent&) { callback1Count++; }
    );
    
    // 第二个回调抛出异常
    registry.RegisterComponentChangeCallback<TransformComponent>(
        [&](EntityID, const TransformComponent&) {
            callback2Count++;
            throw std::runtime_error("测试异常");
        }
    );
    
    // 第三个回调正常
    registry.RegisterComponentChangeCallback<TransformComponent>(
        [&](EntityID, const TransformComponent&) { callback3Count++; }
    );
    
    EntityID entity{40, 1};
    TransformComponent component;
    
    // 应该不会因为异常而中断
    try {
        registry.OnComponentChanged(entity, component);
    } catch (...) {
        TEST_ASSERT(false, "异常不应该传播");
    }
    
    TEST_ASSERT(callback1Count == 1, "第一个回调应该被调用");
    TEST_ASSERT(callback2Count == 1, "第二个回调应该被调用（即使抛出异常）");
    TEST_ASSERT(callback3Count == 1, "第三个回调应该被调用（即使前面的回调抛出异常）");
    
    return true;
}

bool Test_ComponentRegistry_ThreadSafety() {
    ComponentRegistry registry;
    registry.RegisterComponent<TransformComponent>();
    
    std::atomic<int> callbackCount{0};
    const int numThreads = 4;
    const int callbacksPerThread = 100;
    
    // 注册回调
    registry.RegisterComponentChangeCallback<TransformComponent>(
        [&](EntityID, const TransformComponent&) {
            callbackCount++;
        }
    );
    
    // 创建多个线程同时触发事件
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&, i]() {
            EntityID entity{static_cast<uint32_t>(50 + i), 1};
            TransformComponent component;
            
            for (int j = 0; j < callbacksPerThread; ++j) {
                registry.OnComponentChanged(entity, component);
            }
        });
    }
    
    // 同时注册和取消注册回调
    std::vector<uint64_t> callbackIds;
    std::thread registerThread([&]() {
        for (int i = 0; i < 10; ++i) {
            uint64_t id = registry.RegisterComponentChangeCallback<TransformComponent>(
                [](EntityID, const TransformComponent&) {}
            );
            callbackIds.push_back(id);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    registerThread.join();
    
    // 验证结果
    TEST_ASSERT(callbackCount == numThreads * callbacksPerThread, 
                "所有回调应该都被调用");
    
    // 清理
    for (uint64_t id : callbackIds) {
        registry.UnregisterComponentChangeCallback(id);
    }
    
    return true;
}

bool Test_ComponentRegistry_TypeSafety() {
    ComponentRegistry registry;
    registry.RegisterComponent<TransformComponent>();
    registry.RegisterComponent<NameComponent>();
    
    int transformCallbackCount = 0;
    int nameCallbackCount = 0;
    
    registry.RegisterComponentChangeCallback<TransformComponent>(
        [&](EntityID, const TransformComponent&) { transformCallbackCount++; }
    );
    registry.RegisterComponentChangeCallback<NameComponent>(
        [&](EntityID, const NameComponent&) { nameCallbackCount++; }
    );
    
    EntityID entity{60, 1};
    TransformComponent transformComp;
    NameComponent nameComp("Test");
    
    // 只应该触发TransformComponent的回调
    registry.OnComponentChanged(entity, transformComp);
    TEST_ASSERT(transformCallbackCount == 1, "TransformComponent回调应该被调用");
    TEST_ASSERT(nameCallbackCount == 0, "NameComponent回调不应该被调用");
    
    // 只应该触发NameComponent的回调
    registry.OnComponentChanged(entity, nameComp);
    TEST_ASSERT(transformCallbackCount == 1, "TransformComponent回调不应该再被调用");
    TEST_ASSERT(nameCallbackCount == 1, "NameComponent回调应该被调用");
    
    return true;
}

// ============================================================================
// 1.4.3 测试ComponentArray变化通知
// ============================================================================

bool Test_ComponentArray_SetChangeCallback() {
    ComponentRegistry registry;
    registry.RegisterComponent<TransformComponent>();
    
    auto* array = registry.GetComponentArrayForTest<TransformComponent>();
    TEST_ASSERT(array != nullptr, "应该能获取组件数组");
    
    bool callbackCalled = false;
    EntityID calledEntity = EntityID::Invalid();
    
    array->SetChangeCallback([&](EntityID entity, const TransformComponent&) {
        callbackCalled = true;
        calledEntity = entity;
    });
    
    TEST_ASSERT(!callbackCalled, "设置回调不应该立即触发");
    
    return true;
}

bool Test_ComponentArray_ClearChangeCallback() {
    ComponentRegistry registry;
    registry.RegisterComponent<TransformComponent>();
    
    auto* array = registry.GetComponentArrayForTest<TransformComponent>();
    TEST_ASSERT(array != nullptr, "应该能获取组件数组");
    
    int callbackCount = 0;
    
    array->SetChangeCallback([&](EntityID, const TransformComponent&) {
        callbackCount++;
    });
    
    EntityID entity{70, 1};
    TransformComponent component;
    
    // 添加组件应该触发回调
    registry.AddComponent(entity, component);
    TEST_ASSERT(callbackCount == 1, "回调应该被调用");
    
    // 清除回调
    array->ClearChangeCallback();
    
    // 再次添加组件不应该触发回调
    EntityID entity2{71, 1};
    registry.AddComponent(entity2, component);
    TEST_ASSERT(callbackCount == 1, "清除回调后不应该再调用");
    
    return true;
}

bool Test_ComponentArray_AddTriggersCallback() {
    ComponentRegistry registry;
    registry.RegisterComponent<TransformComponent>();
    
    auto* array = registry.GetComponentArrayForTest<TransformComponent>();
    
    int callbackCount = 0;
    std::vector<EntityID> calledEntities;
    
    array->SetChangeCallback([&](EntityID entity, const TransformComponent&) {
        callbackCount++;
        calledEntities.push_back(entity);
    });
    
    EntityID entity1{80, 1};
    EntityID entity2{81, 1};
    EntityID entity3{82, 1};
    
    TransformComponent component1;
    component1.SetPosition(Vector3(1.0f, 2.0f, 3.0f));
    
    TransformComponent component2;
    component2.SetPosition(Vector3(4.0f, 5.0f, 6.0f));
    
    TransformComponent component3;
    component3.SetPosition(Vector3(7.0f, 8.0f, 9.0f));
    
    // 添加组件应该触发回调
    registry.AddComponent(entity1, component1);
    registry.AddComponent(entity2, component2);
    registry.AddComponent(entity3, component3);
    
    TEST_ASSERT(callbackCount == 3, "应该调用3次回调");
    TEST_ASSERT(calledEntities.size() == 3, "应该记录3个实体");
    TEST_ASSERT(calledEntities[0].index == 80, "第一个实体应该正确");
    TEST_ASSERT(calledEntities[1].index == 81, "第二个实体应该正确");
    TEST_ASSERT(calledEntities[2].index == 82, "第三个实体应该正确");
    
    return true;
}

bool Test_ComponentArray_AddMoveSemantics() {
    ComponentRegistry registry;
    registry.RegisterComponent<TransformComponent>();
    
    auto* array = registry.GetComponentArrayForTest<TransformComponent>();
    
    int callbackCount = 0;
    Vector3 receivedPosition;
    
    array->SetChangeCallback([&](EntityID, const TransformComponent& component) {
        callbackCount++;
        receivedPosition = component.GetPosition();
    });
    
    EntityID entity{90, 1};
    TransformComponent component;
    component.SetPosition(Vector3(10.0f, 11.0f, 12.0f));
    
    // 使用移动语义添加
    registry.AddComponent(entity, std::move(component));
    
    TEST_ASSERT(callbackCount == 1, "回调应该被调用");
    TEST_ASSERT(receivedPosition.isApprox(Vector3(10.0f, 11.0f, 12.0f)), 
                "应该收到正确的组件数据");
    
    return true;
}

bool Test_ComponentArray_CallbackExceptionHandling() {
    ComponentRegistry registry;
    registry.RegisterComponent<TransformComponent>();
    
    auto* array = registry.GetComponentArrayForTest<TransformComponent>();
    
    int callbackCount = 0;
    
    // 设置一个会抛出异常的回调
    array->SetChangeCallback([&](EntityID, const TransformComponent&) {
        callbackCount++;
        throw std::runtime_error("测试异常");
    });
    
    EntityID entity{100, 1};
    TransformComponent component;
    
    // 添加组件应该成功，即使回调抛出异常
    try {
        registry.AddComponent(entity, component);
        TEST_ASSERT(true, "添加组件应该成功");
    } catch (...) {
        TEST_ASSERT(false, "异常不应该传播");
    }
    
    TEST_ASSERT(callbackCount == 1, "回调应该被调用");
    TEST_ASSERT(registry.HasComponent<TransformComponent>(entity), 
                "组件应该被成功添加");
    
    return true;
}

// ============================================================================
// 主函数
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "组件变化事件系统测试" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // 1.4.1 测试事件类型定义
    std::cout << "--- 1.4.1 测试事件类型定义 ---" << std::endl;
    RUN_TEST(Test_ComponentChangeEvent_Construction);
    RUN_TEST(Test_TransformComponentChangeEvent_Construction);
    RUN_TEST(Test_TransformComponentChangeEvent_Inheritance);
    std::cout << std::endl;
    
    // 1.4.2 测试ComponentRegistry回调机制
    std::cout << "--- 1.4.2 测试ComponentRegistry回调机制 ---" << std::endl;
    RUN_TEST(Test_ComponentRegistry_CallbackRegistration);
    RUN_TEST(Test_ComponentRegistry_CallbackInvocation);
    RUN_TEST(Test_ComponentRegistry_CallbackUnregistration);
    RUN_TEST(Test_ComponentRegistry_MultipleCallbacks);
    RUN_TEST(Test_ComponentRegistry_CallbackExceptionHandling);
    RUN_TEST(Test_ComponentRegistry_ThreadSafety);
    RUN_TEST(Test_ComponentRegistry_TypeSafety);
    std::cout << std::endl;
    
    // 1.4.3 测试ComponentArray变化通知
    std::cout << "--- 1.4.3 测试ComponentArray变化通知 ---" << std::endl;
    RUN_TEST(Test_ComponentArray_SetChangeCallback);
    RUN_TEST(Test_ComponentArray_ClearChangeCallback);
    RUN_TEST(Test_ComponentArray_AddTriggersCallback);
    RUN_TEST(Test_ComponentArray_AddMoveSemantics);
    RUN_TEST(Test_ComponentArray_CallbackExceptionHandling);
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

