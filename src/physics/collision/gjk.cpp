#include "render/physics/collision/gjk.h"

namespace Render {
namespace Physics {

// ============================================================================
// GJK 算法实现
// ============================================================================

Vector3 GJK::Support(
    const CollisionShape* shapeA, const Vector3& posA, const Quaternion& rotA,
    const CollisionShape* shapeB, const Vector3& posB, const Quaternion& rotB,
    const Vector3& direction
) {
    // 计算闵可夫斯基差的支撑点
    // Support(A - B, d) = Support(A, d) - Support(B, -d)
    
    Matrix3 rotMatrixA = rotA.toRotationMatrix();
    Matrix3 rotMatrixB = rotB.toRotationMatrix();
    
    // 将方向转换到各自的局部空间
    Vector3 localDirA = rotMatrixA.transpose() * direction;
    Vector3 localDirB = rotMatrixB.transpose() * (-direction);
    
    // 获取局部支撑点
    Vector3 localSupportA = shapeA->GetSupportPoint(localDirA);
    Vector3 localSupportB = shapeB->GetSupportPoint(localDirB);
    
    // 转换到世界空间
    Vector3 worldSupportA = posA + rotMatrixA * localSupportA;
    Vector3 worldSupportB = posB + rotMatrixB * localSupportB;
    
    // 闵可夫斯基差
    return worldSupportA - worldSupportB;
}

bool GJK::Intersects(
    const CollisionShape* shapeA, const Vector3& posA, const Quaternion& rotA,
    const CollisionShape* shapeB, const Vector3& posB, const Quaternion& rotB
) {
    // 初始搜索方向：从 A 指向 B
    Vector3 direction = posB - posA;
    if (direction.squaredNorm() < MathUtils::EPSILON * MathUtils::EPSILON) {
        direction = Vector3::UnitX();
    }
    
    // 初始化单纯形
    Simplex simplex;
    
    // 第一个支撑点
    Vector3 support = Support(shapeA, posA, rotA, shapeB, posB, rotB, direction);
    simplex.Push(support);
    
    // 新的搜索方向：朝向原点
    direction = -support;
    
    // 迭代搜索
    for (int iteration = 0; iteration < MAX_ITERATIONS; ++iteration) {
        // 获取新的支撑点
        support = Support(shapeA, posA, rotA, shapeB, posB, rotB, direction);
        
        // 如果新点没有越过原点，说明不可能包含原点
        if (support.dot(direction) < 0.0f) {
            return false;
        }
        
        // 添加到单纯形
        simplex.Push(support);
        
        // 更新单纯形，检查是否包含原点
        if (UpdateSimplex(simplex, direction)) {
            return true;  // 包含原点，相交
        }
    }
    
    // 达到最大迭代次数，保守返回不相交
    return false;
}

bool GJK::IntersectsWithManifold(
    const CollisionShape* shapeA, const Vector3& posA, const Quaternion& rotA,
    const CollisionShape* shapeB, const Vector3& posB, const Quaternion& rotB,
    ContactManifold& manifold
) {
    // 先用 GJK 检测是否相交
    Vector3 direction = posB - posA;
    if (direction.squaredNorm() < MathUtils::EPSILON * MathUtils::EPSILON) {
        direction = Vector3::UnitX();
    }
    
    Simplex simplex;
    Vector3 support = Support(shapeA, posA, rotA, shapeB, posB, rotB, direction);
    simplex.Push(support);
    direction = -support;
    
    for (int iteration = 0; iteration < MAX_ITERATIONS; ++iteration) {
        support = Support(shapeA, posA, rotA, shapeB, posB, rotB, direction);
        
        if (support.dot(direction) < 0.0f) {
            return false;  // 不相交
        }
        
        simplex.Push(support);
        
        if (UpdateSimplex(simplex, direction)) {
            // 相交，使用 EPA 计算穿透信息
            return EPA::ComputePenetration(simplex, shapeA, posA, rotA, shapeB, posB, rotB, manifold);
        }
    }
    
    return false;
}

bool GJK::UpdateSimplex(Simplex& simplex, Vector3& direction) {
    switch (simplex.Size()) {
        case 2: return DoLine(simplex, direction);
        case 3: return DoTriangle(simplex, direction);
        case 4: return DoTetrahedron(simplex, direction);
        default: return false;
    }
}

bool GJK::DoLine(Simplex& simplex, Vector3& direction) {
    Vector3 a = simplex[1];  // 最新点
    Vector3 b = simplex[0];
    
    Vector3 ab = b - a;
    Vector3 ao = -a;
    
    if (ab.dot(ao) > 0.0f) {
        // 原点在线段方向上
        direction = ab.cross(ao).cross(ab);
        if (direction.squaredNorm() < MathUtils::EPSILON * MathUtils::EPSILON) {
            // ab 和 ao 平行，使用垂直方向
            direction = Vector3(ab.y(), -ab.x(), 0).normalized();
        }
    } else {
        // 原点在 a 的另一侧
        simplex.points[0] = a;
        simplex.count = 1;
        direction = ao;
    }
    
    return false;
}

bool GJK::DoTriangle(Simplex& simplex, Vector3& direction) {
    Vector3 a = simplex[2];  // 最新点
    Vector3 b = simplex[1];
    Vector3 c = simplex[0];
    
    Vector3 ab = b - a;
    Vector3 ac = c - a;
    Vector3 ao = -a;
    
    Vector3 abc = ab.cross(ac);
    
    // 检查原点在三角形的哪一侧
    if (abc.cross(ac).dot(ao) > 0.0f) {
        if (ac.dot(ao) > 0.0f) {
            // 原点在 AC 边区域
            simplex.points[0] = c;
            simplex.points[1] = a;
            simplex.count = 2;
            direction = ac.cross(ao).cross(ac);
        } else {
            // 原点在 AB 边区域
            simplex.points[0] = b;
            simplex.points[1] = a;
            simplex.count = 2;
            return DoLine(simplex, direction);
        }
    } else {
        if (ab.cross(abc).dot(ao) > 0.0f) {
            // 原点在 AB 边区域
            simplex.points[0] = b;
            simplex.points[1] = a;
            simplex.count = 2;
            return DoLine(simplex, direction);
        } else {
            // 原点在三角形面区域
            if (abc.dot(ao) > 0.0f) {
                direction = abc;
            } else {
                // 原点在三角形背面
                simplex.points[0] = b;
                simplex.points[1] = c;
                simplex.points[2] = a;
                direction = -abc;
            }
        }
    }
    
    return false;
}

bool GJK::DoTetrahedron(Simplex& simplex, Vector3& direction) {
    Vector3 a = simplex[3];  // 最新点
    Vector3 b = simplex[2];
    Vector3 c = simplex[1];
    Vector3 d = simplex[0];
    
    Vector3 ab = b - a;
    Vector3 ac = c - a;
    Vector3 ad = d - a;
    Vector3 ao = -a;
    
    Vector3 abc = ab.cross(ac);
    Vector3 acd = ac.cross(ad);
    Vector3 adb = ad.cross(ab);
    
    // 检查原点在四面体的哪一侧
    if (abc.dot(ao) > 0.0f) {
        simplex.points[0] = c;
        simplex.points[1] = b;
        simplex.points[2] = a;
        simplex.count = 3;
        return DoTriangle(simplex, direction);
    }
    
    if (acd.dot(ao) > 0.0f) {
        simplex.points[0] = d;
        simplex.points[1] = c;
        simplex.points[2] = a;
        simplex.count = 3;
        return DoTriangle(simplex, direction);
    }
    
    if (adb.dot(ao) > 0.0f) {
        simplex.points[0] = b;
        simplex.points[1] = d;
        simplex.points[2] = a;
        simplex.count = 3;
        return DoTriangle(simplex, direction);
    }
    
    // 原点在四面体内部
    return true;
}

// ============================================================================
// EPA 算法完整实现
// ============================================================================

namespace {

/**
 * @brief EPA 三角形面
 */
struct EPAFace {
    Vector3 points[3];     // 三个顶点
    Vector3 normal;        // 面法线
    float distance;        // 到原点的距离
    
    EPAFace(const Vector3& a, const Vector3& b, const Vector3& c) {
        points[0] = a;
        points[1] = b;
        points[2] = c;
        
        Vector3 ab = b - a;
        Vector3 ac = c - a;
        normal = ab.cross(ac);
        
        float len = normal.norm();
        if (len > MathUtils::EPSILON) {
            normal /= len;
            distance = std::abs(normal.dot(a));
        } else {
            normal = Vector3::UnitY();
            distance = 0.0f;
        }
    }
};

} // anonymous namespace

bool EPA::ComputePenetration(
    const GJK::Simplex& simplex,
    const CollisionShape* shapeA, const Vector3& posA, const Quaternion& rotA,
    const CollisionShape* shapeB, const Vector3& posB, const Quaternion& rotB,
    ContactManifold& manifold
) {
    // 从 GJK 的四面体单纯形构建初始多面体
    if (simplex.Size() != 4) {
        // 如果不是完整的四面体，返回简化结果
        Vector3 closestPoint = simplex[0];
        float minDist = closestPoint.norm();
        
        for (int i = 1; i < simplex.Size(); ++i) {
            float dist = simplex[i].norm();
            if (dist < minDist) {
                minDist = dist;
                closestPoint = simplex[i];
            }
        }
        
        if (minDist < MathUtils::EPSILON) {
            manifold.SetNormal(Vector3::UnitY());
            manifold.penetration = 0.1f;
            manifold.AddContact((posA + posB) * 0.5f, 0.1f);
            return true;
        }
        
        Vector3 normal = closestPoint.normalized();
        manifold.SetNormal(normal);
        manifold.penetration = minDist;
        manifold.AddContact((posA + posB) * 0.5f + normal * (minDist * 0.5f), minDist);
        return true;
    }
    
    // 构建初始的四个面
    std::vector<EPAFace> faces;
    faces.reserve(64);
    
    Vector3 a = simplex[0];
    Vector3 b = simplex[1];
    Vector3 c = simplex[2];
    Vector3 d = simplex[3];
    
    faces.emplace_back(a, b, c);
    faces.emplace_back(a, c, d);
    faces.emplace_back(a, d, b);
    faces.emplace_back(b, d, c);
    
    // 确保法线朝外
    for (auto& face : faces) {
        Vector3 center = (face.points[0] + face.points[1] + face.points[2]) / 3.0f;
        if (center.dot(face.normal) < 0.0f) {
            face.normal = -face.normal;
        }
    }
    
    // EPA 迭代
    for (int iteration = 0; iteration < MAX_ITERATIONS; ++iteration) {
        // 找到距离原点最近的面
        int minIndex = 0;
        float minDistance = faces[0].distance;
        
        for (size_t i = 1; i < faces.size(); ++i) {
            if (faces[i].distance < minDistance) {
                minDistance = faces[i].distance;
                minIndex = static_cast<int>(i);
            }
        }
        
        EPAFace& closestFace = faces[minIndex];
        
        // 在该面法线方向上获取新的支撑点
        Vector3 newPoint = GJK::Support(shapeA, posA, rotA, shapeB, posB, rotB, closestFace.normal);
        
        // 检查新点是否显著改进
        float newDistance = newPoint.dot(closestFace.normal);
        if (newDistance - minDistance < MathUtils::EPSILON) {
            // 收敛，返回结果
            manifold.SetNormal(closestFace.normal);
            manifold.penetration = minDistance;
            
            // 计算接触点（在最近面上）
            Vector3 contactPoint = closestFace.normal * minDistance;
            manifold.AddContact(posA + contactPoint * 0.5f, minDistance);
            
            return true;
        }
        
        // 移除能看到新点的面，添加新面
        std::vector<std::pair<Vector3, Vector3>> edges;
        
        for (int i = static_cast<int>(faces.size()) - 1; i >= 0; --i) {
            Vector3 toPoint = newPoint - faces[i].points[0];
            if (faces[i].normal.dot(toPoint) > 0.0f) {
                // 这个面能看到新点，需要移除
                // 保存边
                for (int j = 0; j < 3; ++j) {
                    Vector3 edgeA = faces[i].points[j];
                    Vector3 edgeB = faces[i].points[(j + 1) % 3];
                    
                    // 检查这条边是否被多个面共享
                    bool unique = true;
                    for (auto it = edges.begin(); it != edges.end(); ++it) {
                        if ((it->first.isApprox(edgeA) && it->second.isApprox(edgeB)) ||
                            (it->first.isApprox(edgeB) && it->second.isApprox(edgeA))) {
                            edges.erase(it);
                            unique = false;
                            break;
                        }
                    }
                    
                    if (unique) {
                        edges.push_back({edgeA, edgeB});
                    }
                }
                
                faces.erase(faces.begin() + i);
            }
        }
        
        // 用新点和唯一边创建新面
        for (const auto& [edgeA, edgeB] : edges) {
            faces.emplace_back(edgeA, edgeB, newPoint);
        }
        
        // 防止面数过多
        if (faces.size() > 100) {
            break;
        }
    }
    
    // 达到最大迭代次数，返回最近面的信息
    if (!faces.empty()) {
        int minIndex = 0;
        float minDistance = faces[0].distance;
        
        for (size_t i = 1; i < faces.size(); ++i) {
            if (faces[i].distance < minDistance) {
                minDistance = faces[i].distance;
                minIndex = static_cast<int>(i);
            }
        }
        
        manifold.SetNormal(faces[minIndex].normal);
        manifold.penetration = minDistance;
        manifold.AddContact((posA + posB) * 0.5f, minDistance);
        return true;
    }
    
    return false;
}

} // namespace Physics
} // namespace Render

