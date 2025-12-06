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
 * @file test_physics_math.cpp
 * @brief 物理引擎数学类型测试
 * 
 * 测试 AABB, OBB, Ray 等物理数学类型
 */

#include "render/types.h"
#include "render/math_utils.h"
#include "render/ecs/entity.h"
#include <iostream>
#include <cmath>
#include <cassert>

using namespace Render;

// ============================================================================
// 简单的测试框架
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
// AABB 测试
// ============================================================================

bool Test_AABB_Creation() {
    AABB aabb(Vector3(0, 0, 0), Vector3(1, 1, 1));
    
    TEST_ASSERT(aabb.min == Vector3(0, 0, 0), "AABB min 应该正确");
    TEST_ASSERT(aabb.max == Vector3(1, 1, 1), "AABB max 应该正确");
    
    return true;
}

bool Test_AABB_GetCenter() {
    AABB aabb(Vector3(-1, -1, -1), Vector3(1, 1, 1));
    Vector3 center = aabb.GetCenter();
    
    TEST_ASSERT(center.isApprox(Vector3(0, 0, 0)), "AABB 中心应该是 (0,0,0)");
    
    return true;
}

bool Test_AABB_GetExtents() {
    AABB aabb(Vector3(-2, -3, -4), Vector3(2, 3, 4));
    Vector3 extents = aabb.GetExtents();
    
    TEST_ASSERT(extents.isApprox(Vector3(2, 3, 4)), "AABB 半尺寸应该正确");
    
    return true;
}

bool Test_AABB_Contains() {
    AABB aabb(Vector3(0, 0, 0), Vector3(10, 10, 10));
    
    TEST_ASSERT(aabb.Contains(Vector3(5, 5, 5)), "应该包含内部点");
    TEST_ASSERT(aabb.Contains(Vector3(0, 0, 0)), "应该包含边界点");
    TEST_ASSERT(!aabb.Contains(Vector3(11, 5, 5)), "不应该包含外部点");
    
    return true;
}

bool Test_AABB_Intersects() {
    AABB aabb1(Vector3(0, 0, 0), Vector3(5, 5, 5));
    AABB aabb2(Vector3(3, 3, 3), Vector3(8, 8, 8));
    AABB aabb3(Vector3(10, 10, 10), Vector3(15, 15, 15));
    
    TEST_ASSERT(aabb1.Intersects(aabb2), "重叠的 AABB 应该相交");
    TEST_ASSERT(!aabb1.Intersects(aabb3), "分离的 AABB 不应该相交");
    
    return true;
}

bool Test_AABB_Merge() {
    AABB aabb1(Vector3(0, 0, 0), Vector3(5, 5, 5));
    AABB aabb2(Vector3(3, 3, 3), Vector3(8, 8, 8));
    
    aabb1.Merge(aabb2);
    
    TEST_ASSERT(aabb1.min.isApprox(Vector3(0, 0, 0)), "合并后 min 应该正确");
    TEST_ASSERT(aabb1.max.isApprox(Vector3(8, 8, 8)), "合并后 max 应该正确");
    
    return true;
}

bool Test_AABB_Expand() {
    AABB aabb(Vector3(0, 0, 0), Vector3(5, 5, 5));
    aabb.Expand(Vector3(10, 2, 2));
    
    TEST_ASSERT(aabb.max.x() == 10.0f, "扩展后应该包含新点");
    TEST_ASSERT(aabb.max.y() == 5.0f, "未扩展的维度应该保持");
    
    return true;
}

bool Test_AABB_GetSurfaceArea() {
    AABB aabb(Vector3(0, 0, 0), Vector3(2, 3, 4));
    float area = aabb.GetSurfaceArea();
    
    // 表面积 = 2 * (2*3 + 3*4 + 4*2) = 2 * (6 + 12 + 8) = 52
    TEST_ASSERT(std::abs(area - 52.0f) < 0.001f, "表面积计算应该正确");
    
    return true;
}

// ============================================================================
// OBB 测试
// ============================================================================

bool Test_OBB_Creation() {
    OBB obb(Vector3(0, 0, 0), Vector3(1, 1, 1), Quaternion::Identity());
    
    TEST_ASSERT(obb.center.isApprox(Vector3(0, 0, 0)), "OBB 中心应该正确");
    TEST_ASSERT(obb.halfExtents.isApprox(Vector3(1, 1, 1)), "OBB 半尺寸应该正确");
    
    return true;
}

bool Test_OBB_FromAABB() {
    AABB aabb(Vector3(-2, -2, -2), Vector3(2, 2, 2));
    OBB obb = OBB::FromAABB(aabb);
    
    TEST_ASSERT(obb.center.isApprox(Vector3(0, 0, 0)), "从 AABB 创建的 OBB 中心应该正确");
    TEST_ASSERT(obb.halfExtents.isApprox(Vector3(2, 2, 2)), "从 AABB 创建的 OBB 半尺寸应该正确");
    
    return true;
}

bool Test_OBB_GetAABB() {
    OBB obb(Vector3(0, 0, 0), Vector3(1, 1, 1), Quaternion::Identity());
    AABB aabb = obb.GetAABB();
    
    TEST_ASSERT(aabb.min.isApprox(Vector3(-1, -1, -1)), "OBB 转 AABB 最小值应该正确");
    TEST_ASSERT(aabb.max.isApprox(Vector3(1, 1, 1)), "OBB 转 AABB 最大值应该正确");
    
    return true;
}

bool Test_OBB_GetVertices() {
    OBB obb(Vector3(0, 0, 0), Vector3(1, 1, 1), Quaternion::Identity());
    Vector3 vertices[8];
    obb.GetVertices(vertices);
    
    // 验证顶点数量
    int validVertices = 0;
    for (int i = 0; i < 8; i++) {
        if (vertices[i].norm() > 0) validVertices++;
    }
    TEST_ASSERT(validVertices == 8, "应该有 8 个有效顶点");
    
    return true;
}

// ============================================================================
// Ray 测试
// ============================================================================

bool Test_Ray_Creation() {
    Ray ray(Vector3(0, 0, 0), Vector3(1, 0, 0));
    
    TEST_ASSERT(ray.origin.isApprox(Vector3(0, 0, 0)), "射线起点应该正确");
    TEST_ASSERT(ray.direction.isApprox(Vector3(1, 0, 0)), "射线方向应该正确");
    
    return true;
}

bool Test_Ray_GetPoint() {
    Ray ray(Vector3(0, 0, 0), Vector3(1, 0, 0));
    Vector3 point = ray.GetPoint(5.0f);
    
    TEST_ASSERT(point.isApprox(Vector3(5, 0, 0)), "射线上的点应该正确");
    
    return true;
}

bool Test_Ray_IntersectAABB() {
    Ray ray(Vector3(-5, 0.5f, 0.5f), Vector3(1, 0, 0));
    AABB aabb(Vector3(0, 0, 0), Vector3(1, 1, 1));
    
    float tMin, tMax;
    bool hit = ray.IntersectAABB(aabb, tMin, tMax);
    
    TEST_ASSERT(hit, "射线应该与 AABB 相交");
    TEST_ASSERT(tMin >= 0.0f, "tMin 应该非负");
    TEST_ASSERT(tMax > tMin, "tMax 应该大于 tMin");
    
    return true;
}

bool Test_Ray_IntersectAABB_Miss() {
    Ray ray(Vector3(-5, 5, 5), Vector3(1, 0, 0));
    AABB aabb(Vector3(0, 0, 0), Vector3(1, 1, 1));
    
    float tMin, tMax;
    bool hit = ray.IntersectAABB(aabb, tMin, tMax);
    
    TEST_ASSERT(!hit, "射线不应该与 AABB 相交");
    
    return true;
}

bool Test_Ray_IntersectPlane() {
    Ray ray(Vector3(0, 5, 0), Vector3(0, -1, 0));
    Plane plane(Vector3(0, 1, 0), 0.0f);  // Y = 0 平面
    
    float t;
    bool hit = ray.IntersectPlane(plane, t);
    
    TEST_ASSERT(hit, "射线应该与平面相交");
    TEST_ASSERT(std::abs(t - 5.0f) < 0.001f, "交点距离应该是 5");
    
    return true;
}

// ============================================================================
// RaycastHit 测试
// ============================================================================

bool Test_RaycastHit_IsValid() {
    RaycastHit hit1;
    TEST_ASSERT(!hit1.IsValid(), "默认 RaycastHit 应该无效");
    
    static ECS::EntityID entityId;  // 使用静态变量避免悬空指针
    RaycastHit hit2;
    hit2.entity = &entityId;
    TEST_ASSERT(hit2.IsValid(), "有实体的 RaycastHit 应该有效");
    
    return true;
}

// ============================================================================
// 主测试函数
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "物理引擎数学类型测试" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // AABB 测试
    std::cout << "\n--- AABB 测试 ---" << std::endl;
    RUN_TEST(Test_AABB_Creation);
    RUN_TEST(Test_AABB_GetCenter);
    RUN_TEST(Test_AABB_GetExtents);
    RUN_TEST(Test_AABB_Contains);
    RUN_TEST(Test_AABB_Intersects);
    RUN_TEST(Test_AABB_Merge);
    RUN_TEST(Test_AABB_Expand);
    RUN_TEST(Test_AABB_GetSurfaceArea);
    
    // OBB 测试
    std::cout << "\n--- OBB 测试 ---" << std::endl;
    RUN_TEST(Test_OBB_Creation);
    RUN_TEST(Test_OBB_FromAABB);
    RUN_TEST(Test_OBB_GetAABB);
    RUN_TEST(Test_OBB_GetVertices);
    
    // Ray 测试
    std::cout << "\n--- Ray 测试 ---" << std::endl;
    RUN_TEST(Test_Ray_Creation);
    RUN_TEST(Test_Ray_GetPoint);
    RUN_TEST(Test_Ray_IntersectAABB);
    RUN_TEST(Test_Ray_IntersectAABB_Miss);
    RUN_TEST(Test_Ray_IntersectPlane);
    
    // RaycastHit 测试
    std::cout << "\n--- RaycastHit 测试 ---" << std::endl;
    RUN_TEST(Test_RaycastHit_IsValid);
    
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

