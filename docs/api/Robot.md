# 机器人系统 API 参考

[返回 API 目录](README.md)

---

## 📋 概述

机器人系统提供了完整的URDF机器人加载、控制和渲染功能。通过ECS架构，支持：

- ✅ URDF文件加载和解析
- ✅ 机器人模型构建（Link、Joint）
- ✅ 关节控制（位置、速度、力矩）
- ✅ 物理约束集成（Bullet Physics）
- ✅ TF（Transform）可视化
- ✅ 正向运动学计算

**命名空间**：`Render::ECS::Robot`

**头文件**：
- `<render/robot/robot_components.h>` - 机器人组件定义
- `<render/robot/robot_systems.h>` - 机器人系统定义
- `<render/robot/robot_control_system.h>` - 关节控制系统
- `<render/robot/urdf_loader.h>` - URDF加载器

**最后更新**：2026-01-12

---

## 🚀 快速开始

### 1. 包含头文件

```cpp
#include "render/robot/robot_systems.h"
#include "render/robot/robot_control_system.h"
#include "render/ecs/world.h"
#include "render/ecs/systems.h"
```

### 2. 注册组件和系统

```cpp
using namespace Render;
using namespace Render::ECS;
using namespace Render::ECS::Robot;

// 创建 ECS 世界
World world;
world.Initialize();

// 注册机器人组件
world.RegisterComponent<RobotComponent>();
world.RegisterComponent<JointComponent>();
world.RegisterComponent<LinkComponent>();

// 注册机器人系统
auto* urdfLoadSystem = world.RegisterSystem<URDFLoadSystem>();
auto* robotControlSystem = world.RegisterSystem<RobotControlSystem>();
auto* jointTFSystem = world.RegisterSystem<JointTFSystem>();
auto* robotRenderSystem = world.RegisterSystem<RobotRenderSystem>();
```

### 3. 加载URDF机器人

```cpp
// 加载URDF文件
std::string urdfPath = "robots/my_robot.urdf";
std::string meshBasePath = "robots/meshes/";
EntityID robotEntity = urdfLoadSystem->LoadRobot(urdfPath, meshBasePath);

if (!robotEntity.IsValid()) {
    Logger::GetInstance().Error("Failed to load robot");
    return;
}

Logger::GetInstance().InfoFormat("Robot loaded as entity %u", robotEntity.index);
```

### 4. 控制关节

```cpp
// 设置关节控制模式为位置控制
robotControlSystem->SetJointControlMode(robotEntity, "joint1", 
                                       JointComponent::ControlMode::Position);

// 设置目标位置（弧度）
robotControlSystem->SetJointTargetPosition(robotEntity, "joint1", 1.57f);  // 90度

// 设置控制参数
EntityID jointEntity = robotControlSystem->GetJointEntity(robotEntity, "joint1");
if (jointEntity.IsValid() && world.HasComponent<JointComponent>(jointEntity)) {
    auto& jointComp = world.GetComponent<JointComponent>(jointEntity);
    jointComp.positionKp = 50.0f;   // 位置增益
    jointComp.positionKd = 1.0f;    // 阻尼增益
    jointComp.maxTorque = 100.0f;    // 最大力矩
}
```

### 5. 查询关节状态

```cpp
// 获取关节位置
float position = robotControlSystem->GetJointPosition(robotEntity, "joint1");

// 获取关节速度
float velocity = robotControlSystem->GetJointVelocity(robotEntity, "joint1");
```

---

## 🧩 核心组件

### RobotComponent

机器人根组件，每个机器人实体都有一个 `RobotComponent`。

```cpp
struct RobotComponent {
    Ref<Render::Robot::RobotModel> model;  // 机器人模型
    EntityID baseLinkEntity;                // 基座实体ID
    std::unordered_map<std::string, EntityID> linkEntityMap;  // link名称 -> 实体ID映射
    
    RobotComponent();
};
```

**字段说明**：

- `model` - 机器人模型对象，包含所有link和joint的定义
- `baseLinkEntity` - 基座link的实体ID
- `linkEntityMap` - link名称到实体ID的映射表

**使用示例**：

```cpp
if (world.HasComponent<RobotComponent>(robotEntity)) {
    const auto& robotComp = world.GetComponent<RobotComponent>(robotEntity);
    if (robotComp.model) {
        // 遍历所有关节
        for (const auto& [jointName, joint] : robotComp.model->joints) {
            Logger::GetInstance().InfoFormat("Joint: %s", jointName.c_str());
        }
    }
}
```

---

### JointComponent

关节组件，每个关节实体都有一个 `JointComponent`。

```cpp
struct JointComponent {
    std::string jointName;              // 关节名称
    std::string parentLink;              // 父link名称
    std::string childLink;               // 子link名称
    Render::Robot::JointType type;       // 关节类型
    
    float position = 0.0f;               // 关节位置（角度或距离）
    Vector3 axis;                        // 关节轴
    Render::Robot::JointLimits limits;   // 关节限制
    
    EntityID parentLinkEntity;           // 父link实体ID
    EntityID childLinkEntity;            // 子link实体ID
    
    // ==================== 关节控制 ====================
    
    enum class ControlMode {
        Position,    ///< 位置控制（使用PD控制器）
        Velocity,    ///< 速度控制（使用P控制器）
        Torque       ///< 力矩控制（直接施加力矩）
    };
    
    ControlMode controlMode = ControlMode::Position;  ///< 当前控制模式
    
    // 控制目标值
    float targetPosition = 0.0f;   ///< 目标位置（角度或距离）
    float targetVelocity = 0.0f;   ///< 目标速度（rad/s 或 m/s）
    float targetTorque = 0.0f;     ///< 目标力矩（N·m 或 N）
    
    // 位置控制参数（PD控制器）
    float positionKp = 100.0f;      ///< 位置比例增益
    float positionKd = 10.0f;      ///< 位置微分增益
    
    // 速度控制参数（P控制器）
    float velocityKp = 50.0f;      ///< 速度比例增益
    
    // 力矩限制
    float maxTorque = 100.0f;      ///< 最大力矩限制（N·m 或 N）
    
    // 当前状态（由RobotControlSystem更新）
    float currentPosition = 0.0f;  ///< 当前位置（角度或距离）
    float currentVelocity = 0.0f;  ///< 当前速度（rad/s 或 m/s）
    
    JointComponent();
};
```

**控制模式说明**：

- **Position（位置控制）**：使用PD控制器，根据位置误差计算目标速度，然后通过物理约束的马达功能驱动关节到达目标位置。
- **Velocity（速度控制）**：使用P控制器，根据速度误差计算目标力矩，直接控制关节速度。
- **Torque（力矩控制）**：直接对刚体施加力矩，完全由用户控制。

**控制参数调优建议**：

- `positionKp`：位置增益，值越大响应越快，但可能产生抖动。推荐范围：10-100
- `positionKd`：阻尼增益，值越大阻尼越大，减少抖动。推荐范围：1-20
- `velocityKp`：速度增益，推荐范围：10-50
- `maxTorque`：最大力矩限制，根据机器人大小和重量调整

**使用示例**：

```cpp
// 设置关节控制参数
EntityID jointEntity = robotControlSystem->GetJointEntity(robotEntity, "joint1");
if (jointEntity.IsValid() && world.HasComponent<JointComponent>(jointEntity)) {
    auto& jointComp = world.GetComponent<JointComponent>(jointEntity);
    
    // 设置为位置控制模式
    jointComp.controlMode = JointComponent::ControlMode::Position;
    
    // 设置控制参数
    jointComp.positionKp = 50.0f;
    jointComp.positionKd = 1.0f;
    jointComp.maxTorque = 100.0f;
    
    // 设置目标位置
    jointComp.targetPosition = 1.57f;  // 90度
}
```

---

### LinkComponent

连杆组件，每个link实体都有一个 `LinkComponent`。

```cpp
struct LinkComponent {
    std::string linkName;                    // link名称
    std::vector<std::string> childJoints;     // 子关节名称列表
    Ref<Mesh> visualMesh;                     // 视觉网格（可选）
    std::vector<Ref<Mesh>> visualMeshes;      // 所有视觉网格
    bool isBaseLink = false;                  // 是否为基座link
    
    LinkComponent();
};
```

**字段说明**：

- `linkName` - link名称
- `childJoints` - 子关节名称列表
- `visualMesh` - 主要的视觉网格（向后兼容）
- `visualMeshes` - 所有视觉网格的列表
- `isBaseLink` - 是否为基座link

---

## 🔧 系统

### URDFLoadSystem

URDF加载系统，负责加载URDF文件并创建机器人实体。

**优先级**：5（高优先级）

#### 类定义

```cpp
class URDFLoadSystem : public System {
public:
    EntityID LoadRobot(const std::string& urdfPath, 
                      const std::string& meshBasePath = "", 
                      RigidBodyType baseLinkType = RigidBodyType::Kinematic);
    
    void UnloadRobot(EntityID robotEntity);
    bool IsRobotLoaded(EntityID robotEntity) const;
};
```

#### 方法说明

##### `LoadRobot()`

加载URDF文件并创建机器人实体。

```cpp
EntityID LoadRobot(const std::string& urdfPath, 
                  const std::string& meshBasePath = "", 
                  RigidBodyType baseLinkType = RigidBodyType::Kinematic);
```

**参数**：

- `urdfPath` - URDF文件路径
- `meshBasePath` - mesh文件基准路径（如果为空，使用URDF文件所在目录）
- `baseLinkType` - base link的刚体类型（默认Kinematic）

**返回值**：机器人实体ID，失败返回 `EntityID::Invalid()`

**说明**：

- 自动创建所有link和joint实体
- 自动创建物理组件（RigidBody、Collider、Constraint）
- 自动设置Transform父子关系
- base link默认使用Kinematic类型，可以设置为Dynamic或Static

**示例**：

```cpp
// 加载机器人
EntityID robotEntity = urdfLoadSystem->LoadRobot(
    "robots/ur5e.urdf",
    "robots/meshes/",
    RigidBodyType::Kinematic  // base link类型
);

if (!robotEntity.IsValid()) {
    Logger::GetInstance().Error("Failed to load robot");
    return;
}
```

##### `UnloadRobot()`

卸载机器人。

```cpp
void UnloadRobot(EntityID robotEntity);
```

**参数**：

- `robotEntity` - 机器人实体ID

**说明**：会删除机器人实体及其所有子实体（link、joint等）

##### `IsRobotLoaded()`

检查机器人是否已加载。

```cpp
bool IsRobotLoaded(EntityID robotEntity) const;
```

**参数**：

- `robotEntity` - 机器人实体ID

**返回值**：如果已加载返回true，否则返回false

---

### RobotControlSystem

机器人控制系统，负责实现关节的位置控制、速度控制和力矩控制。

**优先级**：10（在JointTFSystem之后，PhysicsSystem之前）

#### 类定义

```cpp
class RobotControlSystem : public System {
public:
    // 控制模式设置
    void SetJointControlMode(EntityID robotEntity, const std::string& jointName, 
                            JointComponent::ControlMode mode);
    
    // 目标值设置
    void SetJointTargetPosition(EntityID robotEntity, const std::string& jointName, 
                                float position);
    void SetJointTargetVelocity(EntityID robotEntity, const std::string& jointName, 
                                float velocity);
    void SetJointTargetTorque(EntityID robotEntity, const std::string& jointName, 
                              float torque);
    
    // 批量设置
    void SetJointTargetPositions(EntityID robotEntity, 
                                 const std::unordered_map<std::string, float>& positions);
    void SetJointTargetVelocities(EntityID robotEntity, 
                                  const std::unordered_map<std::string, float>& velocities);
    void SetJointTargetTorques(EntityID robotEntity, 
                               const std::unordered_map<std::string, float>& torques);
    
    // 状态查询
    float GetJointPosition(EntityID robotEntity, const std::string& jointName) const;
    float GetJointVelocity(EntityID robotEntity, const std::string& jointName) const;
    EntityID GetJointEntity(EntityID robotEntity, const std::string& jointName);
    EntityID FindJointEntity(EntityID robotEntity, const std::string& jointName) const;
};
```

#### 方法说明

##### `SetJointControlMode()`

设置关节控制模式。

```cpp
void SetJointControlMode(EntityID robotEntity, const std::string& jointName, 
                        JointComponent::ControlMode mode);
```

**参数**：

- `robotEntity` - 机器人实体ID
- `jointName` - 关节名称
- `mode` - 控制模式（Position、Velocity、Torque）

**示例**：

```cpp
// 设置为位置控制
robotControlSystem->SetJointControlMode(robotEntity, "shoulder_joint", 
                                       JointComponent::ControlMode::Position);

// 设置为速度控制
robotControlSystem->SetJointControlMode(robotEntity, "elbow_joint", 
                                       JointComponent::ControlMode::Velocity);

// 设置为力矩控制
robotControlSystem->SetJointControlMode(robotEntity, "wrist_joint", 
                                       JointComponent::ControlMode::Torque);
```

##### `SetJointTargetPosition()`

设置关节目标位置。

```cpp
void SetJointTargetPosition(EntityID robotEntity, const std::string& jointName, 
                            float position);
```

**参数**：

- `robotEntity` - 机器人实体ID
- `jointName` - 关节名称
- `position` - 目标位置（旋转关节：弧度，平移关节：米）

**说明**：

- 目标位置会被限制在关节的 `limits.lower` 和 `limits.upper` 范围内
- 只有在位置控制模式下才会生效

**示例**：

```cpp
// 设置关节到90度位置
robotControlSystem->SetJointTargetPosition(robotEntity, "shoulder_joint", 
                                          MathUtils::DegreesToRadians(90.0f));
```

##### `SetJointTargetVelocity()`

设置关节目标速度。

```cpp
void SetJointTargetVelocity(EntityID robotEntity, const std::string& jointName, 
                            float velocity);
```

**参数**：

- `robotEntity` - 机器人实体ID
- `jointName` - 关节名称
- `velocity` - 目标速度（旋转关节：rad/s，平移关节：m/s）

**说明**：只有在速度控制模式下才会生效

**示例**：

```cpp
// 设置关节以1 rad/s的速度旋转
robotControlSystem->SetJointTargetVelocity(robotEntity, "shoulder_joint", 1.0f);
```

##### `SetJointTargetTorque()`

设置关节目标力矩。

```cpp
void SetJointTargetTorque(EntityID robotEntity, const std::string& jointName, 
                          float torque);
```

**参数**：

- `robotEntity` - 机器人实体ID
- `jointName` - 关节名称
- `torque` - 目标力矩（旋转关节：N·m，平移关节：N）

**说明**：

- 目标力矩会被限制在 `-maxTorque` 和 `maxTorque` 范围内
- 只有在力矩控制模式下才会生效

**示例**：

```cpp
// 设置关节施加10 N·m的力矩
robotControlSystem->SetJointTargetTorque(robotEntity, "shoulder_joint", 10.0f);
```

##### `SetJointTargetPositions()`

批量设置多个关节的目标位置。

```cpp
void SetJointTargetPositions(EntityID robotEntity, 
                             const std::unordered_map<std::string, float>& positions);
```

**参数**：

- `robotEntity` - 机器人实体ID
- `positions` - 关节名称到位置的映射

**示例**：

```cpp
std::unordered_map<std::string, float> positions;
positions["shoulder_joint"] = 0.5f;
positions["elbow_joint"] = 1.0f;
positions["wrist_joint"] = 0.3f;

robotControlSystem->SetJointTargetPositions(robotEntity, positions);
```

##### `SetJointTargetVelocities()`

批量设置多个关节的目标速度。

```cpp
void SetJointTargetVelocities(EntityID robotEntity, 
                              const std::unordered_map<std::string, float>& velocities);
```

**参数**：

- `robotEntity` - 机器人实体ID
- `velocities` - 关节名称到速度的映射

##### `SetJointTargetTorques()`

批量设置多个关节的目标力矩。

```cpp
void SetJointTargetTorques(EntityID robotEntity, 
                           const std::unordered_map<std::string, float>& torques);
```

**参数**：

- `robotEntity` - 机器人实体ID
- `torques` - 关节名称到力矩的映射

##### `GetJointPosition()`

获取关节当前位置。

```cpp
float GetJointPosition(EntityID robotEntity, const std::string& jointName) const;
```

**参数**：

- `robotEntity` - 机器人实体ID
- `jointName` - 关节名称

**返回值**：关节位置（旋转关节：弧度，平移关节：米），如果关节不存在返回0.0f

**说明**：返回的是 `JointComponent::currentPosition`，由系统每帧从物理系统更新

**示例**：

```cpp
float position = robotControlSystem->GetJointPosition(robotEntity, "shoulder_joint");
Logger::GetInstance().InfoFormat("Joint position: %.3f rad (%.1f deg)", 
                                 position, MathUtils::RadiansToDegrees(position));
```

##### `GetJointVelocity()`

获取关节当前速度。

```cpp
float GetJointVelocity(EntityID robotEntity, const std::string& jointName) const;
```

**参数**：

- `robotEntity` - 机器人实体ID
- `jointName` - 关节名称

**返回值**：关节速度（旋转关节：rad/s，平移关节：m/s），如果关节不存在返回0.0f

**说明**：返回的是 `JointComponent::currentVelocity`，由系统每帧从物理系统更新

##### `GetJointEntity()`

获取关节实体ID（可能更新缓存）。

```cpp
EntityID GetJointEntity(EntityID robotEntity, const std::string& jointName);
```

**参数**：

- `robotEntity` - 机器人实体ID
- `jointName` - 关节名称

**返回值**：关节实体ID，如果不存在返回 `EntityID::Invalid()`

**说明**：此方法会更新内部缓存，因此不是const方法

##### `FindJointEntity()`

查找关节实体ID（不更新缓存，const版本）。

```cpp
EntityID FindJointEntity(EntityID robotEntity, const std::string& jointName) const;
```

**参数**：

- `robotEntity` - 机器人实体ID
- `jointName` - 关节名称

**返回值**：关节实体ID，如果不存在返回 `EntityID::Invalid()`

**说明**：此方法不会更新缓存，适合在const方法中使用

---

### JointTFSystem

关节变换系统，负责计算正向运动学（Forward Kinematics）。

**优先级**：5（高优先级）

#### 类定义

```cpp
class JointTFSystem : public System {
public:
    void SetJointPositions(EntityID robotEntity, 
                          const std::unordered_map<std::string, float>& positions);
    float GetJointPosition(EntityID robotEntity, const std::string& jointName) const;
};
```

#### 方法说明

##### `SetJointPositions()`

设置关节位置（用于正向运动学计算）。

```cpp
void SetJointPositions(EntityID robotEntity, 
                      const std::unordered_map<std::string, float>& positions);
```

**参数**：

- `robotEntity` - 机器人实体ID
- `positions` - 关节名称到位置的映射

**说明**：设置关节位置后，系统会自动计算所有link的Transform

##### `GetJointPosition()`

获取关节位置。

```cpp
float GetJointPosition(EntityID robotEntity, const std::string& jointName) const;
```

**参数**：

- `robotEntity` - 机器人实体ID
- `jointName` - 关节名称

**返回值**：关节位置，如果关节不存在返回0.0f

---

### RobotRenderSystem

机器人渲染系统，负责渲染机器人模型和TF可视化。

**优先级**：200（在普通渲染系统之后）

#### 类定义

```cpp
class RobotRenderSystem : public System {
public:
    void RenderRobots(Renderer* renderer, const Camera& camera);
    TFVisualizer& GetTFVisualizer();
    const TFVisualizer& GetTFVisualizer() const;
};
```

#### 方法说明

##### `RenderRobots()`

渲染机器人。

```cpp
void RenderRobots(Renderer* renderer, const Camera& camera);
```

**参数**：

- `renderer` - 渲染器指针
- `camera` - 相机对象

**说明**：渲染所有机器人的link和joint

##### `GetTFVisualizer()`

获取TF可视化器。

```cpp
TFVisualizer& GetTFVisualizer();
const TFVisualizer& GetTFVisualizer() const;
```

**返回值**：TF可视化器引用

**说明**：TF可视化器用于显示坐标系的轴（X=红色，Y=绿色，Z=蓝色）

---

## 📝 使用示例

### 完整示例：加载和控制机器人

```cpp
#include "render/robot/robot_systems.h"
#include "render/robot/robot_control_system.h"
#include "render/ecs/world.h"
#include "render/ecs/systems.h"
#include "render/math_utils.h"

using namespace Render;
using namespace Render::ECS;
using namespace Render::ECS::Robot;

int main() {
    // 创建世界
    World world;
    world.Initialize();
    
    // 注册组件
    world.RegisterComponent<RobotComponent>();
    world.RegisterComponent<JointComponent>();
    world.RegisterComponent<LinkComponent>();
    
    // 注册系统
    auto* urdfLoadSystem = world.RegisterSystem<URDFLoadSystem>();
    auto* robotControlSystem = world.RegisterSystem<RobotControlSystem>();
    auto* jointTFSystem = world.RegisterSystem<JointTFSystem>();
    
    world.PostInitialize();
    
    // 加载机器人
    EntityID robotEntity = urdfLoadSystem->LoadRobot(
        "robots/ur5e.urdf",
        "robots/meshes/"
    );
    
    if (!robotEntity.IsValid()) {
        Logger::GetInstance().Error("Failed to load robot");
        return -1;
    }
    
    // 获取机器人组件
    if (world.HasComponent<RobotComponent>(robotEntity)) {
        const auto& robotComp = world.GetComponent<RobotComponent>(robotEntity);
        
        if (robotComp.model) {
            // 配置所有旋转关节为位置控制
            for (const auto& [jointName, joint] : robotComp.model->joints) {
                if (joint.type == Render::Robot::JointType::Revolute) {
                    // 设置控制模式
                    robotControlSystem->SetJointControlMode(
                        robotEntity, jointName, 
                        JointComponent::ControlMode::Position
                    );
                    
                    // 设置初始位置（中间位置）
                    float midPosition = (joint.limits.lower + joint.limits.upper) * 0.5f;
                    robotControlSystem->SetJointTargetPosition(robotEntity, jointName, midPosition);
                    
                    // 设置控制参数
                    EntityID jointEntity = robotControlSystem->GetJointEntity(robotEntity, jointName);
                    if (jointEntity.IsValid() && world.HasComponent<JointComponent>(jointEntity)) {
                        auto& jointComp = world.GetComponent<JointComponent>(jointEntity);
                        jointComp.positionKp = 50.0f;
                        jointComp.positionKd = 1.0f;
                        jointComp.maxTorque = 100.0f;
                    }
                }
            }
        }
    }
    
    // 主循环
    float time = 0.0f;
    while (running) {
        float deltaTime = GetDeltaTime();
        time += deltaTime;
        
        // 更新系统
        world.Update(deltaTime);
        
        // 控制关节（示例：正弦波运动）
        if (world.HasComponent<RobotComponent>(robotEntity)) {
            const auto& robotComp = world.GetComponent<RobotComponent>(robotEntity);
            if (robotComp.model) {
                for (const auto& [jointName, joint] : robotComp.model->joints) {
                    if (joint.type == Render::Robot::JointType::Revolute) {
                        // 正弦波目标位置
                        float amplitude = (joint.limits.upper - joint.limits.lower) * 0.3f;
                        float frequency = 0.5f;  // 0.5 Hz
                        float targetPosition = (joint.limits.lower + joint.limits.upper) * 0.5f +
                                             amplitude * std::sin(time * frequency * 2.0f * 3.14159f);
                        
                        robotControlSystem->SetJointTargetPosition(robotEntity, jointName, targetPosition);
                    }
                }
            }
        }
        
        // 查询关节状态
        float position = robotControlSystem->GetJointPosition(robotEntity, "shoulder_joint");
        float velocity = robotControlSystem->GetJointVelocity(robotEntity, "shoulder_joint");
        
        Logger::GetInstance().InfoFormat(
            "Joint position: %.3f rad, velocity: %.3f rad/s",
            position, velocity
        );
    }
    
    return 0;
}
```

### 示例：切换控制模式

```cpp
// 切换到位置控制
robotControlSystem->SetJointControlMode(robotEntity, "joint1", 
                                        JointComponent::ControlMode::Position);
robotControlSystem->SetJointTargetPosition(robotEntity, "joint1", 1.57f);

// 切换到速度控制
robotControlSystem->SetJointControlMode(robotEntity, "joint1", 
                                        JointComponent::ControlMode::Velocity);
robotControlSystem->SetJointTargetVelocity(robotEntity, "joint1", 1.0f);

// 切换到力矩控制
robotControlSystem->SetJointControlMode(robotEntity, "joint1", 
                                        JointComponent::ControlMode::Torque);
robotControlSystem->SetJointTargetTorque(robotEntity, "joint1", 10.0f);
```

### 示例：批量控制多个关节

```cpp
// 批量设置位置
std::unordered_map<std::string, float> positions;
positions["shoulder_joint"] = 0.5f;
positions["elbow_joint"] = 1.0f;
positions["wrist_joint"] = 0.3f;

robotControlSystem->SetJointTargetPositions(robotEntity, positions);
```

---

## ⚠️ 注意事项

### 1. 系统执行顺序

机器人系统的执行顺序很重要：

- `URDFLoadSystem` (优先级5) - 加载机器人
- `JointTFSystem` (优先级5) - 计算正向运动学
- `RobotControlSystem` (优先级10) - 控制关节
- `PhysicsSystem` (优先级15) - 物理模拟
- `RobotRenderSystem` (优先级200) - 渲染

### 2. 控制参数调优

位置控制的抖动问题通常由以下原因引起：

- **增益过大**：`positionKp` 或 `positionKd` 太大
- **速度限制过小**：物理约束的motor速度限制太小
- **时间步长不稳定**：物理模拟的时间步长不稳定

**推荐参数**：

- `positionKp`: 10-100（根据机器人大小调整）
- `positionKd`: 1-20（阻尼增益，减少抖动）
- `velocityKp`: 10-50
- `maxTorque`: 根据机器人大小和重量调整

### 3. 物理约束集成

关节控制通过物理约束的马达功能实现，因此：

- 确保物理系统已正确初始化
- 确保约束已正确创建（由 `URDFLoadSystem` 自动创建）
- 位置控制使用PD控制器计算目标速度，然后通过约束马达驱动

### 4. 关节状态更新

关节的 `currentPosition` 和 `currentVelocity` 由 `RobotControlSystem` 每帧从物理系统更新：

- 优先从物理约束获取（最准确）
- 如果没有约束，从 `JointTFSystem` 获取

### 5. URDF文件要求

URDF文件必须符合ROS URDF标准：

- 支持 `<robot>`、`<link>`、`<joint>` 标签
- 支持 `<visual>`、`<collision>`、`<inertial>` 子标签
- 支持mesh文件引用（STL、OBJ等格式）

---

## 🔗 相关文档

- [ECS系统](ECS.md) - ECS架构概述
- [物理系统](Physics.md) - Bullet Physics集成
- [组件系统](Component.md) - 组件API参考

---

## 📚 参考

- [ROS URDF文档](http://wiki.ros.org/urdf)
- [Bullet Physics文档](https://pybullet.org/Bullet/BulletFull/index.html)
