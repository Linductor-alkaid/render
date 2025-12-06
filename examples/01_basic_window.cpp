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
 * @file 01_basic_window.cpp
 * @brief 基础窗口测试 - 创建窗口并清屏
 */

#include "render/renderer.h"
#include "render/logger.h"
#include <SDL3/SDL.h>
#include <iostream>

using namespace Render;

int main(int argc, char* argv[]) {
    // 设置日志
    Logger::GetInstance().SetLogToConsole(true);
    Logger::GetInstance().SetLogToFile(true); // 自动生成时间戳命名的日志文件
    Logger::GetInstance().SetLogLevel(LogLevel::Info);
    
    LOG_INFO("========================================");
    LOG_INFO("Basic Window Example");
    LOG_INFO("========================================");
    
    // 创建渲染器
    Renderer* renderer = Renderer::Create();
    if (!renderer) {
        LOG_ERROR("Failed to create renderer");
        return -1;
    }
    
    // 初始化
    if (!renderer->Initialize("01 - Basic Window", 1280, 720)) {
        LOG_ERROR("Failed to initialize renderer");
        Renderer::Destroy(renderer);
        return -1;
    }
    
    // 设置 VSync
    renderer->SetVSync(true);
    
    // 设置清屏颜色为深蓝色
    renderer->SetClearColor(0.2f, 0.3f, 0.4f, 1.0f);
    
    LOG_INFO("Renderer initialized successfully");
    LOG_INFO("Press ESC to exit");
    
    // 主循环
    bool running = true;
    uint32_t frameCount = 0;
    float colorTimer = 0.0f;
    
    while (running) {
        // 处理事件
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
                
                // F11 切换全屏
                if (event.key.key == SDLK_F11) {
                    static bool fullscreen = false;
                    fullscreen = !fullscreen;
                    renderer->SetFullscreen(fullscreen);
                    LOG_INFO(fullscreen ? "Fullscreen enabled" : "Fullscreen disabled");
                }
            }
        }
        
        // 更新
        float deltaTime = renderer->GetDeltaTime();
        colorTimer += deltaTime;
        
        // 动态改变清屏颜色
        float r = 0.2f + 0.3f * std::sin(colorTimer * 0.5f);
        float g = 0.3f + 0.3f * std::sin(colorTimer * 0.7f);
        float b = 0.4f + 0.3f * std::sin(colorTimer * 0.3f);
        renderer->SetClearColor(r, g, b, 1.0f);
        
        // 渲染
        renderer->BeginFrame();
        renderer->Clear();
        
        // 这里将来会添加实际的渲染内容
        
        renderer->EndFrame();
        renderer->Present();
        
        frameCount++;
        
        // 每秒输出一次 FPS
        static float fpsTimer = 0.0f;
        fpsTimer += deltaTime;
        if (fpsTimer >= 1.0f) {
            float fps = renderer->GetFPS();
            std::string title = "01 - Basic Window | FPS: " + std::to_string(static_cast<int>(fps));
            renderer->SetWindowTitle(title);
            
            LOG_INFO("FPS: " + std::to_string(fps) + 
                    " | Frame Time: " + std::to_string(renderer->GetStats().frameTime) + "ms");
            
            fpsTimer = 0.0f;
        }
    }
    
    LOG_INFO("Total frames rendered: " + std::to_string(frameCount));
    
    // 清理
    Renderer::Destroy(renderer);
    
    LOG_INFO("Exiting...");
    return 0;
}

