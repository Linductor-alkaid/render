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
#pragma once

#include "render/types.h"

namespace Render {
namespace Physics {

/**
 * @brief 力累加器
 * 
 * 用于累积和计算作用在刚体上的力和扭矩
 * 支持力、扭矩、冲量和角冲量的累加
 */
class ForceAccumulator {
public:
    /**
     * @brief 构造函数 - 初始化所有成员为零
     */
    ForceAccumulator() 
        : m_totalForce(Vector3::Zero())
        , m_totalTorque(Vector3::Zero())
        , m_linearImpulse(Vector3::Zero())    // ← 修复：显式初始化
        , m_angularImpulse(Vector3::Zero())   // ← 修复：显式初始化
    {}
    
    /**
     * @brief 添加力（作用在质心）
     * @param force 力向量 (N)
     */
    void AddForce(const Vector3& force) {
        m_totalForce += force;
    }
    
    /**
     * @brief 在指定点添加力（会产生扭矩）
     * @param force 力向量 (N)
     * @param point 作用点（世界空间）
     * @param centerOfMass 质心位置（世界空间）
     */
    void AddForceAtPoint(const Vector3& force, const Vector3& point, const Vector3& centerOfMass) {
        // 添加力
        m_totalForce += force;
        
        // 计算扭矩：τ = r × F
        // r 是从质心到作用点的向量
        Vector3 r = point - centerOfMass;
        Vector3 torque = r.cross(force);
        m_totalTorque += torque;
    }
    
    /**
     * @brief 添加扭矩
     * @param torque 扭矩向量 (N·m)
     */
    void AddTorque(const Vector3& torque) {
        m_totalTorque += torque;
    }
    
    /**
     * @brief 添加冲量（直接改变速度）
     * @param impulse 冲量向量 (kg·m/s)
     * @param inverseMass 逆质量 (1/kg)
     */
    void AddImpulse(const Vector3& impulse, float inverseMass) {
        // 冲量会直接改变速度，但这里只累加，实际应用在积分器中
        // 为了支持冲量，我们需要存储它们
        m_linearImpulse += impulse * inverseMass;
    }
    
    /**
     * @brief 添加角冲量（直接改变角速度）
     * @param angularImpulse 角冲量向量 (kg·m²/s)
     * @param inverseInertiaTensor 逆惯性张量（世界空间）
     */
    void AddAngularImpulse(const Vector3& angularImpulse, const Matrix3& inverseInertiaTensor) {
        // 角冲量会直接改变角速度
        // 计算角速度变化：Δω = I⁻¹ · L
        Vector3 deltaAngularVelocity = inverseInertiaTensor * angularImpulse;
        m_angularImpulse += deltaAngularVelocity;
    }
    
    /**
     * @brief 获取总力
     * @return 总力向量 (N)
     */
    Vector3 GetTotalForce() const {
        return m_totalForce;
    }
    
    /**
     * @brief 获取总扭矩
     * @return 总扭矩向量 (N·m)
     */
    Vector3 GetTotalTorque() const {
        return m_totalTorque;
    }
    
    /**
     * @brief 获取线性冲量（速度变化）
     * @return 速度变化向量 (m/s)
     */
    Vector3 GetLinearImpulse() const {
        return m_linearImpulse;
    }
    
    /**
     * @brief 获取角冲量（角速度变化）
     * @return 角速度变化向量 (rad/s)
     */
    Vector3 GetAngularImpulse() const {
        return m_angularImpulse;
    }
    
    /**
     * @brief 清空所有累加的力和扭矩
     */
    void Clear() {
        m_totalForce.setZero();
        m_totalTorque.setZero();
        m_linearImpulse.setZero();
        m_angularImpulse.setZero();
    }
    
    /**
     * @brief 清空冲量（保留力和扭矩）
     */
    void ClearImpulses() {
        m_linearImpulse.setZero();
        m_angularImpulse.setZero();
    }
    
private:
    Vector3 m_totalForce;      // 总力 (N)
    Vector3 m_totalTorque;     // 总扭矩 (N·m)
    Vector3 m_linearImpulse;   // 线性冲量产生的速度变化 (m/s)
    Vector3 m_angularImpulse;  // 角冲量产生的角速度变化 (rad/s)
};

} // namespace Physics
} // namespace Render