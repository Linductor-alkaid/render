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
    m_position = position;
    MarkDirty();
}

Vector3 Transform::GetWorldPosition() const {
    if (m_dirtyWorldTransform) {
        UpdateWorldTransformCache();
    }
    return m_cachedWorldPosition;
}

void Transform::Translate(const Vector3& translation) {
    m_position += translation;
    MarkDirty();
}

void Transform::TranslateWorld(const Vector3& translation) {
    if (m_parent) {
        m_position += m_parent->InverseTransformDirection(translation);
    } else {
        m_position += translation;
    }
    MarkDirty();
}

// ============================================================================
// 旋转操作
// ============================================================================

void Transform::SetRotation(const Quaternion& rotation) {
    m_rotation = rotation.normalized();
    MarkDirty();
}

void Transform::SetRotationEuler(const Vector3& euler) {
    m_rotation = MathUtils::FromEuler(euler.x(), euler.y(), euler.z());
    MarkDirty();
}

void Transform::SetRotationEulerDegrees(const Vector3& euler) {
    m_rotation = MathUtils::FromEulerDegrees(euler.x(), euler.y(), euler.z());
    MarkDirty();
}

Vector3 Transform::GetRotationEuler() const {
    return MathUtils::ToEuler(m_rotation);
}

Vector3 Transform::GetRotationEulerDegrees() const {
    return MathUtils::ToEulerDegrees(m_rotation);
}

Quaternion Transform::GetWorldRotation() const {
    if (m_dirtyWorldTransform) {
        UpdateWorldTransformCache();
    }
    return m_cachedWorldRotation;
}

void Transform::Rotate(const Quaternion& rotation) {
    m_rotation = (m_rotation * rotation).normalized();
    MarkDirty();
}

void Transform::RotateAround(const Vector3& axis, float angle) {
    Quaternion rot = MathUtils::AngleAxis(angle, axis);
    m_rotation = (m_rotation * rot).normalized();
    MarkDirty();
}

void Transform::RotateAroundWorld(const Vector3& axis, float angle) {
    Quaternion rot = MathUtils::AngleAxis(angle, axis);
    if (m_parent) {
        Quaternion parentRot = m_parent->GetWorldRotation();
        Quaternion worldRot = parentRot * m_rotation;
        worldRot = (rot * worldRot).normalized();
        m_rotation = (parentRot.inverse() * worldRot).normalized();
    } else {
        m_rotation = (rot * m_rotation).normalized();
    }
    MarkDirty();
}

void Transform::LookAt(const Vector3& target, const Vector3& up) {
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
    
    MarkDirty();
}

// ============================================================================
// 缩放操作
// ============================================================================

void Transform::SetScale(const Vector3& scale) {
    m_scale = scale;
    MarkDirty();
}

void Transform::SetScale(float scale) {
    m_scale = Vector3(scale, scale, scale);
    MarkDirty();
}

Vector3 Transform::GetWorldScale() const {
    if (m_dirtyWorldTransform) {
        UpdateWorldTransformCache();
    }
    return m_cachedWorldScale;
}

// ============================================================================
// 方向向量
// ============================================================================

Vector3 Transform::GetForward() const {
    return m_rotation * Vector3::UnitZ();
}

Vector3 Transform::GetRight() const {
    return m_rotation * Vector3::UnitX();
}

Vector3 Transform::GetUp() const {
    return m_rotation * Vector3::UnitY();
}

// ============================================================================
// 矩阵操作
// ============================================================================

Matrix4 Transform::GetLocalMatrix() const {
    if (m_dirtyLocal) {
        m_localMatrix = MathUtils::TRS(m_position, m_rotation, m_scale);
        m_dirtyLocal = false;
    }
    return m_localMatrix;
}

Matrix4 Transform::GetWorldMatrix() const {
    if (m_dirtyWorld) {
        if (m_parent) {
            m_worldMatrix = m_parent->GetWorldMatrix() * GetLocalMatrix();
        } else {
            m_worldMatrix = GetLocalMatrix();
        }
        m_dirtyWorld = false;
    }
    return m_worldMatrix;
}

void Transform::SetFromMatrix(const Matrix4& matrix) {
    MathUtils::DecomposeMatrix(matrix, m_position, m_rotation, m_scale);
    MarkDirty();
}

// ============================================================================
// 父子关系
// ============================================================================

void Transform::SetParent(Transform* parent) {
    if (m_parent == parent) {
        return;
    }
    
    // 如果要保持世界变换不变，需要先转换为世界坐标
    // 这里采用简单的方式，直接设置父对象
    m_parent = parent;
    MarkDirty();
}

// ============================================================================
// 坐标变换
// ============================================================================

Vector3 Transform::TransformPoint(const Vector3& localPoint) const {
    Matrix4 worldMat = GetWorldMatrix();
    Vector4 point(localPoint.x(), localPoint.y(), localPoint.z(), 1.0f);
    Vector4 result = worldMat * point;
    return Vector3(result.x(), result.y(), result.z());
}

Vector3 Transform::TransformDirection(const Vector3& localDirection) const {
    Quaternion worldRot = GetWorldRotation();
    return worldRot * localDirection;
}

Vector3 Transform::InverseTransformPoint(const Vector3& worldPoint) const {
    Matrix4 worldMat = GetWorldMatrix();
    Matrix4 invMat = worldMat.inverse();
    Vector4 point(worldPoint.x(), worldPoint.y(), worldPoint.z(), 1.0f);
    Vector4 result = invMat * point;
    return Vector3(result.x(), result.y(), result.z());
}

Vector3 Transform::InverseTransformDirection(const Vector3& worldDirection) const {
    Quaternion worldRot = GetWorldRotation();
    return worldRot.inverse() * worldDirection;
}

// ============================================================================
// 私有辅助函数
// ============================================================================

void Transform::MarkDirty() {
    m_dirtyLocal = true;
    m_dirtyWorld = true;
    m_dirtyWorldTransform = true;
}

void Transform::UpdateWorldTransformCache() const {
    if (m_parent) {
        // 获取父节点的世界变换（可能触发父节点的缓存更新）
        m_cachedWorldRotation = m_parent->GetWorldRotation() * m_rotation;
        
        Vector3 parentScale = m_parent->GetWorldScale();
        m_cachedWorldScale = Vector3(
            m_scale.x() * parentScale.x(),
            m_scale.y() * parentScale.y(),
            m_scale.z() * parentScale.z()
        );
        
        // 世界位置 = 父世界位置 + 父世界旋转 * (父世界缩放 * 本地位置)
        Vector3 scaledPosition = Vector3(
            m_position.x() * parentScale.x(),
            m_position.y() * parentScale.y(),
            m_position.z() * parentScale.z()
        );
        m_cachedWorldPosition = m_parent->GetWorldPosition() + 
                                m_parent->GetWorldRotation() * scaledPosition;
    } else {
        m_cachedWorldRotation = m_rotation;
        m_cachedWorldScale = m_scale;
        m_cachedWorldPosition = m_position;
    }
    
    m_dirtyWorldTransform = false;
}

// ============================================================================
// 批量变换操作
// ============================================================================

void Transform::TransformPoints(const std::vector<Vector3>& localPoints, 
                                std::vector<Vector3>& worldPoints) const {
    const Matrix4& worldMat = GetWorldMatrix();
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
    const Quaternion& worldRot = GetWorldRotation();
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

