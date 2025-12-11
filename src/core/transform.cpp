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
#include "render/transform.h"
#include "render/error.h"
#include <algorithm>  // for std::find, std::remove, std::sort, std::reverse
#include <cmath>       // for std::exp, std::isfinite, std::max, std::min
#include <sstream>     // for std::ostringstream

// P1-2.2: SIMD intrinsics
#if defined(__AVX2__)
    #include <immintrin.h>  // AVX2 + FMA
#elif defined(__SSE__)
    #include <xmmintrin.h>  // SSE
#endif

namespace Render {

// ============================================================================
// 内部访问辅助宏（简化代码，避免重复写 m_hotData. 和 m_coldData->）
// ============================================================================
// 注意：这些宏只在 transform.cpp 中使用，避免污染全局命名空间

#define m_position m_hotData.position
#define m_rotation m_hotData.rotation
#define m_scale m_hotData.scale
#define m_localVersion m_hotData.localVersion
#define m_dirtyLocal m_hotData.dirtyLocal
#define m_dirtyWorld m_hotData.dirtyWorld
#define m_dirtyWorldTransform m_hotData.dirtyWorldTransform

#define m_node m_coldData->node
#define m_localMatrix m_coldData->cachedLocalMatrix
#define m_worldMatrix m_coldData->cachedWorldMatrix
#define m_cachedWorldPosition m_coldData->cachedWorldPosition
#define m_cachedWorldRotation m_coldData->cachedWorldRotation
#define m_cachedWorldScale m_coldData->cachedWorldScale
#define m_worldCache m_coldData->worldCache
#define m_dataMutex m_coldData->dataMutex
#define m_hierarchyMutex m_coldData->hierarchyMutex
#define m_changeCallback m_coldData->changeCallback

// 静态成员初始化
std::atomic<uint64_t> Transform::s_nextGlobalId{1};

// ============================================================================
// 构造和初始化
// ============================================================================

Transform::Transform()
    : m_hotData()  // 使用默认构造函数初始化热数据
    , m_coldData(std::make_unique<ColdData>(this))  // 初始化冷数据
    , m_hotCache()  // 初始化热缓存
    , m_globalId(s_nextGlobalId.fetch_add(1, std::memory_order_relaxed))
{
    m_node->shared_this = m_node;  // 允许从内部获取 shared_ptr
}

Transform::Transform(const Vector3& position, const Quaternion& rotation, const Vector3& scale)
    : m_hotData()  // 先使用默认构造函数初始化
    , m_coldData(std::make_unique<ColdData>(this))  // 初始化冷数据
    , m_hotCache()  // 初始化热缓存
    , m_globalId(s_nextGlobalId.fetch_add(1, std::memory_order_relaxed))
{
    // 设置自定义的初始值
    m_position = position;
    m_rotation = rotation;
    m_scale = scale;
    
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
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::TransformInvalidPosition,
            "Transform::SetPosition: 位置包含 NaN 或 Inf，操作被忽略"));
        return;
    }
    
    // 检查值是否真正变化（避免重复通知）
    bool changed = !m_position.isApprox(position, MathUtils::EPSILON);
    
    if (changed) {
        m_position = position;
        MarkDirtyNoLock();
        
        // 通知变化（在持有锁的情况下复制回调，然后释放锁调用）
        ChangeCallback callback = m_changeCallback;
        lock.unlock();  // 释放数据锁
        
        // 调用回调（不持有锁，避免死锁）
        if (callback) {
            try {
                callback(this);
            } catch (...) {
                // 忽略回调异常
            }
        }
        
        // 递归使所有子节点的缓存失效（因为父节点变化会影响所有子节点的世界变换）
        InvalidateChildrenCache();
    } else {
        lock.unlock();  // 值未变化，直接释放锁
    }
}

Transform::Result Transform::TrySetPosition(const Vector3& position) {
    RENDER_TRY {
        std::unique_lock<std::shared_mutex> lock(m_dataMutex);
        
        // 检测 NaN/Inf
        if (!std::isfinite(position.x()) || !std::isfinite(position.y()) || !std::isfinite(position.z())) {
            return Result::Failure(ErrorCode::TransformInvalidPosition,
                "位置包含 NaN 或 Inf");
        }
        
        m_position = position;
        MarkDirtyNoLock();
        lock.unlock();
        
        InvalidateChildrenCache();
        return Result::Success();
    }
    RENDER_CATCH_ALL {
        return Result::Failure(ErrorCode::OperationFailed, "设置位置时发生未知错误");
    }
    
    return Result::Failure(ErrorCode::OperationFailed, "设置位置失败");
}

Vector3 Transform::GetWorldPosition() const {
    // P1-2.1: 三层缓存读取策略
    
    // L1 热缓存：完全无锁读取（最快，~5ns）
    uint64_t hotVersion = m_hotCache.version.load(std::memory_order_acquire);
    uint64_t localVer = m_localVersion.load(std::memory_order_acquire);
    
    if (hotVersion == localVer && hotVersion != 0) {
        // 验证父节点版本
        auto parentNode = m_node ? m_node->parent.lock() : nullptr;
        if (!parentNode) {
            // 无父节点，热缓存命中
            return m_hotCache.worldPosition;
        }
        
        // 检查父节点版本
        uint64_t parentVer = parentNode->transform->m_localVersion.load(std::memory_order_acquire);
        if (m_worldCache.parentVersion == parentVer) {
            // 完全无锁返回（热路径）
            return m_hotCache.worldPosition;
        }
    }
    
    // L2 温缓存：需要读锁，但不遍历层级（~150ns）
    {
        std::shared_lock<std::shared_mutex> lock(m_dataMutex);
        uint64_t cachedVersion = m_worldCache.version;
        
        if (!m_dirtyWorldTransform.load(std::memory_order_acquire) && 
            cachedVersion == localVer) {
            auto parentNode = m_node ? m_node->parent.lock() : nullptr;
            if (!parentNode || 
                m_worldCache.parentVersion == parentNode->transform->m_localVersion.load(std::memory_order_acquire)) {
                // 温缓存命中，更新热缓存
                UpdateHotCache();
                return m_worldCache.position;
            }
        }
    }
    
    // L3 冷路径：需要完整计算（~2.5μs for depth 10）
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
        
        // P1-2.1: 更新热缓存
        UpdateHotCache();
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
    
    // 检查平移是否有效（非零）
    if (translation.squaredNorm() < MathUtils::EPSILON * MathUtils::EPSILON) {
        return;  // 零平移，不需要通知
    }
    
    m_position += translation;
    MarkDirtyNoLock();
    
    // 通知变化（在持有锁的情况下复制回调，然后释放锁调用）
    ChangeCallback callback = m_changeCallback;
    lock.unlock();  // 释放数据锁
    
    // 调用回调（不持有锁，避免死锁）
    if (callback) {
        try {
            callback(this);
        } catch (...) {
            // 忽略回调异常
        }
    }
    
    // 递归使所有子节点的缓存失效
    InvalidateChildrenCache();
}

void Transform::TranslateWorld(const Vector3& translation) {
    // 使用写锁保护数据访问
    std::unique_lock<std::shared_mutex> lock(m_dataMutex);
    
    // 检查平移是否有效（非零）
    if (translation.squaredNorm() < MathUtils::EPSILON * MathUtils::EPSILON) {
        return;  // 零平移，不需要通知
    }
    
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
    
    // 通知变化（在持有锁的情况下复制回调，然后释放锁调用）
    ChangeCallback callback = m_changeCallback;
    lock.unlock();  // 释放数据锁
    
    // 调用回调（不持有锁，避免死锁）
    if (callback) {
        try {
            callback(this);
        } catch (...) {
            // 忽略回调异常
        }
    }
    
    // 递归使所有子节点的缓存失效
    InvalidateChildrenCache();
}

// ============================================================================
// 旋转操作
// ============================================================================

void Transform::SetRotation(const Quaternion& rotation) {
    std::unique_lock<std::shared_mutex> lock(m_dataMutex);
    
    // 检查四元数的模长，避免零四元数导致除零错误
    float norm = rotation.norm();
    Quaternion normalizedRotation;
    if (norm < MathUtils::EPSILON) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::TransformInvalidRotation,
            "Transform::SetRotation: 四元数接近零向量，使用单位四元数替代"));
        normalizedRotation = Quaternion::Identity();
    } else {
        // 手动归一化：访问系数并除以模长
        float invNorm = 1.0f / norm;
        normalizedRotation = Quaternion(
            rotation.w() * invNorm,
            rotation.x() * invNorm,
            rotation.y() * invNorm,
            rotation.z() * invNorm
        );
    }
    
    // 检查值是否真正变化（避免重复通知）
    // 四元数比较：检查是否表示相同的旋转（考虑四元数的双重覆盖）
    bool changed = !m_rotation.coeffs().isApprox(normalizedRotation.coeffs(), MathUtils::EPSILON) &&
                   !m_rotation.coeffs().isApprox(-normalizedRotation.coeffs(), MathUtils::EPSILON);
    
    if (changed) {
        m_rotation = normalizedRotation;
        MarkDirtyNoLock();
        
        // 通知变化（在持有锁的情况下复制回调，然后释放锁调用）
        ChangeCallback callback = m_changeCallback;
        lock.unlock();  // 释放数据锁
        
        // 调用回调（不持有锁，避免死锁）
        if (callback) {
            try {
                callback(this);
            } catch (...) {
                // 忽略回调异常
            }
        }
        
        // 递归使所有子节点的缓存失效
        InvalidateChildrenCache();
    } else {
        lock.unlock();  // 值未变化，直接释放锁
    }
}

Transform::Result Transform::TrySetRotation(const Quaternion& rotation) {
    RENDER_TRY {
        std::unique_lock<std::shared_mutex> lock(m_dataMutex);
        
        // 检查四元数的模长
        float norm = rotation.norm();
        if (norm < MathUtils::EPSILON) {
            return Result::Failure(ErrorCode::TransformInvalidRotation,
                "四元数接近零向量，模长 = " + std::to_string(norm));
        }
        
        // 检查 NaN/Inf
        if (!std::isfinite(rotation.w()) || !std::isfinite(rotation.x()) ||
            !std::isfinite(rotation.y()) || !std::isfinite(rotation.z())) {
            return Result::Failure(ErrorCode::TransformInvalidRotation,
                "四元数包含 NaN 或 Inf");
        }
        
        // 手动归一化
        float invNorm = 1.0f / norm;
        m_rotation = Quaternion(
            rotation.w() * invNorm,
            rotation.x() * invNorm,
            rotation.y() * invNorm,
            rotation.z() * invNorm
        );
        
        MarkDirtyNoLock();
        lock.unlock();
        
        InvalidateChildrenCache();
        return Result::Success();
    }
    RENDER_CATCH_ALL {
        return Result::Failure(ErrorCode::OperationFailed, "设置旋转时发生未知错误");
    }
    
    return Result::Failure(ErrorCode::OperationFailed, "设置旋转失败");
}

void Transform::SetRotationEuler(const Vector3& euler) {
    std::unique_lock<std::shared_mutex> lock(m_dataMutex);
    
    Quaternion newRotation = MathUtils::FromEuler(euler.x(), euler.y(), euler.z());
    
    // 检查值是否真正变化（避免重复通知）
    bool changed = !m_rotation.coeffs().isApprox(newRotation.coeffs(), MathUtils::EPSILON) &&
                   !m_rotation.coeffs().isApprox(-newRotation.coeffs(), MathUtils::EPSILON);
    
    if (changed) {
        m_rotation = newRotation;
        MarkDirtyNoLock();
        
        // 通知变化（在持有锁的情况下复制回调，然后释放锁调用）
        ChangeCallback callback = m_changeCallback;
        lock.unlock();  // 释放数据锁
        
        // 调用回调（不持有锁，避免死锁）
        if (callback) {
            try {
                callback(this);
            } catch (...) {
                // 忽略回调异常
            }
        }
        
        // 递归使所有子节点的缓存失效
        InvalidateChildrenCache();
    } else {
        lock.unlock();  // 值未变化，直接释放锁
    }
}

void Transform::SetRotationEulerDegrees(const Vector3& euler) {
    std::unique_lock<std::shared_mutex> lock(m_dataMutex);
    
    Quaternion newRotation = MathUtils::FromEulerDegrees(euler.x(), euler.y(), euler.z());
    
    // 检查值是否真正变化（避免重复通知）
    bool changed = !m_rotation.coeffs().isApprox(newRotation.coeffs(), MathUtils::EPSILON) &&
                   !m_rotation.coeffs().isApprox(-newRotation.coeffs(), MathUtils::EPSILON);
    
    if (changed) {
        m_rotation = newRotation;
        MarkDirtyNoLock();
        
        // 通知变化（在持有锁的情况下复制回调，然后释放锁调用）
        ChangeCallback callback = m_changeCallback;
        lock.unlock();  // 释放数据锁
        
        // 调用回调（不持有锁，避免死锁）
        if (callback) {
            try {
                callback(this);
            } catch (...) {
                // 忽略回调异常
            }
        }
        
        // 递归使所有子节点的缓存失效
        InvalidateChildrenCache();
    } else {
        lock.unlock();  // 值未变化，直接释放锁
    }
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
    // P1-2.1: 三层缓存读取策略
    
    // L1 热缓存：完全无锁读取
    uint64_t hotVersion = m_hotCache.version.load(std::memory_order_acquire);
    uint64_t localVer = m_localVersion.load(std::memory_order_acquire);
    
    if (hotVersion == localVer && hotVersion != 0) {
        auto parentNode = m_node ? m_node->parent.lock() : nullptr;
        if (!parentNode) {
            return m_hotCache.worldRotation;
        }
        
        uint64_t parentVer = parentNode->transform->m_localVersion.load(std::memory_order_acquire);
        if (m_worldCache.parentVersion == parentVer) {
            return m_hotCache.worldRotation;
        }
    }
    
    // L2 温缓存：需要读锁
    {
        std::shared_lock<std::shared_mutex> lock(m_dataMutex);
        uint64_t cachedVersion = m_worldCache.version;
        
        if (!m_dirtyWorldTransform.load(std::memory_order_acquire) && 
            cachedVersion == localVer) {
            auto parentNode = m_node ? m_node->parent.lock() : nullptr;
            if (!parentNode || 
                m_worldCache.parentVersion == parentNode->transform->m_localVersion.load(std::memory_order_acquire)) {
                UpdateHotCache();
                return m_worldCache.rotation;
            }
        }
    }
    
    // L3 冷路径：需要完整计算
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
    Quaternion normalizedRotation(
        newRotation.w() * invNorm,
        newRotation.x() * invNorm,
        newRotation.y() * invNorm,
        newRotation.z() * invNorm
    );
    
    // 检查值是否真正变化（避免重复通知）
    bool changed = !m_rotation.coeffs().isApprox(normalizedRotation.coeffs(), MathUtils::EPSILON) &&
                   !m_rotation.coeffs().isApprox(-normalizedRotation.coeffs(), MathUtils::EPSILON);
    
    if (changed) {
        m_rotation = normalizedRotation;
        MarkDirtyNoLock();
        
        // 通知变化（在持有锁的情况下复制回调，然后释放锁调用）
        ChangeCallback callback = m_changeCallback;
        lock.unlock();  // 释放数据锁
        
        // 调用回调（不持有锁，避免死锁）
        if (callback) {
            try {
                callback(this);
            } catch (...) {
                // 忽略回调异常
            }
        }
        
        // 递归使所有子节点的缓存失效
        InvalidateChildrenCache();
    } else {
        lock.unlock();  // 值未变化，直接释放锁
    }
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
    Quaternion normalizedRotation(
        newRotation.w() * invNorm,
        newRotation.x() * invNorm,
        newRotation.y() * invNorm,
        newRotation.z() * invNorm
    );
    
    // 检查值是否真正变化（避免重复通知）
    bool changed = !m_rotation.coeffs().isApprox(normalizedRotation.coeffs(), MathUtils::EPSILON) &&
                   !m_rotation.coeffs().isApprox(-normalizedRotation.coeffs(), MathUtils::EPSILON);
    
    if (changed) {
        m_rotation = normalizedRotation;
        MarkDirtyNoLock();
        
        // 通知变化（在持有锁的情况下复制回调，然后释放锁调用）
        ChangeCallback callback = m_changeCallback;
        lock.unlock();  // 释放数据锁
        
        // 调用回调（不持有锁，避免死锁）
        if (callback) {
            try {
                callback(this);
            } catch (...) {
                // 忽略回调异常
            }
        }
        
        // 递归使所有子节点的缓存失效
        InvalidateChildrenCache();
    } else {
        lock.unlock();  // 值未变化，直接释放锁
    }
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
        Quaternion normalizedRotation(
            localRot.w() * invLocalNorm,
            localRot.x() * invLocalNorm,
            localRot.y() * invLocalNorm,
            localRot.z() * invLocalNorm
        );
        
        // 检查值是否真正变化（避免重复通知）
        bool changed = !m_rotation.coeffs().isApprox(normalizedRotation.coeffs(), MathUtils::EPSILON) &&
                       !m_rotation.coeffs().isApprox(-normalizedRotation.coeffs(), MathUtils::EPSILON);
        
        if (changed) {
            m_rotation = normalizedRotation;
            MarkDirtyNoLock();
            
            // 通知变化（在持有锁的情况下复制回调，然后释放锁调用）
            ChangeCallback callback = m_changeCallback;
            lock.unlock();  // 释放数据锁
            
            // 调用回调（不持有锁，避免死锁）
            if (callback) {
                try {
                    callback(this);
                } catch (...) {
                    // 忽略回调异常
                }
            }
            
            // 递归使所有子节点的缓存失效
            InvalidateChildrenCache();
        } else {
            lock.unlock();  // 值未变化，直接释放锁
        }
    } else {
        lock.lock();
        Quaternion newRotation = rot * m_rotation;
        float norm = newRotation.norm();
        if (norm < MathUtils::EPSILON) {
            HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
                "Transform::RotateAroundWorld: 旋转结果异常，保持原旋转"));
            lock.unlock();
            return;
        }
        // 手动归一化
        float invNorm = 1.0f / norm;
        Quaternion normalizedRotation(
            newRotation.w() * invNorm,
            newRotation.x() * invNorm,
            newRotation.y() * invNorm,
            newRotation.z() * invNorm
        );
        
        // 检查值是否真正变化（避免重复通知）
        bool changed = !m_rotation.coeffs().isApprox(normalizedRotation.coeffs(), MathUtils::EPSILON) &&
                       !m_rotation.coeffs().isApprox(-normalizedRotation.coeffs(), MathUtils::EPSILON);
        
        if (changed) {
            m_rotation = normalizedRotation;
            MarkDirtyNoLock();
            
            // 通知变化（在持有锁的情况下复制回调，然后释放锁调用）
            ChangeCallback callback = m_changeCallback;
            lock.unlock();  // 释放数据锁
            
            // 调用回调（不持有锁，避免死锁）
            if (callback) {
                try {
                    callback(this);
                } catch (...) {
                    // 忽略回调异常
                }
            }
            
            // 递归使所有子节点的缓存失效
            InvalidateChildrenCache();
        } else {
            lock.unlock();  // 值未变化，直接释放锁
        }
    }
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
    
    Quaternion newRotation;
    auto parentNode = m_node ? m_node->parent.lock() : nullptr;
    if (parentNode && parentNode->transform) {
        Quaternion parentRot = parentNode->transform->GetWorldRotation();
        newRotation = parentRot.inverse() * lookRotation;
    } else {
        newRotation = lookRotation;
    }
    
    // 检查值是否真正变化（避免重复通知）
    bool changed = !m_rotation.coeffs().isApprox(newRotation.coeffs(), MathUtils::EPSILON) &&
                   !m_rotation.coeffs().isApprox(-newRotation.coeffs(), MathUtils::EPSILON);
    
    if (changed) {
        m_rotation = newRotation;
        MarkDirtyNoLock();
        
        // 通知变化（在持有锁的情况下复制回调，然后释放锁调用）
        ChangeCallback callback = m_changeCallback;
        lock.unlock();  // 释放数据锁
        
        // 调用回调（不持有锁，避免死锁）
        if (callback) {
            try {
                callback(this);
            } catch (...) {
                // 忽略回调异常
            }
        }
        
        // 递归使所有子节点的缓存失效
        InvalidateChildrenCache();
    } else {
        lock.unlock();  // 值未变化，直接释放锁
    }
}

// ============================================================================
// 缩放操作
// ============================================================================

void Transform::SetScale(const Vector3& scale) {
    std::unique_lock<std::shared_mutex> lock(m_dataMutex);
    
    // 检测 NaN/Inf
    if (!std::isfinite(scale.x()) || !std::isfinite(scale.y()) || !std::isfinite(scale.z())) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::TransformInvalidScale,
            "Transform::SetScale: 缩放包含 NaN 或 Inf，操作被忽略"));
        return;
    }
    
    // 检测零缩放或极小缩放
    const float MIN_SCALE = 1e-6f;
    Vector3 safeScale = scale;
    
    if (std::abs(safeScale.x()) < MIN_SCALE) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::TransformInvalidScale,
            "Transform::SetScale: X 缩放过小，限制为最小值"));
        safeScale.x() = (safeScale.x() >= 0.0f) ? MIN_SCALE : -MIN_SCALE;
    }
    if (std::abs(safeScale.y()) < MIN_SCALE) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::TransformInvalidScale,
            "Transform::SetScale: Y 缩放过小，限制为最小值"));
        safeScale.y() = (safeScale.y() >= 0.0f) ? MIN_SCALE : -MIN_SCALE;
    }
    if (std::abs(safeScale.z()) < MIN_SCALE) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::TransformInvalidScale,
            "Transform::SetScale: Z 缩放过小，限制为最小值"));
        safeScale.z() = (safeScale.z() >= 0.0f) ? MIN_SCALE : -MIN_SCALE;
    }
    
    // 检查值是否真正变化（避免重复通知）
    bool changed = !m_scale.isApprox(safeScale, MathUtils::EPSILON);
    
    if (changed) {
        m_scale = safeScale;
        MarkDirtyNoLock();
        
        // 通知变化（在持有锁的情况下复制回调，然后释放锁调用）
        ChangeCallback callback = m_changeCallback;
        lock.unlock();  // 释放数据锁
        
        // 调用回调（不持有锁，避免死锁）
        if (callback) {
            try {
                callback(this);
            } catch (...) {
                // 忽略回调异常
            }
        }
        
        // 递归使所有子节点的缓存失效
        InvalidateChildrenCache();
    } else {
        lock.unlock();  // 值未变化，直接释放锁
    }
}

Transform::Result Transform::TrySetScale(const Vector3& scale) {
    RENDER_TRY {
        std::unique_lock<std::shared_mutex> lock(m_dataMutex);
        
        // 检测 NaN/Inf
        if (!std::isfinite(scale.x()) || !std::isfinite(scale.y()) || !std::isfinite(scale.z())) {
            return Result::Failure(ErrorCode::TransformInvalidScale,
                "缩放包含 NaN 或 Inf");
        }
        
        // 检测零缩放或极小缩放
        const float MIN_SCALE = 1e-6f;
        if (std::abs(scale.x()) < MIN_SCALE || 
            std::abs(scale.y()) < MIN_SCALE || 
            std::abs(scale.z()) < MIN_SCALE) {
            return Result::Failure(ErrorCode::TransformInvalidScale,
                "缩放值过小 (< " + std::to_string(MIN_SCALE) + ")");
        }
        
        // 检测过大缩放
        const float MAX_SCALE = 1e6f;
        if (std::abs(scale.x()) > MAX_SCALE || 
            std::abs(scale.y()) > MAX_SCALE || 
            std::abs(scale.z()) > MAX_SCALE) {
            return Result::Failure(ErrorCode::TransformInvalidScale,
                "缩放值过大 (> " + std::to_string(MAX_SCALE) + ")");
        }
        
        m_scale = scale;
        MarkDirtyNoLock();
        lock.unlock();
        
        InvalidateChildrenCache();
        return Result::Success();
    }
    RENDER_CATCH_ALL {
        return Result::Failure(ErrorCode::OperationFailed, "设置缩放时发生未知错误");
    }
    
    return Result::Failure(ErrorCode::OperationFailed, "设置缩放失败");
}

void Transform::SetScale(float scale) {
    SetScale(Vector3(scale, scale, scale));  // 复用带验证的版本，会自动调用InvalidateChildrenCache
}

Vector3 Transform::GetWorldScale() const {
    // P1-2.1: 三层缓存读取策略
    
    // L1 热缓存：完全无锁读取
    uint64_t hotVersion = m_hotCache.version.load(std::memory_order_acquire);
    uint64_t localVer = m_localVersion.load(std::memory_order_acquire);
    
    if (hotVersion == localVer && hotVersion != 0) {
        auto parentNode = m_node ? m_node->parent.lock() : nullptr;
        if (!parentNode) {
            return m_hotCache.worldScale;
        }
        
        uint64_t parentVer = parentNode->transform->m_localVersion.load(std::memory_order_acquire);
        if (m_worldCache.parentVersion == parentVer) {
            return m_hotCache.worldScale;
        }
    }
    
    // L2 温缓存：需要读锁
    {
        std::shared_lock<std::shared_mutex> lock(m_dataMutex);
        uint64_t cachedVersion = m_worldCache.version;
        
        if (!m_dirtyWorldTransform.load(std::memory_order_acquire) && 
            cachedVersion == localVer) {
            auto parentNode = m_node ? m_node->parent.lock() : nullptr;
            if (!parentNode || 
                m_worldCache.parentVersion == parentNode->transform->m_localVersion.load(std::memory_order_acquire)) {
                UpdateHotCache();
                return m_worldCache.scale;
            }
        }
    }
    
    // L3 冷路径：需要完整计算
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
    
    // 保存旧值用于比较
    Vector3 oldPosition = m_position;
    Quaternion oldRotation = m_rotation;
    Vector3 oldScale = m_scale;
    
    MathUtils::DecomposeMatrix(matrix, m_position, m_rotation, m_scale);
    
    // 检查值是否真正变化（避免重复通知）
    bool changed = !m_position.isApprox(oldPosition, MathUtils::EPSILON) ||
                   !m_rotation.coeffs().isApprox(oldRotation.coeffs(), MathUtils::EPSILON) &&
                   !m_rotation.coeffs().isApprox(-oldRotation.coeffs(), MathUtils::EPSILON) ||
                   !m_scale.isApprox(oldScale, MathUtils::EPSILON);
    
    if (changed) {
        MarkDirtyNoLock();
        
        // 通知变化（在持有锁的情况下复制回调，然后释放锁调用）
        ChangeCallback callback = m_changeCallback;
        lock.unlock();  // 释放数据锁
        
        // 调用回调（不持有锁，避免死锁）
        if (callback) {
            try {
                callback(this);
            } catch (...) {
                // 忽略回调异常
            }
        }
        
        // 递归使所有子节点的缓存失效
        InvalidateChildrenCache();
    } else {
        lock.unlock();  // 值未变化，直接释放锁
    }
}

Transform::Result Transform::TrySetFromMatrix(const Matrix4& matrix) {
    RENDER_TRY {
        std::unique_lock<std::shared_mutex> lock(m_dataMutex);
        
        // 检查矩阵是否包含 NaN/Inf
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                if (!std::isfinite(matrix(i, j))) {
                    return Result::Failure(ErrorCode::TransformInvalidMatrix,
                        "矩阵包含 NaN 或 Inf（位置: [" + 
                        std::to_string(i) + "," + std::to_string(j) + "]）");
                }
            }
        }
        
        // 分解矩阵
        Vector3 position, scale;
        Quaternion rotation;
        MathUtils::DecomposeMatrix(matrix, position, rotation, scale);
        
        // 验证分解结果
        if (!std::isfinite(position.x()) || !std::isfinite(position.y()) || !std::isfinite(position.z())) {
            return Result::Failure(ErrorCode::TransformInvalidPosition,
                "矩阵分解后的位置包含 NaN 或 Inf");
        }
        
        if (!std::isfinite(rotation.w()) || !std::isfinite(rotation.x()) ||
            !std::isfinite(rotation.y()) || !std::isfinite(rotation.z())) {
            return Result::Failure(ErrorCode::TransformInvalidRotation,
                "矩阵分解后的旋转包含 NaN 或 Inf");
        }
        
        if (!std::isfinite(scale.x()) || !std::isfinite(scale.y()) || !std::isfinite(scale.z())) {
            return Result::Failure(ErrorCode::TransformInvalidScale,
                "矩阵分解后的缩放包含 NaN 或 Inf");
        }
        
        // 应用分解结果
        m_position = position;
        m_rotation = rotation;
        m_scale = scale;
        MarkDirtyNoLock();
        
        return Result::Success();
    }
    RENDER_CATCH_ALL {
        return Result::Failure(ErrorCode::OperationFailed, "从矩阵设置变换时发生未知错误");
    }
    
    return Result::Failure(ErrorCode::OperationFailed, "从矩阵设置变换失败");
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
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::TransformSelfReference,
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
                HANDLE_ERROR(RENDER_WARNING(ErrorCode::TransformParentDestroyed,
                    "Transform::SetParent: 父对象已被销毁"));
                return false;
            }
            
            if (ancestor == myNode) {
                HANDLE_ERROR(RENDER_WARNING(ErrorCode::TransformCircularReference,
                    "Transform::SetParent: 检测到循环引用"));
                return false;
            }
            
            ancestor = ancestor->parent.lock();
            depth++;
        }
        
        if (depth >= MAX_DEPTH) {
            HANDLE_ERROR(RENDER_WARNING(ErrorCode::TransformHierarchyTooDeep,
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

Transform::Result Transform::TrySetParent(Transform* parent) {
    RENDER_TRY {
        // 层级操作使用层级锁
        std::lock_guard<std::mutex> hierarchyLock(m_hierarchyMutex);
        
        auto myNode = GetNode(this);
        if (!myNode || myNode->destroyed.load(std::memory_order_acquire)) {
            return Result::Failure(ErrorCode::TransformObjectDestroyed,
                "当前对象已被销毁");
        }
        
        auto currentParentNode = myNode->parent.lock();
        auto newParentNode = GetNode(parent);
        
        // 相同父对象，直接返回成功
        if (currentParentNode == newParentNode) {
            return Result::Success();
        }
        
        // 自引用检查
        if (parent == this) {
            return Result::Failure(ErrorCode::TransformSelfReference,
                "不能将自己设置为父对象");
        }
        
        // 循环引用检查
        if (newParentNode) {
            auto ancestor = newParentNode;
            int depth = 0;
            const int MAX_DEPTH = 1000;
            
            while (ancestor && depth < MAX_DEPTH) {
                if (ancestor->destroyed.load(std::memory_order_acquire)) {
                    return Result::Failure(ErrorCode::TransformParentDestroyed,
                        "父对象已被销毁");
                }
                
                if (ancestor == myNode) {
                    return Result::Failure(ErrorCode::TransformCircularReference,
                        "检测到循环引用（层级深度: " + std::to_string(depth) + "）");
                }
                
                ancestor = ancestor->parent.lock();
                depth++;
            }
            
            if (depth >= MAX_DEPTH) {
                return Result::Failure(ErrorCode::TransformHierarchyTooDeep,
                    "父对象层级过深（>= " + std::to_string(MAX_DEPTH) + "）");
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
        
        // 标记为脏
        {
            std::unique_lock<std::shared_mutex> dataLock(m_dataMutex);
            MarkDirtyNoLock();
        }
        
        return Result::Success();
    }
    RENDER_CATCH_ALL {
        return Result::Failure(ErrorCode::OperationFailed, "设置父对象时发生未知错误");
    }
    
    return Result::Failure(ErrorCode::OperationFailed, "设置父对象失败");
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
    
    // P1-2.1: 失效热缓存（通过将版本号设为 0）
    m_hotCache.version.store(0, std::memory_order_release);
}

void Transform::MarkDirtyNoLock() {
    // 内部版本：假设调用者已经持有锁
    m_dirtyLocal.store(true, std::memory_order_release);
    m_dirtyWorld.store(true, std::memory_order_release);
    m_dirtyWorldTransform.store(true, std::memory_order_release);
    m_localVersion.fetch_add(1, std::memory_order_release);
    
    // P1-2.1: 失效热缓存（通过将版本号设为 0）
    m_hotCache.version.store(0, std::memory_order_release);
    
    // 注意：不在这里通知子对象，避免死锁
    // 子对象会在访问时自动检测父对象变化（通过检查父对象的版本号）
}

void Transform::InvalidateWorldTransformCache() {
    // 外部调用版本：标记世界变换缓存为无效，并递归标记所有子对象
    m_dirtyWorldTransform.store(true, std::memory_order_release);
    // P1-2.1: 失效热缓存
    m_hotCache.version.store(0, std::memory_order_release);
    
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
    // P1-2.1: 失效热缓存
    m_hotCache.version.store(0, std::memory_order_release);
    
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
        // P1-2.1: 同时失效子节点的热缓存
        child->m_hotCache.version.store(0, std::memory_order_release);
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
// P1-2.1: 热缓存更新
// ============================================================================

void Transform::UpdateHotCache() const {
    // 假设已持有 m_dataMutex 写锁或读锁
    // 从温缓存 (m_worldCache) 更新到热缓存 (m_hotCache)
    
    m_hotCache.worldPosition = m_worldCache.position;
    m_hotCache.worldRotation = m_worldCache.rotation;
    m_hotCache.worldScale = m_worldCache.scale;
    
    // 原子地发布新版本（使用 release 语义确保上面的写入对其他线程可见）
    m_hotCache.version.store(m_worldCache.version, std::memory_order_release);
}

// ============================================================================
// P1-2.2: 批量操作优化（RAII + SIMD）
// ============================================================================

Transform::TransformBatchHandle::TransformBatchHandle(const Transform* t) 
    : m_transform(t)
    , m_lock(t->m_dataMutex)
    , m_cachedMatrix(t->GetWorldMatrix()) 
{
    // 构造时获取锁并缓存矩阵
    // 析构时自动释放锁（RAII）
}

void Transform::TransformBatchHandle::TransformPoints(const Vector3* input, Vector3* output, size_t count) const {
    Transform::TransformPointsSIMD(m_cachedMatrix, input, output, count);
}

void Transform::TransformBatchHandle::TransformDirections(const Vector3* input, Vector3* output, size_t count) const {
    // 方向变换只需要旋转部分（矩阵的左上3x3）
    // 这里我们使用完整矩阵，但忽略平移（w=0）
    Quaternion rot;
    rot = Quaternion(m_cachedMatrix.block<3,3>(0,0));  // 从矩阵提取旋转
    Transform::TransformDirectionsSIMD(rot, input, output, count);
}

// ============================================================================
// P1-2.2: SIMD 优化的批量变换实现
// ============================================================================

void Transform::TransformPointsSIMD(const Matrix4& mat, 
                                    const Vector3* input, 
                                    Vector3* output, 
                                    size_t count) {
#if defined(__AVX2__) && defined(__FMA__)
    // AVX2 + FMA 实现：一次处理 4 个点
    const size_t simdCount = count & ~3ULL;  // 对齐到 4 的倍数
    
    if (simdCount >= 4) {
        // 加载矩阵行到 SIMD 寄存器
        // mat 是列主序（Eigen 默认），转换为行主序访问
        __m256 m00_03 = _mm256_broadcast_ss(&mat(0, 0));
        __m256 m01_03 = _mm256_broadcast_ss(&mat(0, 1));
        __m256 m02_03 = _mm256_broadcast_ss(&mat(0, 2));
        __m256 m03_03 = _mm256_broadcast_ss(&mat(0, 3));
        
        __m256 m10_13 = _mm256_broadcast_ss(&mat(1, 0));
        __m256 m11_13 = _mm256_broadcast_ss(&mat(1, 1));
        __m256 m12_13 = _mm256_broadcast_ss(&mat(1, 2));
        __m256 m13_13 = _mm256_broadcast_ss(&mat(1, 3));
        
        __m256 m20_23 = _mm256_broadcast_ss(&mat(2, 0));
        __m256 m21_23 = _mm256_broadcast_ss(&mat(2, 1));
        __m256 m22_23 = _mm256_broadcast_ss(&mat(2, 2));
        __m256 m23_23 = _mm256_broadcast_ss(&mat(2, 3));
        
        for (size_t i = 0; i < simdCount; i += 4) {
            // 加载 4 个点的 x, y, z 分量
            __m256 px = _mm256_set_ps(
                input[i+3].x(), input[i+2].x(), input[i+1].x(), input[i].x(),
                input[i+3].x(), input[i+2].x(), input[i+1].x(), input[i].x()
            );
            __m256 py = _mm256_set_ps(
                input[i+3].y(), input[i+2].y(), input[i+1].y(), input[i].y(),
                input[i+3].y(), input[i+2].y(), input[i+1].y(), input[i].y()
            );
            __m256 pz = _mm256_set_ps(
                input[i+3].z(), input[i+2].z(), input[i+1].z(), input[i].z(),
                input[i+3].z(), input[i+2].z(), input[i+1].z(), input[i].z()
            );
            
            // 矩阵-向量乘法：result = mat * [x, y, z, 1]^T
            // rx = m00*x + m01*y + m02*z + m03
            __m256 rx = _mm256_mul_ps(m00_03, px);
            rx = _mm256_fmadd_ps(m01_03, py, rx);
            rx = _mm256_fmadd_ps(m02_03, pz, rx);
            rx = _mm256_add_ps(rx, m03_03);
            
            // ry = m10*x + m11*y + m12*z + m13
            __m256 ry = _mm256_mul_ps(m10_13, px);
            ry = _mm256_fmadd_ps(m11_13, py, ry);
            ry = _mm256_fmadd_ps(m12_13, pz, ry);
            ry = _mm256_add_ps(ry, m13_13);
            
            // rz = m20*x + m21*y + m22*z + m23
            __m256 rz = _mm256_mul_ps(m20_23, px);
            rz = _mm256_fmadd_ps(m21_23, py, rz);
            rz = _mm256_fmadd_ps(m22_23, pz, rz);
            rz = _mm256_add_ps(rz, m23_23);
            
            // 存储结果（提取低128位，包含4个点）
            alignas(32) float tempX[8], tempY[8], tempZ[8];
            _mm256_store_ps(tempX, rx);
            _mm256_store_ps(tempY, ry);
            _mm256_store_ps(tempZ, rz);
            
            // 写入输出（只使用低4个元素）
            output[i].x() = tempX[0];
            output[i].y() = tempY[0];
            output[i].z() = tempZ[0];
            
            output[i+1].x() = tempX[1];
            output[i+1].y() = tempY[1];
            output[i+1].z() = tempZ[1];
            
            output[i+2].x() = tempX[2];
            output[i+2].y() = tempY[2];
            output[i+2].z() = tempZ[2];
            
            output[i+3].x() = tempX[3];
            output[i+3].y() = tempY[3];
            output[i+3].z() = tempZ[3];
        }
    }
    
    // 处理剩余点（标量）
    for (size_t i = simdCount; i < count; ++i) {
        Vector4 p(input[i].x(), input[i].y(), input[i].z(), 1.0f);
        Vector4 result = mat * p;
        output[i] = Vector3(result.x(), result.y(), result.z());
    }
    
#elif defined(__SSE__)
    // SSE 实现：一次处理 1 个点
    // 注意：Eigen Matrix 默认是列主序
    for (size_t i = 0; i < count; ++i) {
        float x = input[i].x();
        float y = input[i].y();
        float z = input[i].z();
        
        // 计算 result = mat * [x, y, z, 1]^T
        // Eigen 是列主序，所以 mat(row, col)
        float rx = mat(0, 0) * x + mat(0, 1) * y + mat(0, 2) * z + mat(0, 3);
        float ry = mat(1, 0) * x + mat(1, 1) * y + mat(1, 2) * z + mat(1, 3);
        float rz = mat(2, 0) * x + mat(2, 1) * y + mat(2, 2) * z + mat(2, 3);
        
        output[i] = Vector3(rx, ry, rz);
    }
    
#else
    // 标量回退实现
    for (size_t i = 0; i < count; ++i) {
        Vector4 p(input[i].x(), input[i].y(), input[i].z(), 1.0f);
        Vector4 result = mat * p;
        output[i] = Vector3(result.x(), result.y(), result.z());
    }
#endif
}

void Transform::TransformDirectionsSIMD(const Quaternion& rot,
                                       const Vector3* input,
                                       Vector3* output,
                                       size_t count) {
    // P1-2.2-6: 四元数旋转的 SIMD 优化
    // 使用优化公式：v' = v + 2 * cross(q.xyz, cross(q.xyz, v) + q.w * v)
    // 展开为：t = 2 * cross(q.xyz, v)
    //         v' = v + q.w * t + cross(q.xyz, t)
    
#if defined(__AVX2__) && defined(__FMA__)
    // AVX2 + FMA 实现：一次处理 4 个向量
    const size_t simdCount = count & ~3ULL;
    
    if (simdCount >= 4) {
        // 提取四元数分量
        float qx = rot.x();
        float qy = rot.y();
        float qz = rot.z();
        float qw = rot.w();
        
        // 广播四元数分量到 SIMD 寄存器
        __m256 qx_vec = _mm256_set1_ps(qx);
        __m256 qy_vec = _mm256_set1_ps(qy);
        __m256 qz_vec = _mm256_set1_ps(qz);
        __m256 qw_vec = _mm256_set1_ps(qw);
        __m256 two = _mm256_set1_ps(2.0f);
        
        for (size_t i = 0; i < simdCount; i += 4) {
            // 加载 4 个向量的 x, y, z 分量
            __m256 vx = _mm256_set_ps(
                input[i+3].x(), input[i+2].x(), input[i+1].x(), input[i].x(),
                input[i+3].x(), input[i+2].x(), input[i+1].x(), input[i].x()
            );
            __m256 vy = _mm256_set_ps(
                input[i+3].y(), input[i+2].y(), input[i+1].y(), input[i].y(),
                input[i+3].y(), input[i+2].y(), input[i+1].y(), input[i].y()
            );
            __m256 vz = _mm256_set_ps(
                input[i+3].z(), input[i+2].z(), input[i+1].z(), input[i].z(),
                input[i+3].z(), input[i+2].z(), input[i+1].z(), input[i].z()
            );
            
            // 第一次叉乘：t1 = cross(q.xyz, v)
            // cross(a, b) = (a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x)
            __m256 t1x = _mm256_sub_ps(
                _mm256_mul_ps(qy_vec, vz),
                _mm256_mul_ps(qz_vec, vy)
            );
            __m256 t1y = _mm256_sub_ps(
                _mm256_mul_ps(qz_vec, vx),
                _mm256_mul_ps(qx_vec, vz)
            );
            __m256 t1z = _mm256_sub_ps(
                _mm256_mul_ps(qx_vec, vy),
                _mm256_mul_ps(qy_vec, vx)
            );
            
            // t1 += q.w * v
            t1x = _mm256_fmadd_ps(qw_vec, vx, t1x);
            t1y = _mm256_fmadd_ps(qw_vec, vy, t1y);
            t1z = _mm256_fmadd_ps(qw_vec, vz, t1z);
            
            // 第二次叉乘：t2 = cross(q.xyz, t1)
            __m256 t2x = _mm256_sub_ps(
                _mm256_mul_ps(qy_vec, t1z),
                _mm256_mul_ps(qz_vec, t1y)
            );
            __m256 t2y = _mm256_sub_ps(
                _mm256_mul_ps(qz_vec, t1x),
                _mm256_mul_ps(qx_vec, t1z)
            );
            __m256 t2z = _mm256_sub_ps(
                _mm256_mul_ps(qx_vec, t1y),
                _mm256_mul_ps(qy_vec, t1x)
            );
            
            // 最终结果：v' = v + 2 * t2
            __m256 rx = _mm256_fmadd_ps(two, t2x, vx);
            __m256 ry = _mm256_fmadd_ps(two, t2y, vy);
            __m256 rz = _mm256_fmadd_ps(two, t2z, vz);
            
            // 存储结果
            alignas(32) float tempX[8], tempY[8], tempZ[8];
            _mm256_store_ps(tempX, rx);
            _mm256_store_ps(tempY, ry);
            _mm256_store_ps(tempZ, rz);
            
            output[i].x() = tempX[0];
            output[i].y() = tempY[0];
            output[i].z() = tempZ[0];
            
            output[i+1].x() = tempX[1];
            output[i+1].y() = tempY[1];
            output[i+1].z() = tempZ[1];
            
            output[i+2].x() = tempX[2];
            output[i+2].y() = tempY[2];
            output[i+2].z() = tempZ[2];
            
            output[i+3].x() = tempX[3];
            output[i+3].y() = tempY[3];
            output[i+3].z() = tempZ[3];
        }
    }
    
    // 处理剩余向量（标量）
    for (size_t i = simdCount; i < count; ++i) {
        output[i] = rot * input[i];
    }
    
#elif defined(__SSE__)
    // SSE 实现：使用优化的标量版本（一次一个向量）
    float qx = rot.x();
    float qy = rot.y();
    float qz = rot.z();
    float qw = rot.w();
    
    for (size_t i = 0; i < count; ++i) {
        float vx = input[i].x();
        float vy = input[i].y();
        float vz = input[i].z();
        
        // 第一次叉乘：t1 = cross(q.xyz, v) + q.w * v
        float t1x = qy * vz - qz * vy + qw * vx;
        float t1y = qz * vx - qx * vz + qw * vy;
        float t1z = qx * vy - qy * vx + qw * vz;
        
        // 第二次叉乘：t2 = cross(q.xyz, t1)
        float t2x = qy * t1z - qz * t1y;
        float t2y = qz * t1x - qx * t1z;
        float t2z = qx * t1y - qy * t1x;
        
        // 最终结果：v' = v + 2 * t2
        output[i].x() = vx + 2.0f * t2x;
        output[i].y() = vy + 2.0f * t2y;
        output[i].z() = vz + 2.0f * t2z;
    }
    
#else
    // 标量回退实现
    for (size_t i = 0; i < count; ++i) {
        output[i] = rot * input[i];
    }
#endif
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
    worldPoints.resize(localPoints.size());
    const size_t count = localPoints.size();
    
    if (count == 0) return;
    
    // P1-2.2: 使用新的 SIMD 优化实现
    // 优势：
    // 1. SIMD 向量化（AVX2 4x, SSE 1x）
    // 2. 减少锁竞争（一次获取矩阵）
    // 3. 比 OpenMP 更高效（避免线程创建开销）
    
    const Matrix4 worldMat = GetWorldMatrix();
    
    // 对于大批量数据，可以考虑结合 OpenMP + SIMD
    #ifdef _OPENMP
    if (count > 10000) {
        // 大批量：OpenMP 多线程 + 每个线程使用 SIMD
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < static_cast<int>(count); i += 256) {
            size_t batchSize = std::min<size_t>(256, count - i);
            TransformPointsSIMD(worldMat, 
                               &localPoints[i], 
                               &worldPoints[i], 
                               batchSize);
        }
        return;
    }
    #endif
    
    // 中小批量：直接使用 SIMD（比 OpenMP 线程创建更快）
    TransformPointsSIMD(worldMat, 
                       localPoints.data(), 
                       worldPoints.data(), 
                       count);
}

void Transform::TransformDirections(const std::vector<Vector3>& localDirections,
                                    std::vector<Vector3>& worldDirections) const {
    worldDirections.resize(localDirections.size());
    const size_t count = localDirections.size();
    
    if (count == 0) return;
    
    // P1-2.2: 使用新的 SIMD 优化实现
    const Quaternion worldRot = GetWorldRotation();
    
    #ifdef _OPENMP
    if (count > 10000) {
        // 大批量：OpenMP 多线程 + 每个线程使用 SIMD
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < static_cast<int>(count); i += 256) {
            size_t batchSize = std::min<size_t>(256, count - i);
            TransformDirectionsSIMD(worldRot,
                                   &localDirections[i],
                                   &worldDirections[i],
                                   batchSize);
        }
        return;
    }
    #endif
    
    // 中小批量：直接使用 SIMD
    TransformDirectionsSIMD(worldRot,
                           localDirections.data(),
                           worldDirections.data(),
                           count);
}

// ============================================================================
// 组件变化回调支持
// ============================================================================

void Transform::SetChangeCallback(ChangeCallback callback) {
    std::unique_lock<std::shared_mutex> lock(m_dataMutex);
    m_coldData->changeCallback = std::move(callback);
}

void Transform::ClearChangeCallback() {
    std::unique_lock<std::shared_mutex> lock(m_dataMutex);
    m_coldData->changeCallback = nullptr;
}

void Transform::NotifyChanged() {
    // 注意：此方法应该在持有锁的情况下调用
    // 但我们不能在持有锁的情况下调用回调，避免死锁
    // 所以我们需要先复制回调，然后释放锁再调用
    
    // 注意：此方法现在主要用于内部调用，但实际通知逻辑
    // 已经在SetPosition、SetRotation、SetScale等方法中实现
    // 这里保留作为辅助方法，供未来扩展使用
    
    std::unique_lock<std::shared_mutex> lock(m_dataMutex);
    ChangeCallback callback = m_changeCallback;
    lock.unlock();  // 释放锁
    
    // 如果设置了回调，调用它
    if (callback) {
        try {
            callback(this);
        } catch (...) {
            // 忽略回调异常，避免影响Transform操作
            // 在实际项目中可以考虑记录日志
        }
    }
}

} // namespace Render
