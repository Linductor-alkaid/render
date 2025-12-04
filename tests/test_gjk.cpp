/**
 * @file test_gjk.cpp
 * @brief GJK/EPA 算法测试
 */

#include "render/physics/collision/gjk.h"
#include "render/physics/collision/collision_shapes.h"
#include <iostream>

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
// GJK 测试
// ============================================================================

bool Test_GJK_SphereVsSphere_Intersecting() {
    SphereShape sphereA(1.0f);
    SphereShape sphereB(1.0f);
    
    bool hit = GJK::Intersects(
        &sphereA, Vector3(0, 0, 0), Quaternion::Identity(),
        &sphereB, Vector3(1.5f, 0, 0), Quaternion::Identity()
    );
    
    TEST_ASSERT(hit, "GJK 应该检测到球体相交");
    
    return true;
}

bool Test_GJK_SphereVsSphere_Separated() {
    SphereShape sphereA(1.0f);
    SphereShape sphereB(1.0f);
    
    bool hit = GJK::Intersects(
        &sphereA, Vector3(0, 0, 0), Quaternion::Identity(),
        &sphereB, Vector3(5, 0, 0), Quaternion::Identity()
    );
    
    TEST_ASSERT(!hit, "GJK 应该检测到球体分离");
    
    return true;
}

bool Test_GJK_BoxVsBox_Intersecting() {
    BoxShape boxA(Vector3(1, 1, 1));
    BoxShape boxB(Vector3(1, 1, 1));
    
    bool hit = GJK::Intersects(
        &boxA, Vector3(0, 0, 0), Quaternion::Identity(),
        &boxB, Vector3(1.5f, 0, 0), Quaternion::Identity()
    );
    
    TEST_ASSERT(hit, "GJK 应该检测到盒体相交");
    
    return true;
}

bool Test_GJK_BoxVsSphere() {
    BoxShape box(Vector3(1, 1, 1));
    SphereShape sphere(1.0f);
    
    bool hit = GJK::Intersects(
        &box, Vector3(0, 0, 0), Quaternion::Identity(),
        &sphere, Vector3(1.8f, 0, 0), Quaternion::Identity()
    );
    
    TEST_ASSERT(hit, "GJK 应该检测到盒体和球体相交");
    
    return true;
}

bool Test_GJK_CapsuleVsCapsule() {
    CapsuleShape capsuleA(0.5f, 2.0f);
    CapsuleShape capsuleB(0.5f, 2.0f);
    
    bool hit = GJK::Intersects(
        &capsuleA, Vector3(0, 0, 0), Quaternion::Identity(),
        &capsuleB, Vector3(0.8f, 0, 0), Quaternion::Identity()
    );
    
    TEST_ASSERT(hit, "GJK 应该检测到胶囊体相交");
    
    return true;
}

// ============================================================================
// GJK with Manifold (EPA) 测试
// ============================================================================

bool Test_GJK_WithManifold_SphereVsSphere() {
    SphereShape sphereA(1.0f);
    SphereShape sphereB(1.0f);
    ContactManifold manifold;
    
    bool hit = GJK::IntersectsWithManifold(
        &sphereA, Vector3(0, 0, 0), Quaternion::Identity(),
        &sphereB, Vector3(1.5f, 0, 0), Quaternion::Identity(),
        manifold
    );
    
    TEST_ASSERT(hit, "GJK+EPA 应该检测到碰撞");
    TEST_ASSERT(manifold.IsValid(), "流形应该有效");
    TEST_ASSERT(manifold.contactCount > 0, "应该有接触点");
    
    return true;
}

bool Test_GJK_WithManifold_BoxVsBox() {
    BoxShape boxA(Vector3(1, 1, 1));
    BoxShape boxB(Vector3(1, 1, 1));
    ContactManifold manifold;
    
    bool hit = GJK::IntersectsWithManifold(
        &boxA, Vector3(0, 0, 0), Quaternion::Identity(),
        &boxB, Vector3(1.5f, 0, 0), Quaternion::Identity(),
        manifold
    );
    
    TEST_ASSERT(hit, "GJK+EPA 应该检测到碰撞");
    TEST_ASSERT(manifold.IsValid(), "流形应该有效");
    
    return true;
}

bool Test_EPA_PenetrationDepth() {
    SphereShape sphereA(1.0f);
    SphereShape sphereB(1.0f);
    ContactManifold manifold;
    
    // 两球重叠 0.5 单位
    bool hit = GJK::IntersectsWithManifold(
        &sphereA, Vector3(0, 0, 0), Quaternion::Identity(),
        &sphereB, Vector3(1.5f, 0, 0), Quaternion::Identity(),
        manifold
    );
    
    TEST_ASSERT(hit, "应该检测到碰撞");
    TEST_ASSERT(manifold.penetration > 0.0f, "穿透深度应该大于 0");
    TEST_ASSERT(manifold.penetration < 1.0f, "穿透深度应该小于直径");
    
    return true;
}

bool Test_EPA_Normal_Direction() {
    SphereShape sphereA(1.0f);
    SphereShape sphereB(1.0f);
    ContactManifold manifold;
    
    bool hit = GJK::IntersectsWithManifold(
        &sphereA, Vector3(0, 0, 0), Quaternion::Identity(),
        &sphereB, Vector3(1.5f, 0, 0), Quaternion::Identity(),
        manifold
    );
    
    TEST_ASSERT(hit, "应该检测到碰撞");
    
    // 法线应该大致指向 X 轴方向（从 A 指向 B）
    float normalX = std::abs(manifold.normal.x());
    TEST_ASSERT(normalX > 0.9f, "法线应该主要沿 X 轴");
    
    return true;
}

bool Test_EPA_DeepPenetration() {
    SphereShape sphereA(2.0f);
    SphereShape sphereB(2.0f);
    ContactManifold manifold;
    
    // 两球中心重合，深度穿透
    bool hit = GJK::IntersectsWithManifold(
        &sphereA, Vector3(0, 0, 0), Quaternion::Identity(),
        &sphereB, Vector3(0.5f, 0, 0), Quaternion::Identity(),
        manifold
    );
    
    TEST_ASSERT(hit, "深度穿透应该被检测到");
    TEST_ASSERT(manifold.penetration > 1.0f, "穿透深度应该较大");
    
    return true;
}

bool Test_EPA_CapsuleVsSphere() {
    CapsuleShape capsule(0.5f, 2.0f);
    SphereShape sphere(1.0f);
    ContactManifold manifold;
    
    bool hit = GJK::IntersectsWithManifold(
        &capsule, Vector3(0, 0, 0), Quaternion::Identity(),
        &sphere, Vector3(1.2f, 0, 0), Quaternion::Identity(),
        manifold
    );
    
    TEST_ASSERT(hit, "胶囊体和球体碰撞应该被检测到");
    TEST_ASSERT(manifold.IsValid(), "流形应该有效");
    
    return true;
}

bool Test_EPA_RotatedShapes() {
    BoxShape boxA(Vector3(1, 1, 1));
    BoxShape boxB(Vector3(1, 1, 1));
    ContactManifold manifold;
    
    // 旋转的盒体
    Quaternion rotation = MathUtils::AngleAxis(MathUtils::PI / 4.0f, Vector3::UnitZ());
    
    bool hit = GJK::IntersectsWithManifold(
        &boxA, Vector3(0, 0, 0), Quaternion::Identity(),
        &boxB, Vector3(1.5f, 0, 0), rotation,
        manifold
    );
    
    TEST_ASSERT(hit, "旋转的盒体碰撞应该被检测到");
    TEST_ASSERT(manifold.IsValid(), "流形应该有效");
    
    return true;
}

// ============================================================================
// 主测试函数
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "GJK/EPA 算法测试" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::cout << "\n--- GJK 基础测试 ---" << std::endl;
    RUN_TEST(Test_GJK_SphereVsSphere_Intersecting);
    RUN_TEST(Test_GJK_SphereVsSphere_Separated);
    RUN_TEST(Test_GJK_BoxVsBox_Intersecting);
    RUN_TEST(Test_GJK_BoxVsSphere);
    RUN_TEST(Test_GJK_CapsuleVsCapsule);
    
    std::cout << "\n--- GJK + EPA 基础测试 ---" << std::endl;
    RUN_TEST(Test_GJK_WithManifold_SphereVsSphere);
    RUN_TEST(Test_GJK_WithManifold_BoxVsBox);
    
    std::cout << "\n--- EPA 详细测试 ---" << std::endl;
    RUN_TEST(Test_EPA_PenetrationDepth);
    RUN_TEST(Test_EPA_Normal_Direction);
    RUN_TEST(Test_EPA_DeepPenetration);
    RUN_TEST(Test_EPA_CapsuleVsSphere);
    RUN_TEST(Test_EPA_RotatedShapes);
    
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

