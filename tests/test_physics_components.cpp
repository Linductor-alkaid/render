/**
 * @file test_physics_components.cpp
 * @brief 物理引擎组件测试
 * 
 * 测试 RigidBodyComponent, ColliderComponent, PhysicsMaterial
 */

#include "render/physics/physics_components.h"
#include "render/physics/physics_utils.h"
#include <iostream>
#include <cmath>

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
        if (test_func()) { \
            std::cout << "✓ " << #test_func << " 通过" << std::endl; \
        } else { \
            std::cout << "✗ " << #test_func << " 失败" << std::endl; \
        } \
    } while(0)

// ============================================================================
// PhysicsMaterial 测试
// ============================================================================

bool Test_PhysicsMaterial_Default() {
    PhysicsMaterial mat = PhysicsMaterial::Default();
    
    TEST_ASSERT(mat.friction == 0.5f, "默认摩擦系数应该是 0.5");
    TEST_ASSERT(mat.restitution == 0.3f, "默认弹性系数应该是 0.3");
    TEST_ASSERT(mat.density == 1.0f, "默认密度应该是 1.0");
    
    return true;
}

bool Test_PhysicsMaterial_CombineValues() {
    float a = 0.4f;
    float b = 0.8f;
    
    float avg = PhysicsMaterial::CombineValues(a, b, PhysicsMaterial::CombineMode::Average);
    TEST_ASSERT(std::abs(avg - 0.6f) < 0.001f, "平均值应该是 0.6");
    
    float min = PhysicsMaterial::CombineValues(a, b, PhysicsMaterial::CombineMode::Minimum);
    TEST_ASSERT(min == 0.4f, "最小值应该是 0.4");
    
    float max = PhysicsMaterial::CombineValues(a, b, PhysicsMaterial::CombineMode::Maximum);
    TEST_ASSERT(max == 0.8f, "最大值应该是 0.8");
    
    float mul = PhysicsMaterial::CombineValues(a, b, PhysicsMaterial::CombineMode::Multiply);
    TEST_ASSERT(std::abs(mul - 0.32f) < 0.001f, "乘积应该是 0.32");
    
    return true;
}

bool Test_PhysicsMaterial_Presets() {
    PhysicsMaterial rubber = PhysicsMaterial::Rubber();
    TEST_ASSERT(rubber.friction > 0.5f, "橡胶摩擦系数应该较大");
    TEST_ASSERT(rubber.restitution > 0.5f, "橡胶弹性系数应该较大");
    
    PhysicsMaterial ice = PhysicsMaterial::Ice();
    TEST_ASSERT(ice.friction < 0.1f, "冰摩擦系数应该很小");
    
    PhysicsMaterial metal = PhysicsMaterial::Metal();
    TEST_ASSERT(metal.density > 5.0f, "金属密度应该较大");
    
    return true;
}

// ============================================================================
// RigidBodyComponent 测试
// ============================================================================

bool Test_RigidBodyComponent_DefaultValues() {
    RigidBodyComponent rb;
    
    TEST_ASSERT(rb.type == RigidBodyComponent::BodyType::Dynamic, "默认类型应该是 Dynamic");
    TEST_ASSERT(rb.mass == 1.0f, "默认质量应该是 1.0");
    TEST_ASSERT(rb.inverseMass == 1.0f, "默认逆质量应该是 1.0");
    TEST_ASSERT(rb.useGravity, "默认应该受重力影响");
    TEST_ASSERT(!rb.isSleeping, "默认应该不休眠");
    
    return true;
}

bool Test_RigidBodyComponent_SetMass() {
    RigidBodyComponent rb;
    rb.SetMass(5.0f);
    
    TEST_ASSERT(rb.mass == 5.0f, "质量应该设置正确");
    TEST_ASSERT(std::abs(rb.inverseMass - 0.2f) < 0.001f, "逆质量应该自动计算");
    
    return true;
}

bool Test_RigidBodyComponent_SetMass_Static() {
    RigidBodyComponent rb;
    rb.type = RigidBodyComponent::BodyType::Static;
    rb.SetMass(100.0f);
    
    TEST_ASSERT(rb.inverseMass == 0.0f, "静态物体逆质量应该是 0");
    
    return true;
}

bool Test_RigidBodyComponent_WakeUp() {
    RigidBodyComponent rb;
    rb.isSleeping = true;
    rb.sleepTimer = 1.0f;
    
    rb.WakeUp();
    
    TEST_ASSERT(!rb.isSleeping, "唤醒后应该不休眠");
    TEST_ASSERT(rb.sleepTimer == 0.0f, "唤醒后计时器应该重置");
    
    return true;
}

bool Test_RigidBodyComponent_TypeChecks() {
    RigidBodyComponent rb;
    
    rb.type = RigidBodyComponent::BodyType::Static;
    TEST_ASSERT(rb.IsStatic(), "应该识别为静态");
    TEST_ASSERT(!rb.IsDynamic(), "不应该识别为动态");
    
    rb.type = RigidBodyComponent::BodyType::Kinematic;
    TEST_ASSERT(rb.IsKinematic(), "应该识别为运动学");
    
    rb.type = RigidBodyComponent::BodyType::Dynamic;
    TEST_ASSERT(rb.IsDynamic(), "应该识别为动态");
    
    return true;
}

// ============================================================================
// ColliderComponent 测试
// ============================================================================

bool Test_ColliderComponent_DefaultValues() {
    ColliderComponent collider;
    
    TEST_ASSERT(collider.shapeType == ColliderComponent::ShapeType::Box, "默认形状应该是盒体");
    TEST_ASSERT(!collider.isTrigger, "默认不应该是触发器");
    TEST_ASSERT(collider.material != nullptr, "应该有默认材质");
    TEST_ASSERT(collider.aabbDirty, "AABB 应该标记为脏");
    
    return true;
}

bool Test_ColliderComponent_CreateSphere() {
    ColliderComponent collider = ColliderComponent::CreateSphere(2.0f);
    
    TEST_ASSERT(collider.shapeType == ColliderComponent::ShapeType::Sphere, "形状应该是球体");
    TEST_ASSERT(collider.shapeData.sphere.radius == 2.0f, "半径应该正确");
    
    return true;
}

bool Test_ColliderComponent_CreateBox() {
    ColliderComponent collider = ColliderComponent::CreateBox(Vector3(1, 2, 3));
    
    TEST_ASSERT(collider.shapeType == ColliderComponent::ShapeType::Box, "形状应该是盒体");
    
    Vector3 halfExtents = collider.GetBoxHalfExtents();
    TEST_ASSERT(halfExtents.isApprox(Vector3(1, 2, 3)), "半尺寸应该正确");
    
    return true;
}

bool Test_ColliderComponent_CreateCapsule() {
    ColliderComponent collider = ColliderComponent::CreateCapsule(1.0f, 5.0f);
    
    TEST_ASSERT(collider.shapeType == ColliderComponent::ShapeType::Capsule, "形状应该是胶囊体");
    TEST_ASSERT(collider.shapeData.capsule.radius == 1.0f, "半径应该正确");
    TEST_ASSERT(collider.shapeData.capsule.height == 5.0f, "高度应该正确");
    
    return true;
}

// ============================================================================
// PhysicsUtils 测试
// ============================================================================

bool Test_PhysicsUtils_ComputeSphereMass() {
    float mass = PhysicsUtils::ComputeSphereMass(1.0f, 1.0f);
    // V = 4/3 * π * r³ = 4.189
    TEST_ASSERT(std::abs(mass - 4.189f) < 0.01f, "球体质量计算应该正确");
    
    return true;
}

bool Test_PhysicsUtils_ComputeBoxMass() {
    float mass = PhysicsUtils::ComputeBoxMass(1.0f, Vector3(1, 1, 1));
    // V = 2*2*2 = 8
    TEST_ASSERT(std::abs(mass - 8.0f) < 0.001f, "盒体质量计算应该正确");
    
    return true;
}

bool Test_PhysicsUtils_ComputeSphereInertiaTensor() {
    Matrix3 tensor = PhysicsUtils::ComputeSphereInertiaTensor(10.0f, 1.0f);
    // I = 2/5 * m * r² = 2/5 * 10 * 1 = 4
    
    TEST_ASSERT(std::abs(tensor(0, 0) - 4.0f) < 0.001f, "球体惯性张量 XX 应该正确");
    TEST_ASSERT(std::abs(tensor(1, 1) - 4.0f) < 0.001f, "球体惯性张量 YY 应该正确");
    TEST_ASSERT(std::abs(tensor(2, 2) - 4.0f) < 0.001f, "球体惯性张量 ZZ 应该正确");
    TEST_ASSERT(tensor(0, 1) == 0.0f, "非对角线元素应该是 0");
    
    return true;
}

bool Test_PhysicsUtils_InitializeRigidBody() {
    ColliderComponent collider = ColliderComponent::CreateSphere(1.0f);
    collider.material->density = 2.0f;
    
    RigidBodyComponent rigidBody;
    PhysicsUtils::InitializeRigidBody(rigidBody, collider);
    
    TEST_ASSERT(rigidBody.mass > 0.0f, "质量应该已计算");
    TEST_ASSERT(rigidBody.inverseMass > 0.0f, "逆质量应该已计算");
    TEST_ASSERT(rigidBody.inertiaTensor(0, 0) > 0.0f, "惯性张量应该已计算");
    
    return true;
}

// ============================================================================
// 主测试函数
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "物理引擎组件测试" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // PhysicsMaterial 测试
    std::cout << "\n--- PhysicsMaterial 测试 ---" << std::endl;
    RUN_TEST(Test_PhysicsMaterial_Default);
    RUN_TEST(Test_PhysicsMaterial_CombineValues);
    RUN_TEST(Test_PhysicsMaterial_Presets);
    
    // RigidBodyComponent 测试
    std::cout << "\n--- RigidBodyComponent 测试 ---" << std::endl;
    RUN_TEST(Test_RigidBodyComponent_DefaultValues);
    RUN_TEST(Test_RigidBodyComponent_SetMass);
    RUN_TEST(Test_RigidBodyComponent_SetMass_Static);
    RUN_TEST(Test_RigidBodyComponent_WakeUp);
    RUN_TEST(Test_RigidBodyComponent_TypeChecks);
    
    // ColliderComponent 测试
    std::cout << "\n--- ColliderComponent 测试 ---" << std::endl;
    RUN_TEST(Test_ColliderComponent_DefaultValues);
    RUN_TEST(Test_ColliderComponent_CreateSphere);
    RUN_TEST(Test_ColliderComponent_CreateBox);
    RUN_TEST(Test_ColliderComponent_CreateCapsule);
    
    // PhysicsUtils 测试
    std::cout << "\n--- PhysicsUtils 测试 ---" << std::endl;
    RUN_TEST(Test_PhysicsUtils_ComputeSphereMass);
    RUN_TEST(Test_PhysicsUtils_ComputeBoxMass);
    RUN_TEST(Test_PhysicsUtils_ComputeSphereInertiaTensor);
    RUN_TEST(Test_PhysicsUtils_InitializeRigidBody);
    
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

