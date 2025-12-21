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
 * @file 64_cubemap_test.cpp
 * @brief 立方体贴图加载和渲染测试示例
 * 
 * 本示例演示：
 * 1. 使用 TextureCubemap 类从6个面加载立方体贴图
 * 2. 程序化生成立方体贴图（如果文件不存在）
 * 3. 渲染天空盒
 * 4. 验证立方体贴图的基本功能
 * 
 * 控制：
 * - ESC：退出
 * - R：重新加载立方体贴图
 * - M：切换Mipmap显示
 */

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <glad/glad.h>
#include <iostream>
#include <memory>
#include <vector>
#include <cmath>

#include "render/renderer.h"
#include "render/opengl_context.h"
#include "render/shader.h"
#include "render/texture_cubemap.h"
#include "render/mesh.h"
#include "render/mesh_loader.h"
#include "render/logger.h"
#include "render/types.h"
#include "render/math_utils.h"

using namespace Render;

const int WINDOW_WIDTH = 1280;
const int WINDOW_HEIGHT = 720;

// 天空盒着色器
const char* skyboxVertexShader = R"(
#version 450 core
layout (location = 0) in vec3 aPos;

out vec3 TexCoord;

uniform mat4 uProjection;
uniform mat4 uView;

void main() {
    // 立方体贴图坐标：直接使用顶点位置作为方向向量
    // CreateCube创建的立方体顶点位置就是从原点指向外部的方向向量
    // OpenGL立方体贴图坐标系统：从内部看，坐标指向外部
    
    // 移除视图矩阵的平移部分，只保留旋转（天空盒应该跟随相机旋转，但不跟随平移）
    mat4 viewNoTranslation = mat4(mat3(uView));
    
    // 立方体贴图坐标也需要跟随视图旋转（这样天空盒会跟随相机旋转）
    // 注意：使用方向向量（w=0）而不是位置（w=1）
    vec3 viewSpaceDir = (viewNoTranslation * vec4(aPos, 0.0)).xyz;
    
    // OpenGL立方体贴图坐标系统约定：
    // 根据测试结果调整：反转X和Z轴
    TexCoord = vec3(-viewSpaceDir.x, viewSpaceDir.y, -viewSpaceDir.z);
    
    // 计算裁剪空间位置
    vec4 pos = uProjection * viewNoTranslation * vec4(aPos, 1.0);
    // 使用xyww确保深度值始终为1.0（最远），这样天空盒总是在最后渲染
    gl_Position = pos.xyww;
}
)";

const char* skyboxFragmentShader = R"(
#version 450 core
out vec4 FragColor;

in vec3 TexCoord;

uniform samplerCube uSkybox;
uniform bool uShowMipmap;

void main() {
    if (uShowMipmap) {
        // 显示Mipmap级别（用于调试）
        // 使用基于屏幕空间梯度的简单方法来估算Mipmap级别
        // 这对于立方体贴图更可靠，因为textureQueryLod在某些情况下可能不可用
        
        vec3 coord = normalize(TexCoord);
        
        // 计算屏幕空间梯度（纹理坐标的变化率）
        vec3 dx = dFdx(coord);
        vec3 dy = dFdy(coord);
        
        // 计算梯度的长度（表示纹理在屏幕上的缩放）
        float gradientLength = max(length(dx), length(dy));
        
        // 估算Mipmap级别：梯度越大，需要的Mipmap级别越高
        // 对于256x256纹理，最大Mipmap级别约为8
        // 使用log2来将梯度映射到Mipmap级别
        float estimatedLevel = log2(max(gradientLength * 256.0 + 1.0, 1.0));
        
        // 限制在合理范围内（0-8）
        float mipLevel = clamp(estimatedLevel, 0.0, 8.0);
        
        // 归一化到0-1范围用于可视化
        float normalizedLevel = mipLevel / 8.0;
        
        // 使用颜色渐变显示Mipmap级别
        // 0级=蓝色, 中间=绿色, 高级=红色
        vec3 color;
        if (normalizedLevel < 0.33) {
            color = mix(vec3(0.0, 0.0, 1.0), vec3(0.0, 1.0, 1.0), normalizedLevel * 3.0);
        } else if (normalizedLevel < 0.66) {
            color = mix(vec3(0.0, 1.0, 1.0), vec3(0.0, 1.0, 0.0), (normalizedLevel - 0.33) * 3.0);
        } else {
            color = mix(vec3(0.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), (normalizedLevel - 0.66) * 3.0);
        }
        
        FragColor = vec4(color, 1.0);
    } else {
        // 采样立方体贴图
        // 确保坐标已归一化
        vec3 coord = normalize(TexCoord);
        vec4 color = texture(uSkybox, coord);
        
        FragColor = color;
    }
}
)";

/**
 * @brief 创建程序化立方体贴图（每个面不同颜色）
 */
std::shared_ptr<TextureCubemap> CreateProceduralCubemap(int resolution = 256) {
    auto cubemap = std::make_shared<TextureCubemap>();
    
    // 每个面的颜色（RGB）
    struct FaceColor {
        float r, g, b;
    };
    
    FaceColor faceColors[6] = {
        {1.0f, 0.0f, 0.0f},  // +X (右) - 红色
        {0.0f, 1.0f, 0.0f},  // -X (左) - 绿色
        {0.0f, 0.0f, 1.0f},  // +Y (上) - 蓝色
        {1.0f, 1.0f, 0.0f},  // -Y (下) - 黄色
        {1.0f, 0.0f, 1.0f},  // +Z (前) - 品红
        {0.0f, 1.0f, 1.0f}   // -Z (后) - 青色
    };
    
    std::vector<unsigned char> faceData(resolution * resolution * 4);
    
    CubemapFace faces[] = {
        CubemapFace::PositiveX,
        CubemapFace::NegativeX,
        CubemapFace::PositiveY,
        CubemapFace::NegativeY,
        CubemapFace::PositiveZ,
        CubemapFace::NegativeZ
    };
    
    for (int i = 0; i < 6; ++i) {
        // 填充每个面的数据
        for (int y = 0; y < resolution; ++y) {
            for (int x = 0; x < resolution; ++x) {
                int index = (y * resolution + x) * 4;
                faceData[index + 0] = static_cast<unsigned char>(faceColors[i].r * 255);
                faceData[index + 1] = static_cast<unsigned char>(faceColors[i].g * 255);
                faceData[index + 2] = static_cast<unsigned char>(faceColors[i].b * 255);
                faceData[index + 3] = 255;  // Alpha
            }
        }
        
        // 创建该面
        if (!cubemap->CreateFaceFromData(faces[i], faceData.data(), resolution, resolution, TextureFormat::RGBA)) {
            Logger::GetInstance().Error("创建立方体贴图面失败: " + std::to_string(i));
            return nullptr;
        }
    }
    
    // 生成Mipmap
    cubemap->GenerateMipmap();
    
    // 设置支持Mipmap的过滤模式（重要：否则Mipmap不会被使用）
    // TextureFilter::Mipmap 会被转换为 GL_LINEAR_MIPMAP_LINEAR（当有Mipmap时）
    cubemap->SetFilter(TextureFilter::Mipmap, TextureFilter::Linear);
    
    Logger::GetInstance().Info("程序化立方体贴图Mipmap已生成，过滤模式已设置");
    
    Logger::GetInstance().Info("创建程序化立方体贴图成功: " + std::to_string(resolution) + "x" + std::to_string(resolution));
    
    return cubemap;
}

/**
 * @brief 尝试从文件加载立方体贴图
 */
std::shared_ptr<TextureCubemap> LoadCubemapFromFiles() {
    auto cubemap = std::make_shared<TextureCubemap>();
    
    // 尝试从常见的天空盒路径加载
    std::vector<std::vector<std::string>> possiblePaths = {
        // 路径1: textures/skybox/
        {
            "textures/skybox/right.png",   // +X
            "textures/skybox/left.png",    // -X
            "textures/skybox/top.png",     // +Y
            "textures/skybox/bottom.png",  // -Y
            "textures/skybox/front.png",   // +Z
            "textures/skybox/back.png"     // -Z
        },
        // 路径2: textures/skybox/ (使用posx, negx等命名)
        {
            "textures/skybox/posx.png",
            "textures/skybox/negx.png",
            "textures/skybox/posy.png",
            "textures/skybox/negy.png",
            "textures/skybox/posz.png",
            "textures/skybox/negz.png"
        },
        // 路径3: textures/cubemap/
        {
            "textures/cubemap/right.png",
            "textures/cubemap/left.png",
            "textures/cubemap/top.png",
            "textures/cubemap/bottom.png",
            "textures/cubemap/front.png",
            "textures/cubemap/back.png"
        }
    };
    
    for (const auto& paths : possiblePaths) {
        if (cubemap->LoadFromFiles(paths, true)) {
            Logger::GetInstance().Info("成功从文件加载立方体贴图");
            return cubemap;
        }
    }
    
    Logger::GetInstance().Warning("未找到立方体贴图文件，将使用程序化生成");
    return nullptr;
}

int main(int argc, char* argv[]) {
    // 初始化日志系统
    Logger::GetInstance().SetLogToFile(true);
    Logger::GetInstance().Info("=== 立方体贴图测试 ===");
    Logger::GetInstance().Info("日志文件: " + Logger::GetInstance().GetCurrentLogFile());

    // 初始化 SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        Logger::GetInstance().Error("初始化 SDL 失败: " + std::string(SDL_GetError()));
        return -1;
    }

    // 创建 OpenGL 上下文
    auto context = std::make_unique<OpenGLContext>();
    if (!context->Initialize("立方体贴图测试", WINDOW_WIDTH, WINDOW_HEIGHT)) {
        Logger::GetInstance().Error("初始化 OpenGL 上下文失败");
        SDL_Quit();
        return -1;
    }

    // 创建天空盒着色器
    auto skyboxShader = std::make_shared<Shader>();
    if (!skyboxShader->LoadFromSource(skyboxVertexShader, skyboxFragmentShader)) {
        Logger::GetInstance().Error("编译天空盒着色器失败");
        context->Shutdown();
        SDL_Quit();
        return -1;
    }

    // 加载或创建立方体贴图
    Logger::GetInstance().Info("\n--- 加载立方体贴图 ---");
    
    // 先测试程序化立方体贴图，确认渲染管道是否正常
    Logger::GetInstance().Info("步骤1: 测试程序化立方体贴图（验证渲染管道）");
    auto proceduralCubemap = CreateProceduralCubemap(256);
    if (!proceduralCubemap) {
        Logger::GetInstance().Error("创建程序化立方体贴图失败");
        context->Shutdown();
        SDL_Quit();
        return -1;
    }
    Logger::GetInstance().Info("程序化立方体贴图创建成功，ID: " + std::to_string(proceduralCubemap->GetID()));
    
    // 尝试从文件加载
    Logger::GetInstance().Info("步骤2: 尝试从文件加载立方体贴图");
    auto cubemap = LoadCubemapFromFiles();
    if (!cubemap) {
        Logger::GetInstance().Info("文件加载失败，使用程序化立方体贴图");
        cubemap = proceduralCubemap;
    } else {
        Logger::GetInstance().Info("成功从文件加载立方体贴图，ID: " + std::to_string(cubemap->GetID()));
        // 确保文件加载的立方体贴图也设置了正确的过滤模式
        cubemap->SetFilter(TextureFilter::Mipmap, TextureFilter::Linear);
        Logger::GetInstance().Info("文件加载的立方体贴图过滤模式已设置");
    }

    // 验证立方体贴图
    Logger::GetInstance().Info("\n--- 立方体贴图验证 ---");
    Logger::GetInstance().Info("  ID: " + std::to_string(cubemap->GetID()));
    Logger::GetInstance().Info("  分辨率: " + std::to_string(cubemap->GetResolution()) + "x" + std::to_string(cubemap->GetResolution()));
    Logger::GetInstance().Info("  是否完整: " + std::string(cubemap->IsComplete() ? "是" : "否"));
    Logger::GetInstance().Info("  是否有效: " + std::string(cubemap->IsValid() ? "是" : "否"));
    Logger::GetInstance().Info("  内存使用: " + std::to_string(cubemap->GetMemoryUsage() / 1024) + " KB");
    
    // 检查立方体贴图是否真的有效
    if (!cubemap->IsValid() || cubemap->GetID() == 0) {
        Logger::GetInstance().Error("立方体贴图无效！无法渲染");
        context->Shutdown();
        SDL_Quit();
        return -1;
    }
    
    if (!cubemap->IsComplete()) {
        Logger::GetInstance().Warning("立方体贴图不完整（某些面未加载）");
    }
    
    // 检查OpenGL错误
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        Logger::GetInstance().Warning("OpenGL错误（立方体贴图创建后）: " + std::to_string(err));
    }

    // 创建立方体网格（用于天空盒渲染）
    // 注意：天空盒立方体应该足够大，但不需要太大
    // 天空盒需要从内部看，所以立方体应该足够大以包围相机
    auto skyboxMesh = MeshLoader::CreateCube(100.0f, 100.0f, 100.0f, Color::White());
    if (!skyboxMesh) {
        Logger::GetInstance().Error("创建天空盒网格失败");
        context->Shutdown();
        SDL_Quit();
        return -1;
    }
    
    // 确保网格已上传到GPU
    skyboxMesh->Upload();
    if (!skyboxMesh->IsUploaded()) {
        Logger::GetInstance().Error("天空盒网格上传失败");
        context->Shutdown();
        SDL_Quit();
        return -1;
    }
    Logger::GetInstance().Info("天空盒网格已上传到GPU");

    // 创建一个中心物体（球体）用于展示
    auto centerMesh = MeshLoader::CreateSphere(0.3f, 32, 16, Color::White());
    if (!centerMesh) {
        Logger::GetInstance().Error("创建中心物体网格失败");
        context->Shutdown();
        SDL_Quit();
        return -1;
    }

    // 设置OpenGL状态
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);  // 允许深度等于1.0（天空盒在最后渲染）
    glDisable(GL_CULL_FACE);  // 天空盒需要双面渲染

    // 设置投影矩阵
    float aspect = static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT);
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    Matrix4 projection = MathUtils::PerspectiveDegrees(45.0f, aspect, nearPlane, farPlane);

    // 视图矩阵（相机在原点，看向-Z方向）
    // 对于天空盒，视图矩阵只包含旋转，不包含平移
    // 初始视图矩阵是单位矩阵（无旋转）
    Matrix4 view = Matrix4::Identity();
    
    // 设置uniform
    skyboxShader->Use();
    skyboxShader->GetUniformManager()->SetMatrix4("uProjection", projection);
    skyboxShader->GetUniformManager()->SetMatrix4("uView", view);
    skyboxShader->GetUniformManager()->SetInt("uSkybox", 0);
    skyboxShader->GetUniformManager()->SetBool("uShowMipmap", false);
    skyboxShader->Unuse();

    Logger::GetInstance().Info("\n========================================");
    Logger::GetInstance().Info("进入渲染循环...");
    Logger::GetInstance().Info("控制：");
    Logger::GetInstance().Info("  ESC = 退出");
    Logger::GetInstance().Info("  R = 重新加载立方体贴图");
    Logger::GetInstance().Info("  M = 切换Mipmap显示");
    Logger::GetInstance().Info("========================================");

    bool running = true;
    bool showMipmap = false;
    SDL_Event event;
    int frameCount = 0;
    float time = 0.0f;
    
    while (running) {
        // 事件处理
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.key == SDLK_R) {
                    // 重新加载立方体贴图
                    Logger::GetInstance().Info("重新加载立方体贴图...");
                    cubemap = LoadCubemapFromFiles();
                    if (!cubemap) {
                        cubemap = CreateProceduralCubemap(256);
                    }
                } else if (event.key.key == SDLK_M) {
                    // 切换Mipmap显示
                    showMipmap = !showMipmap;
                    skyboxShader->Use();
                    skyboxShader->GetUniformManager()->SetBool("uShowMipmap", showMipmap);
                    skyboxShader->Unuse();
                    Logger::GetInstance().Info("Mipmap显示: " + std::string(showMipmap ? "开启" : "关闭"));
                }
            }
        }

        // 更新时间
        time += 0.016f;  // 假设60fps

        // 暂时禁用视图旋转，先测试基础坐标系统
        // 对于天空盒，视图矩阵只包含旋转，不包含平移
        // float rotationY = time * 0.2f;  // 缓慢旋转
        // Quaternion rotation = MathUtils::AngleAxis(rotationY, Vector3::UnitY());
        // 视图矩阵的旋转部分是相机旋转的逆（共轭）
        // Quaternion viewRotation = rotation.conjugate();
        
        // 构建只包含旋转的视图矩阵（无平移）
        // view = MathUtils::Rotate(viewRotation);
        // 暂时使用单位矩阵（无旋转），测试基础坐标系统
        view = Matrix4::Identity();

        // 渲染
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 渲染天空盒（最后渲染，深度测试设置为LEQUAL）
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_CULL_FACE);  // 确保面剔除被禁用（天空盒从内部看）
        glDepthMask(GL_FALSE);    // 禁用深度写入（天空盒不写入深度）
        
        // 按照05_texture_test.cpp的顺序：先绑定纹理，再使用着色器
        cubemap->Bind(0);
        skyboxShader->Use();
        
        // 验证纹理绑定（调试用）
        if (frameCount == 0) {
            GLint boundTexture = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &boundTexture);
            if (boundTexture != static_cast<GLint>(cubemap->GetID())) {
                Logger::GetInstance().Warning("立方体贴图绑定失败！期望ID: " + 
                    std::to_string(cubemap->GetID()) + ", 实际绑定: " + std::to_string(boundTexture));
            } else {
                Logger::GetInstance().Info("立方体贴图绑定成功，ID: " + std::to_string(boundTexture));
            }
            
            // 检查纹理是否完整
            GLint width = 0;
            glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_WIDTH, &width);
            Logger::GetInstance().Info("立方体贴图+X面宽度: " + std::to_string(width));
        }
        
        // 设置uniform
        skyboxShader->GetUniformManager()->SetInt("uSkybox", 0);
        skyboxShader->GetUniformManager()->SetMatrix4("uView", view);
        skyboxShader->GetUniformManager()->SetMatrix4("uProjection", projection);
        skyboxShader->GetUniformManager()->SetBool("uShowMipmap", showMipmap);
        
        // 检查OpenGL错误
        GLenum err = glGetError();
        if (err != GL_NO_ERROR && frameCount == 0) {  // 只在第一帧报告错误
            Logger::GetInstance().Warning("OpenGL错误（渲染前）: " + std::to_string(err));
        }
        
        skyboxMesh->Draw();
        
        // 检查OpenGL错误
        err = glGetError();
        if (err != GL_NO_ERROR && frameCount == 0) {  // 只在第一帧报告错误
            Logger::GetInstance().Warning("OpenGL错误（渲染后）: " + std::to_string(err));
        }
        
        glDepthMask(GL_TRUE);  // 恢复深度写入
        cubemap->Unbind();
        skyboxShader->Unuse();

        // 渲染中心物体（简单渲染，用于展示）
        glDepthFunc(GL_LESS);
        // 这里可以添加一个简单的着色器来渲染中心物体
        // 暂时跳过，专注于天空盒测试

        // 交换缓冲区
        context->SwapBuffers();
        
        frameCount++;
    }
    
    Logger::GetInstance().Info("\n渲染了 " + std::to_string(frameCount) + " 帧");
    Logger::GetInstance().Info("平均FPS: " + std::to_string(frameCount / (time > 0 ? time : 1.0f)));

    Logger::GetInstance().Info("\n--- 立方体贴图最终信息 ---");
    Logger::GetInstance().Info("  内存使用: " + std::to_string(cubemap->GetMemoryUsage() / 1024) + " KB");
    Logger::GetInstance().Info("  是否完整: " + std::string(cubemap->IsComplete() ? "是" : "否"));

    Logger::GetInstance().Info("\n正在关闭程序...");
    Logger::GetInstance().Info("日志已保存到: " + Logger::GetInstance().GetCurrentLogFile());
    
    context->Shutdown();
    SDL_Quit();

    return 0;
}
