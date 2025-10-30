/**
 * @file 15_resource_manager_test.cpp
 * @brief 测试资源管理器功能
 * 
 * 本示例演示：
 * 1. ResourceManager 的基本使用
 * 2. 统一管理纹理、网格、材质、着色器
 * 3. 资源注册、获取、释放
 * 4. 引用计数管理
 * 5. 资源统计和监控
 * 6. 自动清理未使用资源
 * 
 * 重要说明：
 * - 程序使用全局变量 activeMesh 和 activeMaterial 保持对当前资源的引用
 * - 这些全局引用确保当前使用的资源引用计数 >= 2（ResourceManager + 全局变量）
 * - CleanupUnused() 只会清理引用计数为 1 的资源（未被使用的资源）
 * - 当切换网格时，旧网格的引用被释放，引用计数减少，可能被清理
 * 
 * 控制：
 * - SPACE：清理未使用资源（引用计数为1的资源）
 * - 数字 1-4：切换显示的网格（会更新全局引用）
 * - S：打印资源统计信息和引用计数
 * - C：清空所有资源（并重新加载）
 * - ESC：退出
 */

#include <render/renderer.h>
#include <render/resource_manager.h>
#include <render/shader_cache.h>
#include <render/texture_loader.h>
#include <render/mesh_loader.h>
#include <render/material.h>
#include <render/logger.h>
#include <glad/glad.h>
#include <iostream>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Render;

// 场景资源
std::string currentMeshName = "sphere";
float rotationAngle = 0.0f;

// 保持对常用资源的引用，防止被 CleanupUnused 清理
Ref<Mesh> activeMesh;
Ref<Material> activeMaterial;

/**
 * @brief 初始化场景资源
 */
bool InitScene(Renderer& renderer) {
    Logger::GetInstance().Info("=== 初始化资源管理器测试场景 ===");
    
    auto& resourceMgr = ResourceManager::GetInstance();
    
    // ========================================================================
    // 1. 注册着色器
    // ========================================================================
    Logger::GetInstance().Info("注册着色器资源...");
    
    auto phongShader = ShaderCache::GetInstance().LoadShader(
        "phong", "shaders/material_phong.vert", "shaders/material_phong.frag");
    if (!phongShader) {
        Logger::GetInstance().Error("Failed to load Phong shader");
        return false;
    }
    resourceMgr.RegisterShader("phong", phongShader);
    
    auto basicShader = ShaderCache::GetInstance().LoadShader(
        "basic", "shaders/basic.vert", "shaders/basic.frag");
    if (basicShader) {
        resourceMgr.RegisterShader("basic", basicShader);
    }
    
    // ========================================================================
    // 2. 注册纹理
    // ========================================================================
    Logger::GetInstance().Info("注册纹理资源...");
    
    auto texture1 = TextureLoader::GetInstance().LoadTexture("test_texture", "textures/test.jpg", true);
    if (texture1) {
        resourceMgr.RegisterTexture("test_texture", texture1);
    } else {
        Logger::GetInstance().Warning("Failed to load test texture");
    }
    
    // 创建程序化纹理
    std::vector<unsigned char> checkerboard(256 * 256 * 4);
    for (int y = 0; y < 256; y++) {
        for (int x = 0; x < 256; x++) {
            int idx = (y * 256 + x) * 4;
            bool isWhite = ((x / 32) + (y / 32)) % 2 == 0;
            unsigned char color = isWhite ? 255 : 64;
            checkerboard[idx + 0] = color;
            checkerboard[idx + 1] = color;
            checkerboard[idx + 2] = color;
            checkerboard[idx + 3] = 255;
        }
    }
    
    auto checkerboardTex = TextureLoader::GetInstance().CreateTexture(
        "checkerboard", checkerboard.data(), 256, 256, TextureFormat::RGBA, true);
    if (checkerboardTex) {
        resourceMgr.RegisterTexture("checkerboard", checkerboardTex);
    }
    
    // ========================================================================
    // 3. 注册网格
    // ========================================================================
    Logger::GetInstance().Info("注册网格资源...");
    
    auto cubeMesh = MeshLoader::CreateCube(1.0f, 1.0f, 1.0f, Color::White());
    resourceMgr.RegisterMesh("cube", cubeMesh);
    
    auto sphereMesh = MeshLoader::CreateSphere(0.5f, 64, 32, Color::White());
    resourceMgr.RegisterMesh("sphere", sphereMesh);
    
    auto cylinderMesh = MeshLoader::CreateCylinder(0.4f, 0.4f, 1.0f, 32, Color::White());
    resourceMgr.RegisterMesh("cylinder", cylinderMesh);
    
    auto torusMesh = MeshLoader::CreateTorus(0.8f, 0.2f, 48, 24, Color::White());
    resourceMgr.RegisterMesh("torus", torusMesh);
    
    // ========================================================================
    // 4. 注册材质
    // ========================================================================
    Logger::GetInstance().Info("注册材质资源...");
    
    // 材质 1: 红色塑料
    auto redMaterial = std::make_shared<Material>();
    redMaterial->SetName("red_plastic");
    redMaterial->SetShader(phongShader);
    redMaterial->SetAmbientColor(Color(0.2f, 0.0f, 0.0f, 1.0f));
    redMaterial->SetDiffuseColor(Color(0.8f, 0.1f, 0.1f, 1.0f));
    redMaterial->SetSpecularColor(Color(0.5f, 0.5f, 0.5f, 1.0f));
    redMaterial->SetShininess(32.0f);
    resourceMgr.RegisterMaterial("red_plastic", redMaterial);
    
    // 材质 2: 蓝色金属
    auto blueMaterial = std::make_shared<Material>();
    blueMaterial->SetName("blue_metal");
    blueMaterial->SetShader(phongShader);
    blueMaterial->SetAmbientColor(Color(0.0f, 0.05f, 0.1f, 1.0f));
    blueMaterial->SetDiffuseColor(Color(0.1f, 0.3f, 0.8f, 1.0f));
    blueMaterial->SetSpecularColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
    blueMaterial->SetShininess(128.0f);
    blueMaterial->SetMetallic(1.0f);
    blueMaterial->SetRoughness(0.2f);
    resourceMgr.RegisterMaterial("blue_metal", blueMaterial);
    
    // 材质 3: 棋盘格纹理材质
    if (checkerboardTex) {
        auto checkerMaterial = std::make_shared<Material>();
        checkerMaterial->SetName("checker_textured");
        checkerMaterial->SetShader(phongShader);
        checkerMaterial->SetDiffuseColor(Color::White());
        checkerMaterial->SetShininess(64.0f);
        checkerMaterial->SetTexture("diffuseMap", checkerboardTex);
        resourceMgr.RegisterMaterial("checker_textured", checkerMaterial);
    }
    
    // ========================================================================
    // 5. 打印初始统计信息
    // ========================================================================
    resourceMgr.PrintStatistics();
    
    // ========================================================================
    // 6. 获取并保持对常用资源的引用（防止被 CleanupUnused 清理）
    // ========================================================================
    activeMesh = resourceMgr.GetMesh(currentMeshName);
    activeMaterial = resourceMgr.GetMaterial("red_plastic");
    
    Logger::GetInstance().Info("场景初始化完成");
    return true;
}

/**
 * @brief 渲染当前场景
 */
void RenderScene(Renderer& renderer) {
    // 清屏
    auto renderState = renderer.GetRenderState();
    renderState->Clear();
    
    // 使用全局持有的资源引用
    if (!activeMesh || !activeMaterial || !activeMaterial->IsValid()) {
        return;
    }
    
    // 更新旋转角度
    rotationAngle += 0.01f;
    if (rotationAngle > 2.0f * M_PI) {
        rotationAngle -= 2.0f * M_PI;
    }
    
    // 设置变换矩阵
    Matrix4 modelMatrix = Matrix4::Identity();
    Eigen::AngleAxisf rotation(rotationAngle, Vector3(0.3f, 1.0f, 0.2f).normalized());
    modelMatrix.block<3, 3>(0, 0) = rotation.toRotationMatrix();
    
    Matrix4 viewMatrix = Matrix4::Identity();
    viewMatrix(2, 3) = -3.0f;
    
    float aspect = 800.0f / 600.0f;
    float fov = 45.0f * static_cast<float>(M_PI) / 180.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    float f = 1.0f / std::tan(fov / 2.0f);
    
    Matrix4 projMatrix = Matrix4::Zero();
    projMatrix(0, 0) = f / aspect;
    projMatrix(1, 1) = f;
    projMatrix(2, 2) = (farPlane + nearPlane) / (nearPlane - farPlane);
    projMatrix(2, 3) = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
    projMatrix(3, 2) = -1.0f;
    projMatrix(3, 3) = 0.0f;
    
    Matrix4 mvpMatrix = projMatrix * viewMatrix * modelMatrix;
    
    // 绑定材质并设置 uniforms
    activeMaterial->Bind(renderer.GetRenderState().get());
    
    auto* uniformMgr = activeMaterial->GetShader()->GetUniformManager();
    uniformMgr->SetMatrix4("uProjection", projMatrix);
    uniformMgr->SetMatrix4("uView", viewMatrix);
    uniformMgr->SetMatrix4("uModel", modelMatrix);
    
    // 设置材质颜色属性（材质的Bind已经设置了这些，但我们这里再确保一下）
    uniformMgr->SetColor("uAmbientColor", activeMaterial->GetAmbientColor());
    uniformMgr->SetColor("uDiffuseColor", activeMaterial->GetDiffuseColor());
    uniformMgr->SetColor("uSpecularColor", activeMaterial->GetSpecularColor());
    uniformMgr->SetFloat("uShininess", activeMaterial->GetShininess());
    
    // 光照参数
    uniformMgr->SetVector3("uLightPos", Vector3(2.0f, 2.0f, 2.0f));
    uniformMgr->SetVector3("uViewPos", Vector3(0.0f, 0.0f, 3.0f));
    
    // 渲染网格
    activeMesh->Draw();
    
    activeMaterial->Unbind();
}

/**
 * @brief 处理输入
 */
void HandleInput(SDL_Event& event, bool& running, Renderer& renderer) {
    auto& resourceMgr = ResourceManager::GetInstance();
    
    if (event.type == SDL_EVENT_KEY_DOWN) {
        switch (event.key.key) {
            case SDLK_ESCAPE:
                running = false;
                break;
                
            case SDLK_SPACE:
                // 清理未使用资源
                {
                    Logger::GetInstance().Info("========================================");
                    Logger::GetInstance().Info("清理未使用资源（引用计数为1的资源）...");
                    Logger::GetInstance().Info("注意：当前使用的网格和材质由全局变量持有，不会被清理");
                    
                    // 显示当前活动资源的引用计数
                    Logger::GetInstance().Info("活动资源引用计数:");
                    Logger::GetInstance().Info("  - " + currentMeshName + " (网格): " + 
                        std::to_string(resourceMgr.GetReferenceCount(ResourceType::Mesh, currentMeshName)));
                    Logger::GetInstance().Info("  - red_plastic (材质): " + 
                        std::to_string(resourceMgr.GetReferenceCount(ResourceType::Material, "red_plastic")));
                    
                    size_t cleaned = resourceMgr.CleanupUnused();
                    Logger::GetInstance().Info("清理了 " + std::to_string(cleaned) + " 个未使用资源");
                    Logger::GetInstance().Info("========================================");
                    resourceMgr.PrintStatistics();
                }
                break;
                
            case SDLK_1:
                currentMeshName = "cube";
                activeMesh = resourceMgr.GetMesh(currentMeshName);
                Logger::GetInstance().Info("切换到立方体");
                break;
                
            case SDLK_2:
                currentMeshName = "sphere";
                activeMesh = resourceMgr.GetMesh(currentMeshName);
                Logger::GetInstance().Info("切换到球体");
                break;
                
            case SDLK_3:
                currentMeshName = "cylinder";
                activeMesh = resourceMgr.GetMesh(currentMeshName);
                Logger::GetInstance().Info("切换到圆柱体");
                break;
                
            case SDLK_4:
                currentMeshName = "torus";
                activeMesh = resourceMgr.GetMesh(currentMeshName);
                Logger::GetInstance().Info("切换到圆环");
                break;
                
            case SDLK_S:
                // 打印统计信息
                resourceMgr.PrintStatistics();
                Logger::GetInstance().Info("--- 详细资源列表 ---");
                {
                    auto textures = resourceMgr.ListTextures();
                    Logger::GetInstance().Info("纹理: " + std::to_string(textures.size()) + " 个");
                    for (const auto& name : textures) {
                        long refCount = resourceMgr.GetReferenceCount(ResourceType::Texture, name);
                        Logger::GetInstance().Info("  - " + name + " (引用计数: " + std::to_string(refCount) + ")");
                    }
                    
                    auto meshes = resourceMgr.ListMeshes();
                    Logger::GetInstance().Info("网格: " + std::to_string(meshes.size()) + " 个");
                    for (const auto& name : meshes) {
                        long refCount = resourceMgr.GetReferenceCount(ResourceType::Mesh, name);
                        Logger::GetInstance().Info("  - " + name + " (引用计数: " + std::to_string(refCount) + ")");
                    }
                    
                    auto materials = resourceMgr.ListMaterials();
                    Logger::GetInstance().Info("材质: " + std::to_string(materials.size()) + " 个");
                    for (const auto& name : materials) {
                        long refCount = resourceMgr.GetReferenceCount(ResourceType::Material, name);
                        Logger::GetInstance().Info("  - " + name + " (引用计数: " + std::to_string(refCount) + ")");
                    }
                    
                    auto shaders = resourceMgr.ListShaders();
                    Logger::GetInstance().Info("着色器: " + std::to_string(shaders.size()) + " 个");
                    for (const auto& name : shaders) {
                        long refCount = resourceMgr.GetReferenceCount(ResourceType::Shader, name);
                        Logger::GetInstance().Info("  - " + name + " (引用计数: " + std::to_string(refCount) + ")");
                    }
                }
                break;
                
            case SDLK_C:
                // 清空所有资源并重新加载
                Logger::GetInstance().Info("清空所有资源...");
                
                // 先释放全局引用
                activeMesh.reset();
                activeMaterial.reset();
                
                // 清空所有缓存
                resourceMgr.Clear();
                ShaderCache::GetInstance().Clear();
                TextureLoader::GetInstance().Clear();
                
                Logger::GetInstance().Info("重新初始化场景...");
                InitScene(renderer);  // 这会重新获取 activeMesh 和 activeMaterial
                break;
        }
    }
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    // 开启日志
    Logger::GetInstance().SetLogToFile(true);
    Logger::GetInstance().SetLogToConsole(true);

    try {
        // 初始化渲染器
        Renderer renderer;
        if (!renderer.Initialize("资源管理器测试", 800, 600)) {
            Logger::GetInstance().Error("Failed to initialize renderer");
            return -1;
        }
        
        // 设置渲染状态
        auto renderState = renderer.GetRenderState();
        renderState->SetDepthTest(true);
        renderState->SetCullFace(CullFace::Back);
        renderState->SetClearColor(Color(0.1f, 0.1f, 0.15f, 1.0f));
        
        // 初始化场景
        if (!InitScene(renderer)) {
            Logger::GetInstance().Error("Failed to initialize scene");
            return -1;
        }
        
        Logger::GetInstance().Info("========================================");
        Logger::GetInstance().Info("资源管理器测试");
        Logger::GetInstance().Info("========================================");
        Logger::GetInstance().Info("控制:");
        Logger::GetInstance().Info("  SPACE - 清理未使用资源");
        Logger::GetInstance().Info("  1-4   - 切换网格 (立方体/球体/圆柱/圆环)");
        Logger::GetInstance().Info("  S     - 打印详细统计信息");
        Logger::GetInstance().Info("  C     - 清空所有资源并重新加载");
        Logger::GetInstance().Info("  ESC   - 退出");
        Logger::GetInstance().Info("========================================");
        
        // 主循环
        bool running = true;
        SDL_Event event;
        
        while (running) {
            // 处理事件
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                }
                HandleInput(event, running, renderer);
            }
            
            // 渲染场景
            renderer.BeginFrame();
            RenderScene(renderer);
            renderer.EndFrame();
            
            // 呈现到屏幕
            renderer.Present();
        }
        
        // 清理
        Logger::GetInstance().Info("清理资源...");
        
        // 释放全局资源引用
        activeMesh.reset();
        activeMaterial.reset();
        
        ResourceManager::GetInstance().PrintStatistics();
        
        Logger::GetInstance().Info("程序正常退出");
        return 0;
    }
    catch (const std::exception& e) {
        Logger::GetInstance().Error(std::string("Exception: ") + e.what());
        return -1;
    }
}

