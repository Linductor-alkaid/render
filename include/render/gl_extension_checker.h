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

#include "render/opengl_context.h"
#include <vector>
#include <string>

namespace Render {

/**
 * @brief OpenGL 扩展检查器
 * 
 * 提供便捷的扩展支持检查功能，用于确保硬件满足引擎要求
 */
class GLExtensionChecker {
public:
    /**
     * @brief 检查所有必需的OpenGL扩展是否支持
     * @param context OpenGL上下文
     * @return 所有必需扩展都支持返回 true
     */
    static bool CheckRequiredExtensions(OpenGLContext* context);
    
    /**
     * @brief 检查推荐的OpenGL扩展（可选，但能提升性能）
     * @param context OpenGL上下文
     * @return 所有推荐扩展都支持返回 true（警告级别，不影响运行）
     */
    static bool CheckRecommendedExtensions(OpenGLContext* context);
    
    /**
     * @brief 获取必需的扩展列表
     * @return 必需的扩展名称列表
     */
    static std::vector<std::string> GetRequiredExtensions();
    
    /**
     * @brief 获取推荐的扩展列表
     * @return 推荐的扩展名称列表
     */
    static std::vector<std::string> GetRecommendedExtensions();
};

} // namespace Render

