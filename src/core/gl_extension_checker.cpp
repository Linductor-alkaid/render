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
#include "render/gl_extension_checker.h"
#include "render/logger.h"

namespace Render {

std::vector<std::string> GLExtensionChecker::GetRequiredExtensions() {
    // 必需的扩展列表
    // 注意：根据引擎的实际使用情况添加必需的扩展
    return {
        // 当前引擎使用 OpenGL 4.5 核心功能，大部分功能不依赖扩展
        // 如果需要特定扩展，请在这里添加
        
        // 示例（如果引擎使用了这些功能）：
        // "GL_ARB_direct_state_access",
        // "GL_ARB_multi_draw_indirect",
        // "GL_ARB_shader_storage_buffer_object",
    };
}

std::vector<std::string> GLExtensionChecker::GetRecommendedExtensions() {
    // 推荐的扩展列表（不是必需，但会提升性能）
    return {
        // 示例：
        // "GL_ARB_bindless_texture",       // 无绑定纹理（大幅提升纹理访问性能）
        // "GL_NV_shader_buffer_load",      // NVIDIA GPU缓冲区直接加载
        // "GL_ARB_buffer_storage",         // 持久映射缓冲区
    };
}

bool GLExtensionChecker::CheckRequiredExtensions(OpenGLContext* context) {
    if (!context || !context->IsInitialized()) {
        Logger::GetInstance().Error("[GLExtensionChecker] Context not initialized");
        return false;
    }
    
    auto requiredExtensions = GetRequiredExtensions();
    
    // 如果没有必需的扩展，直接返回成功
    if (requiredExtensions.empty()) {
        Logger::GetInstance().Info("[GLExtensionChecker] No required extensions specified");
        return true;
    }
    
    bool allRequired = true;
    
    Logger::GetInstance().Info("[GLExtensionChecker] === Checking Required Extensions ===");
    for (const auto& ext : requiredExtensions) {
        if (context->IsExtensionSupported(ext)) {
            Logger::GetInstance().InfoFormat("[GLExtensionChecker]   ✓ %s", ext.c_str());
        } else {
            Logger::GetInstance().ErrorFormat("[GLExtensionChecker]   ✗ %s (REQUIRED)", ext.c_str());
            allRequired = false;
        }
    }
    
    if (!allRequired) {
        Logger::GetInstance().Error("[GLExtensionChecker] Missing required OpenGL extensions! Application may not work correctly.");
    } else {
        Logger::GetInstance().Info("[GLExtensionChecker] All required extensions are supported");
    }
    
    return allRequired;
}

bool GLExtensionChecker::CheckRecommendedExtensions(OpenGLContext* context) {
    if (!context || !context->IsInitialized()) {
        Logger::GetInstance().Warning("[GLExtensionChecker] Context not initialized, skipping recommended extension check");
        return false;
    }
    
    auto recommendedExtensions = GetRecommendedExtensions();
    
    // 如果没有推荐的扩展，直接返回成功
    if (recommendedExtensions.empty()) {
        Logger::GetInstance().Debug("[GLExtensionChecker] No recommended extensions specified");
        return true;
    }
    
    bool allRecommended = true;
    
    Logger::GetInstance().Info("[GLExtensionChecker] === Checking Recommended Extensions ===");
    for (const auto& ext : recommendedExtensions) {
        if (context->IsExtensionSupported(ext)) {
            Logger::GetInstance().InfoFormat("[GLExtensionChecker]   ✓ %s", ext.c_str());
        } else {
            Logger::GetInstance().WarningFormat("[GLExtensionChecker]   ✗ %s (Recommended but not required)", ext.c_str());
            allRecommended = false;
        }
    }
    
    if (!allRecommended) {
        Logger::GetInstance().Info("[GLExtensionChecker] Some recommended extensions are not supported. Performance may be suboptimal.");
    } else {
        Logger::GetInstance().Info("[GLExtensionChecker] All recommended extensions are supported");
    }
    
    return allRecommended;
}

} // namespace Render

