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
#include "render/math_utils.h"
#include "render/transform.h"
#include "physics_components.h"

namespace Render {
namespace Physics {

/**
 * @brief 物理工具函数
 * 
 * 提供惯性张量计算、质量计算等辅助功能
 */
class PhysicsUtils {
public:
    // ==================== 惯性张量计算 ====================
    
    /**
     * @brief 计算球体的惯性张量
     * @param mass 质量 (kg)
     * @param radius 半径 (m)
     * @return 惯性张量
     */
    static Matrix3 ComputeSphereInertiaTensor(float mass, float radius) {
        float inertia = (2.0f / 5.0f) * mass * radius * radius;
        Matrix3 tensor = Matrix3::Identity() * inertia;
        return tensor;
    }
    
    /**
     * @brief 计算盒体的惯性张量
     * @param mass 质量 (kg)
     * @param halfExtents 半尺寸 (m)
     * @return 惯性张量
     */
    static Matrix3 ComputeBoxInertiaTensor(float mass, const Vector3& halfExtents) {
        Vector3 size = halfExtents * 2.0f;
        float xx = (1.0f / 12.0f) * mass * (size.y() * size.y() + size.z() * size.z());
        float yy = (1.0f / 12.0f) * mass * (size.x() * size.x() + size.z() * size.z());
        float zz = (1.0f / 12.0f) * mass * (size.x() * size.x() + size.y() * size.y());
        
        Matrix3 tensor = Matrix3::Zero();
        tensor(0, 0) = xx;
        tensor(1, 1) = yy;
        tensor(2, 2) = zz;
        return tensor;
    }
    
    /**
     * @brief 计算胶囊体的惯性张量（近似为圆柱体）
     * @param mass 质量 (kg)
     * @param radius 半径 (m)
     * @param height 高度 (m)
     * @return 惯性张量
     */
    static Matrix3 ComputeCapsuleInertiaTensor(float mass, float radius, float height) {
        // 简化为圆柱体计算
        float radiusSq = radius * radius;
        float heightSq = height * height;
        
        float xx = mass * (3.0f * radiusSq + heightSq) / 12.0f;
        float yy = mass * radiusSq / 2.0f;  // 沿高度轴
        float zz = xx;
        
        Matrix3 tensor = Matrix3::Zero();
        tensor(0, 0) = xx;
        tensor(1, 1) = yy;
        tensor(2, 2) = zz;
        return tensor;
    }
    
    // ==================== 质量计算 ====================
    
    /**
     * @brief 计算球体的质量
     * @param density 密度 (kg/m³)
     * @param radius 半径 (m)
     * @return 质量 (kg)
     */
    static float ComputeSphereMass(float density, float radius) {
        float volume = (4.0f / 3.0f) * MathUtils::PI * radius * radius * radius;
        return density * volume;
    }
    
    /**
     * @brief 计算盒体的质量
     * @param density 密度 (kg/m³)
     * @param halfExtents 半尺寸 (m)
     * @return 质量 (kg)
     */
    static float ComputeBoxMass(float density, const Vector3& halfExtents) {
        Vector3 size = halfExtents * 2.0f;
        float volume = size.x() * size.y() * size.z();
        return density * volume;
    }
    
    /**
     * @brief 计算胶囊体的质量（近似为圆柱体）
     * @param density 密度 (kg/m³)
     * @param radius 半径 (m)
     * @param height 高度 (m)
     * @return 质量 (kg)
     */
    static float ComputeCapsuleMass(float density, float radius, float height) {
        float volume = MathUtils::PI * radius * radius * height;
        return density * volume;
    }
    
    // ==================== 刚体初始化 ====================
    
    /**
     * @brief 根据碰撞体自动初始化刚体的质量和惯性张量
     * @param rigidBody 刚体组件
     * @param collider 碰撞体组件
     * @param density 密度 (kg/m³)，默认使用材质密度
     */
    static void InitializeRigidBody(RigidBodyComponent& rigidBody, 
                                     const ColliderComponent& collider,
                                     float density = 0.0f) {
        // 如果没有指定密度，使用材质密度
        if (density <= 0.0f && collider.material) {
            density = collider.material->density;
        }
        if (density <= 0.0f) {
            density = 1.0f;  // 默认密度
        }
        
        // 根据形状类型计算质量和惯性张量
        float mass = 1.0f;
        Matrix3 inertiaTensor = Matrix3::Identity();
        
        switch (collider.shapeType) {
            case ColliderComponent::ShapeType::Sphere: {
                float radius = collider.shapeData.sphere.radius;
                mass = ComputeSphereMass(density, radius);
                inertiaTensor = ComputeSphereInertiaTensor(mass, radius);
                break;
            }
            
            case ColliderComponent::ShapeType::Box: {
                Vector3 halfExtents = collider.GetBoxHalfExtents();
                mass = ComputeBoxMass(density, halfExtents);
                inertiaTensor = ComputeBoxInertiaTensor(mass, halfExtents);
                break;
            }
            
            case ColliderComponent::ShapeType::Capsule: {
                float radius = collider.shapeData.capsule.radius;
                float height = collider.shapeData.capsule.height;
                mass = ComputeCapsuleMass(density, radius, height);
                inertiaTensor = ComputeCapsuleInertiaTensor(mass, radius, height);
                break;
            }
            
            default:
                // 其他形状使用默认值
                break;
        }
        
        // 设置质量
        rigidBody.SetMass(mass);
        
        // 设置惯性张量
        rigidBody.inertiaTensor = inertiaTensor;
        rigidBody.inverseInertiaTensor = inertiaTensor.inverse();
    }
    
    // ==================== AABB 计算 ====================
    
    /**
     * @brief 计算碰撞体的世界空间 AABB
     * @param collider 碰撞体组件
     * @param worldMatrix 世界变换矩阵
     * @return 世界空间 AABB
     */
    static AABB ComputeWorldAABB(const ColliderComponent& collider, const Matrix4& worldMatrix) {
        // 使用现有的 MathUtils 提取变换信息
        Vector3 position = MathUtils::GetPosition(worldMatrix);
        Vector3 scale = MathUtils::GetScale(worldMatrix);
        
        AABB aabb;
        
        switch (collider.shapeType) {
            case ColliderComponent::ShapeType::Sphere: {
                float radius = collider.shapeData.sphere.radius * scale.maxCoeff();
                Vector3 center = position + collider.center;
                aabb.min = center - Vector3::Ones() * radius;
                aabb.max = center + Vector3::Ones() * radius;
                break;
            }
            
            case ColliderComponent::ShapeType::Box: {
                Vector3 halfExtents = collider.GetBoxHalfExtents().cwiseProduct(scale);
                Vector3 center = position + collider.center;
                aabb.min = center - halfExtents;
                aabb.max = center + halfExtents;
                break;
            }
            
            case ColliderComponent::ShapeType::Capsule: {
                float radius = collider.shapeData.capsule.radius * scale.maxCoeff();
                float halfHeight = collider.shapeData.capsule.height * 0.5f * scale.y();
                Vector3 center = position + collider.center;
                aabb.min = center - Vector3(radius, halfHeight + radius, radius);
                aabb.max = center + Vector3(radius, halfHeight + radius, radius);
                break;
            }
            
            default:
                // 默认情况
                aabb.min = position - Vector3::Ones();
                aabb.max = position + Vector3::Ones();
                break;
        }
        
        return aabb;
    }
    
    /**
     * @brief 计算碰撞体的世界空间 AABB（使用 Transform）
     * @param collider 碰撞体组件
     * @param transform Transform 对象
     * @return 世界空间 AABB
     */
    static AABB ComputeWorldAABB(const ColliderComponent& collider, const Transform& transform) {
        return ComputeWorldAABB(collider, transform.GetWorldMatrix());
    }
    
    // ==================== 物理变换工具 ====================
    
    /**
     * @brief 将世界空间的点转换到刚体的局部空间
     * @param worldPoint 世界空间点
     * @param transform Transform 对象
     * @return 局部空间点
     */
    static Vector3 WorldToLocal(const Vector3& worldPoint, const Transform& transform) {
        return transform.InverseTransformPoint(worldPoint);
    }
    
    /**
     * @brief 将局部空间的点转换到世界空间
     * @param localPoint 局部空间点
     * @param transform Transform 对象
     * @return 世界空间点
     */
    static Vector3 LocalToWorld(const Vector3& localPoint, const Transform& transform) {
        return transform.TransformPoint(localPoint);
    }
    
    /**
     * @brief 将世界空间的方向向量转换到局部空间
     * @param worldDirection 世界空间方向
     * @param transform Transform 对象
     * @return 局部空间方向
     */
    static Vector3 WorldToLocalDirection(const Vector3& worldDirection, const Transform& transform) {
        return transform.InverseTransformDirection(worldDirection);
    }
    
    /**
     * @brief 将局部空间的方向向量转换到世界空间
     * @param localDirection 局部空间方向
     * @param transform Transform 对象
     * @return 世界空间方向
     */
    static Vector3 LocalToWorldDirection(const Vector3& localDirection, const Transform& transform) {
        return transform.TransformDirection(localDirection);
    }
    
    /**
     * @brief 计算两点之间的距离
     * @param a 点 A
     * @param b 点 B
     * @return 距离
     */
    static float Distance(const Vector3& a, const Vector3& b) {
        return MathUtils::Distance(a, b);
    }
    
    /**
     * @brief 计算两点之间的距离平方（避免开方运算）
     * @param a 点 A
     * @param b 点 B
     * @return 距离平方
     */
    static float DistanceSquared(const Vector3& a, const Vector3& b) {
        return MathUtils::DistanceSquared(a, b);
    }
    
    /**
     * @brief 向量投影
     * @param vector 要投影的向量
     * @param onNormal 投影到的法线（单位向量）
     * @return 投影向量
     */
    static Vector3 Project(const Vector3& vector, const Vector3& onNormal) {
        return MathUtils::Project(vector, onNormal);
    }
    
    /**
     * @brief 向量反射
     * @param vector 入射向量
     * @param normal 反射面法线（单位向量）
     * @return 反射向量
     */
    static Vector3 Reflect(const Vector3& vector, const Vector3& normal) {
        return MathUtils::Reflect(vector, normal);
    }
};

} // namespace Physics
} // namespace Render

