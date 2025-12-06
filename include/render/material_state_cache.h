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

#include <cstdint>

namespace Render {

class Material;
class RenderState;

/**
 * @brief 材质绑定状态缓存
 *
 * 线程局部缓存，用于跳过连续的重复 Material::Bind 调用，减少 uniform 与状态设置开销。
 */
class MaterialStateCache {
public:
    /// 获取线程局部缓存实例
    static MaterialStateCache& Get();

    /// 重置缓存（通常在每帧开始时调用）
    void Reset();

    /// 判断当前材质是否需要重新绑定
    bool ShouldBind(const Material* material, RenderState* renderState) const;

    /// 在绑定后更新缓存
    void OnBind(const Material* material, RenderState* renderState);

private:
    const Material* m_lastMaterial = nullptr;
    RenderState* m_lastRenderState = nullptr;
};

} // namespace Render


