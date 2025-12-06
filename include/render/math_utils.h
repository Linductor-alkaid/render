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

#include "types.h"
#include <cmath>

namespace Render {
namespace MathUtils {

// ============================================================================
// 常量
// ============================================================================

constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 6.28318530717958647692f;
constexpr float HALF_PI = 1.57079632679489661923f;
constexpr float DEG2RAD = 0.01745329251994329577f;  // PI / 180
constexpr float RAD2DEG = 57.2957795130823208768f;  // 180 / PI
constexpr float EPSILON = 1e-6f;

// ============================================================================
// 角度转换
// ============================================================================

inline float DegreesToRadians(float degrees) {
    return degrees * DEG2RAD;
}

inline float RadiansToDegrees(float radians) {
    return radians * RAD2DEG;
}

// ============================================================================
// 数值工具
// ============================================================================

inline float Clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

inline float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline float InverseLerp(float a, float b, float value) {
    return (value - a) / (b - a);
}

inline bool NearlyEqual(float a, float b, float epsilon = EPSILON) {
    return std::abs(a - b) < epsilon;
}

inline float Smoothstep(float edge0, float edge1, float x) {
    float t = Clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// ============================================================================
// 向量工具
// ============================================================================

// 检查向量是否已归一化
inline bool IsNormalized(const Vector3& v, float epsilon = EPSILON) {
    float sqrNorm = v.squaredNorm();
    return std::abs(sqrNorm - 1.0f) < epsilon;
}

// 安全归一化：如果已归一化则直接返回
inline Vector3 SafeNormalize(const Vector3& v) {
    if (IsNormalized(v)) {
        return v;
    }
    float sqrNorm = v.squaredNorm();
    if (sqrNorm < EPSILON * EPSILON) {
        return Vector3::UnitX(); // 零向量情况返回默认方向
    }
    return v / std::sqrt(sqrNorm);
}

inline Vector3 Lerp(const Vector3& a, const Vector3& b, float t) {
    return a + (b - a) * t;
}

inline float Distance(const Vector3& a, const Vector3& b) {
    return (b - a).norm();
}

inline float DistanceSquared(const Vector3& a, const Vector3& b) {
    return (b - a).squaredNorm();
}

inline Vector3 Project(const Vector3& vector, const Vector3& onNormal) {
    float sqrMag = onNormal.squaredNorm();
    if (sqrMag < EPSILON) {
        return Vector3::Zero();
    }
    return onNormal * (vector.dot(onNormal) / sqrMag);
}

inline Vector3 Reflect(const Vector3& vector, const Vector3& normal) {
    return vector - 2.0f * vector.dot(normal) * normal;
}

// ============================================================================
// 四元数工具
// ============================================================================

// 创建围绕轴旋转的四元数
inline Quaternion AngleAxis(float angle, const Vector3& axis) {
    return Quaternion(Eigen::AngleAxisf(angle, axis.normalized()));
}

// 从欧拉角创建四元数（弧度，顺序：XYZ）
// 优化版本：直接计算，避免创建3个临时四元数对象
inline Quaternion FromEuler(float x, float y, float z) {
    float cx = std::cos(x * 0.5f);
    float sx = std::sin(x * 0.5f);
    float cy = std::cos(y * 0.5f);
    float sy = std::sin(y * 0.5f);
    float cz = std::cos(z * 0.5f);
    float sz = std::sin(z * 0.5f);
    
    Quaternion q;
    q.w() = cx * cy * cz + sx * sy * sz;
    q.x() = sx * cy * cz - cx * sy * sz;
    q.y() = cx * sy * cz + sx * cy * sz;
    q.z() = cx * cy * sz - sx * sy * cz;
    
    return q;
}

// 从欧拉角创建四元数（度数）
inline Quaternion FromEulerDegrees(float x, float y, float z) {
    return FromEuler(
        DegreesToRadians(x),
        DegreesToRadians(y),
        DegreesToRadians(z)
    );
}

// 转换为欧拉角（弧度）
inline Vector3 ToEuler(const Quaternion& q) {
    return q.toRotationMatrix().eulerAngles(0, 1, 2);
}

// 转换为欧拉角（度数）
inline Vector3 ToEulerDegrees(const Quaternion& q) {
    Vector3 euler = ToEuler(q);
    return Vector3(
        RadiansToDegrees(euler.x()),
        RadiansToDegrees(euler.y()),
        RadiansToDegrees(euler.z())
    );
}

// 四元数插值
inline Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t) {
    return a.slerp(t, b);
}

// 从一个方向向量创建朝向该方向的四元数
// 优化版本：使用 FromTwoVectors 避免矩阵构建 + SafeNormalize 优化
inline Quaternion LookRotation(const Vector3& forward, const Vector3& up = Vector3::UnitY()) {
    Vector3 f = SafeNormalize(forward);
    
    // 从默认前向（Z轴）旋转到目标方向
    Quaternion q = Quaternion::FromTwoVectors(Vector3::UnitZ(), f);
    
    // 调整上方向
    Vector3 currentUp = q * Vector3::UnitY();
    Vector3 targetUp = SafeNormalize(up);
    
    // 计算需要的额外旋转
    float dot = currentUp.dot(targetUp);
    if (dot < 0.9999f) {  // 如果不是几乎平行
        Vector3 axis = currentUp.cross(targetUp);
        float axisLen = axis.norm();
        if (axisLen > EPSILON) {
            float angle = std::acos(Clamp(dot, -1.0f, 1.0f));
            Quaternion upRotation = AngleAxis(angle, axis / axisLen);
            q = upRotation * q;
        }
    }
    
    return q;
}

// ============================================================================
// 矩阵变换工具
// ============================================================================

// 创建平移矩阵
inline Matrix4 Translate(const Vector3& translation) {
    Matrix4 mat = Matrix4::Identity();
    mat(0, 3) = translation.x();
    mat(1, 3) = translation.y();
    mat(2, 3) = translation.z();
    return mat;
}

// 创建旋转矩阵
inline Matrix4 Rotate(const Quaternion& rotation) {
    Matrix4 mat = Matrix4::Identity();
    mat.block<3, 3>(0, 0) = rotation.toRotationMatrix();
    return mat;
}

// 创建缩放矩阵
inline Matrix4 Scale(const Vector3& scale) {
    Matrix4 mat = Matrix4::Identity();
    mat(0, 0) = scale.x();
    mat(1, 1) = scale.y();
    mat(2, 2) = scale.z();
    return mat;
}

// 创建 TRS（平移-旋转-缩放）矩阵
// 优化版本：使用 Eigen 的 Affine 变换（内部优化）
inline Matrix4 TRS(const Vector3& position, const Quaternion& rotation, const Vector3& scale) {
    Eigen::Affine3f transform = 
        Eigen::Translation3f(position) * 
        rotation * 
        Eigen::Scaling(scale);
    return transform.matrix();
}

// 从矩阵中提取位置
inline Vector3 GetPosition(const Matrix4& matrix) {
    return Vector3(matrix(0, 3), matrix(1, 3), matrix(2, 3));
}

// 从矩阵中提取旋转（四元数）
inline Quaternion GetRotation(const Matrix4& matrix) {
    Matrix3 rotMatrix = matrix.block<3, 3>(0, 0);
    
    // 移除缩放影响
    Vector3 scale(
        rotMatrix.col(0).norm(),
        rotMatrix.col(1).norm(),
        rotMatrix.col(2).norm()
    );
    
    rotMatrix.col(0) /= scale.x();
    rotMatrix.col(1) /= scale.y();
    rotMatrix.col(2) /= scale.z();
    
    return Quaternion(rotMatrix);
}

// 从矩阵中提取缩放
inline Vector3 GetScale(const Matrix4& matrix) {
    Matrix3 rotMatrix = matrix.block<3, 3>(0, 0);
    return Vector3(
        rotMatrix.col(0).norm(),
        rotMatrix.col(1).norm(),
        rotMatrix.col(2).norm()
    );
}

// 分解变换矩阵为 TRS 分量
inline void DecomposeMatrix(const Matrix4& matrix, Vector3& position, Quaternion& rotation, Vector3& scale) {
    position = GetPosition(matrix);
    scale = GetScale(matrix);
    rotation = GetRotation(matrix);
}

// ============================================================================
// 投影矩阵
// ============================================================================

// 创建透视投影矩阵
inline Matrix4 Perspective(float fovY, float aspect, float near, float far) {
    float tanHalfFovy = std::tan(fovY / 2.0f);
    
    Matrix4 mat = Matrix4::Zero();
    mat(0, 0) = 1.0f / (aspect * tanHalfFovy);
    mat(1, 1) = 1.0f / tanHalfFovy;
    mat(2, 2) = -(far + near) / (far - near);
    mat(2, 3) = -(2.0f * far * near) / (far - near);
    mat(3, 2) = -1.0f;
    
    return mat;
}

// 创建透视投影矩阵（使用视场角度数）
inline Matrix4 PerspectiveDegrees(float fovYDegrees, float aspect, float near, float far) {
    return Perspective(DegreesToRadians(fovYDegrees), aspect, near, far);
}

// 创建正交投影矩阵
inline Matrix4 Orthographic(float left, float right, float bottom, float top, float near, float far) {
    Matrix4 mat = Matrix4::Identity();
    mat(0, 0) = 2.0f / (right - left);
    mat(1, 1) = 2.0f / (top - bottom);
    mat(2, 2) = -2.0f / (far - near);
    mat(0, 3) = -(right + left) / (right - left);
    mat(1, 3) = -(top + bottom) / (top - bottom);
    mat(2, 3) = -(far + near) / (far - near);
    
    return mat;
}

// 创建视图矩阵（look at）
inline Matrix4 LookAt(const Vector3& eye, const Vector3& center, const Vector3& up) {
    Vector3 f = (center - eye).normalized();
    Vector3 s = f.cross(up).normalized();
    Vector3 u = s.cross(f);
    
    Matrix4 mat = Matrix4::Identity();
    mat(0, 0) = s.x();
    mat(0, 1) = s.y();
    mat(0, 2) = s.z();
    mat(1, 0) = u.x();
    mat(1, 1) = u.y();
    mat(1, 2) = u.z();
    mat(2, 0) = -f.x();
    mat(2, 1) = -f.y();
    mat(2, 2) = -f.z();
    mat(0, 3) = -s.dot(eye);
    mat(1, 3) = -u.dot(eye);
    mat(2, 3) = f.dot(eye);
    
    return mat;
}

} // namespace MathUtils
} // namespace Render

