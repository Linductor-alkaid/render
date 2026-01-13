/*
 * Copyright (c) 2025 Li Chaoyu
 * 
 * This file is part of Render.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * For commercial licensing, please contact: 2052046346@qq.com
 */

#include "render/robot/robot_control_system.h"
#include "render/robot/robot_components.h"
#include "render/robot/joint_tf_system.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/ecs/physics/physics_components.h"
#include "render/logger.h"
#include "render/math_utils.h"
#include <btBulletDynamicsCommon.h>
#include <algorithm>

namespace Render {
namespace ECS {
namespace Robot {

void RobotControlSystem::Update(float deltaTime) {
    if (!m_world) return;
    
    // 获取所有机器人实体
    auto robots = m_world->Query<RobotComponent>();
    
    for (const auto& robotEntity : robots) {
        const auto& robotComp = m_world->GetComponent<RobotComponent>(robotEntity);
        if (!robotComp.model) {
            continue;
        }
        
        // 遍历所有关节
        for (const auto& [jointName, jointInfo] : robotComp.model->joints) {
            // 获取关节实体ID
            EntityID jointEntity = GetJointEntity(robotEntity, jointName);
            if (!jointEntity.IsValid() || !m_world->HasComponent<JointComponent>(jointEntity)) {
                continue;
            }
            
            auto& jointComp = m_world->GetComponent<JointComponent>(jointEntity);
            
            // 跳过固定关节和未知类型关节
            if (jointComp.type == Render::Robot::JointType::Fixed ||
                jointComp.type == Render::Robot::JointType::Unknown) {
                continue;
            }
            
            // 更新关节状态（从物理系统读取）
            UpdateJointState(jointEntity, jointComp);
            
            // 应用控制输出
            ApplyControl(jointEntity, jointComp, deltaTime);
        }
    }
}

void RobotControlSystem::SetJointControlMode(EntityID robotEntity, const std::string& jointName, 
                                              JointComponent::ControlMode mode) {
    if (!m_world) return;
    
    EntityID jointEntity = GetJointEntity(robotEntity, jointName);
    if (!jointEntity.IsValid() || !m_world->HasComponent<JointComponent>(jointEntity)) {
        Logger::GetInstance().WarningFormat(
            "[RobotControlSystem] Joint '%s' not found for robot entity %u",
            jointName.c_str(), robotEntity.index
        );
        return;
    }
    
    auto& jointComp = m_world->GetComponent<JointComponent>(jointEntity);
    jointComp.controlMode = mode;
}

void RobotControlSystem::SetJointTargetPosition(EntityID robotEntity, const std::string& jointName, 
                                                 float position) {
    if (!m_world) return;
    
    EntityID jointEntity = GetJointEntity(robotEntity, jointName);
    if (!jointEntity.IsValid() || !m_world->HasComponent<JointComponent>(jointEntity)) {
        Logger::GetInstance().WarningFormat(
            "[RobotControlSystem] Joint '%s' not found for robot entity %u",
            jointName.c_str(), robotEntity.index
        );
        return;
    }
    
    auto& jointComp = m_world->GetComponent<JointComponent>(jointEntity);
    jointComp.targetPosition = position;
    
    // 限制在关节限制范围内
    if (jointComp.limits.lower < jointComp.limits.upper) {
        jointComp.targetPosition = std::clamp(
            jointComp.targetPosition,
            jointComp.limits.lower,
            jointComp.limits.upper
        );
    }
}

void RobotControlSystem::SetJointTargetVelocity(EntityID robotEntity, const std::string& jointName, 
                                                 float velocity) {
    if (!m_world) return;
    
    EntityID jointEntity = GetJointEntity(robotEntity, jointName);
    if (!jointEntity.IsValid() || !m_world->HasComponent<JointComponent>(jointEntity)) {
        Logger::GetInstance().WarningFormat(
            "[RobotControlSystem] Joint '%s' not found for robot entity %u",
            jointName.c_str(), robotEntity.index
        );
        return;
    }
    
    auto& jointComp = m_world->GetComponent<JointComponent>(jointEntity);
    jointComp.targetVelocity = velocity;
    
    // 限制在最大速度范围内
    if (jointComp.limits.velocity > 0.0f) {
        jointComp.targetVelocity = std::clamp(
            jointComp.targetVelocity,
            -jointComp.limits.velocity,
            jointComp.limits.velocity
        );
    }
}

void RobotControlSystem::SetJointTargetTorque(EntityID robotEntity, const std::string& jointName, 
                                               float torque) {
    if (!m_world) return;
    
    EntityID jointEntity = GetJointEntity(robotEntity, jointName);
    if (!jointEntity.IsValid() || !m_world->HasComponent<JointComponent>(jointEntity)) {
        Logger::GetInstance().WarningFormat(
            "[RobotControlSystem] Joint '%s' not found for robot entity %u",
            jointName.c_str(), robotEntity.index
        );
        return;
    }
    
    auto& jointComp = m_world->GetComponent<JointComponent>(jointEntity);
    jointComp.targetTorque = torque;
    
    // 限制在最大力矩范围内
    if (jointComp.limits.effort > 0.0f) {
        jointComp.targetTorque = std::clamp(
            jointComp.targetTorque,
            -jointComp.limits.effort,
            jointComp.limits.effort
        );
    }
}

void RobotControlSystem::SetJointTargetPositions(EntityID robotEntity, 
                                                 const std::unordered_map<std::string, float>& positions) {
    for (const auto& [jointName, position] : positions) {
        SetJointTargetPosition(robotEntity, jointName, position);
    }
}

void RobotControlSystem::SetJointTargetVelocities(EntityID robotEntity, 
                                                  const std::unordered_map<std::string, float>& velocities) {
    for (const auto& [jointName, velocity] : velocities) {
        SetJointTargetVelocity(robotEntity, jointName, velocity);
    }
}

void RobotControlSystem::SetJointTargetTorques(EntityID robotEntity, 
                                               const std::unordered_map<std::string, float>& torques) {
    for (const auto& [jointName, torque] : torques) {
        SetJointTargetTorque(robotEntity, jointName, torque);
    }
}

float RobotControlSystem::GetJointPosition(EntityID robotEntity, const std::string& jointName) const {
    if (!m_world) return 0.0f;
    
    EntityID jointEntity = FindJointEntity(robotEntity, jointName);
    if (!jointEntity.IsValid() || !m_world->HasComponent<JointComponent>(jointEntity)) {
        return 0.0f;
    }
    
    const auto& jointComp = m_world->GetComponent<JointComponent>(jointEntity);
    return jointComp.currentPosition;
}

float RobotControlSystem::GetJointVelocity(EntityID robotEntity, const std::string& jointName) const {
    if (!m_world) return 0.0f;
    
    EntityID jointEntity = FindJointEntity(robotEntity, jointName);
    if (!jointEntity.IsValid() || !m_world->HasComponent<JointComponent>(jointEntity)) {
        return 0.0f;
    }
    
    const auto& jointComp = m_world->GetComponent<JointComponent>(jointEntity);
    return jointComp.currentVelocity;
}

EntityID RobotControlSystem::GetJointEntity(EntityID robotEntity, const std::string& jointName) {
    if (!m_world) return EntityID::Invalid();
    
    // 检查缓存
    auto robotIt = m_jointEntityMap.find(robotEntity);
    if (robotIt != m_jointEntityMap.end()) {
        auto jointIt = robotIt->second.find(jointName);
        if (jointIt != robotIt->second.end() && jointIt->second.IsValid()) {
            // 验证实体仍然存在且有JointComponent
            if (m_world->HasComponent<JointComponent>(jointIt->second)) {
                return jointIt->second;
            }
        }
    }
    
    // 缓存未命中，从机器人组件查找
    EntityID foundEntity = FindJointEntity(robotEntity, jointName);
    if (foundEntity.IsValid()) {
        // 更新缓存
        m_jointEntityMap[robotEntity][jointName] = foundEntity;
    }
    
    return foundEntity;
}

EntityID RobotControlSystem::FindJointEntity(EntityID robotEntity, const std::string& jointName) const {
    if (!m_world) return EntityID::Invalid();
    
    // 从机器人组件查找
    if (!m_world->HasComponent<RobotComponent>(robotEntity)) {
        return EntityID::Invalid();
    }
    
    const auto& robotComp = m_world->GetComponent<RobotComponent>(robotEntity);
    if (!robotComp.model) {
        return EntityID::Invalid();
    }
    
    // 查找关节实体（通过查询所有JointComponent）
    auto joints = m_world->Query<JointComponent>();
    for (const auto& jointEntity : joints) {
        const auto& jointComp = m_world->GetComponent<JointComponent>(jointEntity);
        if (jointComp.jointName == jointName) {
            // 验证这个关节属于这个机器人（通过检查父link是否在机器人的linkEntityMap中）
            if (robotComp.linkEntityMap.find(jointComp.parentLink) != robotComp.linkEntityMap.end() ||
                robotComp.linkEntityMap.find(jointComp.childLink) != robotComp.linkEntityMap.end()) {
                return jointEntity;
            }
        }
    }
    
    return EntityID::Invalid();
}

void RobotControlSystem::UpdateJointState(EntityID jointEntity, JointComponent& jointComp) {
    if (!m_world) return;
    
    // 优先从物理约束获取关节状态（最准确）
    if (jointComp.childLinkEntity.IsValid() &&
        m_world->HasComponent<ConstraintComponent>(jointComp.childLinkEntity)) {
        
        const auto& constraint = m_world->GetComponent<ConstraintComponent>(jointComp.childLinkEntity);
        
        // 验证约束是否连接到parent link且类型匹配
        if (constraint.connectedEntity == jointComp.parentLinkEntity &&
            constraint.type == ConstraintType::Hinge &&
            (jointComp.type == Render::Robot::JointType::Revolute ||
             jointComp.type == Render::Robot::JointType::Continuous) &&
            constraint.bulletConstraint) {
            
            // 从Hinge约束获取关节角度
            btHingeConstraint* hinge = static_cast<btHingeConstraint*>(constraint.bulletConstraint);
            jointComp.currentPosition = hinge->getHingeAngle();
            
            // 获取角速度（从连接的刚体计算）
            if (m_world->HasComponent<RigidBodyComponent>(jointComp.childLinkEntity)) {
                const auto& rb = m_world->GetComponent<RigidBodyComponent>(jointComp.childLinkEntity);
                if (rb.bulletRigidBody) {
                    btRigidBody* body = static_cast<btRigidBody*>(rb.bulletRigidBody);
                    btVector3 angVel = body->getAngularVelocity();
                    
                    // 获取约束轴方向（从约束的frame中获取）
                    // Hinge约束的轴是frame的Z轴
                    btTransform frameA = hinge->getFrameOffsetA();
                    btVector3 axisInLocal = frameA.getBasis().getColumn(2);  // Z轴是铰链轴
                    btVector3 axisInWorld = body->getCenterOfMassTransform().getBasis() * axisInLocal;
                    jointComp.currentVelocity = angVel.dot(axisInWorld);
                }
            }
            return;  // 已从约束获取状态，直接返回
        }
    }
    
    // 回退：从物理系统获取关节速度
    jointComp.currentVelocity = GetJointVelocityFromPhysics(jointEntity, jointComp);
    
    // 回退：从JointTFSystem获取关节位置
    auto* jointTFSystem = m_world->GetSystem<JointTFSystem>();
    if (jointTFSystem) {
        EntityID robotEntity = EntityID::Invalid();
        auto robots = m_world->Query<RobotComponent>();
        for (const auto& robot : robots) {
            const auto& robotComp = m_world->GetComponent<RobotComponent>(robot);
            if (robotComp.linkEntityMap.find(jointComp.parentLink) != robotComp.linkEntityMap.end() ||
                robotComp.linkEntityMap.find(jointComp.childLink) != robotComp.linkEntityMap.end()) {
                robotEntity = robot;
                break;
            }
        }
        if (robotEntity.IsValid()) {
            jointComp.currentPosition = jointTFSystem->GetJointPosition(robotEntity, jointComp.jointName);
        }
    }
}

void RobotControlSystem::ApplyControl(EntityID jointEntity, JointComponent& jointComp, float deltaTime) {
    (void)deltaTime;
    
    if (!m_world) return;
    
    // 检查是否有约束（优先使用约束控制）
    bool hasConstraint = false;
    if (jointComp.childLinkEntity.IsValid() &&
        m_world->HasComponent<ConstraintComponent>(jointComp.childLinkEntity)) {
        
        auto& constraint = m_world->GetComponent<ConstraintComponent>(jointComp.childLinkEntity);
        
        // 验证约束是否连接到parent link且类型匹配
        if (constraint.connectedEntity == jointComp.parentLinkEntity &&
            constraint.type == ConstraintType::Hinge &&
            (jointComp.type == Render::Robot::JointType::Revolute ||
             jointComp.type == Render::Robot::JointType::Continuous)) {
            
            hasConstraint = true;
            
            // 根据控制模式设置约束参数
            switch (jointComp.controlMode) {
                case JointComponent::ControlMode::Position:
                    // 位置控制：使用约束的位置控制功能
                    constraint.usePositionControl = true;
                    constraint.useMotor = false;  // 位置控制会内部启用马达
                    constraint.targetPosition = jointComp.targetPosition;
                    constraint.positionKp = jointComp.positionKp;
                    constraint.positionKd = jointComp.positionKd;
                    constraint.motorMaxForce = jointComp.maxTorque;
                    return;  // 已通过约束控制，不需要再处理
                    
                case JointComponent::ControlMode::Velocity:
                    // 速度控制：使用约束的马达功能
                    constraint.useMotor = true;
                    constraint.usePositionControl = false;
                    constraint.motorTargetVelocity = jointComp.targetVelocity;
                    constraint.motorMaxForce = jointComp.maxTorque;
                    return;  // 已通过约束控制，不需要再处理
                    
                case JointComponent::ControlMode::Torque:
                    // 力矩控制：仍然直接对刚体施加力矩
                    constraint.useMotor = false;
                    constraint.usePositionControl = false;
                    // 继续执行下面的代码，直接对刚体施加力矩
                    break;
            }
        }
    }
    
    // 如果没有约束或力矩控制模式，直接对刚体施加力矩/力
    float torqueOrForce = 0.0f;
    
    // 根据控制模式计算控制输出
    switch (jointComp.controlMode) {
        case JointComponent::ControlMode::Position:
            // 如果没有约束，使用PD控制器计算力矩
            torqueOrForce = ComputePositionControl(jointComp);
            break;
            
        case JointComponent::ControlMode::Velocity:
            // 如果没有约束，使用P控制器计算力矩
            torqueOrForce = ComputeVelocityControl(jointComp);
            break;
            
        case JointComponent::ControlMode::Torque:
            // 力矩控制：直接使用目标力矩
            torqueOrForce = std::clamp(
                jointComp.targetTorque,
                -jointComp.maxTorque,
                jointComp.maxTorque
            );
            break;
    }
    
    // 应用力矩/力到关节
    ApplyJointTorqueOrForce(jointEntity, jointComp, torqueOrForce);
}

float RobotControlSystem::ComputePositionControl(const JointComponent& jointComp) const {
    // PD控制器
    float error = jointComp.targetPosition - jointComp.currentPosition;
    
    // 限制误差范围，避免过大的控制输出（减少抖动）
    float maxError = 3.14159f;  // 限制在 ±180度范围内
    error = std::clamp(error, -maxError, maxError);
    
    float errorDerivative = -jointComp.currentVelocity;  // 速度是位置的导数（负号因为误差减小）
    
    // 限制速度误差范围
    float maxVelError = 10.0f;  // 限制速度误差范围
    errorDerivative = std::clamp(errorDerivative, -maxVelError, maxVelError);
    
    float torque = jointComp.positionKp * error + jointComp.positionKd * errorDerivative;
    
    // 限制在最大力矩范围内
    return std::clamp(torque, -jointComp.maxTorque, jointComp.maxTorque);
}

float RobotControlSystem::ComputeVelocityControl(const JointComponent& jointComp) const {
    // P控制器
    float error = jointComp.targetVelocity - jointComp.currentVelocity;
    float torque = jointComp.velocityKp * error;
    
    // 限制在最大力矩范围内
    return std::clamp(torque, -jointComp.maxTorque, jointComp.maxTorque);
}

float RobotControlSystem::GetJointVelocityFromPhysics(EntityID jointEntity, const JointComponent& jointComp) const {
    if (!m_world) return 0.0f;
    
    // 获取子link的刚体
    if (!jointComp.childLinkEntity.IsValid() ||
        !m_world->HasComponent<RigidBodyComponent>(jointComp.childLinkEntity)) {
        return 0.0f;
    }
    
    const auto& rb = m_world->GetComponent<RigidBodyComponent>(jointComp.childLinkEntity);
    if (!rb.bulletRigidBody) {
        return 0.0f;
    }
    
    btRigidBody* bulletBody = static_cast<btRigidBody*>(rb.bulletRigidBody);
    
    // 获取关节轴在世界坐标系中的方向
    // 需要从关节的Transform获取世界坐标系中的轴方向
    Vector3 worldAxis = jointComp.axis;  // 默认使用局部轴（简化处理）
    
    if (m_world->HasComponent<TransformComponent>(jointEntity)) {
        const auto& jointTransform = m_world->GetComponent<TransformComponent>(jointEntity);
        if (jointTransform.transform) {
            // 将局部轴转换到世界坐标系
            Quaternion worldRot = jointTransform.transform->GetWorldRotation();
            worldAxis = worldRot * jointComp.axis;
            worldAxis.normalize();
        }
    }
    
    // 根据关节类型计算速度
    if (jointComp.type == Render::Robot::JointType::Revolute ||
        jointComp.type == Render::Robot::JointType::Continuous) {
        // 旋转关节：角速度在关节轴上的投影
        btVector3 btAngularVel = bulletBody->getAngularVelocity();
        Vector3 angularVel(btAngularVel.x(), btAngularVel.y(), btAngularVel.z());
        
        // 投影到关节轴上
        return angularVel.dot(worldAxis);
        
    } else if (jointComp.type == Render::Robot::JointType::Prismatic) {
        // 平移关节：线速度在关节轴上的投影
        btVector3 btLinearVel = bulletBody->getLinearVelocity();
        Vector3 linearVel(btLinearVel.x(), btLinearVel.y(), btLinearVel.z());
        
        // 投影到关节轴上
        return linearVel.dot(worldAxis);
    }
    
    return 0.0f;
}

void RobotControlSystem::ApplyJointTorqueOrForce(EntityID jointEntity, const JointComponent& jointComp, 
                                                  float torqueOrForce) {
    if (!m_world) return;
    
    // 直接对刚体施加力矩/力（用于力矩控制模式或没有约束的情况）
    // 获取子link的刚体
    if (!jointComp.childLinkEntity.IsValid() ||
        !m_world->HasComponent<RigidBodyComponent>(jointComp.childLinkEntity)) {
        return;
    }
    
    auto& rb = m_world->GetComponent<RigidBodyComponent>(jointComp.childLinkEntity);
    if (!rb.bulletRigidBody) {
        return;
    }
    
    // 获取关节轴在世界坐标系中的方向
    // 注意：关节轴是在父link坐标系中定义的，需要转换到世界坐标系
    Vector3 worldAxis = jointComp.axis;  // 默认使用局部轴
    
    // 尝试从父link获取世界旋转（更准确）
    if (jointComp.parentLinkEntity.IsValid() &&
        m_world->HasComponent<TransformComponent>(jointComp.parentLinkEntity)) {
        const auto& parentTransform = m_world->GetComponent<TransformComponent>(jointComp.parentLinkEntity);
        if (parentTransform.transform) {
            // 将局部轴转换到世界坐标系
            Quaternion worldRot = parentTransform.transform->GetWorldRotation();
            worldAxis = worldRot * jointComp.axis;
            worldAxis.normalize();
        }
    } else if (m_world->HasComponent<TransformComponent>(jointEntity)) {
        // 回退到使用关节的Transform
        const auto& jointTransform = m_world->GetComponent<TransformComponent>(jointEntity);
        if (jointTransform.transform) {
            Quaternion worldRot = jointTransform.transform->GetWorldRotation();
            worldAxis = worldRot * jointComp.axis;
            worldAxis.normalize();
        }
    }
    
    // 根据关节类型应用力矩或力
    if (jointComp.type == Render::Robot::JointType::Revolute ||
        jointComp.type == Render::Robot::JointType::Continuous) {
        // 旋转关节：施加力矩
        Vector3 torque = worldAxis * torqueOrForce;
        rb.ApplyTorque(torque);
        
    } else if (jointComp.type == Render::Robot::JointType::Prismatic) {
        // 平移关节：施加力
        Vector3 force = worldAxis * torqueOrForce;
        rb.ApplyForce(force);
    }
}

} // namespace Robot
} // namespace ECS
} // namespace Render
