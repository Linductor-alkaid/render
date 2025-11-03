/**
 * @file 30_framebuffer_test.cpp
 * @brief 帧缓冲测试程序
 * 
 * 演示：
 * 1. 基础离屏渲染
 * 2. 后处理效果（灰度、反色、模糊）
 * 3. MSAA抗锯齿
 * 4. 动态调整大小
 */

#include <render/renderer.h>
#include <render/logger.h>
#include <render/shader_cache.h>
#include <render/mesh_loader.h>
#include <render/framebuffer.h>
#include <render/camera.h>
#include <SDL3/SDL.h>
#include <cmath>
#include <iostream>

using namespace Render;

// 后处理模式
enum class PostProcessMode {
    None,        // 无后处理
    Grayscale,   // 灰度
    Invert,      // 反色
    Blur,        // 模糊
    Sharpen      // 锐化
};

int main() {
    // ========================================
    // 1. 初始化系统
    // ========================================
    
    Logger::GetInstance().SetLogToConsole(true);
    Logger::GetInstance().SetLogToFile(true);
    Logger::GetInstance().Info("=== Framebuffer Test ===");
    
    Renderer renderer;
    if (!renderer.Initialize("Framebuffer Test", 1280, 720)) {
        Logger::GetInstance().Error("Failed to initialize renderer");
        return -1;
    }
    
    auto renderState = renderer.GetRenderState();
    renderState->SetDepthTest(true);
    renderState->SetDepthFunc(DepthFunc::Less);
    renderState->SetCullFace(CullFace::Back);
    renderState->SetClearColor(Color(0.1f, 0.1f, 0.15f, 1.0f));
    
    // ========================================
    // 2. 加载着色器
    // ========================================
    
    auto& shaderCache = ShaderCache::GetInstance();
    
    // 场景着色器（基础光照）
    auto sceneShader = shaderCache.LoadShader("scene",
        "shaders/mesh_test.vert",
        "shaders/mesh_test.frag");
    
    if (!sceneShader || !sceneShader->IsValid()) {
        Logger::GetInstance().Error("Failed to load scene shader");
        return -1;
    }
    
    // 屏幕着色器（后处理）
    auto screenShader = shaderCache.LoadShader("screen",
        "shaders/screen.vert",
        "shaders/screen.frag");
    
    if (!screenShader || !screenShader->IsValid()) {
        Logger::GetInstance().Error("Failed to load screen shader");
        return -1;
    }
    
    // ========================================
    // 3. 创建场景几何
    // ========================================
    
    auto cube = MeshLoader::CreateCube(2.0f, 2.0f, 2.0f, Color::White());
    auto sphere = MeshLoader::CreateSphere(1.0f, 32, 16, Color::White());
    auto plane = MeshLoader::CreatePlane(10.0f, 10.0f, 1, 1, Color::White());
    
    // ========================================
    // 4. 创建相机
    // ========================================
    
    Camera camera;
    camera.SetPerspective(60.0f, 
                         static_cast<float>(renderer.GetWidth()) / renderer.GetHeight(),
                         0.1f, 1000.0f);
    // 调整相机位置：从前方稍高位置看向场景
    camera.SetPosition(Vector3(0.0f, 4.0f, 12.0f));  // 相机位置：稍微高一点往后
    camera.LookAt(Vector3(0.0f, 0.5f, 0.0f));  // 看向场景下方一点
    
    // ========================================
    // 4.5 创建Transform对象（避免在渲染循环中创建临时对象）
    // ========================================
    
    Transform cubeTransform;
    cubeTransform.SetPosition(Vector3(-2.5f, 1.0f, 0.0f));
    
    // ========================================
    // 5. 创建帧缓冲
    // ========================================
    
    // 主帧缓冲（离屏渲染）
    auto framebuffer = std::make_shared<Framebuffer>();
    
    FramebufferConfig fbConfig;
    fbConfig.SetSize(renderer.GetWidth(), renderer.GetHeight())
            .SetName("Main Framebuffer")
            .AddColorAttachment(TextureFormat::RGBA, false)  // 纹理附件（可采样）
            .AddDepthAttachment(true);  // 渲染缓冲（不采样）
    
    if (!framebuffer->Create(fbConfig)) {
        Logger::GetInstance().Error("Failed to create framebuffer");
        return -1;
    }
    
    Logger::GetInstance().Info("Created framebuffer: " + std::to_string(framebuffer->GetWidth()) + 
                              "x" + std::to_string(framebuffer->GetHeight()));
    Logger::GetInstance().Info("Status: " + framebuffer->GetStatusString());
    
    // MSAA 帧缓冲（可选）
    auto msaaFramebuffer = std::make_shared<Framebuffer>();
    
    FramebufferConfig msaaConfig;
    msaaConfig.SetSize(renderer.GetWidth(), renderer.GetHeight())
              .SetSamples(4)  // 4x MSAA
              .SetName("MSAA Framebuffer")
              .AddColorAttachment(TextureFormat::RGBA, true)  // RBO
              .AddDepthAttachment(true);  // RBO
    
    if (!msaaFramebuffer->Create(msaaConfig)) {
        Logger::GetInstance().Warning("Failed to create MSAA framebuffer, MSAA disabled");
        msaaFramebuffer = nullptr;
    }
    
    // ========================================
    // 6. 创建全屏四边形（用于后处理）
    // ========================================
    
    auto screenQuad = MeshLoader::CreateQuad(2.0f, 2.0f);
    
    // ========================================
    // 7. 主循环
    // ========================================
    
    bool running = true;
    float time = 0.0f;
    float lastFrameTime = 0.0f;
    
    PostProcessMode postProcessMode = PostProcessMode::None;
    bool useMSAA = (msaaFramebuffer != nullptr);
    bool showHelp = true;
    
    Logger::GetInstance().Info("Controls:");
    Logger::GetInstance().Info("  [1-5] - Change post-process mode");
    Logger::GetInstance().Info("  [M]   - Toggle MSAA");
    Logger::GetInstance().Info("  [H]   - Toggle help");
    Logger::GetInstance().Info("  [ESC] - Quit");
    
    
    while (running) {
        // ========================================
        // 事件处理
        // ========================================
        
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                switch (event.key.key) {
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                    case SDLK_1:
                        postProcessMode = PostProcessMode::None;
                        Logger::GetInstance().Info("Post-process: None");
                        break;
                    case SDLK_2:
                        postProcessMode = PostProcessMode::Grayscale;
                        Logger::GetInstance().Info("Post-process: Grayscale");
                        break;
                    case SDLK_3:
                        postProcessMode = PostProcessMode::Invert;
                        Logger::GetInstance().Info("Post-process: Invert");
                        break;
                    case SDLK_4:
                        postProcessMode = PostProcessMode::Blur;
                        Logger::GetInstance().Info("Post-process: Blur");
                        break;
                    case SDLK_5:
                        postProcessMode = PostProcessMode::Sharpen;
                        Logger::GetInstance().Info("Post-process: Sharpen");
                        break;
                    case SDLK_M:
                        if (msaaFramebuffer) {
                            useMSAA = !useMSAA;
                            Logger::GetInstance().Info(std::string("MSAA: ") + (useMSAA ? "ON" : "OFF"));
                        }
                        break;
                    case SDLK_H:
                        showHelp = !showHelp;
                        break;
                    default:
                        break;
                }
            } else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                int width = event.window.data1;
                int height = event.window.data2;
                
                Logger::GetInstance().Info("Window resized: " + std::to_string(width) + 
                                          "x" + std::to_string(height));
                
                // 调整帧缓冲大小
                framebuffer->Resize(width, height);
                if (msaaFramebuffer) {
                    msaaFramebuffer->Resize(width, height);
                }
                
                // 更新相机宽高比
                camera.SetAspectRatio(static_cast<float>(width) / height);
                
                // 更新视口
                renderState->SetViewport(0, 0, width, height);
            }
        }
        
        // ========================================
        // 更新
        // ========================================
        
        renderer.BeginFrame();
        float deltaTime = renderer.GetDeltaTime();
        time += deltaTime;
        
        // 计算MVP矩阵
        Matrix4 viewMatrix = camera.GetViewMatrix();
        Matrix4 projMatrix = camera.GetProjectionMatrix();
        
        // ========================================
        // 第一步：渲染场景到帧缓冲
        // ========================================
        
        auto targetFramebuffer = (useMSAA && msaaFramebuffer) ? msaaFramebuffer : framebuffer;
        
        targetFramebuffer->Bind();
        targetFramebuffer->Clear();
        
        renderState->SetDepthTest(true);
        
        sceneShader->Use();
        auto* sceneUniforms = sceneShader->GetUniformManager();
        sceneUniforms->SetVector3("uLightDir", Vector3(-0.3f, -0.8f, -0.5f));
        
        // 绘制立方体
        float cubeRotation = time * 50.0f;  // 角度/秒
        cubeTransform.SetRotationEulerDegrees(Vector3(0.0f, cubeRotation, 0.0f));  // 直接设置欧拉角（度数）
        Matrix4 cubeModel = cubeTransform.GetWorldMatrix();
        
        Matrix4 mvp = projMatrix * viewMatrix * cubeModel;
        sceneUniforms->SetMatrix4("uMVP", mvp);
        sceneUniforms->SetColor("uColor", Color(1.0f, 0.5f, 0.3f, 1.0f));
        cube->Draw();
        
        // 绘制球体
        Matrix4 sphereModel = Matrix4::Identity();
        float sphereY = 1.0f + std::sin(time * 2.0f) * 0.5f;
        sphereModel.col(3).head<3>() = Vector3(2.5f, sphereY, 0.0f);
        
        mvp = projMatrix * viewMatrix * sphereModel;
        sceneUniforms->SetMatrix4("uMVP", mvp);
        sceneUniforms->SetColor("uColor", Color(0.3f, 0.7f, 1.0f, 1.0f));
        sphere->Draw();
        
        // 绘制地面（放在底部）
        Matrix4 planeModel = Matrix4::Identity();
        planeModel.col(3).head<3>() = Vector3(0.0f, -0.5f, 0.0f);  // Y=-0.5，明显在物体下方
        
        mvp = projMatrix * viewMatrix * planeModel;
        sceneUniforms->SetMatrix4("uMVP", mvp);
        sceneUniforms->SetColor("uColor", Color(0.6f, 0.6f, 0.6f, 1.0f));
        plane->Draw();
        
        targetFramebuffer->Unbind();
        
        // 如果使用MSAA，解析到普通帧缓冲
        if (useMSAA && msaaFramebuffer) {
            msaaFramebuffer->BlitTo(framebuffer.get(), 
                                   GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
                                   GL_NEAREST);
        }
        
        // ========================================
        // 第二步：渲染帧缓冲到屏幕（带后处理）
        // ========================================
        
        renderer.Clear();
        renderState->SetDepthTest(false);
        
        // 绑定帧缓冲的颜色附件
        framebuffer->BindColorAttachment(0, 0);
        
        screenShader->Use();
        auto* screenUniforms = screenShader->GetUniformManager();
        screenUniforms->SetInt("uTexture", 0);
        screenUniforms->SetInt("postProcessMode", static_cast<int>(postProcessMode));
        
        screenQuad->Draw();
        
        // ========================================
        // 显示帮助信息
        // ========================================
        
        if (showHelp) {
            // 这里可以使用文字渲染显示帮助
            // 暂时只在窗口标题显示
        }
        
        // 更新窗口标题
        if (time - lastFrameTime >= 1.0f) {
            std::string title = "Framebuffer Test | FPS: " + std::to_string(static_cast<int>(renderer.GetFPS()));
            title += " | Mode: ";
            switch (postProcessMode) {
                case PostProcessMode::None: title += "None"; break;
                case PostProcessMode::Grayscale: title += "Grayscale"; break;
                case PostProcessMode::Invert: title += "Invert"; break;
                case PostProcessMode::Blur: title += "Blur"; break;
                case PostProcessMode::Sharpen: title += "Sharpen"; break;
            }
            title += " | MSAA: " + std::string(useMSAA ? "ON" : "OFF");
            
            renderer.SetWindowTitle(title);
            lastFrameTime = time;
        }
        
        renderer.EndFrame();
        renderer.Present();
    }
    
    // ========================================
    // 清理
    // ========================================
    
    Logger::GetInstance().Info("Shutting down...");
    
    return 0;
}

