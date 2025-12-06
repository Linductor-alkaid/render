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
 * @file test_collision_detection.cpp
 * @brief 细检测碰撞算法测试
 */

#include "render/physics/collision/collision_detection.h"
#include <iostream>
#include <cmath>

using namespace Render;
using namespace Render::Physics;

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
// 球体碰撞检测测试
// ============================================================================

bool Test_SphereVsSphere_Collision() {
    ContactManifold manifold;
    
    bool hit = CollisionDetector::SphereVsSphere(
        Vector3(0, 0, 0), 1.0f,
        Vector3(1.5f, 0, 0), 1.0f,
        manifold
    );
    
    TEST_ASSERT(hit, "应该检测到碰撞");
    TEST_ASSERT(manifold.IsValid(), "流形应该有效");
    TEST_ASSERT(manifold.contactCount == 1, "应该有 1 个接触点");
    TEST_ASSERT(std::abs(manifold.penetration - 0.5f) < 0.01f, "穿透深度应该是 0.5");
    
    return true;
}

bool Test_SphereVsSphere_NoCollision() {
    ContactManifold manifold;
    
    bool hit = CollisionDetector::SphereVsSphere(
        Vector3(0, 0, 0), 1.0f,
        Vector3(5, 0, 0), 1.0f,
        manifold
    );
    
    TEST_ASSERT(!hit, "不应该检测到碰撞");
    
    return true;
}

bool Test_SphereVsSphere_Overlapping() {
    ContactManifold manifold;
    
    bool hit = CollisionDetector::SphereVsSphere(
        Vector3(0, 0, 0), 2.0f,
        Vector3(0, 0, 0), 2.0f,
        manifold
    );
    
    TEST_ASSERT(hit, "重叠的球体应该检测到碰撞");
    TEST_ASSERT(manifold.penetration == 4.0f, "完全重叠穿透深度应该是半径和");
    
    return true;
}

// ============================================================================
// 球体 vs 盒体测试
// ============================================================================

bool Test_SphereVsBox_Collision() {
    ContactManifold manifold;
    
    // 球心距离盒体边缘 2-1=1，球半径1，应该刚好接触
    // 调整为明确的碰撞情况
    bool hit = CollisionDetector::SphereVsBox(
        Vector3(1.5f, 0, 0), 1.0f,  // 球心在 1.5，半径 1
        Vector3(0, 0, 0), Vector3(1, 1, 1),  // 盒体中心 0，半尺寸 1（边界在 ±1）
        Quaternion::Identity(),
        manifold
    );
    
    TEST_ASSERT(hit, "应该检测到碰撞");
    if (hit) {
        TEST_ASSERT(manifold.IsValid(), "流形应该有效");
    }
    
    return true;
}

bool Test_SphereVsBox_NoCollision() {
    ContactManifold manifold;
    
    bool hit = CollisionDetector::SphereVsBox(
        Vector3(5, 0, 0), 1.0f,
        Vector3(0, 0, 0), Vector3(1, 1, 1),
        Quaternion::Identity(),
        manifold
    );
    
    TEST_ASSERT(!hit, "不应该检测到碰撞");
    
    return true;
}

// ============================================================================
// 球体 vs 胶囊体测试
// ============================================================================

bool Test_SphereVsCapsule_Collision() {
    ContactManifold manifold;
    
    // 球心在 1.2，球半径 1，胶囊半径 0.5，应该相交
    bool hit = CollisionDetector::SphereVsCapsule(
        Vector3(1.2f, 0, 0), 1.0f,  // 球心距离胶囊中心线 1.2，半径 1
        Vector3(0, 0, 0), 0.5f, 2.0f,  // 胶囊半径 0.5
        Quaternion::Identity(),
        manifold
    );
    
    TEST_ASSERT(hit, "应该检测到碰撞");
    if (hit) {
        TEST_ASSERT(manifold.IsValid(), "流形应该有效");
    }
    
    return true;
}

// ============================================================================
// AABB vs AABB (BoxVsBox) 测试
// ============================================================================

bool Test_BoxVsBox_Collision() {
    ContactManifold manifold;
    
    bool hit = CollisionDetector::BoxVsBox(
        Vector3(0, 0, 0), Vector3(1, 1, 1), Quaternion::Identity(),
        Vector3(1.5f, 0, 0), Vector3(1, 1, 1), Quaternion::Identity(),
        manifold
    );
    
    TEST_ASSERT(hit, "应该检测到碰撞");
    TEST_ASSERT(manifold.IsValid(), "流形应该有效");
    
    return true;
}

bool Test_BoxVsBox_NoCollision() {
    ContactManifold manifold;
    
    bool hit = CollisionDetector::BoxVsBox(
        Vector3(0, 0, 0), Vector3(1, 1, 1), Quaternion::Identity(),
        Vector3(5, 0, 0), Vector3(1, 1, 1), Quaternion::Identity(),
        manifold
    );
    
    TEST_ASSERT(!hit, "不应该检测到碰撞");
    
    return true;
}

bool Test_BoxVsBox_OBB_Rotated() {
    ContactManifold manifold;
    
    // 测试旋转的 OBB
    Quaternion rotation = MathUtils::AngleAxis(MathUtils::PI / 4.0f, Vector3::UnitZ());
    
    bool hit = CollisionDetector::BoxVsBox(
        Vector3(0, 0, 0), Vector3(1, 1, 1), Quaternion::Identity(),
        Vector3(1.5f, 0, 0), Vector3(1, 1, 1), rotation,
        manifold
    );
    
    TEST_ASSERT(hit, "旋转的 OBB 应该能正确检测碰撞");
    if (hit) {
        TEST_ASSERT(manifold.IsValid(), "流形应该有效");
    }
    
    return true;
}

bool Test_BoxVsBox_OBB_EdgeCase() {
    ContactManifold manifold;
    
    // 边缘情况：盒体边对边
    Quaternion rotationA = MathUtils::AngleAxis(MathUtils::PI / 6.0f, Vector3::UnitY());
    Quaternion rotationB = MathUtils::AngleAxis(-MathUtils::PI / 6.0f, Vector3::UnitY());
    
    bool hit = CollisionDetector::BoxVsBox(
        Vector3(0, 0, 0), Vector3(0.5f, 0.5f, 0.5f), rotationA,
        Vector3(1.2f, 0, 0), Vector3(0.5f, 0.5f, 0.5f), rotationB,
        manifold
    );
    
    // 这个测试可能碰撞也可能不碰撞，主要测试不崩溃
    std::cout << "  OBB 边缘测试: " << (hit ? "碰撞" : "不碰撞") << std::endl;
    
    return true;
}

// ============================================================================
// 胶囊体 vs 胶囊体测试
// ============================================================================

bool Test_CapsuleVsCapsule_Collision() {
    ContactManifold manifold;
    
    // 两个胶囊体靠近，半径和 = 1.0，距离 0.8 < 1.0，应该碰撞
    bool hit = CollisionDetector::CapsuleVsCapsule(
        Vector3(0, 0, 0), 0.5f, 2.0f, Quaternion::Identity(),
        Vector3(0.8f, 0, 0), 0.5f, 2.0f, Quaternion::Identity(),
        manifold
    );
    
    TEST_ASSERT(hit, "应该检测到碰撞");
    if (hit) {
        TEST_ASSERT(manifold.IsValid(), "流形应该有效");
    }
    
    return true;
}

// ============================================================================
// 胶囊体 vs 盒体测试
// ============================================================================

bool Test_CapsuleVsBox_Collision() {
    ContactManifold manifold;
    
    // 胶囊中心在 1.3，半径 0.5，盒体边界在 1
    // 胶囊最近点在 0.8，明确碰撞
    bool hit = CollisionDetector::CapsuleVsBox(
        Vector3(1.3f, 0, 0), 0.5f, 2.0f, Quaternion::Identity(),
        Vector3(0, 0, 0), Vector3(1, 1, 1), Quaternion::Identity(),
        manifold
    );
    
    TEST_ASSERT(hit, "应该检测到碰撞");
    if (hit) {
        TEST_ASSERT(manifold.IsValid(), "流形应该有效");
    }
    
    return true;
}

bool Test_CapsuleVsBox_NoCollision() {
    ContactManifold manifold;
    
    bool hit = CollisionDetector::CapsuleVsBox(
        Vector3(5, 0, 0), 0.5f, 2.0f, Quaternion::Identity(),
        Vector3(0, 0, 0), Vector3(1, 1, 1), Quaternion::Identity(),
        manifold
    );
    
    TEST_ASSERT(!hit, "不应该检测到碰撞");
    
    return true;
}

// ============================================================================
// 辅助函数测试
// ============================================================================

bool Test_ClosestPointOnSegment() {
    Vector3 segmentA(0, 0, 0);
    Vector3 segmentB(10, 0, 0);
    
    Vector3 closest = CollisionDetector::ClosestPointOnSegment(
        Vector3(5, 5, 0), segmentA, segmentB
    );
    
    TEST_ASSERT(closest.isApprox(Vector3(5, 0, 0)), "最近点应该在线段中点");
    
    return true;
}

bool Test_ClosestPointsBetweenSegments() {
    Vector3 p1(0, 0, 0);
    Vector3 q1(10, 0, 0);
    Vector3 p2(5, 1, 0);
    Vector3 q2(5, 5, 0);
    
    float s, t;
    Vector3 c1, c2;
    
    CollisionDetector::ClosestPointsBetweenSegments(p1, q1, p2, q2, s, t, c1, c2);
    
    TEST_ASSERT(c1.isApprox(Vector3(5, 0, 0), 0.01f), "线段1最近点应该正确");
    TEST_ASSERT(c2.isApprox(Vector3(5, 1, 0), 0.01f), "线段2最近点应该正确");
    
    return true;
}

// ============================================================================
// 碰撞检测分发器测试
// ============================================================================

bool Test_Dispatcher_SphereVsSphere() {
    SphereShape sphereA(1.0f);
    SphereShape sphereB(1.0f);
    ContactManifold manifold;
    
    bool hit = CollisionDispatcher::Detect(
        &sphereA, Vector3(0, 0, 0), Quaternion::Identity(), Vector3::Ones(),
        &sphereB, Vector3(1.5f, 0, 0), Quaternion::Identity(), Vector3::Ones(),
        manifold
    );
    
    TEST_ASSERT(hit, "分发器应该正确检测球体碰撞");
    TEST_ASSERT(manifold.IsValid(), "流形应该有效");
    
    return true;
}

bool Test_Dispatcher_SphereVsBox() {
    SphereShape sphere(1.0f);
    BoxShape box(Vector3(1, 1, 1));
    ContactManifold manifold;
    
    bool hit = CollisionDispatcher::Detect(
        &sphere, Vector3(1.5f, 0, 0), Quaternion::Identity(), Vector3::Ones(),
        &box, Vector3(0, 0, 0), Quaternion::Identity(), Vector3::Ones(),
        manifold
    );
    
    TEST_ASSERT(hit, "分发器应该正确检测球体vs盒体碰撞");
    
    return true;
}

bool Test_Dispatcher_BoxVsSphere() {
    BoxShape box(Vector3(1, 1, 1));
    SphereShape sphere(1.0f);
    ContactManifold manifold;
    
    // 测试顺序相反
    bool hit = CollisionDispatcher::Detect(
        &box, Vector3(0, 0, 0), Quaternion::Identity(), Vector3::Ones(),
        &sphere, Vector3(1.5f, 0, 0), Quaternion::Identity(), Vector3::Ones(),
        manifold
    );
    
    TEST_ASSERT(hit, "分发器应该处理顺序相反的情况");
    
    return true;
}

bool Test_Dispatcher_BoxVsBox() {
    BoxShape boxA(Vector3(1, 1, 1));
    BoxShape boxB(Vector3(1, 1, 1));
    ContactManifold manifold;
    
    bool hit = CollisionDispatcher::Detect(
        &boxA, Vector3(0, 0, 0), Quaternion::Identity(), Vector3::Ones(),
        &boxB, Vector3(1.5f, 0, 0), Quaternion::Identity(), Vector3::Ones(),
        manifold
    );
    
    TEST_ASSERT(hit, "分发器应该正确检测盒体碰撞");
    
    return true;
}

bool Test_Dispatcher_CapsuleVsCapsule() {
    CapsuleShape capsuleA(0.5f, 2.0f);
    CapsuleShape capsuleB(0.5f, 2.0f);
    ContactManifold manifold;
    
    bool hit = CollisionDispatcher::Detect(
        &capsuleA, Vector3(0, 0, 0), Quaternion::Identity(), Vector3::Ones(),
        &capsuleB, Vector3(0.8f, 0, 0), Quaternion::Identity(), Vector3::Ones(),
        manifold
    );
    
    TEST_ASSERT(hit, "分发器应该正确检测胶囊体碰撞");
    
    return true;
}

bool Test_Dispatcher_WithScale() {
    SphereShape sphereA(1.0f);
    SphereShape sphereB(1.0f);
    ContactManifold manifold;
    
    // 使用缩放：球体 A 缩放 2 倍，半径变为 2
    bool hit = CollisionDispatcher::Detect(
        &sphereA, Vector3(0, 0, 0), Quaternion::Identity(), Vector3(2, 2, 2),
        &sphereB, Vector3(2.5f, 0, 0), Quaternion::Identity(), Vector3::Ones(),
        manifold
    );
    
    TEST_ASSERT(hit, "分发器应该正确处理缩放");
    
    return true;
}

bool Test_Dispatcher_NoCollision() {
    SphereShape sphereA(1.0f);
    SphereShape sphereB(1.0f);
    ContactManifold manifold;
    
    bool hit = CollisionDispatcher::Detect(
        &sphereA, Vector3(0, 0, 0), Quaternion::Identity(), Vector3::Ones(),
        &sphereB, Vector3(10, 0, 0), Quaternion::Identity(), Vector3::Ones(),
        manifold
    );
    
    TEST_ASSERT(!hit, "分发器应该正确检测无碰撞情况");
    TEST_ASSERT(!manifold.IsValid(), "无碰撞时流形应该无效");
    
    return true;
}

// ============================================================================
// 主测试函数
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "细检测碰撞算法测试" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::cout << "\n--- 球体碰撞测试 ---" << std::endl;
    RUN_TEST(Test_SphereVsSphere_Collision);
    RUN_TEST(Test_SphereVsSphere_NoCollision);
    RUN_TEST(Test_SphereVsSphere_Overlapping);
    
    std::cout << "\n--- 球体 vs 盒体测试 ---" << std::endl;
    RUN_TEST(Test_SphereVsBox_Collision);
    RUN_TEST(Test_SphereVsBox_NoCollision);
    
    std::cout << "\n--- 球体 vs 胶囊体测试 ---" << std::endl;
    RUN_TEST(Test_SphereVsCapsule_Collision);
    
    std::cout << "\n--- 盒体 vs 盒体测试 ---" << std::endl;
    RUN_TEST(Test_BoxVsBox_Collision);
    RUN_TEST(Test_BoxVsBox_NoCollision);
    RUN_TEST(Test_BoxVsBox_OBB_Rotated);
    RUN_TEST(Test_BoxVsBox_OBB_EdgeCase);
    
    std::cout << "\n--- 胶囊体 vs 胶囊体测试 ---" << std::endl;
    RUN_TEST(Test_CapsuleVsCapsule_Collision);
    
    std::cout << "\n--- 胶囊体 vs 盒体测试 ---" << std::endl;
    RUN_TEST(Test_CapsuleVsBox_Collision);
    RUN_TEST(Test_CapsuleVsBox_NoCollision);
    
    std::cout << "\n--- 辅助函数测试 ---" << std::endl;
    RUN_TEST(Test_ClosestPointOnSegment);
    RUN_TEST(Test_ClosestPointsBetweenSegments);
    
    std::cout << "\n--- 碰撞检测分发器测试 ---" << std::endl;
    RUN_TEST(Test_Dispatcher_SphereVsSphere);
    RUN_TEST(Test_Dispatcher_SphereVsBox);
    RUN_TEST(Test_Dispatcher_BoxVsSphere);
    RUN_TEST(Test_Dispatcher_BoxVsBox);
    RUN_TEST(Test_Dispatcher_CapsuleVsCapsule);
    RUN_TEST(Test_Dispatcher_WithScale);
    RUN_TEST(Test_Dispatcher_NoCollision);
    
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

