#include "render/transform.h"
#include "render/error.h"
#include <algorithm>  // for std::find

namespace Render {

// ============================================================================
// 构造和初始化
// ============================================================================

Transform::Transform()
    : m_position(Vector3::Zero())
    , m_rotation(Quaternion::Identity())
    , m_scale(Vector3::Ones())
    , m_parent(nullptr)  // 原子类型可以用 nullptr 初始化
    , m_dirtyLocal(true)
    , m_dirtyWorld(true)
{
}

Transform::Transform(const Vector3& position, const Quaternion& rotation, const Vector3& scale)
    : m_position(position)
    , m_rotation(rotation)
    , m_scale(scale)
    , m_parent(nullptr)  // 原子类型可以用 nullptr 初始化
    , m_dirtyLocal(true)
    , m_dirtyWorld(true)
{
}

Transform::~Transform() {
    // 通知所有子对象：父对象即将销毁
    NotifyChildrenParentDestroyed();
    
    // 从父对象的子对象列表中移除自己
    Transform* parent = m_parent.load(std::memory_order_acquire);
    if (parent) {
        parent->RemoveChild(this);
    }
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
    
    // 使用原子操作读取父指针
    Transform* parent = m_parent.load(std::memory_order_acquire);
    if (parent) {
        // 递归调用父对象方法（父对象有自己的递归锁）
        Vector3 parentPos = parent->GetWorldPosition();
        Quaternion parentRot = parent->GetWorldRotation();
        Vector3 parentScale = parent->GetWorldScale();
        
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
    Transform* parent = m_parent.load(std::memory_order_acquire);
    if (parent) {
        localTranslation = parent->InverseTransformDirection(translation);
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
    
    // 检查四元数的模长，避免零四元数导致除零错误
    float norm = rotation.norm();
    if (norm < MathUtils::EPSILON) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
            "Transform::SetRotation: 四元数接近零向量，使用单位四元数替代"));
        m_rotation = Quaternion::Identity();
    } else {
        // 手动归一化：访问系数并除以模长
        float invNorm = 1.0f / norm;
        m_rotation = Quaternion(
            rotation.w() * invNorm,
            rotation.x() * invNorm,
            rotation.y() * invNorm,
            rotation.z() * invNorm
        );
    }
    
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
    
    Transform* parent = m_parent.load(std::memory_order_acquire);
    if (parent) {
        return parent->GetWorldRotation() * m_rotation;
    }
    return m_rotation;
}

void Transform::Rotate(const Quaternion& rotation) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    // 应用旋转增量
    Quaternion newRotation = m_rotation * rotation;
    
    // 检查结果四元数的模长
    float norm = newRotation.norm();
    if (norm < MathUtils::EPSILON) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
            "Transform::Rotate: 旋转结果异常，保持原旋转"));
        return;
    }
    
    // 手动归一化
    float invNorm = 1.0f / norm;
    m_rotation = Quaternion(
        newRotation.w() * invNorm,
        newRotation.x() * invNorm,
        newRotation.y() * invNorm,
        newRotation.z() * invNorm
    );
    MarkDirtyNoLock();
}

void Transform::RotateAround(const Vector3& axis, float angle) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    // 检查旋转轴是否有效
    if (axis.squaredNorm() < MathUtils::EPSILON) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
            "Transform::RotateAround: 旋转轴接近零向量，操作被忽略"));
        return;
    }
    
    Quaternion rot = MathUtils::AngleAxis(angle, axis.normalized());
    Quaternion newRotation = m_rotation * rot;
    
    float norm = newRotation.norm();
    if (norm < MathUtils::EPSILON) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
            "Transform::RotateAround: 旋转结果异常，保持原旋转"));
        return;
    }
    
    // 手动归一化
    float invNorm = 1.0f / norm;
    m_rotation = Quaternion(
        newRotation.w() * invNorm,
        newRotation.x() * invNorm,
        newRotation.y() * invNorm,
        newRotation.z() * invNorm
    );
    MarkDirtyNoLock();
}

void Transform::RotateAroundWorld(const Vector3& axis, float angle) {
    // 使用递归锁，在持锁状态下安全访问父对象
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    // 检查旋转轴是否有效
    if (axis.squaredNorm() < MathUtils::EPSILON) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
            "Transform::RotateAroundWorld: 旋转轴接近零向量，操作被忽略"));
        return;
    }
    
    Quaternion rot = MathUtils::AngleAxis(angle, axis.normalized());
    
    Transform* parent = m_parent.load(std::memory_order_acquire);
    if (parent) {
        Quaternion parentRot = parent->GetWorldRotation();
        Quaternion worldRot = parentRot * m_rotation;
        worldRot = rot * worldRot;
        
        float worldNorm = worldRot.norm();
        if (worldNorm < MathUtils::EPSILON) {
            HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
                "Transform::RotateAroundWorld: 世界旋转结果异常，保持原旋转"));
            return;
        }
        // 手动归一化世界旋转
        float invWorldNorm = 1.0f / worldNorm;
        worldRot = Quaternion(
            worldRot.w() * invWorldNorm,
            worldRot.x() * invWorldNorm,
            worldRot.y() * invWorldNorm,
            worldRot.z() * invWorldNorm
        );
        
        Quaternion localRot = parentRot.inverse() * worldRot;
        float localNorm = localRot.norm();
        if (localNorm < MathUtils::EPSILON) {
            HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
                "Transform::RotateAroundWorld: 本地旋转结果异常，保持原旋转"));
            return;
        }
        
        // 手动归一化本地旋转
        float invLocalNorm = 1.0f / localNorm;
        m_rotation = Quaternion(
            localRot.w() * invLocalNorm,
            localRot.x() * invLocalNorm,
            localRot.y() * invLocalNorm,
            localRot.z() * invLocalNorm
        );
    } else {
        Quaternion newRotation = rot * m_rotation;
        float norm = newRotation.norm();
        if (norm < MathUtils::EPSILON) {
            HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
                "Transform::RotateAroundWorld: 旋转结果异常，保持原旋转"));
            return;
        }
        // 手动归一化
        float invNorm = 1.0f / norm;
        m_rotation = Quaternion(
            newRotation.w() * invNorm,
            newRotation.x() * invNorm,
            newRotation.y() * invNorm,
            newRotation.z() * invNorm
        );
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
    
    Transform* parent = m_parent.load(std::memory_order_acquire);
    if (parent) {
        Quaternion parentRot = parent->GetWorldRotation();
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
    
    Transform* parent = m_parent.load(std::memory_order_acquire);
    if (parent) {
        Vector3 parentScale = parent->GetWorldScale();
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
    
    Transform* parent = m_parent.load(std::memory_order_acquire);
    if (parent) {
        Matrix4 parentWorldMat = parent->GetWorldMatrix();
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
    
    Transform* currentParent = m_parent.load(std::memory_order_acquire);
    if (currentParent == parent) {
        return;
    }
    
    // 检查自引用
    if (parent == this) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
            "Transform::SetParent: 不能将自己设置为父对象"));
        return;
    }
    
    // 检查循环引用（遍历祖先链）
    if (parent != nullptr) {
        Transform* ancestor = parent;
        int depth = 0;
        const int MAX_DEPTH = 1000;  // 防止无限循环，同时限制层级深度
        
        while (ancestor != nullptr && depth < MAX_DEPTH) {
            if (ancestor == this) {
                HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
                    "Transform::SetParent: 检测到循环引用，操作被拒绝"));
                return;
            }
            // 原子读取祖先的父指针
            ancestor = ancestor->m_parent.load(std::memory_order_acquire);
            depth++;
        }
        
        if (depth >= MAX_DEPTH) {
            HANDLE_ERROR(RENDER_WARNING(ErrorCode::OutOfRange,
                "Transform::SetParent: 父对象层级过深（超过1000层），操作被拒绝"));
            return;
        }
    }
    
    // 从旧父对象的子对象列表中移除自己
    if (currentParent) {
        currentParent->RemoveChild(this);
    }
    
    // 添加到新父对象的子对象列表
    if (parent) {
        parent->AddChild(this);
    }
    
    // 使用原子操作存储父指针
    m_parent.store(parent, std::memory_order_release);
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
}

void Transform::MarkDirtyNoLock() {
    // 内部版本：假设调用者已经持有锁
    m_dirtyLocal.store(true, std::memory_order_release);
    m_dirtyWorld.store(true, std::memory_order_release);
}

// ============================================================================
// 子对象管理（生命周期管理）
// ============================================================================

void Transform::AddChild(Transform* child) {
    if (!child) return;
    
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    // 检查是否已存在（避免重复添加）
    auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it == m_children.end()) {
        m_children.push_back(child);
    }
}

void Transform::RemoveChild(Transform* child) {
    if (!child) return;
    
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    // 移除子对象
    auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it != m_children.end()) {
        m_children.erase(it);
    }
}

void Transform::NotifyChildrenParentDestroyed() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    // 通知所有子对象：父对象即将销毁，清除父指针
    for (Transform* child : m_children) {
        if (child) {
            // 直接设置子对象的父指针为 nullptr
            // 注意：不调用 child->SetParent(nullptr)，避免死锁和递归
            child->m_parent.store(nullptr, std::memory_order_release);
        }
    }
    
    // 清空子对象列表
    m_children.clear();
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
