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

#include <string>
#include <vector>

namespace Render {

/**
 * @brief 文件工具类
 */
class FileUtils {
public:
    /**
     * @brief 读取整个文件内容
     * @param filepath 文件路径
     * @return 文件内容字符串，失败返回空字符串
     */
    static std::string ReadFile(const std::string& filepath);
    
    /**
     * @brief 读取二进制文件
     * @param filepath 文件路径
     * @return 文件内容字节数组
     */
    static std::vector<uint8_t> ReadBinaryFile(const std::string& filepath);
    
    /**
     * @brief 检查文件是否存在
     * @param filepath 文件路径
     * @return 存在返回 true，否则返回 false
     */
    static bool FileExists(const std::string& filepath);
    
    /**
     * @brief 获取文件扩展名
     * @param filepath 文件路径
     * @return 扩展名（不包含点）
     */
    static std::string GetFileExtension(const std::string& filepath);
    
    /**
     * @brief 获取文件名（不包含路径和扩展名）
     * @param filepath 文件路径
     * @return 文件名
     */
    static std::string GetFileName(const std::string& filepath);
    
    /**
     * @brief 获取文件所在目录
     * @param filepath 文件路径
     * @return 目录路径
     */
    static std::string GetDirectory(const std::string& filepath);
    
    /**
     * @brief 组合路径
     * @param base 基础路径
     * @param relative 相对路径
     * @return 组合后的路径
     */
    static std::string CombinePaths(const std::string& base, const std::string& relative);
};

} // namespace Render

