/*
 * Copyright (c) 2025 Li Chaoyu
 * 
 * CCD 基础架构测试
 * 
 * 测试阶段 1 的基础功能：
 * - RigidBodyComponent CCD 字段
 * - CCDDetector 接口
 * - CCDCandidateDetector 快速移动物体检测
 * - PhysicsConfig CCD 配置
 */

#include "render/physics/collision/ccd_detector.h"
#include "render/physics/physics_components.h"
#include "render/physics/physics_config.h"
#include "render/physics/collision/collision_shapes.h"
#include "render/types.h"
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
            g_failedCount++; \
            return false; \
        } \
        g_passedCount++; \
    } while(0)

#define RUN_TEST(test_func) \
    do { \
        std::cout << "运行测试: " << #test_func << "..." << std::endl; \
        if (test_func()) { \
            std::cout << "✅ " << #test_func << " 通过" << std::endl; \
        } else { \
            std::cout << "❌ " << #test_func << " 失败" << std::endl; \
        } \
    } while(0)

// ============================================================================
// RigidBodyComponent CCD 字段测试
// ============================================================================

static bool Test_RigidBodyComponent_CCDFields() {
    RigidBodyComponent body;
    
    // 测试默认值
    TEST_ASSERT(!body.useCCD, "useCCD 默认应为 false");
    TEST_ASSERT(std::abs(body.ccdVelocityThreshold - 10.0f) < 0.001f, "ccdVelocityThreshold 默认应为 10.0");
    TEST_ASSERT(std::abs(body.ccdDisplacementThreshold - 0.5f) < 0.001f, "ccdDisplacementThreshold 默认应为 0.5");
    TEST_ASSERT(!body.ccdCollision.occurred, "ccdCollision.occurred 默认应为 false");
    TEST_ASSERT(std::abs(body.ccdCollision.toi) < 0.001f, "ccdCollision.toi 默认应为 0.0");
    TEST_ASSERT(body.ccdCollision.otherEntity == ECS::EntityID::Invalid(), "ccdCollision.otherEntity 默认应为 Invalid");
    
    // 测试设置值
    body.useCCD = true;
    body.ccdVelocityThreshold = 15.0f;
    body.ccdDisplacementThreshold = 0.3f;
    
    TEST_ASSERT(body.useCCD, "useCCD 应可设置为 true");
    TEST_ASSERT(std::abs(body.ccdVelocityThreshold - 15.0f) < 0.001f, "ccdVelocityThreshold 应可设置为 15.0");
    TEST_ASSERT(std::abs(body.ccdDisplacementThreshold - 0.3f) < 0.001f, "ccdDisplacementThreshold 应可设置为 0.3");
    
    return true;
}

// ============================================================================
// PhysicsConfig CCD 配置测试
// ============================================================================

static bool Test_PhysicsConfig_CCDConfig() {
    PhysicsConfig config;
    
    // 测试默认值
    TEST_ASSERT(!config.enableCCD, "enableCCD 默认应为 false");
    TEST_ASSERT(std::abs(config.ccdVelocityThreshold - 10.0f) < 0.001f, "ccdVelocityThreshold 默认应为 10.0");
    TEST_ASSERT(std::abs(config.ccdDisplacementThreshold - 0.5f) < 0.001f, "ccdDisplacementThreshold 默认应为 0.5");
    TEST_ASSERT(config.maxCCDObjects == 50, "maxCCDObjects 默认应为 50");
    TEST_ASSERT(config.maxCCDSubSteps == 5, "maxCCDSubSteps 默认应为 5");
    TEST_ASSERT(config.enableBroadPhaseCCD, "enableBroadPhaseCCD 默认应为 true");
    
    // 测试设置值
    config.enableCCD = true;
    config.ccdVelocityThreshold = 20.0f;
    config.ccdDisplacementThreshold = 0.4f;
    config.maxCCDObjects = 100;
    config.maxCCDSubSteps = 10;
    config.enableBroadPhaseCCD = false;
    
    TEST_ASSERT(config.enableCCD, "enableCCD 应可设置为 true");
    TEST_ASSERT(std::abs(config.ccdVelocityThreshold - 20.0f) < 0.001f, "ccdVelocityThreshold 应可设置为 20.0");
    TEST_ASSERT(std::abs(config.ccdDisplacementThreshold - 0.4f) < 0.001f, "ccdDisplacementThreshold 应可设置为 0.4");
    TEST_ASSERT(config.maxCCDObjects == 100, "maxCCDObjects 应可设置为 100");
    TEST_ASSERT(config.maxCCDSubSteps == 10, "maxCCDSubSteps 应可设置为 10");
    TEST_ASSERT(!config.enableBroadPhaseCCD, "enableBroadPhaseCCD 应可设置为 false");
    
    return true;
}

// ============================================================================
// CCDResult 测试
// ============================================================================

static bool Test_CCDResult_DefaultValues() {
    CCDResult result;
    
    TEST_ASSERT(!result.collided, "collided 默认应为 false");
    TEST_ASSERT(std::abs(result.toi - 1.0f) < 0.001f, "toi 默认应为 1.0");
    TEST_ASSERT(result.collisionPoint.isApprox(Vector3::Zero()), "collisionPoint 默认应为零向量");
    TEST_ASSERT(result.collisionNormal.isApprox(Vector3::Zero()), "collisionNormal 默认应为零向量");
    TEST_ASSERT(std::abs(result.penetration) < 0.001f, "penetration 默认应为 0.0");
    
    // 测试 Reset
    result.collided = true;
    result.toi = 0.5f;
    result.collisionPoint = Vector3(1.0f, 2.0f, 3.0f);
    result.collisionNormal = Vector3(0.0f, 1.0f, 0.0f);
    result.penetration = 0.1f;
    
    result.Reset();
    
    TEST_ASSERT(!result.collided, "Reset 后 collided 应为 false");
    TEST_ASSERT(std::abs(result.toi - 1.0f) < 0.001f, "Reset 后 toi 应为 1.0");
    TEST_ASSERT(result.collisionPoint.isApprox(Vector3::Zero()), "Reset 后 collisionPoint 应为零向量");
    TEST_ASSERT(result.collisionNormal.isApprox(Vector3::Zero()), "Reset 后 collisionNormal 应为零向量");
    TEST_ASSERT(std::abs(result.penetration) < 0.001f, "Reset 后 penetration 应为 0.0");
    
    return true;
}

// ============================================================================
// CCDCandidateDetector 测试
// ============================================================================

static bool Test_CCDCandidateDetector_ShouldUseCCD_ForceEnable() {
    RigidBodyComponent body;
    body.type = RigidBodyComponent::BodyType::Dynamic;
    body.useCCD = true;  // 强制启用
    
    ColliderComponent collider = ColliderComponent::CreateSphere(0.5f);
    
    // 即使速度很低，也应该启用 CCD
    body.linearVelocity = Vector3(1.0f, 0.0f, 0.0f);
    
    bool shouldUse = CCDCandidateDetector::ShouldUseCCD(body, collider, 0.016f);
    TEST_ASSERT(shouldUse, "强制启用 CCD 时应返回 true");
    
    return true;
}

static bool Test_CCDCandidateDetector_ShouldUseCCD_StaticBody() {
    RigidBodyComponent body;
    body.type = RigidBodyComponent::BodyType::Static;
    body.linearVelocity = Vector3(100.0f, 0.0f, 0.0f);  // 高速
    
    ColliderComponent collider = ColliderComponent::CreateSphere(0.5f);
    
    bool shouldUse = CCDCandidateDetector::ShouldUseCCD(body, collider, 0.016f);
    TEST_ASSERT(!shouldUse, "静态物体不需要 CCD");
    
    return true;
}

static bool Test_CCDCandidateDetector_ShouldUseCCD_VelocityThreshold() {
    RigidBodyComponent body;
    body.type = RigidBodyComponent::BodyType::Dynamic;
    body.useCCD = false;
    
    ColliderComponent collider = ColliderComponent::CreateSphere(0.5f);
    
    // 速度低于阈值
    body.linearVelocity = Vector3(5.0f, 0.0f, 0.0f);
    bool shouldUse = CCDCandidateDetector::ShouldUseCCD(body, collider, 0.016f, 10.0f);
    TEST_ASSERT(!shouldUse, "速度低于阈值时不应启用 CCD");
    
    // 速度超过阈值
    body.linearVelocity = Vector3(15.0f, 0.0f, 0.0f);
    shouldUse = CCDCandidateDetector::ShouldUseCCD(body, collider, 0.016f, 10.0f);
    TEST_ASSERT(shouldUse, "速度超过阈值时应启用 CCD");
    
    return true;
}

static bool Test_CCDCandidateDetector_ShouldUseCCD_DisplacementThreshold() {
    RigidBodyComponent body;
    body.type = RigidBodyComponent::BodyType::Dynamic;
    body.useCCD = false;
    
    ColliderComponent collider = ColliderComponent::CreateSphere(0.5f);  // 半径 0.5，直径 1.0
    
    // 位移 = 速度 * dt = 8.0 * 0.016 = 0.128
    // 形状尺寸 = 1.0
    // 位移阈值 = 0.5，所以需要位移 > 0.5 才启用 CCD
    body.linearVelocity = Vector3(8.0f, 0.0f, 0.0f);
    bool shouldUse = CCDCandidateDetector::ShouldUseCCD(body, collider, 0.016f, 10.0f, 0.5f);
    TEST_ASSERT(!shouldUse, "位移 0.128 < 0.5，不应启用 CCD");
    
    // 位移 = 50.0 * 0.016 = 0.8 > 0.5，启用 CCD
    body.linearVelocity = Vector3(50.0f, 0.0f, 0.0f);
    shouldUse = CCDCandidateDetector::ShouldUseCCD(body, collider, 0.016f, 10.0f, 0.5f);
    TEST_ASSERT(shouldUse, "位移 0.8 > 0.5，应启用 CCD");
    
    return true;
}

static bool Test_CCDCandidateDetector_ComputeShapeSize_Sphere() {
    ColliderComponent collider = ColliderComponent::CreateSphere(0.5f);
    float size = CCDCandidateDetector::ComputeShapeSize(collider);
    TEST_ASSERT(std::abs(size - 1.0f) < 0.001f, "球体尺寸应为直径 = 2 * 半径 = 1.0");
    
    return true;
}

static bool Test_CCDCandidateDetector_ComputeShapeSize_Box() {
    ColliderComponent collider = ColliderComponent::CreateBox(Vector3(1.0f, 2.0f, 0.5f));
    float size = CCDCandidateDetector::ComputeShapeSize(collider);
    TEST_ASSERT(std::abs(size - 4.0f) < 0.001f, "盒体尺寸应为最大边长 = 2 * 2.0 = 4.0");
    
    return true;
}

static bool Test_CCDCandidateDetector_ComputeShapeSize_Capsule() {
    ColliderComponent collider = ColliderComponent::CreateCapsule(0.3f, 1.0f);
    float size = CCDCandidateDetector::ComputeShapeSize(collider);
    TEST_ASSERT(std::abs(size - 1.6f) < 0.001f, "胶囊体尺寸应为高度 + 2*半径 = 1.0 + 2*0.3 = 1.6");
    
    return true;
}

// ============================================================================
// CCDDetector 接口测试（占位符测试，具体算法在后续阶段实现）
// ============================================================================

static bool Test_CCDDetector_Detect_InvalidInput() {
    CCDResult result;
    
    // 测试空指针
    bool collided = CCDDetector::Detect(
        nullptr, Vector3::Zero(), Vector3::Zero(), Quaternion::Identity(), Vector3::Zero(),
        nullptr, Vector3::Zero(), Vector3::Zero(), Quaternion::Identity(), Vector3::Zero(),
        0.016f, result
    );
    TEST_ASSERT(!collided, "空指针输入应返回 false");
    
    // 测试无效时间步长
    auto sphereA = ShapeFactory::CreateSphere(0.5f);
    auto sphereB = ShapeFactory::CreateSphere(0.5f);
    
    collided = CCDDetector::Detect(
        sphereA.get(), Vector3::Zero(), Vector3::Zero(), Quaternion::Identity(), Vector3::Zero(),
        sphereB.get(), Vector3::Zero(), Vector3::Zero(), Quaternion::Identity(), Vector3::Zero(),
        -0.016f, result
    );
    TEST_ASSERT(!collided, "负时间步长应返回 false");
    
    collided = CCDDetector::Detect(
        sphereA.get(), Vector3::Zero(), Vector3::Zero(), Quaternion::Identity(), Vector3::Zero(),
        sphereB.get(), Vector3::Zero(), Vector3::Zero(), Quaternion::Identity(), Vector3::Zero(),
        0.0f, result
    );
    TEST_ASSERT(!collided, "零时间步长应返回 false");
    
    return true;
}

static bool Test_CCDDetector_Detect_NotImplemented() {
    // 测试未实现的算法返回 false
    auto sphereA = ShapeFactory::CreateSphere(0.5f);
    auto sphereB = ShapeFactory::CreateSphere(0.5f);
    
    CCDResult result;
    bool collided = CCDDetector::Detect(
        sphereA.get(), 
        Vector3(0.0f, 0.0f, 0.0f), Vector3(10.0f, 0.0f, 0.0f),
        Quaternion::Identity(), Vector3::Zero(),
        sphereB.get(),
        Vector3(5.0f, 0.0f, 0.0f), Vector3::Zero(),
        Quaternion::Identity(), Vector3::Zero(),
        1.0f, result
    );
    
    // 目前算法未实现，应该返回 false
    TEST_ASSERT(!collided, "未实现的算法应返回 false");
    TEST_ASSERT(!result.collided, "未实现的算法 result.collided 应为 false");
    
    return true;
}

// ============================================================================
// 主函数
// ============================================================================

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    std::cout << "========================================" << std::endl;
    std::cout << "CCD 基础架构测试" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // 运行所有测试
    RUN_TEST(Test_RigidBodyComponent_CCDFields);
    RUN_TEST(Test_PhysicsConfig_CCDConfig);
    RUN_TEST(Test_CCDResult_DefaultValues);
    RUN_TEST(Test_CCDCandidateDetector_ShouldUseCCD_ForceEnable);
    RUN_TEST(Test_CCDCandidateDetector_ShouldUseCCD_StaticBody);
    RUN_TEST(Test_CCDCandidateDetector_ShouldUseCCD_VelocityThreshold);
    RUN_TEST(Test_CCDCandidateDetector_ShouldUseCCD_DisplacementThreshold);
    RUN_TEST(Test_CCDCandidateDetector_ComputeShapeSize_Sphere);
    RUN_TEST(Test_CCDCandidateDetector_ComputeShapeSize_Box);
    RUN_TEST(Test_CCDCandidateDetector_ComputeShapeSize_Capsule);
    RUN_TEST(Test_CCDDetector_Detect_InvalidInput);
    RUN_TEST(Test_CCDDetector_Detect_NotImplemented);
    
    // 输出测试结果
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "测试结果" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "总测试数: " << g_testCount << std::endl;
    std::cout << "通过: " << g_passedCount << std::endl;
    std::cout << "失败: " << g_failedCount << std::endl;
    std::cout << "========================================" << std::endl;
    
    return (g_failedCount == 0) ? 0 : 1;
}
