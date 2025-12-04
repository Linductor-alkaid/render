/**
 * @file test_broad_phase.cpp
 * @brief 粗检测测试
 */

#include "render/physics/collision/broad_phase.h"
#include <iostream>

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
// SpatialHashBroadPhase 测试
// ============================================================================

bool Test_SpatialHash_Empty() {
    SpatialHashBroadPhase broadPhase(5.0f);
    
    std::vector<std::pair<ECS::EntityID, AABB>> entities;
    broadPhase.Update(entities);
    
    auto pairs = broadPhase.DetectPairs();
    
    TEST_ASSERT(pairs.empty(), "空场景不应该有碰撞对");
    TEST_ASSERT(broadPhase.GetObjectCount() == 0, "物体数应该是 0");
    
    return true;
}

bool Test_SpatialHash_SingleEntity() {
    SpatialHashBroadPhase broadPhase(5.0f);
    
    std::vector<std::pair<ECS::EntityID, AABB>> entities;
    ECS::EntityID entity1(0, 1);
    entities.push_back({entity1, AABB(Vector3(0, 0, 0), Vector3(1, 1, 1))});
    
    broadPhase.Update(entities);
    auto pairs = broadPhase.DetectPairs();
    
    TEST_ASSERT(pairs.empty(), "单个物体不应该有碰撞对");
    TEST_ASSERT(broadPhase.GetObjectCount() == 1, "物体数应该是 1");
    
    return true;
}

bool Test_SpatialHash_TwoNearEntities() {
    SpatialHashBroadPhase broadPhase(5.0f);
    
    std::vector<std::pair<ECS::EntityID, AABB>> entities;
    ECS::EntityID entity1(0, 1);
    ECS::EntityID entity2(1, 1);
    
    // 两个靠近的物体
    entities.push_back({entity1, AABB(Vector3(0, 0, 0), Vector3(1, 1, 1))});
    entities.push_back({entity2, AABB(Vector3(2, 0, 0), Vector3(3, 1, 1))});
    
    broadPhase.Update(entities);
    auto pairs = broadPhase.DetectPairs();
    
    TEST_ASSERT(pairs.size() == 1, "应该检测到 1 对可能碰撞");
    TEST_ASSERT(broadPhase.GetObjectCount() == 2, "物体数应该是 2");
    
    return true;
}

bool Test_SpatialHash_TwoFarEntities() {
    SpatialHashBroadPhase broadPhase(5.0f);
    
    std::vector<std::pair<ECS::EntityID, AABB>> entities;
    ECS::EntityID entity1(0, 1);
    ECS::EntityID entity2(1, 1);
    
    // 两个距离很远的物体（超过一个格子）
    entities.push_back({entity1, AABB(Vector3(0, 0, 0), Vector3(1, 1, 1))});
    entities.push_back({entity2, AABB(Vector3(100, 0, 0), Vector3(101, 1, 1))});
    
    broadPhase.Update(entities);
    auto pairs = broadPhase.DetectPairs();
    
    TEST_ASSERT(pairs.empty(), "距离远的物体不应该被检测到");
    
    return true;
}

bool Test_SpatialHash_MultipleEntities() {
    SpatialHashBroadPhase broadPhase(5.0f);
    
    std::vector<std::pair<ECS::EntityID, AABB>> entities;
    
    // 创建 4 个物体，其中 3 个在同一区域
    for (int i = 0; i < 4; ++i) {
        ECS::EntityID entity(i, 1);
        float offset = (i < 3) ? static_cast<float>(i) * 2.0f : 100.0f;
        entities.push_back({
            entity,
            AABB(Vector3(offset, 0, 0), Vector3(offset + 1, 1, 1))
        });
    }
    
    broadPhase.Update(entities);
    auto pairs = broadPhase.DetectPairs();
    
    // 前 3 个物体应该产生 C(3,2) = 3 对
    TEST_ASSERT(pairs.size() == 3, "应该检测到 3 对可能碰撞");
    
    return true;
}

bool Test_SpatialHash_LargeEntity() {
    SpatialHashBroadPhase broadPhase(5.0f);
    
    std::vector<std::pair<ECS::EntityID, AABB>> entities;
    ECS::EntityID entity1(0, 1);
    ECS::EntityID entity2(1, 1);
    
    // 一个大物体跨越多个格子
    entities.push_back({entity1, AABB(Vector3(0, 0, 0), Vector3(15, 1, 1))});
    entities.push_back({entity2, AABB(Vector3(10, 0, 0), Vector3(11, 1, 1))});
    
    broadPhase.Update(entities);
    auto pairs = broadPhase.DetectPairs();
    
    TEST_ASSERT(pairs.size() == 1, "跨格子的大物体应该被正确检测");
    TEST_ASSERT(broadPhase.GetCellCount() > 1, "应该占用多个格子");
    
    return true;
}

bool Test_SpatialHash_NoDuplicates() {
    SpatialHashBroadPhase broadPhase(5.0f);
    
    std::vector<std::pair<ECS::EntityID, AABB>> entities;
    ECS::EntityID entity1(0, 1);
    ECS::EntityID entity2(1, 1);
    
    // 两个物体同时占据多个格子
    entities.push_back({entity1, AABB(Vector3(0, 0, 0), Vector3(6, 6, 6))});
    entities.push_back({entity2, AABB(Vector3(3, 3, 3), Vector3(9, 9, 9))});
    
    broadPhase.Update(entities);
    auto pairs = broadPhase.DetectPairs();
    
    TEST_ASSERT(pairs.size() == 1, "即使在多个格子相遇，也只应该返回 1 对");
    
    return true;
}

bool Test_SpatialHash_Clear() {
    SpatialHashBroadPhase broadPhase(5.0f);
    
    std::vector<std::pair<ECS::EntityID, AABB>> entities;
    entities.push_back({ECS::EntityID(0, 1), AABB(Vector3(0, 0, 0), Vector3(1, 1, 1))});
    
    broadPhase.Update(entities);
    TEST_ASSERT(broadPhase.GetObjectCount() == 1, "更新后应该有物体");
    
    broadPhase.Clear();
    TEST_ASSERT(broadPhase.GetObjectCount() == 0, "清空后应该没有物体");
    TEST_ASSERT(broadPhase.GetCellCount() == 0, "清空后应该没有格子");
    
    return true;
}

// ============================================================================
// OctreeBroadPhase 测试
// ============================================================================

bool Test_Octree_Empty() {
    OctreeBroadPhase octree(AABB(Vector3(-50, -50, -50), Vector3(50, 50, 50)));
    
    std::vector<std::pair<ECS::EntityID, AABB>> entities;
    octree.Update(entities);
    
    auto pairs = octree.DetectPairs();
    
    TEST_ASSERT(pairs.empty(), "空场景不应该有碰撞对");
    TEST_ASSERT(octree.GetObjectCount() == 0, "物体数应该是 0");
    
    return true;
}

bool Test_Octree_TwoNearEntities() {
    OctreeBroadPhase octree(AABB(Vector3(-50, -50, -50), Vector3(50, 50, 50)));
    
    std::vector<std::pair<ECS::EntityID, AABB>> entities;
    ECS::EntityID entity1(0, 1);
    ECS::EntityID entity2(1, 1);
    
    entities.push_back({entity1, AABB(Vector3(0, 0, 0), Vector3(1, 1, 1))});
    entities.push_back({entity2, AABB(Vector3(2, 0, 0), Vector3(3, 1, 1))});
    
    octree.Update(entities);
    auto pairs = octree.DetectPairs();
    
    TEST_ASSERT(pairs.size() == 1, "靠近的物体应该被检测到");
    
    return true;
}

bool Test_Octree_Subdivision() {
    OctreeBroadPhase octree(AABB(Vector3(-50, -50, -50), Vector3(50, 50, 50)), 8, 4);
    
    std::vector<std::pair<ECS::EntityID, AABB>> entities;
    
    // 添加多个物体触发细分
    for (int i = 0; i < 10; ++i) {
        ECS::EntityID entity(i, 1);
        float offset = static_cast<float>(i) * 2.0f;
        entities.push_back({
            entity,
            AABB(Vector3(offset, 0, 0), Vector3(offset + 1, 1, 1))
        });
    }
    
    octree.Update(entities);
    
    TEST_ASSERT(octree.GetCellCount() > 1, "应该触发细分");
    TEST_ASSERT(octree.GetObjectCount() == 10, "应该包含所有物体");
    
    return true;
}

bool Test_Octree_Clear() {
    OctreeBroadPhase octree(AABB(Vector3(-50, -50, -50), Vector3(50, 50, 50)));
    
    std::vector<std::pair<ECS::EntityID, AABB>> entities;
    entities.push_back({ECS::EntityID(0, 1), AABB(Vector3(0, 0, 0), Vector3(1, 1, 1))});
    
    octree.Update(entities);
    TEST_ASSERT(octree.GetObjectCount() == 1, "更新后应该有物体");
    
    octree.Clear();
    TEST_ASSERT(octree.GetObjectCount() == 0, "清空后应该没有物体");
    
    return true;
}

// ============================================================================
// 主测试函数
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "粗检测系统测试" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::cout << "\n--- SpatialHashBroadPhase 测试 ---" << std::endl;
    RUN_TEST(Test_SpatialHash_Empty);
    RUN_TEST(Test_SpatialHash_SingleEntity);
    RUN_TEST(Test_SpatialHash_TwoNearEntities);
    RUN_TEST(Test_SpatialHash_TwoFarEntities);
    RUN_TEST(Test_SpatialHash_MultipleEntities);
    RUN_TEST(Test_SpatialHash_LargeEntity);
    RUN_TEST(Test_SpatialHash_NoDuplicates);
    RUN_TEST(Test_SpatialHash_Clear);
    
    std::cout << "\n--- OctreeBroadPhase 测试 ---" << std::endl;
    RUN_TEST(Test_Octree_Empty);
    RUN_TEST(Test_Octree_TwoNearEntities);
    RUN_TEST(Test_Octree_Subdivision);
    RUN_TEST(Test_Octree_Clear);
    
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

