/**
 * @file 14_model_material_loader_test.cpp
 * @brief 测试从模型文件加载网格、材质和纹理
 * 
 * 本示例演示：
 * 1. 使用 LoadFromFileWithMaterials() 加载完整模型
 * 2. 自动加载材质属性（颜色、光泽度等）
 * 3. 自动加载纹理贴图（漫反射、镜面反射、法线等）
 * 4. 渲染带材质的模型
 * 
 * 控制：
 * - 空格键/左右箭头：切换网格
 * - W：切换线框模式
 * - ESC：退出
 */

#include <render/renderer.h>
#include <render/shader_cache.h>
#include <render/mesh_loader.h>
#include <render/material.h>
#include <render/logger.h>
#include <glad/glad.h>
#include <iostream>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Render;

// 全局资源
Ref<Shader> phongShader;
std::vector<MeshWithMaterial> modelParts;
int currentPartIndex = 0;
float rotationAngle = 0.0f;
bool wireframeMode = false;

/**
 * @brief 初始化场景
 */
bool InitScene(Renderer& renderer) {
    Logger::GetInstance().Info("=== 初始化模型材质加载测试场景 ===");
    
    // 1. 加载 Phong 着色器
    phongShader = ShaderCache::GetInstance().LoadShader(
        "material_phong", "shaders/material_phong.vert", "shaders/material_phong.frag");
    if (!phongShader) {
        Logger::GetInstance().Error("Failed to load Phong shader");
        return false;
    }
    
    // 2. 从文件加载模型（带材质和纹理）
    Logger::GetInstance().Info("\n尝试加载模型文件...");
    
    // 定义可能的模型文件路径
    std::vector<std::string> modelPaths = {
        "models/miku/v4c5.0short.pmx",  // Miku PMX 模型（短发版）
        "models/miku/v4c5.0.pmx",        // Miku PMX 模型（完整版）
        "models/cube.obj",
        "../models/miku/v4c5.0short.pmx",
        "../models/miku/v4c5.0.pmx",
        "../models/cube.obj"
    };
    
    // 尝试加载模型文件（带材质和纹理）
    std::string usedPath;
    for (const auto& path : modelPaths) {
        Logger::GetInstance().Info("尝试: " + path);
        auto parts = MeshLoader::LoadFromFileWithMaterials(path, "", true, phongShader);
        if (!parts.empty()) {
            modelParts = std::move(parts);
            usedPath = path;
            Logger::GetInstance().Info("成功加载模型(含材质): " + path);
            break;
        }
    }
    
    if (modelParts.empty()) {
        Logger::GetInstance().Warning("未能加载模型文件，使用程序生成的网格");
        
        // 创建默认网格和材质
        auto mesh = MeshLoader::CreateSphere(0.5f, 64, 32, Color::White());
        auto material = std::make_shared<Material>();
        material->SetName("Default Material");
        material->SetShader(phongShader);
        material->SetDiffuseColor(Color(0.8f, 0.2f, 0.2f, 1.0f));
        material->SetSpecularColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
        material->SetShininess(32.0f);
        
        modelParts.push_back(MeshWithMaterial(mesh, material, "Default Sphere"));
    } else {
        Logger::GetInstance().Info("\n✅ 模型加载成功！");
        Logger::GetInstance().Info("网格数量: " + std::to_string(modelParts.size()));
        
        // 打印材质信息
        for (size_t i = 0; i < modelParts.size(); ++i) {
            const auto& part = modelParts[i];
            Logger::GetInstance().Info("\n网格 " + std::to_string(i) + ": " + part.name);
            
            if (part.material) {
                Logger::GetInstance().Info("  材质: " + part.material->GetName());
                auto names = part.material->GetTextureNames();
                if (!names.empty()) {
                    Logger::GetInstance().Info("  纹理: " + std::to_string(names.size()) + " 个");
                    for (const auto& texName : names) {
                        Logger::GetInstance().Info("    - " + texName);
                    }
                } else {
                    Logger::GetInstance().Info("  纹理: 无");
                }
            } else {
                Logger::GetInstance().Info("  材质: 无");
            }
        }
    }
    
    Logger::GetInstance().Info("\n初始化完成!");
    return true;
}

/**
 * @brief 更新场景
 */
void UpdateScene(float deltaTime) {
    // 旋转角度更新
    rotationAngle += deltaTime * 30.0f; // 每秒旋转 30 度
    if (rotationAngle >= 360.0f) {
        rotationAngle -= 360.0f;
    }
}

/**
 * @brief 渲染场景
 */
void RenderScene(Renderer& renderer) {
    // 清空屏幕
    auto* renderState = renderer.GetRenderState();
    renderState->SetClearColor(Color(0.15f, 0.15f, 0.2f, 1.0f));
    renderState->Clear();
    
    // 设置视口
    renderState->SetViewport(0, 0, renderer.GetWidth(), renderer.GetHeight());
    
    // 创建变换矩阵
    float aspect = (float)renderer.GetWidth() / (float)renderer.GetHeight();
    
    // 投影矩阵（透视投影）
    Matrix4 projection = Matrix4::Identity();
    float fov = 45.0f * 3.14159f / 180.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    float f = 1.0f / std::tan(fov / 2.0f);
    projection(0, 0) = f / aspect;
    projection(1, 1) = f;
    projection(2, 2) = (farPlane + nearPlane) / (nearPlane - farPlane);
    projection(2, 3) = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
    projection(3, 2) = -1.0f;
    projection(3, 3) = 0.0f;
    
    // 视图矩阵（相机位置）
    Matrix4 view = Matrix4::Identity();
    view(1, 3) = -0.2f;   // Y 轴中间位置
    view(2, 3) = -3.5f;  // Z 轴向后 3.5 单位（拉远相机）
    
    // 模型矩阵
    float angleRad = rotationAngle * 3.14159f / 180.0f;
    
    // 旋转矩阵（绕 Y 轴）
    Matrix4 rotationY = Matrix4::Identity();
    rotationY(0, 0) = std::cos(angleRad);
    rotationY(0, 2) = std::sin(angleRad);
    rotationY(2, 0) = -std::sin(angleRad);
    rotationY(2, 2) = std::cos(angleRad);
    
    // 缩放矩阵（PMX 模型通常需要缩小）
    float scale = (modelParts.size() > 10) ? 0.08f : 1.0f;
    Matrix4 scaleMatrix = Matrix4::Identity();
    scaleMatrix(0, 0) = scale;
    scaleMatrix(1, 1) = scale;
    scaleMatrix(2, 2) = scale;
    
    // 平移矩阵（PMX 模型中心在脚底，向下移动让整个模型居中）
    Matrix4 translateMatrix = Matrix4::Identity();
    if (modelParts.size() > 10) {
        translateMatrix(1, 3) = -0.6f;  // 轻微向下移动，让模型居中
    }
    
    // 组合变换：缩放 -> 旋转 -> 平移
    Matrix4 model = translateMatrix * rotationY * scaleMatrix;
    
    // 渲染所有模型部件（组合成完整模型）
    for (const auto& part : modelParts) {
        if (!part.mesh) {
            continue;
        }
        
        // 如果有材质，使用材质
        if (part.material && part.material->IsValid()) {
            // 应用材质
            part.material->Bind(renderState);
            
            // 设置变换矩阵
            auto* uniformMgr = part.material->GetShader()->GetUniformManager();
            if (uniformMgr) {
                uniformMgr->SetMatrix4("uModel", model);
                uniformMgr->SetMatrix4("uView", view);
                uniformMgr->SetMatrix4("uProjection", projection);
                
                // 设置材质颜色
                uniformMgr->SetColor("uAmbientColor", part.material->GetAmbientColor());
                uniformMgr->SetColor("uDiffuseColor", part.material->GetDiffuseColor());
                uniformMgr->SetColor("uSpecularColor", part.material->GetSpecularColor());
                uniformMgr->SetFloat("uShininess", part.material->GetShininess());
                
                // 设置光照
                uniformMgr->SetVector3("uLightPos", Vector3(5.0f, 5.0f, 5.0f));
                uniformMgr->SetVector3("uViewPos", Vector3(0.0f, 0.0f, 5.0f));
            }
            
            // 渲染网格
            part.mesh->Draw();
            
            // 解绑材质
            part.material->Unbind();
        }
        else if (phongShader && phongShader->IsValid()) {
            // 没有材质，使用默认着色器和颜色
            phongShader->Use();
            
            auto* uniformMgr = phongShader->GetUniformManager();
            if (uniformMgr) {
                uniformMgr->SetMatrix4("uModel", model);
                uniformMgr->SetMatrix4("uView", view);
                uniformMgr->SetMatrix4("uProjection", projection);
                
                // 默认颜色
                uniformMgr->SetColor("uAmbientColor", Color(0.2f, 0.2f, 0.2f, 1.0f));
                uniformMgr->SetColor("uDiffuseColor", Color(0.7f, 0.7f, 0.7f, 1.0f));
                uniformMgr->SetColor("uSpecularColor", Color(1.0f, 1.0f, 1.0f, 1.0f));
                uniformMgr->SetFloat("uShininess", 32.0f);
                
                uniformMgr->SetVector3("uLightPos", Vector3(5.0f, 5.0f, 5.0f));
                uniformMgr->SetVector3("uViewPos", Vector3(0.0f, 0.0f, 5.0f));
            }
            
            part.mesh->Draw();
            phongShader->Unuse();
        }
    }
}

/**
 * @brief 处理输入事件
 */
void HandleInput(SDL_Event& event, bool& running) {
    if (event.type == SDL_EVENT_QUIT) {
        running = false;
    }
    else if (event.type == SDL_EVENT_KEY_DOWN) {
        switch (event.key.key) {
            case SDLK_ESCAPE:
                running = false;
                break;
                
            case SDLK_W:
                // 切换线框模式
                wireframeMode = !wireframeMode;
                glPolygonMode(GL_FRONT_AND_BACK, wireframeMode ? GL_LINE : GL_FILL);
                Logger::GetInstance().Info(wireframeMode ? "线框模式: 开启" : "线框模式: 关闭");
                break;
        }
    }
}

/**
 * @brief 主函数
 */
int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    // 开启日志
    Logger::GetInstance().SetLogToFile(true);
    Logger::GetInstance().SetLogToConsole(true);
    Logger::GetInstance().Info("=== 模型材质加载测试 ===");
    
    // 创建渲染器
    Renderer renderer;
    if (!renderer.Initialize("Model Material Loader Test - 模型材质加载测试", 1280, 720)) {
        Logger::GetInstance().Error("Failed to initialize renderer");
        return -1;
    }
    
    // 初始化场景
    if (!InitScene(renderer)) {
        Logger::GetInstance().Error("Failed to initialize scene");
        renderer.Shutdown();
        return -1;
    }
    
    // 打印控制说明
    Logger::GetInstance().Info("\n=== 控制说明 ===");
    Logger::GetInstance().Info("W: 切换线框模式");
    Logger::GetInstance().Info("ESC: 退出");
    Logger::GetInstance().Info("模型将自动旋转");
    Logger::GetInstance().Info("");
    
    // 主循环
    bool running = true;
    Uint64 lastTime = SDL_GetTicks();
    
    while (running) {
        // 计算 deltaTime
        Uint64 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        
        // 处理事件
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            HandleInput(event, running);
        }
        
        // 更新场景
        UpdateScene(deltaTime);
        
        // 渲染场景
        renderer.BeginFrame();
        RenderScene(renderer);
        renderer.EndFrame();
        
        // 呈现
        renderer.Present();
    }
    
    // 清理
    modelParts.clear();
    phongShader.reset();
    
    renderer.Shutdown();
    Logger::GetInstance().Info("程序正常退出");
    Logger::GetInstance().Info("日志已保存到: " + Logger::GetInstance().GetCurrentLogFile());
    
    return 0;
}

