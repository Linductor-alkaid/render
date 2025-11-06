# ECS Transform 修复方案

[返回文档首页](../README.md)

---

## 修复方案概述

本文档提供了 [ECS Transform 安全性审查](../ECS_TRANSFORM_SAFETY_REVIEW.md) 中发现问题的详细修复方案和代码示例。

---

## 目录

1. [P1 修复：SetParent 返回值检查](#p1-修复setparent-返回值检查)
2. [P1 修复：父子关系生命周期管理](#p1-修复父子关系生命周期管理)
3. [P2 修复：TransformSystem 功能完善](#p2-修复transformsystem-功能完善)
4. [P2 修复：添加验证接口](#p2-修复添加验证接口)
5. [P3 改进：接口一致性](#p3-改进接口一致性)
6. [测试验证](#测试验证)

---

## P1 修复：SetParent 返回值检查

### 修复前

```cpp
// include/render/ecs/components.h
struct TransformComponent {
    void SetParent(const Ref<Transform>& parent) {
        if (transform && parent) transform->SetParent(parent.get());
        // ❌ 未检查返回值
    }
};
```

### 修复后

```cpp
// include/render/ecs/components.h
struct TransformComponent {
    /**
     * @brief 设置父对象
     * @param parent 父 Transform 对象
     * @return 成功返回 true，失败返回 false
     * 
     * @note 失败情况：
     * - 自引用（parent == this）
     * - 循环引用（A->B->C->A）
     * - 层级过深（超过 1000 层）
     */
    bool SetParent(const Ref<Transform>& parent) {
        if (!transform) return false;
        
        if (!parent) {
            // 清除父对象
            transform->SetParent(nullptr);
            return true;
        }
        
        // 调用 Transform::SetParent 并返回结果
        bool success = transform->SetParent(parent.get());
        if (!success) {
            Logger::GetInstance().WarningFormat(
                "[TransformComponent] Failed to set parent (possible circular reference or depth limit)"
            );
        }
        return success;
    }
};
```

---

## P1 修复：父子关系生命周期管理

### 方案 A：使用 weak_ptr（较保守）

```cpp
// include/render/ecs/components.h
struct TransformComponent {
    Ref<Transform> transform;
    std::weak_ptr<Transform> parentTransform;  // 新增：持有父对象的 weak_ptr
    
    /**
     * @brief 设置父对象（使用 weak_ptr 管理生命周期）
     */
    bool SetParent(const Ref<Transform>& parent) {
        if (!transform) return false;
        
        if (parent) {
            // 存储 weak_ptr
            parentTransform = parent;
            
            // 设置原始指针到 Transform
            bool success = transform->SetParent(parent.get());
            if (!success) {
                parentTransform.reset();  // 失败时清除 weak_ptr
                return false;
            }
            return true;
        } else {
            // 清除父对象
            parentTransform.reset();
            transform->SetParent(nullptr);
            return true;
        }
    }
    
    /**
     * @brief 获取父对象（返回 shared_ptr）
     */
    Ref<Transform> GetParentShared() const {
        return parentTransform.lock();
    }
    
    /**
     * @brief 获取父对象（返回原始指针，兼容旧接口）
     */
    Transform* GetParent() const {
        return transform ? transform->GetParent() : nullptr;
    }
    
    /**
     * @brief 验证父对象是否仍然有效
     */
    bool ValidateParent() {
        if (auto parent = parentTransform.lock()) {
            // 父对象仍然存在
            if (transform->GetParent() == parent.get()) {
                return true;  // 一致
            } else {
                // 不一致，可能被其他代码修改，同步状态
                Logger::GetInstance().WarningFormat(
                    "[TransformComponent] Parent pointer inconsistent, syncing"
                );
                parentTransform.reset();
                return false;
            }
        } else {
            // 父对象已销毁
            if (transform->GetParent() != nullptr) {
                // Transform 仍持有父指针（应该被 NotifyChildrenParentDestroyed 清除）
                Logger::GetInstance().WarningFormat(
                    "[TransformComponent] Parent destroyed but pointer not cleared, fixing"
                );
                transform->SetParent(nullptr);
            }
            return true;
        }
    }
};
```

### 方案 B：使用实体 ID（推荐）

```cpp
// include/render/ecs/components.h
struct TransformComponent {
    Ref<Transform> transform;
    EntityID parentEntity = EntityID::Invalid();  // 新增：父实体 ID
    
    /**
     * @brief 设置父实体（通过 World 管理）
     * @param world World 对象
     * @param parent 父实体 ID
     * @return 成功返回 true
     * 
     * @note 此方法应该在 TransformSystem 或 HierarchySystem 中调用
     */
    bool SetParentEntity(World* world, EntityID parent) {
        if (!world || !transform) return false;
        
        if (parent.IsValid()) {
            // 验证父实体存在
            if (!world->IsValidEntity(parent)) {
                Logger::GetInstance().WarningFormat(
                    "[TransformComponent] Parent entity %u is invalid", parent.index
                );
                return false;
            }
            
            // 验证父实体有 TransformComponent
            if (!world->HasComponent<TransformComponent>(parent)) {
                Logger::GetInstance().WarningFormat(
                    "[TransformComponent] Parent entity %u has no TransformComponent", parent.index
                );
                return false;
            }
            
            // 获取父 Transform
            auto& parentComp = world->GetComponent<TransformComponent>(parent);
            if (!parentComp.transform) {
                Logger::GetInstance().WarningFormat(
                    "[TransformComponent] Parent entity %u has null Transform", parent.index
                );
                return false;
            }
            
            // 设置父 Transform
            bool success = transform->SetParent(parentComp.transform.get());
            if (!success) {
                return false;
            }
            
            // 存储父实体 ID
            parentEntity = parent;
            return true;
        } else {
            // 清除父对象
            parentEntity = EntityID::Invalid();
            transform->SetParent(nullptr);
            return true;
        }
    }
    
    /**
     * @brief 获取父实体 ID
     */
    EntityID GetParentEntity() const {
        return parentEntity;
    }
    
    /**
     * @brief 验证父实体是否仍然有效
     */
    bool ValidateParentEntity(World* world) {
        if (!parentEntity.IsValid()) {
            // 无父实体，确保 Transform 也没有父对象
            if (transform && transform->GetParent() != nullptr) {
                Logger::GetInstance().WarningFormat(
                    "[TransformComponent] Inconsistent state: no parentEntity but has parent pointer"
                );
                transform->SetParent(nullptr);
                return false;
            }
            return true;
        }
        
        // 有父实体 ID
        if (!world || !world->IsValidEntity(parentEntity)) {
            // 父实体已销毁
            Logger::GetInstance().WarningFormat(
                "[TransformComponent] Parent entity %u is no longer valid, clearing", parentEntity.index
            );
            parentEntity = EntityID::Invalid();
            if (transform) {
                transform->SetParent(nullptr);
            }
            return false;
        }
        
        return true;
    }
};
```

---

## P2 修复：TransformSystem 功能完善

### 修复前

```cpp
// src/ecs/systems.cpp
void TransformSystem::Update(float deltaTime) {
    (void)deltaTime;  // 未使用
    
    // Transform 的层级更新由 Transform 类自动处理（通过缓存机制）
    // 这里可以添加额外的变换更新逻辑（如果需要）
}
```

### 修复后（方案 A：惰性更新，保持当前设计）

```cpp
// include/render/ecs/systems.h
class TransformSystem : public System {
public:
    void Update(float deltaTime) override;
    [[nodiscard]] int GetPriority() const override { return 10; }
    
    /**
     * @brief 验证所有 Transform 状态（调试用）
     */
    void ValidateAll();
    
    /**
     * @brief 同步父子关系（如果使用实体 ID 管理）
     */
    void SyncParentChildRelations();
};

// src/ecs/systems.cpp
void TransformSystem::Update(float deltaTime) {
    (void)deltaTime;
    
    // 选项 1：如果使用实体 ID 管理父子关系，需要同步
    #ifdef USE_ENTITY_ID_PARENT
    SyncParentChildRelations();
    #endif
    
    // 选项 2：验证父实体（如果使用实体 ID）
    #ifdef DEBUG
    if (m_world) {
        auto entities = m_world->Query<TransformComponent>();
        for (const auto& entity : entities) {
            auto& comp = m_world->GetComponent<TransformComponent>(entity);
            comp.ValidateParentEntity(m_world);
        }
    }
    #endif
}

void TransformSystem::ValidateAll() {
    if (!m_world) return;
    
    auto entities = m_world->Query<TransformComponent>();
    size_t invalidCount = 0;
    
    for (const auto& entity : entities) {
        const auto& comp = m_world->GetComponent<TransformComponent>(entity);
        if (!comp.Validate()) {
            Logger::GetInstance().WarningFormat(
                "[TransformSystem] Entity %u has invalid Transform", entity.index
            );
            invalidCount++;
        }
    }
    
    if (invalidCount > 0) {
        Logger::GetInstance().WarningFormat(
            "[TransformSystem] Found %zu entities with invalid Transform", invalidCount
        );
    }
}

void TransformSystem::SyncParentChildRelations() {
    if (!m_world) return;
    
    auto entities = m_world->Query<TransformComponent>();
    
    for (const auto& entity : entities) {
        auto& comp = m_world->GetComponent<TransformComponent>(entity);
        
        if (comp.parentEntity.IsValid()) {
            // 验证父实体
            if (!m_world->IsValidEntity(comp.parentEntity)) {
                // 父实体已销毁，清除关系
                Logger::GetInstance().DebugFormat(
                    "[TransformSystem] Parent entity %u of entity %u is invalid, clearing",
                    comp.parentEntity.index, entity.index
                );
                comp.parentEntity = EntityID::Invalid();
                if (comp.transform) {
                    comp.transform->SetParent(nullptr);
                }
                continue;
            }
            
            // 验证父实体有 TransformComponent
            if (!m_world->HasComponent<TransformComponent>(comp.parentEntity)) {
                Logger::GetInstance().WarningFormat(
                    "[TransformSystem] Parent entity %u has no TransformComponent, clearing",
                    comp.parentEntity.index
                );
                comp.parentEntity = EntityID::Invalid();
                if (comp.transform) {
                    comp.transform->SetParent(nullptr);
                }
                continue;
            }
            
            // 同步父 Transform 指针
            auto& parentComp = m_world->GetComponent<TransformComponent>(comp.parentEntity);
            if (comp.transform && parentComp.transform) {
                if (comp.transform->GetParent() != parentComp.transform.get()) {
                    // 父指针不一致，重新设置
                    Logger::GetInstance().DebugFormat(
                        "[TransformSystem] Syncing parent pointer for entity %u", entity.index
                    );
                    comp.transform->SetParent(parentComp.transform.get());
                }
            }
        }
    }
}
```

### 修复后（方案 B：主动批量更新，性能优化）

**注意**：此方案需要在 Transform 类中添加以下方法：

```cpp
// include/render/transform.h
class Transform {
public:
    // ... 现有方法 ...
    
    /**
     * @brief 检查是否需要更新世界变换
     */
    [[nodiscard]] bool IsDirty() const {
        return m_dirtyWorld.load(std::memory_order_acquire);
    }
    
    /**
     * @brief 强制更新世界变换（供批量更新使用）
     */
    void ForceUpdateWorldTransform() {
        if (m_dirtyWorld.load(std::memory_order_acquire)) {
            UpdateWorldTransformCache();
        }
    }
};
```

然后实现批量更新：

```cpp
// src/ecs/systems.cpp
void TransformSystem::Update(float deltaTime) {
    (void)deltaTime;
    
    if (!m_world) return;
    
    // 1. 收集所有 Transform
    auto entities = m_world->Query<TransformComponent>();
    if (entities.empty()) return;
    
    // 2. 收集需要更新的 Transform
    struct TransformInfo {
        Transform* transform;
        int depth;
    };
    std::vector<TransformInfo> dirtyTransforms;
    dirtyTransforms.reserve(entities.size());
    
    for (const auto& entity : entities) {
        auto& comp = m_world->GetComponent<TransformComponent>(entity);
        if (comp.transform && comp.transform->IsDirty()) {
            dirtyTransforms.push_back({
                comp.transform.get(),
                comp.transform->GetHierarchyDepth()
            });
        }
    }
    
    if (dirtyTransforms.empty()) return;
    
    // 3. 按层级深度排序（父对象先更新）
    std::sort(dirtyTransforms.begin(), dirtyTransforms.end(),
        [](const TransformInfo& a, const TransformInfo& b) {
            return a.depth < b.depth;
        });
    
    // 4. 批量更新
    for (const auto& info : dirtyTransforms) {
        info.transform->ForceUpdateWorldTransform();
    }
    
    // 5. 统计信息（可选）
    static int frameCount = 0;
    if (++frameCount % 60 == 0) {
        Logger::GetInstance().DebugFormat(
            "[TransformSystem] Updated %zu transforms", dirtyTransforms.size()
        );
    }
}
```

---

## P2 修复：添加验证接口

### 修复实现

```cpp
// include/render/ecs/components.h
struct TransformComponent {
    // ... 现有代码 ...
    
    /**
     * @brief 验证 Transform 状态
     * @return 如果 Transform 有效返回 true
     * 
     * @note 验证内容：
     * - Transform 对象是否为空
     * - 四元数是否归一化
     * - 位置/缩放是否包含 NaN/Inf
     * - 缩放是否过小/过大
     * - 父指针是否有效
     * - 层级深度是否合理
     */
    [[nodiscard]] bool Validate() const {
        if (!transform) {
            Logger::GetInstance().WarningFormat(
                "[TransformComponent] Transform is null"
            );
            return false;
        }
        return transform->Validate();
    }
    
    /**
     * @brief 获取调试字符串
     */
    [[nodiscard]] std::string DebugString() const {
        if (!transform) {
            return "TransformComponent { transform: null }";
        }
        return "TransformComponent { " + transform->DebugString() + " }";
    }
    
    /**
     * @brief 获取层级深度
     */
    [[nodiscard]] int GetHierarchyDepth() const {
        return transform ? transform->GetHierarchyDepth() : 0;
    }
    
    /**
     * @brief 获取子对象数量
     */
    [[nodiscard]] int GetChildCount() const {
        return transform ? transform->GetChildCount() : 0;
    }
    
    /**
     * @brief 打印层级结构（调试用）
     */
    void PrintHierarchy(int indent = 0) const {
        if (transform) {
            transform->PrintHierarchy(indent);
        }
    }
};
```

---

## P3 改进：接口一致性

### 修复前（接口不一致）

```cpp
struct TransformComponent {
    void SetParent(const Ref<Transform>& parent);  // 接受 shared_ptr
    Transform* GetParent() const;                  // 返回原始指针
};
```

### 修复后（提供两套一致的接口）

```cpp
struct TransformComponent {
    // ==================== 原始指针接口（兼容性）====================
    
    /**
     * @brief 获取父对象（原始指针）
     * @warning 返回的指针可能会失效，使用时需谨慎
     */
    [[nodiscard]] Transform* GetParent() const {
        return transform ? transform->GetParent() : nullptr;
    }
    
    // ==================== shared_ptr 接口（推荐）====================
    
    /**
     * @brief 设置父对象（使用 shared_ptr）
     * @param parent 父 Transform 对象
     * @return 成功返回 true
     * 
     * @note 此方法会将 shared_ptr 转换为原始指针传递给 Transform 类
     * @note 生命周期由 weak_ptr 或实体 ID 管理
     */
    bool SetParentShared(const Ref<Transform>& parent) {
        // 使用方案 A 或 B 的实现
        return SetParent(parent);
    }
    
    /**
     * @brief 获取父对象（返回 shared_ptr）
     * @return 父 Transform 的 shared_ptr，如果不存在则返回 nullptr
     * 
     * @note 此方法使用 weak_ptr 保证返回的 shared_ptr 有效
     */
    [[nodiscard]] Ref<Transform> GetParentShared() const {
        return parentTransform.lock();  // 假设使用方案 A
    }
    
    // ==================== 实体 ID 接口（最安全）====================
    
    /**
     * @brief 设置父实体（通过实体 ID）
     * @param world World 对象
     * @param parent 父实体 ID
     * @return 成功返回 true
     */
    bool SetParentEntity(World* world, EntityID parent) {
        // 使用方案 B 的实现
    }
    
    /**
     * @brief 获取父实体 ID
     */
    [[nodiscard]] EntityID GetParentEntity() const {
        return parentEntity;  // 假设使用方案 B
    }
};
```

---

## 测试验证

### 单元测试

创建 `examples/36_ecs_transform_safety_test.cpp`:

```cpp
#include <render/ecs/world.h>
#include <render/ecs/components.h>
#include <render/ecs/systems.h>
#include <render/logger.h>
#include <iostream>

using namespace Render;
using namespace Render::ECS;

// 测试 1：SetParent 返回值检查
void TestSetParentReturnValue() {
    std::cout << "测试 1: SetParent 返回值检查..." << std::endl;
    
    World world;
    world.Initialize();
    world.RegisterComponent<TransformComponent>();
    
    EntityID entity1 = world.CreateEntity();
    EntityID entity2 = world.CreateEntity();
    
    auto& comp1 = world.AddComponent<TransformComponent>(entity1);
    auto& comp2 = world.AddComponent<TransformComponent>(entity2);
    
    // 正常情况
    bool success = comp2.SetParent(comp1.transform);
    if (!success) {
        throw std::runtime_error("Failed to set valid parent");
    }
    std::cout << "  ✓ 正常情况返回 true" << std::endl;
    
    // 自引用（应该失败）
    success = comp1.SetParent(comp1.transform);
    if (success) {
        throw std::runtime_error("Self-reference should fail");
    }
    std::cout << "  ✓ 自引用返回 false" << std::endl;
    
    // 循环引用（应该失败）
    success = comp1.SetParent(comp2.transform);
    if (success) {
        throw std::runtime_error("Circular reference should fail");
    }
    std::cout << "  ✓ 循环引用返回 false" << std::endl;
    
    world.Shutdown();
    std::cout << "  测试 1 通过\n" << std::endl;
}

// 测试 2：父对象生命周期（使用 weak_ptr）
void TestParentLifetimeWithWeakPtr() {
    std::cout << "测试 2: 父对象生命周期（weak_ptr）..." << std::endl;
    
    World world;
    world.Initialize();
    world.RegisterComponent<TransformComponent>();
    
    EntityID childEntity = world.CreateEntity();
    auto& childComp = world.AddComponent<TransformComponent>(childEntity);
    
    {
        // 父实体在作用域内
        EntityID parentEntity = world.CreateEntity();
        auto& parentComp = world.AddComponent<TransformComponent>(parentEntity);
        
        // 设置父对象
        bool success = childComp.SetParent(parentComp.transform);
        if (!success) {
            throw std::runtime_error("Failed to set parent");
        }
        
        // 验证父对象
        if (!childComp.ValidateParent()) {
            throw std::runtime_error("Parent validation failed");
        }
        std::cout << "  ✓ 父对象设置成功" << std::endl;
        
        // 销毁父实体
        world.DestroyEntity(parentEntity);
    }
    
    // 验证父对象已失效
    if (childComp.ValidateParent()) {
        // 应该检测到父对象失效并清除
        if (childComp.GetParent() != nullptr) {
            throw std::runtime_error("Parent pointer not cleared");
        }
    }
    std::cout << "  ✓ 父对象销毁后自动清除" << std::endl;
    
    world.Shutdown();
    std::cout << "  测试 2 通过\n" << std::endl;
}

// 测试 3：父对象生命周期（使用实体 ID）
void TestParentLifetimeWithEntityID() {
    std::cout << "测试 3: 父对象生命周期（实体 ID）..." << std::endl;
    
    World world;
    world.Initialize();
    world.RegisterComponent<TransformComponent>();
    world.RegisterSystem<TransformSystem>();
    
    EntityID parentEntity = world.CreateEntity();
    EntityID childEntity = world.CreateEntity();
    
    auto& parentComp = world.AddComponent<TransformComponent>(parentEntity);
    auto& childComp = world.AddComponent<TransformComponent>(childEntity);
    
    // 设置父实体
    bool success = childComp.SetParentEntity(&world, parentEntity);
    if (!success) {
        throw std::runtime_error("Failed to set parent entity");
    }
    std::cout << "  ✓ 父实体设置成功" << std::endl;
    
    // 更新一帧（同步父子关系）
    world.Update(0.016f);
    
    // 销毁父实体
    world.DestroyEntity(parentEntity);
    
    // 更新一帧（应该检测并清除父子关系）
    world.Update(0.016f);
    
    // 验证父实体已清除
    if (childComp.GetParentEntity().IsValid()) {
        throw std::runtime_error("Parent entity ID not cleared");
    }
    std::cout << "  ✓ 父实体销毁后自动清除" << std::endl;
    
    world.Shutdown();
    std::cout << "  测试 3 通过\n" << std::endl;
}

// 测试 4：验证接口
void TestValidateInterface() {
    std::cout << "测试 4: 验证接口..." << std::endl;
    
    World world;
    world.Initialize();
    world.RegisterComponent<TransformComponent>();
    
    EntityID entity = world.CreateEntity();
    auto& comp = world.AddComponent<TransformComponent>(entity);
    
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
    std::cout << "  ✓ DebugString: " << debugStr << std::endl;
    
    // 获取层级深度
    int depth = comp.GetHierarchyDepth();
    if (depth != 0) {
        throw std::runtime_error("Hierarchy depth should be 0");
    }
    std::cout << "  ✓ 层级深度正确" << std::endl;
    
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
        auto& comp = world.AddComponent<TransformComponent>(entity);
        comp.SetPosition(Vector3(i * 1.0f, 0, 0));
        entities.push_back(entity);
    }
    
    // 更新一帧
    world.Update(0.016f);
    
    // 修改所有实体的位置
    for (const auto& entity : entities) {
        auto& comp = world.GetComponent<TransformComponent>(entity);
        Vector3 pos = comp.GetPosition();
        pos.y() = 10.0f;
        comp.SetPosition(pos);
    }
    
    // 更新一帧（应该批量更新所有 Transform）
    world.Update(0.016f);
    
    // 验证更新成功
    for (const auto& entity : entities) {
        const auto& comp = world.GetComponent<TransformComponent>(entity);
        Vector3 worldPos = comp.GetWorldMatrix().block<3, 1>(0, 3);
        if (std::abs(worldPos.y() - 10.0f) > 0.001f) {
            throw std::runtime_error("Transform not updated correctly");
        }
    }
    std::cout << "  ✓ 批量更新 " << NUM_ENTITIES << " 个 Transform" << std::endl;
    
    world.Shutdown();
    std::cout << "  测试 5 通过\n" << std::endl;
}

int main() {
    try {
        std::cout << "======================================" << std::endl;
        std::cout << "ECS Transform 安全性测试" << std::endl;
        std::cout << "======================================\n" << std::endl;
        
        TestSetParentReturnValue();
        TestParentLifetimeWithWeakPtr();
        TestParentLifetimeWithEntityID();
        TestValidateInterface();
        TestTransformSystemBatchUpdate();
        
        std::cout << "======================================" << std::endl;
        std::cout << "所有测试通过！✓" << std::endl;
        std::cout << "======================================" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n测试失败: " << e.what() << std::endl;
        return 1;
    }
}
```

---

## 集成步骤

### 1. 更新 components.h

根据选择的方案（A 或 B）更新 `include/render/ecs/components.h` 中的 `TransformComponent` 定义。

### 2. 更新 systems.h 和 systems.cpp

根据选择的方案更新 `TransformSystem` 的实现。

### 3. 更新 Transform 类（如果使用方案 B 批量更新）

在 `include/render/transform.h` 中添加 `IsDirty()` 和 `ForceUpdateWorldTransform()` 方法。

### 4. 编译测试

```bash
# 编译测试
mkdir -p build
cd build
cmake ..
cmake --build . --target 36_ecs_transform_safety_test

# 运行测试
./36_ecs_transform_safety_test
```

### 5. 更新现有代码

检查所有使用 `TransformComponent::SetParent` 的地方，确保检查返回值：

```bash
# 搜索使用 SetParent 的地方
grep -r "SetParent(" --include="*.cpp" --include="*.h"
```

### 6. 更新文档

- 更新 `docs/api/Component.md` 添加新接口说明
- 更新 `docs/ECS_INTEGRATION.md` 添加父子关系管理的最佳实践

---

## 后续工作

1. **HierarchySystem 实现**（可选，P3）
   - 创建专门的系统管理场景层级
   - 提供场景图遍历接口
   - 支持层级序列化/反序列化

2. **性能优化**（可选）
   - 测量批量更新的性能提升
   - 考虑使用 dirty 队列避免全量扫描
   - 添加性能统计

3. **文档完善**
   - 添加使用示例到 API 文档
   - 更新 ECS 快速入门指南

---

[上一篇：ECS Transform 安全审查](../ECS_TRANSFORM_SAFETY_REVIEW.md) | [返回文档首页](../README.md)

