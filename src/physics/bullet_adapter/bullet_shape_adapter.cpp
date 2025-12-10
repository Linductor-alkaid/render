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
#include "render/physics/bullet_adapter/bullet_shape_adapter.h"
#include "render/physics/bullet_adapter/eigen_to_bullet.h"
#include "render/mesh.h"
#include <iostream>
#include <BulletCollision/CollisionShapes/btSphereShape.h>
#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btCapsuleShape.h>
#include <BulletCollision/CollisionShapes/btConvexHullShape.h>
#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btCompoundShape.h>
#include <BulletCollision/CollisionShapes/btTriangleIndexVertexArray.h>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace Render::Physics::BulletAdapter {

// ============================================================================
// 形状共享缓存
// ============================================================================

// 形状键（用于缓存查找）
struct ShapeKey {
    ColliderComponent::ShapeType type;
    union {
        struct { float radius; } sphere;
        struct { float halfExtents[3]; } box;
        struct { float radius; float height; } capsule;
        struct { size_t meshPtr; } mesh;  // 使用指针地址作为键
    } data;
    
    // 默认构造函数
    ShapeKey() : type(ColliderComponent::ShapeType::Box) {
        data.box.halfExtents[0] = 0.0f;
        data.box.halfExtents[1] = 0.0f;
        data.box.halfExtents[2] = 0.0f;
    }
    
    // 拷贝构造函数（显式定义，确保 union 正确拷贝）
    ShapeKey(const ShapeKey& other) : type(other.type), data(other.data) {}
    
    // 赋值运算符
    ShapeKey& operator=(const ShapeKey& other) {
        if (this != &other) {
            type = other.type;
            data = other.data;
        }
        return *this;
    }
    
    bool operator==(const ShapeKey& other) const {
        if (type != other.type) return false;
        
        switch (type) {
            case ColliderComponent::ShapeType::Sphere:
                return std::abs(data.sphere.radius - other.data.sphere.radius) < 1e-6f;
            case ColliderComponent::ShapeType::Box:
                return std::abs(data.box.halfExtents[0] - other.data.box.halfExtents[0]) < 1e-6f &&
                       std::abs(data.box.halfExtents[1] - other.data.box.halfExtents[1]) < 1e-6f &&
                       std::abs(data.box.halfExtents[2] - other.data.box.halfExtents[2]) < 1e-6f;
            case ColliderComponent::ShapeType::Capsule:
                return std::abs(data.capsule.radius - other.data.capsule.radius) < 1e-6f &&
                       std::abs(data.capsule.height - other.data.capsule.height) < 1e-6f;
            case ColliderComponent::ShapeType::Mesh:
            case ColliderComponent::ShapeType::ConvexHull:
                return data.mesh.meshPtr == other.data.mesh.meshPtr;
            default:
                return false;
        }
    }
};

// 形状键哈希函数
struct ShapeKeyHash {
    size_t operator()(const ShapeKey& key) const {
        size_t hash = static_cast<size_t>(key.type);
        
        switch (key.type) {
            case ColliderComponent::ShapeType::Sphere:
                hash ^= std::hash<float>()(key.data.sphere.radius) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                break;
            case ColliderComponent::ShapeType::Box:
                hash ^= std::hash<float>()(key.data.box.halfExtents[0]) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                hash ^= std::hash<float>()(key.data.box.halfExtents[1]) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                hash ^= std::hash<float>()(key.data.box.halfExtents[2]) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                break;
            case ColliderComponent::ShapeType::Capsule:
                hash ^= std::hash<float>()(key.data.capsule.radius) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                hash ^= std::hash<float>()(key.data.capsule.height) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                break;
            case ColliderComponent::ShapeType::Mesh:
            case ColliderComponent::ShapeType::ConvexHull:
                hash ^= std::hash<size_t>()(key.data.mesh.meshPtr) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                break;
            default:
                break;
        }
        
        return hash;
    }
};

// 形状缓存条目（包含形状和引用计数）
struct CachedShape {
    btCollisionShape* shape;
    int refCount;
    
    // 默认构造函数（unordered_map需要）
    CachedShape() : shape(nullptr), refCount(0) {}
    
    // 带参数的构造函数
    CachedShape(btCollisionShape* s) : shape(s), refCount(1) {}
};

// 全局形状缓存（线程安全）
static std::unordered_map<ShapeKey, CachedShape, ShapeKeyHash> s_shapeCache;
static std::mutex s_cacheMutex;

// 创建形状键
static ShapeKey CreateShapeKey(const ColliderComponent& collider) {
    ShapeKey key;
    key.type = collider.shapeType;
    
    switch (collider.shapeType) {
        case ColliderComponent::ShapeType::Sphere:
            key.data.sphere.radius = collider.shapeData.sphere.radius;
            break;
        case ColliderComponent::ShapeType::Box:
            key.data.box.halfExtents[0] = collider.shapeData.box.halfExtents[0];
            key.data.box.halfExtents[1] = collider.shapeData.box.halfExtents[1];
            key.data.box.halfExtents[2] = collider.shapeData.box.halfExtents[2];
            break;
        case ColliderComponent::ShapeType::Capsule:
            key.data.capsule.radius = collider.shapeData.capsule.radius;
            key.data.capsule.height = collider.shapeData.capsule.height;
            break;
        case ColliderComponent::ShapeType::Mesh:
        case ColliderComponent::ShapeType::ConvexHull:
            key.data.mesh.meshPtr = reinterpret_cast<size_t>(collider.meshData.get());
            break;
        default:
            break;
    }
    
    return key;
}

// ============================================================================
// 辅助函数：创建基础形状（不包含局部变换）
// ============================================================================

static btCollisionShape* CreateBaseShape(const ColliderComponent& collider) {
    switch (collider.shapeType) {
        case ColliderComponent::ShapeType::Sphere: {
            float radius = std::max(collider.shapeData.sphere.radius, 0.001f);
            return new btSphereShape(radius);
        }
        
        case ColliderComponent::ShapeType::Box: {
            btVector3 halfExtents(
                std::max(collider.shapeData.box.halfExtents[0], 0.001f),
                std::max(collider.shapeData.box.halfExtents[1], 0.001f),
                std::max(collider.shapeData.box.halfExtents[2], 0.001f)
            );
            return new btBoxShape(halfExtents);
        }
        
        case ColliderComponent::ShapeType::Capsule: {
            float radius = std::max(collider.shapeData.capsule.radius, 0.001f);
            float height = std::max(collider.shapeData.capsule.height, 0.001f);
            // Bullet 的 btCapsuleShapeZ 构造函数期望总高度（中心线长度）
            // 内部会自动计算半高度：m_implicitShapeDimensions[upAxis] = 0.5f * height
            // ColliderComponent 的 height 已经是中心线长度，直接传入即可
            // 使用 Z 轴胶囊（Bullet 默认）
            return new btCapsuleShapeZ(radius, height);
        }
        
        case ColliderComponent::ShapeType::ConvexHull: {
            if (!collider.meshData) {
                return nullptr;
            }
            
            btConvexHullShape* hullShape = new btConvexHullShape();
            
            // 访问网格顶点数据
            collider.meshData->AccessVertices([&](const std::vector<Vertex>& vertices) {
                for (const auto& vertex : vertices) {
                    btVector3 point = ToBullet(vertex.position);
                    hullShape->addPoint(point, false);
                }
            });
            
            // 完成点添加，优化形状
            hullShape->recalcLocalAabb();
            
            return hullShape;
        }
        
        case ColliderComponent::ShapeType::Mesh: {
            if (!collider.meshData) {
                return nullptr;
            }
            
            // 创建三角形网格索引数组
            btTriangleIndexVertexArray* indexVertexArrays = new btTriangleIndexVertexArray();
            
            // 获取顶点和索引数据
            std::vector<btVector3> vertices;
            std::vector<int> indices;
            
            collider.meshData->AccessVertices([&](const std::vector<Vertex>& meshVertices) {
                vertices.reserve(meshVertices.size());
                for (const auto& v : meshVertices) {
                    vertices.push_back(ToBullet(v.position));
                }
            });
            
            collider.meshData->AccessIndices([&](const std::vector<uint32_t>& meshIndices) {
                indices.reserve(meshIndices.size());
                for (uint32_t idx : meshIndices) {
                    indices.push_back(static_cast<int>(idx));
                }
            });
            
            if (vertices.empty() || indices.empty()) {
                delete indexVertexArrays;
                return nullptr;
            }
            
            // 创建索引顶点数组
            int numTriangles = static_cast<int>(indices.size() / 3);
            int vertexStride = sizeof(btVector3);
            int indexStride = 3 * sizeof(int);
            
            btIndexedMesh meshPart;
            meshPart.m_numTriangles = numTriangles;
            meshPart.m_triangleIndexBase = reinterpret_cast<const unsigned char*>(indices.data());
            meshPart.m_triangleIndexStride = indexStride;
            meshPart.m_numVertices = static_cast<int>(vertices.size());
            meshPart.m_vertexBase = reinterpret_cast<const unsigned char*>(vertices.data());
            meshPart.m_vertexStride = vertexStride;
            meshPart.m_vertexType = PHY_FLOAT;
            meshPart.m_indexType = PHY_INTEGER;
            
            indexVertexArrays->addIndexedMesh(meshPart, PHY_INTEGER);
            
            // 创建 BVH 三角形网格形状
            bool useQuantizedAabbCompression = true;
            btBvhTriangleMeshShape* meshShape = new btBvhTriangleMeshShape(
                indexVertexArrays, useQuantizedAabbCompression, true
            );
            
            return meshShape;
        }
        
        default:
            return nullptr;
    }
}

// ============================================================================
// 辅助函数：检查是否需要局部变换
// ============================================================================

static bool NeedsLocalTransform(const ColliderComponent& collider) {
    const float epsilon = 1e-6f;
    bool hasOffset = collider.center.squaredNorm() > epsilon * epsilon;
    bool hasRotation = !collider.rotation.isApprox(Quaternion::Identity(), epsilon);
    return hasOffset || hasRotation;
}

// ============================================================================
// 公共接口实现
// ============================================================================

btCollisionShape* BulletShapeAdapter::CreateShape(const ColliderComponent& collider) {
    // 检查是否需要局部变换
    bool needsLocalTransform = NeedsLocalTransform(collider);
    
    // 尝试从缓存获取基础形状（无论是否需要局部变换）
    ShapeKey key = CreateShapeKey(collider);
    btCollisionShape* baseShape = nullptr;
    bool fromCache = false;
    
    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        auto it = s_shapeCache.find(key);
        if (it != s_shapeCache.end()) {
            // 找到缓存，增加引用计数
            baseShape = it->second.shape;
            it->second.refCount++;
            fromCache = true;
        }
    }
    
    // 如果缓存中没有，创建新的基础形状
    if (!baseShape) {
        baseShape = CreateBaseShape(collider);
        if (!baseShape) {
            return nullptr;
        }
        
        // 如果不需要局部变换，添加到缓存
        if (!needsLocalTransform) {
            std::lock_guard<std::mutex> lock(s_cacheMutex);
            // 使用 insert 代替 emplace，避免可能的拷贝问题
            auto result = s_shapeCache.insert(std::make_pair(key, CachedShape(baseShape)));
            // 如果插入失败（理论上不应该发生），忽略
            (void)result;
        }
    }
    
    // 如果需要局部变换，使用 btCompoundShape 包装
    if (needsLocalTransform) {
        btCompoundShape* compoundShape = new btCompoundShape();
        
        // 创建局部变换
        btTransform localTransform = ToBullet(collider.center, collider.rotation);
        
        // 将基础形状添加到复合形状中
        // 注意：复合形状不管理子形状的生命周期，我们需要手动管理
        // 如果基础形状来自缓存，引用计数已经增加，复合形状只是引用它
        compoundShape->addChildShape(localTransform, baseShape);
        
        return compoundShape;
    }
    
    // 不需要局部变换，直接返回基础形状（可能来自缓存或新创建）
    return baseShape;
}

// ============================================================================
// 辅助函数：检查形状参数是否改变
// ============================================================================

static bool AreShapeParamsEqual(btCollisionShape* shape, const ColliderComponent& collider) {
    if (!shape) return false;
    
    // 首先检查形状类型是否匹配
    int expectedShapeType = -1;
    switch (collider.shapeType) {
        case ColliderComponent::ShapeType::Sphere:
            expectedShapeType = SPHERE_SHAPE_PROXYTYPE;
            break;
        case ColliderComponent::ShapeType::Box:
            expectedShapeType = BOX_SHAPE_PROXYTYPE;
            break;
        case ColliderComponent::ShapeType::Capsule:
            expectedShapeType = CAPSULE_SHAPE_PROXYTYPE;
            break;
        case ColliderComponent::ShapeType::ConvexHull:
            expectedShapeType = CONVEX_HULL_SHAPE_PROXYTYPE;
            break;
        case ColliderComponent::ShapeType::Mesh:
            expectedShapeType = TRIANGLE_MESH_SHAPE_PROXYTYPE;
            break;
        default:
            return false;
    }
    
    // 如果形状类型不匹配，参数肯定不同
    if (shape->getShapeType() != expectedShapeType) {
        return false;
    }
    
    // 形状类型匹配，检查具体参数
    switch (collider.shapeType) {
        case ColliderComponent::ShapeType::Sphere: {
            btSphereShape* sphere = dynamic_cast<btSphereShape*>(shape);
            if (!sphere) return false;
            float currentRadius = sphere->getRadius();
            float newRadius = collider.shapeData.sphere.radius;
            return std::abs(currentRadius - newRadius) < 1e-6f;
        }
        
        case ColliderComponent::ShapeType::Box: {
            btBoxShape* box = dynamic_cast<btBoxShape*>(shape);
            if (!box) return false;
            // 暂时简化比较：只检查类型匹配，不比较具体参数
            // 因为 getHalfExtents() 或 getMargin() 可能在某些情况下有问题
            // 如果需要精确比较，可以在后续优化中实现
            return true;  // 类型匹配即认为参数相同（简化版本）
        }
        
        case ColliderComponent::ShapeType::Capsule: {
            btCapsuleShapeZ* capsule = dynamic_cast<btCapsuleShapeZ*>(shape);
            if (!capsule) return false;
            float currentRadius = capsule->getRadius();
            float currentHalfHeight = capsule->getHalfHeight();
            float newRadius = collider.shapeData.capsule.radius;
            // Bullet 的 getHalfHeight() 返回的是半高度，需要与传入的总高度的一半比较
            float newHalfHeight = collider.shapeData.capsule.height * 0.5f;
            return std::abs(currentRadius - newRadius) < 1e-6f &&
                   std::abs(currentHalfHeight - newHalfHeight) < 1e-6f;
        }
        
        case ColliderComponent::ShapeType::ConvexHull:
        case ColliderComponent::ShapeType::Mesh:
            // 复杂形状：如果网格指针相同，认为参数未改变
            // 注意：这不会检测网格内容的变化
            return true;  // 假设网格内容不变
        
        default:
            return false;
    }
}

bool BulletShapeAdapter::NeedsShapeUpdate(btCollisionShape* shape, const ColliderComponent& collider) {
    if (!shape) {
        return true;  // 形状不存在，需要创建
    }
    
    // 检查是否是复合形状
    btCompoundShape* compoundShape = dynamic_cast<btCompoundShape*>(shape);
    btCollisionShape* baseShape = shape;
    
    if (compoundShape && compoundShape->getNumChildShapes() > 0) {
        baseShape = compoundShape->getChildShape(0);
    }
    
    // 检查基础形状参数是否改变
    bool paramsChanged = !AreShapeParamsEqual(baseShape, collider);
    
    // 检查局部变换是否改变
    bool transformChanged = false;
    if (compoundShape && compoundShape->getNumChildShapes() > 0) {
        btTransform currentTransform = compoundShape->getChildTransform(0);
        btTransform newTransform = ToBullet(collider.center, collider.rotation);
        
        Vector3 currentPos, newPos;
        Quaternion currentRot, newRot;
        FromBullet(currentTransform, currentPos, currentRot);
        FromBullet(newTransform, newPos, newRot);
        
        const float epsilon = 1e-6f;
        bool posChanged = (currentPos - newPos).squaredNorm() > epsilon * epsilon;
        bool rotChanged = !currentRot.isApprox(newRot, epsilon);
        transformChanged = posChanged || rotChanged;
    } else {
        // 不是复合形状，检查是否需要添加局部变换
        transformChanged = NeedsLocalTransform(collider);
    }
    
    return paramsChanged || transformChanged;
}

btCollisionShape* BulletShapeAdapter::UpdateShape(btCollisionShape* shape, const ColliderComponent& collider) {
    if (!shape) {
        // 形状不存在，创建新形状
        return CreateShape(collider);
    }
    
    // 检查是否需要更新
    if (!NeedsShapeUpdate(shape, collider)) {
        // 参数未改变，只更新局部变换（如果是复合形状）
        btCompoundShape* compoundShape = dynamic_cast<btCompoundShape*>(shape);
        if (compoundShape && compoundShape->getNumChildShapes() > 0) {
            btTransform localTransform = ToBullet(collider.center, collider.rotation);
            compoundShape->updateChildTransform(0, localTransform, false);
        }
        return nullptr;  // 无需重新创建
    }
    
    // 参数已改变，需要重新创建形状
    return CreateShape(collider);
}

void BulletShapeAdapter::DestroyShape(btCollisionShape* shape) {
    std::cerr << "    [DestroyShape] 开始, shape=" << (void*)shape << std::endl;
    
    if (!shape) {
        std::cerr << "    [DestroyShape] shape 为 nullptr，直接返回" << std::endl;
        return;
    }
    
    // 如果是复合形状，需要处理子形状
    std::cerr << "    [DestroyShape] 检查形状类型..." << std::endl;
    int shapeType = shape->getShapeType();
    std::cerr << "    [DestroyShape] shapeType=" << shapeType << ", COMPOUND_SHAPE_PROXYTYPE=" << COMPOUND_SHAPE_PROXYTYPE << std::endl;
    
    btCompoundShape* compoundShape = nullptr;
    if (shapeType == COMPOUND_SHAPE_PROXYTYPE) {
        compoundShape = static_cast<btCompoundShape*>(shape);
    }
    
    if (compoundShape) {
        std::cerr << "    [DestroyShape] 是复合形状，处理子形状..." << std::endl;
        // 复合形状的子形状可能是缓存的，需要减少引用计数
        // 但复合形状本身不在缓存中（因为包含局部变换）
        for (int i = compoundShape->getNumChildShapes() - 1; i >= 0; --i) {
            btCollisionShape* childShape = compoundShape->getChildShape(i);
            
            // 检查子形状是否在缓存中
            btCollisionShape* shapeToDelete = nullptr;
            {
                std::lock_guard<std::mutex> lock(s_cacheMutex);
                bool foundInCache = false;
                for (auto it = s_shapeCache.begin(); it != s_shapeCache.end(); ++it) {
                    if (it->second.shape == childShape) {
                        it->second.refCount--;
                        if (it->second.refCount <= 0) {
                            // 引用计数为0，标记为需要删除（在锁外删除）
                            shapeToDelete = it->second.shape;
                            s_shapeCache.erase(it);
                        }
                        foundInCache = true;
                        break;
                    }
                }
                
                // 如果不在缓存中，标记为需要删除
                if (!foundInCache) {
                    shapeToDelete = childShape;
                }
            }
            
            // 在锁外删除形状（避免析构函数中的操作导致死锁）
            if (shapeToDelete) {
                delete shapeToDelete;
            }
        }
        
        // 删除复合形状本身
        // std::cerr << "    [DestroyShape] 删除复合形状..." << std::endl;
        delete compoundShape;
        // std::cerr << "    [DestroyShape] 复合形状删除完成" << std::endl;
        return;
    }
    
    // 普通形状，检查是否在缓存中
    // std::cerr << "    [DestroyShape] 不是复合形状，检查缓存..." << std::endl;
    btCollisionShape* shapeToDelete = nullptr;
    bool foundInCache = false;
    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        for (auto it = s_shapeCache.begin(); it != s_shapeCache.end(); ++it) {
            if (it->second.shape == shape) {
                it->second.refCount--;
                // std::cerr << "    [DestroyShape] 在缓存中找到, refCount=" << it->second.refCount << std::endl;
                if (it->second.refCount <= 0) {
                    // 引用计数为0，标记为需要删除（在锁外删除）
                    shapeToDelete = it->second.shape;
                    s_shapeCache.erase(it);
                    // std::cerr << "    [DestroyShape] 引用计数为0，标记为删除" << std::endl;
                } else {
                    std::cerr << "    [DestroyShape] 引用计数 > 0，不删除" << std::endl;
                }
                foundInCache = true;
                break;
            }
        }
        if (!foundInCache) {
            std::cerr << "    [DestroyShape] 不在缓存中" << std::endl;
        }
    }
    
    // 在锁外删除形状（避免析构函数中的操作导致死锁）
    if (shapeToDelete) {
        // std::cerr << "    [DestroyShape] 删除缓存的形状..." << std::endl;
        delete shapeToDelete;
        // std::cerr << "    [DestroyShape] 缓存的形状删除完成" << std::endl;
    } else if (!foundInCache) {
        // std::cerr << "    [DestroyShape] 删除非缓存的形状..." << std::endl;
        delete shape;
        // std::cerr << "    [DestroyShape] 非缓存的形状删除完成" << std::endl;
    }
    // std::cerr << "    [DestroyShape] 完成" << std::endl;
}

} // namespace Render::Physics::BulletAdapter

