/**
 * @file 34_postprocess_test.cpp
 * @brief 后处理效果测试
 * 
 * 测试内容：
 * - 帧缓冲离屏渲染
 * - 后处理着色器效果
 * - 多种后处理效果切换（灰度、反色、模糊、边缘检测）
 */

#include <SDL3/SDL.h>
#include <iostream>
#include <vector>
#include <cmath>

#include "render/renderer.h"
#include "render/framebuffer.h"
#include "render/shader_cache.h"
#include "render/mesh_loader.h"
#include "render/material.h"
#include "render/logger.h"
#include "render/camera.h"
#include "render/math_utils.h"

using namespace Render;

// 后处理效果类型（匹配你的screen.frag）
enum class PostProcessEffect {
    None = 0,           // 无后处理
    Grayscale = 1,      // 灰度化
    Invert = 2,         // 反色
    Blur = 3,           // 模糊
    Sharpen = 4,        // 锐化
    EdgeDetection = 5,  // 边缘检测（新增）
    Count
};

const char* GetEffectName(PostProcessEffect effect) {
    switch (effect) {
        case PostProcessEffect::None: return "None (直接渲染)";
        case PostProcessEffect::Grayscale: return "Grayscale (灰度)";
        case PostProcessEffect::Invert: return "Invert (反色)";
        case PostProcessEffect::Blur: return "Blur (模糊)";
        case PostProcessEffect::Sharpen: return "Sharpen (锐化)";
        case PostProcessEffect::EdgeDetection: return "Edge Detection (边缘检测)";
        default: return "Unknown";
    }
}

// 创建全屏四边形（用于后处理）
// 注意：由于screen.vert已经翻转Y轴，这里使用正常的纹理坐标
std::shared_ptr<Mesh> CreateFullscreenQuad() {
    std::vector<Vertex> vertices = {
        {{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, Color::White()},  // 左下 -> 纹理底部
        {{ 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, Color::White()},  // 右下 -> 纹理底部
        {{ 1.0f,  1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, Color::White()},  // 右上 -> 纹理顶部
        {{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, Color::White()},  // 左上 -> 纹理顶部
    };
    
    std::vector<uint32_t> indices = {
        0, 1, 2,
        2, 3, 0
    };
    
    auto mesh = std::make_shared<Mesh>();
    mesh->SetVertices(vertices);
    mesh->SetIndices(indices);
    mesh->Upload();
    
    return mesh;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    Logger::GetInstance().InfoFormat("[PostProcess Test] === Post-Processing Effects Test ===");
    
    // ============================================================
    // 1. 初始化渲染器
    // ============================================================
    auto renderer = std::make_unique<Renderer>();
    if (!renderer->Initialize("后处理效果测试", 1280, 720)) {
        Logger::GetInstance().ErrorFormat("[PostProcess Test] Failed to initialize renderer");
        return -1;
    }
    Logger::GetInstance().InfoFormat("[PostProcess Test] Renderer initialized");
    
    // ============================================================
    // 2. 创建帧缓冲（用于离屏渲染）
    // ============================================================
    auto framebuffer = std::make_shared<Framebuffer>();
    
    FramebufferConfig fbConfig;
    fbConfig.SetSize(1280, 720)
            .SetName("PostProcessFB")
            .AddColorAttachment(TextureFormat::RGBA)  // 颜色附件
            .AddDepthAttachment(false);                // 深度附件（使用纹理以便采样）
    
    if (!framebuffer->Create(fbConfig)) {
        Logger::GetInstance().ErrorFormat("[PostProcess Test] Failed to create framebuffer");
        renderer->Shutdown();
        return -1;
    }
    Logger::GetInstance().InfoFormat("[PostProcess Test] Framebuffer created: 1280x720");
    
    // ============================================================
    // 3. 加载着色器
    // ============================================================
    auto& shaderCache = ShaderCache::GetInstance();
    
    // 基础着色器（场景渲染）
    auto sceneShader = shaderCache.LoadShader("basic", "shaders/basic.vert", "shaders/basic.frag");
    if (!sceneShader) {
        Logger::GetInstance().ErrorFormat("[PostProcess Test] Failed to load scene shader");
        renderer->Shutdown();
        return -1;
    }
    
    // 后处理着色器（屏幕空间）
    auto postProcessShader = shaderCache.LoadShader("screen", "shaders/screen.vert", "shaders/screen.frag");
    if (!postProcessShader) {
        Logger::GetInstance().ErrorFormat("[PostProcess Test] Failed to load post-process shader");
        renderer->Shutdown();
        return -1;
    }
    
    Logger::GetInstance().InfoFormat("[PostProcess Test] Shaders loaded");
    
    // ============================================================
    // 4. 创建场景对象
    // ============================================================
    // 创建相机
    Camera camera;
    camera.SetPerspective(60.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);
    camera.SetPosition(Vector3(0, 2, 5));
    camera.LookAt(Vector3(0, 0, 0));
    
    // 创建多个立方体
    std::vector<std::shared_ptr<Mesh>> cubes;
    std::vector<Vector3> cubePositions;
    std::vector<Color> cubeColors = {
        Color(1.0f, 0.3f, 0.3f, 1.0f),  // 红
        Color(0.3f, 1.0f, 0.3f, 1.0f),  // 绿
        Color(0.3f, 0.3f, 1.0f, 1.0f),  // 蓝
        Color(1.0f, 1.0f, 0.3f, 1.0f),  // 黄
        Color(1.0f, 0.3f, 1.0f, 1.0f),  // 品红
    };
    
    for (size_t i = 0; i < 5; ++i) {
        auto cube = MeshLoader::CreateCube(1.0f);
        cubes.push_back(cube);
        
        float angle = (float)i * (360.0f / 5.0f);
        float radius = 2.0f;
        float x = radius * std::cos(angle * 3.14159f / 180.0f);
        float z = radius * std::sin(angle * 3.14159f / 180.0f);
        cubePositions.push_back(Vector3(x, 0, z));
    }
    
    // 创建全屏四边形（用于后处理）
    auto screenQuad = CreateFullscreenQuad();
    
    Logger::GetInstance().InfoFormat("[PostProcess Test] Scene created: %zu cubes", cubes.size());
    
    // ============================================================
    // 5. 设置渲染状态
    // ============================================================
    auto renderState = renderer->GetRenderState();
    renderState->SetDepthTest(true);
    renderState->SetCullFace(CullFace::Back);
    
    // ============================================================
    // 6. 主渲染循环
    // ============================================================
    PostProcessEffect currentEffect = PostProcessEffect::None;
    bool running = true;
    int frameCount = 0;
    
    Logger::GetInstance().InfoFormat("[PostProcess Test] Starting render loop...");
    Logger::GetInstance().InfoFormat("[PostProcess Test] Controls:");
    Logger::GetInstance().InfoFormat("[PostProcess Test]   1-6: Switch effects");
    Logger::GetInstance().InfoFormat("[PostProcess Test]   ESC: Exit");
    Logger::GetInstance().InfoFormat("[PostProcess Test] Current effect: %s", GetEffectName(currentEffect));
    
    while (running) {
        // 事件处理
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
                // 切换后处理效果
                if (event.key.key >= SDLK_1 && event.key.key <= SDLK_6) {
                    int effectIndex = event.key.key - SDLK_1;
                    currentEffect = static_cast<PostProcessEffect>(effectIndex);
                    Logger::GetInstance().InfoFormat("[PostProcess Test] Switched to: %s", 
                                 GetEffectName(currentEffect));
                }
            }
        }
        
        float deltaTime = 0.016f;  // 60 FPS
        
        // 旋转立方体
        float rotationAngle = frameCount * 2.0f;
        
        // ============================================================
        // 第一阶段：渲染场景到帧缓冲
        // ============================================================
        if (currentEffect != PostProcessEffect::None) {
            // 绑定帧缓冲进行离屏渲染
            framebuffer->Bind();
            renderState->SetClearColor(Color(0.1f, 0.1f, 0.15f, 1.0f));
            framebuffer->Clear(true, true, false);
        } else {
            // 直接渲染到屏幕
            renderer->BeginFrame();
            renderState->SetClearColor(Color(0.1f, 0.1f, 0.15f, 1.0f));
            renderer->Clear();
        }
        
        // 渲染场景
        sceneShader->Use();
        auto sceneUniforms = sceneShader->GetUniformManager();
        
        Matrix4 view = camera.GetViewMatrix();
        Matrix4 projection = camera.GetProjectionMatrix();
        
        sceneUniforms->SetMatrix4("view", view);
        sceneUniforms->SetMatrix4("projection", projection);
        sceneUniforms->SetBool("useTexture", false);
        sceneUniforms->SetBool("useVertexColor", true);
        
        // 绘制所有立方体
        for (size_t i = 0; i < cubes.size(); ++i) {
            Matrix4 model = Matrix4::Identity();
            
            // 位置
            Vector3 pos = cubePositions[i];
            model(0, 3) = pos.x();
            model(1, 3) = pos.y();
            model(2, 3) = pos.z();
            
            // 旋转
            float angle = (rotationAngle + i * 72.0f) * 3.14159f / 180.0f;
            model(0, 0) = std::cos(angle);
            model(0, 2) = std::sin(angle);
            model(2, 0) = -std::sin(angle);
            model(2, 2) = std::cos(angle);
            
            sceneUniforms->SetMatrix4("model", model);
            sceneUniforms->SetColor("color", cubeColors[i % cubeColors.size()]);
            
            cubes[i]->Draw();
        }
        
        // ============================================================
        // 第二阶段：应用后处理效果
        // ============================================================
        if (currentEffect != PostProcessEffect::None) {
            // 解绑帧缓冲，渲染到屏幕
            framebuffer->Unbind();
            
            renderer->BeginFrame();
            renderState->SetClearColor(Color(0.0f, 0.0f, 0.0f, 1.0f));
            renderer->Clear();
            
            // 禁用深度测试（绘制全屏四边形）
            renderState->SetDepthTest(false);
            
            // 使用后处理着色器
            postProcessShader->Use();
            auto postUniforms = postProcessShader->GetUniformManager();
            
            // 绑定帧缓冲的颜色纹理
            GLuint colorTextureID = framebuffer->GetColorAttachmentID(0);
            if (colorTextureID != 0) {
                renderState->BindTexture(0, colorTextureID);
                postUniforms->SetInt("uTexture", 0);
            }
            
            // 设置后处理效果参数（使用原有的uniform名称）
            postUniforms->SetInt("postProcessMode", static_cast<int>(currentEffect));
            
            // 绘制全屏四边形
            screenQuad->Draw();
            
            // 恢复深度测试
            renderState->SetDepthTest(true);
        }
        
        // 结束渲染帧
        renderer->EndFrame();
        renderer->Present();
        
        frameCount++;
        
        // 每60帧输出信息
        if (frameCount % 60 == 0) {
            Logger::GetInstance().InfoFormat("[PostProcess Test] Frame %d: Effect = %s", 
                         frameCount, GetEffectName(currentEffect));
        }
        
        SDL_Delay(16);  // ~60 FPS
    }
    
    Logger::GetInstance().InfoFormat("[PostProcess Test] Rendered %d frames", frameCount);
    
    // ============================================================
    // 7. 清理
    // ============================================================
    framebuffer->Release();
    renderer->Shutdown();
    
    Logger::GetInstance().InfoFormat("[PostProcess Test] === Test Completed Successfully ===");
    
    return 0;
}

