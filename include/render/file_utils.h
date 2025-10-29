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

