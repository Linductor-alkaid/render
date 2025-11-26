/**
 * @file test_transform_error_handling.cpp
 * @brief Transform 统一错误处理单元测试（CTest 兼容版本）
 * 
 * 测试 Transform 类与项目错误处理系统的集成
 */

#include "render/transform.h"
#include "render/error.h"
#include <iostream>
#include <limits>
#include <memory>
#include <cassert>
#include <cmath>

using namespace Render;

// ============================================================================
// 简单的测试框架（CTest 兼容）
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
// Result 类型测试
// ============================================================================

bool Test_ResultType_Success() {
    auto success = Transform::Result::Success();
    TEST_ASSERT(success.Ok(), "Success result should be Ok");
    TEST_ASSERT(!success.Failed(), "Success result should not be Failed");
    TEST_ASSERT(success.code == ErrorCode::Success, "Success code should be Success");
    TEST_ASSERT(success, "Success should convert to true");
    return true;
}

bool Test_ResultType_Failure() {
    auto failure = Transform::Result::Failure(
        ErrorCode::TransformInvalidPosition, 
        "测试错误消息"
    );
    TEST_ASSERT(!failure.Ok(), "Failure result should not be Ok");
    TEST_ASSERT(failure.Failed(), "Failure result should be Failed");
    TEST_ASSERT(failure.code == ErrorCode::TransformInvalidPosition, 
                "Failure code should match");
    TEST_ASSERT(!failure, "Failure should convert to false");
    TEST_ASSERT(failure.message == "测试错误消息", "Message should match");
    return true;
}

// ============================================================================
// TrySetPosition 测试
// ============================================================================

bool Test_TrySetPosition_ValidInput() {
    Transform transform;
    auto result = transform.TrySetPosition(Vector3(1.0f, 2.0f, 3.0f));
    
    TEST_ASSERT(result.Ok(), "Valid position should succeed");
    TEST_ASSERT(result.code == ErrorCode::Success, "Code should be Success");
    
    Vector3 pos = transform.GetPosition();
    TEST_ASSERT(std::abs(pos.x() - 1.0f) < 1e-5f, "Position X should match");
    TEST_ASSERT(std::abs(pos.y() - 2.0f) < 1e-5f, "Position Y should match");
    TEST_ASSERT(std::abs(pos.z() - 3.0f) < 1e-5f, "Position Z should match");
    return true;
}

bool Test_TrySetPosition_NaN() {
    Transform transform;
    auto result = transform.TrySetPosition(Vector3(
        std::numeric_limits<float>::quiet_NaN(), 
        0.0f, 
        0.0f
    ));
    
    TEST_ASSERT(!result.Ok(), "NaN position should fail");
    TEST_ASSERT(result.code == ErrorCode::TransformInvalidPosition, 
                "Code should be TransformInvalidPosition");
    TEST_ASSERT(!result.message.empty(), "Error message should not be empty");
    return true;
}

bool Test_TrySetPosition_Infinity() {
    Transform transform;
    auto result = transform.TrySetPosition(Vector3(
        0.0f, 
        std::numeric_limits<float>::infinity(), 
        0.0f
    ));
    
    TEST_ASSERT(!result.Ok(), "Infinity position should fail");
    TEST_ASSERT(result.code == ErrorCode::TransformInvalidPosition,
                "Code should be TransformInvalidPosition");
    return true;
}

// ============================================================================
// TrySetRotation 测试
// ============================================================================

bool Test_TrySetRotation_ValidInput() {
    Transform transform;
    auto result = transform.TrySetRotation(Quaternion::Identity());
    
    TEST_ASSERT(result.Ok(), "Valid rotation should succeed");
    TEST_ASSERT(result.code == ErrorCode::Success, "Code should be Success");
    return true;
}

bool Test_TrySetRotation_ZeroQuaternion() {
    Transform transform;
    auto result = transform.TrySetRotation(Quaternion(0, 0, 0, 0));
    
    TEST_ASSERT(!result.Ok(), "Zero quaternion should fail");
    TEST_ASSERT(result.code == ErrorCode::TransformInvalidRotation,
                "Code should be TransformInvalidRotation");
    TEST_ASSERT(!result.message.empty(), "Error message should not be empty");
    return true;
}

bool Test_TrySetRotation_NaN() {
    Transform transform;
    auto result = transform.TrySetRotation(Quaternion(
        std::numeric_limits<float>::quiet_NaN(),
        0.0f, 0.0f, 0.0f
    ));
    
    TEST_ASSERT(!result.Ok(), "NaN rotation should fail");
    TEST_ASSERT(result.code == ErrorCode::TransformInvalidRotation,
                "Code should be TransformInvalidRotation");
    return true;
}

// ============================================================================
// TrySetScale 测试
// ============================================================================

bool Test_TrySetScale_ValidInput() {
    Transform transform;
    auto result = transform.TrySetScale(Vector3(2.0f, 2.0f, 2.0f));
    
    TEST_ASSERT(result.Ok(), "Valid scale should succeed");
    TEST_ASSERT(result.code == ErrorCode::Success, "Code should be Success");
    
    Vector3 scale = transform.GetScale();
    TEST_ASSERT(std::abs(scale.x() - 2.0f) < 1e-5f, "Scale X should match");
    TEST_ASSERT(std::abs(scale.y() - 2.0f) < 1e-5f, "Scale Y should match");
    TEST_ASSERT(std::abs(scale.z() - 2.0f) < 1e-5f, "Scale Z should match");
    return true;
}

bool Test_TrySetScale_TooSmall() {
    Transform transform;
    auto result = transform.TrySetScale(Vector3(1e-10f, 1.0f, 1.0f));
    
    TEST_ASSERT(!result.Ok(), "Too small scale should fail");
    TEST_ASSERT(result.code == ErrorCode::TransformInvalidScale,
                "Code should be TransformInvalidScale");
    TEST_ASSERT(!result.message.empty(), "Error message should not be empty");
    return true;
}

bool Test_TrySetScale_TooLarge() {
    Transform transform;
    auto result = transform.TrySetScale(Vector3(1e10f, 1.0f, 1.0f));
    
    TEST_ASSERT(!result.Ok(), "Too large scale should fail");
    TEST_ASSERT(result.code == ErrorCode::TransformInvalidScale,
                "Code should be TransformInvalidScale");
    return true;
}

bool Test_TrySetScale_NaN() {
    Transform transform;
    auto result = transform.TrySetScale(Vector3(
        std::numeric_limits<float>::quiet_NaN(),
        1.0f, 1.0f
    ));
    
    TEST_ASSERT(!result.Ok(), "NaN scale should fail");
    TEST_ASSERT(result.code == ErrorCode::TransformInvalidScale,
                "Code should be TransformInvalidScale");
    return true;
}

// ============================================================================
// TrySetParent 测试
// ============================================================================

bool Test_TrySetParent_ValidInput() {
    Transform parent, child;
    auto result = child.TrySetParent(&parent);
    
    TEST_ASSERT(result.Ok(), "Valid parent should succeed");
    TEST_ASSERT(result.code == ErrorCode::Success, "Code should be Success");
    TEST_ASSERT(child.GetParent() == &parent, "Parent should be set correctly");
    return true;
}

bool Test_TrySetParent_SelfReference() {
    Transform transform;
    auto result = transform.TrySetParent(&transform);
    
    TEST_ASSERT(!result.Ok(), "Self reference should fail");
    TEST_ASSERT(result.code == ErrorCode::TransformSelfReference,
                "Code should be TransformSelfReference");
    TEST_ASSERT(!result.message.empty(), "Error message should not be empty");
    return true;
}

bool Test_TrySetParent_CircularReference() {
    Transform a, b, c;
    
    // 创建链: a -> b -> c
    auto r1 = b.TrySetParent(&a);
    TEST_ASSERT(r1.Ok(), "First parent set should succeed");
    
    auto r2 = c.TrySetParent(&b);
    TEST_ASSERT(r2.Ok(), "Second parent set should succeed");
    
    // 尝试创建循环: c -> b -> a -> c
    auto result = a.TrySetParent(&c);
    
    TEST_ASSERT(!result.Ok(), "Circular reference should fail");
    TEST_ASSERT(result.code == ErrorCode::TransformCircularReference,
                "Code should be TransformCircularReference");
    TEST_ASSERT(!result.message.empty(), "Error message should not be empty");
    TEST_ASSERT(a.GetParent() == nullptr, "Parent should not be changed");
    return true;
}

bool Test_TrySetParent_Nullptr() {
    Transform parent, child;
    
    // 先设置父对象
    auto r1 = child.TrySetParent(&parent);
    TEST_ASSERT(r1.Ok(), "Setting parent should succeed");
    TEST_ASSERT(child.GetParent() == &parent, "Parent should be set");
    
    // 清除父对象
    auto result = child.TrySetParent(nullptr);
    
    TEST_ASSERT(result.Ok(), "Clearing parent should succeed");
    TEST_ASSERT(child.GetParent() == nullptr, "Parent should be null");
    return true;
}

bool Test_TrySetParent_SameParent() {
    Transform parent, child;
    
    // 设置父对象
    auto r1 = child.TrySetParent(&parent);
    TEST_ASSERT(r1.Ok(), "First set should succeed");
    
    // 再次设置相同的父对象
    auto result = child.TrySetParent(&parent);
    
    TEST_ASSERT(result.Ok(), "Setting same parent should succeed");
    TEST_ASSERT(result.code == ErrorCode::Success, "Code should be Success");
    return true;
}

// ============================================================================
// TrySetFromMatrix 测试
// ============================================================================

bool Test_TrySetFromMatrix_ValidInput() {
    Transform transform;
    
    // 创建简单的变换矩阵
    Matrix4 matrix = Matrix4::Identity();
    matrix(0, 3) = 1.0f;  // X 平移
    matrix(1, 3) = 2.0f;  // Y 平移
    matrix(2, 3) = 3.0f;  // Z 平移
    
    auto result = transform.TrySetFromMatrix(matrix);
    
    TEST_ASSERT(result.Ok(), "Valid matrix should succeed");
    TEST_ASSERT(result.code == ErrorCode::Success, "Code should be Success");
    
    // 验证位置
    Vector3 pos = transform.GetPosition();
    TEST_ASSERT(std::abs(pos.x() - 1.0f) < 1e-4f, "Position X should match");
    TEST_ASSERT(std::abs(pos.y() - 2.0f) < 1e-4f, "Position Y should match");
    TEST_ASSERT(std::abs(pos.z() - 3.0f) < 1e-4f, "Position Z should match");
    return true;
}

bool Test_TrySetFromMatrix_NaN() {
    Transform transform;
    
    Matrix4 matrix = Matrix4::Identity();
    matrix(0, 0) = std::numeric_limits<float>::quiet_NaN();
    
    auto result = transform.TrySetFromMatrix(matrix);
    
    TEST_ASSERT(!result.Ok(), "NaN matrix should fail");
    TEST_ASSERT(result.code == ErrorCode::TransformInvalidMatrix,
                "Code should be TransformInvalidMatrix");
    TEST_ASSERT(!result.message.empty(), "Error message should not be empty");
    return true;
}

// ============================================================================
// 向后兼容性测试
// ============================================================================

bool Test_BackwardCompatibility_SetMethods() {
    Transform transform;
    
    // 旧的 Set* 方法应该仍然工作
    transform.SetPosition(Vector3(1, 2, 3));
    Vector3 pos = transform.GetPosition();
    TEST_ASSERT(std::abs(pos.x() - 1.0f) < 1e-5f, "SetPosition should work");
    TEST_ASSERT(std::abs(pos.y() - 2.0f) < 1e-5f, "SetPosition should work");
    TEST_ASSERT(std::abs(pos.z() - 3.0f) < 1e-5f, "SetPosition should work");
    
    transform.SetRotation(Quaternion::Identity());
    Quaternion rot = transform.GetRotation();
    TEST_ASSERT(std::abs(rot.w() - 1.0f) < 1e-5f, "SetRotation should work");
    
    transform.SetScale(Vector3(2, 2, 2));
    Vector3 scale = transform.GetScale();
    TEST_ASSERT(std::abs(scale.x() - 2.0f) < 1e-5f, "SetScale should work");
    return true;
}

bool Test_BackwardCompatibility_SetParent() {
    Transform parent, child;
    
    // 旧的 SetParent 方法应该仍然返回 bool
    bool success = child.SetParent(&parent);
    
    TEST_ASSERT(success, "SetParent should succeed");
    TEST_ASSERT(child.GetParent() == &parent, "Parent should be set");
    return true;
}

// ============================================================================
// 错误消息质量测试
// ============================================================================

bool Test_ErrorMessageQuality_Scale() {
    Transform transform;
    
    auto result = transform.TrySetScale(Vector3(1e-10f, 1, 1));
    
    TEST_ASSERT(!result.Ok(), "Should fail");
    TEST_ASSERT(!result.message.empty(), "Error message should not be empty");
    
    // 错误消息应该包含有用的信息
    bool hasUsefulInfo = 
        result.message.find("小") != std::string::npos ||
        result.message.find("MIN") != std::string::npos ||
        result.message.find("<") != std::string::npos;
    TEST_ASSERT(hasUsefulInfo, "Error message should contain useful info");
    return true;
}

bool Test_ErrorMessageQuality_CircularReference() {
    Transform a, b, c;
    
    b.TrySetParent(&a);
    c.TrySetParent(&b);
    
    auto result = a.TrySetParent(&c);
    
    TEST_ASSERT(!result.Ok(), "Should fail");
    TEST_ASSERT(!result.message.empty(), "Error message should not be empty");
    
    // 错误消息应该提到循环或深度
    bool hasUsefulInfo =
        result.message.find("循环") != std::string::npos ||
        result.message.find("深度") != std::string::npos;
    TEST_ASSERT(hasUsefulInfo, "Error message should mention circular or depth");
    return true;
}

// ============================================================================
// 主函数
// ============================================================================

int main(int argc, char** argv) {
    std::cout << "========================================" << std::endl;
    std::cout << "Transform 错误处理测试开始" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 设置错误处理器
    ErrorHandler::GetInstance().SetEnabled(true);
    ErrorHandler::GetInstance().ResetStats();
    
    // Result 类型测试
    std::cout << "\n[Result 类型测试]" << std::endl;
    RUN_TEST(Test_ResultType_Success);
    RUN_TEST(Test_ResultType_Failure);
    
    // TrySetPosition 测试
    std::cout << "\n[TrySetPosition 测试]" << std::endl;
    RUN_TEST(Test_TrySetPosition_ValidInput);
    RUN_TEST(Test_TrySetPosition_NaN);
    RUN_TEST(Test_TrySetPosition_Infinity);
    
    // TrySetRotation 测试
    std::cout << "\n[TrySetRotation 测试]" << std::endl;
    RUN_TEST(Test_TrySetRotation_ValidInput);
    RUN_TEST(Test_TrySetRotation_ZeroQuaternion);
    RUN_TEST(Test_TrySetRotation_NaN);
    
    // TrySetScale 测试
    std::cout << "\n[TrySetScale 测试]" << std::endl;
    RUN_TEST(Test_TrySetScale_ValidInput);
    RUN_TEST(Test_TrySetScale_TooSmall);
    RUN_TEST(Test_TrySetScale_TooLarge);
    RUN_TEST(Test_TrySetScale_NaN);
    
    // TrySetParent 测试
    std::cout << "\n[TrySetParent 测试]" << std::endl;
    RUN_TEST(Test_TrySetParent_ValidInput);
    RUN_TEST(Test_TrySetParent_SelfReference);
    RUN_TEST(Test_TrySetParent_CircularReference);
    RUN_TEST(Test_TrySetParent_Nullptr);
    RUN_TEST(Test_TrySetParent_SameParent);
    
    // TrySetFromMatrix 测试
    std::cout << "\n[TrySetFromMatrix 测试]" << std::endl;
    RUN_TEST(Test_TrySetFromMatrix_ValidInput);
    RUN_TEST(Test_TrySetFromMatrix_NaN);
    
    // 向后兼容性测试
    std::cout << "\n[向后兼容性测试]" << std::endl;
    RUN_TEST(Test_BackwardCompatibility_SetMethods);
    RUN_TEST(Test_BackwardCompatibility_SetParent);
    
    // 错误消息质量测试
    std::cout << "\n[错误消息质量测试]" << std::endl;
    RUN_TEST(Test_ErrorMessageQuality_Scale);
    RUN_TEST(Test_ErrorMessageQuality_CircularReference);
    
    // 打印统计
    std::cout << "\n========================================" << std::endl;
    std::cout << "测试完成" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "总测试数: " << g_testCount << std::endl;
    std::cout << "通过: " << g_passedCount << " ✓" << std::endl;
    std::cout << "失败: " << g_failedCount << " ✗" << std::endl;
    
    if (g_failedCount > 0) {
        std::cout << "\n❌ 部分测试失败" << std::endl;
        return 1;  // CTest 会识别非零返回码为失败
    }
    
    std::cout << "\n✅ 所有测试通过" << std::endl;
    return 0;  // CTest 识别返回 0 为成功
}
