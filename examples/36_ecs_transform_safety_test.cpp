/**
 * @file 36_ecs_transform_safety_test.cpp
 * @brief ECS Transform 安全性测试（方案B实现）
 * 
 * 测试内容：
 * - 基于实体ID的父子关系管理
 * - 父对象生命周期安全
 * - TransformSystem 批量更新
 * - 验证接口功能
 */

#include <render/ecs/world.h>
#include <render/ecs/components.h>
#include <render/ecs/systems.h>
#include <render/logger.h>
#include <iostream>
#include <chrono>

using namespace Render;
using namespace Render::ECS;

// 测试 1：SetParentEntity 基础功能
void TestSetParentEntityBasic() {
    std::cout << "测试 1: SetParentEntity 基础功能..." << std::endl;
    
    World world;
    world.Initialize();
    world.RegisterComponent<TransformComponent>();
    
    EntityID parent = world.CreateEntity();
    EntityID child = world.CreateEntity();
    
    world.AddComponent<TransformComponent>(parent, TransformComponent{});
    world.AddComponent<TransformComponent>(child, TransformComponent{});
    
    auto& parentComp = world.GetComponent<TransformComponent>(parent);
    auto& childComp = world.GetComponent<TransformComponent>(child);
    
    // 正常情况：设置父实体
    bool success = childComp.SetParentEntity(&world, parent);
    if (!success) {
        throw std::runtime_error("Failed to set valid parent entity");
    }
    
    if (childComp.GetParentEntity() != parent) {
        throw std::runtime_error("Parent entity ID not set correctly");
    }
    std::cout << "  ✓ 设置父实体成功" << std::endl;
    
    // 自引用（应该失败）
    success = parentComp.SetParentEntity(&world, parent);
    if (success) {
        throw std::runtime_error("Self-reference should fail");
    }
    std::cout << "  ✓ 自引用被正确拒绝" << std::endl;
    
    // 无效实体（应该失败）
    EntityID invalid = EntityID::Invalid();
    invalid.index = 9999;  // 不存在的实体
    success = childComp.SetParentEntity(&world, invalid);
    if (success) {
        throw std::runtime_error("Invalid entity should fail");
    }
    std::cout << "  ✓ 无效实体被正确拒绝" << std::endl;
    
    // 清除父实体
    success = childComp.RemoveParent();
    if (!success || childComp.GetParentEntity().IsValid()) {
        throw std::runtime_error("Failed to remove parent");
    }
    std::cout << "  ✓ 清除父实体成功" << std::endl;
    
    world.Shutdown();
    std::cout << "  测试 1 通过\n" << std::endl;
}

// 测试 2：父对象生命周期（实体ID管理）
void TestParentLifetimeWithEntityID() {
    std::cout << "测试 2: 父对象生命周期（实体ID）..." << std::endl;
    
    World world;
    world.Initialize();
    world.RegisterComponent<TransformComponent>();
    world.RegisterSystem<TransformSystem>();
    
    EntityID parent = world.CreateEntity();
    EntityID child = world.CreateEntity();
    
    world.AddComponent<TransformComponent>(parent, TransformComponent{});
    world.AddComponent<TransformComponent>(child, TransformComponent{});
    
    auto& parentComp = world.GetComponent<TransformComponent>(parent);
    auto& childComp = world.GetComponent<TransformComponent>(child);
    
    // 设置父实体
    bool success = childComp.SetParentEntity(&world, parent);
    if (!success) {
        throw std::runtime_error("Failed to set parent entity");
    }
    std::cout << "  ✓ 父实体设置成功" << std::endl;
    
    // 更新一帧（同步父子关系）
    world.Update(0.016f);
    
    // 验证Transform指针已同步
    if (childComp.transform->GetParent() != parentComp.transform.get()) {
        throw std::runtime_error("Transform parent pointer not synced");
    }
    std::cout << "  ✓ Transform 指针同步成功" << std::endl;
    
    // 销毁父实体
    world.DestroyEntity(parent);
    
    // 更新一帧（应该检测并清除父子关系）
    world.Update(0.016f);
    
    // 验证父实体已清除
    if (childComp.GetParentEntity().IsValid()) {
        throw std::runtime_error("Parent entity ID not cleared after parent destroyed");
    }
    
    if (childComp.transform->GetParent() != nullptr) {
        throw std::runtime_error("Transform parent pointer not cleared after parent destroyed");
    }
    std::cout << "  ✓ 父实体销毁后自动清除" << std::endl;
    
    world.Shutdown();
    std::cout << "  测试 2 通过\n" << std::endl;
}

// 测试 3：循环引用检测（实体ID级别）
void TestCircularReferenceWithEntityID() {
    std::cout << "测试 3: 循环引用检测（实体ID）..." << std::endl;
    
    World world;
    world.Initialize();
    world.RegisterComponent<TransformComponent>();
    world.RegisterSystem<TransformSystem>();
    
    EntityID entityA = world.CreateEntity();
    EntityID entityB = world.CreateEntity();
    EntityID entityC = world.CreateEntity();
    
    world.AddComponent<TransformComponent>(entityA, TransformComponent{});
    world.AddComponent<TransformComponent>(entityB, TransformComponent{});
    world.AddComponent<TransformComponent>(entityC, TransformComponent{});
    
    auto& compA = world.GetComponent<TransformComponent>(entityA);
    auto& compB = world.GetComponent<TransformComponent>(entityB);
    auto& compC = world.GetComponent<TransformComponent>(entityC);
    
    // A -> B
    bool success = compA.SetParentEntity(&world, entityB);
    if (!success) {
        throw std::runtime_error("Failed to set A->B");
    }
    
    // B -> C
    success = compB.SetParentEntity(&world, entityC);
    if (!success) {
        throw std::runtime_error("Failed to set B->C");
    }
    
    std::cout << "  ✓ 建立链式关系 A->B->C" << std::endl;
    
    // 更新一帧（同步所有关系）
    world.Update(0.016f);
    
    // C -> A（应该失败，形成循环）
    success = compC.SetParentEntity(&world, entityA);
    
    // 更新一帧（尝试同步，应该检测到循环并拒绝）
    world.Update(0.016f);
    
    // 验证循环引用被拒绝
    // Transform::SetParent 会检测循环引用并返回false
    // ValidateParentEntity 应该检测到失败并清除
    if (compC.transform->GetParent() == compA.transform.get()) {
        throw std::runtime_error("Circular reference was not rejected");
    }
    std::cout << "  ✓ 循环引用被正确拒绝" << std::endl;
    
    world.Shutdown();
    std::cout << "  测试 3 通过\n" << std::endl;
}

// 测试 4：验证接口
void TestValidateInterface() {
    std::cout << "测试 4: 验证接口..." << std::endl;
    
    World world;
    world.Initialize();
    world.RegisterComponent<TransformComponent>();
    world.RegisterSystem<TransformSystem>();
    
    EntityID entity = world.CreateEntity();
    world.AddComponent<TransformComponent>(entity, TransformComponent{});
    auto& comp = world.GetComponent<TransformComponent>(entity);
    
    // 正常情况应该通过验证
    if (!comp.Validate()) {
        throw std::runtime_error("Valid transform failed validation");
    }
    std::cout << "  ✓ 正常 Transform 通过验证" << std::endl;
    
    // 获取调试字符串
    std::string debugStr = comp.DebugString();
    if (debugStr.empty()) {
        throw std::runtime_error("DebugString returned empty");
    }
    std::cout << "  ✓ DebugString: " << debugStr.substr(0, 50) << "..." << std::endl;
    
    // 获取层级深度
    int depth = comp.GetHierarchyDepth();
    if (depth != 0) {
        throw std::runtime_error("Hierarchy depth should be 0");
    }
    std::cout << "  ✓ 层级深度正确: " << depth << std::endl;
    
    // 系统级验证
    auto* transformSystem = world.GetSystem<TransformSystem>();
    if (transformSystem) {
        size_t invalidCount = transformSystem->ValidateAll();
        if (invalidCount != 0) {
            throw std::runtime_error("System validation found invalid transforms");
        }
        std::cout << "  ✓ 系统验证通过，无无效 Transform" << std::endl;
    }
    
    world.Shutdown();
    std::cout << "  测试 4 通过\n" << std::endl;
}

// 测试 5：TransformSystem 批量更新
void TestTransformSystemBatchUpdate() {
    std::cout << "测试 5: TransformSystem 批量更新..." << std::endl;
    
    World world;
    world.Initialize();
    world.RegisterComponent<TransformComponent>();
    world.RegisterSystem<TransformSystem>();
    
    // 创建多个实体
    const int NUM_ENTITIES = 100;
    std::vector<EntityID> entities;
    for (int i = 0; i < NUM_ENTITIES; ++i) {
        EntityID entity = world.CreateEntity();
        world.AddComponent<TransformComponent>(entity, TransformComponent{});
        auto& comp = world.GetComponent<TransformComponent>(entity);
        comp.SetPosition(Vector3(i * 1.0f, 0, 0));
        entities.push_back(entity);
    }
    
    // 更新一帧
    world.Update(0.016f);
    
    // 修改所有实体的位置（触发dirty）
    for (const auto& entity : entities) {
        auto& comp = world.GetComponent<TransformComponent>(entity);
        Vector3 pos = comp.GetPosition();
        pos.y() = 10.0f;
        comp.SetPosition(pos);
    }
    
    // 更新一帧（应该批量更新所有dirty Transform）
    world.Update(0.016f);
    
    // 获取统计信息
    auto* transformSystem = world.GetSystem<TransformSystem>();
    if (transformSystem) {
        const auto& stats = transformSystem->GetStats();
        std::cout << "  统计信息:" << std::endl;
        std::cout << "    - 总实体数: " << stats.totalEntities << std::endl;
        std::cout << "    - 更新的 Transform: " << stats.dirtyTransforms << std::endl;
        std::cout << "    - 同步的父子关系: " << stats.syncedParents << std::endl;
        std::cout << "    - 清除的无效关系: " << stats.clearedParents << std::endl;
    }
    
    // 验证更新成功
    for (const auto& entity : entities) {
        const auto& comp = world.GetComponent<TransformComponent>(entity);
        Vector3 pos = comp.GetPosition();
        if (std::abs(pos.y() - 10.0f) > 0.001f) {
            throw std::runtime_error("Transform not updated correctly");
        }
    }
    std::cout << "  ✓ 批量更新 " << NUM_ENTITIES << " 个 Transform 成功" << std::endl;
    
    world.Shutdown();
    std::cout << "  测试 5 通过\n" << std::endl;
}

// 测试 6：父子关系同步性能测试
void TestParentChildSyncPerformance() {
    std::cout << "测试 6: 父子关系同步性能..." << std::endl;
    
    World world;
    world.Initialize();
    world.RegisterComponent<TransformComponent>();
    world.RegisterSystem<TransformSystem>();
    
    // 创建层级结构：1个根 + 10个子 + 每个子有10个孙
    EntityID root = world.CreateEntity();
    world.AddComponent<TransformComponent>(root, TransformComponent{});
    auto& rootComp = world.GetComponent<TransformComponent>(root);
    rootComp.SetPosition(Vector3(0, 0, 0));
    
    std::vector<EntityID> children;
    std::vector<EntityID> grandchildren;
    
    for (int i = 0; i < 10; ++i) {
        EntityID child = world.CreateEntity();
        world.AddComponent<TransformComponent>(child, TransformComponent{});
        auto& childComp = world.GetComponent<TransformComponent>(child);
        childComp.SetPosition(Vector3(i * 2.0f, 0, 0));
        childComp.SetParentEntity(&world, root);
        children.push_back(child);
        
        for (int j = 0; j < 10; ++j) {
            EntityID grandchild = world.CreateEntity();
            world.AddComponent<TransformComponent>(grandchild, TransformComponent{});
            auto& grandchildComp = world.GetComponent<TransformComponent>(grandchild);
            grandchildComp.SetPosition(Vector3(0, j * 2.0f, 0));
            grandchildComp.SetParentEntity(&world, child);
            grandchildren.push_back(grandchild);
        }
    }
    
    std::cout << "  创建了 1 根 + 10 子 + 100 孙 = 111 实体" << std::endl;
    
    // 第一次更新（同步所有父子关系）
    auto start = std::chrono::high_resolution_clock::now();
    world.Update(0.016f);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "  第一次更新（同步）耗时: " << duration.count() << " μs" << std::endl;
    
    // 验证所有Transform指针已同步
    int syncedCount = 0;
    for (const auto& child : children) {
        auto& childComp = world.GetComponent<TransformComponent>(child);
        if (childComp.transform->GetParent() == rootComp.transform.get()) {
            syncedCount++;
        }
    }
    std::cout << "  同步的子实体: " << syncedCount << "/10" << std::endl;
    
    // 修改根节点（触发所有子孙节点dirty）
    rootComp.SetPosition(Vector3(100, 0, 0));
    
    // 第二次更新（批量更新dirty Transform）
    start = std::chrono::high_resolution_clock::now();
    world.Update(0.016f);
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "  第二次更新（批量更新）耗时: " << duration.count() << " μs" << std::endl;
    
    // 获取统计信息
    auto* transformSystem = world.GetSystem<TransformSystem>();
    if (transformSystem) {
        const auto& stats = transformSystem->GetStats();
        std::cout << "  批量更新了 " << stats.dirtyTransforms << " 个 Transform" << std::endl;
    }
    
    world.Shutdown();
    std::cout << "  测试 6 通过\n" << std::endl;
}

// 测试 7：并发场景（多实体同时修改父子关系）
void TestConcurrentParentChildChanges() {
    std::cout << "测试 7: 并发场景..." << std::endl;
    
    World world;
    world.Initialize();
    world.RegisterComponent<TransformComponent>();
    world.RegisterSystem<TransformSystem>();
    
    // 创建多个实体
    std::vector<EntityID> entities;
    for (int i = 0; i < 20; ++i) {
        EntityID entity = world.CreateEntity();
        world.AddComponent<TransformComponent>(entity, TransformComponent{});
        entities.push_back(entity);
    }
    
    // 模拟多次重新分配父子关系
    for (int frame = 0; frame < 10; ++frame) {
        // 随机重新分配父子关系
        for (size_t i = 1; i < entities.size(); ++i) {
            auto& comp = world.GetComponent<TransformComponent>(entities[i]);
            
            if (frame % 2 == 0) {
                // 偶数帧：设置父实体为前一个
                comp.SetParentEntity(&world, entities[i - 1]);
            } else {
                // 奇数帧：清除父实体
                comp.RemoveParent();
            }
        }
        
        // 更新一帧
        world.Update(0.016f);
    }
    
    std::cout << "  ✓ 完成 10 帧父子关系变化，无崩溃" << std::endl;
    
    // 验证最终状态一致
    auto* transformSystem = world.GetSystem<TransformSystem>();
    if (transformSystem) {
        size_t invalidCount = transformSystem->ValidateAll();
        if (invalidCount > 0) {
            throw std::runtime_error("Found invalid transforms after concurrent changes");
        }
        std::cout << "  ✓ 最终状态验证通过" << std::endl;
    }
    
    world.Shutdown();
    std::cout << "  测试 7 通过\n" << std::endl;
}

int main() {
    try {
        std::cout << "======================================" << std::endl;
        std::cout << "ECS Transform 安全性测试（方案B）" << std::endl;
        std::cout << "======================================\n" << std::endl;
        
        TestSetParentEntityBasic();
        TestParentLifetimeWithEntityID();
        TestCircularReferenceWithEntityID();
        TestValidateInterface();
        TestTransformSystemBatchUpdate();
        TestParentChildSyncPerformance();
        TestConcurrentParentChildChanges();
        
        std::cout << "======================================" << std::endl;
        std::cout << "所有测试通过！✓" << std::endl;
        std::cout << "======================================" << std::endl;
        std::cout << "\n测试总结：" << std::endl;
        std::cout << "  1. ✓ SetParentEntity 基础功能" << std::endl;
        std::cout << "  2. ✓ 父对象生命周期（实体ID）" << std::endl;
        std::cout << "  3. ✓ 循环引用检测" << std::endl;
        std::cout << "  4. ✓ 验证接口" << std::endl;
        std::cout << "  5. ✓ TransformSystem 批量更新" << std::endl;
        std::cout << "  6. ✓ 父子关系同步性能" << std::endl;
        std::cout << "  7. ✓ 并发场景测试" << std::endl;
        std::cout << "======================================" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n测试失败: " << e.what() << std::endl;
        return 1;
    }
}
