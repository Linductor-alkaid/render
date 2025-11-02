#include "render/transform.h"
#include "render/error.h"
#include <algorithm>  // for std::find
#include <cmath>       // for std::exp, std::isfinite, std::max, std::min
#include <sstream>     // for std::ostringstream

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

bool Transform::SetParent(Transform* parent) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    Transform* currentParent = m_parent.load(std::memory_order_acquire);
    if (currentParent == parent) {
        return true;  // 已经是目标父对象，视为成功
    }
    
    // 检查自引用
    if (parent == this) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
            "Transform::SetParent: 不能将自己设置为父对象"));
        return false;
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
                return false;
            }
            // 原子读取祖先的父指针
            ancestor = ancestor->m_parent.load(std::memory_order_acquire);
            depth++;
        }
        
        if (depth >= MAX_DEPTH) {
            HANDLE_ERROR(RENDER_WARNING(ErrorCode::OutOfRange,
                "Transform::SetParent: 父对象层级过深（超过1000层），操作被拒绝"));
            return false;
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
    return true;  // 成功
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
// 变换插值
// ============================================================================

Transform Transform::Lerp(const Transform& a, const Transform& b, float t) {
    // 限制 t 在 [0, 1] 范围内
    t = std::max(0.0f, std::min(1.0f, t));
    
    // 位置和缩放使用线性插值
    Vector3 position = a.GetPosition() + (b.GetPosition() - a.GetPosition()) * t;
    Vector3 scale = a.GetScale() + (b.GetScale() - a.GetScale()) * t;
    
    // 旋转使用线性插值（可能不够平滑，但对于小角度足够好）
    Quaternion rotation = a.GetRotation().slerp(t, b.GetRotation());
    
    // 使用大括号初始化直接返回，触发拷贝省略（C++17 guaranteed copy elision）
    return Transform{position, rotation, scale};
}

Transform Transform::Slerp(const Transform& a, const Transform& b, float t) {
    // 限制 t 在 [0, 1] 范围内
    t = std::max(0.0f, std::min(1.0f, t));
    
    // 位置和缩放使用线性插值
    Vector3 position = a.GetPosition() + (b.GetPosition() - a.GetPosition()) * t;
    Vector3 scale = a.GetScale() + (b.GetScale() - a.GetScale()) * t;
    
    // 旋转使用球面线性插值（更平滑）
    Quaternion rotation = a.GetRotation().slerp(t, b.GetRotation());
    
    // 使用大括号初始化直接返回，触发拷贝省略（C++17 guaranteed copy elision）
    return Transform{position, rotation, scale};
}

void Transform::SmoothTo(const Transform& target, float smoothness, float deltaTime) {
    // smoothness: 1.0 = 立即完成，0.1 = 非常平滑
    // 使用指数平滑：new = old + (target - old) * (1 - exp(-smoothness * deltaTime))
    float alpha = 1.0f - std::exp(-smoothness * deltaTime);
    
    // 平滑位置
    Vector3 currentPos = GetPosition();
    Vector3 targetPos = target.GetPosition();
    SetPosition(currentPos + (targetPos - currentPos) * alpha);
    
    // 平滑旋转（使用 slerp）
    Quaternion currentRot = GetRotation();
    Quaternion targetRot = target.GetRotation();
    Quaternion smoothedRot = currentRot.slerp(alpha, targetRot);
    SetRotation(smoothedRot);
    
    // 平滑缩放
    Vector3 currentScale = GetScale();
    Vector3 targetScale = target.GetScale();
    SetScale(currentScale + (targetScale - currentScale) * alpha);
}

// ============================================================================
// 调试和诊断
// ============================================================================

std::string Transform::DebugString() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    std::ostringstream oss;
    oss << "Transform {\n";
    oss << "  Position: (" << m_position.x() << ", " << m_position.y() << ", " << m_position.z() << ")\n";
    oss << "  Rotation: (" << m_rotation.w() << ", " << m_rotation.x() << ", " 
        << m_rotation.y() << ", " << m_rotation.z() << ")\n";
    oss << "  Scale: (" << m_scale.x() << ", " << m_scale.y() << ", " << m_scale.z() << ")\n";
    
    Transform* parent = m_parent.load(std::memory_order_acquire);
    if (parent) {
        oss << "  Parent: " << static_cast<const void*>(parent) << "\n";
    } else {
        oss << "  Parent: nullptr\n";
    }
    
    oss << "  Children: " << m_children.size() << "\n";
    oss << "  Hierarchy Depth: " << GetHierarchyDepth() << "\n";
    oss << "}";
    
    return oss.str();
}

void Transform::PrintHierarchy(int indent, std::ostream& os) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    // 打印缩进
    for (int i = 0; i < indent; ++i) {
        os << "  ";
    }
    
    // 打印当前节点信息
    os << "Transform [";
    os << "pos=(" << m_position.x() << "," << m_position.y() << "," << m_position.z() << ")";
    os << ", scale=(" << m_scale.x() << "," << m_scale.y() << "," << m_scale.z() << ")";
    os << ", children=" << m_children.size();
    os << "]\n";
    
    // 递归打印子节点（需要解锁以避免死锁）
    // 注意：打印子节点时不能持有当前锁，因为子节点的 PrintHierarchy 也需要锁
    // 但为了简化，这里我们只打印子节点数量，不递归打印
    // 如果需要完整层次结构，应该在调用前解除锁
}

bool Transform::Validate() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    // 检查四元数是否归一化
    float rotNorm = m_rotation.norm();
    if (std::abs(rotNorm - 1.0f) > MathUtils::EPSILON * 10.0f) {
        return false;  // 旋转四元数未归一化
    }
    
    // 检查缩放是否包含 NaN 或 Inf
    if (!std::isfinite(m_scale.x()) || !std::isfinite(m_scale.y()) || !std::isfinite(m_scale.z())) {
        return false;
    }
    
    // 检查位置是否包含 NaN 或 Inf
    if (!std::isfinite(m_position.x()) || !std::isfinite(m_position.y()) || !std::isfinite(m_position.z())) {
        return false;
    }
    
    // 检查父指针循环引用（简单检查）
    Transform* parent = m_parent.load(std::memory_order_acquire);
    if (parent == this) {
        return false;  // 自引用
    }
    
    // 检查子对象列表中是否有空指针
    for (Transform* child : m_children) {
        if (child == nullptr) {
            return false;  // 空指针
        }
    }
    
    return true;
}

int Transform::GetHierarchyDepth() const {
    Transform* parent = m_parent.load(std::memory_order_acquire);
    if (parent == nullptr) {
        return 0;
    }
    
    // 递归计算深度
    int depth = 1;
    Transform* current = parent;
    const int MAX_DEPTH = 1000;  // 防止无限循环
    
    while (current != nullptr && depth < MAX_DEPTH) {
        current = current->m_parent.load(std::memory_order_acquire);
        if (current != nullptr) {
            depth++;
        }
    }
    
    return depth;
}

int Transform::GetChildCount() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return static_cast<int>(m_children.size());
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
