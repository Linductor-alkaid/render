# RenderEngine v0.1.1 发行说明

**发布日期：** 2026年1月13日  
**版本：** 0.1.1  
**构建：** Release-x64

---

## 🎉 RenderEngine v0.1.1 更新

RenderEngine v0.1.1 是一个小版本更新，主要增强了机器人仿真系统的控制能力，新增了动力学与运动学控制模式支持。

## ✨ v0.1.1 新特性

### 机器人控制系统增强

#### 动力学控制模式
- **位置控制模式** - 使用 PD 控制器，通过物理约束实现精确的位置控制
- **速度控制模式** - 使用 P 控制器，通过物理约束实现速度控制
- **力矩控制模式** - 直接施加力矩，通过物理约束实现力矩控制
- **控制参数可配置** - 支持设置位置增益（Kp）、阻尼增益（Kd）、速度增益和最大力矩限制

#### 运动学控制模式
- **运动学控制模式** - 直接设置关节位置和速度，绕过物理模拟，适用于快速运动学计算
- **正向运动学支持** - 运动学模式下自动更新机器人变换（TF）
- **速度积分** - 支持通过目标速度自动积分计算位置

#### 控制模式切换
- **运行时切换** - 支持在运行时动态切换关节控制模式
- **状态查询** - 可查询关节当前位置、速度和目标值
- **统一接口** - 提供统一的 API 接口控制不同模式

## 📝 API 更新

### 新增方法

```cpp
// 设置关节控制模式
robotControlSystem->SetJointControlMode(robotEntity, "joint1", 
                                       JointComponent::ControlMode::Position);

// 设置目标位置（位置控制模式）
robotControlSystem->SetJointTargetPosition(robotEntity, "joint1", 1.57f);

// 设置目标速度（速度控制模式）
robotControlSystem->SetJointTargetVelocity(robotEntity, "joint1", 0.5f);

// 设置目标力矩（力矩控制模式）
robotControlSystem->SetJointTargetTorque(robotEntity, "joint1", 10.0f);
```

### 控制模式枚举

```cpp
enum class ControlMode {
    Position,    ///< 位置控制（使用PD控制器，通过物理约束）
    Velocity,    ///< 速度控制（使用P控制器，通过物理约束）
    Torque,      ///< 力矩控制（直接施加力矩，通过物理约束）
    Kinematic    ///< 运动学控制（直接设置位置/速度，不通过物理模拟）
};
```

## 🔄 兼容性

- **向后兼容** - 完全兼容 v0.1.0 的 API 和功能
- **默认行为** - 关节默认使用位置控制模式，保持与 v0.1.0 一致的行为
- **URDF 支持** - 继续支持所有 URDF 机器人模型

## 📚 文档更新

- **API 文档** - 更新了 `docs/api/Robot.md`，包含详细的控制模式使用说明

## 🐛 修复

- 修复了关节控制中约束与刚体同步的问题
- 改进了运动学模式下的 TF 更新性能

### 使用示例

```cpp
#include "render/robot/robot_control_system.h"

// 设置关节为位置控制模式
robotControlSystem->SetJointControlMode(robotEntity, "shoulder_joint", 
                                       JointComponent::ControlMode::Position);

// 设置目标位置
robotControlSystem->SetJointTargetPosition(robotEntity, "shoulder_joint", 1.57f);

// 或者使用运动学模式进行快速计算
robotControlSystem->SetJointControlMode(robotEntity, "shoulder_joint", 
                                       JointComponent::ControlMode::Kinematic);
robotControlSystem->SetJointTargetPosition(robotEntity, "shoulder_joint", 1.57f);
```

更多示例请查看源代码仓库的 `examples/` 目录。

---

**下载：** 
- [RenderEngine-prebuilt-Release-x64-Static.zip](RenderEngine-prebuilt-Release-x64-Static.zip)（静态库版本）

**文档：** 参见 `docs/` 目录  
**示例：** 参见源代码仓库的 `examples/` 目录

**祝使用愉快！🚀**
