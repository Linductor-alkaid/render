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
 * @file test_bullet_adapter_eigen_to_bullet.cpp
 * @brief Bullet 适配器数据转换层测试
 * 
 * 测试 Eigen 类型与 Bullet 类型之间的转换函数
 */

#ifdef USE_BULLET_PHYSICS

#include "render/physics/bullet_adapter/eigen_to_bullet.h"
#include "render/types.h"
#include <LinearMath/btVector3.h>
#include <LinearMath/btQuaternion.h>
#include <LinearMath/btMatrix3x3.h>
#include <LinearMath/btTransform.h>
#include "render/math_utils.h"
#include <iostream>
#include <cmath>
#include <cassert>

#define PI Render::MathUtils::PI
using namespace Render;
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
        if (test_func()) { \
            std::cout << "✓ " << #test_func << " 通过" << std::endl; \
        } else { \
            std::cout << "✗ " << #test_func << " 失败" << std::endl; \
        } \
    } while(0)

// ============================================================================
// Vector3 转换测试
// ============================================================================

bool Test_Vector3_ToBullet() {
    Vector3 eigenVec(1.0f, 2.0f, 3.0f);
    btVector3 bulletVec = ToBullet(eigenVec);
    
    TEST_ASSERT_NEAR(bulletVec.x(), 1.0f, 0.0001f, "Vector3 x 坐标应该正确");
    TEST_ASSERT_NEAR(bulletVec.y(), 2.0f, 0.0001f, "Vector3 y 坐标应该正确");
    TEST_ASSERT_NEAR(bulletVec.z(), 3.0f, 0.0001f, "Vector3 z 坐标应该正确");
    
    return true;
}

bool Test_Vector3_FromBullet() {
    btVector3 bulletVec(4.0f, 5.0f, 6.0f);
    Vector3 eigenVec = FromBullet(bulletVec);
    
    TEST_ASSERT_NEAR(eigenVec.x(), 4.0f, 0.0001f, "Vector3 x 坐标应该正确");
    TEST_ASSERT_NEAR(eigenVec.y(), 5.0f, 0.0001f, "Vector3 y 坐标应该正确");
    TEST_ASSERT_NEAR(eigenVec.z(), 6.0f, 0.0001f, "Vector3 z 坐标应该正确");
    
    return true;
}

bool Test_Vector3_RoundTrip() {
    Vector3 original(7.0f, 8.0f, 9.0f);
    btVector3 bullet = ToBullet(original);
    Vector3 back = FromBullet(bullet);
    
    TEST_ASSERT(back.isApprox(original, 0.0001f), "Vector3 往返转换应该保持值不变");
    
    return true;
}

bool Test_Vector3_Zero() {
    Vector3 zero = Vector3::Zero();
    btVector3 bulletZero = ToBullet(zero);
    Vector3 back = FromBullet(bulletZero);
    
    TEST_ASSERT(back.isApprox(zero, 0.0001f), "零向量转换应该正确");
    TEST_ASSERT_NEAR(bulletZero.length2(), 0.0f, 0.0001f, "零向量长度应该为 0");
    
    return true;
}

bool Test_Vector3_Negative() {
    Vector3 negative(-1.0f, -2.0f, -3.0f);
    btVector3 bullet = ToBullet(negative);
    Vector3 back = FromBullet(bullet);
    
    TEST_ASSERT(back.isApprox(negative, 0.0001f), "负向量转换应该正确");
    
    return true;
}

// ============================================================================
// Quaternion 转换测试
// ============================================================================

bool Test_Quaternion_ToBullet() {
    // Eigen 四元数顺序: (w, x, y, z)
    Quaternion eigenQuat(0.9238795f, 0.0f, 0.3826834f, 0.0f);  // 45度绕Y轴旋转
    btQuaternion bulletQuat = ToBullet(eigenQuat);
    
    // Bullet 四元数顺序: (x, y, z, w)
    TEST_ASSERT_NEAR(bulletQuat.x(), 0.0f, 0.0001f, "Quaternion x 分量应该正确");
    TEST_ASSERT_NEAR(bulletQuat.y(), 0.3826834f, 0.0001f, "Quaternion y 分量应该正确");
    TEST_ASSERT_NEAR(bulletQuat.z(), 0.0f, 0.0001f, "Quaternion z 分量应该正确");
    TEST_ASSERT_NEAR(bulletQuat.w(), 0.9238795f, 0.0001f, "Quaternion w 分量应该正确");
    
    return true;
}

bool Test_Quaternion_FromBullet() {
    // Bullet 四元数顺序: (x, y, z, w)
    btQuaternion bulletQuat(0.0f, 0.3826834f, 0.0f, 0.9238795f);
    Quaternion eigenQuat = FromBullet(bulletQuat);
    
    // Eigen 四元数顺序: (w, x, y, z)
    TEST_ASSERT_NEAR(eigenQuat.w(), 0.9238795f, 0.0001f, "Quaternion w 分量应该正确");
    TEST_ASSERT_NEAR(eigenQuat.x(), 0.0f, 0.0001f, "Quaternion x 分量应该正确");
    TEST_ASSERT_NEAR(eigenQuat.y(), 0.3826834f, 0.0001f, "Quaternion y 分量应该正确");
    TEST_ASSERT_NEAR(eigenQuat.z(), 0.0f, 0.0001f, "Quaternion z 分量应该正确");
    
    return true;
}

bool Test_Quaternion_RoundTrip() {
    Quaternion original = Quaternion::Identity();
    btQuaternion bullet = ToBullet(original);
    Quaternion back = FromBullet(bullet);
    
    TEST_ASSERT(back.coeffs().isApprox(original.coeffs(), 0.0001f), 
                "单位四元数往返转换应该保持值不变");
    
    return true;
}

bool Test_Quaternion_RotationConsistency() {
    // 测试旋转一致性：转换前后应该表示相同的旋转
    
    // 创建一个 90 度绕 Z 轴旋转的四元数
    float angle = PI / 2.0f;  // 90 度
    Vector3 axis(0.0f, 0.0f, 1.0f);  // Z 轴
    Quaternion eigenQuat = Quaternion(Eigen::AngleAxisf(angle, axis));
    
    // 转换为 Bullet 四元数
    btQuaternion bulletQuat = ToBullet(eigenQuat);
    
    // 测试：对一个向量应用旋转，结果应该相同
    Vector3 testVec(1.0f, 0.0f, 0.0f);  // X 轴方向
    
    // 使用 Eigen 四元数旋转
    Vector3 eigenResult = eigenQuat * testVec;
    
    // 使用 Bullet 四元数旋转
    btVector3 bulletTestVec = ToBullet(testVec);
    btVector3 bulletResult = quatRotate(bulletQuat, bulletTestVec);
    Vector3 eigenFromBulletResult = FromBullet(bulletResult);
    
    // 验证结果应该相同（90度绕Z轴旋转 (1,0,0) -> (0,1,0)）
    TEST_ASSERT(eigenFromBulletResult.isApprox(eigenResult, 0.0001f),
                "四元数旋转应该保持一致");
    TEST_ASSERT(eigenFromBulletResult.isApprox(Vector3(0.0f, 1.0f, 0.0f), 0.0001f),
                "旋转结果应该正确");
    
    return true;
}

bool Test_Quaternion_MultipleRotations() {
    // 测试多个旋转的转换一致性
    
    // 旋转1: 90度绕X轴
    Quaternion rot1 = Quaternion(Eigen::AngleAxisf(PI / 2.0f, Vector3::UnitX()));
    btQuaternion bulletRot1 = ToBullet(rot1);
    
    // 旋转2: 90度绕Y轴
    Quaternion rot2 = Quaternion(Eigen::AngleAxisf(PI / 2.0f, Vector3::UnitY()));
    btQuaternion bulletRot2 = ToBullet(rot2);
    
    // 组合旋转
    Quaternion combinedEigen = rot2 * rot1;
    btQuaternion combinedBullet = bulletRot2 * bulletRot1;
    Quaternion backFromBullet = FromBullet(combinedBullet);
    
    // 验证组合旋转应该相同
    TEST_ASSERT(backFromBullet.coeffs().isApprox(combinedEigen.coeffs(), 0.0001f),
                "组合旋转应该保持一致");
    
    // 测试应用到向量
    Vector3 testVec(1.0f, 1.0f, 1.0f);
    Vector3 eigenResult = combinedEigen * testVec;
    btVector3 bulletResult = quatRotate(combinedBullet, ToBullet(testVec));
    Vector3 bulletEigenResult = FromBullet(bulletResult);
    
    TEST_ASSERT(bulletEigenResult.isApprox(eigenResult, 0.0001f),
                "组合旋转应用到向量应该保持一致");
    
    return true;
}

// ============================================================================
// Matrix3 转换测试
// ============================================================================

bool Test_Matrix3_ToBullet() {
    Matrix3 eigenMat;
    eigenMat << 1.0f, 2.0f, 3.0f,
                4.0f, 5.0f, 6.0f,
                7.0f, 8.0f, 9.0f;
    
    btMatrix3x3 bulletMat = ToBullet(eigenMat);
    
    // 验证所有元素
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            TEST_ASSERT_NEAR(bulletMat[i][j], eigenMat(i, j), 0.0001f,
                            "Matrix3 元素应该正确");
        }
    }
    
    return true;
}

bool Test_Matrix3_FromBullet() {
    btMatrix3x3 bulletMat;
    bulletMat[0][0] = 1.0f; bulletMat[0][1] = 2.0f; bulletMat[0][2] = 3.0f;
    bulletMat[1][0] = 4.0f; bulletMat[1][1] = 5.0f; bulletMat[1][2] = 6.0f;
    bulletMat[2][0] = 7.0f; bulletMat[2][1] = 8.0f; bulletMat[2][2] = 9.0f;
    
    Matrix3 eigenMat = FromBullet(bulletMat);
    
    // 验证所有元素
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            TEST_ASSERT_NEAR(eigenMat(i, j), bulletMat[i][j], 0.0001f,
                            "Matrix3 元素应该正确");
        }
    }
    
    return true;
}

bool Test_Matrix3_RoundTrip() {
    Matrix3 original;
    original << 1.0f, 2.0f, 3.0f,
                4.0f, 5.0f, 6.0f,
                7.0f, 8.0f, 9.0f;
    
    btMatrix3x3 bullet = ToBullet(original);
    Matrix3 back = FromBullet(bullet);
    
    TEST_ASSERT(back.isApprox(original, 0.0001f), "Matrix3 往返转换应该保持值不变");
    
    return true;
}

bool Test_Matrix3_Identity() {
    Matrix3 identity = Matrix3::Identity();
    btMatrix3x3 bulletIdentity = ToBullet(identity);
    Matrix3 back = FromBullet(bulletIdentity);
    
    TEST_ASSERT(back.isApprox(identity, 0.0001f), "单位矩阵转换应该正确");
    
    // 验证 Bullet 矩阵也是单位矩阵
    btMatrix3x3 expectedIdentity;
    expectedIdentity.setIdentity();
    
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            float expected = (i == j) ? 1.0f : 0.0f;
            TEST_ASSERT_NEAR(bulletIdentity[i][j], expected, 0.0001f,
                            "单位矩阵元素应该正确");
        }
    }
    
    return true;
}

bool Test_Matrix3_RotationFromQuaternion() {
    // 测试从四元数创建旋转矩阵的一致性
    
    // 创建旋转四元数（90度绕Z轴）
    Quaternion eigenQuat = Quaternion(Eigen::AngleAxisf(PI / 2.0f, Vector3::UnitZ()));
    Matrix3 eigenMat = eigenQuat.toRotationMatrix();
    
    btQuaternion bulletQuat = ToBullet(eigenQuat);
    btMatrix3x3 bulletMat;
    bulletMat.setRotation(bulletQuat);
    
    Matrix3 eigenFromBullet = FromBullet(bulletMat);
    
    // 验证旋转矩阵应该相同
    TEST_ASSERT(eigenFromBullet.isApprox(eigenMat, 0.0001f),
                "从四元数创建的旋转矩阵应该保持一致");
    
    // 测试应用到向量
    Vector3 testVec(1.0f, 0.0f, 0.0f);
    Vector3 eigenResult = eigenMat * testVec;
    btVector3 bulletResult = bulletMat * ToBullet(testVec);
    Vector3 bulletEigenResult = FromBullet(bulletResult);
    
    TEST_ASSERT(bulletEigenResult.isApprox(eigenResult, 0.0001f),
                "旋转矩阵应用到向量应该保持一致");
    
    return true;
}

// ============================================================================
// Transform 转换测试
// ============================================================================

bool Test_Transform_ToBullet() {
    Vector3 pos(1.0f, 2.0f, 3.0f);
    Quaternion rot = Quaternion::Identity();
    
    btTransform bulletTransform = ToBullet(pos, rot);
    
    Vector3 bulletPos = FromBullet(bulletTransform.getOrigin());
    Quaternion bulletRot = FromBullet(bulletTransform.getRotation());
    
    TEST_ASSERT(bulletPos.isApprox(pos, 0.0001f), "Transform 位置应该正确");
    TEST_ASSERT(bulletRot.coeffs().isApprox(rot.coeffs(), 0.0001f), "Transform 旋转应该正确");
    
    return true;
}

bool Test_Transform_FromBullet() {
    btTransform bulletTransform;
    bulletTransform.setOrigin(btVector3(4.0f, 5.0f, 6.0f));
    bulletTransform.setRotation(btQuaternion(0.0f, 0.0f, 0.0f, 1.0f));  // 单位四元数
    
    Vector3 pos;
    Quaternion rot;
    FromBullet(bulletTransform, pos, rot);
    
    TEST_ASSERT_NEAR(pos.x(), 4.0f, 0.0001f, "Transform 位置 x 应该正确");
    TEST_ASSERT_NEAR(pos.y(), 5.0f, 0.0001f, "Transform 位置 y 应该正确");
    TEST_ASSERT_NEAR(pos.z(), 6.0f, 0.0001f, "Transform 位置 z 应该正确");
    TEST_ASSERT(rot.coeffs().isApprox(Quaternion::Identity().coeffs(), 0.0001f),
                "Transform 旋转应该是单位四元数");
    
    return true;
}

bool Test_Transform_RoundTrip() {
    Vector3 originalPos(7.0f, 8.0f, 9.0f);
    Quaternion originalRot = Quaternion(Eigen::AngleAxisf(PI / 4.0f, Vector3::UnitY()));
    
    btTransform bullet = ToBullet(originalPos, originalRot);
    Vector3 backPos;
    Quaternion backRot;
    FromBullet(bullet, backPos, backRot);
    
    TEST_ASSERT(backPos.isApprox(originalPos, 0.0001f), "Transform 位置往返转换应该保持值不变");
    TEST_ASSERT(backRot.coeffs().isApprox(originalRot.coeffs(), 0.0001f),
                "Transform 旋转往返转换应该保持值不变");
    
    return true;
}

bool Test_Transform_TransformPoint() {
    // 测试变换应用到点的一致性
    
    Vector3 pos(1.0f, 2.0f, 3.0f);
    Quaternion rot = Quaternion(Eigen::AngleAxisf(PI / 2.0f, Vector3::UnitZ()));
    Vector3 point(1.0f, 0.0f, 0.0f);
    
    // 使用 Eigen 变换
    Eigen::Transform<float, 3, Eigen::Affine> eigenTransform = 
        Eigen::Translation3f(pos) * rot;
    Vector3 eigenResult = eigenTransform * point;
    
    // 使用 Bullet 变换
    btTransform bulletTransform = ToBullet(pos, rot);
    btVector3 bulletPoint = ToBullet(point);
    btVector3 bulletResult = bulletTransform * bulletPoint;
    Vector3 bulletEigenResult = FromBullet(bulletResult);
    
    // 验证结果应该相同
    TEST_ASSERT(bulletEigenResult.isApprox(eigenResult, 0.0001f),
                "变换应用到点应该保持一致");
    
    return true;
}

// ============================================================================
// 主测试函数
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Bullet 适配器数据转换层测试" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Vector3 转换测试
    std::cout << "\n--- Vector3 转换测试 ---" << std::endl;
    RUN_TEST(Test_Vector3_ToBullet);
    RUN_TEST(Test_Vector3_FromBullet);
    RUN_TEST(Test_Vector3_RoundTrip);
    RUN_TEST(Test_Vector3_Zero);
    RUN_TEST(Test_Vector3_Negative);
    
    // Quaternion 转换测试
    std::cout << "\n--- Quaternion 转换测试 ---" << std::endl;
    RUN_TEST(Test_Quaternion_ToBullet);
    RUN_TEST(Test_Quaternion_FromBullet);
    RUN_TEST(Test_Quaternion_RoundTrip);
    RUN_TEST(Test_Quaternion_RotationConsistency);
    RUN_TEST(Test_Quaternion_MultipleRotations);
    
    // Matrix3 转换测试
    std::cout << "\n--- Matrix3 转换测试 ---" << std::endl;
    RUN_TEST(Test_Matrix3_ToBullet);
    RUN_TEST(Test_Matrix3_FromBullet);
    RUN_TEST(Test_Matrix3_RoundTrip);
    RUN_TEST(Test_Matrix3_Identity);
    RUN_TEST(Test_Matrix3_RotationFromQuaternion);
    
    // Transform 转换测试
    std::cout << "\n--- Transform 转换测试 ---" << std::endl;
    RUN_TEST(Test_Transform_ToBullet);
    RUN_TEST(Test_Transform_FromBullet);
    RUN_TEST(Test_Transform_RoundTrip);
    RUN_TEST(Test_Transform_TransformPoint);
    
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

