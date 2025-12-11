# 调试日志添加说明

## 问题

测试显示Transform变化回调已连接，但组件变化事件未被触发。

## 添加的调试日志

### 1. Transform变化回调日志 (`src/ecs/components.cpp`)

- **回调被调用时**：记录"Transform change callback invoked for entity X"
- **实体无效时**：记录"Entity X is invalid, skipping component change event"
- **组件不存在时**：记录"Entity X has no TransformComponent, skipping component change event"
- **调用OnComponentChanged前**：记录"Calling OnComponentChanged for entity X"
- **异常捕获**：记录详细的异常信息

### 2. ComponentRegistry日志 (`include/render/ecs/component_registry.h`)

- **OnComponentChanged被调用时**：
  - 记录实体ID
  - 记录组件类型名称
  - 记录总回调数量
  
- **找到匹配回调时**：记录匹配的回调数量

- **调用每个回调时**：
  - 记录回调索引
  - 记录实体ID
  - 如果回调为空，记录警告
  - 如果回调抛出异常，记录详细异常信息

## 如何使用这些日志

运行测试后，查看日志输出，可以确定：

1. **Transform变化回调是否被调用**
   - 如果看到"Transform change callback invoked"，说明Transform变化确实触发了回调
   - 如果没有看到，说明Transform变化没有触发回调

2. **实体/组件检查是否通过**
   - 如果看到"Entity X is invalid"或"Entity X has no TransformComponent"，说明检查失败
   - 这可能是问题的根源

3. **OnComponentChanged是否被调用**
   - 如果看到"OnComponentChanged called"，说明回调成功调用了OnComponentChanged
   - 如果没有看到，说明回调在检查阶段就返回了

4. **回调列表是否为空**
   - 查看"total callbacks"和"Found X matching callbacks"
   - 如果匹配的回调数量为0，说明没有注册组件变化回调，或者类型不匹配

5. **回调是否被正确调用**
   - 如果看到"Invoking callback"，说明回调被调用了
   - 如果没有看到，说明回调列表为空或类型不匹配

## 可能的问题场景

### 场景1：Transform变化回调未被调用
- **现象**：没有看到"Transform change callback invoked"
- **原因**：Transform的SetPosition等方法没有正确触发回调
- **解决**：检查Transform的实现

### 场景2：实体/组件检查失败
- **现象**：看到"Entity X is invalid"或"Entity X has no TransformComponent"
- **原因**：实体已被销毁或组件已被移除
- **解决**：检查实体和组件的生命周期管理

### 场景3：OnComponentChanged未被调用
- **现象**：看到"Transform change callback invoked"但没有看到"OnComponentChanged called"
- **原因**：回调在检查阶段就返回了
- **解决**：检查实体和组件的有效性

### 场景4：回调列表为空
- **现象**：看到"OnComponentChanged called"但"Found 0 matching callbacks"
- **原因**：没有注册组件变化回调，或者类型不匹配
- **解决**：检查回调注册的时机和类型匹配

### 场景5：回调未被调用
- **现象**：看到"Found X matching callbacks"但没有看到"Invoking callback"
- **原因**：回调列表为空或回调为空
- **解决**：检查回调注册逻辑

## 下一步

运行测试，查看日志输出，根据上述场景定位问题。

