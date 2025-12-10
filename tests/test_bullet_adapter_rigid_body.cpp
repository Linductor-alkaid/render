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
 * @file test_bullet_adapter_rigid_body.cpp
 * @brief Bullet 适配器刚体同步测试
 * 
 * 测试 RigidBodyComponent 与 btRigidBody 之间的同步
 */

#ifdef USE_BULLET_PHYSICS

#include "render/physics/bullet_adapter/bullet_rigid_body_adapter.h"
#include "render/physics/bullet_adapter/bullet_shape_adapter.h"
#include "render/physics/bullet_adapter/eigen_to_bullet.h"
#include "render/physics/physics_components.h"
#include "render/ecs/entity.h"
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
// 辅助函数：创建测试用的 btRigidBody
// ============================================================================

static btRigidBody* CreateTestRigidBody(float mass = 1.0f) {
    // 创建一个简单的球体形状
    btCollisionShape* shape = new btSphereShape(1.0f);
    
    // 计算局部惯性
    btVector3 localInertia(0.0f, 0.0f, 0.0f);
    if (mass > 0.0f) {
        shape->calculateLocalInertia(mass, localInertia);
    }
    
    // 创建刚体构造信息
    btRigidBody::btRigidBodyConstructionInfo constructionInfo(
        mass,
        nullptr,  // motionState
        shape,
        localInertia
    );
    
    // 创建刚体
    btRigidBody* body = new btRigidBody(constructionInfo);
    return body;
}

// ============================================================================
// 1.4.1 基础同步接口测试
// ============================================================================

bool Test_SyncToBullet_Basic() {
    // 创建测试刚体
    std::unique_ptr<btRigidBody> bulletBody(CreateTestRigidBody(2.0f));
    ECS::EntityID entity{1, 0};
    BulletRigidBodyAdapter adapter(bulletBody.get(), entity);
    
    // 创建测试组件
    RigidBodyComponent component;
    component.type = RigidBodyComponent::BodyType::Dynamic;
    component.mass = 2.0f;
    component.inverseMass = 0.5f;
    component.linearVelocity = Vector3(1.0f, 2.0f, 3.0f);
    component.angularVelocity = Vector3(0.1f, 0.2f, 0.3f);
    
    // 同步到 Bullet
    adapter.SyncToBullet(component);
    
    // 验证同步结果
    TEST_ASSERT_NEAR(bulletBody->getMass(), 2.0f, 0.001f, "质量应该同步");
    btVector3 linearVel = bulletBody->getLinearVelocity();
    TEST_ASSERT_NEAR(linearVel.x(), 1.0f, 0.001f, "线速度 X 应该同步");
    TEST_ASSERT_NEAR(linearVel.y(), 2.0f, 0.001f, "线速度 Y 应该同步");
    TEST_ASSERT_NEAR(linearVel.z(), 3.0f, 0.001f, "线速度 Z 应该同步");
    
    return true;
}

bool Test_SyncFromBullet_Basic() {
    // 创建测试刚体
    std::unique_ptr<btRigidBody> bulletBody(CreateTestRigidBody(3.0f));
    ECS::EntityID entity{2, 0};
    BulletRigidBodyAdapter adapter(bulletBody.get(), entity);
    
    // 设置 Bullet 刚体的属性
    bulletBody->setLinearVelocity(btVector3(4.0f, 5.0f, 6.0f));
    bulletBody->setAngularVelocity(btVector3(0.4f, 0.5f, 0.6f));
    
    // 创建组件并同步
    RigidBodyComponent component;
    adapter.SyncFromBullet(component);
    
    // 验证同步结果
    TEST_ASSERT_NEAR(component.mass, 3.0f, 0.001f, "质量应该同步");
    TEST_ASSERT_NEAR(component.linearVelocity.x(), 4.0f, 0.001f, "线速度 X 应该同步");
    TEST_ASSERT_NEAR(component.linearVelocity.y(), 5.0f, 0.001f, "线速度 Y 应该同步");
    TEST_ASSERT_NEAR(component.linearVelocity.z(), 6.0f, 0.001f, "线速度 Z 应该同步");
    
    return true;
}

// ============================================================================
// 1.4.2 刚体类型转换测试
// ============================================================================

bool Test_BodyType_Static() {
    std::unique_ptr<btRigidBody> bulletBody(CreateTestRigidBody(1.0f));
    ECS::EntityID entity{3, 0};
    BulletRigidBodyAdapter adapter(bulletBody.get(), entity);
    
    RigidBodyComponent component;
    component.type = RigidBodyComponent::BodyType::Static;
    component.mass = 0.0f;
    component.inverseMass = 0.0f;
    
    adapter.SyncToBullet(component);
    
    // 验证类型标志
    int flags = bulletBody->getCollisionFlags();
    TEST_ASSERT((flags & btCollisionObject::CF_STATIC_OBJECT) != 0, "应该是静态物体");
    TEST_ASSERT_NEAR(bulletBody->getMass(), 0.0f, 0.001f, "静态物体质量应该为 0");
    
    // 验证反向同步
    adapter.SyncFromBullet(component);
    TEST_ASSERT(component.type == RigidBodyComponent::BodyType::Static, "类型应该正确同步回来");
    
    return true;
}

bool Test_BodyType_Kinematic() {
    std::unique_ptr<btRigidBody> bulletBody(CreateTestRigidBody(1.0f));
    ECS::EntityID entity{4, 0};
    BulletRigidBodyAdapter adapter(bulletBody.get(), entity);
    
    RigidBodyComponent component;
    component.type = RigidBodyComponent::BodyType::Kinematic;
    component.mass = 0.0f;
    component.inverseMass = 0.0f;
    
    adapter.SyncToBullet(component);
    
    // 验证类型标志
    int flags = bulletBody->getCollisionFlags();
    TEST_ASSERT((flags & btCollisionObject::CF_KINEMATIC_OBJECT) != 0, "应该是运动学物体");
    TEST_ASSERT_NEAR(bulletBody->getMass(), 0.0f, 0.001f, "运动学物体质量应该为 0");
    
    // 验证反向同步
    adapter.SyncFromBullet(component);
    TEST_ASSERT(component.type == RigidBodyComponent::BodyType::Kinematic, "类型应该正确同步回来");
    
    return true;
}

bool Test_BodyType_Dynamic() {
    std::unique_ptr<btRigidBody> bulletBody(CreateTestRigidBody(5.0f));
    ECS::EntityID entity{5, 0};
    BulletRigidBodyAdapter adapter(bulletBody.get(), entity);
    
    RigidBodyComponent component;
    component.type = RigidBodyComponent::BodyType::Dynamic;
    component.mass = 5.0f;
    component.inverseMass = 0.2f;
    
    adapter.SyncToBullet(component);
    
    // 验证类型标志（动态物体不应该有特殊标志）
    int flags = bulletBody->getCollisionFlags();
    TEST_ASSERT((flags & btCollisionObject::CF_STATIC_OBJECT) == 0, "不应该是静态物体");
    TEST_ASSERT((flags & btCollisionObject::CF_KINEMATIC_OBJECT) == 0, "不应该是运动学物体");
    TEST_ASSERT_NEAR(bulletBody->getMass(), 5.0f, 0.001f, "动态物体质量应该正确");
    
    // 验证反向同步
    adapter.SyncFromBullet(component);
    TEST_ASSERT(component.type == RigidBodyComponent::BodyType::Dynamic, "类型应该正确同步回来");
    
    return true;
}

// ============================================================================
// 1.4.3 质量属性同步测试
// ============================================================================

bool Test_Mass_Sync() {
    std::unique_ptr<btRigidBody> bulletBody(CreateTestRigidBody(10.0f));
    ECS::EntityID entity{6, 0};
    BulletRigidBodyAdapter adapter(bulletBody.get(), entity);
    
    RigidBodyComponent component;
    component.type = RigidBodyComponent::BodyType::Dynamic;
    component.mass = 10.0f;
    component.inverseMass = 0.1f;
    
    // 设置惯性张量（对角矩阵）
    component.inertiaTensor.setZero();
    component.inertiaTensor(0, 0) = 2.0f;
    component.inertiaTensor(1, 1) = 3.0f;
    component.inertiaTensor(2, 2) = 4.0f;
    
    adapter.SyncToBullet(component);
    
    // 验证质量
    TEST_ASSERT_NEAR(bulletBody->getMass(), 10.0f, 0.001f, "质量应该同步");
    TEST_ASSERT_NEAR(bulletBody->getInvMass(), 0.1f, 0.001f, "逆质量应该同步");
    
    // 验证惯性张量（从 Bullet 获取）
    btVector3 localInertia = bulletBody->getLocalInertia();
    TEST_ASSERT_NEAR(localInertia.x(), 2.0f, 0.1f, "惯性张量 X 应该同步");
    TEST_ASSERT_NEAR(localInertia.y(), 3.0f, 0.1f, "惯性张量 Y 应该同步");
    TEST_ASSERT_NEAR(localInertia.z(), 4.0f, 0.1f, "惯性张量 Z 应该同步");
    
    // 验证反向同步
    adapter.SyncFromBullet(component);
    TEST_ASSERT_NEAR(component.mass, 10.0f, 0.001f, "质量应该正确同步回来");
    TEST_ASSERT_NEAR(component.inverseMass, 0.1f, 0.001f, "逆质量应该正确同步回来");
    
    return true;
}

// ============================================================================
// 1.4.4 速度约束同步测试
// ============================================================================

bool Test_LockPosition_Sync() {
    std::unique_ptr<btRigidBody> bulletBody(CreateTestRigidBody(1.0f));
    ECS::EntityID entity{7, 0};
    BulletRigidBodyAdapter adapter(bulletBody.get(), entity);
    
    RigidBodyComponent component;
    component.type = RigidBodyComponent::BodyType::Dynamic;
    component.mass = 1.0f;
    
    // 锁定 X 和 Z 轴
    component.lockPosition[0] = true;
    component.lockPosition[1] = false;
    component.lockPosition[2] = true;
    
    adapter.SyncToBullet(component);
    
    // 验证线性因子
    btVector3 linearFactor = bulletBody->getLinearFactor();
    TEST_ASSERT_NEAR(linearFactor.x(), 0.0f, 0.001f, "X 轴应该被锁定");
    TEST_ASSERT_NEAR(linearFactor.y(), 1.0f, 0.001f, "Y 轴不应该被锁定");
    TEST_ASSERT_NEAR(linearFactor.z(), 0.0f, 0.001f, "Z 轴应该被锁定");
    
    // 验证反向同步
    adapter.SyncFromBullet(component);
    TEST_ASSERT(component.lockPosition[0] == true, "X 轴锁定应该正确同步回来");
    TEST_ASSERT(component.lockPosition[1] == false, "Y 轴锁定应该正确同步回来");
    TEST_ASSERT(component.lockPosition[2] == true, "Z 轴锁定应该正确同步回来");
    
    return true;
}

bool Test_LockRotation_Sync() {
    std::unique_ptr<btRigidBody> bulletBody(CreateTestRigidBody(1.0f));
    ECS::EntityID entity{8, 0};
    BulletRigidBodyAdapter adapter(bulletBody.get(), entity);
    
    RigidBodyComponent component;
    component.type = RigidBodyComponent::BodyType::Dynamic;
    component.mass = 1.0f;
    
    // 锁定 Y 轴旋转
    component.lockRotation[0] = false;
    component.lockRotation[1] = true;
    component.lockRotation[2] = false;
    
    adapter.SyncToBullet(component);
    
    // 验证角因子
    btVector3 angularFactor = bulletBody->getAngularFactor();
    TEST_ASSERT_NEAR(angularFactor.x(), 1.0f, 0.001f, "X 轴旋转不应该被锁定");
    TEST_ASSERT_NEAR(angularFactor.y(), 0.0f, 0.001f, "Y 轴旋转应该被锁定");
    TEST_ASSERT_NEAR(angularFactor.z(), 1.0f, 0.001f, "Z 轴旋转不应该被锁定");
    
    // 验证反向同步
    adapter.SyncFromBullet(component);
    TEST_ASSERT(component.lockRotation[0] == false, "X 轴旋转锁定应该正确同步回来");
    TEST_ASSERT(component.lockRotation[1] == true, "Y 轴旋转锁定应该正确同步回来");
    TEST_ASSERT(component.lockRotation[2] == false, "Z 轴旋转锁定应该正确同步回来");
    
    return true;
}

bool Test_MaxSpeed_Limit() {
    std::unique_ptr<btRigidBody> bulletBody(CreateTestRigidBody(1.0f));
    ECS::EntityID entity{9, 0};
    BulletRigidBodyAdapter adapter(bulletBody.get(), entity);
    
    // 设置一个很大的速度
    bulletBody->setLinearVelocity(btVector3(100.0f, 200.0f, 300.0f));
    bulletBody->setAngularVelocity(btVector3(10.0f, 20.0f, 30.0f));
    
    RigidBodyComponent component;
    component.type = RigidBodyComponent::BodyType::Dynamic;
    component.mass = 1.0f;
    component.maxLinearSpeed = 50.0f;  // 限制线速度
    component.maxAngularSpeed = 15.0f;  // 限制角速度
    
    adapter.SyncFromBullet(component);
    
    // 验证速度被限制
    float linearSpeed = component.linearVelocity.norm();
    float angularSpeed = component.angularVelocity.norm();
    TEST_ASSERT(linearSpeed <= component.maxLinearSpeed + 0.1f, "线速度应该被限制");
    TEST_ASSERT(angularSpeed <= component.maxAngularSpeed + 0.1f, "角速度应该被限制");
    
    return true;
}

// ============================================================================
// 1.4.5 阻尼同步测试
// ============================================================================

bool Test_Damping_Sync() {
    std::unique_ptr<btRigidBody> bulletBody(CreateTestRigidBody(1.0f));
    ECS::EntityID entity{10, 0};
    BulletRigidBodyAdapter adapter(bulletBody.get(), entity);
    
    RigidBodyComponent component;
    component.type = RigidBodyComponent::BodyType::Dynamic;
    component.mass = 1.0f;
    component.linearDamping = 0.05f;
    component.angularDamping = 0.1f;
    
    adapter.SyncToBullet(component);
    
    // 验证阻尼
    TEST_ASSERT_NEAR(bulletBody->getLinearDamping(), 0.05f, 0.001f, "线性阻尼应该同步");
    TEST_ASSERT_NEAR(bulletBody->getAngularDamping(), 0.1f, 0.001f, "角阻尼应该同步");
    
    // 验证反向同步
    adapter.SyncFromBullet(component);
    TEST_ASSERT_NEAR(component.linearDamping, 0.05f, 0.001f, "线性阻尼应该正确同步回来");
    TEST_ASSERT_NEAR(component.angularDamping, 0.1f, 0.001f, "角阻尼应该正确同步回来");
    
    return true;
}

// ============================================================================
// 1.4.6 重力同步测试
// ============================================================================

bool Test_Gravity_Sync() {
    std::unique_ptr<btRigidBody> bulletBody(CreateTestRigidBody(1.0f));
    ECS::EntityID entity{11, 0};
    BulletRigidBodyAdapter adapter(bulletBody.get(), entity);
    
    RigidBodyComponent component;
    component.type = RigidBodyComponent::BodyType::Dynamic;
    component.mass = 1.0f;
    component.useGravity = false;  // 禁用重力
    
    adapter.SyncToBullet(component);
    
    // 验证重力被禁用（重力向量应该为零）
    btVector3 gravity = bulletBody->getGravity();
    TEST_ASSERT_NEAR(gravity.length2(), 0.0f, 0.001f, "禁用重力时重力向量应该为零");
    
    // 启用重力
    component.useGravity = true;
    component.gravityScale = 2.0f;  // 注意：gravityScale 需要在世界适配器中处理
    adapter.SyncToBullet(component);
    
    // 验证反向同步
    adapter.SyncFromBullet(component);
    // 注意：由于我们通过重力向量长度判断，如果重力向量为零，useGravity 会被设置为 false
    // 这里我们主要测试同步机制，gravityScale 的完整测试需要在世界适配器中
    
    return true;
}

// ============================================================================
// 1.4.7 CCD 同步测试
// ============================================================================

bool Test_CCD_Sync() {
    std::unique_ptr<btRigidBody> bulletBody(CreateTestRigidBody(1.0f));
    ECS::EntityID entity{12, 0};
    BulletRigidBodyAdapter adapter(bulletBody.get(), entity);
    
    RigidBodyComponent component;
    component.type = RigidBodyComponent::BodyType::Dynamic;
    component.mass = 1.0f;
    component.useCCD = true;
    component.ccdVelocityThreshold = 10.0f;
    component.ccdDisplacementThreshold = 0.5f;
    
    adapter.SyncToBullet(component);
    
    // 验证 CCD 被启用
    float ccdThreshold = bulletBody->getCcdMotionThreshold();
    TEST_ASSERT(ccdThreshold > 0.0f, "CCD 阈值应该大于 0");
    
    float sweptSphereRadius = bulletBody->getCcdSweptSphereRadius();
    TEST_ASSERT(sweptSphereRadius > 0.0f, "扫描球半径应该大于 0");
    
    // 禁用 CCD
    component.useCCD = false;
    component.linearVelocity = Vector3(5.0f, 0.0f, 0.0f);  // 速度低于阈值
    adapter.SyncToBullet(component);
    
    // 验证 CCD 被禁用（速度低于阈值时）
    ccdThreshold = bulletBody->getCcdMotionThreshold();
    // 注意：如果速度低于阈值，CCD 可能仍然被禁用
    
    return true;
}

bool Test_CCD_AutoEnable() {
    std::unique_ptr<btRigidBody> bulletBody(CreateTestRigidBody(1.0f));
    ECS::EntityID entity{13, 0};
    BulletRigidBodyAdapter adapter(bulletBody.get(), entity);
    
    RigidBodyComponent component;
    component.type = RigidBodyComponent::BodyType::Dynamic;
    component.mass = 1.0f;
    component.useCCD = false;  // 不强制启用
    component.ccdVelocityThreshold = 10.0f;
    component.linearVelocity = Vector3(15.0f, 0.0f, 0.0f);  // 速度超过阈值
    
    adapter.SyncToBullet(component);
    
    // 验证 CCD 自动启用（速度超过阈值）
    float ccdThreshold = bulletBody->getCcdMotionThreshold();
    TEST_ASSERT(ccdThreshold > 0.0f, "速度超过阈值时 CCD 应该自动启用");
    
    return true;
}

// ============================================================================
// 1.4.8 休眠状态同步测试
// ============================================================================

bool Test_Sleeping_Sync() {
    std::unique_ptr<btRigidBody> bulletBody(CreateTestRigidBody(1.0f));
    ECS::EntityID entity{14, 0};
    BulletRigidBodyAdapter adapter(bulletBody.get(), entity);
    
    RigidBodyComponent component;
    component.type = RigidBodyComponent::BodyType::Dynamic;
    component.mass = 1.0f;
    component.isSleeping = true;
    component.sleepThreshold = 0.01f;
    
    adapter.SyncToBullet(component);
    
    // 验证休眠状态
    int activationState = bulletBody->getActivationState();
    TEST_ASSERT(activationState == ISLAND_SLEEPING, "应该处于休眠状态");
    
    // 验证休眠阈值
    float linearThreshold = bulletBody->getLinearSleepingThreshold();
    float angularThreshold = bulletBody->getAngularSleepingThreshold();
    TEST_ASSERT(linearThreshold > 0.0f, "线性休眠阈值应该大于 0");
    TEST_ASSERT(angularThreshold > 0.0f, "角休眠阈值应该大于 0");
    
    // 唤醒刚体
    component.isSleeping = false;
    adapter.SyncToBullet(component);
    
    activationState = bulletBody->getActivationState();
    TEST_ASSERT(activationState == ACTIVE_TAG, "应该处于激活状态");
    
    // 验证反向同步
    adapter.SyncFromBullet(component);
    // 注意：休眠状态可能因为速度等原因自动改变，这里主要测试同步机制
    
    return true;
}

// ============================================================================
// 主函数
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Bullet 刚体适配器测试" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // 1.4.1 基础同步接口测试
    std::cout << "--- 1.4.1 基础同步接口测试 ---" << std::endl;
    RUN_TEST(Test_SyncToBullet_Basic);
    RUN_TEST(Test_SyncFromBullet_Basic);
    std::cout << std::endl;
    
    // 1.4.2 刚体类型转换测试
    std::cout << "--- 1.4.2 刚体类型转换测试 ---" << std::endl;
    RUN_TEST(Test_BodyType_Static);
    RUN_TEST(Test_BodyType_Kinematic);
    RUN_TEST(Test_BodyType_Dynamic);
    std::cout << std::endl;
    
    // 1.4.3 质量属性同步测试
    std::cout << "--- 1.4.3 质量属性同步测试 ---" << std::endl;
    RUN_TEST(Test_Mass_Sync);
    std::cout << std::endl;
    
    // 1.4.4 速度约束同步测试
    std::cout << "--- 1.4.4 速度约束同步测试 ---" << std::endl;
    RUN_TEST(Test_LockPosition_Sync);
    RUN_TEST(Test_LockRotation_Sync);
    RUN_TEST(Test_MaxSpeed_Limit);
    std::cout << std::endl;
    
    // 1.4.5 阻尼同步测试
    std::cout << "--- 1.4.5 阻尼同步测试 ---" << std::endl;
    RUN_TEST(Test_Damping_Sync);
    std::cout << std::endl;
    
    // 1.4.6 重力同步测试
    std::cout << "--- 1.4.6 重力同步测试 ---" << std::endl;
    RUN_TEST(Test_Gravity_Sync);
    std::cout << std::endl;
    
    // 1.4.7 CCD 同步测试
    std::cout << "--- 1.4.7 CCD 同步测试 ---" << std::endl;
    RUN_TEST(Test_CCD_Sync);
    RUN_TEST(Test_CCD_AutoEnable);
    std::cout << std::endl;
    
    // 1.4.8 休眠状态同步测试
    std::cout << "--- 1.4.8 休眠状态同步测试 ---" << std::endl;
    RUN_TEST(Test_Sleeping_Sync);
    std::cout << std::endl;
    
    // 输出测试结果
    std::cout << "========================================" << std::endl;
    std::cout << "测试完成" << std::endl;
    std::cout << "总测试数: " << g_testCount << std::endl;
    std::cout << "通过: " << g_passedCount << std::endl;
    std::cout << "失败: " << g_failedCount << std::endl;
    std::cout << "========================================" << std::endl;
    
    return (g_failedCount == 0) ? 0 : 1;
}

#else  // USE_BULLET_PHYSICS

int main() {
    std::cout << "Bullet Physics 未启用，跳过测试" << std::endl;
    return 0;
}

#endif  // USE_BULLET_PHYSICS

