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
#include "render/skinning.h"
#include "render/logger.h"

#include <queue>

namespace Render {

Skeleton Skeleton::FromSkinningData(const MeshSkinningData& data) {
    Skeleton skeleton;
    skeleton.m_bones.reserve(data.bones.size());
    skeleton.m_children.assign(data.bones.size(), {});
    skeleton.m_boneOffsets = data.boneOffsetMatrices;
    if (skeleton.m_boneOffsets.size() < data.bones.size()) {
        skeleton.m_boneOffsets.resize(data.bones.size(), Matrix4::Identity());
    }

    for (size_t index = 0; index < data.bones.size(); ++index) {
        const auto& srcBone = data.bones[index];
        SkeletonBone bone;
        bone.name = srcBone.name;

        if (!srcBone.parentName.empty()) {
            auto it = data.boneNameToIndex.find(srcBone.parentName);
            if (it != data.boneNameToIndex.end()) {
                bone.parentIndex = static_cast<int32_t>(it->second);
            } else {
                Logger::GetInstance().Warning(
                    "Skeleton::FromSkinningData: 未找到父骨骼 " + srcBone.parentName +
                    "，骨骼 " + srcBone.name + " 将作为根骨骼处理");
            }
        }

        skeleton.m_nameToIndex[bone.name] = index;
        skeleton.m_bones.push_back(bone);
    }

    for (size_t index = 0; index < skeleton.m_bones.size(); ++index) {
        const auto parentIndex = skeleton.m_bones[index].parentIndex;
        if (parentIndex >= 0 && static_cast<size_t>(parentIndex) < skeleton.m_children.size()) {
            skeleton.m_children[static_cast<size_t>(parentIndex)].push_back(index);
        }
    }

    return skeleton;
}

std::optional<size_t> Skeleton::FindBoneIndex(const std::string& name) const {
    auto it = m_nameToIndex.find(name);
    if (it == m_nameToIndex.end()) {
        return std::nullopt;
    }
    return it->second;
}

void Skeleton::EvaluateWorldTransforms(const SkeletonPose& pose, MatrixArray& outWorld) const {
    const size_t boneCount = m_bones.size();
    outWorld.resize(boneCount, Matrix4::Identity());

    if (boneCount == 0) {
        return;
    }

    const auto& localTransforms = pose.localTransforms;

    for (size_t i = 0; i < boneCount; ++i) {
        Matrix4 local = Matrix4::Identity();
        if (i < localTransforms.size()) {
            local = localTransforms[i];
        }

        const int32_t parentIndex = m_bones[i].parentIndex;
        if (parentIndex >= 0) {
            outWorld[i] = outWorld[static_cast<size_t>(parentIndex)] * local;
        } else {
            outWorld[i] = local;
        }
    }
}

void Skeleton::BuildSkinningPalette(const SkeletonPose& pose, MatrixArray& outPalette) const {
    const size_t boneCount = m_bones.size();
    outPalette.resize(boneCount, Matrix4::Identity());

    if (boneCount == 0) {
        return;
    }

    MatrixArray worldMatrices;
    EvaluateWorldTransforms(pose, worldMatrices);

    for (size_t i = 0; i < boneCount; ++i) {
        const Matrix4& offset = (i < m_boneOffsets.size()) ? m_boneOffsets[i] : Matrix4::Identity();
        outPalette[i] = worldMatrices[i] * offset;
    }
}

} // namespace Render
