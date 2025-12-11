# Transform同步到Bullet的事件驱动方案 Todolist

> **版本**: v1.0  
> **日期**: 2025-12-11  
> **基于**: `docs/designs/transform_sync_event_driven_design.md`  
> **目标**: 实现事件驱动的Transform同步机制，解决Transform变化到Bullet同步失效问题

---

## 目录

1. [阶段一：基础事件系统](#阶段一基础事件系统)
2. [阶段二：Transform变化监听](#阶段二transform变化监听)
3. [阶段三：物理引擎集成](#阶段三物理引擎集成)
4. [阶段四：测试和优化](#阶段四测试和优化)

---

## 阶段一：基础事件系统

**优先级**: 🔴 Critical  
**预计时间**: 1-2 周  
**依赖**: 无

### 1.1 组件变化事件类型定义

- [x] **1.1.1** 创建组件事件头文件
  - [x] 创建 `include/render/ecs/component_events.h`
  - [x] 添加文件头注释和版权信息
  - [x] 包含必要的头文件（`entity.h`, `types.h`, `<typeindex>`）

- [x] **1.1.2** 定义组件变化事件基类
  - [x] 定义 `ComponentChangeEvent` 结构体
    - [x] 包含 `EntityID entity` 成员
    - [x] 包含 `std::type_index componentType` 成员
    - [x] 添加构造函数
  - [x] 编写文档注释

- [x] **1.1.3** 定义TransformComponent变化事件
  - [x] 定义 `TransformComponentChangeEvent` 结构体
    - [x] 继承自 `ComponentChangeEvent`
    - [x] 包含 `Vector3 position` 成员
    - [x] 包含 `Quaternion rotation` 成员
    - [x] 包含 `Vector3 scale` 成员
    - [x] 添加构造函数
  - [x] 编写文档注释

**验证标准**:
- ✅ 文件可以编译
- ✅ 事件类型可以正确构造
- ✅ 类型信息正确存储

---

### 1.2 ComponentRegistry扩展

- [x] **1.2.1** 添加回调存储结构
  - [x] 在 `ComponentRegistry` 私有区域添加：
    - [x] `ComponentChangeCallbackRecord` 结构体
      - [x] `uint64_t id` 成员
      - [x] `std::type_index componentType` 成员
      - [x] `std::function<void(EntityID, const void*)> callback` 成员（类型擦除）
    - [x] `std::atomic<uint64_t> m_nextCallbackId{1}` 成员
    - [x] `std::vector<ComponentChangeCallbackRecord> m_componentChangeCallbacks` 成员
    - [x] `mutable std::mutex m_callbackMutex` 成员

- [x] **1.2.2** 实现回调注册接口
  - [x] 添加 `RegisterComponentChangeCallback<T>()` 模板方法
    - [x] 参数：`ComponentChangeCallback<T> callback`
    - [x] 返回：`uint64_t` 回调ID
    - [x] 实现逻辑：
      - [x] 生成唯一回调ID
      - [x] 创建类型擦除的回调包装
      - [x] 存储到回调列表
      - [x] 返回回调ID
    - [x] 添加线程安全保护（使用 `m_callbackMutex`）
    - [x] 编写文档注释

- [x] **1.2.3** 实现回调取消注册接口
  - [x] 添加 `UnregisterComponentChangeCallback(uint64_t callbackId)` 方法
    - [x] 从回调列表中移除指定ID的回调
    - [x] 添加线程安全保护
    - [x] 编写文档注释

- [x] **1.2.4** 实现组件变化通知接口
  - [x] 添加 `OnComponentChanged<T>(EntityID entity, const T& component)` 模板方法
    - [x] 查找所有匹配组件类型的回调
    - [x] 调用每个回调（类型安全的转换）
    - [x] 添加线程安全保护
    - [x] 处理回调异常（避免一个回调失败影响其他回调）
    - [x] 编写文档注释

**验证标准**:
- ✅ 回调可以正确注册和取消注册
- ✅ 组件变化时回调被正确调用
- ✅ 线程安全测试通过
- ✅ 异常处理正确

---

### 1.3 ComponentArray扩展

- [x] **1.3.1** 添加变化回调支持
  - [x] 在 `ComponentArray<T>` 私有区域添加：
    - [x] `std::function<void(EntityID, const T&)> m_changeCallback` 成员

- [x] **1.3.2** 实现变化回调设置接口
  - [x] 添加 `SetChangeCallback(std::function<void(EntityID, const T&)> callback)` 方法
    - [x] 存储回调函数
    - [x] 编写文档注释
  - [x] 添加 `ClearChangeCallback()` 方法

- [x] **1.3.3** 修改Add方法支持变化通知
  - [x] 在 `Add(EntityID entity, const T& component)` 中：
    - [x] 保持原有逻辑
    - [x] 如果设置了回调，在添加后调用回调
    - [x] 注意：回调在持有写锁的情况下调用
  - [x] 在 `Add(EntityID entity, T&& component)` 中也添加回调支持

- [x] **1.3.4** 处理Get方法的变化通知
  - [x] 注意：`Get()` 返回引用，实际修改在外部
  - [x] 考虑方案：
    - [x] 方案A：在 `ComponentRegistry::GetComponent()` 中检测变化（需要比较旧值）
    - [x] 方案B：要求外部在修改后调用 `OnComponentChanged()`（不推荐）
    - [x] 方案C：在 `Transform` 类中直接触发事件（推荐，见阶段二）
  - [x] 选择方案C，本阶段暂不实现Get的变化通知

**验证标准**:
- ✅ Add组件时回调被正确调用
- ✅ 回调可以正确设置和清除
- ✅ 线程安全测试通过

---

### 1.4 单元测试

- [x] **1.4.1** 测试事件类型定义
  - [x] 创建测试文件 `tests/test_component_events_system.cpp`（统一测试文件）
  - [x] 测试 `ComponentChangeEvent` 构造
  - [x] 测试 `TransformComponentChangeEvent` 构造
  - [x] 验证类型信息正确
  - [x] 测试继承关系

- [x] **1.4.2** 测试ComponentRegistry回调机制
  - [x] 创建测试文件 `tests/test_component_events_system.cpp`（统一测试文件）
  - [x] 测试回调注册
  - [x] 测试回调调用
  - [x] 测试回调取消注册
  - [x] 测试多个回调
  - [x] 测试线程安全
  - [x] 测试类型安全
  - [x] 测试异常处理

- [x] **1.4.3** 测试ComponentArray变化通知
  - [x] 测试Add时的变化通知
  - [x] 测试回调设置和清除
  - [x] 测试移动语义
  - [x] 测试异常处理

**验证标准**:
- ✅ 所有单元测试通过
- ✅ 代码覆盖率 > 80%

---

## 阶段二：Transform变化监听

**优先级**: 🔴 Critical  
**预计时间**: 1-2 周  
**依赖**: 阶段一完成

### 2.1 Transform类扩展

- [x] **2.1.1** 添加变化回调支持
  - [x] 在 `Transform` 类私有区域添加：
    - [x] `using ChangeCallback = std::function<void(const Transform*)>` 类型别名（在公共接口中定义）
    - [x] `ChangeCallback changeCallback` 成员（在ColdData中）
    - [x] 考虑线程安全（使用 `m_coldData->dataMutex` 保护）

- [x] **2.1.2** 实现变化回调设置接口
  - [x] 添加 `SetChangeCallback(ChangeCallback callback)` 方法
    - [x] 存储回调函数
    - [x] 添加线程安全保护
    - [x] 编写文档注释

- [x] **2.1.3** 实现变化回调清除接口
  - [x] 添加 `ClearChangeCallback()` 方法
    - [x] 清除回调函数
    - [x] 添加线程安全保护
    - [x] 编写文档注释

- [x] **2.1.4** 实现变化通知辅助方法
  - [x] 添加私有方法 `NotifyChanged()`
    - [x] 检查回调是否设置
    - [x] 如果设置，调用回调
    - [x] 注意：在持有锁的情况下调用，避免死锁
    - [x] 处理回调异常

- [x] **2.1.5** 在SetPosition中触发通知
  - [x] 修改 `SetPosition(const Vector3& position)` 方法
    - [x] 保持原有逻辑
    - [x] 在位置实际变化后调用回调
    - [x] 注意：只在值真正变化时通知（避免重复通知）

- [x] **2.1.6** 在SetRotation中触发通知
  - [x] 修改 `SetRotation(const Quaternion& rotation)` 方法
    - [x] 保持原有逻辑
    - [x] 在旋转实际变化后调用回调
    - [x] 注意：只在值真正变化时通知

- [x] **2.1.7** 在SetScale中触发通知
  - [x] 修改 `SetScale(const Vector3& scale)` 方法
    - [x] 保持原有逻辑
    - [x] 在缩放实际变化后调用回调
    - [x] 注意：只在值真正变化时通知
  - [x] `SetScale(float)` 方法调用 `SetScale(Vector3)`，已包含通知逻辑

- [x] **2.1.8** 在其他修改方法中触发通知
  - [x] 检查 `Translate()`, `Rotate()`, `SetFromMatrix()` 等方法
  - [x] 在适当位置添加回调调用
  - [x] 确保不重复通知
  - [x] 已修改的方法：
    - [x] `Translate()` - 添加通知
    - [x] `TranslateWorld()` - 添加通知
    - [x] `Rotate()` - 添加通知
    - [x] `RotateAround()` - 添加通知
    - [x] `RotateAroundWorld()` - 添加通知
    - [x] `SetRotationEuler()` - 添加通知
    - [x] `SetRotationEulerDegrees()` - 添加通知
    - [x] `SetFromMatrix()` - 添加通知
    - [x] `LookAt()` - 添加通知

**验证标准**:
- ✅ Transform变化时回调被正确调用
- ✅ 只在值真正变化时通知
- ✅ 线程安全测试通过
- ✅ 不会导致死锁

---

### 2.2 World集成

- [ ] **2.2.1** 添加Transform变化回调设置方法
  - [ ] 在 `World` 类中添加私有方法：
    - [ ] `SetupTransformChangeCallback(EntityID entity, TransformComponent& transformComp)`
    - [ ] 实现逻辑：
      - [ ] 检查 `transformComp.transform` 是否有效
      - [ ] 设置Transform的变化回调
      - [ ] 回调中触发组件变化事件：
        - [ ] 检查实体是否仍有TransformComponent
        - [ ] 调用 `ComponentRegistry::OnComponentChanged()`

- [ ] **2.2.2** 在Initialize中设置现有组件回调
  - [ ] 修改 `World::Initialize()` 方法
    - [ ] 在初始化完成后，遍历所有现有的TransformComponent
    - [ ] 为每个组件调用 `SetupTransformChangeCallback()`

- [ ] **2.2.3** 在AddComponent中设置新组件回调
  - [ ] 修改 `World::AddComponent<TransformComponent>()` 方法
    - [ ] 在添加组件后
    - [ ] 调用 `SetupTransformChangeCallback()` 设置回调

- [ ] **2.2.4** 在RemoveComponent中清理回调
  - [ ] 修改 `World::RemoveComponent<TransformComponent>()` 方法
    - [ ] 在移除组件前
    - [ ] 清除Transform的变化回调（调用 `ClearChangeCallback()`）

- [ ] **2.2.5** 在DestroyEntity中清理回调
  - [ ] 修改 `World::DestroyEntity()` 方法
    - [ ] 在销毁实体时
    - [ ] 如果实体有TransformComponent，清除回调

**验证标准**:
- ✅ 新添加的TransformComponent自动设置回调
- ✅ 移除组件时回调被正确清理
- ✅ 实体销毁时回调被正确清理
- ✅ 不会导致内存泄漏

---

### 2.3 TransformComponent扩展（可选）

- [ ] **2.3.1** 添加变化回调连接辅助方法（可选）
  - [ ] 如果需要，在 `TransformComponent` 中添加：
    - [ ] `SetChangeCallback(std::function<void(EntityID, const TransformComponent&)> callback)` 方法
    - [ ] 注意：需要EntityID，实际在World中设置更合适
  - [ ] 如果不需要，跳过此任务

**验证标准**:
- ✅ 如果实现，确保正确连接Transform和组件事件

---

### 2.4 单元测试

- [ ] **2.4.1** 测试Transform变化回调
  - [ ] 创建测试文件 `tests/test_transform_change_callback.cpp`
  - [ ] 测试回调设置和清除
  - [ ] 测试SetPosition触发回调
  - [ ] 测试SetRotation触发回调
  - [ ] 测试SetScale触发回调
  - [ ] 测试只在值变化时触发
  - [ ] 测试线程安全

- [ ] **2.4.2** 测试World集成
  - [ ] 创建测试文件 `tests/test_world_transform_events.cpp`
  - [ ] 测试添加TransformComponent时自动设置回调
  - [ ] 测试移除组件时清理回调
  - [ ] 测试实体销毁时清理回调
  - [ ] 测试Transform变化触发组件事件

**验证标准**:
- ✅ 所有单元测试通过
- ✅ 代码覆盖率 > 80%

---

## 阶段三：物理引擎集成

**优先级**: 🔴 Critical  
**预计时间**: 1 周  
**依赖**: 阶段一、阶段二完成

### 3.1 PhysicsWorld事件订阅

- [ ] **3.1.1** 添加回调ID存储
  - [ ] 在 `PhysicsWorld` 类私有区域添加：
    - [ ] `uint64_t m_transformChangeCallbackId = 0` 成员

- [ ] **3.1.2** 在构造函数中注册事件回调
  - [ ] 修改 `PhysicsWorld::PhysicsWorld()` 构造函数
    - [ ] 在 `#ifdef USE_BULLET_PHYSICS` 块中
    - [ ] 检查 `m_ecsWorld` 是否有效
    - [ ] 调用 `m_ecsWorld->GetComponentRegistry().RegisterComponentChangeCallback<ECS::TransformComponent>()`
    - [ ] 回调函数调用 `OnTransformComponentChanged()`
    - [ ] 存储回调ID到 `m_transformChangeCallbackId`

- [ ] **3.1.3** 在析构函数中取消注册
  - [ ] 修改 `PhysicsWorld::~PhysicsWorld()` 析构函数
    - [ ] 在 `#ifdef USE_BULLET_PHYSICS` 块中
    - [ ] 检查 `m_ecsWorld` 和 `m_transformChangeCallbackId` 是否有效
    - [ ] 调用 `UnregisterComponentChangeCallback()` 取消注册

**验证标准**:
- ✅ 构造函数中回调正确注册
- ✅ 析构函数中回调正确取消注册
- ✅ 不会导致内存泄漏

---

### 3.2 变化处理逻辑

- [ ] **3.2.1** 实现OnTransformComponentChanged方法
  - [ ] 在 `PhysicsWorld` 类中添加私有方法：
    - [ ] `OnTransformComponentChanged(EntityID entity, const TransformComponent& transformComp)`
    - [ ] 实现逻辑：
      - [ ] 检查 `m_bulletAdapter` 和 `m_ecsWorld` 是否有效
      - [ ] 检查实体是否有 `RigidBodyComponent`
      - [ ] 如果没有，直接返回
      - [ ] 获取 `RigidBodyComponent`
      - [ ] 检查刚体类型：
        - [ ] 如果是 `Kinematic` 或 `Static`，继续处理
        - [ ] 如果是 `Dynamic`，直接返回（由物理模拟驱动）
      - [ ] 检查刚体是否已在Bullet中创建
      - [ ] 如果已创建，立即同步Transform到Bullet
      - [ ] 如果未创建，跳过（会在下次Step中创建）
      - [ ] 使用try-catch处理异常

- [ ] **3.2.2** 添加实体有效性检查
  - [ ] 在 `OnTransformComponentChanged()` 中：
    - [ ] 使用 `m_ecsWorld->IsValidEntity(entity)` 检查实体有效性
    - [ ] 如果无效，直接返回

- [ ] **3.2.3** 优化同步逻辑
  - [ ] 考虑只同步实际变化的值（位置/旋转）
  - [ ] 考虑批量优化（收集变化，在Step中批量同步）
  - [ ] 当前阶段：实现立即同步
  - [ ] 批量优化留到阶段四

**验证标准**:
- ✅ Kinematic物体Transform变化立即同步
- ✅ Static物体Transform变化立即同步
- ✅ Dynamic物体不触发同步
- ✅ 无物理组件的实体不触发同步
- ✅ 异常情况正确处理

---

### 3.3 调试和日志

- [ ] **3.3.1** 添加调试日志
  - [ ] 在 `OnTransformComponentChanged()` 中添加日志：
    - [ ] 记录同步的实体ID
    - [ ] 记录同步的位置和旋转
    - [ ] 使用适当的日志级别（Debug）
    - [ ] 可以通过编译选项控制日志输出

- [ ] **3.3.2** 添加性能统计（可选）
  - [ ] 统计同步次数
  - [ ] 统计同步耗时
  - [ ] 在调试模式下输出统计信息

**验证标准**:
- ✅ 日志信息有助于调试
- ✅ 性能统计准确

---

### 3.4 集成测试

- [ ] **3.4.1** 测试基本同步功能
  - [ ] 创建测试文件 `tests/test_physics_world_transform_sync.cpp`
  - [ ] 测试Kinematic物体Transform变化立即同步
  - [ ] 测试Static物体Transform变化立即同步
  - [ ] 测试Dynamic物体不触发同步
  - [ ] 测试无物理组件的实体不触发同步

- [ ] **3.4.2** 测试边界情况
  - [ ] 测试实体销毁时的处理
  - [ ] 测试组件移除时的处理
  - [ ] 测试多次快速变化
  - [ ] 测试并发修改

- [ ] **3.4.3** 测试原有功能不受影响
  - [ ] 运行现有的物理引擎测试
  - [ ] 确保所有测试通过
  - [ ] 验证行为兼容性

**验证标准**:
- ✅ 所有集成测试通过
- ✅ 原有功能不受影响
- ✅ 行为向后兼容

---

## 阶段四：测试和优化

**优先级**: 🟡 High  
**预计时间**: 1-2 周  
**依赖**: 阶段一、阶段二、阶段三完成

### 4.1 性能测试

- [ ] **4.1.1** 测量事件系统开销
  - [ ] 创建性能测试用例
  - [ ] 测量回调调用开销
  - [ ] 测量锁竞争开销
  - [ ] 测量内存使用
  - [ ] 记录基准数据

- [ ] **4.1.2** 优化回调调用路径
  - [ ] 分析性能瓶颈
  - [ ] 优化回调查找（考虑使用unordered_map按类型索引）
  - [ ] 优化类型擦除开销
  - [ ] 减少不必要的锁竞争

- [ ] **4.1.3** 实现批量处理优化（可选）
  - [ ] 考虑延迟同步方案：
    - [ ] 收集变化事件到队列
    - [ ] 在Step开始时批量处理
    - [ ] 减少单次同步开销
  - [ ] 评估批量处理的收益
  - [ ] 如果收益明显，实现批量处理

**验证标准**:
- ✅ 事件系统开销可接受（< 1% CPU）
- ✅ 性能测试通过
- ✅ 优化后性能提升明显

---

### 4.2 变化过滤优化

- [ ] **4.2.1** 实现变化值过滤
  - [ ] 在 `OnTransformComponentChanged()` 中：
    - [ ] 比较新旧位置/旋转值
    - [ ] 只在值真正变化时同步
    - [ ] 注意：Transform类已经做了变化检测，这里可以简化

- [ ] **4.2.2** 实现实体过滤优化
  - [ ] 考虑只监听有物理组件的实体
    - [ ] 在注册回调时过滤
    - [ ] 或在回调中快速返回
  - [ ] 评估过滤的收益

**验证标准**:
- ✅ 不必要的同步被过滤
- ✅ 性能有所提升

---

### 4.3 完整测试套件

- [ ] **4.3.1** 编写完整测试用例
  - [ ] 基本同步测试（见设计文档8.1）
  - [ ] 批量变化测试（见设计文档8.2）
  - [ ] 边界情况测试
  - [ ] 并发测试
  - [ ] 性能测试

- [ ] **4.3.2** 运行所有测试
  - [ ] 单元测试
  - [ ] 集成测试
  - [ ] 性能测试
  - [ ] 确保所有测试通过

- [ ] **4.3.3** 代码覆盖率
  - [ ] 运行代码覆盖率分析
  - [ ] 确保覆盖率 > 85%
  - [ ] 补充缺失的测试用例

**验证标准**:
- ✅ 所有测试通过
- ✅ 代码覆盖率 > 85%
- ✅ 测试用例覆盖所有场景

---

### 4.4 文档更新

- [ ] **4.4.1** 更新API文档
  - [ ] 更新 `ComponentRegistry` API文档
  - [ ] 更新 `Transform` API文档
  - [ ] 更新 `PhysicsWorld` API文档
  - [ ] 添加事件系统使用示例

- [ ] **4.4.2** 更新用户指南
  - [ ] 说明事件驱动同步机制
  - [ ] 说明何时需要手动同步
  - [ ] 添加使用示例

- [ ] **4.4.3** 更新迁移指南
  - [ ] 说明从旧方案迁移的注意事项
  - [ ] 说明兼容性保证

**验证标准**:
- ✅ 文档完整准确
- ✅ 示例代码可运行

---

### 4.5 代码审查和清理

- [ ] **4.5.1** 代码审查
  - [ ] 审查所有新增代码
  - [ ] 检查代码风格一致性
  - [ ] 检查错误处理
  - [ ] 检查线程安全

- [ ] **4.5.2** 代码清理
  - [ ] 移除调试代码
  - [ ] 移除未使用的代码
  - [ ] 优化代码结构
  - [ ] 添加必要的注释

- [ ] **4.5.3** 最终验证
  - [ ] 运行完整测试套件
  - [ ] 验证性能指标
  - [ ] 验证内存使用
  - [ ] 准备发布

**验证标准**:
- ✅ 代码质量符合项目标准
- ✅ 所有测试通过
- ✅ 性能指标达标

---

## 总结

### 完成标准

所有阶段完成后，应该达到以下标准：

1. **功能完整性**
   - ✅ Transform变化时立即同步到Bullet
   - ✅ 只同步Kinematic/Static物体
   - ✅ Dynamic物体仍由物理模拟驱动
   - ✅ 向后兼容，用户代码无需修改

2. **代码质量**
   - ✅ 代码覆盖率 > 85%
   - ✅ 所有测试通过
   - ✅ 代码审查通过
   - ✅ 文档完整

3. **性能指标**
   - ✅ 事件系统开销 < 1% CPU
   - ✅ 同步延迟 < 1ms
   - ✅ 内存使用合理

4. **稳定性**
   - ✅ 无内存泄漏
   - ✅ 无死锁风险
   - ✅ 异常处理完善

### 风险评估

- **技术风险**: 中等
  - 事件系统可能增加开销 → 通过性能测试和优化缓解
  - 死锁风险 → 通过仔细设计锁机制缓解
  - 生命周期管理复杂 → 通过充分测试缓解

- **兼容性风险**: 低
  - 事件系统是可选功能，不影响现有代码
  - 行为变化只影响Kinematic/Static物体，且是优化

### 后续工作

完成本todolist后，可以考虑：

1. **扩展到其他组件**
   - RigidBodyComponent变化事件
   - ColliderComponent变化事件

2. **批量优化**
   - 实现延迟同步机制
   - 批量处理变化事件

3. **性能监控**
   - 添加性能监控工具
   - 实时监控同步性能

---

**最后更新**: 2025-12-11  
**状态**: 🟡 进行中

