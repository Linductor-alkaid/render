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

#include "render/types.h"
#include "render/gl_thread_checker.h"
#include <SDL3/SDL.h>
#include <string>
#include <functional>
#include <vector>
#include <mutex>

namespace Render {

/**
 * @brief OpenGL 上下文配置
 */
struct OpenGLConfig {
    int majorVersion = 4;
    int minorVersion = 5;
    bool coreProfile = true;
    bool debugContext = true;
    int depthBits = 24;
    int stencilBits = 8;
    int msaaSamples = 4;
    bool doubleBuffer = true;
};

/**
 * @brief 窗口大小变化回调函数类型
 * @param width 新的窗口宽度
 * @param height 新的窗口高度
 */
using WindowResizeCallback = std::function<void(int width, int height)>;

/**
 * @brief OpenGL 上下文管理类
 * 
 * 负责创建和管理 OpenGL 上下文、窗口以及扩展检测
 */
class OpenGLContext {
public:
    OpenGLContext();
    ~OpenGLContext();
    
    /**
     * @brief 初始化 OpenGL 上下文
     * @param title 窗口标题
     * @param width 窗口宽度
     * @param height 窗口高度
     * @param config OpenGL 配置
     * @return 成功返回 true，失败返回 false
     */
    bool Initialize(const std::string& title, 
                   int width, 
                   int height, 
                   const OpenGLConfig& config = OpenGLConfig());
    
    /**
     * @brief 关闭并清理上下文
     */
    void Shutdown();
    
    /**
     * @brief 交换缓冲区（呈现）
     */
    void SwapBuffers();
    
    /**
     * @brief 设置 VSync
     * @param enable 是否启用 VSync
     */
    void SetVSync(bool enable);
    
    /**
     * @brief 设置窗口标题
     * @param title 窗口标题
     */
    void SetWindowTitle(const std::string& title);
    
    /**
     * @brief 设置窗口大小
     * @param width 宽度
     * @param height 高度
     */
    void SetWindowSize(int width, int height);
    
    /**
     * @brief 设置全屏模式
     * @param fullscreen 是否全屏
     */
    void SetFullscreen(bool fullscreen);
    
    /**
     * @brief 获取窗口宽度
     */
    int GetWidth() const { return m_width; }
    
    /**
     * @brief 获取窗口高度
     */
    int GetHeight() const { return m_height; }
    
    /**
     * @brief 获取 SDL 窗口指针
     */
    SDL_Window* GetWindow() const { return m_window; }
    
    /**
     * @brief 获取 OpenGL 上下文
     */
    SDL_GLContext GetGLContext() const { return m_glContext; }
    
    /**
     * @brief 检查是否已初始化
     */
    bool IsInitialized() const { return m_initialized; }
    
    /**
     * @brief 获取 OpenGL 版本字符串
     */
    std::string GetGLVersion() const;
    
    /**
     * @brief 获取 GPU 信息
     */
    std::string GetGPUInfo() const;
    
    /**
     * @brief 检查 OpenGL 扩展是否支持
     * @param extension 扩展名称
     * @return 支持返回 true，否则返回 false
     */
    bool IsExtensionSupported(const std::string& extension) const;
    
    /**
     * @brief 添加窗口大小变化回调
     * @param callback 回调函数
     * @note 当窗口大小改变时，所有已注册的回调将被调用
     */
    void AddResizeCallback(WindowResizeCallback callback);
    
    /**
     * @brief 清除所有窗口大小变化回调
     */
    void ClearResizeCallbacks();
    
private:
    bool CreateWindow(const std::string& title, int width, int height);
    bool CreateGLContext(const OpenGLConfig& config);
    bool InitializeGLAD();
    void LogGLInfo();
    void NotifyResizeCallbacks(int width, int height);
    
    SDL_Window* m_window;
    SDL_GLContext m_glContext;
    int m_width;
    int m_height;
    bool m_initialized;
    bool m_vsyncEnabled;
    std::vector<WindowResizeCallback> m_resizeCallbacks;
    mutable std::mutex m_callbackMutex;
};

} // namespace Render

