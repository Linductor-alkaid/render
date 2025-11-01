#include "render/transform.h"
#include "render/error.h"

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
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_position = position;
    MarkDirtyNoLock();
}

Vector3 Transform::GetWorldPosition() const {
    // 使用递归锁，允许在持锁状态下递归调用父对象的方法
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    if (m_parent) {
        // 递归调用父对象方法（父对象有自己的递归锁）
        Vector3 parentPos = m_parent->GetWorldPosition();
        Quaternion parentRot = m_parent->GetWorldRotation();
        Vector3 parentScale = m_parent->GetWorldScale();
        
        Vector3 scaledPos(
            m_position.x() * parentScale.x(),
            m_position.y() * parentScale.y(),
            m_position.z() * parentScale.z()
        );
        
        return parentPos + parentRot * scaledPos;
    }
    return m_position;
}

void Transform::Translate(const Vector3& translation) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_position += translation;
    MarkDirtyNoLock();
}

void Transform::TranslateWorld(const Vector3& translation) {
    // 使用递归锁，在持锁状态下安全访问父对象
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    Vector3 localTranslation;
    if (m_parent) {
        localTranslation = m_parent->InverseTransformDirection(translation);
    } else {
        localTranslation = translation;
    }
    
    m_position += localTranslation;
    MarkDirtyNoLock();
}

// ============================================================================
// 旋转操作
// ============================================================================

void Transform::SetRotation(const Quaternion& rotation) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_rotation = rotation.normalized();
    MarkDirtyNoLock();
}

void Transform::SetRotationEuler(const Vector3& euler) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_rotation = MathUtils::FromEuler(euler.x(), euler.y(), euler.z());
    MarkDirtyNoLock();
}

void Transform::SetRotationEulerDegrees(const Vector3& euler) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_rotation = MathUtils::FromEulerDegrees(euler.x(), euler.y(), euler.z());
    MarkDirtyNoLock();
}

Vector3 Transform::GetRotationEuler() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return MathUtils::ToEuler(m_rotation);
}

Vector3 Transform::GetRotationEulerDegrees() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return MathUtils::ToEulerDegrees(m_rotation);
}

Quaternion Transform::GetWorldRotation() const {
    // 使用递归锁，允许在持锁状态下递归调用父对象的方法
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    if (m_parent) {
        return m_parent->GetWorldRotation() * m_rotation;
    }
    return m_rotation;
}

void Transform::Rotate(const Quaternion& rotation) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_rotation = (m_rotation * rotation).normalized();
    MarkDirtyNoLock();
}

void Transform::RotateAround(const Vector3& axis, float angle) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    Quaternion rot = MathUtils::AngleAxis(angle, axis);
    m_rotation = (m_rotation * rot).normalized();
    MarkDirtyNoLock();
}

void Transform::RotateAroundWorld(const Vector3& axis, float angle) {
    // 使用递归锁，在持锁状态下安全访问父对象
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    Quaternion rot = MathUtils::AngleAxis(angle, axis);
    
    if (m_parent) {
        Quaternion parentRot = m_parent->GetWorldRotation();
        Quaternion worldRot = parentRot * m_rotation;
        worldRot = (rot * worldRot).normalized();
        m_rotation = (parentRot.inverse() * worldRot).normalized();
    } else {
        m_rotation = (rot * m_rotation).normalized();
    }
    
    MarkDirtyNoLock();
}

void Transform::LookAt(const Vector3& target, const Vector3& up) {
    // 使用递归锁，在持锁状态下安全访问父对象
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    // 获取世界位置（递归锁允许在持锁状态下调用）
    Vector3 worldPos = GetWorldPosition();
    Vector3 direction = (target - worldPos).normalized();
    
    if (direction.squaredNorm() < MathUtils::EPSILON) {
        return; // 目标点与当前位置重合
    }
    
    Quaternion lookRotation = MathUtils::LookRotation(direction, up);
    
    if (m_parent) {
        Quaternion parentRot = m_parent->GetWorldRotation();
        m_rotation = parentRot.inverse() * lookRotation;
    } else {
        m_rotation = lookRotation;
    }
    
    MarkDirtyNoLock();
}

// ============================================================================
// 缩放操作
// ============================================================================

void Transform::SetScale(const Vector3& scale) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_scale = scale;
    MarkDirtyNoLock();
}

void Transform::SetScale(float scale) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_scale = Vector3(scale, scale, scale);
    MarkDirtyNoLock();
}

Vector3 Transform::GetWorldScale() const {
    // 使用递归锁，允许在持锁状态下递归调用父对象的方法
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    if (m_parent) {
        Vector3 parentScale = m_parent->GetWorldScale();
        return Vector3(
            m_scale.x() * parentScale.x(),
            m_scale.y() * parentScale.y(),
            m_scale.z() * parentScale.z()
        );
    }
    return m_scale;
}

// ============================================================================
// 方向向量
// ============================================================================

Vector3 Transform::GetForward() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_rotation * Vector3::UnitZ();
}

Vector3 Transform::GetRight() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_rotation * Vector3::UnitX();
}

Vector3 Transform::GetUp() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_rotation * Vector3::UnitY();
}

// ============================================================================
// 矩阵操作
// ============================================================================

Matrix4 Transform::GetLocalMatrix() const {
    // 使用递归锁保护数据访问
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return MathUtils::TRS(m_position, m_rotation, m_scale);
}

Matrix4 Transform::GetWorldMatrix() const {
    // 使用递归锁，允许在持锁状态下递归调用父对象的方法
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    Matrix4 localMat = GetLocalMatrix();
    
    if (m_parent) {
        Matrix4 parentWorldMat = m_parent->GetWorldMatrix();
        return parentWorldMat * localMat;
    }
    return localMat;
}

void Transform::SetFromMatrix(const Matrix4& matrix) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    MathUtils::DecomposeMatrix(matrix, m_position, m_rotation, m_scale);
    MarkDirtyNoLock();
}

// ============================================================================
// 父子关系
// ============================================================================

void Transform::SetParent(Transform* parent) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    if (m_parent == parent) {
        return;
    }
    
    m_parent = parent;
    MarkDirtyNoLock();
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
