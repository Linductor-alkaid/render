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

