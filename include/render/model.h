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
#pragma once

#include "types.h"
#include "mesh.h"
#include "material.h"

#include <string>
#include <vector>
#include <shared_mutex>
#include <atomic>

namespace Render {

struct MeshExtraData;
struct MeshSkinningData;

/**
 * @brief 模型部件，封装单个网格与材质绑定关系。
 */
struct ModelPart {
    std::string name;
    Ref<Mesh> mesh;
    Ref<Material> material;
    Matrix4 localTransform = Matrix4::Identity();
    AABB localBounds;
    bool castShadows = true;
    bool receiveShadows = true;
    Ref<MeshExtraData> extraData;

    ModelPart() = default;

    ModelPart(std::string partName,
              Ref<Mesh> partMesh,
              Ref<Material> partMaterial,
              const Matrix4& transform = Matrix4::Identity(),
              const AABB& bounds = AABB(),
              bool casts = true,
              bool receives = true,
              Ref<MeshExtraData> extra = nullptr)
        : name(std::move(partName))
        , mesh(std::move(partMesh))
        , material(std::move(partMaterial))
        , localTransform(transform)
        , localBounds(bounds)
        , castShadows(casts)
        , receiveShadows(receives)
        , extraData(std::move(extra)) {}

    bool HasSkinning() const;
    const MeshSkinningData* GetSkinningData() const;
};

/**
 * @brief 模型统计信息。
 */
struct ModelStatistics {
    size_t meshCount = 0;
    size_t materialCount = 0;
    size_t vertexCount = 0;
    size_t indexCount = 0;

    bool IsEmpty() const { return meshCount == 0; }
};

/**
 * @brief Model 类表示包含多个子网格和材质的组合模型。
 *
 * 线程安全：所有公共方法均使用读写锁保护，可在多线程环境安全访问。
 */
class Model {
public:
    Model();
    explicit Model(std::string name);

    // 基础信息
    void SetName(const std::string& name);
    std::string GetName() const;

    void SetSourcePath(const std::string& path);
    std::string GetSourcePath() const;

    // 部件管理
    void SetParts(std::vector<ModelPart> parts);
    void AddPart(const ModelPart& part);
    void ClearParts();

    size_t GetPartCount() const;
    bool IsEmpty() const;

    template<typename Func>
    void AccessParts(Func&& func) const {
        std::shared_lock lock(m_mutex);
        func(m_parts);
    }

    template<typename Func>
    void ModifyParts(Func&& func) {
        std::unique_lock lock(m_mutex);
        func(m_parts);
        UpdateStatisticsLocked();
        m_boundsDirty.store(true, std::memory_order_relaxed);
    }

    // 包围盒与统计
    AABB GetBounds() const;
    void RecalculateBounds() const;

    ModelStatistics GetStatistics() const;

    bool AreAllMeshesUploaded() const;
    bool HasSkinning() const;

private:
    Vector3 TransformPoint(const Matrix4& matrix, const Vector3& point) const;
    AABB TransformBounds(const Matrix4& matrix, const AABB& bounds) const;
    void UpdateStatisticsLocked() const;

private:
    std::string m_name;
    std::string m_sourcePath;

    mutable std::shared_mutex m_mutex;
    std::vector<ModelPart> m_parts;

    mutable ModelStatistics m_statistics;
    mutable AABB m_bounds;
    mutable std::atomic<bool> m_boundsDirty;
};

using ModelPtr = std::shared_ptr<Model>;

} // namespace Render


