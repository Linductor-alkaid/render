#include "render/transform.h"
#include "render/error.h"
#include <algorithm>  // for std::find, std::remove, std::sort, std::reverse
#include <cmath>       // for std::exp, std::isfinite, std::max, std::min
#include <sstream>     // for std::ostringstream

namespace Render {

// 静态成员初始化
std::atomic<uint64_t> Transform::s_nextGlobalId{1};

// ============================================================================
// 构造和初始化
// ============================================================================

Transform::Transform()
    : m_globalId(s_nextGlobalId.fetch_add(1, std::memory_order_relaxed))
    , m_position(Vector3::Zero())
    , m_rotation(Quaternion::Identity())
    , m_scale(Vector3::Ones())
    , m_node(std::make_shared<TransformNode>(this))
    , m_dirtyLocal(true)
    , m_dirtyWorld(true)
    , m_dirtyWorldTransform(true)
    , m_cachedWorldPosition(Vector3::Zero())
    , m_cachedWorldRotation(Quaternion::Identity())
    , m_cachedWorldScale(Vector3::Ones())
    , m_worldCache{}
    , m_localVersion(0)
{
    m_node->shared_this = m_node;  // 允许从内部获取 shared_ptr
}

Transform::Transform(const Vector3& position, const Quaternion& rotation, const Vector3& scale)
    : m_globalId(s_nextGlobalId.fetch_add(1, std::memory_order_relaxed))
    , m_position(position)
    , m_rotation(rotation)
    , m_scale(scale)
    , m_node(std::make_shared<TransformNode>(this))
    , m_dirtyLocal(true)
    , m_dirtyWorld(true)
    , m_dirtyWorldTransform(true)
    , m_cachedWorldPosition(Vector3::Zero())
    , m_cachedWorldRotation(Quaternion::Identity())
    , m_cachedWorldScale(Vector3::Ones())
    , m_worldCache{}
    , m_localVersion(0)
{
    m_node->shared_this = m_node;  // 允许从内部获取 shared_ptr
}

Transform::~Transform() {
    if (m_node) {
        m_node->destroyed.store(true, std::memory_order_release);
        
        // 安全地通知子节点（使用层级锁）
        std::lock_guard<std::mutex> hierarchyLock(m_hierarchyMutex);
        for (auto& childNode : m_node->children) {
            if (childNode && !childNode->destroyed.load(std::memory_order_acquire)) {
                childNode->parent.reset();
            }
        }
        m_node->children.clear();
        
        // 从父节点移除
        if (auto parentNode = m_node->parent.lock()) {
            if (!parentNode->destroyed.load(std::memory_order_acquire)) {
                if (parentNode->transform) {
                    parentNode->transform->RemoveChild(this);
                }
            }
        }
    }
}

// ============================================================================
// 位置操作
// ============================================================================

void Transform::SetPosition(const Vector3& position) {
    std::unique_lock<std::shared_mutex> lock(m_dataMutex);
    
    // 检测 NaN/Inf
    if (!std::isfinite(position.x()) || !std::isfinite(position.y()) || !std::isfinite(position.z())) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
            "Transform::SetPosition: 位置包含 NaN 或 Inf，操作被忽略"));
        return;
    }
    
    m_position = position;
    MarkDirtyNoLock();
    lock.unlock();  // 释放数据锁，因为InvalidateChildrenCache需要层级锁
    
    // 递归使所有子节点的缓存失效（因为父节点变化会影响所有子节点的世界变换）
    InvalidateChildrenCache();
}

Vector3 Transform::GetWorldPosition() const {
    // 第一步：乐观无锁读取（快速路径）
    // 先检查本地版本号（原子操作，无需加锁）
    uint64_t localVer = m_localVersion.load(std::memory_order_acquire);
    
    // 使用读锁保护缓存读取，确保内存可见性
    std::shared_lock<std::shared_mutex> lock(m_dataMutex);
    uint64_t cachedVersion = m_worldCache.version;
    
    // 检查本地是否变化
    if (!m_dirtyWorldTransform.load(std::memory_order_acquire) && 
        cachedVersion == localVer) {
        auto parentNode = m_node ? m_node->parent.lock() : nullptr;
        if (!parentNode || 
            m_worldCache.parentVersion == parentNode->transform->m_localVersion.load(std::memory_order_acquire)) {
            return m_worldCache.position;
        }
    }
    lock.unlock();  // 释放读锁，进入慢速路径
    
    // 第二步：慢速路径 - 需要重新计算
    return GetWorldPositionSlow();
}

Vector3 Transform::GetWorldPositionSlow() const {
    // P0: 检查当前节点是否已销毁
    if (m_node && m_node->destroyed.load(std::memory_order_acquire)) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
            "Transform::GetWorldPositionSlow: 对象已被销毁"));
        return Vector3::Zero();
    }
    
    // 收集祖先链（不持有锁，因为parent指针通过weak_ptr访问是线程安全的）
    std::vector<Transform*> chain;
    chain.reserve(32);
    
    Transform* current = const_cast<Transform*>(this);
    while (current && chain.size() < 1000) {
        chain.push_back(current);
        auto node = GetNode(current);
        if (node) {
            // P0: 检查节点是否已销毁
            if (node->destroyed.load(std::memory_order_acquire)) {
                break;
            }
            // parent指针通过weak_ptr访问，线程安全，无需加锁
            auto parentNode = node->parent.lock();
            if (parentNode && !parentNode->destroyed.load(std::memory_order_acquire)) {
                current = parentNode->transform;
            } else {
                current = nullptr;
            }
        } else {
            current = nullptr;
        }
    }
    
    // 按 ID 顺序锁定整个链（避免死锁）
    std::vector<std::shared_lock<std::shared_mutex>> locks;
    locks.reserve(chain.size());
    
    // 按ID排序
    std::sort(chain.begin(), chain.end(), 
        [](const Transform* a, const Transform* b) {
            return a->m_globalId < b->m_globalId;
        });
    
    // 按顺序加锁（现在可以安全地加锁，因为之前没有持有任何锁）
    for (auto* node : chain) {
        locks.emplace_back(node->m_dataMutex);
    }
    
    // 现在安全地从根到叶计算（需要重新按层级顺序排列）
    // 先找到根节点（没有父节点的节点）
    std::vector<Transform*> hierarchyChain;
    hierarchyChain.reserve(chain.size());
    
    // 找到根节点
    Transform* root = nullptr;
    for (auto* node : chain) {
        auto nodePtr = GetNode(node);
        if (nodePtr) {
            auto parentNode = nodePtr->parent.lock();
            if (!parentNode) {
                root = node;
                break;
            }
        }
    }
    
    // 如果没找到根，使用第一个节点
    if (!root && !chain.empty()) {
        root = chain[0];
    }
    
    // 从根开始构建层级链
    if (root) {
        Transform* iter = root;
        while (iter && hierarchyChain.size() < chain.size()) {
            hierarchyChain.push_back(iter);
            
            // 找到当前节点的子节点（在chain中）
            bool found = false;
            for (auto* node : chain) {
                if (node == iter) continue;
                auto nodePtr = GetNode(node);
                if (nodePtr) {
                    auto parentNode = nodePtr->parent.lock();
                    if (parentNode && parentNode->transform == iter) {
                        iter = node;
                        found = true;
                        break;
                    }
                }
            }
            if (!found) break;
        }
    }
    
    // 如果层级链构建失败，使用原始链的反向
    if (hierarchyChain.empty()) {
        hierarchyChain = chain;
        std::reverse(hierarchyChain.begin(), hierarchyChain.end());
    }
    
    // 计算世界变换
    Vector3 worldPos = Vector3::Zero();
    Quaternion worldRot = Quaternion::Identity();
    Vector3 worldScale = Vector3::Ones();
    
    for (size_t i = 0; i < hierarchyChain.size(); ++i) {
        Transform* node = hierarchyChain[i];
        
        if (i == 0) {
            worldPos = node->m_position;
            worldRot = node->m_rotation;
            worldScale = node->m_scale;
        } else {
            Vector3 scaledPos = worldScale.cwiseProduct(node->m_position);
            worldPos = worldPos + worldRot * scaledPos;
            worldRot = worldRot * node->m_rotation;
            worldScale = worldScale.cwiseProduct(node->m_scale);
        }
    }
    
    // 先释放所有读锁，然后获取写锁更新缓存（避免从读锁升级到写锁导致死锁）
    locks.clear();  // 释放所有读锁
    
    // 更新缓存（仅对自己，需要写锁）
    {
        std::unique_lock<std::shared_mutex> writeLock(m_dataMutex);
        m_worldCache.position = worldPos;
        m_worldCache.rotation = worldRot;
        m_worldCache.scale = worldScale;
        m_worldCache.version = m_localVersion.load(std::memory_order_relaxed);
        
        auto parentNode = m_node ? m_node->parent.lock() : nullptr;
        m_worldCache.parentVersion = parentNode ? 
            parentNode->transform->m_localVersion.load(std::memory_order_acquire) : 0;
        
        // 同时更新旧的缓存（向后兼容）
        m_cachedWorldPosition = worldPos;
        m_cachedWorldRotation = worldRot;
        m_cachedWorldScale = worldScale;
        m_dirtyWorldTransform.store(false, std::memory_order_release);
    }
    
    return worldPos;
}

Quaternion Transform::GetWorldRotationSlow() const {
    // P0: 检查当前节点是否已销毁
    if (m_node && m_node->destroyed.load(std::memory_order_acquire)) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
            "Transform::GetWorldRotationSlow: 对象已被销毁"));
        return Quaternion::Identity();
    }
    
    // 复用GetWorldPositionSlow的逻辑,但只返回旋转
    // 为了效率,我们直接调用GetWorldPositionSlow来更新缓存,然后返回缓存的旋转
    GetWorldPositionSlow();  // 这会更新所有缓存
    return m_worldCache.rotation;
}

Vector3 Transform::GetWorldScaleSlow() const {
    // P0: 检查当前节点是否已销毁
    if (m_node && m_node->destroyed.load(std::memory_order_acquire)) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
            "Transform::GetWorldScaleSlow: 对象已被销毁"));
        return Vector3::Ones();
    }
    
    // 复用GetWorldPositionSlow的逻辑,但只返回缩放
    // 为了效率,我们直接调用GetWorldPositionSlow来更新缓存,然后返回缓存的缩放
    GetWorldPositionSlow();  // 这会更新所有缓存
    return m_worldCache.scale;
}

Vector3 Transform::GetWorldPositionIterative() const {
    // 迭代版本：使用新的慢速路径实现
    return GetWorldPositionSlow();
}

void Transform::Translate(const Vector3& translation) {
    std::unique_lock<std::shared_mutex> lock(m_dataMutex);
    m_position += translation;
    MarkDirtyNoLock();
    lock.unlock();  // 释放数据锁
    
    // 递归使所有子节点的缓存失效
    InvalidateChildrenCache();
}

void Transform::TranslateWorld(const Vector3& translation) {
    // 使用写锁保护数据访问
    std::unique_lock<std::shared_mutex> lock(m_dataMutex);
    
    Vector3 localTranslation;
    auto parentNode = m_node ? m_node->parent.lock() : nullptr;
    if (parentNode && parentNode->transform) {
        // 需要访问父对象，先释放当前锁
        lock.unlock();
        localTranslation = parentNode->transform->InverseTransformDirection(translation);
        lock.lock();
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
    std::unique_lock<std::shared_mutex> lock(m_dataMutex);
    
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
    lock.unlock();  // 释放数据锁
    
    // 递归使所有子节点的缓存失效
    InvalidateChildrenCache();
}

void Transform::SetRotationEuler(const Vector3& euler) {
    std::unique_lock<std::shared_mutex> lock(m_dataMutex);
    m_rotation = MathUtils::FromEuler(euler.x(), euler.y(), euler.z());
    MarkDirtyNoLock();
    lock.unlock();  // 释放数据锁
    
    // 递归使所有子节点的缓存失效
    InvalidateChildrenCache();
}

void Transform::SetRotationEulerDegrees(const Vector3& euler) {
    std::unique_lock<std::shared_mutex> lock(m_dataMutex);
    m_rotation = MathUtils::FromEulerDegrees(euler.x(), euler.y(), euler.z());
    MarkDirtyNoLock();
    lock.unlock();  // 释放数据锁
    
    // 递归使所有子节点的缓存失效
    InvalidateChildrenCache();
}

Vector3 Transform::GetRotationEuler() const {
    std::shared_lock<std::shared_mutex> lock(m_dataMutex);
    return MathUtils::ToEuler(m_rotation);
}

Vector3 Transform::GetRotationEulerDegrees() const {
    std::shared_lock<std::shared_mutex> lock(m_dataMutex);
    return MathUtils::ToEulerDegrees(m_rotation);
}

Quaternion Transform::GetWorldRotation() const {
    // 第一步：乐观无锁读取（快速路径）
    uint64_t cachedVersion = m_worldCache.version;
    uint64_t localVer = m_localVersion.load(std::memory_order_acquire);
    
    // 检查本地是否变化
    if (!m_dirtyWorldTransform.load(std::memory_order_acquire) && 
        cachedVersion == localVer) {
        auto parentNode = m_node ? m_node->parent.lock() : nullptr;
        if (!parentNode || 
            m_worldCache.parentVersion == parentNode->transform->m_localVersion.load(std::memory_order_acquire)) {
            return m_worldCache.rotation;
        }
    }
    
    // 第二步：慢速路径 - 需要重新计算
    return GetWorldRotationSlow();
}

void Transform::Rotate(const Quaternion& rotation) {
    std::unique_lock<std::shared_mutex> lock(m_dataMutex);
    
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
    std::unique_lock<std::shared_mutex> lock(m_dataMutex);
    
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
    // 使用写锁保护数据访问
    std::unique_lock<std::shared_mutex> lock(m_dataMutex);
    
    // 检查旋转轴是否有效
    if (axis.squaredNorm() < MathUtils::EPSILON) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
            "Transform::RotateAroundWorld: 旋转轴接近零向量，操作被忽略"));
        return;
    }
    
    Quaternion rot = MathUtils::AngleAxis(angle, axis.normalized());
    
    // 需要访问父对象，先释放当前锁
    lock.unlock();
    auto parentNode = m_node ? m_node->parent.lock() : nullptr;
    if (parentNode && parentNode->transform) {
        Quaternion parentRot = parentNode->transform->GetWorldRotation();
        lock.lock();
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
        lock.lock();
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
    // 使用写锁保护数据访问
    std::unique_lock<std::shared_mutex> lock(m_dataMutex);
    
    // 获取世界位置（需要先释放锁，避免死锁）
    lock.unlock();
    Vector3 worldPos = GetWorldPosition();
    lock.lock();
    Vector3 direction = (target - worldPos).normalized();
    
    if (direction.squaredNorm() < MathUtils::EPSILON) {
        return; // 目标点与当前位置重合
    }
    
    Quaternion lookRotation = MathUtils::LookRotation(-direction, up);
    
    auto parentNode = m_node ? m_node->parent.lock() : nullptr;
    if (parentNode && parentNode->transform) {
        Quaternion parentRot = parentNode->transform->GetWorldRotation();
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
    std::unique_lock<std::shared_mutex> lock(m_dataMutex);
    
    // 检测 NaN/Inf
    if (!std::isfinite(scale.x()) || !std::isfinite(scale.y()) || !std::isfinite(scale.z())) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
            "Transform::SetScale: 缩放包含 NaN 或 Inf，操作被忽略"));
        return;
    }
    
    // 检测零缩放或极小缩放
    const float MIN_SCALE = 1e-6f;
    Vector3 safeScale = scale;
    
    if (std::abs(safeScale.x()) < MIN_SCALE) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
            "Transform::SetScale: X 缩放过小，限制为最小值"));
        safeScale.x() = (safeScale.x() >= 0.0f) ? MIN_SCALE : -MIN_SCALE;
    }
    if (std::abs(safeScale.y()) < MIN_SCALE) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
            "Transform::SetScale: Y 缩放过小，限制为最小值"));
        safeScale.y() = (safeScale.y() >= 0.0f) ? MIN_SCALE : -MIN_SCALE;
    }
    if (std::abs(safeScale.z()) < MIN_SCALE) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
            "Transform::SetScale: Z 缩放过小，限制为最小值"));
        safeScale.z() = (safeScale.z() >= 0.0f) ? MIN_SCALE : -MIN_SCALE;
    }
    
    m_scale = safeScale;
    MarkDirtyNoLock();
    lock.unlock();  // 释放数据锁
    
    // 递归使所有子节点的缓存失效
    InvalidateChildrenCache();
}

void Transform::SetScale(float scale) {
    SetScale(Vector3(scale, scale, scale));  // 复用带验证的版本，会自动调用InvalidateChildrenCache
}

Vector3 Transform::GetWorldScale() const {
    // 第一步：乐观无锁读取（快速路径）
    uint64_t cachedVersion = m_worldCache.version;
    uint64_t localVer = m_localVersion.load(std::memory_order_acquire);
    
    // 检查本地是否变化
    if (!m_dirtyWorldTransform.load(std::memory_order_acquire) && 
        cachedVersion == localVer) {
        auto parentNode = m_node ? m_node->parent.lock() : nullptr;
        if (!parentNode || 
            m_worldCache.parentVersion == parentNode->transform->m_localVersion.load(std::memory_order_acquire)) {
            return m_worldCache.scale;
        }
    }
    
    // 第二步：慢速路径 - 需要重新计算
    return GetWorldScaleSlow();
}

// ============================================================================
// 方向向量
// ============================================================================

Vector3 Transform::GetForward() const {
    std::shared_lock<std::shared_mutex> lock(m_dataMutex);
    return m_rotation * Vector3::UnitZ();
}

Vector3 Transform::GetRight() const {
    std::shared_lock<std::shared_mutex> lock(m_dataMutex);
    return m_rotation * Vector3::UnitX();
}

Vector3 Transform::GetUp() const {
    std::shared_lock<std::shared_mutex> lock(m_dataMutex);
    return m_rotation * Vector3::UnitY();
}

// ============================================================================
// 矩阵操作
// ============================================================================

Matrix4 Transform::GetLocalMatrix() const {
    // 使用读锁保护数据访问
    std::shared_lock<std::shared_mutex> lock(m_dataMutex);
    return MathUtils::TRS(m_position, m_rotation, m_scale);
}

Matrix4 Transform::GetWorldMatrix() const {
    // 使用读锁保护数据访问
    std::shared_lock<std::shared_mutex> lock(m_dataMutex);
    
    Matrix4 localMat = MathUtils::TRS(m_position, m_rotation, m_scale);
    
    // 需要访问父对象，先释放当前锁
    lock.unlock();
    auto parentNode = m_node ? m_node->parent.lock() : nullptr;
    if (parentNode && parentNode->transform) {
        Matrix4 parentWorldMat = parentNode->transform->GetWorldMatrix();
        return parentWorldMat * localMat;
    }
    return localMat;
}

void Transform::SetFromMatrix(const Matrix4& matrix) {
    std::unique_lock<std::shared_mutex> lock(m_dataMutex);
    MathUtils::DecomposeMatrix(matrix, m_position, m_rotation, m_scale);
    MarkDirtyNoLock();
}

// ============================================================================
// 父子关系
// ============================================================================

bool Transform::SetParent(Transform* parent) {
    // 层级操作使用层级锁
    std::lock_guard<std::mutex> hierarchyLock(m_hierarchyMutex);
    
    auto myNode = GetNode(this);
    if (!myNode || myNode->destroyed.load(std::memory_order_acquire)) {
        return false;
    }
    
    auto currentParentNode = myNode->parent.lock();
    auto newParentNode = GetNode(parent);
    
    if (currentParentNode == newParentNode) {
        return true;
    }
    
    // 自引用检查
    if (parent == this) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
            "Transform::SetParent: 不能将自己设置为父对象"));
        return false;
    }
    
    // 循环引用检查（使用智能指针）
    if (newParentNode) {
        auto ancestor = newParentNode;
        int depth = 0;
        const int MAX_DEPTH = 1000;
        
        while (ancestor && depth < MAX_DEPTH) {
            if (ancestor->destroyed.load(std::memory_order_acquire)) {
                HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
                    "Transform::SetParent: 父对象已被销毁"));
                return false;
            }
            
            if (ancestor == myNode) {
                HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
                    "Transform::SetParent: 检测到循环引用"));
                return false;
            }
            
            ancestor = ancestor->parent.lock();
            depth++;
        }
        
        if (depth >= MAX_DEPTH) {
            HANDLE_ERROR(RENDER_WARNING(ErrorCode::OutOfRange,
                "Transform::SetParent: 父对象层级过深"));
            return false;
        }
    }
    
    // 从旧父节点移除
    if (currentParentNode && currentParentNode->transform) {
        currentParentNode->transform->RemoveChild(this);
    }
    
    // 添加到新父节点
    if (newParentNode && newParentNode->transform) {
        newParentNode->transform->AddChild(this);
    }
    
    // 更新父指针
    myNode->parent = newParentNode;
    
    // 标记为脏（需要数据锁）
    {
        std::unique_lock<std::shared_mutex> dataLock(m_dataMutex);
        MarkDirtyNoLock();
    }
    
    return true;
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
    // 使用原子操作标记为脏并递增版本号（无需加锁）
    m_dirtyLocal.store(true, std::memory_order_release);
    m_dirtyWorld.store(true, std::memory_order_release);
    m_dirtyWorldTransform.store(true, std::memory_order_release);
    m_localVersion.fetch_add(1, std::memory_order_release);
}

void Transform::MarkDirtyNoLock() {
    // 内部版本：假设调用者已经持有锁
    m_dirtyLocal.store(true, std::memory_order_release);
    m_dirtyWorld.store(true, std::memory_order_release);
    m_dirtyWorldTransform.store(true, std::memory_order_release);
    m_localVersion.fetch_add(1, std::memory_order_release);
    
    // 注意：不在这里通知子对象，避免死锁
    // 子对象会在访问时自动检测父对象变化（通过检查父对象的版本号）
}

void Transform::InvalidateWorldTransformCache() {
    // 外部调用版本：标记世界变换缓存为无效，并递归标记所有子对象
    m_dirtyWorldTransform.store(true, std::memory_order_release);
    
    // 通知所有子对象更新缓存（使用层级锁）
    std::lock_guard<std::mutex> hierarchyLock(m_hierarchyMutex);
    if (m_node) {
        for (auto& childNode : m_node->children) {
            if (childNode && childNode->transform) {
                childNode->transform->InvalidateWorldTransformCacheNoLock();
            }
        }
    }
}

void Transform::InvalidateWorldTransformCacheNoLock() {
    // 内部版本：仅标记原子标志，不加锁，避免死锁
    m_dirtyWorldTransform.store(true, std::memory_order_release);
    
    // 注意：不递归调用子对象，避免复杂的锁依赖
    // 子对象在下次访问时会自动检测父对象变化并更新缓存
}

void Transform::InvalidateChildrenCache() {
    // 递归使所有子节点的缓存失效（因为父节点变化会影响所有子节点的世界变换）
    // 使用非递归方式，避免重复获取层级锁
    
    // P0: 检查当前节点是否已销毁
    if (m_node && m_node->destroyed.load(std::memory_order_acquire)) {
        return;
    }
    
    // 收集所有子节点（需要层级锁）
    std::vector<Transform*> allChildren;
    {
        std::lock_guard<std::mutex> hierarchyLock(m_hierarchyMutex);
        if (m_node) {
            // 使用队列进行广度优先遍历，收集所有子节点
            std::vector<Transform*> queue;
            for (auto& childNode : m_node->children) {
                // P0: 跳过已销毁的子节点
                if (childNode && childNode->transform && 
                    !childNode->destroyed.load(std::memory_order_acquire)) {
                    queue.push_back(childNode->transform);
                }
            }
            
            while (!queue.empty()) {
                Transform* current = queue.back();
                queue.pop_back();
                allChildren.push_back(current);
                
                // 收集当前节点的子节点（使用 try_lock 避免死锁）
                std::unique_lock<std::mutex> childLock(current->m_hierarchyMutex, std::try_to_lock);
                if (childLock.owns_lock() && current->m_node) {
                    for (auto& childNode : current->m_node->children) {
                        // P0: 跳过已销毁的子节点
                        if (childNode && childNode->transform && 
                            !childNode->destroyed.load(std::memory_order_acquire)) {
                            queue.push_back(childNode->transform);
                        }
                    }
                }
                // P0: 如果获取锁失败，该子节点正在被其他线程操作，跳过它
                // 它会在下次访问时检测到父节点变化并更新缓存
            }
        }
    }
    
    // 现在释放所有锁，原子地失效所有子节点的缓存
    for (Transform* child : allChildren) {
        child->m_dirtyWorldTransform.store(true, std::memory_order_release);
    }
}

void Transform::UpdateWorldTransformCache() const {
    // 注意：此函数已被GetWorldPositionSlow等替代，保留用于向后兼容
    // 现在直接调用GetWorldPositionSlow来更新缓存
    GetWorldPositionSlow();
    
    // 以下代码已不再使用，但保留以防需要
    /*
    auto parentNode = m_node ? m_node->parent.lock() : nullptr;
    
    if (parentNode && parentNode->transform) {
        // 有父对象：递归计算（会利用父对象的缓存）
        Vector3 parentPos = parentNode->transform->GetWorldPosition();
        Quaternion parentRot = parentNode->transform->GetWorldRotation();
        Vector3 parentScale = parentNode->transform->GetWorldScale();
        
        // 计算世界变换
        m_cachedWorldPosition = parentPos + parentRot * (parentScale.cwiseProduct(m_position));
        m_cachedWorldRotation = parentRot * m_rotation;
        m_cachedWorldScale = parentScale.cwiseProduct(m_scale);
    } else {
        // 无父对象：世界变换=本地变换
        m_cachedWorldPosition = m_position;
        m_cachedWorldRotation = m_rotation;
        m_cachedWorldScale = m_scale;
    }
    
    // 标记缓存为有效
    m_dirtyWorldTransform.store(false, std::memory_order_release);
    */
}

// ============================================================================
// 子对象管理（生命周期管理）
// ============================================================================

void Transform::AddChild(Transform* child) {
    if (!child) return;
    
    std::lock_guard<std::mutex> hierarchyLock(m_hierarchyMutex);
    
    if (!m_node) return;
    
    // P0: 检查当前节点是否已销毁
    if (m_node->destroyed.load(std::memory_order_acquire)) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
            "Transform::AddChild: 当前对象已被销毁"));
        return;
    }
    
    auto childNode = GetNode(child);
    if (!childNode) return;
    
    // P0: 检查子节点是否已销毁
    if (childNode->destroyed.load(std::memory_order_acquire)) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
            "Transform::AddChild: 子对象已被销毁"));
        return;
    }
    
    // 检查是否已存在（避免重复添加）
    auto it = std::find_if(m_node->children.begin(), m_node->children.end(),
        [childNode](const std::shared_ptr<TransformNode>& node) {
            return node == childNode;
        });
    if (it == m_node->children.end()) {
        m_node->children.push_back(childNode);
    }
}

void Transform::RemoveChild(Transform* child) {
    if (!child) return;
    
    std::lock_guard<std::mutex> hierarchyLock(m_hierarchyMutex);
    
    if (!m_node) return;
    
    // P0: 检查当前节点是否已销毁（如果已销毁，静默返回，因为析构函数会清理）
    if (m_node->destroyed.load(std::memory_order_acquire)) {
        return;
    }
    
    auto childNode = GetNode(child);
    if (!childNode) return;
    
    // 注意：不检查子节点是否已销毁，因为移除操作应该总是成功
    // 即使子节点正在销毁，我们也应该能够从父节点列表中移除它
    
    // 移除子对象
    auto it = std::find_if(m_node->children.begin(), m_node->children.end(),
        [childNode](const std::shared_ptr<TransformNode>& node) {
            return node == childNode;
        });
    if (it != m_node->children.end()) {
        m_node->children.erase(it);
    }
}

// NotifyChildrenParentDestroyed 方法已在析构函数中实现，不再需要此方法
// 但为了向后兼容，保留空实现
void Transform::NotifyChildrenParentDestroyed() {
    // 已在析构函数中实现，此方法不再需要
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
    std::shared_lock<std::shared_mutex> lock(m_dataMutex);
    
    std::ostringstream oss;
    oss << "Transform {\n";
    oss << "  Position: (" << m_position.x() << ", " << m_position.y() << ", " << m_position.z() << ")\n";
    oss << "  Rotation: (" << m_rotation.w() << ", " << m_rotation.x() << ", " 
        << m_rotation.y() << ", " << m_rotation.z() << ")\n";
    oss << "  Scale: (" << m_scale.x() << ", " << m_scale.y() << ", " << m_scale.z() << ")\n";
    
    auto parentNode = m_node ? m_node->parent.lock() : nullptr;
    if (parentNode && parentNode->transform) {
        oss << "  Parent: " << static_cast<const void*>(parentNode->transform) << "\n";
    } else {
        oss << "  Parent: nullptr\n";
    }
    
    oss << "  Children: " << (m_node ? m_node->children.size() : 0) << "\n";
    oss << "  Hierarchy Depth: " << GetHierarchyDepth() << "\n";
    oss << "}";
    
    return oss.str();
}

void Transform::PrintHierarchy(int indent, std::ostream& os) const {
    std::shared_lock<std::shared_mutex> lock(m_dataMutex);
    
    // 打印缩进
    for (int i = 0; i < indent; ++i) {
        os << "  ";
    }
    
    // 打印当前节点信息
    os << "Transform [";
    os << "pos=(" << m_position.x() << "," << m_position.y() << "," << m_position.z() << ")";
    os << ", scale=(" << m_scale.x() << "," << m_scale.y() << "," << m_scale.z() << ")";
    os << ", children=" << (m_node ? m_node->children.size() : 0);
    os << "]\n";
    
    // 递归打印子节点（需要解锁以避免死锁）
    // 注意：打印子节点时不能持有当前锁，因为子节点的 PrintHierarchy 也需要锁
    // 但为了简化，这里我们只打印子节点数量，不递归打印
    // 如果需要完整层次结构，应该在调用前解除锁
}

bool Transform::Validate() const {
    std::shared_lock<std::shared_mutex> lock(m_dataMutex);
    
    // 1. 检查四元数是否归一化
    float rotNorm = m_rotation.norm();
    if (std::abs(rotNorm - 1.0f) > MathUtils::EPSILON * 10.0f) {
        return false;  // 旋转四元数未归一化
    }
    
    // 2. 检查四元数是否包含 NaN 或 Inf
    if (!std::isfinite(m_rotation.w()) || !std::isfinite(m_rotation.x()) ||
        !std::isfinite(m_rotation.y()) || !std::isfinite(m_rotation.z())) {
        return false;
    }
    
    // 3. 检查位置是否包含 NaN 或 Inf
    if (!std::isfinite(m_position.x()) || !std::isfinite(m_position.y()) || !std::isfinite(m_position.z())) {
        return false;
    }
    
    // 4. 检查缩放是否包含 NaN 或 Inf
    if (!std::isfinite(m_scale.x()) || !std::isfinite(m_scale.y()) || !std::isfinite(m_scale.z())) {
        return false;
    }
    
    // 5. 检查缩放是否过小（可能导致数值问题）
    const float MIN_SCALE = 1e-7f;  // 比设置时更严格
    if (std::abs(m_scale.x()) < MIN_SCALE || std::abs(m_scale.y()) < MIN_SCALE || std::abs(m_scale.z()) < MIN_SCALE) {
        return false;  // 缩放过小
    }
    
    // 6. 检查缩放是否过大（可能导致溢出）
    const float MAX_SCALE = 1e6f;
    if (std::abs(m_scale.x()) > MAX_SCALE || std::abs(m_scale.y()) > MAX_SCALE || std::abs(m_scale.z()) > MAX_SCALE) {
        return false;  // 缩放过大
    }
    
    // 7. 检查父指针自引用
    auto parentNode = m_node ? m_node->parent.lock() : nullptr;
    if (parentNode && parentNode->transform == this) {
        return false;  // 自引用
    }
    
    // 8. 检查子对象列表中是否有空指针或自引用
    if (m_node) {
        for (auto& childNode : m_node->children) {
            if (!childNode || !childNode->transform || childNode->transform == this) {
                return false;
            }
        }
    }
    
    // 9. 检查层级深度（间接检测循环引用）
    int depth = GetHierarchyDepth();
    if (depth >= 1000) {
        return false;  // 层级过深或存在循环
    }
    
    return true;
}

int Transform::GetHierarchyDepth() const {
    auto parentNode = m_node ? m_node->parent.lock() : nullptr;
    if (!parentNode) {
        return 0;
    }
    
    // 递归计算深度
    int depth = 1;
    auto current = parentNode;
    const int MAX_DEPTH = 1000;  // 防止无限循环
    
    while (current && depth < MAX_DEPTH) {
        current = current->parent.lock();
        if (current) {
            depth++;
        }
    }
    
    return depth;
}

int Transform::GetChildCount() const {
    std::lock_guard<std::mutex> hierarchyLock(m_hierarchyMutex);
    return static_cast<int>(m_node ? m_node->children.size() : 0);
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
