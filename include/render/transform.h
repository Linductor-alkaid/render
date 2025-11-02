#pragma once

#include "types.h"
#include "math_utils.h"
#include <mutex>
#include <atomic>
#include <vector>

namespace Render {

/**
 * @class Transform
 * @brief 表示3D空间中的变换（位置、旋转、缩放）
 * 
 * Transform 类用于管理3D对象的位置、旋转和缩放信息。
 * 支持本地变换和世界变换，以及父子关系的变换层级。
 * 
 * @section thread_safety 线程安全
 * - Transform 类是线程安全的，所有 public 方法都有适当的同步保护
 * - 可以在多线程环境下安全使用（读、写、混合操作）
 * - 内部使用递归互斥锁，避免递归调用时的死锁
 * - 父指针使用原子类型，保证多线程读写的原子性
 * - 批量操作（TransformPoints/TransformDirections）要求：
 *   调用者必须确保输出向量不被多个线程并发访问
 * 
 * @section parent_child 父子关系
 * - 父对象指针是观察指针（non-owning），不负责生命周期管理
 * - **生命周期保护**：父对象销毁时自动通知所有子对象，清除子对象的父指针
 * - **安全保证**：子对象不会持有悬空指针，访问时会自动退化为无父对象状态
 * - 自动检测并拒绝自引用（将自己设为父对象）
 * - 自动检测并拒绝循环引用（A->B->C->A）
 * - 父子层级深度限制为 1000 层
 * 
 * @section numeric_safety 数值安全
 * - 所有旋转操作会自动检查和归一化四元数
 * - 零向量和无效输入会被检测并产生警告
 * - 使用 MathUtils::EPSILON 进行浮点数比较
 * 
 * @section best_practices 最佳实践
 * 1. **生命周期管理**：
 *    - 父对象应该比子对象先创建、后销毁
 *    - 避免在动态数组（std::vector）中存储 Transform 后设置父子关系
 *      （数组扩容会导致对象地址改变）
 *    - 推荐使用 std::list 或智能指针容器管理 Transform 对象
 * 
 * 2. **性能优化**：
 *    - GetWorld* 方法会递归计算，频繁调用时考虑缓存结果
 *    - 批量操作（TransformPoints）对大量数据更高效
 *    - 批量操作在数据量 > 5000 时自动使用 OpenMP 并行加速
 * 
 * 3. **线程安全**：
 *    - 虽然类本身是线程安全的，但避免在持有锁时执行长时间操作
 *    - 批量操作的输出向量不应在多线程间共享
 * 
 * @example 正确用法
 * @code
 * // ✅ 正确：父对象生命周期长于子对象
 * {
 *     Transform parent;
 *     Transform child;
 *     child.SetParent(&parent);
 *     Vector3 worldPos = child.GetWorldPosition();
 *     // parent 和 child 同时离开作用域
 * }
 * 
 * // ✅ 正确：使用智能指针管理
 * auto parent = std::make_shared<Transform>();
 * auto child = std::make_shared<Transform>();
 * child->SetParent(parent.get());
 * 
 * // ✅ 正确：使用 std::list
 * std::list<Transform> transforms;
 * transforms.emplace_back();
 * transforms.emplace_back();
 * auto it = transforms.begin();
 * Transform* parent = &(*it);
 * ++it;
 * it->SetParent(parent);  // 安全，地址不会改变
 * @endcode
 * 
 * @example 错误用法（已自动处理）
 * @code
 * // ❌ 错误：循环引用会被检测并拒绝
 * Transform a, b, c;
 * a.SetParent(&b);
 * b.SetParent(&c);
 * c.SetParent(&a);  // 错误！产生警告并拒绝操作
 * 
 * // ✅ 已修复：父对象先销毁（自动安全处理）
 * Transform child;
 * {
 *     Transform parent;
 *     child.SetParent(&parent);
 * }  // parent 销毁时自动清除 child.m_parent
 * child.GetWorldPosition();  // ✓ 安全！返回本地位置（无父对象状态）
 * 
 * // ⚠️ 注意：使用 std::vector 仍需小心（地址可能改变）
 * std::vector<Transform> transforms;
 * transforms.emplace_back();  // transforms[0]
 * Transform child;
 * child.SetParent(&transforms[0]);
 * transforms.emplace_back();  // 可能触发重新分配，transforms[0] 地址改变！
 * // 虽然父指针会失效，但不会崩溃（会访问错误的对象或在销毁时清理）
 * // 建议：使用 std::list 或智能指针容器避免地址改变
 * @endcode
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
    
    /**
     * @brief 析构函数
     * 
     * 自动通知所有子对象清除父指针，避免悬空指针。
     */
    ~Transform();
    
    // 禁用拷贝和移动（因为包含 std::atomic 和 std::recursive_mutex，它们都不可拷贝不可移动）
    Transform(const Transform&) = delete;
    Transform& operator=(const Transform&) = delete;
    Transform(Transform&&) = delete;
    Transform& operator=(Transform&&) = delete;
    
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
     * 
     * @note 性能优化：
     * - 当点数量 > 5000 时，自动使用 OpenMP 并行加速（如果可用）
     * - 批量操作比单个调用 TransformPoint 效率高得多
     * 
     * @warning 线程安全：
     * - worldPoints 会被修改（resize 和写入）
     * - 调用者必须确保 worldPoints 不被多个线程并发访问
     * - 如果多线程调用此方法，每个线程应使用独立的输出向量
     */
    void TransformPoints(const std::vector<Vector3>& localPoints, 
                        std::vector<Vector3>& worldPoints) const;
    
    /**
     * @brief 批量变换方向从本地空间到世界空间
     * @param localDirections 本地空间的方向数组
     * @param worldDirections 输出的世界空间方向数组
     * 
     * @note 性能优化：
     * - 当方向数量 > 5000 时，自动使用 OpenMP 并行加速（如果可用）
     * - 批量操作比单个调用 TransformDirection 效率高得多
     * 
     * @warning 线程安全：
     * - worldDirections 会被修改（resize 和写入）
     * - 调用者必须确保 worldDirections 不被多个线程并发访问
     * - 如果多线程调用此方法，每个线程应使用独立的输出向量
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
     * 
     * @note 安全检查：
     * - 自动检测并拒绝自引用（parent == this）
     * - 自动检测并拒绝循环引用（例如 A->B->C->A）
     * - 检测父对象层级深度，拒绝超过 1000 层的层级
     * - 检测失败时会产生警告日志，操作被拒绝
     * 
     * @note 生命周期管理（自动安全）：
     * - parent 是观察指针（non-owning），不负责生命周期管理
     * - **安全保证**：父对象销毁时会自动清除所有子对象的父指针
     * - 子对象不会持有悬空指针，即使父对象先销毁也是安全的
     * - 无需手动在父对象销毁前调用 SetParent(nullptr)
     * 
     * @example
     * @code
     * Transform parent, child;
     * child.SetParent(&parent);  // 正确
     * child.SetParent(&child);   // 错误：自引用，会被拒绝
     * 
     * // 正确的清理顺序
     * child.SetParent(nullptr);  // 先解除父子关系
     * // 然后可以安全销毁 parent
     * @endcode
     */
    void SetParent(Transform* parent);
    
    /**
     * @brief 获取父变换
     * @return 父变换指针（可能为 nullptr）
     */
    Transform* GetParent() const { 
        return m_parent.load(std::memory_order_acquire); 
    }
    
    /**
     * @brief 是否有父变换
     * @return 如果有父变换返回 true
     */
    bool HasParent() const { 
        return m_parent.load(std::memory_order_acquire) != nullptr; 
    }
    
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
    
    // 父变换指针（原子类型，保证多线程读写安全）
    // 注意：这是观察指针（non-owning），不负责生命周期管理
    // 父对象销毁时会自动通知子对象清除父指针
    std::atomic<Transform*> m_parent;
    
    // 子对象列表（用于生命周期管理）
    // 父对象销毁时通知所有子对象清除父指针，避免悬空指针
    std::vector<Transform*> m_children;
    
    // 缓存系统：使用 dirty flag 避免重复计算
    mutable std::atomic<bool> m_dirtyLocal;  // 本地矩阵是否需要更新
    mutable std::atomic<bool> m_dirtyWorld;  // 世界矩阵是否需要更新
    
    mutable Matrix4 m_localMatrix;   // 缓存的本地矩阵
    mutable Matrix4 m_worldMatrix;   // 缓存的世界矩阵
    
    // 线程安全：使用递归互斥锁保护数据访问
    // 使用递归锁允许同一线程多次获取锁，避免在递归调用（如GetWorldPosition调用父对象的GetWorldPosition）时死锁
    mutable std::recursive_mutex m_mutex;  // 主锁：保护基本成员变量
    
    void MarkDirty();
    void MarkDirtyNoLock();  // 无锁版本，供内部已加锁的方法调用
    
    // 子对象管理（私有方法，用于生命周期管理）
    void AddChild(Transform* child);
    void RemoveChild(Transform* child);
    void NotifyChildrenParentDestroyed();
};

} // namespace Render

