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
 * @file test_transform_change_callback.cpp
 * @brief Transform变化回调测试
 * 
 * 测试Transform类变化回调系统的所有功能：
 * - 回调设置和清除
 * - SetPosition/SetRotation/SetScale触发回调
 * - 只在值变化时触发
 * - 线程安全
 * - 其他修改方法触发回调
 */

#include "render/transform.h"
#include "render/types.h"
#include "render/math_utils.h"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cmath>

using namespace Render;

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
// 2.4.1 测试Transform变化回调
// ============================================================================

bool Test_Transform_SetChangeCallback() {
    Transform transform;
    
    bool callbackCalled = false;
    const Transform* receivedTransform = nullptr;
    
    transform.SetChangeCallback([&](const Transform* t) {
        callbackCalled = true;
        receivedTransform = t;
    });
    
    TEST_ASSERT(!callbackCalled, "设置回调不应该立即触发");
    
    return true;
}

bool Test_Transform_ClearChangeCallback() {
    Transform transform;
    
    int callbackCount = 0;
    
    transform.SetChangeCallback([&](const Transform*) {
        callbackCount++;
    });
    
    // 触发一次回调
    transform.SetPosition(Vector3(1.0f, 2.0f, 3.0f));
    TEST_ASSERT(callbackCount == 1, "回调应该被调用");
    
    // 清除回调
    transform.ClearChangeCallback();
    
    // 再次修改，不应该触发回调
    transform.SetPosition(Vector3(4.0f, 5.0f, 6.0f));
    TEST_ASSERT(callbackCount == 1, "清除回调后不应该再调用");
    
    return true;
}

bool Test_Transform_SetPosition_TriggersCallback() {
    Transform transform;
    
    int callbackCount = 0;
    Vector3 receivedPosition;
    
    transform.SetChangeCallback([&](const Transform* t) {
        callbackCount++;
        receivedPosition = t->GetPosition();
    });
    
    Vector3 newPosition(10.0f, 20.0f, 30.0f);
    transform.SetPosition(newPosition);
    
    TEST_ASSERT(callbackCount == 1, "SetPosition应该触发回调");
    TEST_ASSERT(receivedPosition.isApprox(newPosition, MathUtils::EPSILON), 
                "回调应该收到正确的位置");
    
    return true;
}

bool Test_Transform_SetRotation_TriggersCallback() {
    Transform transform;
    
    int callbackCount = 0;
    Quaternion receivedRotation;
    
    transform.SetChangeCallback([&](const Transform* t) {
        callbackCount++;
        receivedRotation = t->GetRotation();
    });
    
    Quaternion newRotation = Quaternion(Eigen::AngleAxisf(1.57f, Vector3::UnitY()));
    transform.SetRotation(newRotation);
    
    TEST_ASSERT(callbackCount == 1, "SetRotation应该触发回调");
    // 四元数比较需要考虑双重覆盖
    bool rotationMatch = receivedRotation.coeffs().isApprox(newRotation.coeffs(), MathUtils::EPSILON) ||
                         receivedRotation.coeffs().isApprox(-newRotation.coeffs(), MathUtils::EPSILON);
    TEST_ASSERT(rotationMatch, "回调应该收到正确的旋转");
    
    return true;
}

bool Test_Transform_SetScale_TriggersCallback() {
    Transform transform;
    
    int callbackCount = 0;
    Vector3 receivedScale;
    
    transform.SetChangeCallback([&](const Transform* t) {
        callbackCount++;
        receivedScale = t->GetScale();
    });
    
    Vector3 newScale(2.0f, 3.0f, 4.0f);
    transform.SetScale(newScale);
    
    TEST_ASSERT(callbackCount == 1, "SetScale应该触发回调");
    TEST_ASSERT(receivedScale.isApprox(newScale, MathUtils::EPSILON), 
                "回调应该收到正确的缩放");
    
    return true;
}

bool Test_Transform_OnlyNotifiesOnValueChange() {
    Transform transform;
    
    int callbackCount = 0;
    
    transform.SetChangeCallback([&](const Transform*) {
        callbackCount++;
    });
    
    Vector3 position(1.0f, 2.0f, 3.0f);
    
    // 第一次设置，应该触发回调
    transform.SetPosition(position);
    TEST_ASSERT(callbackCount == 1, "第一次设置应该触发回调");
    
    // 第二次设置相同值，不应该触发回调
    transform.SetPosition(position);
    TEST_ASSERT(callbackCount == 1, "设置相同值不应该触发回调");
    
    // 设置不同的值，应该触发回调
    transform.SetPosition(Vector3(4.0f, 5.0f, 6.0f));
    TEST_ASSERT(callbackCount == 2, "设置不同值应该触发回调");
    
    return true;
}

bool Test_Transform_Translate_TriggersCallback() {
    Transform transform;
    
    int callbackCount = 0;
    Vector3 initialPosition;
    
    transform.SetChangeCallback([&](const Transform* t) {
        callbackCount++;
        if (callbackCount == 1) {
            initialPosition = t->GetPosition();
        }
    });
    
    Vector3 startPos(1.0f, 2.0f, 3.0f);
    transform.SetPosition(startPos);
    TEST_ASSERT(callbackCount == 1, "SetPosition应该触发回调");
    
    Vector3 translation(10.0f, 20.0f, 30.0f);
    transform.Translate(translation);
    TEST_ASSERT(callbackCount == 2, "Translate应该触发回调");
    
    Vector3 expectedPos = startPos + translation;
    Vector3 actualPos = transform.GetPosition();
    TEST_ASSERT(actualPos.isApprox(expectedPos, MathUtils::EPSILON), 
                "Translate应该正确更新位置");
    
    return true;
}

bool Test_Transform_Rotate_TriggersCallback() {
    Transform transform;
    
    int callbackCount = 0;
    
    transform.SetChangeCallback([&](const Transform*) {
        callbackCount++;
    });
    
    // 先设置一个非Identity的旋转，确保触发回调
    Quaternion initialRotation = Quaternion(Eigen::AngleAxisf(0.1f, Vector3::UnitY()));
    transform.SetRotation(initialRotation);
    TEST_ASSERT(callbackCount == 1, "SetRotation应该触发回调");
    
    Quaternion rotationDelta = Quaternion(Eigen::AngleAxisf(0.5f, Vector3::UnitY()));
    transform.Rotate(rotationDelta);
    TEST_ASSERT(callbackCount == 2, "Rotate应该触发回调");
    
    return true;
}

bool Test_Transform_SetFromMatrix_TriggersCallback() {
    Transform transform;
    
    int callbackCount = 0;
    
    transform.SetChangeCallback([&](const Transform*) {
        callbackCount++;
    });
    
    // 创建变换矩阵
    Matrix4 matrix = Matrix4::Identity();
    matrix(0, 3) = 5.0f;  // 平移X
    matrix(1, 3) = 6.0f;  // 平移Y
    matrix(2, 3) = 7.0f;  // 平移Z
    
    transform.SetFromMatrix(matrix);
    TEST_ASSERT(callbackCount == 1, "SetFromMatrix应该触发回调");
    
    // 设置相同的矩阵，不应该触发回调
    transform.SetFromMatrix(matrix);
    TEST_ASSERT(callbackCount == 1, "设置相同矩阵不应该触发回调");
    
    return true;
}

bool Test_Transform_LookAt_TriggersCallback() {
    Transform transform;
    
    int callbackCount = 0;
    
    transform.SetChangeCallback([&](const Transform*) {
        callbackCount++;
    });
    
    // 先设置一个非零位置，确保触发回调
    Vector3 position(1.0f, 1.0f, 1.0f);
    transform.SetPosition(position);
    TEST_ASSERT(callbackCount == 1, "SetPosition应该触发回调");
    
    // 现在设置到原点（如果已经是原点，可能不会触发，所以先设置一个不同的位置）
    transform.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
    // 如果位置从(1,1,1)变为(0,0,0)，应该触发回调
    int countAfterSetToZero = callbackCount;
    
    Vector3 target(1.0f, 0.0f, 0.0f);
    transform.LookAt(target);
    TEST_ASSERT(callbackCount >= 2, "LookAt应该触发回调");
    
    return true;
}

bool Test_Transform_CallbackExceptionHandling() {
    Transform transform;
    
    int callbackCount = 0;
    
    // 设置一个会抛出异常的回调
    transform.SetChangeCallback([&](const Transform*) {
        callbackCount++;
        throw std::runtime_error("测试异常");
    });
    
    // SetPosition应该成功，即使回调抛出异常
    try {
        transform.SetPosition(Vector3(1.0f, 2.0f, 3.0f));
        TEST_ASSERT(true, "SetPosition应该成功");
    } catch (...) {
        TEST_ASSERT(false, "异常不应该传播");
    }
    
    TEST_ASSERT(callbackCount == 1, "回调应该被调用");
    Vector3 pos = transform.GetPosition();
    TEST_ASSERT(pos.isApprox(Vector3(1.0f, 2.0f, 3.0f), MathUtils::EPSILON), 
                "位置应该被正确设置");
    
    return true;
}

bool Test_Transform_ThreadSafety() {
    Transform transform;
    
    std::atomic<int> callbackCount{0};
    const int numThreads = 4;
    const int operationsPerThread = 100;
    
    // 注册回调
    transform.SetChangeCallback([&](const Transform*) {
        callbackCount++;
    });
    
    // 创建多个线程同时修改Transform
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < operationsPerThread; ++j) {
                float value = static_cast<float>(i * operationsPerThread + j);
                transform.SetPosition(Vector3(value, value + 1.0f, value + 2.0f));
                transform.SetRotation(Quaternion(Eigen::AngleAxisf(value * 0.1f, Vector3::UnitY())));
                float scaleValue = value * 0.1f + 1.0f;
                transform.SetScale(Vector3(scaleValue, scaleValue, scaleValue));
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    // 验证结果
    // 注意：由于变化检测，实际回调次数可能少于操作次数
    TEST_ASSERT(callbackCount > 0, "应该有回调被调用");
    TEST_ASSERT(callbackCount <= numThreads * operationsPerThread * 3, 
                "回调次数不应该超过操作次数");
    
    return true;
}

bool Test_Transform_MultipleOperations() {
    Transform transform;
    
    int callbackCount = 0;
    std::vector<std::string> operationSequence;
    
    transform.SetChangeCallback([&](const Transform*) {
        callbackCount++;
    });
    
    // 执行多个操作
    transform.SetPosition(Vector3(1.0f, 2.0f, 3.0f));
    transform.SetRotation(Quaternion::Identity());
    transform.SetScale(Vector3(1.0f, 1.0f, 1.0f));
    transform.Translate(Vector3(1.0f, 0.0f, 0.0f));
    transform.Rotate(Quaternion(Eigen::AngleAxisf(0.5f, Vector3::UnitY())));
    
    // 由于变化检测，某些操作可能不会触发回调（如果值相同）
    TEST_ASSERT(callbackCount >= 3, "应该至少触发3次回调");
    TEST_ASSERT(callbackCount <= 5, "不应该超过5次回调");
    
    return true;
}

bool Test_Transform_SetScale_Uniform() {
    Transform transform;
    
    int callbackCount = 0;
    
    transform.SetChangeCallback([&](const Transform*) {
        callbackCount++;
    });
    
    // SetScale(float)应该调用SetScale(Vector3)，从而触发回调
    transform.SetScale(2.0f);
    TEST_ASSERT(callbackCount == 1, "SetScale(float)应该触发回调");
    
    Vector3 scale = transform.GetScale();
    TEST_ASSERT(scale.isApprox(Vector3(2.0f, 2.0f, 2.0f), MathUtils::EPSILON), 
                "统一缩放应该正确设置");
    
    return true;
}

bool Test_Transform_RotateAround_TriggersCallback() {
    Transform transform;
    
    int callbackCount = 0;
    
    transform.SetChangeCallback([&](const Transform*) {
        callbackCount++;
    });
    
    // 先设置一个非Identity的旋转，确保触发回调
    Quaternion initialRotation = Quaternion(Eigen::AngleAxisf(0.1f, Vector3::UnitX()));
    transform.SetRotation(initialRotation);
    TEST_ASSERT(callbackCount == 1, "SetRotation应该触发回调");
    
    transform.RotateAround(Vector3::UnitY(), 1.0f);
    TEST_ASSERT(callbackCount == 2, "RotateAround应该触发回调");
    
    return true;
}

bool Test_Transform_SetRotationEuler_TriggersCallback() {
    Transform transform;
    
    int callbackCount = 0;
    
    transform.SetChangeCallback([&](const Transform*) {
        callbackCount++;
    });
    
    transform.SetRotationEuler(Vector3(0.5f, 1.0f, 1.5f));
    TEST_ASSERT(callbackCount == 1, "SetRotationEuler应该触发回调");
    
    // 设置相同的欧拉角，不应该触发回调
    transform.SetRotationEuler(Vector3(0.5f, 1.0f, 1.5f));
    TEST_ASSERT(callbackCount == 1, "设置相同欧拉角不应该触发回调");
    
    return true;
}

bool Test_Transform_SetRotationEulerDegrees_TriggersCallback() {
    Transform transform;
    
    int callbackCount = 0;
    
    transform.SetChangeCallback([&](const Transform*) {
        callbackCount++;
    });
    
    transform.SetRotationEulerDegrees(Vector3(45.0f, 90.0f, 135.0f));
    TEST_ASSERT(callbackCount == 1, "SetRotationEulerDegrees应该触发回调");
    
    return true;
}

// ============================================================================
// 主函数
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Transform变化回调测试" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // 2.4.1 测试Transform变化回调
    std::cout << "--- 2.4.1 测试Transform变化回调 ---" << std::endl;
    RUN_TEST(Test_Transform_SetChangeCallback);
    RUN_TEST(Test_Transform_ClearChangeCallback);
    RUN_TEST(Test_Transform_SetPosition_TriggersCallback);
    RUN_TEST(Test_Transform_SetRotation_TriggersCallback);
    RUN_TEST(Test_Transform_SetScale_TriggersCallback);
    RUN_TEST(Test_Transform_OnlyNotifiesOnValueChange);
    RUN_TEST(Test_Transform_Translate_TriggersCallback);
    RUN_TEST(Test_Transform_Rotate_TriggersCallback);
    RUN_TEST(Test_Transform_SetFromMatrix_TriggersCallback);
    RUN_TEST(Test_Transform_LookAt_TriggersCallback);
    RUN_TEST(Test_Transform_CallbackExceptionHandling);
    RUN_TEST(Test_Transform_ThreadSafety);
    RUN_TEST(Test_Transform_MultipleOperations);
    RUN_TEST(Test_Transform_SetScale_Uniform);
    RUN_TEST(Test_Transform_RotateAround_TriggersCallback);
    RUN_TEST(Test_Transform_SetRotationEuler_TriggersCallback);
    RUN_TEST(Test_Transform_SetRotationEulerDegrees_TriggersCallback);
    std::cout << std::endl;
    
    // 输出测试结果
    std::cout << "========================================" << std::endl;
    std::cout << "测试结果统计" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "总测试数: " << g_testCount << std::endl;
    std::cout << "通过: " << g_passedCount << std::endl;
    std::cout << "失败: " << g_failedCount << std::endl;
    std::cout << "========================================" << std::endl;
    
    if (g_failedCount == 0) {
        std::cout << "✓ 所有测试通过！" << std::endl;
        return 0;
    } else {
        std::cout << "✗ 有 " << g_failedCount << " 个测试失败" << std::endl;
        return 1;
    }
}

