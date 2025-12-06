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
 * @file 02_shader_test.cpp
 * @brief 着色器系统测试 - 加载和使用着色器
 */

#include "render/renderer.h"
#include "render/shader.h"
#include "render/logger.h"
#include <SDL3/SDL.h>
#include <iostream>

using namespace Render;

int main(int argc, char* argv[]) {
    // 设置日志
    Logger::GetInstance().SetLogToConsole(true);
    Logger::GetInstance().SetLogToFile(true); // 自动生成时间戳命名的日志文件
    Logger::GetInstance().SetLogLevel(LogLevel::Info);
    
    LOG_INFO("Log file: " + Logger::GetInstance().GetCurrentLogFile());
    
    LOG_INFO("========================================");
    LOG_INFO("Shader System Test");
    LOG_INFO("========================================");
    
    // 创建渲染器
    Renderer* renderer = Renderer::Create();
    if (!renderer) {
        LOG_ERROR("Failed to create renderer");
        return -1;
    }
    
    // 初始化
    if (!renderer->Initialize("02 - Shader Test", 1280, 720)) {
        LOG_ERROR("Failed to initialize renderer");
        Renderer::Destroy(renderer);
        return -1;
    }
    
    renderer->SetVSync(true);
    
    // 加载着色器
    LOG_INFO("========================================");
    LOG_INFO("Loading shaders...");
    LOG_INFO("========================================");
    
    Shader solidColorShader;
    if (!solidColorShader.LoadFromFile("shaders/solid_color.vert", "shaders/solid_color.frag")) {
        LOG_ERROR("Failed to load solid_color shader");
        Renderer::Destroy(renderer);
        return -1;
    }
    solidColorShader.SetName("SolidColor");
    
    Shader basicShader;
    if (!basicShader.LoadFromFile("shaders/basic.vert", "shaders/basic.frag")) {
        LOG_ERROR("Failed to load basic shader");
        Renderer::Destroy(renderer);
        return -1;
    }
    basicShader.SetName("Basic");
    
    LOG_INFO("All shaders loaded successfully!");
    
    // 打印 uniform 信息
    LOG_INFO("========================================");
    LOG_INFO("Solid Color Shader Uniforms:");
    solidColorShader.GetUniformManager()->PrintUniformInfo();
    
    LOG_INFO("========================================");
    LOG_INFO("Basic Shader Uniforms:");
    basicShader.GetUniformManager()->PrintUniformInfo();
    LOG_INFO("========================================");
    
    // 测试 uniform 设置
    solidColorShader.Use();
    
    // 设置矩阵
    Matrix4 identity = Matrix4::Identity();
    solidColorShader.GetUniformManager()->SetMatrix4("model", identity);
    solidColorShader.GetUniformManager()->SetMatrix4("view", identity);
    solidColorShader.GetUniformManager()->SetMatrix4("projection", identity);
    
    // 设置颜色
    Color redColor(1.0f, 0.0f, 0.0f, 1.0f);
    solidColorShader.GetUniformManager()->SetColor("color", redColor);
    
    LOG_INFO("Uniforms set successfully!");
    
    solidColorShader.Unuse();
    
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
                
                // R 键重载着色器
                if (event.key.key == SDLK_R) {
                    LOG_INFO("Reloading shaders...");
                    if (solidColorShader.Reload()) {
                        LOG_INFO("Solid color shader reloaded successfully");
                    }
                    if (basicShader.Reload()) {
                        LOG_INFO("Basic shader reloaded successfully");
                    }
                }
            }
        }
        
        // 更新
        float deltaTime = renderer->GetDeltaTime();
        colorTimer += deltaTime;
        
        // 动态改变清屏颜色
        float r = 0.1f + 0.1f * std::sin(colorTimer * 0.5f);
        float g = 0.1f + 0.1f * std::sin(colorTimer * 0.7f);
        float b = 0.1f + 0.1f * std::sin(colorTimer * 0.3f);
        renderer->SetClearColor(r, g, b, 1.0f);
        
        // 渲染
        renderer->BeginFrame();
        renderer->Clear();
        
        // 使用着色器（但目前没有几何体渲染）
        solidColorShader.Use();
        // TODO: 这里将来会渲染几何体
        solidColorShader.Unuse();
        
        renderer->EndFrame();
        renderer->Present();
        
        frameCount++;
        
        // 每秒输出一次 FPS
        static float fpsTimer = 0.0f;
        fpsTimer += deltaTime;
        if (fpsTimer >= 1.0f) {
            float fps = renderer->GetFPS();
            std::string title = "02 - Shader Test | FPS: " + 
                              std::to_string(static_cast<int>(fps));
            renderer->SetWindowTitle(title);
            fpsTimer = 0.0f;
        }
    }
    
    LOG_INFO("Total frames rendered: " + std::to_string(frameCount));
    
    // 清理
    Renderer::Destroy(renderer);
    
    LOG_INFO("Exiting...");
    return 0;
}

