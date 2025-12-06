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
#include "render/model.h"

#include "render/logger.h"
#include "render/mesh_loader.h"

#include <unordered_set>
#include <array>
#include <limits>
#include <cmath>

namespace Render {

namespace {
constexpr float kDefaultBoundsEpsilon = 1e-5f;
}

bool ModelPart::HasSkinning() const {
    return extraData && extraData->skinning.HasBones();
}

const MeshSkinningData* ModelPart::GetSkinningData() const {
    if (!extraData || !extraData->skinning.HasBones()) {
        return nullptr;
    }
    return &extraData->skinning;
}

Model::Model()
    : m_bounds()
    , m_boundsDirty(true) {
    m_statistics = {};
}

Model::Model(std::string name)
    : m_name(std::move(name))
    , m_bounds()
    , m_boundsDirty(true) {
    m_statistics = {};
}

void Model::SetName(const std::string& name) {
    std::unique_lock lock(m_mutex);
    m_name = name;
}

std::string Model::GetName() const {
    std::shared_lock lock(m_mutex);
    return m_name;
}

void Model::SetSourcePath(const std::string& path) {
    std::unique_lock lock(m_mutex);
    m_sourcePath = path;
}

std::string Model::GetSourcePath() const {
    std::shared_lock lock(m_mutex);
    return m_sourcePath;
}

void Model::SetParts(std::vector<ModelPart> parts) {
    std::unique_lock lock(m_mutex);
    m_parts = std::move(parts);

    for (auto& part : m_parts) {
        if (part.mesh && part.localBounds.min.isApprox(part.localBounds.max, kDefaultBoundsEpsilon)) {
            part.localBounds = part.mesh->CalculateBounds();
        }
    }

    UpdateStatisticsLocked();
    m_boundsDirty.store(true, std::memory_order_relaxed);
}

void Model::AddPart(const ModelPart& part) {
    std::unique_lock lock(m_mutex);
    m_parts.push_back(part);

    auto& stored = m_parts.back();
    if (stored.mesh && stored.localBounds.min.isApprox(stored.localBounds.max, kDefaultBoundsEpsilon)) {
        stored.localBounds = stored.mesh->CalculateBounds();
    }

    UpdateStatisticsLocked();
    m_boundsDirty.store(true, std::memory_order_relaxed);
}

void Model::ClearParts() {
    std::unique_lock lock(m_mutex);
    m_parts.clear();
    UpdateStatisticsLocked();
    m_bounds = AABB();
    m_boundsDirty.store(false, std::memory_order_relaxed);
}

size_t Model::GetPartCount() const {
    std::shared_lock lock(m_mutex);
    return m_parts.size();
}

bool Model::IsEmpty() const {
    return GetPartCount() == 0;
}

AABB Model::GetBounds() const {
    if (m_boundsDirty.load(std::memory_order_acquire)) {
        RecalculateBounds();
    }

    std::shared_lock lock(m_mutex);
    return m_bounds;
}

void Model::RecalculateBounds() const {
    std::unique_lock lock(m_mutex);

    bool hasValidBounds = false;
    Vector3 minPoint = Vector3::Zero();
    Vector3 maxPoint = Vector3::Zero();

    for (const auto& part : m_parts) {
        if (!part.mesh) {
            continue;
        }

        AABB localBounds = part.localBounds;
        if (localBounds.min.isApprox(localBounds.max, kDefaultBoundsEpsilon)) {
            localBounds = part.mesh->CalculateBounds();
        }

        Matrix4 transform = part.localTransform;
        AABB worldBounds = TransformBounds(transform, localBounds);

        if (!hasValidBounds) {
            minPoint = worldBounds.min;
            maxPoint = worldBounds.max;
            hasValidBounds = true;
        } else {
            minPoint = minPoint.cwiseMin(worldBounds.min);
            maxPoint = maxPoint.cwiseMax(worldBounds.max);
        }
    }

    if (hasValidBounds) {
        m_bounds = AABB(minPoint, maxPoint);
    } else {
        m_bounds = AABB();
    }

    m_boundsDirty.store(false, std::memory_order_release);
}

ModelStatistics Model::GetStatistics() const {
    std::shared_lock lock(m_mutex);
    return m_statistics;
}

bool Model::AreAllMeshesUploaded() const {
    std::shared_lock lock(m_mutex);
    if (m_parts.empty()) {
        return false;
    }

    for (const auto& part : m_parts) {
        if (!part.mesh || !part.mesh->IsUploaded()) {
            return false;
        }
    }
    return true;
}

bool Model::HasSkinning() const {
    std::shared_lock lock(m_mutex);
    for (const auto& part : m_parts) {
        if (part.HasSkinning()) {
            return true;
        }
    }
    return false;
}

Vector3 Model::TransformPoint(const Matrix4& matrix, const Vector3& point) const {
    Vector4 homogenous(point.x(), point.y(), point.z(), 1.0f);
    Vector4 transformed = matrix * homogenous;

    if (std::abs(transformed.w()) > std::numeric_limits<float>::epsilon()) {
        return transformed.head<3>() / transformed.w();
    }
    return transformed.head<3>();
}

AABB Model::TransformBounds(const Matrix4& matrix, const AABB& bounds) const {
    std::array<Vector3, 8> corners = {
        Vector3(bounds.min.x(), bounds.min.y(), bounds.min.z()),
        Vector3(bounds.max.x(), bounds.min.y(), bounds.min.z()),
        Vector3(bounds.min.x(), bounds.max.y(), bounds.min.z()),
        Vector3(bounds.max.x(), bounds.max.y(), bounds.min.z()),
        Vector3(bounds.min.x(), bounds.min.y(), bounds.max.z()),
        Vector3(bounds.max.x(), bounds.min.y(), bounds.max.z()),
        Vector3(bounds.min.x(), bounds.max.y(), bounds.max.z()),
        Vector3(bounds.max.x(), bounds.max.y(), bounds.max.z())
    };

    Vector3 transformedMin = TransformPoint(matrix, corners[0]);
    Vector3 transformedMax = transformedMin;

    for (size_t i = 1; i < corners.size(); ++i) {
        Vector3 transformed = TransformPoint(matrix, corners[i]);
        transformedMin = transformedMin.cwiseMin(transformed);
        transformedMax = transformedMax.cwiseMax(transformed);
    }

    return AABB(transformedMin, transformedMax);
}

void Model::UpdateStatisticsLocked() const {
    ModelStatistics stats;
    std::unordered_set<const Material*> uniqueMaterials;

    for (const auto& part : m_parts) {
        if (part.mesh) {
            ++stats.meshCount;
            stats.vertexCount += part.mesh->GetVertexCount();
            stats.indexCount += part.mesh->GetIndexCount();
        }

        if (part.material) {
            uniqueMaterials.insert(part.material.get());
        }
    }

    stats.materialCount = uniqueMaterials.size();
    m_statistics = stats;
}

} // namespace Render


