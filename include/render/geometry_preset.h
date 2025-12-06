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

#include "render/mesh.h"
#include "render/types.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace Render {

class ResourceManager;

/**
 * @brief GeometryPreset 提供常用预定义几何体的创建与注册接口。
 *
 * - 支持在 ResourceManager 中注册/检索常用形状，避免重复生成
 * - 所有几何体默认包含法线、UV、切线/副切线，满足法线贴图需求
 * - 可在引擎初始化阶段批量调用 `RegisterDefaults` 预热 geometry 缓存
 */
class GeometryPreset {
public:
    using MeshFactory = std::function<Ref<Mesh>()>;

    struct PresetInfo {
        std::string name;
        MeshFactory factory;
    };

    /**
     * @brief 注册所有默认几何体至 ResourceManager
     *
     * 默认注册列表：
     *  - geometry::plane_xz
     *  - geometry::cube
     *  - geometry::sphere
     *  - geometry::cylinder
     *  - geometry::cone
     *  - geometry::torus
     *  - geometry::capsule
     *  - geometry::quad_xy / quad_xz
     *  - geometry::triangle
     *  - geometry::circle
     */
    static void RegisterDefaults(ResourceManager& resourceManager);

    /**
     * @brief 按名称获取或生成几何体网格
     *
     * @param resourceManager 资源管理器
     * @param name 预设名称（如 "geometry::cube"）
     * @return Mesh 引用，若未注册对应预设则返回 nullptr
     */
    static Ref<Mesh> GetMesh(ResourceManager& resourceManager, const std::string& name);

    /**
     * @brief 判断给定名称是否为已支持的预设
     */
    static bool HasPreset(const std::string& name);

private:
    static const std::unordered_map<std::string, PresetInfo>& GetPresetMap();
};

} // namespace Render


