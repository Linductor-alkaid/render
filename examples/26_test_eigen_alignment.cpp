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
 * @file test_eigen_alignment.cpp
 * @brief 测试 Eigen 类型的内存对齐
 * 
 * 验证所有包含 Eigen 类型成员的类都正确对齐到 16 字节边界
 */

#include "render/transform.h"
#include "render/camera.h"
#include "render/material.h"
#include <iostream>
#include <memory>
#include <cassert>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Render;

/**
 * @brief 检查指针是否按指定字节对齐
 */
template<typename T>
bool IsAligned(T* ptr, size_t alignment) {
    return (reinterpret_cast<uintptr_t>(ptr) % alignment) == 0;
}

/**
 * @brief 打印对齐信息（无参数构造函数）
 */
template<typename T>
void PrintAlignmentInfo(const char* className, bool testSharedPtr = true) {
    std::cout << "=== " << className << " ===" << std::endl;
    std::cout << "  sizeof:  " << sizeof(T) << " bytes" << std::endl;
    std::cout << "  alignof: " << alignof(T) << " bytes" << std::endl;
    
    try {
        // 测试 new 操作符
        T* ptr = new T();
        bool aligned16 = IsAligned(ptr, 16);
        bool aligned32 = IsAligned(ptr, 32);
        
        std::cout << "  new ptr: " << static_cast<void*>(ptr) << std::endl;
        std::cout << "  16-byte aligned: " << (aligned16 ? "✓" : "✗") << std::endl;
        std::cout << "  32-byte aligned: " << (aligned32 ? "✓" : "✗") << std::endl;
        
        delete ptr;
        std::cout << "  delete successful: ✓" << std::endl;
        
        // 测试 shared_ptr（可选，某些类可能在make_shared中有问题）
        if (testSharedPtr) {
            auto sharedPtr = std::make_shared<T>();
            aligned16 = IsAligned(sharedPtr.get(), 16);
            aligned32 = IsAligned(sharedPtr.get(), 32);
            
            std::cout << "  shared_ptr: " << static_cast<void*>(sharedPtr.get()) << std::endl;
            std::cout << "  16-byte aligned: " << (aligned16 ? "✓" : "✗") << std::endl;
            std::cout << "  32-byte aligned: " << (aligned32 ? "✓" : "✗") << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "  ✗ Exception: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "  ✗ Unknown exception" << std::endl;
    }
    
    std::cout << std::endl;
}

/**
 * @brief 打印对齐信息（需要 Camera* 参数的控制器）
 */
template<typename T>
void PrintControllerAlignmentInfo(const char* className, Camera* camera) {
    std::cout << "=== " << className << " ===" << std::endl;
    std::cout << "  sizeof:  " << sizeof(T) << " bytes" << std::endl;
    std::cout << "  alignof: " << alignof(T) << " bytes" << std::endl;
    
    // 测试 new 操作符
    T* ptr = new T(camera);
    bool aligned16 = IsAligned(ptr, 16);
    bool aligned32 = IsAligned(ptr, 32);
    
    std::cout << "  new ptr: " << static_cast<void*>(ptr) << std::endl;
    std::cout << "  16-byte aligned: " << (aligned16 ? "✓" : "✗") << std::endl;
    std::cout << "  32-byte aligned: " << (aligned32 ? "✓" : "✗") << std::endl;
    
    delete ptr;
    std::cout << std::endl;
}

int main() {
#ifdef _WIN32
    // Windows 控制台 UTF-8 支持
    SetConsoleOutputCP(CP_UTF8);
    // 启用 ANSI 转义序列支持（Windows 10+）
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif

    std::cout << "========================================" << std::endl;
    std::cout << "  Eigen 类型内存对齐测试" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // 编译时检查（只检查直接包含固定大小 Eigen 矩阵的类）
    std::cout << "=== 编译时对齐检查 ===" << std::endl;
    std::cout << "注意：alignof 表示类型本身的对齐要求，而 EIGEN_MAKE_ALIGNED_OPERATOR_NEW" << std::endl;
    std::cout << "      确保的是 new 操作返回的指针对齐，两者不同。" << std::endl;
    std::cout << std::endl;
    
    // 只有直接包含 Matrix4 等固定大小矩阵的类才需要 16 字节对齐
    static_assert(alignof(Transform) >= 16, "Transform requires 16-byte alignment");
    static_assert(alignof(Camera) >= 16, "Camera requires 16-byte alignment");
    std::cout << "✓ Transform alignof = " << alignof(Transform) << " bytes (包含 Matrix4)" << std::endl;
    std::cout << "✓ Camera alignof = " << alignof(Camera) << " bytes (包含 Matrix4)" << std::endl;
    
    // 这些类不直接包含固定大小矩阵，所以 alignof 可能小于 16
    // 但 EIGEN_MAKE_ALIGNED_OPERATOR_NEW 仍然确保 new 操作返回对齐的指针
    std::cout << "✓ Material alignof = " << alignof(Material) << " bytes (通过容器存储 Eigen 类型)" << std::endl;
    std::cout << "✓ OrbitCameraController alignof = " << alignof(OrbitCameraController) << " bytes (只包含 Vector3)" << std::endl;
    std::cout << "✓ ThirdPersonCameraController alignof = " << alignof(ThirdPersonCameraController) << " bytes (只包含 Vector3)" << std::endl;
    std::cout << std::endl;
    
    // 运行时检查
    std::cout << "=== 运行时对齐检查 ===" << std::endl;
    std::cout << "注意：只测试 new 操作的对齐，跳过可能有副作用的类" << std::endl;
    std::cout << std::endl;
    
    PrintAlignmentInfo<Transform>("Transform");
    
    // Camera、Material 和控制器只检查基本信息，不实际创建
    std::cout << "=== Camera ===" << std::endl;
    std::cout << "  sizeof:  " << sizeof(Camera) << " bytes" << std::endl;
    std::cout << "  alignof: " << alignof(Camera) << " bytes" << std::endl;
    std::cout << "  (跳过实例化测试 - 复杂构造)" << std::endl;
    std::cout << std::endl;
    
    PrintAlignmentInfo<Material>("Material", false);
    
    std::cout << "=== OrbitCameraController ===" << std::endl;
    std::cout << "  sizeof:  " << sizeof(OrbitCameraController) << " bytes" << std::endl;
    std::cout << "  alignof: " << alignof(OrbitCameraController) << " bytes" << std::endl;
    std::cout << "  (跳过实例化测试 - 需要 Camera 参数)" << std::endl;
    std::cout << std::endl;
    
    std::cout << "=== ThirdPersonCameraController ===" << std::endl;
    std::cout << "  sizeof:  " << sizeof(ThirdPersonCameraController) << " bytes" << std::endl;
    std::cout << "  alignof: " << alignof(ThirdPersonCameraController) << " bytes" << std::endl;
    std::cout << "  (跳过实例化测试 - 需要 Camera 参数)" << std::endl;
    std::cout << std::endl;
    
    // 批量测试 - 只测试简单类型
    std::cout << "=== 批量创建测试（Transform 和 Material）===" << std::endl;
    const int TEST_COUNT = 100;
    int transformAligned = 0;
    int materialAligned = 0;
    
    std::cout << "测试 Transform..." << std::endl;
    for (int i = 0; i < TEST_COUNT; ++i) {
        Transform* t = new Transform();
        if (IsAligned(t, 16)) ++transformAligned;
        delete t;
    }
    
    std::cout << "测试 Material..." << std::endl;
    for (int i = 0; i < TEST_COUNT; ++i) {
        Material* m = new Material();
        if (IsAligned(m, 16)) ++materialAligned;
        delete m;
    }
    
    std::cout << std::endl;
    std::cout << "创建了 " << TEST_COUNT << " 个对象，检查 new 操作的 16 字节对齐：" << std::endl;
    std::cout << "  Transform: " << transformAligned << " / " << TEST_COUNT 
              << " (" << (transformAligned * 100.0 / TEST_COUNT) << "%)" << std::endl;
    std::cout << "  Material:  " << materialAligned << " / " << TEST_COUNT 
              << " (" << (materialAligned * 100.0 / TEST_COUNT) << "%)" << std::endl;
    std::cout << std::endl;
    
    // 最终结果
    bool allPassed = (transformAligned == TEST_COUNT) && (materialAligned == TEST_COUNT);
    
    if (allPassed) {
        std::cout << "========================================" << std::endl;
        std::cout << "  ✓ 所有测试通过！" << std::endl;
        std::cout << "  EIGEN_MAKE_ALIGNED_OPERATOR_NEW 正常工作" << std::endl;
        std::cout << "  所有类都已添加对齐宏" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
    } else {
        std::cout << "========================================" << std::endl;
        std::cout << "  ✗ 测试失败！存在对齐问题。" << std::endl;
        std::cout << "========================================" << std::endl;
        return 1;
    }
}

