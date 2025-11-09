#pragma once

#include "render/mesh_loader.h"
#include <optional>
#include <unordered_map>
#include <vector>

namespace Render {

/**
 * @brief 骨骼关节的静态信息。
 */
struct SkeletonBone {
    std::string name;                 ///< 骨骼名称
    int32_t parentIndex = -1;         ///< 父骨骼索引（-1 表示根）
};

/**
 * @brief 描述骨骼姿势的局部矩阵集合。
 */
struct SkeletonPose {
    std::vector<Matrix4, Eigen::aligned_allocator<Matrix4>> localTransforms; ///< 每个骨骼的局部矩阵（骨架空间）

    void Resize(size_t count) {
        localTransforms.resize(count, Matrix4::Identity());
    }

    size_t Size() const { return localTransforms.size(); }
};

/**
 * @brief 骨骼系统，基于 MeshSkinningData 构建。
 */
class Skeleton {
public:
    Skeleton() = default;

    using MatrixArray = std::vector<Matrix4, Eigen::aligned_allocator<Matrix4>>;
    using BoneArray = std::vector<SkeletonBone>;

    /**
     * @brief 从 MeshSkinningData 构建骨骼。
     */
    static Skeleton FromSkinningData(const MeshSkinningData& data);

    size_t GetBoneCount() const { return m_bones.size(); }

    const BoneArray& GetBones() const { return m_bones; }

    const SkeletonBone& GetBone(size_t index) const { return m_bones[index]; }

    std::optional<size_t> FindBoneIndex(const std::string& name) const;

    const std::vector<std::vector<size_t>>& GetChildren() const { return m_children; }

    /**
     * @brief 根据局部姿势计算骨骼世界矩阵。
     * @param pose      局部姿势矩阵；不足部分将使用单位矩阵。
     * @param outWorld  输出世界矩阵数组。
     */
    void EvaluateWorldTransforms(const SkeletonPose& pose, MatrixArray& outWorld) const;

    /**
     * @brief 根据局部姿势生成用于 GPU 蒙皮的骨骼调色板。
     * @param pose 局部姿势矩阵。
     * @param outPalette 结果调色板（世界矩阵乘以 offset）。
     */
    void BuildSkinningPalette(const SkeletonPose& pose, MatrixArray& outPalette) const;

private:
    BoneArray m_bones;
    MatrixArray m_boneOffsets;
    std::unordered_map<std::string, size_t> m_nameToIndex;
    std::vector<std::vector<size_t>> m_children;
};

} // namespace Render
