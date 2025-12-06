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
#include <iostream>
#include <iomanip>
#include <sstream>

#include "render/skinning.h"
#include "render/mesh_loader.h"

using namespace Render;

namespace {

using Render::Matrix4;
using Render::MeshSkinningData;
using Render::MeshBoneInfo;

void PrintUtf8(const std::string& text) {
    std::cout << text;
}

MeshSkinningData BuildSampleSkinningData() {
    MeshSkinningData skinning;

    MeshBoneInfo root;
    root.name = "root";
    root.parentName.clear();

    MeshBoneInfo child;
    child.name = "child";
    child.parentName = "root";

    skinning.bones.push_back(root);
    skinning.bones.push_back(child);

    skinning.boneOffsetMatrices.push_back(Matrix4::Identity());
    skinning.boneOffsetMatrices.push_back(Matrix4::Identity());

    skinning.boneNameToIndex[root.name] = 0;
    skinning.boneNameToIndex[child.name] = 1;

    // 一个顶点示例：完全受 root 影响
    skinning.vertexWeights.resize(1);
    skinning.vertexWeights[0].push_back({0u, 1.0f});

    return skinning;
}

int FindBoneIndexByName(const Skeleton& skeleton, const std::string& name) {
    for (size_t i = 0; i < skeleton.GetBoneCount(); ++i) {
        if (skeleton.GetBone(i).name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void PrintMatrix(const Matrix4& matrix) {
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            std::cout << std::setw(8) << std::fixed << std::setprecision(3)
                      << matrix(row, col) << ' ';
        }
        std::cout << '\n';
    }
}

} // namespace

int main() {
    std::cout << "Starting skeleton palette test..." << std::endl;

    MeshSkinningData skinning = BuildSampleSkinningData();

    std::cout << "Input MeshSkinningData bones: " << skinning.bones.size() << std::endl;
    for (size_t i = 0; i < skinning.bones.size(); ++i) {
        const auto& bone = skinning.bones[i];
        const Matrix4& offset = (i < skinning.boneOffsetMatrices.size()) ? skinning.boneOffsetMatrices[i] : Matrix4::Identity();
        std::cout << "  [" << i << "] name=" << bone.name
                  << ", parentName=" << bone.parentName
                  << ", offset[0,0]=" << offset(0, 0) << std::endl;
    }

    Skeleton skeleton = Skeleton::FromSkinningData(skinning);

    std::cout << "Skeleton bones: " << skeleton.GetBoneCount() << '\n';
    for (size_t i = 0; i < skeleton.GetBoneCount(); ++i) {
        const auto& bone = skeleton.GetBone(i);
        try {
            std::cout << "  [" << i << "] name="
                      << (bone.name.empty() ? std::string("<unnamed>") : bone.name)
                      << ", parent=" << bone.parentIndex << '\n';
        } catch (...) {
            std::cout << "  [" << i << "] name=<exception during output>, parent="
                      << bone.parentIndex << '\n';
        }

        std::cout << "    raw name bytes: ";
        for (unsigned char c : bone.name) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c) << ' ';
        }
        std::cout << std::dec << " (length=" << bone.name.size() << ")\n";
    }

    bool hasChild = false;
    int childIndex = -1;
    for (size_t i = 0; i < skeleton.GetBoneCount(); ++i) {
        if (skeleton.GetBone(i).name == "child") {
            childIndex = static_cast<int>(i);
            hasChild = true;
            break;
        }
    }

    std::cout << "Located child? " << (hasChild ? "yes" : "no")
              << ", index=" << childIndex << std::endl;

    if (hasChild) {
        const auto& child = skeleton.GetBone(static_cast<size_t>(childIndex));
        std::cout << "  (lookup) child bone index=" << childIndex
                  << ", parent=" << child.parentIndex << '\n';
    }

    std::cout << "Finished printing bones" << std::endl;

    SkeletonPose pose;
    pose.Resize(skeleton.GetBoneCount());

    if (hasChild) {
        Matrix4 childLocal = Matrix4::Identity();
        childLocal(0, 3) = 1.0f;
        pose.localTransforms[static_cast<size_t>(childIndex)] = childLocal;
    }

    Skeleton::MatrixArray worldMatrices;
    skeleton.EvaluateWorldTransforms(pose, worldMatrices);

    Skeleton::MatrixArray palette;
    skeleton.BuildSkinningPalette(pose, palette);

    if (hasChild) {
        std::cout << "\nWorld matrix of child bone:\n";
        PrintMatrix(worldMatrices[static_cast<size_t>(childIndex)]);

        std::cout << "\nSkinning palette matrix of child bone:\n";
        PrintMatrix(palette[static_cast<size_t>(childIndex)]);
    }

    std::cout << "\nIf the last column is (1, 0, 0, 1), the translation was applied correctly.\n";
    std::cout << "Test completed." << std::endl;
    return 0;
}
