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

#include "render/sprite/sprite_atlas.h"
#include "render/ecs/components.h"
#include <optional>

namespace Render {

struct SpriteAtlasImportResult {
    SpriteAtlasPtr atlas;
    SpriteSheet spriteSheet;
    ECS::SpriteAnimationComponent animationComponent;
    std::string defaultAnimation;
    bool autoPlay = false;
};

/**
 * @brief 精灵图集导入器，解析 JSON 图集描述并生成渲染所需的数据
 */
class SpriteAtlasImporter {
public:
    /**
     * @brief 从 JSON 文件加载图集
     * @param filePath 图集描述文件路径
     * @param atlasName 可选的图集名称（为空则使用文件或 meta 中的名称）
     * @param error 输出错误信息
     * @return 成功时返回导入结果，失败返回 std::nullopt
     */
    static std::optional<SpriteAtlasImportResult> LoadFromFile(
        const std::string& filePath,
        const std::string& atlasName,
        std::string& error);

    /**
     * @brief 加载图集并注册到 ResourceManager
     * @param filePath 图集描述文件路径
     * @param atlasName 图集名称
     * @param error 输出错误信息
     * @return 注册成功返回 true
     */
    static bool LoadAndRegister(
        const std::string& filePath,
        const std::string& atlasName,
        std::string& error);
};

} // namespace Render


