# 机器人关节控制设计方案

## 问题分析

当前实现中，`RobotControlSystem` 直接对刚体施加力矩来控制关节，但关节有物理约束（`ConstraintComponent`），约束会限制运动，导致控制效果不佳或无效。

## 解决方案

应该通过物理约束的马达功能来控制关节，而不是绕过约束直接对刚体施加力矩。

## 设计步骤

### 1. 扩展 ConstraintComponent

在 `ConstraintComponent` 中添加马达控制字段：

```cpp
struct ConstraintComponent {
    // ... 现有字段 ...
    
    // 马达控制（用于关节驱动）
    bool useMotor = false;              ///< 是否启用马达
    float motorTargetVelocity = 0.0f;  ///< 目标速度（rad/s 或 m/s）
    float motorMaxForce = 0.0f;        ///< 最大马达力/力矩（N 或 N·m）
    
    // 位置控制（用于位置控制模式）
    bool usePositionControl = false;   ///< 是否启用位置控制
    float targetPosition = 0.0f;       ///< 目标位置（角度或距离）
    float positionKp = 100.0f;         ///< 位置比例增益
    float positionKd = 10.0f;          ///< 位置微分增益
};
```

### 2. 扩展 PhysicsSystem

在 `PhysicsSystem` 中实现约束马达功能：

#### 2.1 创建约束时设置马达

在 `CreateConstraint` 方法中，对于 Hinge 约束，如果启用了马达，调用：
- `btHingeConstraint::enableAngularMotor()`
- `btHingeConstraint::setMotorTargetVelocity()`
- `btHingeConstraint::setMaxMotorImpulse()`

#### 2.2 更新约束马达

在 `UpdateConstraints` 方法中，每帧更新马达参数：
- 检查 `useMotor` 标志
- 更新 `setMotorTargetVelocity()`
- 更新 `setMaxMotorImpulse()`

#### 2.3 位置控制实现

对于位置控制模式：
- 计算位置误差：`error = targetPosition - currentPosition`
- 计算速度误差：`velocityError = -currentVelocity`
- 使用PD控制器计算目标速度：`targetVelocity = Kp * error + Kd * velocityError`
- 将目标速度设置为马达目标速度

### 3. 修改 RobotControlSystem

#### 3.1 查找约束

通过 `JointComponent` 的 `childLinkEntity` 查找约束：
- 查询 child link 上的 `ConstraintComponent`
- 验证约束的 `connectedEntity` 是否为 parent link

#### 3.2 通过约束控制

根据控制模式设置约束参数：

**位置控制模式**：
```cpp
constraint.usePositionControl = true;
constraint.targetPosition = targetPosition;
constraint.positionKp = jointComp.positionKp;
constraint.positionKd = jointComp.positionKd;
```

**速度控制模式**：
```cpp
constraint.useMotor = true;
constraint.motorTargetVelocity = targetVelocity;
constraint.motorMaxForce = jointComp.maxTorque;
```

**力矩控制模式**：
- 计算目标速度（基于力矩和当前速度）
- 或直接对刚体施加力矩（如果约束允许）

### 4. 实现细节

#### 4.1 获取关节状态

从约束中获取实际关节位置和速度：
- Hinge约束：`btHingeConstraint::getHingeAngle()` 获取角度
- Hinge约束：`btHingeConstraint::getHingeAngleRate()` 获取角速度

#### 4.2 坐标系转换

注意关节轴的方向和约束轴的方向需要一致。

## 实现优先级

1. **P0**: 扩展 `ConstraintComponent`，添加马达字段
2. **P0**: 在 `PhysicsSystem` 中实现 Hinge 约束的马达功能
3. **P1**: 修改 `RobotControlSystem`，通过约束控制关节
4. **P1**: 实现位置控制模式（通过PD控制器计算目标速度）
5. **P2**: 实现从约束读取关节状态
6. **P2**: 支持 Generic6Dof 约束的马达（用于 Prismatic 关节）

## 注意事项

1. **约束创建时机**：约束可能在控制系统更新后才创建，需要处理这种情况
2. **坐标系一致性**：确保关节轴和约束轴方向一致
3. **性能考虑**：每帧更新约束参数，但Bullet内部会优化
4. **约束类型**：当前主要支持 Hinge 约束，Generic6Dof 需要特殊处理

## 参考

- Bullet Physics: `btHingeConstraint::enableAngularMotor()`
- Bullet Physics: `btHingeConstraint::setMotorTargetVelocity()`
- Bullet Physics: `btHingeConstraint::setMaxMotorImpulse()`
