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
 * @file 05_texture_test.cpp
 * @brief 纹理加载和渲染测试示例
 * 
 * 演示如何使用 Texture 类加载图片并渲染带纹理的矩形
 */

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <glad/glad.h>
#include <iostream>
#include <memory>

#include "render/renderer.h"
#include "render/opengl_context.h"
#include "render/shader.h"
#include "render/texture.h"
#include "render/texture_loader.h"
#include "render/logger.h"

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;

// 带纹理的顶点数据（位置 + 纹理坐标）
const float vertices[] = {
    // 位置              // 纹理坐标
    -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,  // 左下
     0.5f, -0.5f, 0.0f,  1.0f, 0.0f,  // 右下
     0.5f,  0.5f, 0.0f,  1.0f, 1.0f,  // 右上
    -0.5f,  0.5f, 0.0f,  0.0f, 1.0f   // 左上
};

const unsigned int indices[] = {
    0, 1, 2,  // 第一个三角形
    2, 3, 0   // 第二个三角形
};

int main(int argc, char* argv[]) {
    // 初始化日志系统并启用文件输出
    Render::Logger::GetInstance().SetLogToFile(true);  // 自动生成带时间戳的日志文件
    Render::Logger::GetInstance().Info("=== 纹理加载测试 ===");
    Render::Logger::GetInstance().Info("日志文件: " + Render::Logger::GetInstance().GetCurrentLogFile());

    // 初始化 SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        Render::Logger::GetInstance().Error("初始化 SDL 失败: " + std::string(SDL_GetError()));
        return -1;
    }

    // SDL3_image 不再需要显式初始化
    Render::Logger::GetInstance().Info("SDL3_image 已就绪");

    // 创建 OpenGL 上下文
    auto context = std::make_unique<Render::OpenGLContext>();
    if (!context->Initialize("纹理测试", WINDOW_WIDTH, WINDOW_HEIGHT)) {
        Render::Logger::GetInstance().Error("初始化 OpenGL 上下文失败");
        SDL_Quit();
        return -1;
    }

    // 创建着色器程序
    auto shader = std::make_shared<Render::Shader>();
    
    // 简单的纹理着色器代码
    const char* vertexShaderSource = R"(
#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos, 1.0);
    // 翻转 Y 轴以修正图片上下颠倒的问题（OpenGL 纹理坐标原点在左下，图片原点在左上）
    TexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);
}
)";
    
    const char* fragmentShaderSource = R"(
#version 450 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D uTexture;

        void main() {
            // 使用纹理采样
            FragColor = texture(uTexture, TexCoord);
        }
)";
    
    if (!shader->LoadFromSource(vertexShaderSource, fragmentShaderSource)) {
        Render::Logger::GetInstance().Error("编译着色器失败");
        context->Shutdown();
        SDL_Quit();
        return -1;
    }

    // 创建 VAO、VBO、EBO
    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    // 上传顶点数据
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 上传索引数据
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // 位置属性 (location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 纹理坐标属性 (location = 1)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    
    Render::Logger::GetInstance().Info("几何体创建完成");

    // 使用 TextureLoader 加载纹理（带缓存）
    auto& textureLoader = Render::TextureLoader::GetInstance();
    Render::TexturePtr texture;
    
    // 尝试加载测试图片
    texture = textureLoader.LoadTexture("test_image", "textures/test.jpg", true);
    if (!texture) {
        texture = textureLoader.LoadTexture("test_image", "textures/test.png", true);
    }
    
    // 如果没有找到纹理文件，创建一个程序生成的纹理（棋盘格）
    if (!texture) {
        Render::Logger::GetInstance().Warning("未找到纹理文件，创建程序化棋盘格纹理");
        
        const int texWidth = 256;
        const int texHeight = 256;
        std::vector<unsigned char> checkerboard(texWidth * texHeight * 4);
        
        // 生成棋盘格纹理数据
        for (int y = 0; y < texHeight; y++) {
            for (int x = 0; x < texWidth; x++) {
                int index = (y * texWidth + x) * 4;
                bool isWhite = ((x / 32) + (y / 32)) % 2 == 0;
                unsigned char color = isWhite ? 255 : 64;
                
                checkerboard[index + 0] = color;      // R
                checkerboard[index + 1] = color;      // G
                checkerboard[index + 2] = color;      // B
                checkerboard[index + 3] = 255;        // A
            }
        }
        
        texture = textureLoader.CreateTexture("checkerboard", 
                                              checkerboard.data(), 
                                              texWidth, texHeight,
                                              Render::TextureFormat::RGBA, 
                                              true);
        
        if (!texture) {
            Render::Logger::GetInstance().Error("创建程序化纹理失败");
            glDeleteVertexArrays(1, &VAO);
            glDeleteBuffers(1, &VBO);
            glDeleteBuffers(1, &EBO);
            context->Shutdown();
            SDL_Quit();
            return -1;
        }
    }

    // 设置纹理参数
    texture->SetFilter(Render::TextureFilter::Linear, Render::TextureFilter::Linear);
    texture->SetWrap(Render::TextureWrap::Repeat, Render::TextureWrap::Repeat);

    Render::Logger::GetInstance().Info("纹理加载成功: " + 
        std::to_string(texture->GetWidth()) + "x" + std::to_string(texture->GetHeight()));
    
    // 演示缓存功能：再次加载相同的纹理（应该从缓存中获取）
    Render::Logger::GetInstance().Info("\n--- 测试纹理缓存 ---");
    auto texture2 = textureLoader.GetTexture("test_image");
    if (!texture2) {
        texture2 = textureLoader.GetTexture("checkerboard");
    }
    if (texture2) {
        Render::Logger::GetInstance().Info("从缓存获取纹理成功（同一实例: " + 
            std::string(texture2.get() == texture.get() ? "是" : "否") + "）");
    }
    
    // 打印缓存统计信息
    textureLoader.PrintStatistics();

    // 预先设置 uniform
    shader->Use();
    shader->GetUniformManager()->SetInt("uTexture", 0);
    shader->Unuse();
    Render::Logger::GetInstance().Info("纹理采样器 uniform 已设置");

    // 禁用面剔除和深度测试（2D 渲染）
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    
    Render::Logger::GetInstance().Info("\n========================================");
    Render::Logger::GetInstance().Info("进入渲染循环...");
    Render::Logger::GetInstance().Info("控制：ESC = 退出");
    Render::Logger::GetInstance().Info("========================================");
    
    bool running = true;
    SDL_Event event;
    int frameCount = 0;
    
    while (running) {
        // 事件处理
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
            }
        }

        // 渲染
        glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // 绑定纹理并渲染
        texture->Bind(0);
        shader->Use();
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // 交换缓冲区
        context->SwapBuffers();
        
        frameCount++;
    }
    
    Render::Logger::GetInstance().Info("\n渲染了 " + std::to_string(frameCount) + " 帧");

    // 清理
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);

    // 清理未使用的纹理
    Render::Logger::GetInstance().Info("\n--- 清理资源 ---");
    size_t cleaned = textureLoader.CleanupUnused();
    Render::Logger::GetInstance().Info("清理了 " + std::to_string(cleaned) + " 个未使用的纹理");
    
    // 最终统计
    Render::Logger::GetInstance().Info("\n--- 最终统计 ---");
    textureLoader.PrintStatistics();

    Render::Logger::GetInstance().Info("\n正在关闭程序...");
    Render::Logger::GetInstance().Info("日志已保存到: " + Render::Logger::GetInstance().GetCurrentLogFile());
    
    context->Shutdown();
    SDL_Quit();

    return 0;
}

