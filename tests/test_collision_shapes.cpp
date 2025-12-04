/**
 * @file test_collision_shapes.cpp
 * @brief 碰撞形状测试
 */

#include "render/physics/collision/collision_shapes.h"
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
// SphereShape 测试
// ============================================================================

bool Test_SphereShape_Creation() {
    SphereShape sphere(2.0f);
    
    TEST_ASSERT(sphere.GetType() == ShapeType::Sphere, "类型应该是 Sphere");
    TEST_ASSERT(sphere.GetRadius() == 2.0f, "半径应该正确");
    
    return true;
}

bool Test_SphereShape_Volume() {
    SphereShape sphere(1.0f);
    float volume = sphere.ComputeVolume();
    
    // V = 4/3 * π * r³ = 4.189
    TEST_ASSERT(std::abs(volume - 4.189f) < 0.01f, "球体体积计算应该正确");
    
    return true;
}

bool Test_SphereShape_AABB() {
    SphereShape sphere(1.0f);
    Vector3 pos(5, 5, 5);
    AABB aabb = sphere.ComputeAABB(pos, Quaternion::Identity(), Vector3::Ones());
    
    TEST_ASSERT(aabb.min.isApprox(Vector3(4, 4, 4)), "AABB min 应该正确");
    TEST_ASSERT(aabb.max.isApprox(Vector3(6, 6, 6)), "AABB max 应该正确");
    
    return true;
}

bool Test_SphereShape_InertiaTensor() {
    SphereShape sphere(1.0f);
    Matrix3 tensor = sphere.ComputeInertiaTensor(10.0f);
    
    // I = 2/5 * m * r² = 4.0
    TEST_ASSERT(std::abs(tensor(0, 0) - 4.0f) < 0.001f, "惯性张量 XX 应该正确");
    TEST_ASSERT(std::abs(tensor(1, 1) - 4.0f) < 0.001f, "惯性张量 YY 应该正确");
    TEST_ASSERT(tensor(0, 1) == 0.0f, "非对角线应该是 0");
    
    return true;
}

bool Test_SphereShape_SupportPoint() {
    SphereShape sphere(2.0f);
    Vector3 support = sphere.GetSupportPoint(Vector3(1, 0, 0));
    
    TEST_ASSERT(support.isApprox(Vector3(2, 0, 0)), "支撑点应该在 +X 方向");
    
    return true;
}

// ============================================================================
// BoxShape 测试
// ============================================================================

bool Test_BoxShape_Creation() {
    BoxShape box(Vector3(1, 2, 3));
    
    TEST_ASSERT(box.GetType() == ShapeType::Box, "类型应该是 Box");
    TEST_ASSERT(box.GetHalfExtents().isApprox(Vector3(1, 2, 3)), "半尺寸应该正确");
    
    return true;
}

bool Test_BoxShape_Volume() {
    BoxShape box(Vector3(1, 1, 1));
    float volume = box.ComputeVolume();
    
    // V = 2*2*2 = 8
    TEST_ASSERT(std::abs(volume - 8.0f) < 0.001f, "盒体体积计算应该正确");
    
    return true;
}

bool Test_BoxShape_AABB() {
    BoxShape box(Vector3(1, 2, 3));
    Vector3 pos(0, 0, 0);
    AABB aabb = box.ComputeAABB(pos, Quaternion::Identity(), Vector3::Ones());
    
    TEST_ASSERT(aabb.min.isApprox(Vector3(-1, -2, -3)), "AABB min 应该正确");
    TEST_ASSERT(aabb.max.isApprox(Vector3(1, 2, 3)), "AABB max 应该正确");
    
    return true;
}

bool Test_BoxShape_GetVertices() {
    BoxShape box(Vector3(1, 1, 1));
    Vector3 vertices[8];
    box.GetVertices(vertices);
    
    // 验证所有顶点
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT(std::abs(vertices[i].norm() - std::sqrt(3.0f)) < 0.01f, 
                    "所有顶点到中心距离应该相同");
    }
    
    return true;
}

bool Test_BoxShape_SupportPoint() {
    BoxShape box(Vector3(1, 2, 3));
    Vector3 support = box.GetSupportPoint(Vector3(1, 1, 1));
    
    TEST_ASSERT(support.isApprox(Vector3(1, 2, 3)), "支撑点应该是角点");
    
    return true;
}

// ============================================================================
// CapsuleShape 测试
// ============================================================================

bool Test_CapsuleShape_Creation() {
    CapsuleShape capsule(1.0f, 3.0f);
    
    TEST_ASSERT(capsule.GetType() == ShapeType::Capsule, "类型应该是 Capsule");
    TEST_ASSERT(capsule.GetRadius() == 1.0f, "半径应该正确");
    TEST_ASSERT(capsule.GetHeight() == 3.0f, "高度应该正确");
    
    return true;
}

bool Test_CapsuleShape_LineSegment() {
    CapsuleShape capsule(1.0f, 4.0f);
    Vector3 pointA, pointB;
    capsule.GetLineSegment(pointA, pointB);
    
    TEST_ASSERT(pointA.isApprox(Vector3(0, -2, 0)), "端点 A 应该正确");
    TEST_ASSERT(pointB.isApprox(Vector3(0, 2, 0)), "端点 B 应该正确");
    
    return true;
}

bool Test_CapsuleShape_Volume() {
    CapsuleShape capsule(1.0f, 2.0f);
    float volume = capsule.ComputeVolume();
    
    // V = π*r²*h + 4/3*π*r³ = π*1*2 + 4/3*π = 10.47
    TEST_ASSERT(std::abs(volume - 10.47f) < 0.1f, "胶囊体体积计算应该正确");
    
    return true;
}

bool Test_CapsuleShape_AABB() {
    CapsuleShape capsule(1.0f, 2.0f);
    Vector3 pos(0, 0, 0);
    AABB aabb = capsule.ComputeAABB(pos, Quaternion::Identity(), Vector3::Ones());
    
    // 半高 = 1, 半径 = 1, 总高 = 2
    TEST_ASSERT(aabb.min.y() == -2.0f, "AABB min Y 应该正确");
    TEST_ASSERT(aabb.max.y() == 2.0f, "AABB max Y 应该正确");
    TEST_ASSERT(aabb.min.x() == -1.0f, "AABB min X 应该正确");
    
    return true;
}

// ============================================================================
// ShapeFactory 测试
// ============================================================================

bool Test_ShapeFactory_CreateShapes() {
    auto sphere = ShapeFactory::CreateSphere(2.0f);
    TEST_ASSERT(sphere != nullptr, "应该创建球体");
    TEST_ASSERT(sphere->GetType() == ShapeType::Sphere, "类型应该正确");
    
    auto box = ShapeFactory::CreateBox(Vector3(1, 2, 3));
    TEST_ASSERT(box != nullptr, "应该创建盒体");
    TEST_ASSERT(box->GetType() == ShapeType::Box, "类型应该正确");
    
    auto capsule = ShapeFactory::CreateCapsule(1.0f, 3.0f);
    TEST_ASSERT(capsule != nullptr, "应该创建胶囊体");
    TEST_ASSERT(capsule->GetType() == ShapeType::Capsule, "类型应该正确");
    
    return true;
}

// ============================================================================
// 主测试函数
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "碰撞形状测试" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::cout << "\n--- SphereShape 测试 ---" << std::endl;
    RUN_TEST(Test_SphereShape_Creation);
    RUN_TEST(Test_SphereShape_Volume);
    RUN_TEST(Test_SphereShape_AABB);
    RUN_TEST(Test_SphereShape_InertiaTensor);
    RUN_TEST(Test_SphereShape_SupportPoint);
    
    std::cout << "\n--- BoxShape 测试 ---" << std::endl;
    RUN_TEST(Test_BoxShape_Creation);
    RUN_TEST(Test_BoxShape_Volume);
    RUN_TEST(Test_BoxShape_AABB);
    RUN_TEST(Test_BoxShape_GetVertices);
    RUN_TEST(Test_BoxShape_SupportPoint);
    
    std::cout << "\n--- CapsuleShape 测试 ---" << std::endl;
    RUN_TEST(Test_CapsuleShape_Creation);
    RUN_TEST(Test_CapsuleShape_LineSegment);
    RUN_TEST(Test_CapsuleShape_Volume);
    RUN_TEST(Test_CapsuleShape_AABB);
    
    std::cout << "\n--- ShapeFactory 测试 ---" << std::endl;
    RUN_TEST(Test_ShapeFactory_CreateShapes);
    
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

