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
 * @file 03_geometry_shader_test.cpp
 * @brief 几何着色器和着色器缓存系统测试
 */

#include "render/renderer.h"
#include "render/shader.h"
#include "render/shader_cache.h"
#include "render/logger.h"
#include <SDL3/SDL.h>
#include <glad/glad.h>
#include <iostream>
#include <cmath>

using namespace Render;

// 点数据结构
struct PointVertex {
    float x, y, z;
    float r, g, b, a;
};

int main(int argc, char* argv[]) {
    // 设置日志
    Logger::GetInstance().SetLogToConsole(true);
    Logger::GetInstance().SetLogToFile(true);
    Logger::GetInstance().SetLogLevel(LogLevel::Info);
    
    LOG_INFO("Log file: " + Logger::GetInstance().GetCurrentLogFile());
    
    LOG_INFO("========================================");
    LOG_INFO("Geometry Shader & Shader Cache Test");
    LOG_INFO("========================================");
    
    // 创建渲染器
    Renderer* renderer = Renderer::Create();
    if (!renderer) {
        LOG_ERROR("Failed to create renderer");
        return -1;
    }
    
    // 初始化
    if (!renderer->Initialize("03 - Geometry Shader Test", 1280, 720)) {
        LOG_ERROR("Failed to initialize renderer");
        Renderer::Destroy(renderer);
        return -1;
    }
    
    renderer->SetVSync(true);
    
    // 使用着色器缓存管理器加载着色器
    LOG_INFO("========================================");
    LOG_INFO("Loading shaders via ShaderCache...");
    LOG_INFO("========================================");
    
    ShaderCache& shaderCache = ShaderCache::GetInstance();
    
    // 预编译着色器列表
    std::vector<std::tuple<std::string, std::string, std::string, std::string>> shaderList = {
        {"SolidColor", "shaders/solid_color.vert", "shaders/solid_color.frag", ""},
        {"Basic", "shaders/basic.vert", "shaders/basic.frag", ""},
        {"PointToQuad", "shaders/point_to_quad.vert", "shaders/point_to_quad.frag", "shaders/point_to_quad.geom"}
    };
    
    size_t loadedCount = shaderCache.PrecompileShaders(shaderList);
    LOG_INFO("Loaded " + std::to_string(loadedCount) + "/" + std::to_string(shaderList.size()) + " shaders");
    
    if (loadedCount < shaderList.size()) {
        LOG_WARNING("Some shaders failed to load, continuing with loaded shaders...");
    }
    
    // 打印缓存统计
    shaderCache.PrintStatistics();
    
    // 获取着色器
    auto pointShader = shaderCache.GetShader("PointToQuad");
    if (!pointShader) {
        LOG_ERROR("Failed to get PointToQuad shader");
        Renderer::Destroy(renderer);
        return -1;
    }
    
    // 创建点几何数据
    LOG_INFO("========================================");
    LOG_INFO("Creating point geometry...");
    LOG_INFO("========================================");
    
    // 创建多个彩色点
    std::vector<PointVertex> points;
    const int numPoints = 20;
    for (int i = 0; i < numPoints; ++i) {
        float angle = (i / static_cast<float>(numPoints)) * 2.0f * 3.14159f;
        float radius = 0.5f + 0.3f * std::sin(i * 0.5f);
        
        PointVertex point;
        point.x = radius * std::cos(angle);
        point.y = radius * std::sin(angle);
        point.z = 0.0f;
        
        // 彩虹色
        point.r = 0.5f + 0.5f * std::sin(angle);
        point.g = 0.5f + 0.5f * std::sin(angle + 2.094f);
        point.b = 0.5f + 0.5f * std::sin(angle + 4.189f);
        point.a = 1.0f;
        
        points.push_back(point);
    }
    
    // 创建 VAO 和 VBO
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(PointVertex), points.data(), GL_STATIC_DRAW);
    
    // 位置属性
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PointVertex), (void*)0);
    
    // 颜色属性
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(PointVertex), (void*)(3 * sizeof(float)));
    
    glBindVertexArray(0);
    
    LOG_INFO("Created " + std::to_string(points.size()) + " points");
    
    // 设置着色器 uniform
    pointShader->Use();
    
    Matrix4 model = Matrix4::Identity();
    Matrix4 view = Matrix4::Identity();
    Matrix4 projection = Matrix4::Identity();
    
    pointShader->GetUniformManager()->SetMatrix4("model", model);
    pointShader->GetUniformManager()->SetMatrix4("view", view);
    pointShader->GetUniformManager()->SetMatrix4("projection", projection);
    pointShader->GetUniformManager()->SetFloat("quadSize", 0.05f);
    
    pointShader->Unuse();
    
    LOG_INFO("========================================");
    LOG_INFO("Controls:");
    LOG_INFO("  ESC - Exit");
    LOG_INFO("  R - Reload all shaders");
    LOG_INFO("  + - Increase quad size");
    LOG_INFO("  - - Decrease quad size");
    LOG_INFO("  S - Print shader cache statistics");
    LOG_INFO("========================================");
    
    // 主循环
    bool running = true;
    uint32_t frameCount = 0;
    float rotationAngle = 0.0f;
    float quadSize = 0.05f;
    
    while (running) {
        // 处理事件
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            
            if (event.type == SDL_EVENT_KEY_DOWN) {
                switch (event.key.key) {
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                        
                    case SDLK_R:
                        LOG_INFO("Reloading all shaders...");
                        shaderCache.ReloadAll();
                        break;
                        
                    case SDLK_EQUALS: // + key
                    case SDLK_KP_PLUS:
                        quadSize += 0.01f;
                        if (quadSize > 0.2f) quadSize = 0.2f;
                        LOG_INFO("Quad size: " + std::to_string(quadSize));
                        break;
                        
                    case SDLK_MINUS:
                    case SDLK_KP_MINUS:
                        quadSize -= 0.01f;
                        if (quadSize < 0.01f) quadSize = 0.01f;
                        LOG_INFO("Quad size: " + std::to_string(quadSize));
                        break;
                        
                    case SDLK_S:
                        shaderCache.PrintStatistics();
                        break;
                }
            }
        }
        
        // 更新
        float deltaTime = renderer->GetDeltaTime();
        rotationAngle += deltaTime * 0.5f; // 旋转速度
        
        // 更新模型矩阵（旋转）
        float cosAngle = std::cos(rotationAngle);
        float sinAngle = std::sin(rotationAngle);
        
        Matrix4 rotation;
        rotation << cosAngle, -sinAngle, 0, 0,
                    sinAngle,  cosAngle, 0, 0,
                    0,         0,        1, 0,
                    0,         0,        0, 1;
        
        // 渲染
        renderer->BeginFrame();
        renderer->SetClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        renderer->Clear();
        
        // 使用几何着色器渲染点
        if (pointShader && pointShader->IsValid()) {
            pointShader->Use();
            pointShader->GetUniformManager()->SetMatrix4("model", rotation);
            pointShader->GetUniformManager()->SetFloat("quadSize", quadSize);
            
            glBindVertexArray(VAO);
            glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(points.size()));
            glBindVertexArray(0);
            
            pointShader->Unuse();
        }
        
        renderer->EndFrame();
        renderer->Present();
        
        frameCount++;
        
        // 更新窗口标题显示 FPS
        static float fpsTimer = 0.0f;
        fpsTimer += deltaTime;
        if (fpsTimer >= 1.0f) {
            float fps = renderer->GetFPS();
            std::string title = "03 - Geometry Shader Test | FPS: " + 
                              std::to_string(static_cast<int>(fps)) +
                              " | Quad Size: " + std::to_string(quadSize);
            renderer->SetWindowTitle(title);
            fpsTimer = 0.0f;
        }
    }
    
    LOG_INFO("Total frames rendered: " + std::to_string(frameCount));
    
    // 清理
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    
    // 清空着色器缓存
    shaderCache.Clear();
    
    Renderer::Destroy(renderer);
    
    LOG_INFO("Exiting...");
    return 0;
}

