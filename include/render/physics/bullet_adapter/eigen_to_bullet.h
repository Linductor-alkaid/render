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
#include <LinearMath/btVector3.h>
#include <LinearMath/btQuaternion.h>
#include <LinearMath/btMatrix3x3.h>
#include <LinearMath/btTransform.h>

namespace Render::Physics::BulletAdapter {

/**
 * @brief 数据转换层
 * 
 * 负责在 Eigen 类型（项目使用）和 Bullet 类型之间进行转换
 */

// ============================================================================
// Vector3 转换
// ============================================================================

/**
 * @brief 将 Eigen::Vector3f 转换为 btVector3
 */
inline btVector3 ToBullet(const Vector3& v) {
    return btVector3(v.x(), v.y(), v.z());
}

/**
 * @brief 将 btVector3 转换为 Eigen::Vector3f
 */
inline Vector3 FromBullet(const btVector3& v) {
    return Vector3(v.x(), v.y(), v.z());
}

// ============================================================================
// Quaternion 转换
// ============================================================================

/**
 * @brief 将 Eigen::Quaternionf 转换为 btQuaternion
 * 
 * @note Eigen 四元数顺序为 (w, x, y, z)，Bullet 为 (x, y, z, w)
 */
inline btQuaternion ToBullet(const Quaternion& q) {
    return btQuaternion(q.x(), q.y(), q.z(), q.w());
}

/**
 * @brief 将 btQuaternion 转换为 Eigen::Quaternionf
 * 
 * @note Eigen 四元数顺序为 (w, x, y, z)，Bullet 为 (x, y, z, w)
 */
inline Quaternion FromBullet(const btQuaternion& q) {
    return Quaternion(q.w(), q.x(), q.y(), q.z());  // Eigen: (w, x, y, z)
}

// ============================================================================
// Matrix3 转换
// ============================================================================

/**
 * @brief 将 Eigen::Matrix3f 转换为 btMatrix3x3
 */
inline btMatrix3x3 ToBullet(const Matrix3& m) {
    return btMatrix3x3(
        m(0, 0), m(0, 1), m(0, 2),
        m(1, 0), m(1, 1), m(1, 2),
        m(2, 0), m(2, 1), m(2, 2)
    );
}

/**
 * @brief 将 btMatrix3x3 转换为 Eigen::Matrix3f
 */
inline Matrix3 FromBullet(const btMatrix3x3& m) {
    Matrix3 result;
    result << m[0][0], m[0][1], m[0][2],
              m[1][0], m[1][1], m[1][2],
              m[2][0], m[2][1], m[2][2];
    return result;
}

// ============================================================================
// Transform 转换
// ============================================================================

/**
 * @brief 将 Eigen 位置和旋转转换为 btTransform
 */
inline btTransform ToBullet(const Vector3& pos, const Quaternion& rot) {
    btTransform transform;
    transform.setOrigin(ToBullet(pos));
    transform.setRotation(ToBullet(rot));
    return transform;
}

/**
 * @brief 将 btTransform 转换为 Eigen 位置和旋转
 */
inline void FromBullet(const btTransform& transform, Vector3& pos, Quaternion& rot) {
    pos = FromBullet(transform.getOrigin());
    rot = FromBullet(transform.getRotation());
}

} // namespace Render::Physics::BulletAdapter

