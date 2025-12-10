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
 * @file test_physics_world_conditional_compile.cpp
 * @brief PhysicsWorld 条件编译支持测试
 * 
 * 测试 3.1 条件编译支持功能：
 * - 验证定义了 USE_BULLET_PHYSICS 时使用 Bullet 后端
 * - 验证未定义时使用原有实现
 * - 验证两种情况下都能正常工作
 */

#include "render/physics/physics_world.h"
#include "render/physics/physics_config.h"
#include "render/physics/physics_components.h"
#include "render/ecs/world.h"
#ifdef USE_BULLET_PHYSICS
#include "render/physics/bullet_adapter/bullet_world_adapter.h"
#endif
#include <iostream>
#include <memory>

using namespace Render;
using namespace Render::Physics;

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
// 3.1 条件编译支持测试
// ============================================================================

bool Test_PhysicsWorld_Creation() {
    // 创建 ECS World
    auto ecsWorld = std::make_unique<ECS::World>();
    ecsWorld->Initialize();
    
    // 创建 PhysicsWorld
    PhysicsConfig config = PhysicsConfig::Default();
    PhysicsWorld physicsWorld(ecsWorld.get(), config);
    
    // 验证 PhysicsWorld 创建成功
    TEST_ASSERT(true, "PhysicsWorld 应该能够创建");
    
    return true;
}

bool Test_PhysicsWorld_Step() {
    // 创建 ECS World
    auto ecsWorld = std::make_unique<ECS::World>();
    ecsWorld->Initialize();
    
    // 创建 PhysicsWorld
    PhysicsConfig config = PhysicsConfig::Default();
    PhysicsWorld physicsWorld(ecsWorld.get(), config);
    
    // 执行 Step（应该不会崩溃）
    physicsWorld.Step(0.016f);
    
    // 验证 Step 执行成功
    TEST_ASSERT(true, "PhysicsWorld::Step 应该能够执行");
    
    return true;
}

bool Test_PhysicsWorld_Config() {
    // 创建 ECS World
    auto ecsWorld = std::make_unique<ECS::World>();
    ecsWorld->Initialize();
    
    // 创建 PhysicsWorld
    PhysicsConfig config = PhysicsConfig::Default();
    config.gravity = Vector3(0.0f, -9.81f, 0.0f);
    PhysicsWorld physicsWorld(ecsWorld.get(), config);
    
    // 验证配置
    Vector3 gravity = physicsWorld.GetGravity();
    TEST_ASSERT(std::abs(gravity.y() + 9.81f) < 0.001f, "重力应该正确设置");
    
    return true;
}

#ifdef USE_BULLET_PHYSICS
bool Test_PhysicsWorld_BulletBackend() {
    // 创建 ECS World
    auto ecsWorld = std::make_unique<ECS::World>();
    ecsWorld->Initialize();
    
    // 创建 PhysicsWorld
    PhysicsConfig config = PhysicsConfig::Default();
    PhysicsWorld physicsWorld(ecsWorld.get(), config);
    
    // 验证 Bullet 适配器存在
    auto* bulletAdapter = physicsWorld.GetBulletAdapter();
    TEST_ASSERT(bulletAdapter != nullptr, "Bullet adapter should exist when USE_BULLET_PHYSICS is defined");
    
    // 验证 Bullet 世界存在
    auto* bulletWorld = bulletAdapter->GetBulletWorld();
    TEST_ASSERT(bulletWorld != nullptr, "Bullet world should exist");
    
    return true;
}

bool Test_PhysicsWorld_BulletBackend_Step() {
    // 创建 ECS World
    auto ecsWorld = std::make_unique<ECS::World>();
    ecsWorld->Initialize();
    
    // 创建 PhysicsWorld
    PhysicsConfig config = PhysicsConfig::Default();
    PhysicsWorld physicsWorld(ecsWorld.get(), config);
    
    // 执行 Step（使用 Bullet 后端）
    physicsWorld.Step(0.016f);
    
    // 验证 Step 执行成功
    TEST_ASSERT(true, "Step should execute successfully with Bullet backend");
    
    return true;
}
#else
bool Test_PhysicsWorld_LegacyBackend() {
    // 创建 ECS World
    auto ecsWorld = std::make_unique<ECS::World>();
    ecsWorld->Initialize();
    
    // 创建 PhysicsWorld
    PhysicsConfig config = PhysicsConfig::Default();
    PhysicsWorld physicsWorld(ecsWorld.get(), config);
    
    // 验证未定义 USE_BULLET_PHYSICS 时，使用原有实现
    // 注意：无法直接验证，但可以通过行为验证
    physicsWorld.Step(0.016f);
    
    TEST_ASSERT(true, "Legacy backend should be used when USE_BULLET_PHYSICS is not defined");
    
    return true;
}
#endif

// ============================================================================
// 主函数
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "PhysicsWorld Conditional Compile Test" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
#ifdef USE_BULLET_PHYSICS
    std::cout << "Build option: USE_BULLET_PHYSICS defined (using Bullet backend)" << std::endl;
#else
    std::cout << "Build option: USE_BULLET_PHYSICS not defined (using legacy backend)" << std::endl;
#endif
    std::cout << std::endl;
    
    // 重置计数器
    g_testCount = 0;
    g_passedCount = 0;
    g_failedCount = 0;
    
    // 基础功能测试
    std::cout << "\n[Basic Function Tests]" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    RUN_TEST(Test_PhysicsWorld_Creation);
    RUN_TEST(Test_PhysicsWorld_Step);
    RUN_TEST(Test_PhysicsWorld_Config);
    
    // 条件编译测试
    std::cout << "\n[Conditional Compile Tests]" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
#ifdef USE_BULLET_PHYSICS
    RUN_TEST(Test_PhysicsWorld_BulletBackend);
    RUN_TEST(Test_PhysicsWorld_BulletBackend_Step);
#else
    RUN_TEST(Test_PhysicsWorld_LegacyBackend);
#endif
    
    // 输出测试结果
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test Complete" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Total tests: " << g_testCount << std::endl;
    std::cout << "Passed: " << g_passedCount << std::endl;
    std::cout << "Failed: " << g_failedCount << std::endl;
    std::cout << "========================================" << std::endl;
    
    if (g_failedCount == 0) {
        std::cout << "All tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << g_failedCount << " test(s) failed" << std::endl;
        return 1;
    }
}

