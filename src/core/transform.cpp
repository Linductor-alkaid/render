#include "render/transform.h"

namespace Render {

// ============================================================================
// 构造和初始化
// ============================================================================

Transform::Transform()
    : m_position(Vector3::Zero())
    , m_rotation(Quaternion::Identity())
    , m_scale(Vector3::Ones())
    , m_parent(nullptr)
    , m_dirtyLocal(true)
    , m_dirtyWorld(true)
    , m_dirtyWorldTransform(true)
    , m_cachedWorldPosition(Vector3::Zero())
    , m_cachedWorldRotation(Quaternion::Identity())
    , m_cachedWorldScale(Vector3::Ones())
{
}

Transform::Transform(const Vector3& position, const Quaternion& rotation, const Vector3& scale)
    : m_position(position)
    , m_rotation(rotation)
    , m_scale(scale)
    , m_parent(nullptr)
    , m_dirtyLocal(true)
    , m_dirtyWorld(true)
    , m_dirtyWorldTransform(true)
    , m_cachedWorldPosition(position)
    , m_cachedWorldRotation(rotation)
    , m_cachedWorldScale(scale)
{
}

// ============================================================================
// 位置操作
// ============================================================================

void Transform::SetPosition(const Vector3& position) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_position = position;
    MarkDirtyNoLock();
}

Vector3 Transform::GetWorldPosition() const {
    // 先获取父节点指针和本地位置
    Transform* parent = nullptr;
    Vector3 localPos;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        parent = m_parent;
        localPos = m_position;
    }
    
    // 递归计算（不持有任何锁）
    if (parent) {
        Vector3 parentPos = parent->GetWorldPosition();
        Quaternion parentRot = parent->GetWorldRotation();
        Vector3 parentScale = parent->GetWorldScale();
        
        Vector3 scaledPos(
            localPos.x() * parentScale.x(),
            localPos.y() * parentScale.y(),
            localPos.z() * parentScale.z()
        );
        
        return parentPos + parentRot * scaledPos;
    } else {
        return localPos;
    }
}

void Transform::Translate(const Vector3& translation) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_position += translation;
    MarkDirtyNoLock();
}

void Transform::TranslateWorld(const Vector3& translation) {
    // 先获取父节点指针（需要锁）
    Transform* parent = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        parent = m_parent;
    }
    
    // 计算变换（不持有锁）
    Vector3 localTranslation;
    if (parent) {
        localTranslation = parent->InverseTransformDirection(translation);
    } else {
        localTranslation = translation;
    }
    
    // 更新位置（持有锁）
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_position += localTranslation;
        MarkDirtyNoLock();
    }
}

// ============================================================================
// 旋转操作
// ============================================================================

void Transform::SetRotation(const Quaternion& rotation) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rotation = rotation.normalized();
    MarkDirtyNoLock();
}

void Transform::SetRotationEuler(const Vector3& euler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rotation = MathUtils::FromEuler(euler.x(), euler.y(), euler.z());
    MarkDirtyNoLock();
}

void Transform::SetRotationEulerDegrees(const Vector3& euler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rotation = MathUtils::FromEulerDegrees(euler.x(), euler.y(), euler.z());
    MarkDirtyNoLock();
}

Vector3 Transform::GetRotationEuler() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return MathUtils::ToEuler(m_rotation);
}

Vector3 Transform::GetRotationEulerDegrees() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return MathUtils::ToEulerDegrees(m_rotation);
}

Quaternion Transform::GetWorldRotation() const {
    // 先获取父节点指针和本地旋转
    Transform* parent = nullptr;
    Quaternion localRot;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        parent = m_parent;
        localRot = m_rotation;
    }
    
    // 递归计算（不持有任何锁）
    if (parent) {
        return parent->GetWorldRotation() * localRot;
    } else {
        return localRot;
    }
}

void Transform::Rotate(const Quaternion& rotation) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rotation = (m_rotation * rotation).normalized();
    MarkDirtyNoLock();
}

void Transform::RotateAround(const Vector3& axis, float angle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    Quaternion rot = MathUtils::AngleAxis(angle, axis);
    m_rotation = (m_rotation * rot).normalized();
    MarkDirtyNoLock();
}

void Transform::RotateAroundWorld(const Vector3& axis, float angle) {
    Quaternion rot = MathUtils::AngleAxis(angle, axis);
    
    // 先获取父节点指针
    Transform* parent = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        parent = m_parent;
    }
    
    // 计算新的旋转（不持有锁）
    Quaternion newRotation;
    if (parent) {
        Quaternion parentRot = parent->GetWorldRotation();
        Quaternion localRot;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            localRot = m_rotation;
        }
        Quaternion worldRot = parentRot * localRot;
        worldRot = (rot * worldRot).normalized();
        newRotation = (parentRot.inverse() * worldRot).normalized();
    } else {
        std::lock_guard<std::mutex> lock(m_mutex);
        newRotation = (rot * m_rotation).normalized();
    }
    
    // 更新旋转（持有锁）
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_rotation = newRotation;
        MarkDirtyNoLock();
    }
}

void Transform::LookAt(const Vector3& target, const Vector3& up) {
    // 先获取世界位置（不持有锁）
    Vector3 worldPos = GetWorldPosition();
    Vector3 direction = (target - worldPos).normalized();
    
    if (direction.squaredNorm() < MathUtils::EPSILON) {
        return; // 目标点与当前位置重合
    }
    
    Quaternion lookRotation = MathUtils::LookRotation(direction, up);
    
    // 获取父节点指针
    Transform* parent = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        parent = m_parent;
    }
    
    // 计算新的旋转（不持有锁）
    Quaternion newRotation;
    if (parent) {
        Quaternion parentRot = parent->GetWorldRotation();
        newRotation = parentRot.inverse() * lookRotation;
    } else {
        newRotation = lookRotation;
    }
    
    // 更新旋转（持有锁）
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_rotation = newRotation;
        MarkDirtyNoLock();
    }
}

// ============================================================================
// 缩放操作
// ============================================================================

void Transform::SetScale(const Vector3& scale) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_scale = scale;
    MarkDirtyNoLock();
}

void Transform::SetScale(float scale) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_scale = Vector3(scale, scale, scale);
    MarkDirtyNoLock();
}

Vector3 Transform::GetWorldScale() const {
    // 先获取父节点指针和本地缩放
    Transform* parent = nullptr;
    Vector3 localScale;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        parent = m_parent;
        localScale = m_scale;
    }
    
    // 递归计算（不持有任何锁）
    if (parent) {
        Vector3 parentScale = parent->GetWorldScale();
        return Vector3(
            localScale.x() * parentScale.x(),
            localScale.y() * parentScale.y(),
            localScale.z() * parentScale.z()
        );
    } else {
        return localScale;
    }
}

// ============================================================================
// 方向向量
// ============================================================================

Vector3 Transform::GetForward() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rotation * Vector3::UnitZ();
}

Vector3 Transform::GetRight() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rotation * Vector3::UnitX();
}

Vector3 Transform::GetUp() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rotation * Vector3::UnitY();
}

// ============================================================================
// 矩阵操作
// ============================================================================

Matrix4 Transform::GetLocalMatrix() const {
    // 读取数据并实时计算
    Vector3 pos, scale;
    Quaternion rot;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        pos = m_position;
        rot = m_rotation;
        scale = m_scale;
    }
    
    // 计算并返回（无锁）
    return MathUtils::TRS(pos, rot, scale);
}

Matrix4 Transform::GetWorldMatrix() const {
    // 先获取父节点指针
    Transform* parent = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        parent = m_parent;
    }
    
    // 计算矩阵（不持有任何锁）
    Matrix4 localMat = GetLocalMatrix();
    
    if (parent) {
        Matrix4 parentWorldMat = parent->GetWorldMatrix();
        return parentWorldMat * localMat;
    } else {
        return localMat;
    }
}

void Transform::SetFromMatrix(const Matrix4& matrix) {
    std::lock_guard<std::mutex> lock(m_mutex);
    MathUtils::DecomposeMatrix(matrix, m_position, m_rotation, m_scale);
    MarkDirtyNoLock();
}

// ============================================================================
// 父子关系
// ============================================================================

void Transform::SetParent(Transform* parent) {
    // 先检查是否相同
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_parent == parent) {
            return;
        }
    }
    
    // 设置父对象
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_parent = parent;
        MarkDirtyNoLock();
    }
}

// ============================================================================
// 坐标变换
// ============================================================================

Vector3 Transform::TransformPoint(const Vector3& localPoint) const {
    // GetWorldMatrix 内部已有线程安全保护
    Matrix4 worldMat = GetWorldMatrix();
    Vector4 point(localPoint.x(), localPoint.y(), localPoint.z(), 1.0f);
    Vector4 result = worldMat * point;
    return Vector3(result.x(), result.y(), result.z());
}

Vector3 Transform::TransformDirection(const Vector3& localDirection) const {
    // GetWorldRotation 内部已有线程安全保护
    Quaternion worldRot = GetWorldRotation();
    return worldRot * localDirection;
}

Vector3 Transform::InverseTransformPoint(const Vector3& worldPoint) const {
    // GetWorldMatrix 内部已有线程安全保护
    Matrix4 worldMat = GetWorldMatrix();
    Matrix4 invMat = worldMat.inverse();
    Vector4 point(worldPoint.x(), worldPoint.y(), worldPoint.z(), 1.0f);
    Vector4 result = invMat * point;
    return Vector3(result.x(), result.y(), result.z());
}

Vector3 Transform::InverseTransformDirection(const Vector3& worldDirection) const {
    // GetWorldRotation 内部已有线程安全保护
    Quaternion worldRot = GetWorldRotation();
    return worldRot.inverse() * worldDirection;
}

// ============================================================================
// 私有辅助函数
// ============================================================================

void Transform::MarkDirty() {
    // 使用原子操作标记为脏（无需加锁）
    m_dirtyLocal.store(true, std::memory_order_release);
    m_dirtyWorld.store(true, std::memory_order_release);
    m_dirtyWorldTransform.store(true, std::memory_order_release);
}

void Transform::MarkDirtyNoLock() {
    // 内部版本：假设调用者已经持有锁
    m_dirtyLocal.store(true, std::memory_order_release);
    m_dirtyWorld.store(true, std::memory_order_release);
    m_dirtyWorldTransform.store(true, std::memory_order_release);
}

void Transform::UpdateWorldTransformCache() const {
    // 此方法已废弃，保留空实现以保持二进制兼容
}

// ============================================================================
// 批量变换操作
// ============================================================================

void Transform::TransformPoints(const std::vector<Vector3>& localPoints, 
                                std::vector<Vector3>& worldPoints) const {
    // GetWorldMatrix 内部已有线程安全保护
    // 一次获取矩阵后，多线程可以安全地并行使用（只读）
    const Matrix4 worldMat = GetWorldMatrix();
    worldPoints.resize(localPoints.size());
    
    const size_t count = localPoints.size();
    
    // 只有在数据量足够大时才使用并行（避免线程创建开销）
    // 经测试，小于5000个点时，串行更快
    #ifdef _OPENMP
    if (count > 5000) {
        #pragma omp parallel for
        for (int i = 0; i < static_cast<int>(count); ++i) {
            const Vector3& local = localPoints[i];
            Vector4 point(local.x(), local.y(), local.z(), 1.0f);
            Vector4 result = worldMat * point;
            worldPoints[i] = Vector3(result.x(), result.y(), result.z());
        }
        return;
    }
    #endif
    
    // 串行处理（小批量或无 OpenMP）
    for (size_t i = 0; i < count; ++i) {
        const Vector3& local = localPoints[i];
        Vector4 point(local.x(), local.y(), local.z(), 1.0f);
        Vector4 result = worldMat * point;
        worldPoints[i] = Vector3(result.x(), result.y(), result.z());
    }
}

void Transform::TransformDirections(const std::vector<Vector3>& localDirections,
                                    std::vector<Vector3>& worldDirections) const {
    // GetWorldRotation 内部已有线程安全保护
    // 一次获取旋转后，多线程可以安全地并行使用（只读）
    const Quaternion worldRot = GetWorldRotation();
    worldDirections.resize(localDirections.size());
    
    const size_t count = localDirections.size();
    
    #ifdef _OPENMP
    if (count > 5000) {
        #pragma omp parallel for
        for (int i = 0; i < static_cast<int>(count); ++i) {
            worldDirections[i] = worldRot * localDirections[i];
        }
        return;
    }
    #endif
    
    // 串行处理
    for (size_t i = 0; i < count; ++i) {
        worldDirections[i] = worldRot * localDirections[i];
    }
}

} // namespace Render
