# TransformComponent变化回调连接方案

## 问题背景

在World集成Transform变化事件系统时，发现Transform变化没有正确触发组件变化事件。测试显示：
- `Test_World_AddTransformComponent_AutoSetupCallback` 失败
- `Test_World_TransformChange_TriggersComponentEvent` 失败

## 解决方案

在`TransformComponent`中添加变化回调连接辅助方法，封装回调设置逻辑，确保回调正确连接。

## 实现细节

### 1. 在TransformComponent中添加方法

**头文件**：`include/render/ecs/components.h`

```cpp
/**
 * @brief 连接Transform变化回调到组件变化事件
 * 
 * 设置Transform的变化回调，当Transform变化时自动触发ComponentRegistry的组件变化事件。
 * 
 * @param entity 实体ID
 * @param worldPtr World的shared_ptr（用于回调中的生命周期管理）
 * @return 如果成功设置回调返回 true，如果transform为空返回 false
 */
bool ConnectChangeCallback(EntityID entity, std::shared_ptr<World> worldPtr);

/**
 * @brief 清除Transform的变化回调
 */
void DisconnectChangeCallback();
```

### 2. 实现方法

**实现文件**：`src/ecs/components.cpp`

`ConnectChangeCallback`方法封装了原来在`World::SetupTransformChangeCallback`中的逻辑：
- 检查transform和worldPtr的有效性
- 设置Transform的变化回调
- 回调中检查实体和组件的有效性
- 触发ComponentRegistry的组件变化事件

`DisconnectChangeCallback`方法简单地调用`Transform::ClearChangeCallback()`。

### 3. 更新World使用新方法

**文件**：`src/ecs/world.cpp`

- `SetupTransformChangeCallback`现在简单地调用`transformComp.ConnectChangeCallback(entity, worldPtr)`
- `DestroyEntity`和`RemoveComponent`中使用`DisconnectChangeCallback()`替代直接调用`ClearChangeCallback()`

## 优势

1. **封装性更好**：回调设置逻辑集中在TransformComponent中，而不是分散在World中
2. **更容易维护**：如果需要修改回调逻辑，只需要修改一个地方
3. **更清晰的接口**：TransformComponent提供了明确的API来管理变化回调
4. **可能解决问题**：通过集中管理回调设置，可以更容易发现和修复问题

## 可能解决的问题

1. **回调设置时机问题**：通过封装的方法，确保回调在正确的时机设置
2. **生命周期管理**：使用shared_ptr确保World在回调执行时仍然有效
3. **错误处理**：在ConnectChangeCallback中添加了更详细的错误检查和日志

## 测试建议

1. 运行失败的测试，确认问题是否解决
2. 添加调试日志，确认回调是否正确设置和调用
3. 检查回调执行时的实体和组件有效性检查

## 后续优化

如果问题仍然存在，可以考虑：
1. 在ConnectChangeCallback中添加更详细的日志
2. 检查OnComponentChanged的实现
3. 验证回调注册的时机是否正确

