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
 * @file 12_material_test.cpp
 * @brief 测试材质系统功能
 * 
 * 本示例演示：
 * 1. Material 类的基本使用
 * 2. 材质属性设置（颜色、物理参数等）
 * 3. 材质纹理贴图
 * 4. 材质渲染状态控制
 * 5. 多种材质的场景渲染
 * 
 * 控制：
 * - 空格键/左右箭头：切换材质
 * - W：切换线框模式
 * - ESC：退出
 */

#include <render/renderer.h>
#include <render/shader.h>
#include <render/shader_cache.h>
#include <render/material.h>
#include <render/mesh.h>
#include <render/mesh_loader.h>
#include <render/texture_loader.h>
#include <render/logger.h>
#include <glad/glad.h>
#include <iostream>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Render;

// 全局资源
Ref<Shader> basicShader;
Ref<Shader> texturedShader;
std::vector<Ref<Material>> materials;
Ref<Mesh> sphereMesh;
int currentMaterialIndex = 0;
float rotationAngle = 0.0f;
bool wireframeMode = false;

// 材质名称
std::vector<std::string> materialNames = {
    "基础红色材质",
    "银色金属材质",
    "塑料材质",
    "发光材质",
    "半透明材质",
    "金色金属材质",
    "纹理材质"
};

/**
 * @brief 初始化场景
 */
bool InitScene(Renderer& renderer) {
    Logger::GetInstance().Info("=== 初始化材质测试场景 ===");
    
    // 1. 加载 Phong 光照着色器（支持镜面反射高光）
    basicShader = ShaderCache::GetInstance().LoadShader(
        "material_phong", "shaders/material_phong.vert", "shaders/material_phong.frag");
    if (!basicShader) {
        Logger::GetInstance().Error("Failed to load Phong shader");
        return false;
    }
    
    // 2. 创建球体网格
    sphereMesh = MeshLoader::CreateSphere(0.5f, 64, 32, Color::White());
    if (!sphereMesh) {
        Logger::GetInstance().Error("Failed to create sphere mesh");
        return false;
    }
    
    // 3. 创建各种材质
    Logger::GetInstance().Info("创建材质...");
    
    // 材质 1: 基础红色材质
    {
        auto material = std::make_shared<Material>();
        material->SetName("Red Material");
        material->SetShader(basicShader);
        material->SetAmbientColor(Color(0.2f, 0.0f, 0.0f, 1.0f));
        material->SetDiffuseColor(Color(0.8f, 0.1f, 0.1f, 1.0f));
        material->SetSpecularColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
        material->SetShininess(32.0f);
        materials.push_back(material);
    }
    
    // 材质 2: 金属材质（银色/铬）
    {
        auto material = std::make_shared<Material>();
        material->SetName("Metal Material");
        material->SetShader(basicShader);
        // 金属特性：低环境光，较暗的漫反射，强镜面反射
        material->SetAmbientColor(Color(0.05f, 0.05f, 0.05f, 1.0f));  // 很低的环境光
        material->SetDiffuseColor(Color(0.5f, 0.5f, 0.55f, 1.0f));     // 较暗的漫反射
        material->SetSpecularColor(Color(1.0f, 1.0f, 1.0f, 1.0f));     // 强烈的白色高光
        material->SetShininess(128.0f);                                  // 非常锐利的高光
        material->SetMetallic(1.0f);
        material->SetRoughness(0.2f);
        materials.push_back(material);
    }
    
    // 材质 3: 塑料材质
    {
        auto material = std::make_shared<Material>();
        material->SetName("Plastic Material");
        material->SetShader(basicShader);
        material->SetAmbientColor(Color(0.0f, 0.1f, 0.2f, 1.0f));
        material->SetDiffuseColor(Color(0.2f, 0.4f, 0.8f, 1.0f));
        material->SetSpecularColor(Color(0.5f, 0.5f, 0.5f, 1.0f));
        material->SetShininess(16.0f);
        material->SetMetallic(0.0f);
        material->SetRoughness(0.6f);
        materials.push_back(material);
    }
    
    // 材质 4: 发光材质
    {
        auto material = std::make_shared<Material>();
        material->SetName("Emissive Material");
        material->SetShader(basicShader);
        material->SetAmbientColor(Color(0.1f, 0.1f, 0.0f, 1.0f));
        material->SetDiffuseColor(Color(0.8f, 0.8f, 0.2f, 1.0f));
        material->SetSpecularColor(Color(0.3f, 0.3f, 0.1f, 1.0f));
        material->SetEmissiveColor(Color(1.0f, 1.0f, 0.0f, 1.0f));
        material->SetShininess(8.0f);
        materials.push_back(material);
    }
    
    // 材质 5: 半透明材质
    {
        auto material = std::make_shared<Material>();
        material->SetName("Transparent Material");
        material->SetShader(basicShader);
        material->SetAmbientColor(Color(0.0f, 0.2f, 0.2f, 0.5f));
        material->SetDiffuseColor(Color(0.2f, 0.8f, 0.8f, 0.5f));
        material->SetSpecularColor(Color(1.0f, 1.0f, 1.0f, 0.5f));
        material->SetShininess(64.0f);
        material->SetOpacity(0.5f);
        material->SetBlendMode(BlendMode::Alpha);
        material->SetDepthWrite(false);
        materials.push_back(material);
    }
    
    // 材质 6: 金色金属材质
    {
        auto material = std::make_shared<Material>();
        material->SetName("Gold Material");
        material->SetShader(basicShader);
        // 金色特性
        material->SetAmbientColor(Color(0.1f, 0.08f, 0.02f, 1.0f));
        material->SetDiffuseColor(Color(0.83f, 0.69f, 0.22f, 1.0f));  // 金黄色
        material->SetSpecularColor(Color(1.0f, 0.95f, 0.7f, 1.0f));   // 金色高光
        material->SetShininess(128.0f);
        material->SetMetallic(1.0f);
        material->SetRoughness(0.15f);
        materials.push_back(material);
    }
    
    // 材质 7: 纹理材质（如果纹理存在）
    {
        auto material = std::make_shared<Material>();
        material->SetName("Textured Material");
        material->SetShader(basicShader);
        material->SetAmbientColor(Color(0.2f, 0.2f, 0.2f, 1.0f));
        material->SetDiffuseColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
        material->SetSpecularColor(Color(0.5f, 0.5f, 0.5f, 1.0f));
        material->SetShininess(32.0f);
        
        // 尝试加载纹理
        auto texture = TextureLoader::GetInstance().LoadTexture("test_texture", "textures/test.jpg");
        if (texture) {
            material->SetTexture("diffuseMap", texture);
            Logger::GetInstance().Info("纹理加载成功");
        } else {
            Logger::GetInstance().Warning("纹理加载失败，使用纯色");
        }
        
        materials.push_back(material);
    }
    
    Logger::GetInstance().Info("创建了 " + std::to_string(materials.size()) + " 个材质");
    Logger::GetInstance().Info("初始化完成!");
    
    return true;
}

/**
 * @brief 更新场景
 */
void UpdateScene(float deltaTime) {
    // 旋转角度更新
    rotationAngle += deltaTime * 45.0f; // 每秒旋转 45 度
    if (rotationAngle >= 360.0f) {
        rotationAngle -= 360.0f;
    }
}

/**
 * @brief 渲染场景
 */
void RenderScene(Renderer& renderer) {
    // 清空屏幕
    auto renderState = renderer.GetRenderState();
    renderState->SetClearColor(Color(0.1f, 0.1f, 0.15f, 1.0f));
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
    
    // 视图矩阵（相机）
    Matrix4 view = Matrix4::Identity();
    view(2, 3) = -3.0f; // 向后移动3个单位
    
    // 模型矩阵（旋转）
    float angleRad = rotationAngle * 3.14159f / 180.0f;
    Matrix4 model = Matrix4::Identity();
    model(0, 0) = std::cos(angleRad);
    model(0, 2) = std::sin(angleRad);
    model(2, 0) = -std::sin(angleRad);
    model(2, 2) = std::cos(angleRad);
    
    // 绑定当前材质
    if (currentMaterialIndex >= 0 && currentMaterialIndex < materials.size()) {
        auto& material = materials[currentMaterialIndex];
        
        // 应用材质
        material->Bind(renderState.get());
        
        // 设置变换矩阵和光照
        auto* uniformMgr = material->GetShader()->GetUniformManager();
        if (uniformMgr) {
            // 设置变换矩阵
            uniformMgr->SetMatrix4("uModel", model);
            uniformMgr->SetMatrix4("uView", view);
            uniformMgr->SetMatrix4("uProjection", projection);
            
            // 设置材质颜色属性
            uniformMgr->SetColor("uAmbientColor", material->GetAmbientColor());
            uniformMgr->SetColor("uDiffuseColor", material->GetDiffuseColor());
            uniformMgr->SetColor("uSpecularColor", material->GetSpecularColor());
            uniformMgr->SetFloat("uShininess", material->GetShininess());
            
            // 设置光照（点光源位置在右上方）
            uniformMgr->SetVector3("uLightPos", Vector3(3.0f, 3.0f, 3.0f));
            // 相机位置
            uniformMgr->SetVector3("uViewPos", Vector3(0.0f, 0.0f, 3.0f));
        }
        
        // 渲染球体
        if (sphereMesh) {
            sphereMesh->Draw();
        }
        
        // 解绑材质
        material->Unbind();
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
                
            case SDLK_SPACE:
            case SDLK_RIGHT:
                // 切换到下一个材质
                currentMaterialIndex = (currentMaterialIndex + 1) % materials.size();
                Logger::GetInstance().Info("当前材质: " + materialNames[currentMaterialIndex]);
                break;
                
            case SDLK_LEFT:
                // 切换到上一个材质
                currentMaterialIndex--;
                if (currentMaterialIndex < 0) {
                    currentMaterialIndex = materials.size() - 1;
                }
                Logger::GetInstance().Info("当前材质: " + materialNames[currentMaterialIndex]);
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

    // 开启日志文件输出
    Logger::GetInstance().SetLogToFile(true);
    Logger::GetInstance().SetLogToConsole(true);
    
    Logger::GetInstance().Info("=== 材质系统测试 ===");
    
    // 创建渲染器
    Renderer renderer;
    if (!renderer.Initialize("Material Test - 材质系统测试", 1280, 720)) {
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
    Logger::GetInstance().Info("");
    Logger::GetInstance().Info("=== 控制说明 ===");
    Logger::GetInstance().Info("空格键/左右箭头: 切换材质");
    Logger::GetInstance().Info("W: 切换线框模式");
    Logger::GetInstance().Info("ESC: 退出");
    Logger::GetInstance().Info("");
    Logger::GetInstance().Info("当前材质: " + materialNames[currentMaterialIndex]);
    
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
    materials.clear();
    sphereMesh.reset();
    basicShader.reset();
    texturedShader.reset();
    
    renderer.Shutdown();
    Logger::GetInstance().Info("程序正常退出");
    Logger::GetInstance().Info("日志已保存到: " + Logger::GetInstance().GetCurrentLogFile());
    
    return 0;
}

