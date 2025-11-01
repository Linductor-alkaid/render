#pragma once

#include "types.h"
#include "math_utils.h"
#include <mutex>
#include <atomic>

namespace Render {

/**
 * @class Transform
 * @brief 表示3D空间中的变换（位置、旋转、缩放）
 * 
 * Transform 类用于管理3D对象的位置、旋转和缩放信息。
 * 支持本地变换和世界变换，以及父子关系的变换层级。
 */
class Transform {
    // 确保正确的内存对齐以使用 Eigen 的 SIMD 优化
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    
public:
    /**
     * @brief 默认构造函数
     * 创建一个单位变换（位置为原点，无旋转，缩放为1）
     */
    Transform();
    
    /**
     * @brief 构造函数
     * @param position 位置
     * @param rotation 旋转（四元数）
     * @param scale 缩放
     */
    Transform(const Vector3& position, const Quaternion& rotation = Quaternion::Identity(), 
              const Vector3& scale = Vector3::Ones());
    
    // ========================================================================
    // 位置操作
    // ========================================================================
    
    /**
     * @brief 设置本地位置
     * @param position 新的本地位置
     */
    void SetPosition(const Vector3& position);
    
    /**
     * @brief 获取本地位置
     * @return 本地位置
     */
    const Vector3& GetPosition() const { return m_position; }
    
    /**
     * @brief 获取世界位置
     * @return 世界位置
     */
    Vector3 GetWorldPosition() const;
    
    /**
     * @brief 平移对象（本地空间）
     * @param translation 平移向量
     */
    void Translate(const Vector3& translation);
    
    /**
     * @brief 在世界空间中平移对象
     * @param translation 平移向量
     */
    void TranslateWorld(const Vector3& translation);
    
    // ========================================================================
    // 批量变换操作（性能优化）
    // ========================================================================
    
    /**
     * @brief 批量变换点从本地空间到世界空间
     * @param localPoints 本地空间的点数组
     * @param worldPoints 输出的世界空间点数组
     */
    void TransformPoints(const std::vector<Vector3>& localPoints, 
                        std::vector<Vector3>& worldPoints) const;
    
    /**
     * @brief 批量变换方向从本地空间到世界空间
     * @param localDirections 本地空间的方向数组
     * @param worldDirections 输出的世界空间方向数组
     */
    void TransformDirections(const std::vector<Vector3>& localDirections,
                            std::vector<Vector3>& worldDirections) const;
    
    // ========================================================================
    // 旋转操作
    // ========================================================================
    
    /**
     * @brief 设置本地旋转（四元数）
     * @param rotation 新的本地旋转
     */
    void SetRotation(const Quaternion& rotation);
    
    /**
     * @brief 设置本地旋转（欧拉角，弧度）
     * @param euler 欧拉角（弧度）
     */
    void SetRotationEuler(const Vector3& euler);
    
    /**
     * @brief 设置本地旋转（欧拉角，度数）
     * @param euler 欧拉角（度数）
     */
    void SetRotationEulerDegrees(const Vector3& euler);
    
    /**
     * @brief 获取本地旋转
     * @return 本地旋转（四元数）
     */
    const Quaternion& GetRotation() const { return m_rotation; }
    
    /**
     * @brief 获取本地旋转（欧拉角，弧度）
     * @return 欧拉角（弧度）
     */
    Vector3 GetRotationEuler() const;
    
    /**
     * @brief 获取本地旋转（欧拉角，度数）
     * @return 欧拉角（度数）
     */
    Vector3 GetRotationEulerDegrees() const;
    
    /**
     * @brief 获取世界旋转
     * @return 世界旋转（四元数）
     */
    Quaternion GetWorldRotation() const;
    
    /**
     * @brief 旋转对象（本地空间）
     * @param rotation 旋转增量（四元数）
     */
    void Rotate(const Quaternion& rotation);
    
    /**
     * @brief 围绕轴旋转（本地空间）
     * @param axis 旋转轴
     * @param angle 旋转角度（弧度）
     */
    void RotateAround(const Vector3& axis, float angle);
    
    /**
     * @brief 围绕轴旋转（世界空间）
     * @param axis 旋转轴
     * @param angle 旋转角度（弧度）
     */
    void RotateAroundWorld(const Vector3& axis, float angle);
    
    /**
     * @brief 使变换朝向目标点
     * @param target 目标点（世界坐标）
     * @param up 上方向（默认为世界上方）
     */
    void LookAt(const Vector3& target, const Vector3& up = Vector3::UnitY());
    
    // ========================================================================
    // 缩放操作
    // ========================================================================
    
    /**
     * @brief 设置本地缩放
     * @param scale 新的本地缩放
     */
    void SetScale(const Vector3& scale);
    
    /**
     * @brief 设置统一缩放
     * @param scale 统一缩放值
     */
    void SetScale(float scale);
    
    /**
     * @brief 获取本地缩放
     * @return 本地缩放
     */
    const Vector3& GetScale() const { return m_scale; }
    
    /**
     * @brief 获取世界缩放
     * @return 世界缩放
     */
    Vector3 GetWorldScale() const;
    
    // ========================================================================
    // 方向向量
    // ========================================================================
    
    /**
     * @brief 获取前方向（本地空间）
     * @return 前方向向量
     */
    Vector3 GetForward() const;
    
    /**
     * @brief 获取右方向（本地空间）
     * @return 右方向向量
     */
    Vector3 GetRight() const;
    
    /**
     * @brief 获取上方向（本地空间）
     * @return 上方向向量
     */
    Vector3 GetUp() const;
    
    // ========================================================================
    // 矩阵操作
    // ========================================================================
    
    /**
     * @brief 获取本地变换矩阵
     * @return 本地变换矩阵
     */
    Matrix4 GetLocalMatrix() const;
    
    /**
     * @brief 获取世界变换矩阵
     * @return 世界变换矩阵
     */
    Matrix4 GetWorldMatrix() const;
    
    /**
     * @brief 从矩阵设置变换
     * @param matrix 变换矩阵
     */
    void SetFromMatrix(const Matrix4& matrix);
    
    // ========================================================================
    // 父子关系
    // ========================================================================
    
    /**
     * @brief 设置父变换
     * @param parent 父变换指针（nullptr 表示无父对象）
     */
    void SetParent(Transform* parent);
    
    /**
     * @brief 获取父变换
     * @return 父变换指针（可能为 nullptr）
     */
    Transform* GetParent() const { return m_parent; }
    
    /**
     * @brief 是否有父变换
     * @return 如果有父变换返回 true
     */
    bool HasParent() const { return m_parent != nullptr; }
    
    // ========================================================================
    // 坐标变换
    // ========================================================================
    
    /**
     * @brief 将点从本地空间变换到世界空间
     * @param localPoint 本地空间的点
     * @return 世界空间的点
     */
    Vector3 TransformPoint(const Vector3& localPoint) const;
    
    /**
     * @brief 将方向从本地空间变换到世界空间
     * @param localDirection 本地空间的方向
     * @return 世界空间的方向
     */
    Vector3 TransformDirection(const Vector3& localDirection) const;
    
    /**
     * @brief 将点从世界空间变换到本地空间
     * @param worldPoint 世界空间的点
     * @return 本地空间的点
     */
    Vector3 InverseTransformPoint(const Vector3& worldPoint) const;
    
    /**
     * @brief 将方向从世界空间变换到本地空间
     * @param worldDirection 世界空间的方向
     * @return 本地空间的方向
     */
    Vector3 InverseTransformDirection(const Vector3& worldDirection) const;
    
private:
    Vector3 m_position;      // 本地位置
    Quaternion m_rotation;   // 本地旋转
    Vector3 m_scale;         // 本地缩放
    
    Transform* m_parent;     // 父变换（可选）
    
    // 缓存系统：使用 dirty flag 避免重复计算
    mutable std::atomic<bool> m_dirtyLocal;  // 本地矩阵是否需要更新
    mutable std::atomic<bool> m_dirtyWorld;  // 世界矩阵是否需要更新
    mutable std::atomic<bool> m_dirtyWorldTransform;  // 世界变换组件是否需要更新
    
    mutable Matrix4 m_localMatrix;   // 缓存的本地矩阵
    mutable Matrix4 m_worldMatrix;   // 缓存的世界矩阵
    
    // 世界变换组件缓存（避免递归计算）
    mutable Vector3 m_cachedWorldPosition;
    mutable Quaternion m_cachedWorldRotation;
    mutable Vector3 m_cachedWorldScale;
    
    // 线程安全：使用递归互斥锁保护数据访问
    // 使用递归锁允许同一线程多次获取锁，避免在递归调用（如GetWorldPosition调用父对象的GetWorldPosition）时死锁
    mutable std::recursive_mutex m_mutex;           // 主锁：保护基本成员变量
    mutable std::mutex m_cacheMutex;      // 缓存锁：保护缓存变量（已废弃，保留以保持二进制兼容）
    
    void MarkDirty();
    void MarkDirtyNoLock();  // 无锁版本，供内部已加锁的方法调用
    void UpdateWorldTransformCache() const;
};

} // namespace Render

