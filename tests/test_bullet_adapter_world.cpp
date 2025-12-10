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
 * @file test_bullet_adapter_world.cpp
 * @brief Bullet 适配器世界适配器测试
 * 
 * 测试 BulletWorldAdapter 的核心功能：
 * - 世界初始化
 * - 配置同步
 * - Step() 方法
 * - 实体到刚体映射
 */

#ifdef USE_BULLET_PHYSICS

#include "render/physics/bullet_adapter/bullet_world_adapter.h"
#include "render/physics/physics_config.h"
#include "render/physics/bullet_adapter/eigen_to_bullet.h"
#include "render/ecs/entity.h"
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <BulletCollision/CollisionShapes/btSphereShape.h>
#include <BulletCollision/CollisionDispatch/btCollisionObject.h>
#include <iostream>
#include <cmath>
#include <memory>

using namespace Render;
using namespace Render::Physics;
using namespace Render::Physics::BulletAdapter;

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

#define TEST_ASSERT_NEAR(actual, expected, tolerance, message) \
    do { \
        g_testCount++; \
        float diff = std::abs((actual) - (expected)); \
        if (diff > (tolerance)) { \
            std::cerr << "❌ 测试失败: " << message << std::endl; \
            std::cerr << "   位置: " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::cerr << "   实际值: " << (actual) << std::endl; \
            std::cerr << "   期望值: " << (expected) << std::endl; \
            std::cerr << "   差值: " << diff << " (容忍度: " << (tolerance) << ")" << std::endl; \
            g_failedCount++; \
            return false; \
        } \
        g_passedCount++; \
    } while(0)

#define RUN_TEST(test_func) \
    do { \
        std::cout << "运行测试: " << #test_func << "..." << std::endl; \
        std::cout.flush(); \
        bool result = false; \
        try { \
            result = test_func(); \
        } catch (const std::exception& e) { \
            std::cerr << "异常: " << #test_func << " - " << e.what() << std::endl; \
            result = false; \
        } catch (...) { \
            std::cerr << "未知异常: " << #test_func << std::endl; \
            result = false; \
        } \
        if (result) { \
            std::cout << "✓ " << #test_func << " 通过" << std::endl; \
            std::cout.flush(); \
        } else { \
            std::cout << "✗ " << #test_func << " 失败" << std::endl; \
            std::cout.flush(); \
        } \
    } while(0)

// ============================================================================
// 2.1.1 世界初始化测试
// ============================================================================

bool Test_World_Initialization() {
    PhysicsConfig config = PhysicsConfig::Default();
    BulletWorldAdapter adapter(config);
    
    // 验证世界已创建
    TEST_ASSERT(adapter.GetBulletWorld() != nullptr, "Bullet 世界应该已创建");
    
    // 验证世界不为空
    btDiscreteDynamicsWorld* world = adapter.GetBulletWorld();
    TEST_ASSERT(world != nullptr, "GetBulletWorld() 应该返回非空指针");
    
    return true;
}

bool Test_World_Initialization_WithCustomConfig() {
    PhysicsConfig config;
    config.gravity = Vector3(0.0f, -10.0f, 0.0f);
    config.fixedDeltaTime = 1.0f / 120.0f;
    config.maxSubSteps = 10;
    config.solverIterations = 20;
    config.positionIterations = 8;
    config.enableCCD = true;
    config.enableSleeping = false;
    
    BulletWorldAdapter adapter(config);
    
    // 验证世界已创建
    TEST_ASSERT(adapter.GetBulletWorld() != nullptr, "Bullet 世界应该已创建");
    
    // 验证配置已应用（通过 GetGravity 验证）
    Vector3 gravity = adapter.GetGravity();
    TEST_ASSERT(gravity.isApprox(config.gravity, 0.0001f), "重力应该已正确设置");
    
    return true;
}

// ============================================================================
// 2.1.2 世界配置同步测试
// ============================================================================

bool Test_Config_SyncGravity() {
    PhysicsConfig config = PhysicsConfig::Default();
    BulletWorldAdapter adapter(config);
    
    // 测试设置重力
    Vector3 customGravity(0.0f, -15.0f, 0.0f);
    adapter.SetGravity(customGravity);
    
    Vector3 retrievedGravity = adapter.GetGravity();
    TEST_ASSERT(retrievedGravity.isApprox(customGravity, 0.0001f), 
                "重力应该正确同步");
    
    return true;
}

bool Test_Config_SyncSolverIterations() {
    PhysicsConfig config = PhysicsConfig::Default();
    config.solverIterations = 15;
    config.positionIterations = 6;
    
    BulletWorldAdapter adapter(config);
    
    // 验证求解器迭代次数已设置
    btDiscreteDynamicsWorld* world = adapter.GetBulletWorld();
    if (world) {
        btContactSolverInfo& solverInfo = world->getSolverInfo();
        TEST_ASSERT(solverInfo.m_numIterations == config.solverIterations,
                    "求解器迭代次数应该正确设置");
        
        // 验证位置迭代（通过 splitImpulse）
        if (config.positionIterations > 0) {
            TEST_ASSERT(solverInfo.m_splitImpulse == true,
                        "位置迭代应该启用 split impulse");
        }
    }
    
    return true;
}

bool Test_Config_SyncCCD() {
    PhysicsConfig config = PhysicsConfig::Default();
    config.enableCCD = true;
    
    BulletWorldAdapter adapter(config);
    
    // 验证 CCD 已启用
    btDiscreteDynamicsWorld* world = adapter.GetBulletWorld();
    if (world) {
        btDispatcherInfo& dispatchInfo = world->getDispatchInfo();
        TEST_ASSERT(dispatchInfo.m_useContinuous == config.enableCCD,
                    "CCD 应该正确启用");
    }
    
    // 测试禁用 CCD
    config.enableCCD = false;
    adapter.SyncConfig(config);
    
    if (world) {
        btDispatcherInfo& dispatchInfo = world->getDispatchInfo();
        TEST_ASSERT(dispatchInfo.m_useContinuous == false,
                    "CCD 应该正确禁用");
    }
    
    return true;
}

bool Test_Config_SyncConfig() {
    PhysicsConfig config1 = PhysicsConfig::Default();
    config1.gravity = Vector3(0.0f, -9.81f, 0.0f);
    config1.solverIterations = 10;
    
    BulletWorldAdapter adapter(config1);
    
    // 更新配置
    PhysicsConfig config2 = PhysicsConfig::Default();
    config2.gravity = Vector3(0.0f, -20.0f, 0.0f);
    config2.solverIterations = 25;
    config2.enableCCD = true;
    
    adapter.SyncConfig(config2);
    
    // 验证新配置已应用
    Vector3 gravity = adapter.GetGravity();
    TEST_ASSERT(gravity.isApprox(config2.gravity, 0.0001f),
                "更新后的重力应该正确应用");
    
    btDiscreteDynamicsWorld* world = adapter.GetBulletWorld();
    if (world) {
        btContactSolverInfo& solverInfo = world->getSolverInfo();
        TEST_ASSERT(solverInfo.m_numIterations == config2.solverIterations,
                    "更新后的求解器迭代次数应该正确应用");
        
        btDispatcherInfo& dispatchInfo = world->getDispatchInfo();
        TEST_ASSERT(dispatchInfo.m_useContinuous == config2.enableCCD,
                    "更新后的 CCD 设置应该正确应用");
    }
    
    return true;
}

// ============================================================================
// 2.1.3 Step() 方法测试
// ============================================================================

bool Test_Step_Basic() {
    PhysicsConfig config = PhysicsConfig::Default();
    config.fixedDeltaTime = 1.0f / 60.0f;
    config.maxSubSteps = 5;
    
    BulletWorldAdapter adapter(config);
    
    // 执行一步物理更新
    float deltaTime = 0.016f;  // 约 60 FPS
    adapter.Step(deltaTime);
    
    // 验证没有崩溃（基本测试）
    TEST_ASSERT(adapter.GetBulletWorld() != nullptr,
                "Step() 后世界应该仍然有效");
    
    return true;
}

bool Test_Step_FixedTimeStep() {
    PhysicsConfig config = PhysicsConfig::Default();
    config.fixedDeltaTime = 1.0f / 60.0f;  // 固定时间步长 1/60 秒
    config.maxSubSteps = 5;
    
    BulletWorldAdapter adapter(config);
    
    // 执行一个较大的时间步长，应该被分割为多个子步
    float largeDeltaTime = 0.1f;  // 0.1 秒，应该被分割为多个 1/60 秒的子步
    adapter.Step(largeDeltaTime);
    
    // 验证没有崩溃
    TEST_ASSERT(adapter.GetBulletWorld() != nullptr,
                "大时间步长应该正确处理");
    
    return true;
}

bool Test_Step_MultipleSteps() {
    PhysicsConfig config = PhysicsConfig::Default();
    BulletWorldAdapter adapter(config);
    
    // 执行多步物理更新
    for (int i = 0; i < 10; ++i) {
        adapter.Step(0.016f);
    }
    
    // 验证没有崩溃
    TEST_ASSERT(adapter.GetBulletWorld() != nullptr,
                "多步更新后世界应该仍然有效");
    
    return true;
}

bool Test_Step_ZeroDeltaTime() {
    PhysicsConfig config = PhysicsConfig::Default();
    BulletWorldAdapter adapter(config);
    
    // 执行零时间步长（应该安全处理）
    adapter.Step(0.0f);
    
    // 验证没有崩溃
    TEST_ASSERT(adapter.GetBulletWorld() != nullptr,
                "零时间步长应该安全处理");
    
    return true;
}

bool Test_Step_NegativeDeltaTime() {
    PhysicsConfig config = PhysicsConfig::Default();
    BulletWorldAdapter adapter(config);
    
    // 执行负时间步长（应该安全处理或忽略）
    adapter.Step(-0.016f);
    
    // 验证没有崩溃
    TEST_ASSERT(adapter.GetBulletWorld() != nullptr,
                "负时间步长应该安全处理");
    
    return true;
}

// ============================================================================
// 2.1.4 实体到刚体映射测试
// ============================================================================

bool Test_Mapping_AddRigidBody() {
    PhysicsConfig config = PhysicsConfig::Default();
    BulletWorldAdapter adapter(config);
    
    // 创建测试刚体
    btSphereShape* shape = new btSphereShape(1.0f);
    btVector3 localInertia(0.0f, 0.0f, 0.0f);
    shape->calculateLocalInertia(1.0f, localInertia);
    btRigidBody::btRigidBodyConstructionInfo rbInfo(1.0f, nullptr, shape, localInertia);
    btRigidBody* rigidBody = new btRigidBody(rbInfo);
    
    // 创建测试实体 ID
    ECS::EntityID entity1{1, 0};
    ECS::EntityID entity2{2, 0};
    
    // 添加映射
    adapter.AddRigidBodyMapping(entity1, rigidBody);
    
    // 验证映射已添加
    btRigidBody* retrieved = adapter.GetRigidBody(entity1);
    TEST_ASSERT(retrieved == rigidBody, "应该能通过实体 ID 获取刚体");
    
    ECS::EntityID retrievedEntity = adapter.GetEntity(rigidBody);
    TEST_ASSERT(retrievedEntity == entity1, "应该能通过刚体获取实体 ID");
    
    // 清理
    delete rigidBody;
    delete shape;
    
    return true;
}

bool Test_Mapping_RemoveRigidBodyByEntity() {
    PhysicsConfig config = PhysicsConfig::Default();
    BulletWorldAdapter adapter(config);
    
    // 创建测试刚体
    btSphereShape* shape = new btSphereShape(1.0f);
    btVector3 localInertia(0.0f, 0.0f, 0.0f);
    shape->calculateLocalInertia(1.0f, localInertia);
    btRigidBody::btRigidBodyConstructionInfo rbInfo(1.0f, nullptr, shape, localInertia);
    btRigidBody* rigidBody = new btRigidBody(rbInfo);
    
    ECS::EntityID entity{1, 0};
    
    // 添加映射
    adapter.AddRigidBodyMapping(entity, rigidBody);
    
    // 移除映射（通过实体 ID）
    adapter.RemoveRigidBodyMapping(entity);
    
    // 验证映射已移除
    btRigidBody* retrieved = adapter.GetRigidBody(entity);
    TEST_ASSERT(retrieved == nullptr, "移除后应该无法通过实体 ID 获取刚体");
    
    ECS::EntityID retrievedEntity = adapter.GetEntity(rigidBody);
    TEST_ASSERT(!retrievedEntity.IsValid(), "移除后应该无法通过刚体获取实体 ID");
    
    // 清理
    delete rigidBody;
    delete shape;
    
    return true;
}

bool Test_Mapping_RemoveRigidBodyByPointer() {
    PhysicsConfig config = PhysicsConfig::Default();
    BulletWorldAdapter adapter(config);
    
    // 创建测试刚体
    btSphereShape* shape = new btSphereShape(1.0f);
    btVector3 localInertia(0.0f, 0.0f, 0.0f);
    shape->calculateLocalInertia(1.0f, localInertia);
    btRigidBody::btRigidBodyConstructionInfo rbInfo(1.0f, nullptr, shape, localInertia);
    btRigidBody* rigidBody = new btRigidBody(rbInfo);
    
    ECS::EntityID entity{1, 0};
    
    // 添加映射
    adapter.AddRigidBodyMapping(entity, rigidBody);
    
    // 移除映射（通过刚体指针）
    adapter.RemoveRigidBodyMapping(rigidBody);
    
    // 验证映射已移除
    btRigidBody* retrieved = adapter.GetRigidBody(entity);
    TEST_ASSERT(retrieved == nullptr, "移除后应该无法通过实体 ID 获取刚体");
    
    ECS::EntityID retrievedEntity = adapter.GetEntity(rigidBody);
    TEST_ASSERT(!retrievedEntity.IsValid(), "移除后应该无法通过刚体获取实体 ID");
    
    // 清理
    delete rigidBody;
    delete shape;
    
    return true;
}

bool Test_Mapping_UpdateMapping() {
    PhysicsConfig config = PhysicsConfig::Default();
    BulletWorldAdapter adapter(config);
    
    // 创建两个测试刚体
    btSphereShape* shape1 = new btSphereShape(1.0f);
    btVector3 localInertia1(0.0f, 0.0f, 0.0f);
    shape1->calculateLocalInertia(1.0f, localInertia1);
    btRigidBody::btRigidBodyConstructionInfo rbInfo1(1.0f, nullptr, shape1, localInertia1);
    btRigidBody* rigidBody1 = new btRigidBody(rbInfo1);
    
    btSphereShape* shape2 = new btSphereShape(2.0f);
    btVector3 localInertia2(0.0f, 0.0f, 0.0f);
    shape2->calculateLocalInertia(2.0f, localInertia2);
    btRigidBody::btRigidBodyConstructionInfo rbInfo2(2.0f, nullptr, shape2, localInertia2);
    btRigidBody* rigidBody2 = new btRigidBody(rbInfo2);
    
    ECS::EntityID entity{1, 0};
    
    // 添加第一个映射
    adapter.AddRigidBodyMapping(entity, rigidBody1);
    TEST_ASSERT(adapter.GetRigidBody(entity) == rigidBody1,
                "第一个映射应该正确");
    
    // 更新为第二个刚体
    adapter.AddRigidBodyMapping(entity, rigidBody2);
    TEST_ASSERT(adapter.GetRigidBody(entity) == rigidBody2,
                "更新后的映射应该正确");
    
    // 验证旧的映射已移除
    ECS::EntityID oldEntity = adapter.GetEntity(rigidBody1);
    TEST_ASSERT(!oldEntity.IsValid(), "旧的映射应该已移除");
    
    // 清理
    delete rigidBody2;
    delete shape2;
    delete rigidBody1;
    delete shape1;
    
    return true;
}

bool Test_Mapping_MultipleEntities() {
    PhysicsConfig config = PhysicsConfig::Default();
    BulletWorldAdapter adapter(config);
    
    // 创建多个测试刚体和实体
    const int count = 5;
    std::vector<btRigidBody*> rigidBodies;
    std::vector<ECS::EntityID> entities;
    
    for (int i = 0; i < count; ++i) {
        btSphereShape* shape = new btSphereShape(1.0f);
        btVector3 localInertia(0.0f, 0.0f, 0.0f);
        shape->calculateLocalInertia(1.0f, localInertia);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(1.0f, nullptr, shape, localInertia);
        btRigidBody* rigidBody = new btRigidBody(rbInfo);
        
        ECS::EntityID entity{static_cast<uint32_t>(i + 1), 0};
        
        adapter.AddRigidBodyMapping(entity, rigidBody);
        
        rigidBodies.push_back(rigidBody);
        entities.push_back(entity);
    }
    
    // 验证所有映射都正确
    for (int i = 0; i < count; ++i) {
        btRigidBody* retrieved = adapter.GetRigidBody(entities[i]);
        TEST_ASSERT(retrieved == rigidBodies[i],
                    "应该能正确获取所有映射的刚体");
        
        ECS::EntityID retrievedEntity = adapter.GetEntity(rigidBodies[i]);
        TEST_ASSERT(retrievedEntity == entities[i],
                    "应该能正确获取所有映射的实体 ID");
    }
    
    // 清理
    for (auto* rb : rigidBodies) {
        delete rb->getCollisionShape();
        delete rb;
    }
    
    return true;
}

bool Test_Mapping_InvalidEntity() {
    PhysicsConfig config = PhysicsConfig::Default();
    BulletWorldAdapter adapter(config);
    
    // 创建测试刚体
    btSphereShape* shape = new btSphereShape(1.0f);
    btVector3 localInertia(0.0f, 0.0f, 0.0f);
    shape->calculateLocalInertia(1.0f, localInertia);
    btRigidBody::btRigidBodyConstructionInfo rbInfo(1.0f, nullptr, shape, localInertia);
    btRigidBody* rigidBody = new btRigidBody(rbInfo);
    
    // 尝试使用无效实体 ID 添加映射（应该被忽略）
    ECS::EntityID invalidEntity = ECS::EntityID::Invalid();
    adapter.AddRigidBodyMapping(invalidEntity, rigidBody);
    
    // 验证映射未添加
    btRigidBody* retrieved = adapter.GetRigidBody(invalidEntity);
    TEST_ASSERT(retrieved == nullptr, "无效实体 ID 不应该添加映射");
    
    // 清理
    delete rigidBody;
    delete shape;
    
    return true;
}

// ============================================================================
// 主测试函数
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Bullet 适配器世界适配器测试" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 2.1.1 世界初始化测试
    std::cout << "\n--- 2.1.1 世界初始化测试 ---" << std::endl;
    RUN_TEST(Test_World_Initialization);
    RUN_TEST(Test_World_Initialization_WithCustomConfig);
    
    // 2.1.2 世界配置同步测试
    std::cout << "\n--- 2.1.2 世界配置同步测试 ---" << std::endl;
    RUN_TEST(Test_Config_SyncGravity);
    RUN_TEST(Test_Config_SyncSolverIterations);
    RUN_TEST(Test_Config_SyncCCD);
    RUN_TEST(Test_Config_SyncConfig);
    
    // 2.1.3 Step() 方法测试
    std::cout << "\n--- 2.1.3 Step() 方法测试 ---" << std::endl;
    RUN_TEST(Test_Step_Basic);
    RUN_TEST(Test_Step_FixedTimeStep);
    RUN_TEST(Test_Step_MultipleSteps);
    RUN_TEST(Test_Step_ZeroDeltaTime);
    RUN_TEST(Test_Step_NegativeDeltaTime);
    
    // 2.1.4 实体到刚体映射测试
    std::cout << "\n--- 2.1.4 实体到刚体映射测试 ---" << std::endl;
    RUN_TEST(Test_Mapping_AddRigidBody);
    RUN_TEST(Test_Mapping_RemoveRigidBodyByEntity);
    RUN_TEST(Test_Mapping_RemoveRigidBodyByPointer);
    RUN_TEST(Test_Mapping_UpdateMapping);
    RUN_TEST(Test_Mapping_MultipleEntities);
    RUN_TEST(Test_Mapping_InvalidEntity);
    
    // 输出测试结果
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

#else  // USE_BULLET_PHYSICS

#include <iostream>

int main() {
    std::cout << "Bullet Physics 未启用，跳过测试" << std::endl;
    return 0;
}

#endif  // USE_BULLET_PHYSICS

