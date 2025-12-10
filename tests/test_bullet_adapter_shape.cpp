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
 * @file test_bullet_adapter_shape.cpp
 * @brief Bullet 适配器形状创建测试
 * 
 * 测试 ColliderComponent 到 btCollisionShape 的转换
 */

#ifdef USE_BULLET_PHYSICS

#include "render/physics/bullet_adapter/bullet_shape_adapter.h"
#include "render/physics/bullet_adapter/eigen_to_bullet.h"
#include "render/physics/physics_components.h"
#include "render/mesh.h"
#include "render/math_utils.h"
#include <BulletCollision/CollisionShapes/btSphereShape.h>
#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btCapsuleShape.h>
#include <BulletCollision/CollisionShapes/btConvexHullShape.h>
#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btCompoundShape.h>
#include <BulletCollision/BroadphaseCollision/btBroadphaseProxy.h>  // for PROXYTYPE constants
#include <iostream>
#include <cmath>
#include <cassert>

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
// 基础形状创建测试
// ============================================================================


bool Test_SphereShape_Creation() {
    auto collider = ColliderComponent::CreateSphere(2.0f);
    btCollisionShape* shape = BulletShapeAdapter::CreateShape(collider);
    
    TEST_ASSERT(shape != nullptr, "应该创建形状");
    TEST_ASSERT(shape->getShapeType() == SPHERE_SHAPE_PROXYTYPE, "应该是球体形状");
    
    // 使用 static_cast 代替 dynamic_cast（避免 RTTI 问题）
    btSphereShape* sphere = static_cast<btSphereShape*>(shape);
    TEST_ASSERT(sphere != nullptr, "应该可以转换为 btSphereShape");
    TEST_ASSERT_NEAR(sphere->getRadius(), 2.0f, 0.001f, "半径应该正确");
    
    BulletShapeAdapter::DestroyShape(shape);
    return true;
}

bool Test_BoxShape_Creation() {
    auto collider = ColliderComponent::CreateBox(Vector3(1.0f, 2.0f, 3.0f));
    btCollisionShape* shape = BulletShapeAdapter::CreateShape(collider);
    
    TEST_ASSERT(shape != nullptr, "应该创建形状");
    
    // 使用 getShapeType() 检查类型，避免 dynamic_cast 的 RTTI 问题
    TEST_ASSERT(shape->getShapeType() == BOX_SHAPE_PROXYTYPE, "应该是盒体形状");
    
    // 使用 static_cast 进行类型转换（在确认类型后）
    btBoxShape* box = static_cast<btBoxShape*>(shape);
    TEST_ASSERT(box != nullptr, "应该可以转换为 btBoxShape");
    
    BulletShapeAdapter::DestroyShape(shape);
    return true;
}

bool Test_CapsuleShape_Creation() {
    auto collider = ColliderComponent::CreateCapsule(1.0f, 3.0f);
    btCollisionShape* shape = BulletShapeAdapter::CreateShape(collider);
    
    TEST_ASSERT(shape != nullptr, "应该创建形状");
    TEST_ASSERT(shape->getShapeType() == CAPSULE_SHAPE_PROXYTYPE, "应该是胶囊体形状");
    
    // 使用 static_cast 代替 dynamic_cast（避免 RTTI 问题）
    btCapsuleShapeZ* capsule = static_cast<btCapsuleShapeZ*>(shape);
    TEST_ASSERT(capsule != nullptr, "应该可以转换为 btCapsuleShapeZ");
    TEST_ASSERT_NEAR(capsule->getRadius(), 1.0f, 0.001f, "半径应该正确");
    TEST_ASSERT_NEAR(capsule->getHalfHeight(), 1.5f, 0.001f, "半高度应该正确（高度3.0的一半）");
    
    BulletShapeAdapter::DestroyShape(shape);
    return true;
}

// ============================================================================
// 局部变换测试
// ============================================================================

bool Test_Shape_LocalTransform_Offset() {
    auto collider = ColliderComponent::CreateSphere(1.0f);
    collider.center = Vector3(1.0f, 2.0f, 3.0f);
    
    btCollisionShape* shape = BulletShapeAdapter::CreateShape(collider);
    TEST_ASSERT(shape != nullptr, "应该创建形状");
    
    // 应该使用复合形状包装
    TEST_ASSERT(shape->getShapeType() == COMPOUND_SHAPE_PROXYTYPE, "应该使用复合形状");
    btCompoundShape* compound = static_cast<btCompoundShape*>(shape);
    TEST_ASSERT(compound != nullptr, "应该可以转换为 btCompoundShape");
    TEST_ASSERT(compound->getNumChildShapes() == 1, "应该有一个子形状");
    
    // 检查局部变换
    btTransform localTransform = compound->getChildTransform(0);
    Vector3 pos;
    Quaternion rot;
    FromBullet(localTransform, pos, rot);
    
    TEST_ASSERT_NEAR(pos.x(), 1.0f, 0.001f, "X 偏移应该正确");
    TEST_ASSERT_NEAR(pos.y(), 2.0f, 0.001f, "Y 偏移应该正确");
    TEST_ASSERT_NEAR(pos.z(), 3.0f, 0.001f, "Z 偏移应该正确");
    
    BulletShapeAdapter::DestroyShape(shape);
    return true;
}

bool Test_Shape_LocalTransform_Rotation() {
    auto collider = ColliderComponent::CreateBox(Vector3(1.0f, 1.0f, 1.0f));
    collider.rotation = Quaternion(Eigen::AngleAxisf(MathUtils::PI / 2.0f, Vector3::UnitZ()));
    
    btCollisionShape* shape = BulletShapeAdapter::CreateShape(collider);
    TEST_ASSERT(shape != nullptr, "应该创建形状");
    
    TEST_ASSERT(shape->getShapeType() == COMPOUND_SHAPE_PROXYTYPE, "应该使用复合形状");
    btCompoundShape* compound = static_cast<btCompoundShape*>(shape);
    TEST_ASSERT(compound != nullptr, "应该可以转换为 btCompoundShape");
    
    BulletShapeAdapter::DestroyShape(shape);
    return true;
}

bool Test_Shape_NoLocalTransform() {
    auto collider = ColliderComponent::CreateSphere(1.0f);
    // 不设置 center 和 rotation，应该直接返回基础形状
    
    btCollisionShape* shape = BulletShapeAdapter::CreateShape(collider);
    TEST_ASSERT(shape != nullptr, "应该创建形状");
    
    // 不应该使用复合形状
    TEST_ASSERT(shape->getShapeType() != COMPOUND_SHAPE_PROXYTYPE, "不应该使用复合形状");
    
    // 应该是直接的球体形状
    TEST_ASSERT(shape->getShapeType() == SPHERE_SHAPE_PROXYTYPE, "应该是直接的球体形状");
    btSphereShape* sphere = static_cast<btSphereShape*>(shape);
    TEST_ASSERT(sphere != nullptr, "应该可以转换为 btSphereShape");
    
    BulletShapeAdapter::DestroyShape(shape);
    return true;
}

// ============================================================================
// 形状更新测试
// ============================================================================

bool Test_Shape_Update_ParameterChange() {
    auto collider1 = ColliderComponent::CreateSphere(1.0f);
    btCollisionShape* shape1 = BulletShapeAdapter::CreateShape(collider1);
    
    // 改变半径
    auto collider2 = ColliderComponent::CreateSphere(2.0f);
    btCollisionShape* shape2 = BulletShapeAdapter::UpdateShape(shape1, collider2);
    
    TEST_ASSERT(shape2 != nullptr, "参数改变时应该返回新形状");
    TEST_ASSERT(shape2 != shape1, "应该返回不同的形状");
    
    TEST_ASSERT(shape2->getShapeType() == SPHERE_SHAPE_PROXYTYPE, "应该是球体形状");
    btSphereShape* sphere = static_cast<btSphereShape*>(shape2);
    TEST_ASSERT(sphere != nullptr, "应该可以转换为 btSphereShape");
    TEST_ASSERT_NEAR(sphere->getRadius(), 2.0f, 0.001f, "新半径应该正确");
    
    BulletShapeAdapter::DestroyShape(shape1);
    BulletShapeAdapter::DestroyShape(shape2);
    return true;
}

bool Test_Shape_Update_NoChange() {
    auto collider = ColliderComponent::CreateSphere(1.0f);
    btCollisionShape* shape = BulletShapeAdapter::CreateShape(collider);
    
    // 使用相同参数更新
    btCollisionShape* updated = BulletShapeAdapter::UpdateShape(shape, collider);
    
    TEST_ASSERT(updated == nullptr, "参数未改变时应该返回 nullptr");
    
    BulletShapeAdapter::DestroyShape(shape);
    return true;
}

bool Test_Shape_Update_LocalTransformChange() {
    auto collider1 = ColliderComponent::CreateSphere(1.0f);
    collider1.center = Vector3(1.0f, 0.0f, 0.0f);
    btCollisionShape* shape = BulletShapeAdapter::CreateShape(collider1);
    
    // 改变局部变换
    auto collider2 = ColliderComponent::CreateSphere(1.0f);
    collider2.center = Vector3(2.0f, 0.0f, 0.0f);
    btCollisionShape* updated = BulletShapeAdapter::UpdateShape(shape, collider2);
    
    // 局部变换改变时，应该更新复合形状的变换，不需要重新创建
    TEST_ASSERT(updated == nullptr, "仅局部变换改变时应该返回 nullptr（已原地更新）");
    
    // 验证变换已更新
    TEST_ASSERT(shape->getShapeType() == COMPOUND_SHAPE_PROXYTYPE, "应该是复合形状");
    btCompoundShape* compound = static_cast<btCompoundShape*>(shape);
    TEST_ASSERT(compound != nullptr, "应该可以转换为 btCompoundShape");
    btTransform localTransform = compound->getChildTransform(0);
    Vector3 pos;
    Quaternion rot;
    FromBullet(localTransform, pos, rot);
    TEST_ASSERT_NEAR(pos.x(), 2.0f, 0.001f, "局部变换应该已更新");
    
    BulletShapeAdapter::DestroyShape(shape);
    return true;
}

bool Test_Shape_Update_TypeChange() {
    auto collider1 = ColliderComponent::CreateSphere(1.0f);
    btCollisionShape* shape1 = BulletShapeAdapter::CreateShape(collider1);
    
    // 改变形状类型
    auto collider2 = ColliderComponent::CreateBox(Vector3(1.0f, 1.0f, 1.0f));
    btCollisionShape* shape2 = BulletShapeAdapter::UpdateShape(shape1, collider2);
    
    TEST_ASSERT(shape2 != nullptr, "形状类型改变时应该返回新形状");
    TEST_ASSERT(shape2 != shape1, "应该返回不同的形状");
    
    TEST_ASSERT(shape2->getShapeType() == BOX_SHAPE_PROXYTYPE, "应该是盒体形状");
    btBoxShape* box = static_cast<btBoxShape*>(shape2);
    TEST_ASSERT(box != nullptr, "应该可以转换为 btBoxShape");
    
    BulletShapeAdapter::DestroyShape(shape1);
    BulletShapeAdapter::DestroyShape(shape2);
    return true;
}

// ============================================================================
// 形状共享测试
// ============================================================================

bool Test_Shape_Sharing() {
    auto collider = ColliderComponent::CreateSphere(1.0f);
    
    // 创建两个相同参数的形状
    btCollisionShape* shape1 = BulletShapeAdapter::CreateShape(collider);
    btCollisionShape* shape2 = BulletShapeAdapter::CreateShape(collider);
    
    TEST_ASSERT(shape1 != nullptr, "应该创建形状1");
    TEST_ASSERT(shape2 != nullptr, "应该创建形状2");
    
    // 由于形状共享，两个指针应该相同
    TEST_ASSERT(shape1 == shape2, "相同参数的形状应该共享");
    
    // 销毁一个，另一个应该仍然有效（因为引用计数）
    BulletShapeAdapter::DestroyShape(shape1);
    
    // 验证 shape2 仍然有效（通过检查类型）
    TEST_ASSERT(shape2->getShapeType() == SPHERE_SHAPE_PROXYTYPE, "shape2 应该仍然是球体形状");
    btSphereShape* sphere = static_cast<btSphereShape*>(shape2);
    TEST_ASSERT(sphere != nullptr, "shape2 应该仍然有效");
    
    BulletShapeAdapter::DestroyShape(shape2);
    return true;
}

// ============================================================================
// 主测试函数
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Bullet 形状适配器测试" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::cout << "\n--- 基础形状创建测试 ---" << std::endl;
    RUN_TEST(Test_SphereShape_Creation);
    RUN_TEST(Test_BoxShape_Creation);
    RUN_TEST(Test_CapsuleShape_Creation);
    
    std::cout << "\n--- 局部变换测试 ---" << std::endl;
    RUN_TEST(Test_Shape_LocalTransform_Offset);
    RUN_TEST(Test_Shape_LocalTransform_Rotation);
    RUN_TEST(Test_Shape_NoLocalTransform);
    
    std::cout << "\n--- 形状更新测试 ---" << std::endl;
    RUN_TEST(Test_Shape_Update_ParameterChange);
    RUN_TEST(Test_Shape_Update_NoChange);
    RUN_TEST(Test_Shape_Update_LocalTransformChange);
    RUN_TEST(Test_Shape_Update_TypeChange);
    
    std::cout << "\n--- 形状共享测试 ---" << std::endl;
    RUN_TEST(Test_Shape_Sharing);
    
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

#else
int main() {
    std::cout << "Bullet Physics 未启用，跳过测试" << std::endl;
    return 0;
}
#endif

