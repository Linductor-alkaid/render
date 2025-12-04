#pragma once

#include "render/types.h"
#include <array>

namespace Render {
namespace Physics {

/**
 * @brief 接触点
 * 
 * 表示两个物体之间的一个接触点
 */
struct ContactPoint {
    Vector3 position;        // 接触点位置（世界空间）
    Vector3 localPointA;     // 在物体 A 的局部空间坐标
    Vector3 localPointB;     // 在物体 B 的局部空间坐标
    float penetration;       // 穿透深度
    
    ContactPoint()
        : position(Vector3::Zero())
        , localPointA(Vector3::Zero())
        , localPointB(Vector3::Zero())
        , penetration(0.0f) {}
};

/**
 * @brief 接触流形
 * 
 * 表示两个物体之间的接触信息
 * 最多包含 4 个接触点（足够稳定接触求解）
 */
struct ContactManifold {
    Vector3 normal;                          // 碰撞法线（从 A 指向 B）
    float penetration;                       // 最大穿透深度
    int contactCount;                        // 接触点数量
    std::array<ContactPoint, 4> contacts;    // 接触点数组
    
    ContactManifold()
        : normal(Vector3::UnitY())
        , penetration(0.0f)
        , contactCount(0) {}
    
    /**
     * @brief 是否有效（有接触点）
     */
    bool IsValid() const {
        return contactCount > 0 && penetration > 0.0f;
    }
    
    /**
     * @brief 添加接触点
     */
    void AddContact(const Vector3& position, float pen) {
        if (contactCount < 4) {
            contacts[contactCount].position = position;
            contacts[contactCount].penetration = pen;
            contactCount++;
            
            // 更新最大穿透深度
            if (pen > penetration) {
                penetration = pen;
            }
        }
    }
    
    /**
     * @brief 添加接触点（完整信息）
     */
    void AddContact(const Vector3& position, const Vector3& localA, 
                    const Vector3& localB, float pen) {
        if (contactCount < 4) {
            contacts[contactCount].position = position;
            contacts[contactCount].localPointA = localA;
            contacts[contactCount].localPointB = localB;
            contacts[contactCount].penetration = pen;
            contactCount++;
            
            if (pen > penetration) {
                penetration = pen;
            }
        }
    }
    
    /**
     * @brief 清空流形
     */
    void Clear() {
        normal = Vector3::UnitY();
        penetration = 0.0f;
        contactCount = 0;
    }
    
    /**
     * @brief 设置法线
     */
    void SetNormal(const Vector3& n) {
        normal = n.normalized();
    }
};

} // namespace Physics
} // namespace Render

