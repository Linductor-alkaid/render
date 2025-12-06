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

#include "render/render_state.h"
#include <cstdint>
#include <cstddef>
#include <optional>

namespace Render {

class Material;

enum MaterialPipelineFlags : uint32_t {
    MaterialPipelineFlags_None          = 0,
    MaterialPipelineFlags_CastShadow    = 1u << 0,
    MaterialPipelineFlags_ReceiveShadow = 1u << 1,
    MaterialPipelineFlags_ScreenSpace   = 1u << 2,
    MaterialPipelineFlags_Instanced     = 1u << 3
};

/**
 * @brief 用于材质排序与批处理的键
 *
 * 保存与渲染状态相关的核心信息，便于在 Renderer 层稳定地聚合 Renderable。
 */
struct MaterialSortKey {
    uint32_t materialID = 0;
    uint32_t shaderID = 0;
    BlendMode blendMode = BlendMode::None;
    CullFace cullFace = CullFace::Back;
    bool depthTest = true;
    bool depthWrite = true;
    DepthFunc depthFunc = DepthFunc::Less;  // 新增：深度测试函数
    uint32_t overrideHash = 0;
    uint32_t pipelineFlags = 0;

    bool operator==(const MaterialSortKey& other) const noexcept;
    bool operator!=(const MaterialSortKey& other) const noexcept {
        return !(*this == other);
    }
};

/**
 * @brief 将材质与覆盖参数转换为排序键
 * @param material 材质指针（允许为 nullptr）
 * @param overrideHash 材质覆盖哈希值（不同覆盖应生成不同键）
 * @param pipelineFlags 额外的管线标记位（如阴影、实例化等）
 * @param depthFuncOverride 深度函数覆盖（从层级状态获取，如果有）
 */
MaterialSortKey BuildMaterialSortKey(const Material* material,
                                     uint32_t overrideHash = 0,
                                     uint32_t pipelineFlags = 0,
                                     std::optional<DepthFunc> depthFuncOverride = std::nullopt) noexcept;

struct MaterialSortKeyHasher {
    std::size_t operator()(const MaterialSortKey& key) const noexcept;
};

struct MaterialSortKeyLess {
    bool operator()(const MaterialSortKey& lhs, const MaterialSortKey& rhs) const noexcept;
};

} // namespace Render

