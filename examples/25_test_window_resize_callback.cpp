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
/**
 * @file test_window_resize_callback.cpp
 * @brief 测试 OpenGLContext 窗口大小变化回调功能
 * 
 * 此示例演示如何使用新的窗口大小变化回调机制来自动更新相机宽高比
 * 和其他依赖窗口大小的组件。
 * 
 * @date 2025-10-31
 * @note 修复了警告4: OpenGLContext::SetWindowSize 直接修改成员变量的问题
 */

#include "render/opengl_context.h"
#include "render/camera.h"
#include "render/logger.h"
#include <iostream>

using namespace Render;

int main() {
    // 初始化日志系统
    Logger::GetInstance().SetLogLevel(LogLevel::Debug);
    Logger::GetInstance().SetLogToFile(true, "test_window_resize_callback.log");
    Logger::GetInstance().SetLogToConsole(true);
    
    // 创建 OpenGL 上下文
    OpenGLContext context;
    
    // 初始化上下文
    if (!context.Initialize("窗口大小变化回调测试", 1920, 1080)) {
        LOG_ERROR("无法初始化 OpenGL 上下文");
        return -1;
    }
    
    LOG_INFO("OpenGL 上下文已初始化");
    LOG_INFO("OpenGL 版本: " + context.GetGLVersion());
    LOG_INFO("GPU: " + context.GetGPUInfo());
    
    // 创建相机
    Camera camera;
    camera.SetPerspective(45.0f, 1920.0f / 1080.0f, 0.1f, 1000.0f);
    LOG_INFO("相机初始化完成，初始宽高比: " + std::to_string(1920.0f / 1080.0f));
    
    // 回调计数器
    int callbackCount = 0;
    
    // 注册回调 1: 更新相机宽高比
    context.AddResizeCallback([&camera](int width, int height) {
        float aspectRatio = static_cast<float>(width) / height;
        camera.SetAspectRatio(aspectRatio);
        LOG_INFO("回调 1: 相机宽高比已更新为 " + std::to_string(aspectRatio));
    });
    
    // 注册回调 2: 记录窗口大小变化
    context.AddResizeCallback([&callbackCount](int width, int height) {
        callbackCount++;
        LOG_INFO("回调 2: 窗口大小变化为 " + std::to_string(width) + "x" + 
                 std::to_string(height) + "，这是第 " + std::to_string(callbackCount) + " 次回调");
    });
    
    // 注册回调 3: 输出视口信息
    context.AddResizeCallback([](int width, int height) {
        LOG_INFO("回调 3: 视口已更新为 " + std::to_string(width) + "x" + std::to_string(height));
    });
    
    LOG_INFO("\n===== 测试窗口大小变化 =====\n");
    
    // 测试 1: 改变为 1280x720
    LOG_INFO("测试 1: 将窗口大小改为 1280x720");
    context.SetWindowSize(1280, 720);
    std::cout << "当前窗口大小: " << context.GetWidth() << "x" << context.GetHeight() << std::endl;
    std::cout << "回调调用次数: " << callbackCount << std::endl << std::endl;
    
    // 测试 2: 改变为 800x600
    LOG_INFO("测试 2: 将窗口大小改为 800x600");
    context.SetWindowSize(800, 600);
    std::cout << "当前窗口大小: " << context.GetWidth() << "x" << context.GetHeight() << std::endl;
    std::cout << "回调调用次数: " << callbackCount << std::endl << std::endl;
    
    // 测试 3: 改变为 1024x768
    LOG_INFO("测试 3: 将窗口大小改为 1024x768");
    context.SetWindowSize(1024, 768);
    std::cout << "当前窗口大小: " << context.GetWidth() << "x" << context.GetHeight() << std::endl;
    std::cout << "回调调用次数: " << callbackCount << std::endl << std::endl;
    
    // 测试 4: 清除所有回调
    LOG_INFO("测试 4: 清除所有回调");
    context.ClearResizeCallbacks();
    
    // 测试 5: 清除回调后再次改变窗口大小
    LOG_INFO("测试 5: 清除回调后将窗口大小改为 640x480");
    int oldCallbackCount = callbackCount;
    context.SetWindowSize(640, 480);
    std::cout << "当前窗口大小: " << context.GetWidth() << "x" << context.GetHeight() << std::endl;
    std::cout << "回调调用次数: " << callbackCount << " (应该与之前相同)" << std::endl << std::endl;
    
    if (callbackCount == oldCallbackCount) {
        LOG_INFO("✅ 测试通过：清除回调后不再触发");
        std::cout << "✅ 测试通过：清除回调后不再触发" << std::endl;
    } else {
        LOG_ERROR("❌ 测试失败：清除回调后仍然触发");
        std::cout << "❌ 测试失败：清除回调后仍然触发" << std::endl;
    }
    
    // 验证结果
    LOG_INFO("\n===== 测试结果 =====\n");
    std::cout << "\n===== 测试结果 =====" << std::endl;
    std::cout << "总回调次数: " << callbackCount << " (预期: 3)" << std::endl;
    std::cout << "最终窗口大小: " << context.GetWidth() << "x" << context.GetHeight() << std::endl;
    
    if (callbackCount == 3) {
        std::cout << "\n✅ 所有测试通过！窗口大小变化回调功能正常工作。" << std::endl;
        LOG_INFO("✅ 所有测试通过！窗口大小变化回调功能正常工作。");
    } else {
        std::cout << "\n❌ 测试失败！回调次数不正确。" << std::endl;
        LOG_ERROR("❌ 测试失败！回调次数不正确。");
    }
    
    // 清理
    context.Shutdown();
    LOG_INFO("OpenGL 上下文已关闭");
    
    return 0;
}

