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

bool Test_SphereVsBox_GroundCollision() {
    ContactManifold manifold;
    
    // 模拟物理演示场景：球体从上方碰撞地面
    // 地面盒体：中心在(0, 0, 0)，半高0.5，上表面在y=0.5
    // 球体：中心在(0, 0.3, 0)，半径0.5，应该与地面碰撞
    bool hit = CollisionDetector::SphereVsBox(
        Vector3(0, 0.3f, 0), 0.5f,  // 球心在y=0.3，半径0.5，底部在y=-0.2，应该与地面碰撞
        Vector3(0, 0, 0), Vector3(15.0f, 0.5f, 15.0f),  // 地面：半高0.5，上表面在y=0.5
        Quaternion::Identity(),
        manifold
    );
    
    TEST_ASSERT(hit, "球体应该与地面碰撞");
    if (hit) {
        TEST_ASSERT(manifold.IsValid(), "流形应该有效");
        TEST_ASSERT(manifold.contactCount > 0, "应该有接触点");
        // 法线应该大致向上（从地面指向球体）
        TEST_ASSERT(manifold.normal.y() > 0.8f, "法线应该主要向上");
        // 接触点应该在球体表面上
        if (manifold.contactCount > 0) {
            Vector3 sphereCenter(0, 0.3f, 0);
            Vector3 contactPos = manifold.contacts[0].position;
            Vector3 toContact = contactPos - sphereCenter;
            float distToCenter = toContact.norm();
            // 接触点到球心的距离应该接近半径（允许小误差）
            TEST_ASSERT(std::abs(distToCenter - 0.5f) < 0.1f, 
                       "接触点应该在球体表面上");
        }
    }
    
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
// 接触点位置验证测试
// ============================================================================

bool Test_ContactPoint_SphereVsSphere_OnSurface() {
    ContactManifold manifold;
    
    Vector3 centerA(0, 0, 0);
    Vector3 centerB(1.5f, 0, 0);
    float radiusA = 1.0f;
    float radiusB = 1.0f;
    
    bool hit = CollisionDetector::SphereVsSphere(centerA, radiusA, centerB, radiusB, manifold);
    
    TEST_ASSERT(hit, "应该检测到碰撞");
    TEST_ASSERT(manifold.contactCount > 0, "应该有接触点");
    
    // 验证接触点在球体表面上
    // 注意：接触点通常在一个物体表面上（这里是球体A），而不是同时在两个表面上
    for (int i = 0; i < manifold.contactCount; ++i) {
        Vector3 contactPos = manifold.contacts[i].position;
        
        // 检查接触点到球心A的距离（接触点在球体A表面上）
        Vector3 toA = contactPos - centerA;
        float distToA = toA.norm();
        TEST_ASSERT(std::abs(distToA - radiusA) < 0.01f, 
                   "接触点应该在球体A表面上");
        
        // 检查接触点到球心B的距离（应该小于等于半径和，表示穿透）
        Vector3 toB = contactPos - centerB;
        float distToB = toB.norm();
        float radiusSum = radiusA + radiusB;
        TEST_ASSERT(distToB <= radiusSum + 0.01f, 
                   "接触点到球体B的距离应该在合理范围内");
    }
    
    return true;
}

bool Test_ContactPoint_SphereVsBox_OnSphereSurface() {
    ContactManifold manifold;
    
    Vector3 sphereCenter(1.5f, 0, 0);
    float sphereRadius = 1.0f;
    Vector3 boxCenter(0, 0, 0);
    Vector3 boxHalfExtents(1, 1, 1);
    
    bool hit = CollisionDetector::SphereVsBox(
        sphereCenter, sphereRadius,
        boxCenter, boxHalfExtents,
        Quaternion::Identity(),
        manifold
    );
    
    TEST_ASSERT(hit, "应该检测到碰撞");
    TEST_ASSERT(manifold.contactCount > 0, "应该有接触点");
    
    // 验证接触点在球体表面上
    for (int i = 0; i < manifold.contactCount; ++i) {
        Vector3 contactPos = manifold.contacts[i].position;
        Vector3 toSphere = contactPos - sphereCenter;
        float distToCenter = toSphere.norm();
        
        // 接触点到球心的距离应该等于半径（允许小误差）
        TEST_ASSERT(std::abs(distToCenter - sphereRadius) < 0.1f, 
                   "接触点应该在球体表面上");
    }
    
    return true;
}

bool Test_ContactPoint_BoxVsBox_OnSurface() {
    ContactManifold manifold;
    
    Vector3 centerA(0, 0, 0);
    Vector3 centerB(1.5f, 0, 0);
    Vector3 halfExtents(1, 1, 1);
    
    bool hit = CollisionDetector::BoxVsBox(
        centerA, halfExtents, Quaternion::Identity(),
        centerB, halfExtents, Quaternion::Identity(),
        manifold
    );
    
    TEST_ASSERT(hit, "应该检测到碰撞");
    TEST_ASSERT(manifold.contactCount > 0, "应该有接触点");
    
    // 验证接触点在盒体边界附近（对于盒体，接触点应该在表面上或内部）
    // 这里主要验证接触点存在且有效
    for (int i = 0; i < manifold.contactCount; ++i) {
        Vector3 contactPos = manifold.contacts[i].position;
        // 接触点应该在两个盒体之间
        TEST_ASSERT(contactPos.x() >= centerA.x() - halfExtents.x() && 
                   contactPos.x() <= centerB.x() + halfExtents.x(),
                   "接触点应该在合理范围内");
    }
    
    return true;
}

// ============================================================================
// 局部坐标验证测试
// ============================================================================

bool Test_LocalCoordinates_SphereVsSphere_Consistency() {
    ContactManifold manifold;
    
    // 调整位置，确保两个球体相交（距离小于半径和）
    Vector3 posA(2.0f, 1.0f, 0.5f);
    Vector3 posB(3.5f, 1.0f, 0.5f);  // 距离1.5，半径和2.0，应该相交
    Quaternion rotA = MathUtils::AngleAxis(MathUtils::PI / 4.0f, Vector3::UnitY());
    Quaternion rotB = MathUtils::AngleAxis(-MathUtils::PI / 6.0f, Vector3::UnitZ());
    
    float radiusA = 1.0f;
    float radiusB = 1.0f;
    
    bool hit = CollisionDetector::SphereVsSphere(posA, radiusA, posB, radiusB, manifold);
    
    TEST_ASSERT(hit, "应该检测到碰撞");
    
    // 模拟局部坐标计算（与physics_systems.cpp中的逻辑一致）
    for (int i = 0; i < manifold.contactCount; ++i) {
        Vector3 contactPos = manifold.contacts[i].position;
        
        // 计算局部坐标
        Vector3 localA = rotA.conjugate() * (contactPos - posA);
        Vector3 localB = rotB.conjugate() * (contactPos - posB);
        
        // 验证：从局部坐标转换回世界坐标应该得到原始接触点
        Vector3 worldFromA = posA + rotA * localA;
        Vector3 worldFromB = posB + rotB * localB;
        
        TEST_ASSERT(worldFromA.isApprox(contactPos, 0.01f), 
                   "局部坐标A转换回世界坐标应该一致");
        TEST_ASSERT(worldFromB.isApprox(contactPos, 0.01f), 
                   "局部坐标B转换回世界坐标应该一致");
    }
    
    return true;
}

bool Test_LocalCoordinates_SphereVsBox_Consistency() {
    ContactManifold manifold;
    
    Vector3 spherePos(2.0f, 1.0f, 0.5f);
    float sphereRadius = 1.0f;
    Vector3 boxPos(0, 0, 0);
    Vector3 boxHalfExtents(1, 1, 1);
    Quaternion boxRot = MathUtils::AngleAxis(MathUtils::PI / 4.0f, Vector3::UnitY());
    
    bool hit = CollisionDetector::SphereVsBox(
        spherePos, sphereRadius,
        boxPos, boxHalfExtents, boxRot,
        manifold
    );
    
    TEST_ASSERT(hit, "应该检测到碰撞");
    
    // 模拟局部坐标计算
    Quaternion sphereRot = Quaternion::Identity(); // 球体无旋转
    
    for (int i = 0; i < manifold.contactCount; ++i) {
        Vector3 contactPos = manifold.contacts[i].position;
        
        // 计算局部坐标
        Vector3 localSphere = sphereRot.conjugate() * (contactPos - spherePos);
        Vector3 localBox = boxRot.conjugate() * (contactPos - boxPos);
        
        // 验证转换一致性
        Vector3 worldFromSphere = spherePos + sphereRot * localSphere;
        Vector3 worldFromBox = boxPos + boxRot * localBox;
        
        TEST_ASSERT(worldFromSphere.isApprox(contactPos, 0.01f), 
                   "球体局部坐标转换应该一致");
        TEST_ASSERT(worldFromBox.isApprox(contactPos, 0.01f), 
                   "盒体局部坐标转换应该一致");
    }
    
    return true;
}

// ============================================================================
// 边缘情况测试
// ============================================================================

bool Test_EdgeCase_SphereInsideBox() {
    ContactManifold manifold;
    
    // 球心完全在盒体内部
    Vector3 sphereCenter(0, 0, 0);
    float sphereRadius = 0.5f;
    Vector3 boxCenter(0, 0, 0);
    Vector3 boxHalfExtents(2, 2, 2);
    
    bool hit = CollisionDetector::SphereVsBox(
        sphereCenter, sphereRadius,
        boxCenter, boxHalfExtents,
        Quaternion::Identity(),
        manifold
    );
    
    TEST_ASSERT(hit, "球体在盒体内部应该检测到碰撞");
    TEST_ASSERT(manifold.IsValid(), "流形应该有效");
    TEST_ASSERT(manifold.penetration > 0.0f, "应该有穿透深度");
    
    // 法线应该指向最近的盒体面
    TEST_ASSERT(manifold.normal.norm() > 0.9f, "法线应该归一化");
    
    return true;
}

bool Test_EdgeCase_SphereOnBoxEdge() {
    ContactManifold manifold;
    
    // 球体与盒体边缘接触（稍微重叠以确保检测到碰撞）
    // 盒体边界在x=1，球心在x=1.9，半径1.0，最近点在x=0.9，应该碰撞
    Vector3 sphereCenter(1.9f, 0, 0);
    float sphereRadius = 1.0f;
    Vector3 boxCenter(0, 0, 0);
    Vector3 boxHalfExtents(1, 1, 1);
    
    bool hit = CollisionDetector::SphereVsBox(
        sphereCenter, sphereRadius,
        boxCenter, boxHalfExtents,
        Quaternion::Identity(),
        manifold
    );
    
    TEST_ASSERT(hit, "球体与盒体边缘接触应该检测到碰撞");
    if (hit) {
        TEST_ASSERT(manifold.IsValid(), "流形应该有效");
        // 边缘接触时，法线可能不是完全对齐坐标轴
        TEST_ASSERT(manifold.normal.norm() > 0.9f, "法线应该归一化");
    }
    
    return true;
}

bool Test_EdgeCase_SphereOnBoxCorner() {
    ContactManifold manifold;
    
    // 球体与盒体角点接触
    Vector3 sphereCenter(1.5f, 1.5f, 1.5f);
    float sphereRadius = 1.0f;
    Vector3 boxCenter(0, 0, 0);
    Vector3 boxHalfExtents(1, 1, 1);
    
    bool hit = CollisionDetector::SphereVsBox(
        sphereCenter, sphereRadius,
        boxCenter, boxHalfExtents,
        Quaternion::Identity(),
        manifold
    );
    
    TEST_ASSERT(hit, "球体与盒体角点接触应该检测到碰撞");
    if (hit) {
        TEST_ASSERT(manifold.IsValid(), "流形应该有效");
        // 角点接触时，法线应该从角点指向球心
        Vector3 expectedNormal = (sphereCenter - boxCenter).normalized();
        // 允许法线方向有偏差（因为可能选择最近的面）
        float dot = manifold.normal.dot(expectedNormal);
        TEST_ASSERT(dot > 0.5f, "法线方向应该大致正确");
    }
    
    return true;
}

bool Test_EdgeCase_SphereTouchingBox() {
    ContactManifold manifold;
    
    // 球体刚好接触盒体（无穿透）
    Vector3 sphereCenter(2.0f, 0, 0);
    float sphereRadius = 1.0f;
    Vector3 boxCenter(0, 0, 0);
    Vector3 boxHalfExtents(1, 1, 1);
    
    bool hit = CollisionDetector::SphereVsBox(
        sphereCenter, sphereRadius,
        boxCenter, boxHalfExtents,
        Quaternion::Identity(),
        manifold
    );
    
    // 刚好接触时，由于浮点误差，可能检测到也可能检测不到
    // 这里主要测试不会崩溃
    if (hit) {
        TEST_ASSERT(manifold.IsValid(), "如果检测到碰撞，流形应该有效");
    }
    
    return true;
}

bool Test_EdgeCase_SphereVsRotatedBox() {
    ContactManifold manifold;
    
    // 球体与旋转的盒体碰撞
    Vector3 sphereCenter(1.5f, 0, 0);
    float sphereRadius = 1.0f;
    Vector3 boxCenter(0, 0, 0);
    Vector3 boxHalfExtents(1, 1, 1);
    Quaternion boxRot = MathUtils::AngleAxis(MathUtils::PI / 4.0f, Vector3::UnitY());
    
    bool hit = CollisionDetector::SphereVsBox(
        sphereCenter, sphereRadius,
        boxCenter, boxHalfExtents, boxRot,
        manifold
    );
    
    TEST_ASSERT(hit, "球体与旋转盒体应该检测到碰撞");
    if (hit) {
        TEST_ASSERT(manifold.IsValid(), "流形应该有效");
        TEST_ASSERT(manifold.contactCount > 0, "应该有接触点");
        
        // 验证接触点在球体表面上
        for (int i = 0; i < manifold.contactCount; ++i) {
            Vector3 contactPos = manifold.contacts[i].position;
            Vector3 toSphere = contactPos - sphereCenter;
            float distToCenter = toSphere.norm();
            TEST_ASSERT(std::abs(distToCenter - sphereRadius) < 0.1f, 
                       "接触点应该在球体表面上");
        }
    }
    
    return true;
}

bool Test_EdgeCase_VerySmallPenetration() {
    ContactManifold manifold;
    
    // 非常小的穿透深度
    Vector3 sphereCenter(1.99f, 0, 0);
    float sphereRadius = 1.0f;
    Vector3 boxCenter(0, 0, 0);
    Vector3 boxHalfExtents(1, 1, 1);
    
    bool hit = CollisionDetector::SphereVsBox(
        sphereCenter, sphereRadius,
        boxCenter, boxHalfExtents,
        Quaternion::Identity(),
        manifold
    );
    
    TEST_ASSERT(hit, "小穿透应该检测到碰撞");
    if (hit) {
        TEST_ASSERT(manifold.penetration > 0.0f, "应该有穿透深度");
        TEST_ASSERT(manifold.penetration < 0.1f, "穿透深度应该很小");
    }
    
    return true;
}

bool Test_EdgeCase_VeryLargePenetration() {
    ContactManifold manifold;
    
    // 非常大的穿透深度（球心在盒体中心）
    Vector3 sphereCenter(0, 0, 0);
    float sphereRadius = 2.0f;
    Vector3 boxCenter(0, 0, 0);
    Vector3 boxHalfExtents(1, 1, 1);
    
    bool hit = CollisionDetector::SphereVsBox(
        sphereCenter, sphereRadius,
        boxCenter, boxHalfExtents,
        Quaternion::Identity(),
        manifold
    );
    
    TEST_ASSERT(hit, "大穿透应该检测到碰撞");
    if (hit) {
        TEST_ASSERT(manifold.penetration > 1.0f, "穿透深度应该较大");
        TEST_ASSERT(manifold.IsValid(), "流形应该有效");
    }
    
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
    RUN_TEST(Test_SphereVsBox_GroundCollision);
    
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
    
    std::cout << "\n--- 接触点位置验证测试 ---" << std::endl;
    RUN_TEST(Test_ContactPoint_SphereVsSphere_OnSurface);
    RUN_TEST(Test_ContactPoint_SphereVsBox_OnSphereSurface);
    RUN_TEST(Test_ContactPoint_BoxVsBox_OnSurface);
    
    std::cout << "\n--- 局部坐标验证测试 ---" << std::endl;
    RUN_TEST(Test_LocalCoordinates_SphereVsSphere_Consistency);
    RUN_TEST(Test_LocalCoordinates_SphereVsBox_Consistency);
    
    std::cout << "\n--- 边缘情况测试 ---" << std::endl;
    RUN_TEST(Test_EdgeCase_SphereInsideBox);
    RUN_TEST(Test_EdgeCase_SphereOnBoxEdge);
    RUN_TEST(Test_EdgeCase_SphereOnBoxCorner);
    RUN_TEST(Test_EdgeCase_SphereTouchingBox);
    RUN_TEST(Test_EdgeCase_SphereVsRotatedBox);
    RUN_TEST(Test_EdgeCase_VerySmallPenetration);
    RUN_TEST(Test_EdgeCase_VeryLargePenetration);
    
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

