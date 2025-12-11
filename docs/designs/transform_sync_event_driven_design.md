# Transform同步到Bullet的事件驱动方案设计文档

## 1. 问题分析

### 1.1 当前问题

在物理引擎迁移到Bullet后，出现了Transform同步失效的问题：

1. **原有架构**：PhysicsWorld直接操作ECS组件，Transform变化在ECS中立即生效
2. **新架构**：PhysicsWorld通过Bullet适配器间接操作，Transform需要同步到Bullet
3. **同步时机问题**：`SyncECSToBullet()`只在PhysicsWorld的`Step()`中被调用，但ECS组件可能在任意时刻被修改
4. **具体场景**：
   - 用户代码在`Step()`调用之间修改了TransformComponent
   - 对于Kinematic/Static物体，这些变化需要立即同步到Bullet
   - 但当前实现只在`Step()`中同步，导致变化丢失

### 1.2 方案选择

根据问题分析文档，方案3（事件驱动同步）是最优解：
- ✅ 实时同步，延迟最低
- ✅ 最符合ECS理念
- ✅ 可以扩展到其他组件
- ⚠️ 需要ECS支持组件变化事件

## 2. 架构设计

### 2.1 设计目标

1. **实时同步**：Transform变化时立即同步到Bullet，无需等待`Step()`
2. **向后兼容**：不破坏现有代码，用户无需修改调用方式
3. **性能优化**：只同步Kinematic/Static物体，Dynamic物体仍由物理模拟驱动
4. **可扩展性**：事件机制可扩展到其他组件类型

### 2.2 核心组件

#### 2.2.1 组件变化事件系统

在ECS架构中添加组件变化事件支持：

```
ComponentRegistry
    ├── 组件存储（现有）
    └── 组件变化事件（新增）
        ├── RegisterComponentChangeCallback<T>()
        ├── OnComponentChanged<T>()
        └── 事件回调列表
```

#### 2.2.2 Transform变化监听

在Transform类中添加变化通知机制：

```
Transform
    ├── SetPosition() / SetRotation() / SetScale()
    └── 变化通知（新增）
        └── OnTransformChanged() 回调
```

#### 2.2.3 物理引擎事件订阅

PhysicsWorld订阅Transform变化事件：

```
PhysicsWorld
    ├── 构造函数：注册事件回调
    ├── OnTransformChanged()：处理变化事件
    └── 立即同步到Bullet
```

## 3. 详细设计

### 3.1 组件变化事件系统

#### 3.1.1 事件类型定义

```cpp
// include/render/ecs/component_events.h

namespace Render::ECS {

/**
 * @brief 组件变化事件基类
 */
struct ComponentChangeEvent {
    EntityID entity;           ///< 发生变化的实体ID
    std::type_index componentType;  ///< 组件类型
    
    ComponentChangeEvent(EntityID e, std::type_index t)
        : entity(e), componentType(t) {}
};

/**
 * @brief TransformComponent变化事件
 */
struct TransformComponentChangeEvent : public ComponentChangeEvent {
    Vector3 position;          ///< 新位置
    Quaternion rotation;       ///< 新旋转
    Vector3 scale;             ///< 新缩放
    
    TransformComponentChangeEvent(EntityID e, 
                                  const Vector3& pos,
                                  const Quaternion& rot,
                                  const Vector3& scl)
        : ComponentChangeEvent(e, std::type_index(typeid(TransformComponent)))
        , position(pos)
        , rotation(rot)
        , scale(scl) {}
};

} // namespace Render::ECS
```

#### 3.1.2 ComponentRegistry扩展

在`ComponentRegistry`中添加事件支持：

```cpp
// include/render/ecs/component_registry.h

class ComponentRegistry {
public:
    // ... 现有接口 ...
    
    // ==================== 组件变化事件（新增）====================
    
    /**
     * @brief 组件变化回调函数类型
     * @tparam T 组件类型
     */
    template<typename T>
    using ComponentChangeCallback = std::function<void(EntityID, const T&)>;
    
    /**
     * @brief 注册组件变化回调
     * @tparam T 组件类型
     * @param callback 回调函数 void(EntityID, const T&)
     * @return 回调ID（用于取消注册）
     * 
     * @note 当组件被修改时，会调用所有注册的回调
     * @note 回调在组件修改后立即调用
     * @note 线程安全：回调在持有写锁的情况下调用，注意避免死锁
     */
    template<typename T>
    uint64_t RegisterComponentChangeCallback(ComponentChangeCallback<T> callback);
    
    /**
     * @brief 取消注册组件变化回调
     * @param callbackId 回调ID
     */
    void UnregisterComponentChangeCallback(uint64_t callbackId);
    
    /**
     * @brief 触发组件变化事件（内部使用）
     * @tparam T 组件类型
     * @param entity 实体ID
     * @param component 组件引用
     */
    template<typename T>
    void OnComponentChanged(EntityID entity, const T& component);

private:
    // 组件变化回调存储
    struct ComponentChangeCallbackRecord {
        uint64_t id;
        std::type_index componentType;
        std::function<void(EntityID, const void*)> callback;  // 类型擦除
    };
    
    std::atomic<uint64_t> m_nextCallbackId{1};
    std::vector<ComponentChangeCallbackRecord> m_componentChangeCallbacks;
    mutable std::mutex m_callbackMutex;
};
```

#### 3.1.3 ComponentArray扩展

在`ComponentArray`中添加变化通知：

```cpp
// include/render/ecs/component_registry.h

template<typename T>
class ComponentArray : public IComponentArray {
public:
    // ... 现有接口 ...
    
    /**
     * @brief 设置变化通知回调
     * @param callback 回调函数 void(EntityID, const T&)
     */
    void SetChangeCallback(std::function<void(EntityID, const T&)> callback) {
        m_changeCallback = std::move(callback);
    }
    
    /**
     * @brief 添加组件（带变化通知）
     */
    void Add(EntityID entity, const T& component) {
        std::unique_lock lock(m_mutex);
        m_components[entity] = component;
        if (m_changeCallback) {
            m_changeCallback(entity, component);
        }
    }
    
    /**
     * @brief 获取组件（可修改，带变化通知）
     */
    T& Get(EntityID entity) {
        std::shared_lock lock(m_mutex);
        auto it = m_components.find(entity);
        if (it == m_components.end()) {
            throw std::out_of_range("Component not found for entity");
        }
        // 注意：这里返回引用，实际修改在外部进行
        // 需要在外部调用OnComponentChanged通知变化
        return it->second;
    }

private:
    std::function<void(EntityID, const T&)> m_changeCallback;
};
```

### 3.2 Transform变化监听

#### 3.2.1 Transform类扩展

在`Transform`类中添加变化回调：

```cpp
// include/render/transform.h

class Transform {
public:
    // ... 现有接口 ...
    
    /**
     * @brief 变化回调函数类型
     */
    using ChangeCallback = std::function<void(const Transform*)>;
    
    /**
     * @brief 设置变化回调
     * @param callback 回调函数
     * 
     * @note 当位置、旋转或缩放发生变化时调用回调
     * @note 回调在持有锁的情况下调用，注意避免死锁
     */
    void SetChangeCallback(ChangeCallback callback);
    
    /**
     * @brief 清除变化回调
     */
    void ClearChangeCallback();

private:
    ChangeCallback m_changeCallback;
    
    // 在SetPosition/SetRotation/SetScale中调用
    void NotifyChanged() {
        if (m_changeCallback) {
            m_changeCallback(this);
        }
    }
};
```

#### 3.2.2 TransformComponent包装

在`TransformComponent`中连接Transform变化到ECS事件：

```cpp
// include/render/ecs/components.h

struct TransformComponent {
    Ref<Transform> transform;
    EntityID parentEntity = EntityID::Invalid();
    
    // ... 现有接口 ...
    
    /**
     * @brief 设置变化通知回调（内部使用）
     * @param callback 回调函数 void(EntityID, const TransformComponent&)
     */
    void SetChangeCallback(std::function<void(EntityID, const TransformComponent&)> callback) {
        if (transform) {
            transform->SetChangeCallback([this, callback](const Transform* t) {
                // 需要从World获取EntityID，这里暂时留空
                // 实际实现需要在World中设置
            });
        }
    }
};
```

### 3.3 物理引擎事件订阅

#### 3.3.1 PhysicsWorld事件订阅

在`PhysicsWorld`构造函数中注册事件回调：

```cpp
// src/physics/physics_world.cpp

PhysicsWorld::PhysicsWorld(ECS::World* ecsWorld, const PhysicsConfig& config)
    : m_ecsWorld(ecsWorld), m_config(config) {
    
#ifdef USE_BULLET_PHYSICS
    m_useBulletBackend = true;
    m_bulletAdapter = std::make_unique<BulletAdapter::BulletWorldAdapter>(config);
    
    // ==================== 注册Transform变化事件回调 ====================
    if (m_ecsWorld) {
        m_transformChangeCallbackId = m_ecsWorld->GetComponentRegistry()
            .RegisterComponentChangeCallback<ECS::TransformComponent>(
                [this](ECS::EntityID entity, const ECS::TransformComponent& transformComp) {
                    OnTransformComponentChanged(entity, transformComp);
                }
            );
    }
    
    // ... 其他初始化 ...
#endif
}

PhysicsWorld::~PhysicsWorld() {
#ifdef USE_BULLET_PHYSICS
    // 取消注册事件回调
    if (m_ecsWorld && m_transformChangeCallbackId != 0) {
        m_ecsWorld->GetComponentRegistry()
            .UnregisterComponentChangeCallback(m_transformChangeCallbackId);
    }
    m_bulletAdapter.reset();
#endif
}
```

#### 3.3.2 变化处理逻辑

```cpp
// src/physics/physics_world.cpp

#ifdef USE_BULLET_PHYSICS

void PhysicsWorld::OnTransformComponentChanged(ECS::EntityID entity, 
                                                 const ECS::TransformComponent& transformComp) {
    if (!m_bulletAdapter || !m_ecsWorld) {
        return;
    }
    
    // 检查实体是否有物理组件
    if (!m_ecsWorld->HasComponent<RigidBodyComponent>(entity)) {
        return;  // 没有物理组件，不需要同步
    }
    
    try {
        auto& body = m_ecsWorld->GetComponent<RigidBodyComponent>(entity);
        
        // 只同步Kinematic/Static物体
        // Dynamic物体由物理模拟驱动，不应从ECS同步
        if (body.type == RigidBodyComponent::BodyType::Kinematic ||
            body.type == RigidBodyComponent::BodyType::Static) {
            
            // 检查是否已在Bullet中
            if (m_bulletAdapter->HasRigidBody(entity)) {
                // 立即同步到Bullet
                Vector3 position = transformComp.GetPosition();
                Quaternion rotation = transformComp.GetRotation();
                
                m_bulletAdapter->SyncTransformToBullet(entity, position, rotation);
            }
            // 如果刚体尚未创建，会在下次Step()中创建
        }
    } catch (...) {
        // 忽略组件访问错误（实体可能已被销毁）
    }
}

#endif
```

### 3.4 World集成

在`World`中连接Transform变化到组件事件：

```cpp
// src/ecs/world.cpp

void World::Initialize() {
    // ... 现有初始化 ...
    
    // ==================== 注册TransformComponent变化监听 ====================
    // 当TransformComponent的Transform对象发生变化时，触发组件变化事件
    auto& registry = GetComponentRegistry();
    
    // 为所有现有的TransformComponent设置变化回调
    registry.ForEachComponent<ECS::TransformComponent>(
        [this](ECS::EntityID entity, ECS::TransformComponent& transformComp) {
            SetupTransformChangeCallback(entity, transformComp);
        }
    );
}

void World::AddComponent(EntityID entity, const ECS::TransformComponent& component) {
    m_componentRegistry.AddComponent(entity, component);
    
    // 为新添加的TransformComponent设置变化回调
    auto& transformComp = m_componentRegistry.GetComponent<ECS::TransformComponent>(entity);
    SetupTransformChangeCallback(entity, transformComp);
}

void World::SetupTransformChangeCallback(ECS::EntityID entity, 
                                          ECS::TransformComponent& transformComp) {
    if (!transformComp.transform) {
        return;
    }
    
    // 设置Transform的变化回调
    transformComp.transform->SetChangeCallback(
        [this, entity](const Transform* transform) {
            // 触发组件变化事件
            if (m_componentRegistry.HasComponent<ECS::TransformComponent>(entity)) {
                auto& transformComp = m_componentRegistry.GetComponent<ECS::TransformComponent>(entity);
                m_componentRegistry.OnComponentChanged(entity, transformComp);
            }
        }
    );
}
```

## 4. 实现步骤

### 4.1 第一阶段：基础事件系统

1. **添加组件变化事件类型**
   - 创建`include/render/ecs/component_events.h`
   - 定义`ComponentChangeEvent`和`TransformComponentChangeEvent`

2. **扩展ComponentRegistry**
   - 添加`RegisterComponentChangeCallback()`方法
   - 添加`OnComponentChanged()`方法
   - 实现回调存储和调用逻辑

3. **扩展ComponentArray**
   - 添加变化回调支持
   - 在`Get()`方法返回后，需要外部调用通知（因为返回的是引用）

### 4.2 第二阶段：Transform变化监听

1. **扩展Transform类**
   - 添加`SetChangeCallback()`方法
   - 在`SetPosition()`、`SetRotation()`、`SetScale()`中调用回调

2. **扩展TransformComponent**
   - 添加变化回调连接逻辑
   - 在World中设置回调

### 4.3 第三阶段：物理引擎集成

1. **PhysicsWorld事件订阅**
   - 在构造函数中注册Transform变化回调
   - 实现`OnTransformComponentChanged()`方法

2. **同步逻辑优化**
   - 只同步Kinematic/Static物体
   - 添加实体有效性检查
   - 处理异常情况

### 4.4 第四阶段：测试和优化

1. **单元测试**
   - 测试事件触发机制
   - 测试同步逻辑
   - 测试边界情况

2. **性能测试**
   - 测量事件系统开销
   - 优化回调调用路径
   - 考虑批量处理优化

## 5. 性能考虑

### 5.1 事件系统开销

- **回调调用**：每次Transform变化都会触发回调，需要确保回调执行快速
- **锁竞争**：事件回调在持有锁的情况下调用，注意避免死锁和性能瓶颈
- **批量优化**：考虑在`Step()`中批量处理变化，减少单次回调开销

### 5.2 优化策略

1. **延迟同步**：收集变化事件，在`Step()`开始时批量同步
2. **变化过滤**：只同步实际发生变化的值（位置/旋转/缩放）
3. **实体过滤**：只监听有物理组件的实体的Transform变化

## 6. 向后兼容性

### 6.1 API兼容

- ✅ 所有现有API保持不变
- ✅ 用户代码无需修改
- ✅ 事件系统是可选功能，不影响现有代码

### 6.2 行为兼容

- ✅ Kinematic/Static物体：立即同步（新行为）
- ✅ Dynamic物体：仍由物理模拟驱动（保持原行为）
- ✅ 无物理组件的实体：不触发同步（保持原行为）

## 7. 扩展性

### 7.1 其他组件支持

事件系统可以扩展到其他组件：

```cpp
// 示例：RigidBodyComponent变化事件
m_ecsWorld->GetComponentRegistry()
    .RegisterComponentChangeCallback<ECS::RigidBodyComponent>(
        [this](ECS::EntityID entity, const ECS::RigidBodyComponent& body) {
            OnRigidBodyComponentChanged(entity, body);
        }
    );
```

### 7.2 多物理引擎支持

事件系统不依赖特定物理引擎，可以支持：
- Bullet Physics（当前）
- PhysX（未来）
- 自定义物理引擎

## 8. 测试用例

### 8.1 基本同步测试

```cpp
// 测试Kinematic物体Transform变化立即同步
TEST(PhysicsWorld, TransformSync_Kinematic_Immediate) {
    auto world = CreateTestWorld();
    auto entity = world->CreateEntity();
    
    // 添加物理组件
    world->AddComponent<RigidBodyComponent>(entity, {
        .type = RigidBodyComponent::BodyType::Kinematic
    });
    world->AddComponent<TransformComponent>(entity);
    
    // 修改Transform
    auto& transform = world->GetComponent<TransformComponent>(entity);
    transform.SetPosition({1.0f, 2.0f, 3.0f});
    
    // 验证立即同步到Bullet
    auto* physicsWorld = world->GetSystem<PhysicsWorld>();
    ASSERT_TRUE(physicsWorld->IsTransformSynced(entity));
}
```

### 8.2 批量变化测试

```cpp
// 测试多次Transform变化只同步一次
TEST(PhysicsWorld, TransformSync_Batch) {
    // ... 设置 ...
    
    // 连续修改Transform
    transform.SetPosition({1.0f, 0.0f, 0.0f});
    transform.SetRotation(Quaternion::Identity());
    transform.SetScale({1.0f, 1.0f, 1.0f});
    
    // 验证只同步一次（如果实现了批量优化）
}
```

## 9. 风险评估

### 9.1 技术风险

1. **性能风险**：事件系统可能增加开销
   - **缓解**：实现批量处理和变化过滤

2. **死锁风险**：回调在持有锁的情况下调用
   - **缓解**：使用递归锁，避免在回调中再次获取锁

3. **生命周期风险**：实体销毁时回调可能访问无效数据
   - **缓解**：在回调中检查实体有效性

### 9.2 兼容性风险

1. **现有代码破坏**：事件系统可能影响现有代码
   - **缓解**：事件系统是可选功能，默认不启用

2. **行为变化**：同步时机变化可能导致行为差异
   - **缓解**：保持向后兼容，只优化Kinematic/Static物体

## 10. 总结

本设计方案通过事件驱动机制实现Transform到Bullet的实时同步，解决了当前同步失效的问题。方案具有以下特点：

1. **实时性**：Transform变化立即同步，无需等待`Step()`
2. **兼容性**：完全向后兼容，用户代码无需修改
3. **性能**：只同步必要的物体，优化回调路径
4. **扩展性**：事件系统可扩展到其他组件

实施建议：
- 分阶段实施，逐步验证
- 充分测试，确保稳定性
- 性能监控，及时优化

